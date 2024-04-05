#pragma once

//project headers:
#include "FastMath.h"
#include "PlatformSpecific.h"
#include "RandomStream.h"

//system headers:
#include <algorithm>
#include <climits>
#include <cstdint>
#include <vector>

//container for holding sparse integers that maximizes efficiency of interoperating
// with BitArrayIntegerSet
class SortedIntegerSet
{
public:

	SortedIntegerSet()
	{	}

	template <typename Collection>
	SortedIntegerSet(const Collection &other)
	{
		integers.reserve(other.size());
		for(const size_t element : other)
			insert(element);
	}

	//defined to keep compatibility with stl containers
	using value_type = size_t;

	using Iterator = std::vector<size_t>::iterator;

	//assignment operator, deep copies bit buffer
	inline void operator =(const SortedIntegerSet &other)
	{
		integers = other.integers;
	}

	//std begin (must be lowercase)
	__forceinline auto begin()
	{
		return std::begin(integers);
	}

	//std end (must be lowercase)
	//returns bit 0 of the lowest bucket that is not populated
	__forceinline auto end()
	{
		return std::end(integers);
	}

	//returns the nth id in the set by sorted order
	size_t GetNthElement(size_t n)
	{
		//if asking for something too big, just return last element (size)
		if(n > integers.size())
			return GetEndInteger();

		return integers[n];
	}

	//returns a random integer
	size_t GetRandomElement(RandomStream &random_stream)
	{
		size_t i = random_stream.RandSize(integers.size());
		return integers[i];
	}

	//clears the BitArrayIntegerSet as if it is new
	__forceinline void clear()
	{
		integers.clear();
	}

	//returns the number of elements that exist in the hash set
	__forceinline size_t size()
	{
		return integers.size();
	}

	//returns one past the maximum index in the container, 0 if empty
	__forceinline size_t GetEndInteger()
	{
		size_t max_position = integers.size();
		if(max_position > 0)
			return integers[max_position - 1] + 1;

		return 0;
	}

	//reserves the number of elements to be inserted
	__forceinline void ReserveNumIntegers(size_t num_elements)
	{
		integers.reserve(num_elements);
	}

	//returns true if the id exists in the set
	__forceinline bool contains(size_t id)
	{
		auto location = std::lower_bound(std::begin(integers), std::end(integers), id);
		return (location != std::end(integers) && id == *location);
	}

	//returns true if the id exists in the set
	__forceinline bool operator [](size_t id)
	{
		return contains(id);
	}

	//inserts id into hash set, does nothing if id already exists
	void insert(size_t id)
	{
		auto location = std::lower_bound(std::begin(integers), std::end(integers), id);

		//insert as long as it doesn't already exist
		if(location == std::end(integers) || id != *location)
			integers.emplace(location, id);
	}

	//inserts all elements in collection
	//assumes that the elements are not in this set, but does not assume elements are sorted
	template <typename Collection>
	__forceinline void insert(Collection &other)
	{
		for(const size_t element : other)
			insert(element);
	}

	//inserts all elements in collection
	//assumes that the elements are not in this set, but does not assume elements are sorted
	//functionally identical to insert
	template <typename Collection>
	__forceinline void InsertInBatch(Collection &other)
	{
		for(const size_t element : other)
			insert(element);
	}

	//inserts all elements in collection
	//assumes that the elements are not in this set and that the elements are sorted
	template <typename Collection>
	__forceinline void InsertNewSortedIntegers(Collection &other)
	{
		integers.reserve(other.size());
		for(const size_t element : other)
			integers.emplace_back(element);
	}

	//insert an id is larger than GetEndInteger()
	//assumes that the element is not in this set
	__forceinline void InsertNewLargestInteger(size_t id)
	{
		integers.push_back(id);
	}

	//removes id from hash set, does nothing if id does not exist in the hash
	void erase(size_t id)
	{
		auto location = std::lower_bound(std::begin(integers), std::end(integers), id);
		if(location == std::end(integers))
			return;

		if(id == *location)
			integers.erase(location);
	}

	//removes all elements contained by other
	template<typename Container>
	void erase(Container &other)
	{
		auto other_iter = std::begin(other);
		auto other_end = std::end(other);

		//copies data to the destination instead of erasing
		// to reduce computational complexity
		size_t dest_index = 0;
		size_t cur_index = 0;
		while(cur_index != integers.size())
		{
			//if other is exhausted, then nothing left to erase, so move everything downward
			if(other_iter == other_end)
			{
				integers.erase(std::begin(integers) + dest_index, std::begin(integers) + cur_index);
				return;
			}

			//if next integer in other is greater, copy and continue
			if(integers[cur_index] < *other_iter)
			{
				if(dest_index != cur_index)
					integers[dest_index] = integers[cur_index];

				dest_index++;
				cur_index++;
			}
			else //other is less than or equal
			{
				//if next integer is the same, erase
				if(integers[cur_index] == *other_iter)
					cur_index++;

				//pre-increment in case it matters for performance for iterator type
				++other_iter;
			}
		}
	}

	//removes all elements contained by other, intended for calling in a batch
	//for this container, it is the same as erase
	template<typename Container>
	inline void EraseInBatch(Container &other)
	{
		erase(other);
	}

	//removes the id and returns true if it was in the id before removal
	bool EraseAndRetrieve(size_t id)
	{
		auto location = std::lower_bound(std::begin(integers), std::end(integers), id);
		if(location == std::end(integers))
			return false;

		if(id == *location)
		{
			integers.erase(location);
			return true;
		}

		return false;
	}

	//does not need to do anything, just conforming to the interface
	constexpr void UpdateNumElements()
	{	}

	//sets this to the set that contains all elements of itself or other
	template<typename Container>
	void Union(Container &other)
	{
		auto other_iter = std::begin(other);
		auto other_end = std::end(other);

		size_t cur_index = 0;
		while(cur_index != integers.size())
		{
			//if other is exhausted, then done
			if(other_iter == other_end)
				return;

			//if next integer in other is greater, just continue
			if(integers[cur_index] < *other_iter)
			{
				cur_index++;
			}
			else //other is less than or equal
			{
				//only insert the next integer if it is different
				if(integers[cur_index] != *other_iter)
					integers.insert(std::begin(integers) + cur_index, *other_iter);

				++other_iter;
			}
		}

		integers.insert(std::end(integers), other_iter, other_end);
	}

	//sets this to the set that contains only elements that it and another jointly contain
	template<typename Container>
	void Intersect(Container &other)
	{
		auto other_iter = std::begin(other);
		auto other_end = std::end(other);

		//copies data to the destination instead of erasing
		// to reduce computational complexity
		size_t dest_index = 0;
		size_t cur_index = 0;
		while(cur_index != integers.size() && other_iter != other_end)
		{
			//if next integer is greater, skip
			if(integers[cur_index] < *other_iter)
			{
				cur_index++;
			}
			else //other is less than or equal
			{
				//if next integer is the same, copy and keep
				if(integers[cur_index] == *other_iter)
				{
					if(dest_index != cur_index)
						integers[dest_index] = integers[cur_index];

					dest_index++;
					cur_index++;
				}

				++other_iter;
			}
		}

		//cut off anything at the end
		integers.resize(dest_index);
	}

	//returns the first offset of the vector returned by GetIntegerVector that is
	// greater than id; will return the size of the integers vector if id is larger
	// than all in it
	__forceinline size_t GetFirstIntegerVectorLocationGreaterThan(size_t id)
	{
		auto location = std::upper_bound(std::begin(integers), std::end(integers), id);
		return std::distance(std::begin(integers), location);
	}

	constexpr std::vector<size_t> &GetIntegerVector()
	{
		return integers;
	}

protected:
	std::vector<size_t> integers;
};

//uses bit-compression to hash integral key values into a set
// note that some of the methods follow the STL convention so that
// this library can be closer to a drop-in replacement for STL sets
class BitArrayIntegerSet
{
public:
	//defined to keep compatibility with stl containers
	using value_type = size_t;

	BitArrayIntegerSet()
	{
		numElements = 0;
		curMaxNumIndices = 0;
	}

	struct Iterator
	{
		constexpr Iterator()
			: bucket(0), bit(0), hash(nullptr)
		{	}

		constexpr Iterator(BitArrayIntegerSet *_hash, size_t _bucket, size_t _bit)
			: bucket(_bucket), bit(_bit), hash(_hash)
		{	}

		constexpr Iterator operator =(const Iterator &other)
		{
			bucket = other.bucket;
			bit = other.bit;
			hash = other.hash;
			return *this;
		}

		constexpr bool operator ==(const Iterator &other)
		{
			return (bucket == other.bucket && bit == other.bit);
		}

		constexpr bool operator !=(const Iterator &other)
		{
			return (bucket != other.bucket || bit != other.bit);
		}

		__forceinline Iterator &operator ++()
		{
			hash->FindNext(bucket, bit);
			return *this;
		}

		//dereference operator
		constexpr size_t operator *()
		{
			return hash->GetIndexFromBucketAndBit(bucket, bit);
		}

		//bucket index
		size_t bucket;

		//bit index
		size_t bit;

		//associated set
		BitArrayIntegerSet *hash;
	};

	//assignment operator, deep copies bit buffer
	inline void operator =(const BitArrayIntegerSet &other)
	{
		numElements = other.numElements;
		curMaxNumIndices = other.curMaxNumIndices;
		bitBucket = other.bitBucket;
	}

	//std begin (must be lowercase)
	inline Iterator begin()
	{
		size_t bucket = 0;
		size_t bit = 0;
		FindFirst(bucket, bit);
		return Iterator(this, bucket, bit);
	}

	//std end (must be lowercase)
	//returns bit 0 of the lowest bucket that is not populated
	__forceinline Iterator end()
	{
		//get the last bucket with the 0th bit
		size_t bucket = bitBucket.size();
		size_t bit = 0;

		return Iterator(this, bucket, bit);
	}

	//iterates over all of the integers as efficiently as possible, passing them into func
	template<typename IntegerFunction>
	inline void IterateOver(IntegerFunction func, size_t up_to_index = std::numeric_limits<size_t>::max())
	{
		size_t end_integer = GetEndInteger();
		size_t num_buckets = (end_integer + 63) / 64;
		size_t num_indices = size();
		size_t end_index = std::min(up_to_index, end_integer);

		//if dense, loop over, assuming likely to hit
		//writing out this code yields notably better performance than
		//using ContainsWithoutMaximumIndexCheck and attempting to let the compiler optimize
		size_t indices_per_bucket = num_indices / num_buckets;
		if(indices_per_bucket >= 48)
		{
			for(size_t bucket = 0, index = 0;
				bucket < num_buckets; bucket++, index++)
			{
				uint64_t bucket_bits = bitBucket[bucket];
				for(size_t bit = 0; bit < 64; bit++)
				{
					uint64_t mask = (1ULL << bit);
					if(bucket_bits & mask)
						func(index);
				}
			}
		}
		else if(indices_per_bucket >= 32)
		{
			for(size_t index = 0; index < end_index; index++)
			{
				if(ContainsWithoutMaximumIndexCheck(index))
					func(index);
			}
		}
		else //use the iterator, which is more efficient when sparse
		{
			auto iter = begin();
			size_t index = *iter;
			while(index < end_index)
			{
				func(index);
				++iter;
				index = *iter;
			}
		}
	}

	//sets bucket and bit to the values pointing to the first id in the hash,
	// or the first element if it is empty
	inline void FindFirst(size_t &bucket, size_t &bit)
	{
		bucket = 0;
		bit = 0;

		if(bitBucket.size() == 0)
			return;

		//if the first isn't set, then find the next
		if(!(bitBucket[0] & 1))
			FindNext(bucket, bit);
	}

	//returns the first id in the hash, or size_t::max if there are no ids in the hash
	size_t First()
	{
		size_t bucket = 0;
		size_t bit = 0;
		FindFirst(bucket, bit);
		return GetIndexFromBucketAndBit(bucket, bit);
	}

	//sets bucket and bit to the values pointing to the next id in the hash
	// assumes that bucket and bit point to a valid index
	//if there are no more ids, then it will return bit 0 of the lowest bucket that is not populated
	inline void FindNext(size_t &bucket, size_t &bit)
	{
		bit++;

		//optimized early exit for dense arrays
		if(bit < numBitsPerBucket && (bitBucket[bucket] & (1ULL << bit)))
			return;

		//move on to next bucket if out of bits
		if(bit == numBitsPerBucket || bitBucket[bucket] < (1ULL << bit))
		{
			bit = 0;
			bucket++;

			//exit out if no more buckets
			if(bucket == bitBucket.size())
				return;
		}

		//there's leftover bits set, find the next
		if(bitBucket[bucket] > 0)
		{
			while((bitBucket[bucket] & (1ULL << bit)) == 0)
				bit++;
			return;
		}

		//empty bucket, skip until find non-empty or run out of buckets
		do
		{
			bucket++;
			if(bucket == bitBucket.size())
				return;
		} while(bitBucket[bucket] == 0);

		//if made it here, then there is nonzero value in bitBucket[bucket]
		bit = Platform_FindFirstBitSet(bitBucket[bucket]);
	}

	//returns the next id in the hash
	//if there are no more ids, then it will return bit 0 of the lowest bucket that is not populated
	inline size_t Next(size_t id)
	{
		size_t bucket = GetBucket(id);
		size_t bit = GetBit(id);
		FindNext(bucket, bit);
		return GetIndexFromBucketAndBit(bucket, bit);
	}

	//returns the nth id in the set by sorted order
	size_t GetNthElement(size_t n)
	{
		//if asking for something too big, just return last element (size)
		if(n > numElements)
			return GetEndInteger();

		//fast forward using poopulation count to find the bucket
		size_t iteration = 0;
		size_t bucket = 0;
		for(; bucket < bitBucket.size(); bucket++)
		{
			size_t bucket_count = __popcnt64(bitBucket[bucket]);
			//look for where the count exceeds n because the bit hasn't been found yet (e.g., bit 0 is found by the first count of 1)
			if(iteration + bucket_count > n)
				break;

			iteration += bucket_count;
		}

		//start iterating from the bucket where the bit was found
		// want to enter the loop at least once, even if iteration == n and have increments at the end,
		// so a for loop doesn't quite work and neither does a do-while, hence the while(true)
		size_t bit = 0;
		while(true)
		{
			//find next bit
			while((bitBucket[bucket] & (1ULL << bit)) == 0)
				bit++;

			if(iteration == n)
				break;

			//move on to subsequent iteration and bit
			iteration++;
			bit++;
		}

		return GetIndexFromBucketAndBit(bucket, bit);
	}

	//does not uniformly get an element, first selects a bucket at random, then selects an element in the bucket at random
	size_t GetRandomElement(RandomStream &random_stream)
	{
		//if there are significatly less elements than the set size, use the iterative method to select a uniformly random element
		if(curMaxNumIndices / 4 > numElements)
			return GetNthElement(random_stream.RandSize(numElements));

		//pick a bucket at random as long as it has some data in it (isn't 0)
		size_t bucket_index = random_stream.RandSize(bitBucket.size());
		while(bitBucket[bucket_index] == 0)
			bucket_index = random_stream.RandSize(bitBucket.size());

		size_t out = GetIndexFromBucketAndBit(bucket_index, 0);

		int rand_limit = numBitsPerBucket;

		//use a rough fast approximation for finding the largest bit set for the value in the bucket without having to loop over all its bits or having to take logs
		//break up smaller values into 4 categories so that we don't need to loop for a long time to find a small random number out of 64
		//for example any value under 128 only has a maximum of 7 bits set, so we'd do a randomStream.Rand() * 16 in that case
		if(bitBucket[bucket_index] < 65536) //any values with a max bit of 16
			rand_limit = 16;
		else if(bitBucket[bucket_index] < 4294967296) //any values with a max bit of 32
			rand_limit = 32;
		else if(bitBucket[bucket_index] < 281474976710656) //any values with a max bit of 48
			rand_limit = 48;

		//pick out a set bit in the bucket at random
		size_t bit = random_stream.RandSize(rand_limit);
		while((bitBucket[bucket_index] & (1ULL << bit)) == 0)
			bit = random_stream.RandSize(rand_limit);

		//output the index of the set bit
		return out + bit;
	}

	//clears the BitArrayIntegerSet as if it is new
	__forceinline void clear()
	{
		bitBucket.clear();
		curMaxNumIndices = 0;
		numElements = 0;
	}

	//returns the number of elements that exist in the hash set
	constexpr size_t size()
	{
		return numElements;
	}

	//resizes to best fit num_ids, updates curMaxNumIndices
	// will set all values as present or not based on fill_value
	__forceinline void resize(size_t num_ids, bool fill_value = false)
	{
		//num_ids is 1-based, need to get the bucket for 0-based,
		// then get the size, which adds 1 to the bucket
		size_t total_num_buckets = GetBucket(num_ids - 1) + 1;
		bitBucket.resize(total_num_buckets, fill_value ? 0xFFFFFFFFFFFFFFFFULL : 0);
		curMaxNumIndices = total_num_buckets * numBitsPerBucket;
	}

	//reserves space such that num_ids ranging from 0..num_ids-1 could then be directly placed into the hash
	// implements stl standard function
	inline void ReserveNumIntegers(size_t num_ids)
	{
		//this will catch num_ids = 0, since curMaxNumIndices can't be less than 0
		if(num_ids <= curMaxNumIndices)
			return;

		resize(num_ids);
	}

	//returns one past the maximum index in the container, 0 if empty
	constexpr size_t GetEndInteger()
	{
		if(numElements == 0)
			return 0;

		size_t bucket = bitBucket.size() - 1;
		while(bucket > 0 && bitBucket[bucket] == 0)
			bucket--;

		if(bitBucket[bucket] == 0)
			return 0;

		//return 1 past the max index
		return numBitsPerBucket * bucket + Platform_FindLastBitSet(bitBucket[bucket]) + 1;
	}

	//returns true if the id exists in the set
	__forceinline bool contains(size_t id)
	{
		if(id >= curMaxNumIndices)
			return false;

		uint64_t bucket = bitBucket[GetBucket(id)];
		uint64_t mask = (1ULL << GetBit(id));

		return bucket & mask;
	}

	//returns true if the id exists in the set
	// but does not check to see if the id is beyond the range
	__forceinline bool ContainsWithoutMaximumIndexCheck(size_t id)
	{
		uint64_t bucket = bitBucket[GetBucket(id)];
		uint64_t mask = (1ULL << GetBit(id));

		return bucket & mask;
	}

	//returns true if the id exists in the set
	__forceinline bool operator [](size_t id)
	{
		return contains(id);
	}

	//sets all up_to_id integers to true/exist
	void SetAllIds(size_t up_to_id)
	{
		if(up_to_id == 0)
		{
			clear();
			return;
		}

		resize(up_to_id, true);

		//set the last field if applicable
		if(up_to_id % numBitsPerBucket != 0)
		{
			size_t last_id = up_to_id - 1;
			size_t last_bucket = GetBucket(last_id);
			size_t first_unused_bit = GetBit(up_to_id);
			size_t last_bucket_value = 0xFFFFFFFFFFFFFFFFULL >> (numBitsPerBucket - first_unused_bit);
			bitBucket[last_bucket] = last_bucket_value;
		}

		numElements = up_to_id;
	}

	//inserts id into hash set, does nothing if id already exists in the hash
	inline void insert(size_t id)
	{
		ReserveNumIntegers(id + 1);

		uint64_t &bucket = bitBucket[GetBucket(id)];
		uint64_t mask = (1ULL << GetBit(id));
		if((bucket & mask) == 0)
		{
			//set bit to 1
			bucket |= mask;
			numElements++;
		}
	}

	//inserts all elements in collection
	template <typename Collection>
	__forceinline void insert(Collection &other)
	{
		for(const size_t element : other)
			insert(element);

		UpdateNumElements();
	}

	//inserts all elements in sis
	inline void InsertInBatch(SortedIntegerSet &sis)
	{
		if(sis.size() == 0)
			return;

		ReserveNumIntegers(sis.GetEndInteger());

		//if there are elements, need to check if overwriting for keeping numElements updated
		if(numElements > 0)
		{
			for(auto id : sis)
			{
				uint64_t &bucket = bitBucket[GetBucket(id)];
				uint64_t mask = (1ULL << GetBit(id));
				if((bucket & mask) == 0)
				{
					//set bit to 1
					bucket |= mask;
					numElements++;
				}
			}
		}
		else //can just insert and count
		{
			for(auto id : sis)
			{
				uint64_t &bucket = bitBucket[GetBucket(id)];
				uint64_t mask = (1ULL << GetBit(id));

				//set bit to 1
				bucket |= mask;
				numElements++;
			}
		}
	}

	//inserts all elements from other
	__forceinline void InsertInBatch(BitArrayIntegerSet &other)
	{
		Union(other);
	}

	//inserts all elements in collection
	template <typename Collection>
	__forceinline void InsertInBatch(Collection &other)
	{
		for(const size_t element : other)
			insert(element);
	}

	//insert an id is larger than or equal to GetEndInteger()
	__forceinline void InsertNewLargestInteger(size_t id)
	{
		insert(id);
	}

	//removes id from hash set, does nothing if id does not exist in the hash
	inline void erase(size_t id)
	{
		if(id >= curMaxNumIndices)
			return;

		uint64_t &bucket = bitBucket[GetBucket(id)];
		uint64_t mask = (1ULL << GetBit(id));

		//if nothing in the bucket, return early
		if((bucket & mask) == 0)
			return;

		//set bit to 0
		bucket &= ~mask;
		numElements--;

		TrimBack();
	}

	//Sets this to the BitArrayIntegerSet to the set that contains only elements that it contains that other does not contain
	// does NOT update the number of elements, so UpdateNumElements must be called
	void EraseInBatch(BitArrayIntegerSet &other)
	{
		size_t max_index = std::min(curMaxNumIndices, other.curMaxNumIndices);
		if(max_index == 0)
			return;

		size_t max_bucket = GetBucket(max_index - 1);

		//perform intersection
		for(size_t i = 0; i <= max_bucket; i++)
			bitBucket[i] &= ~(other.bitBucket[i]);

		TrimBack();
	}

	//erases all elements in collection
	template <typename Collection>
	__forceinline void EraseInBatch(Collection &collection)
	{
		for(const size_t id : collection)
		{
			if(id >= curMaxNumIndices)
				continue;

			uint64_t &bucket = bitBucket[GetBucket(id)];
			uint64_t mask = (1ULL << GetBit(id));
			if((bucket & mask) != 0)
			{
				//set bit to 0
				bucket &= ~mask;
				numElements--;
			}
		}

		TrimBack();
	}

	//removes all elements contained by other
	void erase(BitArrayIntegerSet &other)
	{
		EraseInBatch(other);
		UpdateNumElements();
	}

	//erases all elements in collection
	template <typename Collection>
	__forceinline void erase(Collection &other)
	{
		for(auto i : other)
			erase(i);

		TrimBack();
		UpdateNumElements();
	}

	//removes the id and returns true if it was in the id before removal
	bool EraseAndRetrieve(size_t id)
	{
		if(id >= curMaxNumIndices)
			return false;

		uint64_t &bucket = bitBucket[GetBucket(id)];
		uint64_t mask = (1ULL << GetBit(id));

		//if nothing in the bucket, return early
		if((bucket & mask) == 0)
			return false;

		//set bit to 0
		bucket &= ~mask;
		numElements--;

		TrimBack();

		return true;
	}

	//if id_from is present, it will "rename" it to id_to
	void ChangeIdIfPresent(size_t id_from, size_t id_to)
	{
		if(id_from >= curMaxNumIndices)
			return;

		uint64_t &bucket_from = bitBucket[GetBucket(id_from)];
		uint64_t mask_from = (1ULL << GetBit(id_from));

		//if the id isn't present, conclude
		if(!(bucket_from & mask_from))
			return;

		//remove id_from
		if((bucket_from & mask_from) != 0)
		{
			//set bit to 0
			bucket_from &= ~mask_from;
			numElements--;
		}

		insert(id_to);
		TrimBack();
	}

	//recomputes the number of inserted elements (may be necessary if doing parallel insertion operations like bit merging in union or intersect)
	// must be called if a Batch operation is used
	__forceinline void UpdateNumElements()
	{
		//update num elements
		numElements = 0;
		for(const auto &bucket : bitBucket)
			numElements += __popcnt64(bucket);
	}

	//trims off trailing empty buckets
	__forceinline void TrimBack()
	{
		//always want to leave one bucket left
		while(bitBucket.size() > 1 && bitBucket.back() == 0)
		{
			bitBucket.pop_back();
			curMaxNumIndices -= numBitsPerBucket;
		}
	}

	//Sets this to the BitArrayIntegerSet to the set that contains all elements of itself or other
	void Union(BitArrayIntegerSet &other)
	{
		//skip if empty
		if(other.curMaxNumIndices == 0)
			return;

		//make sure it can hold all of the other
		ReserveNumIntegers(other.curMaxNumIndices);

		//perform union
		for(size_t i = 0; i < other.bitBucket.size(); i++)
			bitBucket[i] |= other.bitBucket[i];

		UpdateNumElements();
	}

	//Sets this to the BitArrayIntegerSet to the set that contains only elements that it and another jointly contain
	// does NOT update the number of elements, so UpdateNumElements must be called
	void IntersectInBatch(BitArrayIntegerSet &other)
	{
		//if no intersection, then just clear and exit
		if(numElements == 0 || other.numElements == 0)
		{
			clear();
			return;
		}

		size_t this_bucket_end = bitBucket.size();
		size_t other_bucket_end = other.bitBucket.size();

		//perform intersection on overlap
		for(size_t i = 0; i < this_bucket_end && i < other_bucket_end; i++)
			bitBucket[i] &= other.bitBucket[i];

		//clear buckets after the other
		for(size_t i = other_bucket_end; i < this_bucket_end; i++)
			bitBucket[i] = 0;

		TrimBack();
	}

	//Sets this to the BitArrayIntegerSet to the set that contains only elements that it and another jointly contain
	inline void Intersect(BitArrayIntegerSet &other)
	{
		IntersectInBatch(other);
		UpdateNumElements();
	}

	//Sets this to the BitArrayIntegerSet to the set that contains only elements that it and sis jointly contain
	// does NOT update the number of elements, so UpdateNumElements must be called
	void IntersectInBatch(SortedIntegerSet &sis)
	{
		if(numElements == 0)
			return;

		if(sis.size() == 0)
		{
			clear();
			return;
		}

		//remove elements off the top first for efficiency
		size_t sis_end_index = sis.GetEndInteger();
		resize(sis_end_index);
		size_t num_buckets = bitBucket.size();

		//intersect
		size_t cur_id = 0;
		size_t cur_bucket = 0;
		for(auto other_id : sis)
		{
			size_t other_id_bucket = GetBucket(other_id);
			//if next id is beyond last bucket, then just truncate
			if(other_id_bucket >= num_buckets)
			{
				bitBucket.resize(cur_bucket + 1);
				break;
			}

			//any buckets that need to be skipped should be zeroed out
			if(other_id_bucket > cur_bucket)
			{
				//if there are any bits left in the last bucket after the last cur_id, clear them
				size_t first_empty_bit = GetBit(cur_id);
				if(first_empty_bit > 0)
				{
					size_t last_bucket_bitmask = (0xFFFFFFFFFFFFFFFFULL >> (numBitsPerBucket - first_empty_bit));
					bitBucket[cur_bucket] &= last_bucket_bitmask;
				}
				//set cur_id to the next id past the bucket
				cur_bucket = GetBucket(cur_id + numBitsPerBucket - 1);
				cur_id = numBitsPerBucket * cur_bucket;

				//zero out buckets skipped over
				cur_id += numBitsPerBucket * (other_id_bucket - cur_bucket);
				for(; cur_bucket < other_id_bucket; cur_bucket++)
					bitBucket[cur_bucket] = 0;
			}

			//zero out everything until the other id
			auto &bucket_value = bitBucket[cur_bucket];
			for(; cur_id < other_id; cur_id++)
				bucket_value &= ~(1ULL << GetBit(cur_id));

			//cur_id and other_id are in both sets, so don't remove it
			cur_id++;
			cur_bucket = GetBucket(cur_id);
		}

		//if there are any bits left in the last bucket after the last cur_id, clear them
		if(cur_bucket < bitBucket.size())
		{
			size_t first_empty_bit = GetBit(cur_id);
			if(first_empty_bit > 0)
			{
				size_t last_bucket_bitmask = (0xFFFFFFFFFFFFFFFFULL >> (numBitsPerBucket - first_empty_bit));
				bitBucket[cur_bucket] &= last_bucket_bitmask;
			}
		}

		curMaxNumIndices = (bitBucket.size() * numBitsPerBucket);
		TrimBack();
	}

	//Sets this to the BitArrayIntegerSet to the set that contains only elements that it and sis jointly contain
	__forceinline void Intersect(SortedIntegerSet &sis)
	{
		IntersectInBatch(sis);
		UpdateNumElements();
	}

	//flips the elements in the set starting with element 0 up to but not including up_to_id
	// resetting the size of the container
	void Not(size_t up_to_id)
	{
		if(up_to_id == 0)
		{
			clear();
			return;
		}

		resize(up_to_id);

		//flip buckets up to the last bucket
		size_t num_buckets = bitBucket.size();
		for(size_t i = 0; i < num_buckets; i++)
			bitBucket[i] = ~bitBucket[i];

		//clear any remaining bits in the last bucket
		size_t up_to_bit = GetBit(up_to_id);
		if(up_to_bit > 0)
		{
			size_t last_bucket_bitmask = (0xFFFFFFFFFFFFFFFFULL >> (numBitsPerBucket - up_to_bit));
			size_t last_bucket = num_buckets - 1;
			bitBucket[last_bucket] &= last_bucket_bitmask;
		}

		TrimBack();
		UpdateNumElements();
	}

	//sets elements to the flip of the elements in other up to but not including up_to_id
	// up_to_id must be at least as large as the max index of other
	void Not(BitArrayIntegerSet &other, size_t up_to_id)
	{
		if(up_to_id == 0)
		{
			clear();
			return;
		}

		resize(up_to_id);

		//flip buckets up to the last other bucket
		size_t num_other_buckets = other.bitBucket.size();
		for(size_t i = 0; i < num_other_buckets; i++)
			bitBucket[i] = ~other.bitBucket[i];

		//fill in any past the other's max
		size_t num_buckets = bitBucket.size();
		for(size_t i = num_other_buckets; i < num_buckets; i++)
			bitBucket[i] = 0xFFFFFFFFFFFFFFFFULL;

		//clear any remaining bits in the last bucket
		size_t up_to_bit = GetBit(up_to_id);
		if(up_to_bit > 0)
		{
			size_t last_bucket_bitmask = (0xFFFFFFFFFFFFFFFFULL >> (numBitsPerBucket - up_to_bit));
			size_t last_bucket = num_buckets - 1;
			bitBucket[last_bucket] &= last_bucket_bitmask;
		}

		TrimBack();
		UpdateNumElements();
	}

	//bits per bucket given uint64_t
	static constexpr size_t numBitsPerBucket = 64;

protected:

	//gets the bucket index for a given id
	constexpr size_t GetBucket(size_t id)
	{
		return id / numBitsPerBucket;
	}

	//gets the bit index for a given id
	constexpr size_t GetBit(size_t id)
	{
		return id % numBitsPerBucket;
	}

	constexpr size_t GetIndexFromBucketAndBit(size_t bucket, size_t bit)
	{
		return (bucket * numBitsPerBucket) + bit;
	}

	//num elements that exist as inserted in the hash
	size_t numElements;

	//maximum possible index for the given number of data buckets
	size_t curMaxNumIndices;

	//buffer of bit buckets
	std::vector<uint64_t> bitBucket;
};

class EfficientIntegerSet
{
public:
	//defined to keep compatibility with stl containers
	using value_type = size_t;

	EfficientIntegerSet()
		: isSisContainer(true)
	{	}

	//assignment operator, deep copies
	inline void operator =(const EfficientIntegerSet &other)
	{
		isSisContainer = other.isSisContainer;

		if(other.isSisContainer)
			sisContainer = other.sisContainer;
		else
			baisContainer = other.baisContainer;
	}

	//assignment operator, deep copies bit buffer
	inline void operator =(const SortedIntegerSet &other)
	{
		baisContainer.clear();
		isSisContainer = true;
		sisContainer = other;
	}

	//assignment operator, deep copies bit buffer
	inline void operator =(const BitArrayIntegerSet &other)
	{
		sisContainer.clear();
		isSisContainer = false;
		baisContainer = other;
	}

	//copies the data to other
	inline void CopyTo(BitArrayIntegerSet &other)
	{
		if(isSisContainer)
		{
			other.clear();
			other.insert(sisContainer);
		}
		else
			other = baisContainer;
	}

	struct Iterator
	{
		inline Iterator(const Iterator &other)
		{
			isSisContainer = other.isSisContainer;

			if(other.isSisContainer)
				sisIterator = other.sisIterator;
			else
				baisIterator = other.baisIterator;
		}

		inline Iterator(SortedIntegerSet::Iterator _iterator)
		{
			sisIterator = _iterator;
			isSisContainer = true;
		}

		inline Iterator(BitArrayIntegerSet::Iterator _iterator)
		{
			baisIterator = _iterator;
			isSisContainer = false;
		}

		~Iterator()
		{	}

		inline Iterator operator =(const Iterator &other)
		{
			isSisContainer = other.isSisContainer;

			if(other.isSisContainer)
				sisIterator = other.sisIterator;
			else
				baisIterator = other.baisIterator;

			return *this;
		}

		constexpr bool operator ==(const Iterator &other)
		{
			if(isSisContainer)
				return (sisIterator == other.sisIterator);
			else
				return (baisIterator == other.baisIterator);
		}

		constexpr bool operator !=(const Iterator &other)
		{
			if(isSisContainer)
				return (sisIterator != other.sisIterator);
			else
				return (baisIterator != other.baisIterator);
		}

		__forceinline Iterator &operator ++()
		{
			if(isSisContainer)
				++sisIterator;
			else
				++baisIterator;

			return *this;
		}

		//dereference operator
		constexpr size_t operator *()
		{
			if(isSisContainer)
				return *sisIterator;
			else
				return *baisIterator;
		}

		SortedIntegerSet::Iterator sisIterator;
		BitArrayIntegerSet::Iterator baisIterator;

		bool isSisContainer;
	};

	//std begin (must be lowercase)
	__forceinline auto begin()
	{
		if(isSisContainer)
			return Iterator(sisContainer.begin());
		else
			return Iterator(baisContainer.begin());
	}

	//std end (must be lowercase)
	__forceinline auto end()
	{
		if(isSisContainer)
			return Iterator(sisContainer.end());
		else
			return Iterator(baisContainer.end());
	}

	//iterates over all elements in the container, passing in the value to func
	//this is intended for fast operations performed at volume, where even small bits
	//of extra logic in the iterator would affect performance
	template<typename ElementFunc>
	__forceinline void IterateFunctionOverElements(ElementFunc func)
	{
		if(isSisContainer)
		{
			for(auto element : sisContainer)
				func(element);
		}
		else
		{
			for(auto element : baisContainer)
				func(element);
		}
	}

	//returns the nth id in the set by sorted order
	inline size_t GetNthElement(size_t n)
	{
		if(isSisContainer)
			return sisContainer.GetNthElement(n);
		else
			return baisContainer.GetNthElement(n);
	}

	//gets a random element in a performant way
	// note that if it is a bais container, it will not necessarily obtain elements with uniform probability
	inline size_t GetRandomElement(RandomStream &random_stream)
	{
		if(isSisContainer)
			return sisContainer.GetRandomElement(random_stream);
		else
			return baisContainer.GetRandomElement(random_stream);
	}

	//clears the container as if it is new
	inline void clear()
	{
		if(isSisContainer)
			sisContainer.clear();
		else
			baisContainer.clear();
	}

	//returns the number of elements that exist
	__forceinline size_t size()
	{
		if(isSisContainer)
			return sisContainer.size();
		else
			return baisContainer.size();
	}

	//reserves the number of elements to be inserted
	__forceinline void ReserveNumIntegers(size_t num_elements)
	{
		if(isSisContainer)
			sisContainer.ReserveNumIntegers(num_elements);
		else
			baisContainer.ReserveNumIntegers(num_elements);
	}

	//returns one past the maximum index in the container, 0 if empty
	inline size_t GetEndInteger()
	{
		if(isSisContainer)
			return sisContainer.GetEndInteger();
		else
			return baisContainer.GetEndInteger();
	}

	//returns true if the id exists in the set
	inline bool contains(size_t id)
	{
		if(isSisContainer)
			return sisContainer.contains(id);
		else
			return baisContainer.contains(id);
	}

	//returns true if the id exists in the set
	inline bool operator [](size_t id)
	{
		if(isSisContainer)
			return sisContainer.contains(id);
		else
			return baisContainer.contains(id);
	}

	//sets all up_to_id integers to true/exist
	void SetAllIds(size_t up_to_id)
	{
		if(isSisContainer)
			ConvertSisToBais();

		baisContainer.SetAllIds(up_to_id);
	}

	//inserts id into set, does nothing if id already exists
	void insert(size_t id)
	{
		if(isSisContainer)
		{
			sisContainer.insert(id);
			ConvertSisToBaisIfBetter();
		}
		else
		{
			baisContainer.insert(id);
			ConvertBaisToSisIfBetter();
		}
	}

	//inserts all elements from other
	__forceinline void InsertInBatch(EfficientIntegerSet &other)
	{
		if(other.isSisContainer)
		{
			if(isSisContainer)
				sisContainer.InsertInBatch(other.sisContainer);
			else
				baisContainer.InsertInBatch(other.sisContainer);
		}
		else
		{
			if(isSisContainer)
				sisContainer.InsertInBatch(other.baisContainer);
			else
				baisContainer.InsertInBatch(other.baisContainer);
		}
	}

	//inserts all elements in collection
	template <typename Collection>
	__forceinline void InsertInBatch(Collection &other)
	{
		if(isSisContainer)
			sisContainer.InsertInBatch(other);
		else
			baisContainer.InsertInBatch(other);
	}

	//quickly inserts an id
	// it assumes that the id is larger than GetEndInteger()
	inline void InsertNewLargestInteger(size_t id)
	{
		if(isSisContainer)
		{
			sisContainer.InsertNewLargestInteger(id);
			ConvertSisToBaisIfBetter();
		}
		else
		{
			baisContainer.insert(id);
			ConvertBaisToSisIfBetter();
		}
	}

	//removes id from hash set, does nothing if id does not exist in the hash
	void erase(size_t id)
	{
		if(isSisContainer)
		{
			sisContainer.erase(id);
			ConvertSisToBaisIfBetter();
		}
		else
		{
			baisContainer.erase(id);
			ConvertBaisToSisIfBetter();
		}
	}

	//removes all elements contained by other
	void erase(EfficientIntegerSet &other)
	{
		if(isSisContainer)
		{
			sisContainer.erase(other);
			ConvertSisToBaisIfBetter();
		}
		else
		{
			baisContainer.erase(other);
			ConvertBaisToSisIfBetter();
		}
	}

	//removs all elements of this container from other
	inline void EraseTo(BitArrayIntegerSet &other, bool in_batch = false)
	{
		if(isSisContainer)
		{
			if(in_batch)
				other.EraseInBatch(sisContainer);
			else
				other.erase(sisContainer);
		}
		else
		{
			if(in_batch)
				other.EraseInBatch(baisContainer);
			else
				other.erase(baisContainer);
		}
	}

	//removes all elements contained by other, intended for calling in a batch
	template<typename Container>
	inline void EraseInBatch(Container &other)
	{
		if(isSisContainer)
		{
			sisContainer.EraseInBatch(other);
			ConvertSisToBaisIfBetter();
		}
		else
		{
			baisContainer.EraseInBatch(other);
			ConvertBaisToSisIfBetter();
		}
	}

	//removes all elements from other in this container, intended for calling in a batch
	void EraseInBatchFrom(BitArrayIntegerSet &other)
	{
		if(isSisContainer)
			other.EraseInBatch(sisContainer);
		else
			other.EraseInBatch(baisContainer);
	}

	//removes all elements contained by other, intended for calling in a batch
	inline void EraseInBatch(EfficientIntegerSet &other)
	{
		if(isSisContainer)
		{
			if(other.isSisContainer)
				sisContainer.EraseInBatch(other.sisContainer);
			else
				sisContainer.EraseInBatch(other.baisContainer);

			ConvertSisToBaisIfBetter();
		}
		else
		{
			if(other.isSisContainer)
				baisContainer.EraseInBatch(other.sisContainer);
			else
				baisContainer.EraseInBatch(other.baisContainer);

			ConvertBaisToSisIfBetter();
		}
	}

	//removes the id and returns true if it was in the id before removal
	inline bool EraseAndRetrieve(size_t id)
	{
		if(isSisContainer)
		{
			if(sisContainer.EraseAndRetrieve(id))
			{
				ConvertSisToBaisIfBetter();
				return true;
			}
		}
		else
		{
			if(baisContainer.EraseAndRetrieve(id))
			{
				ConvertBaisToSisIfBetter();
				return true;
			}
		}

		return false;
	}

	//updates the number of elements
	void UpdateNumElements()
	{
		if(isSisContainer)
		{
			sisContainer.UpdateNumElements();
			ConvertSisToBaisIfBetter();
		}
		else
		{
			baisContainer.UpdateNumElements();
			ConvertBaisToSisIfBetter();
		}
	}

	//sets this to the set that contains all elements of itself or other
	void Union(EfficientIntegerSet &other)
	{
		//see if should convert to bais before merging to speed things up
		if(isSisContainer)
		{
			size_t lower_bound_num_elements = std::max(sisContainer.size(), other.size());
			size_t lower_bound_max_size = std::max(sisContainer.GetEndInteger(), other.GetEndInteger());
			if(IsBaisPreferredToSis(lower_bound_num_elements, lower_bound_max_size))
				ConvertSisToBais();
		}

		if(isSisContainer)
		{
			if(other.isSisContainer)
				sisContainer.insert(other.sisContainer);
			else
				sisContainer.insert(other.baisContainer);

			ConvertSisToBaisIfBetter();
		}
		else
		{
			if(other.isSisContainer)
				baisContainer.insert(other.sisContainer);
			else
				baisContainer.Union(other.baisContainer);

			ConvertBaisToSisIfBetter();
		}
	}

	//sets other to the set that contains all elements of itself or other
	inline void UnionTo(BitArrayIntegerSet &other)
	{
		if(IsSisContainer())
			other.insert(sisContainer);
		else
			other.Union(baisContainer);
	}

	//sets this to the set that contains only elements that it and other jointly contain
	void Intersect(EfficientIntegerSet &other)
	{
		//see if should convert to sis before merging to speed things up
		if(!isSisContainer)
		{
			size_t upper_bound_num_elements = std::min(sisContainer.size(), other.size());
			size_t upper_bound_max_size = std::min(sisContainer.GetEndInteger(), other.GetEndInteger());
			if(IsSisPreferredToBais(upper_bound_num_elements, upper_bound_max_size))
				ConvertBaisToSis();
		}

		if(isSisContainer)
		{
			if(other.isSisContainer)
				sisContainer.Intersect(other.sisContainer);
			else
				sisContainer.Intersect(other.baisContainer);

			ConvertSisToBaisIfBetter();
		}
		else
		{
			if(other.isSisContainer)
				baisContainer.Intersect(other.sisContainer);
			else
				baisContainer.Intersect(other.baisContainer);

			ConvertBaisToSisIfBetter();
		}
	}

	//sets other to the set that contains only elements that it and other jointly contain
	inline void IntersectTo(BitArrayIntegerSet &other, bool in_batch = false)
	{
		if(IsSisContainer())
		{
			if(in_batch)
				other.IntersectInBatch(sisContainer);
			else
				other.Intersect(sisContainer);
		}
		else
		{
			if(in_batch)
				other.IntersectInBatch(baisContainer);
			else
				other.Intersect(baisContainer);
		}
	}

	//flips the elements in the set starting with element 0 up to but not including up_to_id
	// resetting the size of the container
	void Not(size_t up_to_id)
	{
		if(isSisContainer)
		{
			//if it was a sisContainer, then it was sparse, so convert to baisContainer
			//set all and remove those from sisContainer
			baisContainer.SetAllIds(up_to_id);
			baisContainer.erase(sisContainer);
			sisContainer.clear();
			isSisContainer = false;
		}
		else
		{
			baisContainer.Not(up_to_id);
			ConvertBaisToSisIfBetter();
		}
	}

	//sets elements to the flip of the elements in other up to but not including up_to_id
	// up_to_id must be at least as large as the max index of other
	template<typename Container>
	void Not(Container &other, size_t up_to_id)
	{
		clear();
		isSisContainer = false;

		if(other.isSisContainer)
		{
			//if it was a sisContainer, then it was sparse, so convert to baisContainer
			//set all and remove those from sisContainer
			baisContainer.SetAllIds(up_to_id);
			baisContainer.erase(other.sisContainer);
		}
		else
		{
			baisContainer.Not(other.baisContainer, up_to_id);
			ConvertBaisToSisIfBetter();
		}
	}

	//sets other's elements to the flip of the elements up to but not including up_to_id
	// up_to_id must be at least as large as the max index of other
	void NotTo(BitArrayIntegerSet &other, size_t up_to_id)
	{
		if(isSisContainer)
		{
			other.SetAllIds(up_to_id);
			other.erase(sisContainer);
		}
		else
		{
			other.Not(baisContainer, up_to_id);
		}
	}

	//functions for specialized use

	constexpr bool IsSisContainer()
	{
		return isSisContainer;
	}

	constexpr bool IsBaisContainer()
	{
		return !isSisContainer;
	}

	constexpr auto &GetSisContainer()
	{
		return sisContainer;
	}

	constexpr auto &GetBaisContainer()
	{
		return baisContainer;
	}

protected:

	//returns true if it would be more efficient to convert from sis to bais
	//assumes conitainer is already sis
	inline bool IsBaisPreferredToSis(size_t num_elements, size_t max_element)
	{
		//add 1 to round up to make it less likely to flip back and forth between types
		size_t num_bais_elements_required = ((max_element + BitArrayIntegerSet::numBitsPerBucket - 1) / BitArrayIntegerSet::numBitsPerBucket) + 1;
		//use a heuristic of 2 values per bais bucket, since some operations are faster when can just iterate over a list
		return (num_elements > 2 * num_bais_elements_required);
	}

	//returns true if it would be more efficient to convert from bais to sis
	//assumes conitainer is already bais
	inline bool IsSisPreferredToBais(size_t num_elements, size_t max_element)
	{
		//round this down (don't take ceil) to make it less likely to flip back and forth between types
		size_t num_bais_elements_required = (max_element + BitArrayIntegerSet::numBitsPerBucket - 1) / BitArrayIntegerSet::numBitsPerBucket;
		//use a heuristic of 2 values per bais bucket, since some operations are faster when can just iterate over a list
		return (2 * num_bais_elements_required > num_elements);
	}

	//converts data storage to bais; assumes it is already sis
	inline void ConvertSisToBais()
	{
		baisContainer.InsertInBatch(sisContainer);
		sisContainer.clear();
		isSisContainer = false;
	}

	//converts data storage to sis; assumes it is already bais
	inline void ConvertBaisToSis()
	{
		sisContainer.InsertNewSortedIntegers(baisContainer);
		baisContainer.clear();
		isSisContainer = true;
	}

	//automatically converts Sis to Bais when better
	//assumes isSisContainer is true
	__forceinline void ConvertSisToBaisIfBetter()
	{
		if(IsBaisPreferredToSis(sisContainer.size(), sisContainer.GetEndInteger()))
			ConvertSisToBais();
	}

	//automatically converts Bais to Sis when better
	//assumes isSisContainer is false
	__forceinline void ConvertBaisToSisIfBetter()
	{
		if(IsSisPreferredToBais(baisContainer.size(), baisContainer.GetEndInteger()))
			ConvertBaisToSis();
	}

	//if true, use sisContainer, if false use baisContainer
	bool isSisContainer;

	//keep both container types
	SortedIntegerSet sisContainer;
	BitArrayIntegerSet baisContainer;
};
