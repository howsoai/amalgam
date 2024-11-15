#pragma once

//project headers:
#include "HashMaps.h"
#include "StringInternPool.h"

//system headers:
#include <vector>

//Contains the data from evaluating the goodness or commonality of merging two or more things,
//but without the things merged
class MergeMetricResultsBase
{
public:
	//starts off with an exact match of nothing
	constexpr MergeMetricResultsBase()
		: commonality(0.0), mustMatch(false), exactMatch(true)
	{	}

	constexpr MergeMetricResultsBase(double _similarity, bool must_match = false, bool exact_match = true)
		: commonality(_similarity), mustMatch(must_match), exactMatch(exact_match)
	{	}

	//adds the commonality and keeps track of whether it is an exact match
	constexpr void AccumulateResults(const MergeMetricResultsBase &mmr)
	{
		commonality += mmr.commonality;

		if(exactMatch && !mmr.exactMatch)
			exactMatch = false;
	}

	//syntactic sugar for accumulating merge metric results
	constexpr MergeMetricResultsBase &operator +=(const MergeMetricResultsBase &mmr)
	{
		AccumulateResults(mmr);
		return *this;
	}

	//returns true if this entity has more favorable matching results than mmr
	// if require_nontrivial_match, then it requires at least one node or atomic value to be equal
	constexpr bool IsBetterMatchThan(const MergeMetricResultsBase &mmr)
	{
		if(mustMatch && !mmr.mustMatch)
			return true;

		//if same amount of commonality, prefer exact matches
		if(commonality == mmr.commonality)
		{
			if(!exactMatch && mmr.exactMatch)
				return false;
			if(exactMatch && !mmr.exactMatch)
				return true;
		}

		return commonality > mmr.commonality;
	}

	//syntactic sugar for comparing merge metric results
	constexpr bool operator >(const MergeMetricResultsBase &mmr)
	{
		return IsBetterMatchThan(mmr);
	}

	//returns true if the match is substantial enough that it has at least one equal value of its atoms
	constexpr bool IsNontrivialMatch()
	{
		return exactMatch || mustMatch || commonality >= 1.0;
	}

	//A value between indicating the commonality of the two sets of data being compared
	double commonality;

	//if true, the data must be matched regardless of commonality (e.g., have the same label)
	bool mustMatch;

	//if true, then the data were an exact match
	bool exactMatch;
};

//Contains the data from evaluating the goodness or commonality of merging two or more things,
//with the things merged of type ElementType
template<typename ElementType>
class MergeMetricResults : public MergeMetricResultsBase
{
public:
	//starts off with an exact match of nothing
	//note that if ElementType is a pointer or has a nondefault constructor,
	//C++ initializes to 0 or nullptr
	constexpr MergeMetricResults()
		: MergeMetricResultsBase(), elementA(), elementB()
	{	}

	constexpr MergeMetricResults(double _similarity, ElementType a, ElementType b, bool must_match = false, bool exact_match = true)
		: MergeMetricResultsBase(_similarity, must_match, exact_match), elementA(a), elementB(b)
	{	}

	//the two elements being compared
	ElementType elementA;
	ElementType elementB;
};

//implements a very simple 2d matrix of data using one vector
template<typename ElementType>
class FlatMatrix
{
public:
	void ClearAndResize(size_t size1, size_t size2)
	{
		firstDimensionSize = size1;
		secondDimensionSize = size2;
		flatMatrix.clear();
		flatMatrix.resize(size1 * size2);
	}

	//returns the matrix value at pos1, pos2
	constexpr ElementType &At(size_t pos1, size_t pos2)
	{
		return flatMatrix[firstDimensionSize * pos2 + pos1];
	}
	
	size_t firstDimensionSize;
	size_t secondDimensionSize;
	std::vector<ElementType> flatMatrix;
};

//computes the commonality matrix for computing edit distances between vectors a and b
//the technique is similar to the Wagner–Fischer algorithm, except for commonality rather than distance
//commonality_function is used to return the commonality of two given elements of ElementType,
//and starting_index can be specified if some elements should be skipped before computing commonality
template<typename ElementType, typename MergeResultType, typename CommonalityFunction>
void ComputeSequenceCommonalityMatrix(FlatMatrix<MergeResultType> &sequence_commonality,
	std::vector<ElementType> &a, std::vector<ElementType> &b,
	CommonalityFunction commonality_function, size_t starting_index = 0)
{
	size_t a_size = a.size();
	size_t b_size = b.size();
	sequence_commonality.ClearAndResize(a_size + 1, b_size + 1);

	//start at second location so can compare to previous
	starting_index++;

	//check all possible orders and accumulate, but skip first index
	for(size_t i = starting_index; i <= a_size; i++)
	{
		for(size_t j = starting_index; j <= b_size; j++)
		{
			auto prev_with_new_match = sequence_commonality.At(i - 1, j - 1);
			prev_with_new_match += commonality_function(a[i - 1], b[j - 1]);

			//assign sequence_commonality[i][j] the best of sequence_commonality[i][j - 1], sequence_commonality[i - 1][j]),
			//or sequence_commonality[i - 1][j - 1] + commonality_function(a[i - 1], b[j - 1])
			if(sequence_commonality.At(i, j - 1) > sequence_commonality.At(i - 1, j))
			{
				if(sequence_commonality.At(i, j - 1) > prev_with_new_match)
					sequence_commonality.At(i, j) = sequence_commonality.At(i, j - 1);
				else
					sequence_commonality.At(i, j) = prev_with_new_match;
			}
			else
			{
				if(sequence_commonality.At(i - 1, j) > prev_with_new_match)
					sequence_commonality.At(i, j) = sequence_commonality.At(i - 1, j);
				else
					sequence_commonality.At(i, j) = prev_with_new_match;
			}

		}
	}
}

//Merges elements of type T
// AssocType should be some map where the variables are pointers to T
template<typename T,
	T NullValue = nullptr,
	typename AssocType = CompactHashMap<StringInternPool::StringID, T*>>
class Merger
{
public:
	//Evaluates the commonality between values specified
	virtual MergeMetricResults<T> MergeMetric(T a, T b) = 0;

	//Yields a new value to put into the merged list that is being built
	// if must_merge is true, then it must attempt to create something merging the entities, preferring
	// the value that is more valid if applicable
	virtual T MergeValues(T a, T b, bool must_merge = false) = 0;

	//Returns true if the merge should keep all elements that do not have a corresponding element to merge with
	virtual bool KeepAllNonMergeableValues() = 0;

	//Returns true if the merge should keep some elements that do not have a corresponding element to merge with
	//if KeepAllNonMergeableValues returns true, then this should return true too
	virtual bool KeepSomeNonMergeableValues() = 0;

	//Returns true if the merge should keep one of either particular element, a or b, that does not have a corresponding element
	//may be stochastic
	virtual bool KeepNonMergeableValue() = 0;

	//Returns true if the merge should keep element a instead of element b during for the merge
	//assumes that KeepNonMergeableValue has returned true, because that means either a or b was selected,
	//and this does the selecting
	//may be stochastic
	virtual bool KeepNonMergeableAInsteadOfB() = 0;

	//Returns true if the merge should keep the corresponding element during a merge
	//may be stochastic
	virtual bool KeepNonMergeableA() = 0;
	virtual bool KeepNonMergeableB() = 0;

	//Returns true if the merge should attempt to merge two elements that are not necessarily matches
	//may be stochastic
	virtual bool AreMergeable(T a, T b) = 0;

	//Merges two unordered lists based on the specified MergeMethods
	std::vector<T> MergeUnorderedSets(std::vector<T> &list_a, std::vector<T> &list_b)
	{
		//return empty if nothing passed in
		if(list_a.empty() && list_b.empty())
			return std::vector<T>();

		//copy over lists
		std::vector<T> a1(list_a);
		std::vector<T> a2(list_b);

		std::vector<T> merged;

		std::vector<T> unmatched_a1;
		if(KeepAllNonMergeableValues())
		{
			merged.reserve(std::max(a1.size(), a2.size()));
			unmatched_a1.reserve(a1.size());
		}

		//for every element in a1, find best match (if one exists) in a2
		while(a1.size() > 0)
		{
			//look to see if there's a matching node
			bool best_match_found = false;
			size_t best_match_index = 0;
			MergeMetricResults<T> best_match_value;
			for(size_t match_index = 0; match_index < a2.size(); match_index++)
			{
				auto match_value = MergeMetric(a1[0], a2[match_index]);
				if(match_value.IsNontrivialMatch()
					&& (!best_match_found || match_value > best_match_value))
				{
					best_match_found = true;
					best_match_value = match_value;
					best_match_index = match_index;
				}
			}

			//if found a match, merge the trees, then remove it from the match list and put it in the list
			if(best_match_found)
			{
				T m = MergeValues(a1[0], a2[best_match_index]);
				merged.emplace_back(m);

				a2.erase(begin(a2) + best_match_index);
			}
			else
			{
				//no match, so keep it for later if keeping all
				if(KeepSomeNonMergeableValues())
					unmatched_a1.emplace_back(a1[0]);
			}

			//remove from the first list
			a1.erase(begin(a1));
		}

		//add on remainder if keeping all or some that weren't mergeable
		if(KeepSomeNonMergeableValues())
		{
			for(auto &n : unmatched_a1)
			{
				if(!KeepNonMergeableA())
					continue;

				T m = MergeValues(n, NullValue, true);
				merged.emplace_back(m);
			}

			for(auto &n : a2)
			{
				if(!KeepNonMergeableB())
					continue;

				T m = MergeValues(NullValue, n, true);
				merged.emplace_back(m);
			}
		}

		return merged;
	}

	//Merges two lists that are comprised of unordered sets of pairs based on the specified MergeMethods
	std::vector<T> MergeUnorderedSetsOfPairs(std::vector<T> &list_a, std::vector<T> &list_b)
	{
		//return empty if nothing passed in
		if(list_a.empty() && list_b.empty())
			return std::vector<T>();

		//copy over lists
		std::vector<T> a1(list_a);
		std::vector<T> a2(list_b);

		std::vector<T> merged;

		std::vector<T> unmatched_a1;
		if(KeepAllNonMergeableValues())
		{
			merged.reserve(std::max(a1.size(), a2.size()));
			unmatched_a1.reserve(a1.size());
		}

		//for every element in a1, find best match (if one exists) in a2
		while(a1.size() > 0)
		{
			//look to see if there's a matching node
			bool best_match_found = false;
			size_t best_match_index = 0;
			MergeMetricResults<T> best_match_value;
			for(size_t match_index = 0; match_index < a2.size(); match_index += 2)
			{
				auto match_value = MergeMetric(a1[0], a2[match_index]);
				if(match_value.IsNontrivialMatch() && match_value > best_match_value)
				{
					best_match_found = true;
					best_match_value = match_value;
					best_match_index = match_index;
				}
			}

			//if found a match, merge the trees, then remove it from the match list and put it in the list
			if(best_match_found)
			{
				//merge the keys
				T m_key = MergeValues(a1[0], a2[best_match_index]);

				//get both values if exist and remove key and value from second list (first list will be cleaned up at the end)
				T m_value_1 = 0;
				if(a1.size() > 1)
					m_value_1 = a1[1];

				T m_value_2 = 0;
				if(a2.size() > best_match_index + 1)
				{
					m_value_2 = a2[best_match_index + 1];
					a2.erase(begin(a2) + best_match_index + 1);
				}
				if(a2.size() > best_match_index)
					a2.erase(begin(a2) + best_match_index);

				//merge the values
				T m_value = MergeValues(m_value_1, m_value_2);

				merged.emplace_back(m_key);
				merged.emplace_back(m_value);
			}
			else
			{
				//no match, so keep it for later if keeping all
				if(KeepSomeNonMergeableValues())
				{
					if(a1.size() > 0)
						unmatched_a1.emplace_back(a1[0]);

					if(a1.size() > 1)
						unmatched_a1.emplace_back(a1[1]);
				}
			}

			//remove the key-value pair from the first list
			a1.erase(begin(a1));
			if(a1.size() > 0)
				a1.erase(begin(a1));
		}

		//add on remainder if keeping all or some that weren't mergeable
		if(KeepSomeNonMergeableValues())
		{
			for(size_t i = 0; i < unmatched_a1.size(); i+= 2)
			{
				if(!KeepNonMergeableA())
					continue;

				T m = MergeValues(unmatched_a1[i], NullValue, true);
				merged.emplace_back(m);

				if(i + 1 < unmatched_a1.size())
				{
					T m = MergeValues(unmatched_a1[i + 1], NullValue, true);
					merged.emplace_back(m);
				}
				else
				{
					merged.emplace_back(NullValue);
				}
			}

			for(size_t i = 0; i < a2.size(); i += 2)
			{
				if(!KeepNonMergeableB())
					continue;

				T m = MergeValues(NullValue, a2[i], true);
				merged.emplace_back(m);

				if(i + 1 < a2.size())
				{
					T m = MergeValues(NullValue, a2[i + 1], true);
					merged.emplace_back(m);
				}
				else
				{
					merged.emplace_back(NullValue);
				}
			}
		}

		return merged;
	}

	//Merges two ordered (sequence) lists based on the specified MergeMethods
	std::vector<T> MergeSequences(std::vector<T> &list_a, std::vector<T> &list_b)
	{
		//return empty if nothing passed in
		if(list_a.empty() && list_b.empty())
			return std::vector<T>();

		//build sequence commonality matrix
		FlatMatrix<MergeMetricResults<T>> sequence_commonality;
		ComputeSequenceCommonalityMatrix(sequence_commonality, list_a, list_b,
			[this]
			(T a, T b)
			{
				return MergeMetric(a, b);
			});

		//build a new list, in reverse
		std::vector<T> merged;
		if(KeepAllNonMergeableValues())
			merged.reserve(std::max(list_a.size(), list_b.size()));

		//start in the maximal position
		auto a_index = list_a.size();
		auto b_index = list_b.size();

		//iterate over everything, finding which was the maximal path
		while(a_index > 0 && b_index > 0)
		{
			//if it's not a good match or worse than matching with the next one down in b, then take one from b
			if(!sequence_commonality.At(a_index, b_index).IsNontrivialMatch()
					|| !sequence_commonality.At(a_index, b_index).IsBetterMatchThan(sequence_commonality.At(a_index, b_index - 1)))
			{
				b_index--;
				if(KeepNonMergeableB())
				{
					T m = MergeValues(NullValue, list_b[b_index], true);
					merged.emplace_back(m);
				}
				continue;
			}

			//if it's not better to merge with the next one down in a, then take a off
			if(!sequence_commonality.At(a_index, b_index).IsBetterMatchThan(sequence_commonality.At(a_index - 1, b_index)))
			{
				a_index--;
				if(KeepNonMergeableB())
				{
					T m = MergeValues(list_a[a_index], NullValue, true);
					merged.emplace_back(m);
				}
				continue;
			}

			//must be that it's kept in both; if mergeable, merge, if not, take both if applicable
			a_index--;
			b_index--;
			if(AreMergeable(list_a[a_index], list_b[b_index]))
			{
				T m = MergeValues(list_a[a_index], list_b[b_index]);
				merged.emplace_back(m);
			}
			else
			{
				if(KeepNonMergeableA())
				{
					T m = MergeValues(list_a[a_index], NullValue, true);
					merged.emplace_back(m);
				}
				if(KeepNonMergeableB())
				{
					T m = MergeValues(NullValue, list_b[b_index], true);
					merged.emplace_back(m);
				}
			}
		}

		//put any remaining elements of either array on if keeping all or some that weren't mergeable
		if(KeepSomeNonMergeableValues())
		{
			while(a_index > 0)
			{
				a_index--;

				if(!KeepNonMergeableA())
					continue;

				T m = MergeValues(list_a[a_index], NullValue, true);
				merged.emplace_back(m);
			}

			while(b_index > 0)
			{
				b_index--;
				
				if(!KeepNonMergeableB())
					continue;

				T m = MergeValues(NullValue, list_b[b_index], true);
				merged.emplace_back(m);
			}
		}

		//put back in the right order
		std::reverse(begin(merged), end(merged));
		return merged;
	}

	//Merges two position-based ordered lists based on the specified MergeMethods
	std::vector<T> MergePositions(std::vector<T> &list_a, std::vector<T> &list_b)
	{
		//return empty if nothing passed in
		if(list_a.empty() && list_b.empty())
			return std::vector<T>();

		//accumulate the array
		std::vector<T> merged;
		if(KeepAllNonMergeableValues())
			merged.reserve(std::max(list_a.size(), list_b.size()));

		//use size of smallest list and merge all positions that are common
		size_t smallest_list_size = std::min(list_a.size(), list_b.size());
		for(size_t i = 0; i < smallest_list_size; i++)
		{
			T m = MergeValues(list_a[i], list_b[i]);
			merged.emplace_back(m);
		}

		if(KeepSomeNonMergeableValues())
		{
			//merge anything left in a
			for(auto i = smallest_list_size; i < list_a.size(); i++)
			{
				if(KeepNonMergeableA())
				{
					T m = MergeValues(list_a[i], NullValue, true);
					merged.emplace_back(m);
				}
				else
				{
					merged.emplace_back(NullValue);
				}
			}

			//merge anything left in b
			for(auto i = smallest_list_size; i < list_b.size(); i++)
			{
				if(KeepNonMergeableB())
				{
					T m = MergeValues(NullValue, list_b[i], true);
					merged.emplace_back(m);
				}
				else
				{
					merged.emplace_back(NullValue);
				}
			}
		}

		return merged;
	}

	//Merges two mappings based on the specified MergeMethods
	AssocType MergeMaps(AssocType &map_a, AssocType &map_b)
	{
		AssocType merged;

		if(map_a.empty() && map_b.empty())
			return merged;

		//if not potentially keeping any that are common,
		//can just do a quick pass finding those common in both
		if(!KeepSomeNonMergeableValues())
		{
			//see if both trees have mapped child nodes
			if(map_a.size() > 0 && map_b.size() > 0)
			{
				//use keys from first node
				for(auto &[n_key, n_value] : map_a)
				{
					//skip unless both trees have the key
					auto found_b = map_b.find(n_key);
					if(found_b == end(map_b))
						continue;

					//merge what's under the key
					auto m_value = MergeValues(n_value, found_b->second);
					merged[n_key] = m_value;
				}
			}

			return merged;
		}
		//else need to track some that might be in either or both
		
		//fast iteration if one element doesn't have any nodes
		if(map_a.size() > 0 && map_b.empty())
		{
			//merge all values with null
			for(auto &[n_key, n_value] : map_a)
			{
				if(!KeepNonMergeableA())
					continue;

				auto m_value = MergeValues(n_value, NullValue, true);
				merged[n_key] = m_value;
			}
		}
		else if(map_a.empty() && map_b.size() > 0)
		{
			//merge all values with null
			for(auto &[n_key, n_value] : map_b)
			{
				if(!KeepNonMergeableB())
					continue;

				auto m_value = MergeValues(NullValue, n_value, true);
				merged[n_key] = m_value;
			}
		}
		else if(map_a.size() > 0 && map_b.size() > 0)
		{
			//include all keys that are in both nodes
			for(auto &[n_key, _] : map_a)
			{
				if(map_b.find(n_key) != end(map_b))
					merged.emplace(n_key, NullValue);
			}
			size_t num_common_indices = merged.size();

			//keep those from a and b as appropriate
			//but can skip if the merged is the same size as the map
			if(map_a.size() != num_common_indices)
			{
				for(auto &[n_key, _] : map_a)
				{
					if(KeepNonMergeableA())
						merged.emplace(n_key, NullValue);
				}
			}

			if(map_b.size() != num_common_indices)
			{
				for(auto &[n_key, _] : map_b)
				{
					if(KeepNonMergeableB())
						merged.emplace(n_key, NullValue);
				}
			}

			for(auto &[m_key, m_value] : merged)
			{
				//merge what's under the key
				auto found_a = map_a.find(m_key);
				auto found_b = map_b.find(m_key);

				//if found both, merge both
				if(found_a != end(map_a) && found_b != end(map_b))
					m_value = MergeValues(found_a->second, found_b->second);
				else if(found_b == end(map_b))
					m_value = MergeValues(found_a->second, NullValue, true);
				else //a not found
					m_value = MergeValues(NullValue, found_b->second, true);
			}
		}

		return merged;
	}
};
