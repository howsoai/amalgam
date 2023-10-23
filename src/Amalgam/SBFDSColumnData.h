#pragma once

//project headers:
#include "DistanceReferencePair.h"
#include "EvaluableNode.h"
#include "HashMaps.h"
#include "IntegerSet.h"

//system headers:
#include <algorithm>
#include <memory>
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
		static constexpr size_t NAN_INDEX = 0;

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
		SortedIntegerSet indicesWithValue;
		size_t valueInternIndex;
	};

	//column needs to be named when it is created
	inline SBFDSColumnData(StringInternPool::StringID sid)
		: stringId(sid), indexWithLongestString(0), longestStringLength(0),
		indexWithLargestCode(0), largestCodeSize(0), numberValuesInterned(false)
	{	}

	//like InsertIndexValue, but used only for building the column data from an empty column
	//this function must be called on each index in ascending order; for example, index 2 must be called after index 1
	//inserts number values in entities_with_number_values
	//AppendSortedNumberIndicesWithSortedIndices should be called after all indices are inserted
	void InsertNextIndexValueExceptNumbers(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue &value,
		size_t index, std::vector<DistanceReferencePair<size_t>> &entities_with_number_values)
	{
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
			if(FastIsNaN(value.number))
				nanIndices.insert(index);
			else
				entities_with_number_values.emplace_back(value.number, index);
		}
		else if(value_type == ENIVT_STRING_ID)
		{
			stringIdIndices.insert(index);

			//try to insert the value if not already there, inserting an empty pointer
			auto [id_entry, inserted] = stringIdValueToIndices.emplace(value.stringID, nullptr);
			if(inserted)
				id_entry->second = std::make_unique<SortedIntegerSet>();

			auto &ids = id_entry->second;

			ids->InsertNewLargestInteger(index);

			UpdateLongestString(value.stringID, index);
		}
		else if(value_type == ENIVT_CODE)
		{
			codeIndices.insert(index);

			//find the entities that have the correspending size; if the size doesn't exist, create it
			size_t code_size = EvaluableNode::GetDeepSize(value.code);

			auto [size_entry, inserted] = valueCodeSizeToIndices.emplace(code_size, nullptr);
			if(inserted)
				size_entry->second = std::make_unique<SortedIntegerSet>();

			//add the entity
			size_entry->second->insert(index);

			UpdateLargestCode(code_size, index);
		}
	}

	//inserts indices assuming that they have been sorted by value,
	// and that index_values are also sorted from smallest to largest
	void AppendSortedNumberIndicesWithSortedIndices(std::vector<DistanceReferencePair<size_t>> &index_values)
	{
		if(index_values.size() == 0)
			return;

		//count unique values so only need to perform one allocation for the main list
		size_t num_uniques = 1;
		double prev_value = index_values[0].distance;
		for(size_t i = 1; i < index_values.size(); i++)
		{
			if(prev_value != index_values[i].distance)
			{
				num_uniques++;
				prev_value = index_values[i].distance;
			}
		}

		sortedNumberValueEntries.reserve(num_uniques);
		numberIndices.ReserveNumIntegers(index_values.back().reference + 1);

		for(auto &index_value : index_values)
		{
			//if don't have the right bucket, then need to create one
			if(sortedNumberValueEntries.size() == 0 || sortedNumberValueEntries.back()->value.number != index_value.distance)
				sortedNumberValueEntries.emplace_back(std::make_unique<ValueEntry>(index_value.distance));

			sortedNumberValueEntries.back()->indicesWithValue.InsertNewLargestInteger(index_value.reference);
			numberIndices.insert(index_value.reference);
		}
	}

	//returns the value type of the given index given the value
	__forceinline EvaluableNodeImmediateValueType GetIndexValueType(size_t index)
	{
		if(numberIndices.contains(index))
		{
			if(numberValuesInterned)
				return ENIVT_NUMBER_INDIRECTION_INDEX;
			return ENIVT_NUMBER;
		}

		if(stringIdIndices.contains(index))
			return ENIVT_STRING_ID;
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
		return value_type;
	}

	//returns the value type that represents the values stored in this column, performing the reverse of any resolution for intern lookups
	__forceinline EvaluableNodeImmediateValueType GetUnresolvedValueType(EvaluableNodeImmediateValueType value_type)
	{
		if(value_type == ENIVT_NUMBER && numberValuesInterned)
			return ENIVT_NUMBER_INDIRECTION_INDEX;
		return value_type;
	}

	//returns the value performing any intern lookup if necessary
	__forceinline EvaluableNodeImmediateValue GetResolvedValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue value)
	{
		if(value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
			return EvaluableNodeImmediateValue(internedNumberIndexToNumberValue[value.indirectionIndex]);
		return value;
	}

	//moves index from being associated with key old_value to key new_value
	//returns the value that should be used to reference the value, which may be an index
	//depending on the state of the column data
	EvaluableNodeImmediateValue ChangeIndexValue(EvaluableNodeImmediateValueType old_value_type, EvaluableNodeImmediateValue old_value,
		EvaluableNodeImmediateValueType new_value_type, EvaluableNodeImmediateValue new_value, size_t index)
	{
		//if new one is invalid, can quickly delete or return
		if(new_value_type == ENIVT_NOT_EXIST)
		{
			if(!invalidIndices.contains(index))
			{
				DeleteIndexValue(old_value_type, old_value, index);
				invalidIndices.insert(index);
			}

			if(numberValuesInterned)
				return EvaluableNodeImmediateValue(ValueEntry::NAN_INDEX);
			else
				return EvaluableNodeImmediateValue();
		}

		//if the types are the same, some shortcuts may apply
		if(old_value_type == new_value_type)
		{
			if(old_value_type == ENIVT_STRING_ID)
			{
				if(old_value.stringID == new_value.stringID)
					return new_value;

				//try to insert the new value if not already there
				auto [new_id_entry, inserted] = stringIdValueToIndices.emplace(new_value.stringID, nullptr);
			
				auto old_id_entry = stringIdValueToIndices.find(old_value.stringID);
				if(old_id_entry != end(stringIdValueToIndices))
				{
					//if there are multiple entries for this string, just move the id
					if(old_id_entry->second->size() > 1)
					{
						if(inserted)
							new_id_entry->second = std::make_unique<SortedIntegerSet>();
			
						new_id_entry->second->insert(index);
						old_id_entry->second->erase(index);
					}
					else //it's the last old_id_entry
					{
						//put the SortedIntegerSet in the new value or move the container
						if(inserted)
							new_id_entry->second = std::move(old_id_entry->second);
						else
							new_id_entry->second->insert(index);
			
						//erase after no longer need inserted_id_entry, as it may be invalidated
						stringIdValueToIndices.erase(old_id_entry);
					}
				}
			
				//update longest string as appropriate
				if(index == indexWithLongestString)
					RecomputeLongestString();
				else
					UpdateLongestString(new_value.stringID, index);
			
				return new_value;
			}

			//TODO 17861: add logic for code, numbers and interned numbers
		}

		//delete index at old value
		DeleteIndexValue(old_value_type, old_value, index);

		//add index at new value bucket
		return InsertIndexValue(new_value_type, new_value, index);
	}

	//deletes everything involving the value at the index
	void DeleteIndexValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue value, size_t index)
	{
		if(invalidIndices.EraseAndRetrieve(index))
			return;

		//if value is null, just need to remove from the appropriate index
		if(nullIndices.EraseAndRetrieve(index))
			return;

		if(numberIndices.EraseAndRetrieve(index))
		{
			//remove, and if not a nan, then need to also remove the number
			if(!nanIndices.EraseAndRetrieve(index))
			{
				auto resolved_value = GetResolvedValue(value_type, value);

				//look up value
				auto [value_index, exact_index_found] = FindExactIndexForValue(resolved_value.number);
				if(!exact_index_found)
					return;

				//if the bucket has only one entry, we must delete the entire bucket
				if(sortedNumberValueEntries[value_index]->indicesWithValue.size() == 1)
				{
					if(numberValuesInterned)
					{
						size_t value_intern_index = sortedNumberValueEntries[value_index]->valueInternIndex;
						//if the last entry, can just resize
						if(value_intern_index == internedNumberIndexToNumberValue.size() - 1)
						{
							internedNumberIndexToNumberValue.resize(value_intern_index);
						}
						else //need to actually erase it
						{
							internedNumberIndexToNumberValue[value_intern_index] = std::numeric_limits<double>::quiet_NaN();
							unusedNumberValueIndices.emplace(value_intern_index);
						}

						//clear out any unusedNumberValueIndices at the end other than the 0th entry
						while(internedNumberIndexToNumberValue.size() > 1 && FastIsNaN(internedNumberIndexToNumberValue.back()))
							internedNumberIndexToNumberValue.pop_back();
					}

					sortedNumberValueEntries.erase(sortedNumberValueEntries.begin() + value_index);
				}
				else //else we can just remove the id from the bucket
				{
					sortedNumberValueEntries[value_index]->indicesWithValue.erase(index);
				}
			}

			return;
		}

		if(stringIdIndices.EraseAndRetrieve(index))
		{
			auto id_entry = stringIdValueToIndices.find(value.stringID);
			if(id_entry != end(stringIdValueToIndices))
			{
				auto &entities = *(id_entry->second);
				entities.erase(index);
				
				//if no more entries have the value, remove it
				if(entities.size() == 0)
					stringIdValueToIndices.erase(id_entry);
			}

			//see if need to compute new longest string
			if(index == indexWithLongestString)
				RecomputeLongestString();

			return;
		}

		//if made it here, then just remove from a code value type
		codeIndices.erase(index);

		//find the entities that have the correspending size
		size_t num_indices = EvaluableNode::GetDeepSize(value.code);
		auto id_entry = valueCodeSizeToIndices.find(num_indices);
		if(id_entry == end(valueCodeSizeToIndices))
			return;

		//remove the entity
		auto &entities = *(id_entry->second);
		entities.erase(index);

		if(entities.size() == 0)
			valueCodeSizeToIndices.erase(id_entry);

		//see if need to update largest code
		if(index == indexWithLargestCode)
			RecomputeLargestCode();
	}

	//inserts the value at id
	//returns the value that should be used to reference the value, which may be an index
	//depending on the state of the column data
	EvaluableNodeImmediateValue InsertIndexValue(EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &value, size_t index)
	{
		if(value_type == ENIVT_NOT_EXIST)
		{
			invalidIndices.insert(index);

			if(numberValuesInterned)
				return EvaluableNodeImmediateValue(ValueEntry::NAN_INDEX);
			else
				return value;
		}

		if(value_type == ENIVT_NULL)
		{
			nullIndices.insert(index);

			if(numberValuesInterned)
				return EvaluableNodeImmediateValue(ValueEntry::NAN_INDEX);
			else
				return value;
		}

		if(value_type == ENIVT_NUMBER || value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
		{
			numberIndices.insert(index);

			double number_value = GetResolvedValue(value_type, value).number;
			if(FastIsNaN(number_value))
			{
				nanIndices.insert(index);

				if(numberValuesInterned)
					return EvaluableNodeImmediateValue(ValueEntry::NAN_INDEX);
				else
					return value;
			}
			
			//if the value already exists, then put the index in the list
			auto [value_index, exact_index_found] = FindExactIndexForValue(number_value);
			if(exact_index_found)
			{
				sortedNumberValueEntries[value_index]->indicesWithValue.insert(index);

				if(numberValuesInterned)
					return EvaluableNodeImmediateValue(sortedNumberValueEntries[value_index]->valueInternIndex);
				else
					return value;
			}

			//insert new value in correct position
			size_t new_value_index = FindUpperBoundIndexForValue(number_value);
			auto inserted = sortedNumberValueEntries.emplace(sortedNumberValueEntries.begin() + new_value_index,
				std::make_unique<ValueEntry>(number_value));

			ValueEntry *value_entry = inserted->get();
			value_entry->indicesWithValue.insert(index);

			if(numberValuesInterned)
			{
				if(value_entry->valueInternIndex == ValueEntry::NO_INDEX)
				{
					//get the highest value 
					if(unusedNumberValueIndices.size() > 0)
					{
						value_entry->valueInternIndex = unusedNumberValueIndices.top();

						//make sure the value is valid
						if(value_entry->valueInternIndex < sortedNumberValueEntries.size())
						{
							unusedNumberValueIndices.pop();
						}
						else //not valid, clear queue
						{
							unusedNumberValueIndices.clear();
							//just use a new value, leaving a spot open for NAN_INDEX
							value_entry->valueInternIndex = sortedNumberValueEntries.size() + 1;
						}
					}
					else //just use new value
					{
						value_entry->valueInternIndex = sortedNumberValueEntries.size() + 1;
					}

					if(value_entry->valueInternIndex >= internedNumberIndexToNumberValue.size())
						internedNumberIndexToNumberValue.resize(value_entry->valueInternIndex + 1, std::numeric_limits<double>::quiet_NaN());

					internedNumberIndexToNumberValue[value_entry->valueInternIndex] = number_value;
				}

				return value_entry->valueInternIndex;
			}
			else //just return the value
				return value;
		}

		if(value_type == ENIVT_STRING_ID)
		{
			stringIdIndices.insert(index);

			//try to insert the value if not already there
			auto [inserted_id_entry, inserted] = stringIdValueToIndices.emplace(value.stringID, nullptr);
			if(inserted)
				inserted_id_entry->second = std::make_unique<SortedIntegerSet>();

			auto &ids = *(inserted_id_entry->second);
			ids.insert(index);

			UpdateLongestString(value.stringID, index);
			return value;
		}

		//value_type == ENIVT_CODE
		codeIndices.insert(index);

		//find the entities that have the correspending size; if the size doesn't exist, create it
		size_t code_size = EvaluableNode::GetDeepSize(value.code);

		auto [size_entry, inserted] = valueCodeSizeToIndices.emplace(code_size, nullptr);
		if(inserted)
			size_entry->second = std::make_unique<SortedIntegerSet>();

		//add the entity
		size_entry->second->insert(index);

		UpdateLargestCode(code_size, index);

		return value;
	}

	//returns the maximum difference between value and any other value for this column
	//if empty, will return infinity
	inline double GetMaxDifferenceTermFromValue(GeneralizedDistance::FeatureParams &feature_params, EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue &value)
	{
		switch(feature_params.featureType)
		{
		case GeneralizedDistance::FDT_NOMINAL:
			return 1.0;

		case GeneralizedDistance::FDT_CONTINUOUS_NUMERIC:
			if(sortedNumberValueEntries.size() <= 1)
				return 0.0;

			return sortedNumberValueEntries.back()->value.number - sortedNumberValueEntries[0]->value.number;

		case GeneralizedDistance::FDT_CONTINUOUS_NUMERIC_CYCLIC:
			//maximum is the other side of the cycle
			return feature_params.typeAttributes.maxCyclicDifference / 2;

		case GeneralizedDistance::FDT_CONTINUOUS_STRING:
			//the max difference is the worst case edit distance, of removing all the characters
			// and adding all the new ones
			if(value_type == ENIVT_STRING_ID)
			{
				auto &s = string_intern_pool.GetStringFromID(value.stringID);
				return static_cast<double>(longestStringLength + StringManipulation::GetNumUTF8Characters(s));
			}
			else if(value_type == ENIVT_NULL)
			{
				//if null, then could potentially have to remove a string, then add a new one, so counts as double
				return static_cast<double>(longestStringLength * 2);
			}
			else //not a string, so just count distance of adding the string plus one to remove the non-string value
			{
				return static_cast<double>(longestStringLength + 1);
			}

		case GeneralizedDistance::FDT_CONTINUOUS_CODE:
			if(value_type == ENIVT_CODE)
				return static_cast<double>(largestCodeSize + EvaluableNode::GetDeepSize(value.code));
			else if(value_type == ENIVT_NULL)
				//if null, then could potentially have to remove a the code, then add a all new, so counts as double
				return static_cast<double>(largestCodeSize * 2);
			else //all other immediate types have a size of 1
				return static_cast<double>(largestCodeSize + 1);

		default:
			return std::numeric_limits<double>::infinity();
		}
	}

	//returns the exact index of value
	//Same as std::binary_search but returns both index and if found
	// .first: found index - if not found, returns closest index from lower_bound if
	// return_index_lower_bound is set, -1 otherwise
	// .second: true if exact index was found, false otherwise
	inline std::pair<size_t, bool> FindExactIndexForValue(double value, bool return_index_lower_bound = false)
	{
		auto target_iter = std::lower_bound(begin(sortedNumberValueEntries), end(sortedNumberValueEntries), value,
			[](const auto &value_entry, double value)
			{
				return value_entry->value.number < value;
			});

		if((target_iter == end(sortedNumberValueEntries)) || ((*target_iter)->value.number != value)) // not exact match
		{
			return std::make_pair(return_index_lower_bound ? std::distance(begin(sortedNumberValueEntries), target_iter) : -1 , false);
		}

		return std::make_pair(std::distance(begin(sortedNumberValueEntries), target_iter), true); // exact match
	}

	//returns the index of the lower bound of value
	inline size_t FindLowerBoundIndexForValue(double value)
	{
		auto target_iter = std::lower_bound(begin(sortedNumberValueEntries), end(sortedNumberValueEntries), value,
			[](const auto &value_entry, double value)
			{
				return value_entry->value.number < value;
			});
		return std::distance(begin(sortedNumberValueEntries), target_iter);
	}

	//returns the index of the upper bound of value
	inline size_t FindUpperBoundIndexForValue(double value)
	{
		auto target_iter = std::upper_bound(begin(sortedNumberValueEntries), end(sortedNumberValueEntries), value,
			[](double value, const auto &value_entry)
			{
				return value < value_entry->value.number;
			});
		return std::distance(begin(sortedNumberValueEntries), target_iter);
	}

	//given a value, returns the index at which the value should be inserted into the sortedNumberValueEntries
	//returns true for .second when an exact match is found, false otherwise
	//O(log(n))
	//cycle_length will take into account whether wrapping around is closer
	inline std::pair<size_t, bool> FindClosestValueIndexForValue(double value, double cycle_length = std::numeric_limits<double>::infinity())
	{
		//first check if value exists
		// returns the closest index (lower_bound) if an exact match is not found
		auto [value_index, exact_index_found] = FindExactIndexForValue(value, true);
		if(exact_index_found)
			return std::make_pair(value_index, true);

		//if only have one element (or zero), short circuit code below
		if(sortedNumberValueEntries.size() <= 1)
			return std::make_pair(0, false);

		size_t max_valid_index = sortedNumberValueEntries.size() - 1;
		size_t target_index = std::min(max_valid_index, value_index); //value_index is lower bound index since no exact match

		//if not cyclic or cyclic and not at the edge
		if(cycle_length == std::numeric_limits<double>::infinity()
			|| (target_index > 0 && target_index < max_valid_index) )
		{
			//need to check index again in case not cyclic
			// return index with the closer difference
			if(target_index < max_valid_index
					&& (std::abs(sortedNumberValueEntries[target_index + 1]->value.number - value) < std::abs(sortedNumberValueEntries[target_index]->value.number - value)))
				return std::make_pair(target_index + 1, false);
			else
				return std::make_pair(target_index, false);
		}
		else //cyclic
		{
			double dist_to_max_index = std::abs(sortedNumberValueEntries[max_valid_index]->value.number - value);
			double dist_to_0_index = std::abs(sortedNumberValueEntries[0]->value.number - value);
			size_t other_closest_index;

			if(target_index == 0)
			{
				//wrap around the top
				dist_to_max_index = cycle_length - dist_to_max_index;
				other_closest_index = 1;
			}
			else //target_index == max_valid_index
			{
				//wrap around bottom
				dist_to_0_index = cycle_length - dist_to_0_index;
				other_closest_index = max_valid_index - 1;
			}

			double dist_to_other_closest_index = std::abs(sortedNumberValueEntries[other_closest_index]->value.number - value);
			if(dist_to_0_index <= dist_to_other_closest_index && dist_to_0_index <= dist_to_max_index)
				return std::make_pair(0, false);
			else if(dist_to_other_closest_index <= dist_to_0_index)
				return std::make_pair(other_closest_index, false);
			else
				return std::make_pair(max_valid_index, false);
		}
	}

	//given a feature_id and a range [low, high], inserts all the elements with values of feature feature_id within specified range into out; does not clear out
	//Note about Null/NaNs:
	//if the feature value is Nan/Null, it will NOT be present in the search results, ie "x" != 3 will NOT include elements with x is nan/Null, even though nan/null != 3
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
				//both are NaN
				if(FastIsNaN(low_number) && FastIsNaN(high_number))
				{
					//if looking for NaN
					if(between_values)
					{
						nanIndices.CopyTo(out);
					}
					else //looking for anything but NaN
					{
						numberIndices.CopyTo(out);
						nanIndices.EraseTo(out);
					}

					return;
				}

				//if NaN specified and within range, then we want to include NaN indices
				if(between_values)
					nanIndices.CopyTo(out);

				//modify range to include elements from or up to -/+inf
				if(FastIsNaN(low_number)) //find all NaN values and all values up to max
					low_number = -std::numeric_limits<double>::infinity(); //else include elements from -inf to high as well as NaN elements
				else
					high_number = std::numeric_limits<double>::infinity(); //include elements from low to +inf as well as NaN elements
			}

			//handle equality and nonequality case
			if(low_number == high_number)
			{
				auto [value_index, exact_index_found] = FindExactIndexForValue(low_number);
				if(!exact_index_found)
				{
					//if not found but looking for it, then just return
					if(between_values)
						return;
					else //the value doesn't exist, include everything
					{
						//include nans
						numberIndices.CopyTo(out);
					}
				}

				//if within range, and range has no length, just return indices in that one bucket
				if(between_values)
				{
					size_t index = value_index;
					out.InsertInBatch(sortedNumberValueEntries[index]->indicesWithValue);
				}
				else //if not within, populate with all indices not equal to value
				{
					//include nans
					nanIndices.CopyTo(out);

					for(auto &value_entry : sortedNumberValueEntries)
					{
						if(value_entry->value.number == low_number)
							continue;

						out.InsertInBatch(value_entry->indicesWithValue);
					}
				}

				return;
			}

			size_t start_index = (low_number == -std::numeric_limits<double>::infinity()) ? 0 : FindLowerBoundIndexForValue(low_number);
			size_t end_index = (high_number == std::numeric_limits<double>::infinity()) ? sortedNumberValueEntries.size() : FindUpperBoundIndexForValue(high_number);

			if(between_values)
			{
				//insert everything between the two indices
				for(size_t i = start_index; i < end_index; i++)
					out.InsertInBatch(sortedNumberValueEntries[i]->indicesWithValue);

				//include end_index if value matches
				if(end_index < sortedNumberValueEntries.size() && sortedNumberValueEntries[end_index]->value.number == high_number)
					out.InsertInBatch(sortedNumberValueEntries[end_index]->indicesWithValue);
			}
			else //not between_values
			{
				//insert everything left of range
				for(size_t i = 0; i < start_index; i++)
					out.InsertInBatch(sortedNumberValueEntries[i]->indicesWithValue);

				//insert everything right of range
				for(size_t i = end_index; i < sortedNumberValueEntries.size(); i++)
					out.InsertInBatch(sortedNumberValueEntries[i]->indicesWithValue);
			}

		}
		else if(value_type == ENIVT_STRING_ID)
		{
			if(stringIdValueToIndices.size() == 0)
				return;

			//check every string value to see if between
			for(auto &[id, entry] : stringIdValueToIndices)
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
				for(auto index : *entry)
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
			if(FastIsNaN(value.number))
			{
				//only want nans
				nanIndices.UnionTo(out);
				return;
			}

			auto [value_index, exact_index_found] = FindExactIndexForValue(value.number);
			if(exact_index_found)
				out.InsertInBatch(sortedNumberValueEntries[value_index]->indicesWithValue);
		}
		else if(value_type == ENIVT_STRING_ID)
		{
			auto id_entry = stringIdValueToIndices.find(value.stringID);
			if(id_entry != end(stringIdValueToIndices))
				out.InsertInBatch(*(id_entry->second));
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

			//search left to right for max (bucket 0 is largest) or right to left for min
			int64_t value_index = find_max ? sortedNumberValueEntries.size() - 1 : 0;

			while(value_index < static_cast<int64_t>(sortedNumberValueEntries.size()) && value_index >= 0)
			{
				//add each index to the out indices and optionally output compute results
				for(const auto &index : sortedNumberValueEntries[value_index]->indicesWithValue)
				{
					if(indices_to_consider != nullptr && !indices_to_consider->contains(index))
						continue;

					out.insert(index);

					//return once we have num_to_find entities
					if(out.size() >= num_to_find)
						return;
				}

				value_index += find_max ? -1 : 1; //search right to right for max or left to right for min
			}
		}
		else if(value_type == ENIVT_STRING_ID)
		{
			if(stringIdValueToIndices.size() == 0)
				return;

			//else it's a string, need to do it the brute force way
			std::vector<StringInternPool::StringID> all_sids;
			all_sids.reserve(stringIdValueToIndices.size());

			//get all strings
			for(auto &[id, _] : stringIdValueToIndices)
				all_sids.push_back(id);

			std::sort(begin(all_sids), end(all_sids), StringIDNaturalCompareSort);

			//search left to right for max (bucket 0 is largest) or right to left for min
			int64_t value_index = find_max ? 0 : all_sids.size() - 1;

			while(value_index < static_cast<int64_t>(all_sids.size()) && value_index >= 0)
			{
				const auto &sid_entry = stringIdValueToIndices.find(all_sids[value_index]);
				for(auto index : *(sid_entry->second))
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
		//use heuristic of sqrt number of values compared to num unique values
		// (but computed with a multiply instead of sqrt)
		size_t num_unique_values = sortedNumberValueEntries.size();
		return (num_unique_values * num_unique_values <= numberIndices.size());
	}

	//returns true if switching to number values would be expected to yield better results
	// than number interning given the current data
	inline bool AreNumberValuesPreferredToInterns()
	{
		//use heuristic of sqrt number of values compared to num unique values
		// (but computed with a multiply instead of sqrt)
		//round up to reduce flipping back and forth
		size_t num_unique_values = sortedNumberValueEntries.size();
		return (num_unique_values * num_unique_values > numberIndices.size() - num_unique_values);
	}

	//clears number intern caches and changes state to not perform interning for numbers
	void ConvertNumberInternsToValues()
	{
		if(!numberValuesInterned)
			return;

		internedNumberIndexToNumberValue.clear();
		unusedNumberValueIndices.clear();
		numberValuesInterned = false;
	}

	//initializes and sets up number value interning caches and changes state to perform interning for numbers
	void ConvertNumberValuesToInterns()
	{
		if(numberValuesInterned)
			return;

		internedNumberIndexToNumberValue.resize(sortedNumberValueEntries.size() + 1);
		internedNumberIndexToNumberValue[0] = std::numeric_limits<double>::quiet_NaN();
		for(size_t i = 0; i < sortedNumberValueEntries.size(); i++)
		{
			auto &value_entry = sortedNumberValueEntries[i];
			value_entry->valueInternIndex = i + 1;
			internedNumberIndexToNumberValue[i + 1] = value_entry->value.number;
		}

		numberValuesInterned = true;
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
		for(auto &[s_id, s_entry] : stringIdValueToIndices)
			UpdateLongestString(s_id, *s_entry->begin());
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

	//stores values in sorted order and the entities that have each value
	std::vector<std::unique_ptr<ValueEntry>> sortedNumberValueEntries;

	//maps a string id to a vector of indices that have that string
	CompactHashMap<StringInternPool::StringID, std::unique_ptr<SortedIntegerSet>> stringIdValueToIndices;

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

	//indices of entities with a NaN for this feature
	// the entities will also be included in numberIndices
	EfficientIntegerSet nanIndices;

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

	//if numberValuesInterned is true, then contains an index of each value to its location in sortedNumberValueEntries
	//if a given index isn't used, then it will contain the maximum value for the index
	//the 0th index is reserved for NaN, regardless of whether NaN appears in the data
	std::vector<double> internedNumberIndexToNumberValue;

	//unused / free indices in internedNumberIndexToNumberValue to make adding and removing new values efficient
	//always want to fetch the lowest index to keep the interned NumberIndexToNumberValue small
	FlexiblePriorityQueue<size_t, std::vector<size_t>, std::greater<size_t>> unusedNumberValueIndices;

	//if true, then the indices of the values should be used and internedNumberIndexToValue populated
	bool numberValuesInterned;
};
