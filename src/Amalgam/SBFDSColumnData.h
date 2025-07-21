#pragma once

//project headers:
#include "DistanceReferencePair.h"
#include "EvaluableNode.h"
#include "GeneralizedDistance.h"
#include "HashMaps.h"
#include "IntegerSet.h"

//system headers:
#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

//if SBFDS_VERIFICATION is defined, then it will frequently verify integrity at cost of performance
//if FORCE_SBFDS_VALUE_INTERNING is defined, then it will force value interning to always be on
//if DISABLE_SBFDS_VALUE_INTERNING is defined, then it will disable all value interning
//if FORCE_SBFDS_VALUE_INTERNING and DISABLE_SBFDS_VALUE_INTERNING, FORCE_SBFDS_VALUE_INTERNING takes precedence

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

		ValueEntry(const ValueEntry &ve)
			: value(ve.value), indicesWithValue(ve.indicesWithValue), valueInternIndex(ve.valueInternIndex)
		{	}

		EvaluableNodeImmediateValue value;
		SortedIntegerSet indicesWithValue;
		size_t valueInternIndex;
	};

	//column needs to be named when it is created
	inline SBFDSColumnData(StringInternPool::StringID sid)
		: stringId(sid), indexWithLongestString(0), longestStringLength(0),
		indexWithLargestCode(0), largestCodeSize(0)
	{	}

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

	//returns the resolved type and value at the index
	inline std::pair<EvaluableNodeImmediateValueType, EvaluableNodeImmediateValue> GetResolvedIndexValueTypeAndValue(size_t index)
	{
		auto value_type = GetIndexValueType(index);
		auto value = ResolveValue(value_type, valueEntries[index]);
		value_type = ResolveValueType(value_type);
		return std::make_pair(value_type, value);
	}

	//returns the resolved value at the index
	inline EvaluableNodeImmediateValue GetResolvedIndexValue(size_t index)
	{
		auto value_type = GetIndexValueType(index);
		auto value = ResolveValue(value_type, valueEntries[index]);
		return value;
	}

	//returns the value type, performing any resolution for intern lookups
	static __forceinline EvaluableNodeImmediateValueType ResolveValueType(EvaluableNodeImmediateValueType value_type)
	{
		if(value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
			return ENIVT_NUMBER;
		if(value_type == ENIVT_STRING_ID_INDIRECTION_INDEX)
			return ENIVT_STRING_ID;
		return value_type;
	}

	//returns the value performing any intern lookup if necessary
	__forceinline EvaluableNodeImmediateValue ResolveValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue value)
	{
		if(value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
			return EvaluableNodeImmediateValue(internedNumberValues.internedIndexToValue[value.indirectionIndex]);
		if(value_type == ENIVT_STRING_ID_INDIRECTION_INDEX)
			return EvaluableNodeImmediateValue(internedStringIdValues.internedIndexToValue[value.indirectionIndex]);
		return value;
	}

	//inserts the value at id
	//returns the value that should be used to reference the value, which may be an index
	//depending on the state of the column data
	void InsertIndexValue(EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &value, size_t index);

	//like InsertIndexValue, but used only for building the column data from an empty column
	//this function must be called on each index in ascending order; for example, index 2 must be called after index 1
	void InsertNextIndexValueExceptNumbers(EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &value, size_t index);

	//inserts a particular value based on the value_index
	//templated to make it efficiently work regardless of the container
	template<typename StringIdValueEntryIterator>
	void InsertFirstIndexIntoStringIdValueEntry(size_t index, StringIdValueEntryIterator &value_iter)
	{
		auto &value_entry = *(value_iter->second.get());

		value_entry.indicesWithValue.insert(index);
		internedStringIdValues.InsertValueEntry(value_entry, stringIdValueEntries.size());
	}

	//moves index from being associated with key old_value to key new_value
	void ChangeIndexValue(EvaluableNodeImmediateValueType new_value_type,
		EvaluableNodeImmediateValue new_value, size_t index);

	//deletes everything involving the value at the index
	//if remove_last_entity is true, then it will remove the last entry (assumes index is the last entry)
	void DeleteIndexValue(EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue value,
		size_t index, bool remove_last_entity);

	//changes column to/from interning as would yield best performance
	void Optimize();

	//returns the number of unique values in the column
	//if value_type is ENIVT_NULL, then it will include all types, otherwise it will only consider
	//the unique values for the type requested
	inline size_t GetNumUniqueValues(EvaluableNodeImmediateValueType value_type = ENIVT_NULL)
	{
		if(value_type == ENIVT_NUMBER)
			return sortedNumberValueEntries.size();

		if(value_type == ENIVT_STRING_ID)
			return stringIdValueEntries.size();

		//if there are any null values, count that as one
		size_t null_count = 0;
		if(nullIndices.size() > 0)
			null_count = 1;

		//add up unique number and string values,
		// and use a heuristic for judging how many unique code values there are
		return null_count + sortedNumberValueEntries.size() + stringIdIndices.size()
			+ (valueCodeSizeToIndices.size() + codeIndices.size()) / 2;
	}

	//returns the number of valid values (exist and not null) in the column
	inline size_t GetNumValidDataElements()
	{
		return numberIndices.size() + stringIdIndices.size() + codeIndices.size();
	}

	//returns the maximum difference between value and any other value for this column
	//if empty, will return infinity
	inline double GetMaxDifference(GeneralizedDistanceEvaluator::FeatureAttributes &feature_attribs)
	{
		switch(feature_attribs.featureType)
		{
		case GeneralizedDistanceEvaluator::FDT_NOMINAL_BOOL:
		case GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMBER:
		case GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING:
		case GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE:
			return 1.0 - 1.0 / (std::max<size_t>(1, GetNumValidDataElements()) + 0.5);

		case GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER:
			if(sortedNumberValueEntries.size() <= 1)
				return 0.0;

			return sortedNumberValueEntries.rbegin()->second.value.number
				- sortedNumberValueEntries.begin()->second.value.number;

		case GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER_CYCLIC:
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

				//make a copy of the entry in case the entry is invalidated elsewhere
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
		EvaluableNodeImmediateValue &low, EvaluableNodeImmediateValue &high, BitArrayIntegerSet &out, bool between_values = true);

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
		BitArrayIntegerSet *indices_to_consider, BitArrayIntegerSet &out);

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

		//use heuristic of sqrt number of values compared to num unique values
		// (but computed with a multiply instead of sqrt)
		//round up to reduce flipping back and forth
		size_t num_unique_values = sortedNumberValueEntries.size();

		return (num_unique_values * num_unique_values > num_indices - num_unique_values);
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

	//used for debugging to make sure all entities are valid
	inline void VerifyAllEntities(size_t max_num_entities = std::numeric_limits<size_t>::max())
	{
		for(auto &value_entry : sortedNumberValueEntries)
		{
			//ensure all interned values are valid
			if(internedNumberValues.valueInterningEnabled)
			{
				auto &interns = internedNumberValues;
				assert(value_entry.second.valueInternIndex < interns.internedIndexToValue.size());
				assert(!FastIsNaN(interns.internedIndexToValue[value_entry.second.valueInternIndex]));
			}

			//ensure all entity ids are not out of range
			for(auto entity_index : value_entry.second.indicesWithValue)
				assert(entity_index < max_num_entities);
		}

		//ensure all numbers are valid
		for(auto entity_index : numberIndices)
		{
			auto &feature_value = valueEntries[entity_index];
			auto feature_type = GetIndexValueType(entity_index);
			assert(feature_type == ENIVT_NUMBER || feature_type == ENIVT_NUMBER_INDIRECTION_INDEX);
			if(feature_type == ENIVT_NUMBER_INDIRECTION_INDEX && feature_value.indirectionIndex != 0)
			{
				auto feature_value_resolved = ResolveValue(feature_type, feature_value);
				assert(!FastIsNaN(feature_value_resolved.number));
			}
		}

		for(auto &[sid, value_entry] : stringIdValueEntries)
		{
			//ensure all interned values are valid
			if(internedStringIdValues.valueInterningEnabled)
			{
				auto &interns = internedStringIdValues;
				assert(value_entry->valueInternIndex < interns.internedIndexToValue.size());
			}
		}

		//ensure all string ids are valid
		for(auto entity_index : stringIdIndices)
		{
			auto &feature_value = valueEntries[entity_index];
			auto feature_type = GetIndexValueType(entity_index);
			assert(feature_type == ENIVT_STRING_ID || feature_type == ENIVT_STRING_ID_INDIRECTION_INDEX);
			if(feature_type == ENIVT_STRING_ID_INDIRECTION_INDEX && feature_value.indirectionIndex != 0)
			{
				auto feature_value_resolved = ResolveValue(feature_type, feature_value);
				assert(feature_value_resolved.stringID != string_intern_pool.NOT_A_STRING_ID);
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

	//TODO 22139: implement this
	//indices of entities with a bool value for this feature
	EfficientIntegerSet boolIndices;

	//for all indices that are boolean, contains the truth value of each
	EfficientIntegerSet boolIndicesValues;

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
