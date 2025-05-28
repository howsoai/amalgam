//project headers:
#include "SBFDSColumnData.h"

void SBFDSColumnData::InsertIndexValue(EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &value, size_t index)
{
	if(index >= valueEntries.size())
		valueEntries.resize(index + 1);

	if(value_type == ENIVT_NOT_EXIST)
	{
		invalidIndices.insert(index);

		if(internedNumberValues.valueInterningEnabled || internedStringIdValues.valueInterningEnabled)
			valueEntries[index] = EvaluableNodeImmediateValue(ValueEntry::NULL_INDEX);
		else
			valueEntries[index] = value;

		return;
	}

	if(value_type == ENIVT_NULL)
	{
		nullIndices.insert(index);

		if(internedNumberValues.valueInterningEnabled || internedStringIdValues.valueInterningEnabled)
			valueEntries[index] = EvaluableNodeImmediateValue(ValueEntry::NULL_INDEX);
		else
			valueEntries[index] = value;

		return;
	}

	if(value_type == ENIVT_NUMBER || value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
	{
		numberIndices.insert(index);

		double number_value = ResolveValue(value_type, value).number;

		auto [value_entry_iter, inserted] = sortedNumberValueEntries.try_emplace(number_value, number_value);
		auto &value_entry = value_entry_iter->second;
		if(!inserted)
		{
			value_entry.indicesWithValue.insert(index);

			if(internedNumberValues.valueInterningEnabled)
				valueEntries[index] = EvaluableNodeImmediateValue(value_entry.valueInternIndex);
			else
				valueEntries[index] = value;

			return;
		}

		value_entry.indicesWithValue.insert(index);

		if(internedNumberValues.valueInterningEnabled)
		{
			internedNumberValues.InsertValueEntry(value_entry, sortedNumberValueEntries.size());
			valueEntries[index] = value_entry.valueInternIndex;
		}
		else
		{
			valueEntries[index] = value;
		}

		return;
	}

	if(value_type == ENIVT_STRING_ID || value_type == ENIVT_STRING_ID_INDIRECTION_INDEX)
	{
		stringIdIndices.insert(index);

		auto string_id = ResolveValue(value_type, value).stringID;

		//try to insert the value if not already there
		auto [inserted_id_entry, inserted] = stringIdValueEntries.emplace(string_id, nullptr);
		if(inserted)
			inserted_id_entry->second = std::make_unique<ValueEntry>(string_id);

		InsertFirstIndexIntoStringIdValueEntry(index, inserted_id_entry);

		UpdateLongestString(string_id, index);

		if(internedStringIdValues.valueInterningEnabled)
			valueEntries[index] = inserted_id_entry->second->valueInternIndex;
		else
			valueEntries[index] = value;

		return;
	}

	//value_type == ENIVT_CODE
	codeIndices.insert(index);

	//find the entities that have the corresponding size; if the size doesn't exist, create it
	size_t code_size = EvaluableNode::GetDeepSize(value.code);

	auto [size_entry, inserted] = valueCodeSizeToIndices.emplace(code_size, nullptr);
	if(inserted)
		size_entry->second = std::make_unique<SortedIntegerSet>();

	//add the entity
	size_entry->second->insert(index);

	UpdateLargestCode(code_size, index);

	valueEntries[index] = value;
}

void SBFDSColumnData::InsertNextIndexValueExceptNumbers(EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &value, size_t index)
{
	valueEntries[index] = value;

	if(value_type == ENIVT_NOT_EXIST)
	{
		invalidIndices.insert(index);
	}
	else if(value_type == ENIVT_NULL)
	{
		nullIndices.insert(index);
	}
	else if(value_type == ENIVT_NUMBER)
	{
		numberIndices.insert(index);

		auto [value_entry_iter, inserted] = sortedNumberValueEntries.try_emplace(value.number, value.number);
		value_entry_iter->second.indicesWithValue.InsertNewLargestInteger(index);
	}
	else if(value_type == ENIVT_STRING_ID)
	{
		stringIdIndices.insert(index);

		//try to insert the value if not already there, inserting an empty pointer
		auto [id_entry, inserted] = stringIdValueEntries.emplace(value.stringID, nullptr);
		if(inserted)
			id_entry->second = std::make_unique<ValueEntry>(value.stringID);

		id_entry->second->indicesWithValue.InsertNewLargestInteger(index);

		UpdateLongestString(value.stringID, index);
	}
	else if(value_type == ENIVT_CODE)
	{
		codeIndices.insert(index);

		//find the entities that have the corresponding size; if the size doesn't exist, create it
		size_t code_size = EvaluableNode::GetDeepSize(value.code);

		auto [size_entry, inserted] = valueCodeSizeToIndices.emplace(code_size, nullptr);
		if(inserted)
			size_entry->second = std::make_unique<SortedIntegerSet>();

		//add the entity
		size_entry->second->insert(index);

		UpdateLargestCode(code_size, index);
	}
}

void SBFDSColumnData::ChangeIndexValue(EvaluableNodeImmediateValueType new_value_type,
		EvaluableNodeImmediateValue new_value, size_t index)
{
	EvaluableNodeImmediateValue old_value = valueEntries[index];
	EvaluableNodeImmediateValueType old_value_type = GetIndexValueType(index);

	//if new one is invalid, can quickly delete or return
	if(new_value_type == ENIVT_NOT_EXIST)
	{
		if(!invalidIndices.contains(index))
		{
			DeleteIndexValue(old_value_type, old_value, index);
			invalidIndices.insert(index);
		}

		if(internedNumberValues.valueInterningEnabled || internedStringIdValues.valueInterningEnabled)
			valueEntries[index] = EvaluableNodeImmediateValue(ValueEntry::NULL_INDEX);
		else
			valueEntries[index] = EvaluableNodeImmediateValue();

		return;
	}

	auto old_value_type_resolved = ResolveValueType(old_value_type);
	auto old_value_resolved = ResolveValue(old_value_type, old_value);
	auto new_value_type_resolved = ResolveValueType(new_value_type);
	auto new_value_resolved = ResolveValue(new_value_type, new_value);

	//if the types are the same, some shortcuts may apply
	//note that if the values match types and match resolved values, the old_value should be returned
	//because it is already in the correct storage format for the column
	if(old_value_type_resolved == new_value_type_resolved)
	{
		if(old_value_type_resolved == ENIVT_NULL)
			return;

		if(old_value_type_resolved == ENIVT_NUMBER)
		{
			double old_number_value = old_value_resolved.number;
			double new_number_value = new_value_resolved.number;
			if(old_number_value == new_number_value)
				return;

			//if the value already exists, then put the index in the list
			//but return the lower bound if not found so don't have to search a second time
			//need to search the old value before inserting, as FindExactIndexForValue is fragile a placeholder empty entry
			auto [new_value_entry_iter, inserted] = sortedNumberValueEntries.try_emplace(new_number_value, new_number_value);
			auto &new_value_entry = new_value_entry_iter->second;
			auto old_value_entry = sortedNumberValueEntries.find(old_number_value);

			size_t new_value_index = 0;
			if(old_value_entry != end(sortedNumberValueEntries))
			{
				//if there are multiple entries for this number, just remove the id from the old value
				if(old_value_entry->second.indicesWithValue.size() > 1)
				{
					old_value_entry->second.indicesWithValue.erase(index);
				}
				else //it's the last old_number_entry
				{
					internedNumberValues.DeleteInternIndex(old_value_entry->second.valueInternIndex);
					sortedNumberValueEntries.erase(old_value_entry);
				}

				new_value_entry.indicesWithValue.insert(index);

				//if new value didn't exist exists, insert it properly
				if(inserted)
					internedNumberValues.InsertValueEntry(new_value_entry, sortedNumberValueEntries.size());

				new_value_index = new_value_entry.valueInternIndex;
			}
			else //shouldn't make it here, but ensure integrity just in case
			{
				assert(false);
				new_value_entry.indicesWithValue.insert(index);
				internedNumberValues.InsertValueEntry(new_value_entry, sortedNumberValueEntries.size());
			}

			if(internedNumberValues.valueInterningEnabled)
				valueEntries[index] = EvaluableNodeImmediateValue(new_value_index);
			else
				valueEntries[index] = EvaluableNodeImmediateValue(new_value);

			return;
		}

		if(old_value_type_resolved == ENIVT_STRING_ID)
		{
			StringInternPool::StringID old_sid_value = old_value_resolved.stringID;
			StringInternPool::StringID new_sid_value = new_value_resolved.stringID;
			if(old_sid_value == new_sid_value)
				return;

			//try to insert the new value if not already there
			auto [new_id_entry, inserted] = stringIdValueEntries.emplace(new_sid_value, nullptr);

			size_t new_value_index = 0;
			auto old_id_entry = stringIdValueEntries.find(old_sid_value);
			if(old_id_entry != end(stringIdValueEntries))
			{
				//if there are multiple entries for this string, just move the id
				if(old_id_entry->second->indicesWithValue.size() > 1)
				{
					old_id_entry->second->indicesWithValue.erase(index);

					//if it was inserted, then construct everything
					if(inserted)
					{
						new_id_entry->second = std::make_unique<ValueEntry>(new_sid_value);
						InsertFirstIndexIntoStringIdValueEntry(index, new_id_entry);
					}
					else
					{
						new_id_entry->second->indicesWithValue.insert(index);
					}

					new_value_index = new_id_entry->second->valueInternIndex;
				}
				else //it's the last old_id_entry
				{
					//if newly inserted, then can just move the data structure
					if(inserted)
					{
						new_id_entry->second = std::move(old_id_entry->second);
						internedStringIdValues.UpdateInternIndexValue(*(new_id_entry->second.get()),
							new_sid_value);
						new_value_index = new_id_entry->second->valueInternIndex;
						//perform erase at the end since the iterator may no longer be viable after
						stringIdValueEntries.erase(old_id_entry);
					}
					else //need to clean up
					{
						new_id_entry->second->indicesWithValue.insert(index);
						new_value_index = new_id_entry->second->valueInternIndex;

						//erase after no longer need inserted_id_entry
						internedStringIdValues.DeleteInternIndex(old_id_entry->second->valueInternIndex);
						stringIdValueEntries.erase(old_id_entry);
					}
				}
			}
			else if(inserted) //shouldn't make it here, but ensure integrity just in case
			{
				assert(false);
				new_id_entry->second = std::make_unique<ValueEntry>(new_sid_value);
				InsertFirstIndexIntoStringIdValueEntry(index, new_id_entry);
				new_value_index = new_id_entry->second->valueInternIndex;
			}

			//update longest string as appropriate
			if(index == indexWithLongestString)
				RecomputeLongestString();
			else
				UpdateLongestString(new_sid_value, index);

			if(internedStringIdValues.valueInterningEnabled)
				valueEntries[index] = EvaluableNodeImmediateValue(new_value_index);
			else
				valueEntries[index] = EvaluableNodeImmediateValue(new_value);

			return;
		}

		if(old_value_type_resolved == ENIVT_CODE)
		{
			//only early exit if the pointers to the code are exactly the same,
			// as equivalent code may be garbage collected
			if(old_value.code == new_value.code)
				return;

			size_t old_code_size = EvaluableNode::GetDeepSize(old_value.code);
			size_t new_code_size = EvaluableNode::GetDeepSize(new_value.code);

			//only need to do insert / removal logic if sizes are different
			if(old_code_size != new_code_size)
			{
				auto [new_size_entry, inserted] = valueCodeSizeToIndices.emplace(new_code_size, nullptr);

				auto old_size_entry = valueCodeSizeToIndices.find(old_code_size);
				if(old_size_entry != end(valueCodeSizeToIndices))
				{
					//if there are multiple entries for this string, just move the id
					if(old_size_entry->second->size() > 1)
					{
						if(inserted)
							new_size_entry->second = std::make_unique<SortedIntegerSet>();

						new_size_entry->second->insert(index);
						old_size_entry->second->erase(index);
					}
					else //it's the last old_size_entry
					{
						//put the SortedIntegerSet in the new value or move the container
						if(inserted)
							new_size_entry->second = std::move(old_size_entry->second);
						else
							new_size_entry->second->insert(index);

						//erase after no longer need inserted_size_entry, as it may be invalidated
						valueCodeSizeToIndices.erase(old_size_entry);
					}
				}
				else if(inserted) //shouldn't make it here, but ensure integrity just in case
				{
					assert(false);
					new_size_entry->second = std::make_unique<SortedIntegerSet>();
					new_size_entry->second->insert(index);
				}
			}

			//update longest string as appropriate
			//see if need to update largest code
			if(index == indexWithLargestCode)
				RecomputeLargestCode();
			else
				UpdateLargestCode(new_code_size, index);

			valueEntries[index] = new_value;
			return;
		}

		if(old_value_type == ENIVT_NUMBER_INDIRECTION_INDEX || old_value_type == ENIVT_STRING_ID_INDIRECTION_INDEX)
		{
			if(old_value.indirectionIndex == new_value.indirectionIndex)
				return;
		}
	}

	//delete index at old value
	DeleteIndexValue(old_value_type_resolved, old_value_resolved, index);

	//add index at new value bucket
	InsertIndexValue(new_value_type_resolved, new_value_resolved, index);
}

void SBFDSColumnData::DeleteIndexValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue value, size_t index)
{
	switch(value_type)
	{
	case ENIVT_NOT_EXIST:
		invalidIndices.erase(index);
		break;

	case ENIVT_NULL:
		nullIndices.erase(index);
		break;

	case ENIVT_NUMBER:
	case ENIVT_NUMBER_INDIRECTION_INDEX:
	{
		numberIndices.erase(index);

		auto resolved_value = ResolveValue(value_type, value);

		//look up value
		auto value_entry = sortedNumberValueEntries.find(resolved_value.number);
		if(value_entry == end(sortedNumberValueEntries))
			assert(false);

		//if the bucket has only one entry, we must delete the entire bucket
		if(value_entry->second.indicesWithValue.size() == 1)
		{
			internedNumberValues.DeleteInternIndex(value_entry->second.valueInternIndex);
			sortedNumberValueEntries.erase(value_entry);
		}
		else //else we can just remove the id from the bucket
		{
			value_entry->second.indicesWithValue.erase(index);
		}

		break;
	}

	case ENIVT_STRING_ID:
	case ENIVT_STRING_ID_INDIRECTION_INDEX:
	{
		stringIdIndices.erase(index);

		auto resolved_value = ResolveValue(value_type, value);

		auto id_entry = stringIdValueEntries.find(resolved_value.stringID);
		if(id_entry == end(stringIdValueEntries))
			assert(false);

		auto &entities = id_entry->second->indicesWithValue;
		entities.erase(index);

		//if no more entries have the value, remove it
		if(entities.size() == 0)
		{
			internedStringIdValues.DeleteInternIndex(id_entry->second->valueInternIndex);
			stringIdValueEntries.erase(id_entry);
		}

		//see if need to compute new longest string
		if(index == indexWithLongestString)
			RecomputeLongestString();
	}
	break;

	case ENIVT_CODE:
	{
		codeIndices.erase(index);

		//find the entities that have the corresponding size
		size_t num_indices = EvaluableNode::GetDeepSize(value.code);
		auto id_entry = valueCodeSizeToIndices.find(num_indices);
		if(id_entry == end(valueCodeSizeToIndices))
		{
			//value must have changed sizes, look in each size
			//note that this is inefficient -- if this ends up being a bottleneck,
			//an additional data structure will need to be built to maintain the previous size
			for(auto cur_id_entry = begin(valueCodeSizeToIndices); cur_id_entry != end(valueCodeSizeToIndices); ++cur_id_entry)
			{
				if(cur_id_entry->second->contains(index))
				{
					id_entry = cur_id_entry;
					break;
				}
			}

			if(id_entry == end(valueCodeSizeToIndices))
				assert(false);
		}

		//remove the entity
		auto &entities = *(id_entry->second);
		entities.erase(index);

		if(entities.size() == 0)
			valueCodeSizeToIndices.erase(id_entry);

		//see if need to update largest code
		if(index == indexWithLargestCode)
			RecomputeLargestCode();
		break;
	}

	default: //shouldn't make it here
		break;
	}
}

void SBFDSColumnData::Optimize()
{
#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForColumn(column_index);
#endif

	if(internedNumberValues.valueInterningEnabled)
	{
		if(AreNumberValuesPreferredToInterns())
		{
			for(auto &value_entry : sortedNumberValueEntries)
			{
				double value = value_entry.first;
				for(auto entity_index : value_entry.second.indicesWithValue)
					valueEntries[entity_index].number = value;
			}

			for(auto entity_index : nullIndices)
				valueEntries[entity_index].number = std::numeric_limits<double>::quiet_NaN();

			ConvertNumberInternsToValues();
		}
	}
	else if(AreNumberInternsPreferredToValues())
	{
		ConvertNumberValuesToInterns();

		for(auto &value_entry : sortedNumberValueEntries)
		{
			size_t value_index = value_entry.second.valueInternIndex;
			for(auto entity_index : value_entry.second.indicesWithValue)
				valueEntries[entity_index].indirectionIndex = value_index;
		}

		for(auto entity_index : nullIndices)
			valueEntries[entity_index].indirectionIndex = SBFDSColumnData::ValueEntry::NULL_INDEX;
	}

	if(internedStringIdValues.valueInterningEnabled)
	{
		if(AreStringIdValuesPreferredToInterns())
		{
			for(auto &[sid, value_entry] : stringIdValueEntries)
			{
				auto value = value_entry->value.stringID;
				for(auto entity_index : value_entry->indicesWithValue)
					valueEntries[entity_index].stringID = value;
			}

			for(auto entity_index : nullIndices)
				valueEntries[entity_index].stringID = StringInternPool::NOT_A_STRING_ID;

			ConvertStringIdInternsToValues();
		}
	}
	else if(AreStringIdInternsPreferredToValues())
	{
		ConvertStringIdValuesToInterns();

		for(auto &[sid, value_entry] : stringIdValueEntries)
		{
			size_t value_index = value_entry->valueInternIndex;
			for(auto entity_index : value_entry->indicesWithValue)
				valueEntries[entity_index].indirectionIndex = value_index;
		}

		for(auto entity_index : nullIndices)
			valueEntries[entity_index].indirectionIndex = SBFDSColumnData::ValueEntry::NULL_INDEX;
	}

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForColumn(column_index);
#endif
}

void SBFDSColumnData::FindAllIndicesWithinRange(EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &low, EvaluableNodeImmediateValue &high, BitArrayIntegerSet &out, bool between_values)
{
	if(value_type == ENIVT_NUMBER)
	{
		//there are no ids for this column, so return no results
		if(sortedNumberValueEntries.size() == 0)
			return;

		//make a copy because passed by reference, and may need to change value for logic below
		double low_number = low.number;
		double high_number = high.number;

		if(FastIsNaN(low_number) || FastIsNaN(high_number))
		{
			//both are NaN, return nothing
			if(FastIsNaN(low_number) && FastIsNaN(high_number))
				return;

			//modify range to include elements from or up to -/+inf
			if(FastIsNaN(low_number)) //find all NaN values and all values up to max
				low_number = -std::numeric_limits<double>::infinity(); //else include elements from -inf to high as well as NaN elements
			else
				high_number = std::numeric_limits<double>::infinity(); //include elements from low to +inf as well as NaN elements
		}

		//handle equality and nonequality case
		if(low_number == high_number)
		{
			auto value_entry = sortedNumberValueEntries.find(low_number);
			if(value_entry == end(sortedNumberValueEntries))
			{
				//if not found but looking for it, then just return
				if(between_values)
					return;
				else //the value doesn't exist, include everything
					numberIndices.CopyTo(out);
			}

			//if within range, and range has no length, just return indices in that one bucket
			if(between_values)
			{
				out.InsertInBatch(value_entry->second.indicesWithValue);
			}
			else //if not within, populate with all indices not equal to value
			{
				for(auto &other_value_entry : sortedNumberValueEntries)
				{
					if(other_value_entry.second.value.number == low_number)
						continue;

					out.InsertInBatch(other_value_entry.second.indicesWithValue);
				}
			}

			return;
		}

		if(between_values)
		{
			for(auto value_entry = sortedNumberValueEntries.lower_bound(low_number);
					value_entry != end(sortedNumberValueEntries) && value_entry->first <= high_number; ++value_entry)
				out.InsertInBatch(value_entry->second.indicesWithValue);
		}
		else //not between_values
		{
			//insert everything left of range
			for(auto &value_entry : sortedNumberValueEntries)
			{
				if(value_entry.first >= low_number)
					break;

				out.InsertInBatch(value_entry.second.indicesWithValue);
			}

			//insert everything right of range
			for(auto value_entry = sortedNumberValueEntries.rbegin();
				value_entry != sortedNumberValueEntries.rend(); ++value_entry)
			{
				if(value_entry->first <= high_number)
					break;

				out.InsertInBatch(value_entry->second.indicesWithValue);
			}
		}

	}
	else if(value_type == ENIVT_STRING_ID)
	{
		if(stringIdValueEntries.size() == 0)
			return;

		//check every string value to see if between
		for(auto &[id, entry] : stringIdValueEntries)
		{
			//check where the string is in the order; empty strings for comparison always pass
			bool value_less_than_low = true;
			if(low.stringID != string_intern_pool.NOT_A_STRING_ID && StringNaturalCompare(low.stringID, id) <= 0)
				value_less_than_low = false;

			bool value_less_than_high = true;
			if(high.stringID != string_intern_pool.NOT_A_STRING_ID && StringNaturalCompare(high.stringID, id) <= 0)
				value_less_than_high = false;

			if(between_values)
			{
				if(value_less_than_low || !value_less_than_high)
					continue;
			}
			else //not between_values
			{
				if(!value_less_than_low && value_less_than_high)
					continue;
			}

			//insert all entities with this value
			for(auto index : entry->indicesWithValue)
				out.insert(index);
		}
	}
}

void SBFDSColumnData::FindMinMax(EvaluableNodeImmediateValueType value_type, size_t num_to_find, bool find_max,
		BitArrayIntegerSet *indices_to_consider, BitArrayIntegerSet &out)
{
	if(value_type == ENIVT_NUMBER)
	{
		//there are no ids for this column, so return no results
		if(sortedNumberValueEntries.size() == 0)
			return;

		if(find_max)
		{
			//can't use a range-based for efficiently because going in reverse order
			for(auto value_entry = sortedNumberValueEntries.rbegin();
				value_entry != sortedNumberValueEntries.rend(); ++value_entry)
			{
				//add each index to the out indices and optionally output compute results
				for(const auto &index : value_entry->second.indicesWithValue)
				{
					if(indices_to_consider != nullptr && !indices_to_consider->contains(index))
						continue;

					out.insert(index);

					//return once we have num_to_find entities
					if(out.size() >= num_to_find)
						return;
				}
			}
		}
		else //find min
		{
			for(auto &value_entry : sortedNumberValueEntries)
			{
				//add each index to the out indices and optionally output compute results
				for(const auto &index : value_entry.second.indicesWithValue)
				{
					if(indices_to_consider != nullptr && !indices_to_consider->contains(index))
						continue;

					out.insert(index);

					//return once we have num_to_find entities
					if(out.size() >= num_to_find)
						return;
				}
			}
		}
	}
	else if(value_type == ENIVT_STRING_ID)
	{
		if(stringIdValueEntries.size() == 0)
			return;

		//else it's a string, need to do it the brute force way
		std::vector<StringInternPool::StringID> all_sids;
		all_sids.reserve(stringIdValueEntries.size());

		//get all strings
		for(auto &[id, _] : stringIdValueEntries)
			all_sids.push_back(id);

		std::sort(begin(all_sids), end(all_sids), StringIDNaturalCompareSort);

		//search left to right for max (bucket 0 is largest) or right to left for min
		int64_t value_index = find_max ? 0 : all_sids.size() - 1;

		while(value_index < static_cast<int64_t>(all_sids.size()) && value_index >= 0)
		{
			const auto &sid_entry = stringIdValueEntries.find(all_sids[value_index]);
			for(auto index : sid_entry->second->indicesWithValue)
			{
				if(indices_to_consider != nullptr && !indices_to_consider->contains(index))
					continue;

				out.insert(index);

				//return once we have num_to_find entities
				if(out.size() >= num_to_find)
					return;
			}

			value_index += find_max ? 1 : -1; //search left to right for max (bucket 0 is largest) or right to left for min
		}
	}
}
