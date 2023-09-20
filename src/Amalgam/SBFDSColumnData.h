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
	//column needs to be named when it is created
	inline SBFDSColumnData(StringInternPool::StringID sid)
		: stringId(sid)
	{	
		indexWithLongestString = 0;
		longestStringLength = 0;
		indexWithLargestCode = 0;
		largestCodeSize = 0;
	}

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

		sortedNumberValueIndexPairs.reserve(num_uniques);
		numberIndices.ReserveNumIntegers(index_values.back().reference + 1);

		for(auto &index_value : index_values)
		{
			//if don't have the right bucket, then need to create one
			if(sortedNumberValueIndexPairs.size() == 0 || sortedNumberValueIndexPairs.back().first != index_value.distance)
				sortedNumberValueIndexPairs.emplace_back(index_value.distance, std::make_unique<SortedIntegerSet>());

			sortedNumberValueIndexPairs.back().second->InsertNewLargestInteger(index_value.reference);
			numberIndices.insert(index_value.reference);
		}
	}

	//returns the value type of the given index given the value
	__forceinline EvaluableNodeImmediateValueType GetIndexValueType(size_t index)
	{
		if(numberIndices.contains(index))
			return ENIVT_NUMBER;
		if(stringIdIndices.contains(index))
			return ENIVT_STRING_ID;
		if(nullIndices.contains(index))
			return ENIVT_NULL;
		if(invalidIndices.contains(index))
			return ENIVT_NOT_EXIST;
		return ENIVT_CODE;
	}

	//moves index from being associated with key old_value to key new_value
	void ChangeIndexValue(EvaluableNodeImmediateValue old_value, EvaluableNodeImmediateValueType new_value_type, EvaluableNodeImmediateValue new_value, size_t index)
	{
		//if new one is invalid, can quickly delete or return
		if(new_value_type == ENIVT_NOT_EXIST)
		{
			if(!invalidIndices.contains(index))
			{
				DeleteIndexValue(old_value, index);
				invalidIndices.insert(index);
			}
			return;
		}

		//delete index at old value
		DeleteIndexValue(old_value, index);

		//add index at new value bucket 
		InsertIndexValue(new_value_type, new_value, index);
	}

	//deletes everything involving the value at the index
	void DeleteIndexValue(EvaluableNodeImmediateValue value, size_t index)
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
				//look up value
				auto [value_index, exact_index_found] = FindExactIndexForValue(value.number);
				if(!exact_index_found)
					return;

				//if the bucket has only one entry, we must delete the entire bucket
				if(sortedNumberValueIndexPairs[value_index].second->size() == 1)
				{
					sortedNumberValueIndexPairs.erase(sortedNumberValueIndexPairs.begin() + value_index);
				}
				else //else we can just remove the id from the bucket
				{
					sortedNumberValueIndexPairs[value_index].second->erase(index);
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
			{
				longestStringLength = 0;
				//initialize to 0 in case there are no entities with strings
				indexWithLongestString = 0;
				for(auto &[s_id, s_entry] : stringIdValueToIndices)
					UpdateLongestString(s_id, *s_entry->begin());
			}

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
		{
			largestCodeSize = 0;
			//initialize to 0 in case there are no entities with code
			indexWithLargestCode = 0;
			for(auto &[size, entry] : valueCodeSizeToIndices)
				UpdateLargestCode(size, *entry->begin());
		}
	}

	//inserts the value at id
	void InsertIndexValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue &value, size_t index)
	{
		if(value_type == ENIVT_NOT_EXIST)
		{
			invalidIndices.insert(index);
			return;
		}

		if(value_type == ENIVT_NULL)
		{
			nullIndices.insert(index);
			return;
		}

		if(value_type == ENIVT_NUMBER)
		{
			numberIndices.insert(index);

			if(FastIsNaN(value.number))
			{
				nanIndices.insert(index);
				return;
			}
			
			//if the value already exists, then put the index in the list
			auto [value_index, exact_index_found] = FindExactIndexForValue(value.number);
			if(exact_index_found)
			{
				sortedNumberValueIndexPairs[value_index].second->insert(index);
				return;
			}

			//insert new value in correct position
			size_t new_value_index = FindUpperBoundIndexForValue(value.number);
			auto inserted = sortedNumberValueIndexPairs.emplace(sortedNumberValueIndexPairs.begin() + new_value_index, value.number, std::make_unique<SortedIntegerSet>());
			inserted->second->insert(index);

			return;
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
			return;
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
	}

	//returns the maximum difference between value and any other value for this column
	//if empty, will return infinity
	inline double GetMaxDifferenceTermFromValue(GeneralizedDistance::FeatureParams &feature_params, EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue &value)
	{
		switch(feature_params.featureType)
		{
		case FDT_NOMINAL:
			return 1.0;

		case FDT_CONTINUOUS_NUMERIC:
		case FDT_CONTINUOUS_UNIVERSALLY_NUMERIC:
			if(sortedNumberValueIndexPairs.size() <= 1)
				return 0.0;

			return sortedNumberValueIndexPairs.back().first - sortedNumberValueIndexPairs[0].first;

		case FDT_CONTINUOUS_NUMERIC_CYCLIC:
			//maximum is the other side of the cycle
			return feature_params.typeAttributes.maxCyclicDifference / 2;

		case FDT_CONTINUOUS_STRING:
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

		case FDT_CONTINUOUS_CODE:
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
		auto target_iter = std::lower_bound(begin(sortedNumberValueIndexPairs), end(sortedNumberValueIndexPairs), value,
			[](const auto& value_index_pair, double value)
			{
				return value_index_pair.first < value;
			});

		if ((target_iter == end(sortedNumberValueIndexPairs)) || (target_iter->first != value)) // not exact match
		{
			return std::make_pair(return_index_lower_bound ? std::distance(begin(sortedNumberValueIndexPairs), target_iter) : -1 , false);
		}

		return std::make_pair(std::distance(begin(sortedNumberValueIndexPairs), target_iter), true); // exact match
	}

	//returns the index of the lower bound of value
	inline size_t FindLowerBoundIndexForValue(double value)
	{
		auto target_iter = std::lower_bound(begin(sortedNumberValueIndexPairs), end(sortedNumberValueIndexPairs), value,
			[](const auto &value_index_pair, double value)
			{
				return value_index_pair.first < value;
			});
		return std::distance(begin(sortedNumberValueIndexPairs), target_iter);
	}

	//returns the index of the upper bound of value
	inline size_t FindUpperBoundIndexForValue(double value)
	{
		auto target_iter = std::upper_bound(begin(sortedNumberValueIndexPairs), end(sortedNumberValueIndexPairs), value,
			[](double value, const auto &value_index_pair)
			{
				return value < value_index_pair.first;
			});
		return std::distance(begin(sortedNumberValueIndexPairs), target_iter);
	}

	//given a value, returns the index at which the value should be inserted into the sortedNumberValueIndexPairs
	//returns true for .second when an exact match is found, false otherwise
	//O(log(n))
	//cycle_length will take into account whether wrapping around is closer
	inline std::pair<size_t, bool> FindClosestValueIndexForValue(double value, double cycle_length = std::numeric_limits<double>::infinity())
	{
		//first check if value exists
		// returns the closest index (lower_bound) if an exact match is not found
		auto [value_index, exact_index_found] = FindExactIndexForValue(value, true);
		if(exact_index_found)
		{
			return std::make_pair(value_index, true);
		}

		//if only have one element (or zero), short circuit code below
		if(sortedNumberValueIndexPairs.size() <= 1)
			return std::make_pair(0, false);

		size_t max_valid_index = sortedNumberValueIndexPairs.size() - 1;
		size_t target_index = std::min(max_valid_index, value_index); //value_index is lower bound index since no exact match

		//if not cyclic or cyclic and not at the edge
		if(cycle_length == std::numeric_limits<double>::infinity()
			|| (target_index > 0 && target_index < max_valid_index) )
		{
			//need to check index again in case not cyclic
			// return index with the closer difference
			if(target_index < max_valid_index
					&& (std::abs(sortedNumberValueIndexPairs[target_index + 1].first - value) < std::abs(sortedNumberValueIndexPairs[target_index].first - value)))
				return std::make_pair(target_index + 1, false);
			else
				return std::make_pair(target_index, false);
		}
		else //cyclic
		{
			double dist_to_max_index = std::abs(sortedNumberValueIndexPairs[max_valid_index].first - value);
			double dist_to_0_index = std::abs(sortedNumberValueIndexPairs[0].first - value);
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

			double dist_to_other_closest_index = std::abs(sortedNumberValueIndexPairs[other_closest_index].first - value);
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
			if(sortedNumberValueIndexPairs.size() == 0)
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
					out.InsertInBatch(*sortedNumberValueIndexPairs[index].second);
				}
				else //if not within, populate with all indices not equal to value
				{
					//include nans
					nanIndices.CopyTo(out);

					for(auto &[bucket_val, bucket] : sortedNumberValueIndexPairs)
					{
						if(bucket_val == low_number)
							continue;

						out.InsertInBatch(*bucket);
					}
				}

				return;
			}

			size_t start_index = (low_number == -std::numeric_limits<double>::infinity()) ? 0 : FindLowerBoundIndexForValue(low_number);
			size_t end_index = (high_number == std::numeric_limits<double>::infinity()) ? sortedNumberValueIndexPairs.size() : FindUpperBoundIndexForValue(high_number);

			if(between_values)
			{
				//insert everything between the two indices
				for(size_t i = start_index; i < end_index; i++)
					out.InsertInBatch(*sortedNumberValueIndexPairs[i].second);

				//include end_index if value matches
				if(end_index < sortedNumberValueIndexPairs.size() && sortedNumberValueIndexPairs[end_index].first == high_number)
					out.InsertInBatch(*sortedNumberValueIndexPairs[end_index].second);
			}
			else //not between_values
			{
				//insert everything left of range
				for(size_t i = 0; i < start_index; i++)
					out.InsertInBatch(*sortedNumberValueIndexPairs[i].second);

				//insert everything right of range
				for(size_t i = end_index; i < sortedNumberValueIndexPairs.size(); i++)
					out.InsertInBatch(*sortedNumberValueIndexPairs[i].second);
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
				out.InsertInBatch(*sortedNumberValueIndexPairs[value_index].second);
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
			if(sortedNumberValueIndexPairs.size() == 0)
				return;

			//search left to right for max (bucket 0 is largest) or right to left for min
			int64_t value_index = find_max ? sortedNumberValueIndexPairs.size() - 1 : 0;

			while(value_index < static_cast<int64_t>(sortedNumberValueIndexPairs.size()) && value_index >= 0)
			{
				//add each index to the out indices and optionally output compute results
				for(const auto &index : *sortedNumberValueIndexPairs[value_index].second)
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

	//updates largestCodeSize and indexWithLargestCode based on parameters
	inline void UpdateLargestCode(size_t code_size, size_t index)
	{
		if(code_size > largestCodeSize)
		{
			largestCodeSize = code_size;
			indexWithLargestCode = index;
		}
	}

public:

	//name of the column
	StringInternPool::StringID stringId;

	//stores values in sorted order and the entities that have each value
	std::vector< std::pair<double, std::unique_ptr<SortedIntegerSet>> > sortedNumberValueIndexPairs;

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
};
