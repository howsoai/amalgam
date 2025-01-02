#pragma once

//project headers:
#include "DistanceReferencePair.h"
#include "EvaluableNode.h"
#include "HashMaps.h"
#include "IntegerSet.h"

//system headers:
#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

//SBFDSColumnData class maintains a sorted linear and random access data collection
//values with the same key are placed into the same bucket.  buckets are stored in sorted order by key
class SBFDSColumnData
{
public:

	struct ValueEntry
	{
		//indicates the column does not use indices
		static constexpr size_t NO_INDEX = std::numeric_limits<size_t>::max();
		//nan value is always the 0th index
		static constexpr size_t NULL_INDEX = 0;

		//if empty, initialize to invalid index
		ValueEntry()
			: value(), indicesWithValue(),
			valueInternIndex(NO_INDEX)
		{	}

		ValueEntry(double number_value, size_t intern_index = NO_INDEX)
			: value(number_value), indicesWithValue(),
			valueInternIndex(intern_index)
		{	}

		ValueEntry(StringInternPool::StringID sid_value, size_t intern_index = NO_INDEX)
			: value(sid_value), indicesWithValue(),
			valueInternIndex(intern_index)
		{	}

		ValueEntry(ValueEntry &ve)
			: value(ve.value), indicesWithValue(ve.indicesWithValue), valueInternIndex(ve.valueInternIndex)
		{	}

		EvaluableNodeImmediateValue value;
		//TODO 22454: attempt changing this to EfficientIntegerSet
		SortedIntegerSet indicesWithValue;
		size_t valueInternIndex;
	};

	//column needs to be named when it is created
	inline SBFDSColumnData(StringInternPool::StringID sid)
		: stringId(sid), indexWithLongestString(0), longestStringLength(0),
		indexWithLargestCode(0), largestCodeSize(0)
	{	}

	//like InsertIndexValue, but used only for building the column data from an empty column
	//this function must be called on each index in ascending order; for example, index 2 must be called after index 1
	void InsertNextIndexValueExceptNumbers(EvaluableNodeImmediateValueType value_type,
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

			auto new_entry_emplacement = sortedNumberValueEntries.try_emplace(value.number, value.number);
			auto &value_entry_iter = new_entry_emplacement.first;
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

	//returns the value type of the given index given the value
	__forceinline EvaluableNodeImmediateValueType GetIndexValueType(size_t index)
	{
		if(numberIndices.contains(index))
		{
			if(internedNumberValues.valueInterningEnabled)
				return ENIVT_NUMBER_INDIRECTION_INDEX;
			return ENIVT_NUMBER;
		}

		if(stringIdIndices.contains(index))
		{
			if(internedStringIdValues.valueInterningEnabled)
				return ENIVT_STRING_ID_INDIRECTION_INDEX;
			return ENIVT_STRING_ID;
		}

		if(nullIndices.contains(index))
			return ENIVT_NULL;
		if(invalidIndices.contains(index))
			return ENIVT_NOT_EXIST;
		return ENIVT_CODE;
	}

	//returns the value type, performing any resolution for intern lookups
	static __forceinline EvaluableNodeImmediateValueType GetResolvedValueType(EvaluableNodeImmediateValueType value_type)
	{
		if(value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
			return ENIVT_NUMBER;
		if(value_type == ENIVT_STRING_ID_INDIRECTION_INDEX)
			return ENIVT_STRING_ID;
		return value_type;
	}

	//returns the value type that represents the values stored in this column, performing the reverse of any resolution for intern lookups
	__forceinline EvaluableNodeImmediateValueType GetUnresolvedValueType(EvaluableNodeImmediateValueType value_type)
	{
		if(value_type == ENIVT_NUMBER && internedNumberValues.valueInterningEnabled)
			return ENIVT_NUMBER_INDIRECTION_INDEX;
		if(value_type == ENIVT_STRING_ID && internedStringIdValues.valueInterningEnabled)
			return ENIVT_STRING_ID_INDIRECTION_INDEX;
		return value_type;
	}

	//returns the value performing any intern lookup if necessary
	__forceinline EvaluableNodeImmediateValue GetResolvedValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue value)
	{
		if(value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
			return EvaluableNodeImmediateValue(internedNumberValues.internedIndexToValue[value.indirectionIndex]);
		if(value_type == ENIVT_STRING_ID_INDIRECTION_INDEX)
			return EvaluableNodeImmediateValue(internedStringIdValues.internedIndexToValue[value.indirectionIndex]);
		return value;
	}

	//moves index from being associated with key old_value to key new_value
	void ChangeIndexValue(EvaluableNodeImmediateValueType new_value_type,
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

		auto old_value_type_resolved = GetResolvedValueType(old_value_type);
		auto old_value_resolved = GetResolvedValue(old_value_type, old_value);
		auto new_value_type_resolved = GetResolvedValueType(new_value_type);
		auto new_value_resolved = GetResolvedValue(new_value_type, new_value);

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
				auto new_value_entry_emplacement = sortedNumberValueEntries.try_emplace(new_number_value, new_number_value);
				auto &new_value_entry = new_value_entry_emplacement.first;
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

					new_value_entry->second.indicesWithValue.insert(index);

					//if new value didn't exist exists, insert it properly
					if(new_value_entry_emplacement.second)
						internedNumberValues.InsertValueEntry(new_value_entry->second, sortedNumberValueEntries.size());

					new_value_index = new_value_entry->second.valueInternIndex;
				}
				else //shouldn't make it here, but ensure integrity just in case
				{
					assert(false);
					new_value_entry->second.indicesWithValue.insert(index);
					internedNumberValues.InsertValueEntry(new_value_entry->second, sortedNumberValueEntries.size());
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

	//deletes everything involving the value at the index
	void DeleteIndexValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue value, size_t index)
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

			auto resolved_value = GetResolvedValue(value_type, value);

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

			auto resolved_value = GetResolvedValue(value_type, value);

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

	//inserts a particular value based on the value_index
	//templated to make it efficiently work regardless of the container
	template<typename StringIdValueEntryIterator>
	void InsertFirstIndexIntoStringIdValueEntry(size_t index, StringIdValueEntryIterator &value_iter)
	{
		auto &value_entry = *(value_iter->second.get());

		value_entry.indicesWithValue.insert(index);
		internedStringIdValues.InsertValueEntry(value_entry, stringIdValueEntries.size());
	}

	//inserts the value at id
	//returns the value that should be used to reference the value, which may be an index
	//depending on the state of the column data
	void InsertIndexValue(EvaluableNodeImmediateValueType value_type,
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

			double number_value = GetResolvedValue(value_type, value).number;

			auto value_emplacement = sortedNumberValueEntries.try_emplace(number_value, number_value);
			auto &value_entry = value_emplacement.first;
			if(!value_emplacement.second)
			{
				value_entry->second.indicesWithValue.insert(index);

				if(internedNumberValues.valueInterningEnabled)
					valueEntries[index] = EvaluableNodeImmediateValue(value_entry->second.valueInternIndex);
				else
					valueEntries[index] = value;

				return;
			}

			value_entry->second.indicesWithValue.insert(index);

			if(internedNumberValues.valueInterningEnabled)
			{
				internedNumberValues.InsertValueEntry(value_entry->second, sortedNumberValueEntries.size());
				valueEntries[index] = value_entry->second.valueInternIndex;
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

			auto string_id = GetResolvedValue(value_type, value).stringID;

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

	//returns the number of unique values in the column
	//if value_type is ENIVT_NULL, then it will include all types, otherwise it will only consider
	//the unique values for the type requested
	inline size_t GetNumUniqueValues(EvaluableNodeImmediateValueType value_type = ENIVT_NULL)
	{
		if(value_type == ENIVT_NUMBER)
			return sortedNumberValueEntries.size();

		if(value_type == ENIVT_STRING_ID)
			return stringIdValueEntries.size();

		//add up unique number and string values,
		// and use a heuristic for judging how many unique code values there are
		return sortedNumberValueEntries.size() + stringIdIndices.size()
			+ (valueCodeSizeToIndices.size() + codeIndices.size()) / 2;
	}

	//returns the number of valid values (exist and not null) in the column
	inline size_t GetNumValidDataElements()
	{
		return numberIndices.size() + stringIdIndices.size() + codeIndices.size();
	}

	//returns the maximum difference between value and any other value for this column
	//if empty, will return infinity
	inline double GetMaxDifferenceTerm(GeneralizedDistanceEvaluator::FeatureAttributes &feature_attribs)
	{
		switch(feature_attribs.featureType)
		{
		case GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMERIC:
		case GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING:
		case GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE:
			return 1.0 - 1.0 / GetNumValidDataElements();

		case GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMERIC:
			if(sortedNumberValueEntries.size() <= 1)
				return 0.0;

			return sortedNumberValueEntries.rbegin()->second.value.number
				- sortedNumberValueEntries.begin()->second.value.number;

		case GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMERIC_CYCLIC:
			//maximum is the other side of the cycle
			return feature_attribs.typeAttributes.maxCyclicDifference / 2;

		case GeneralizedDistanceEvaluator::FDT_CONTINUOUS_STRING:
			//the max difference is the worst case edit distance, of removing all the characters
			// and then adding back in another of equal size but different
			return static_cast<double>(longestStringLength * 2);

		case GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE:
			//the max difference is the worst case edit distance, of removing all the characters
			// and then adding back in another of equal size but different
			return static_cast<double>(largestCodeSize * 2);

		default:
			return std::numeric_limits<double>::infinity();
		}
	}

	//given a value, returns the entry at which the value should be inserted into the sortedNumberValueEntries
	//cycle_length will take into account whether wrapping around is closer
	inline auto FindClosestValueEntryForNumberValue(double value, double cycle_length = std::numeric_limits<double>::infinity())
	{
		//if only have one element (or zero), short circuit code below
		if(sortedNumberValueEntries.size() <= 1)
			return begin(sortedNumberValueEntries);

		auto value_entry_lb = sortedNumberValueEntries.lower_bound(value);
		//if exact match, just return
		if(value_entry_lb != end(sortedNumberValueEntries) && value_entry_lb->first == value)
			return value_entry_lb;

		if(cycle_length != std::numeric_limits<double>::infinity())
		{
			if(value_entry_lb == begin(sortedNumberValueEntries) || value_entry_lb == end(sortedNumberValueEntries))
			{
				auto min_value_entry = sortedNumberValueEntries.begin();
				double dist_to_min_entry = cycle_length - std::abs(min_value_entry->first - value);

				auto max_value_entry = --sortedNumberValueEntries.end();
				double dist_to_max_entry = cycle_length - std::abs(max_value_entry->first - value);

				if(dist_to_max_entry < dist_to_min_entry)
					return max_value_entry;
				return min_value_entry;
			}
		}
		else //not cyclic, check ends
		{
			//either not cyclic, or in the middle
			if(value_entry_lb == begin(sortedNumberValueEntries))
				return value_entry_lb;
			if(value_entry_lb == end(sortedNumberValueEntries))
				return --sortedNumberValueEntries.end();
		}

		//in the middle, check value_entry_lb and one below it
		auto lower_value_entry = value_entry_lb;
		--lower_value_entry;
		if(std::abs(value_entry_lb->first - value) < std::abs(lower_value_entry->first - value))
			return value_entry_lb;
		return lower_value_entry;
	}

	//given a feature_id and a range [low, high], inserts all the elements with values of feature feature_id within specified range into out; does not clear out
	//if the feature value is null, it will NOT be present in the search results, ie "x" != 3 will NOT include elements with x is null, even though null != 3
	void FindAllIndicesWithinRange(EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &low, EvaluableNodeImmediateValue &high, BitArrayIntegerSet &out, bool between_values = true)
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

	//given a value, inserts into out all the entities that have the value
	// does not handle ENIVT_CODE because it doesn't have the data
	void UnionAllIndicesWithValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue &value, BitArrayIntegerSet &out)
	{
		if(value_type == ENIVT_NOT_EXIST)
			return;

		if(value_type == ENIVT_NULL)
		{
			//only want nulls that are not numbers
			nullIndices.UnionTo(out);
		}
		else if(value_type == ENIVT_NUMBER)
		{
			auto value_entry = sortedNumberValueEntries.find(value.number);
			if(value_entry != end(sortedNumberValueEntries))
				out.InsertInBatch(value_entry->second.indicesWithValue);
		}
		else if(value_type == ENIVT_STRING_ID)
		{
			auto id_entry = stringIdValueEntries.find(value.stringID);
			if(id_entry != end(stringIdValueEntries))
				out.InsertInBatch(id_entry->second->indicesWithValue);
		}
	}

	//fills out with the num_to_find min (if findMax == false) or max (find_max == true) entities in the database
	//note, if indices_to_consider is not nullptr, will take the intersect, ie out will be set to the num_to_find min or max elements that exist in input indices_to_consider
	void FindMinMax(EvaluableNodeImmediateValueType value_type, size_t num_to_find, bool find_max,
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

	//returns true if switching to number interning would be expected to yield better results
	// than number values given the current data
	inline bool AreNumberInternsPreferredToValues()
	{
	#ifdef FORCE_SBFDS_VALUE_INTERNING
		return true;
	#endif
	#ifdef DISABLE_SBFDS_VALUE_INTERNING
		return false;
	#endif
		size_t num_indices = numberIndices.size();
		if(num_indices < 100)
			return false;

		//use heuristic of sqrt number of values compared to num unique values
		// (but computed with a multiply instead of sqrt)
		size_t num_unique_values = sortedNumberValueEntries.size();

		return (num_unique_values * num_unique_values <= num_indices);
		//return (3 * num_unique_values * num_unique_values / 2 <= num_indices);
	}

	//returns true if switching to number values would be expected to yield better results
	// than interning given the current data
	inline bool AreNumberValuesPreferredToInterns()
	{
	#ifdef FORCE_SBFDS_VALUE_INTERNING
		return false;
	#endif
	#ifdef DISABLE_SBFDS_VALUE_INTERNING
		return true;
	#endif
		size_t num_indices = numberIndices.size();
		if(num_indices < 90)
			return true;

		//TODO 22454: revisit this logic based on scale of data and number of operations needed and if can replace SIS data storage for entity-values (experiment w/ asteroid data set)
		//try: 0.8 * numberIndices.size() ^ 0.65, revert when 1.5x above that number of values, and make a general function
		//and/or try current value times 1.5 via * 3 / 2 to keep as integer

		//use heuristic of sqrt number of values compared to num unique values
		// (but computed with a multiply instead of sqrt)
		//round up to reduce flipping back and forth
		size_t num_unique_values = sortedNumberValueEntries.size();

		return (num_unique_values * num_unique_values > num_indices - num_unique_values);
		//return (3 * num_unique_values * num_unique_values / 2 > num_indices - num_unique_values);
	}

	//returns true if switching to StringId interning would be expected to yield better results
	// than StringId values given the current data
	inline bool AreStringIdInternsPreferredToValues()
	{
	#ifdef FORCE_SBFDS_VALUE_INTERNING
		return true;
	#endif
	#ifdef DISABLE_SBFDS_VALUE_INTERNING
		return false;
	#endif
		size_t num_indices = stringIdIndices.size();
		if(num_indices < 100)
			return false;

		//use heuristic of sqrt number of values compared to num unique values
		// (but computed with a multiply instead of sqrt)
		size_t num_unique_values = stringIdValueEntries.size();

		return (num_unique_values * num_unique_values <= num_indices);
		//return (3 * num_unique_values * num_unique_values / 2 <= num_indices);
	}

	//returns true if switching to StringID values would be expected to yield better results
	// than interning given the current data
	inline bool AreStringIdValuesPreferredToInterns()
	{
	#ifdef FORCE_SBFDS_VALUE_INTERNING
		return false;
	#endif
	#ifdef DISABLE_SBFDS_VALUE_INTERNING
		return true;
	#endif
		size_t num_indices = stringIdIndices.size();
		if(num_indices < 90)
			return true;

		//use heuristic of sqrt number of values compared to num unique values
		// (but computed with a multiply instead of sqrt)
		//round up to reduce flipping back and forth
		size_t num_unique_values = stringIdValueEntries.size();

		return (num_unique_values * num_unique_values > num_indices - num_unique_values);
		//return (3 * num_unique_values * num_unique_values / 2 > num_indices - num_unique_values);
	}

	//clears number intern caches and changes state to not perform interning for numbers
	inline void ConvertNumberInternsToValues()
	{
		internedNumberValues.ClearInterning();
	}

	//initializes and sets up number value interning caches and changes state to perform interning for numbers
	inline void ConvertNumberValuesToInterns()
	{
		internedNumberValues.ConvertValueCollectionToInterns(sortedNumberValueEntries,
			[](auto &value_entry_iter) -> ValueEntry & { return value_entry_iter.second; });
	}

	//clears string intern caches and changes state to not perform interning for StringIds
	inline void ConvertStringIdInternsToValues()
	{
		internedStringIdValues.ClearInterning();
	}

	//initializes and sets up number value interning caches and changes state to perform interning for StringIds
	inline void ConvertStringIdValuesToInterns()
	{
		internedStringIdValues.ConvertValueCollectionToInterns(stringIdValueEntries,
			[](auto &value_entry_iter) -> ValueEntry & { return *(value_entry_iter.second.get()); });
	}

protected:

	//updates longestStringLength and indexWithLongestString based on parameters
	inline void UpdateLongestString(StringInternPool::StringID sid, size_t index)
	{
		auto &str = string_intern_pool.GetStringFromID(sid);
		size_t str_size = StringManipulation::GetUTF8CharacterLength(str);
		if(str_size > longestStringLength)
		{
			longestStringLength = str_size;
			indexWithLongestString = index;
		}
	}

	//should be called when the longest string is invalidated
	inline void RecomputeLongestString()
	{
		longestStringLength = 0;
		//initialize to 0 in case there are no entities with strings
		indexWithLongestString = 0;
		for(auto &[s_id, s_entry] : stringIdValueEntries)
			UpdateLongestString(s_id, s_entry->indicesWithValue.GetNthElement(0));
	}

	//updates largestCodeSize and indexWithLargestCode based on parameters
	inline void UpdateLargestCode(size_t code_size, size_t index)
	{
		if(code_size > largestCodeSize)
		{
			largestCodeSize = code_size;
			indexWithLargestCode = index;
		}
	}

	//should be called when the largest code is invalidated
	inline void RecomputeLargestCode()
	{
		largestCodeSize = 0;
		//initialize to 0 in case there are no entities with code
		indexWithLargestCode = 0;
		for(auto &[size, entry] : valueCodeSizeToIndices)
			UpdateLargestCode(size, *entry->begin());
	}

public:

	//name of the column
	StringInternPool::StringID stringId;

	//for each index, stores the value
	std::vector<EvaluableNodeImmediateValue> valueEntries;

	//stores values in sorted order and the entities that have each value
	std::map<double, ValueEntry> sortedNumberValueEntries;

	//maps a string id to a vector of indices that have that string
	CompactHashMap<StringInternPool::StringID, std::unique_ptr<ValueEntry>> stringIdValueEntries;

	//for any value that doesn't fit into other values ( ENIVT_CODE ), maps the number of elements in the code
	// to the indices of the same size
	CompactHashMap<size_t, std::unique_ptr<SortedIntegerSet>> valueCodeSizeToIndices;

	//indices of entities with no value for this feature
	EfficientIntegerSet invalidIndices;

	//indices of entities with a number value for this feature
	EfficientIntegerSet numberIndices;

	//indices of entities with a string id value for this feature
	EfficientIntegerSet stringIdIndices;

	//indices of entities with a null for this feature
	EfficientIntegerSet nullIndices;

	//indices that don't fall into the number/string/null types but are valid
	EfficientIntegerSet codeIndices;

	//entity index with the longest string value for this label
	size_t indexWithLongestString;
	//the longest string length for this label
	size_t longestStringLength;

	//entity index with the largest code size for this label
	size_t indexWithLargestCode;
	//the largest code size for this label
	size_t largestCodeSize;

	template<typename ValueType>
	class InternedValues
	{
	public:
		InternedValues()
			: valueInterningEnabled(false)
		{	}

		//clears all interning and cleans up data structures
		inline void ClearInterning()
		{
			if(!valueInterningEnabled)
				return;

			internedIndexToValue.clear();
			unusedValueIndices.clear();
			valueInterningEnabled = false;
		}

		//converts the values in value_collection into interned values
		template<typename ValueEntryCollectionType, typename GetValueEntryFunction>
		inline void ConvertValueCollectionToInterns(ValueEntryCollectionType &value_collection, GetValueEntryFunction get_value_entry)
		{
			if(valueInterningEnabled)
				return;

			//include extra entry for different type value
			internedIndexToValue.resize(value_collection.size() + 1);
			internedIndexToValue[0] = notAValue;

			size_t intern_index = 1;
			for(auto &value_entry_iter : value_collection)
			{
				auto &value_entry = get_value_entry(value_entry_iter);
				value_entry.valueInternIndex = intern_index;
				internedIndexToValue[intern_index] = value_entry.value;
				intern_index++;
			}

			valueInterningEnabled = true;
		}

		//if interning is enabled, inserts value_entry and populates it with the appropriate intern index
		//total_num_values is the number of unique values
		inline void InsertValueEntry(ValueEntry &value_entry, size_t total_num_values)
		{
			if(!valueInterningEnabled)
				return;

			if(value_entry.valueInternIndex == ValueEntry::NO_INDEX)
			{
				//get the highest value 
				if(unusedValueIndices.size() > 0)
				{
					value_entry.valueInternIndex = unusedValueIndices.top();

					//make sure the value is valid
					if(value_entry.valueInternIndex < total_num_values)
					{
						unusedValueIndices.pop();
					}
					else //not valid, clear queue
					{
						unusedValueIndices.clear();
						//just use a new value, 0-based but leaving a spot open for NULL_INDEX
						value_entry.valueInternIndex = total_num_values;
					}
				}
				else //just use new value of the latest size, 0-based but leaving a spot open for NULL_INDEX
				{
					value_entry.valueInternIndex = total_num_values;
				}
			}

			if(value_entry.valueInternIndex >= internedIndexToValue.size())
				internedIndexToValue.resize(value_entry.valueInternIndex + 1, notAValue);

			internedIndexToValue[value_entry.valueInternIndex] = value_entry.value;
		}

		//if interning is enabled, updates internedIndexToValue with the appropriate
		//new value for value_entry
		inline void UpdateInternIndexValue(ValueEntry &value_entry, ValueType value)
		{
			if(!valueInterningEnabled)
				return;

			internedIndexToValue[value_entry.valueInternIndex] = value;
		}

		//deletes the intern index if interning is enabled
		inline void DeleteInternIndex(size_t intern_index)
		{
			if(!valueInterningEnabled)
				return;

			//if the last entry (off by one, including ValueEntry::NO_INDEX), can just resize
			if(intern_index == internedIndexToValue.size() - 1)
			{
				internedIndexToValue.resize(intern_index);
			}
			else //need to actually erase it
			{
				internedIndexToValue[intern_index] = notAValue;
				unusedValueIndices.emplace(intern_index);
			}

			//clear out any unused internedIndexToValue at the end other than the 0th entry
			while(internedIndexToValue.size() > 1 && IsNotAValue(internedIndexToValue.back()))
				internedIndexToValue.pop_back();
		}

		//returns true if the value is equal to notAValue
		//note that this is a method due to not-a-number being treated as not equal to itself
		__forceinline bool IsNotAValue(ValueType value)
		{
			if constexpr(std::is_same_v<ValueType, double>)
				return FastIsNaN(value);
			else
				return (value == notAValue);
		}

		//special value that represents a value of a different type or null
		static constexpr ValueType notAValue = []()
			{
				if constexpr(std::is_same_v<ValueType, double>)
					return std::numeric_limits<double>::quiet_NaN();
				else if constexpr(std::is_same_v<ValueType, StringInternPool::StringID>)
					return string_intern_pool.NOT_A_STRING_ID;
				else
					return ValueType();
			}();

		//if valueInterningEnabled is true, then contains each value for the given index
		//if a given index isn't used, then it will contain the maximum value for the index
		//the 0th index is reserved for values that are not of type ValueType,
		//regardless of whether such values appear in the data
		std::vector<ValueType> internedIndexToValue;

		//unused / free indices in internedIndexToValue to make adding and removing new values efficient
		//always want to fetch the lowest index to keep the interned NumberIndexToNumberValue small
		FlexiblePriorityQueue<size_t, std::vector<size_t>, std::greater<size_t>> unusedValueIndices;

		//if true, then the indices of the values should be used and internedIndexToValue populated
		bool valueInterningEnabled;
	};

	//object that contains interned number values if applicable
	InternedValues<double> internedNumberValues;
	InternedValues<StringInternPool::StringID> internedStringIdValues;
};
