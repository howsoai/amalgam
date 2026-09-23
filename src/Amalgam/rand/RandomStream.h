#pragma once

//project headers:
#include "FastMath.h"

//system headers:
#include <cmath>
#include <limits>
#include <queue>
#include <string>

//Implements a stateful stream of random numbers that can be serialized/deserialized easily into a
// very small amount of data, based on:
//  O'Neill, Melissa E. "PCG: A family of simple fast space-efficient statistically good algorithms
//  for random number generation." ACM Transactions on Mathematical Software(2014).
//More info at https://www.pcg-random.org
class RandomStream
{
public:
	constexpr RandomStream()
		: increment(0), state(0)
	{}

	RandomStream(const std::string initial_state);

	constexpr RandomStream(const RandomStream &stream)
		: increment(stream.increment), state(stream.state)
	{}

	//gets the current state of the random stream in string form
	std::string GetState();

	//sets (seeds) the current state of the random stream based on string
	void SetState(const std::string &new_state);

	//returns a random seed based on this stream's current state and seed_string parameter
	std::string CreateOtherStreamStateViaString(std::string_view seed_string);

	//returns a RandomStream based on this stream's current state and seed_string parameter
	RandomStream CreateOtherStreamViaString(const std::string &seed_string);

	//consumes random numbers from the stream to create a new RandomStream
	RandomStream CreateOtherStreamViaRand();

	//returns a value in the range [0.0,1.0) with 32 bits of randomness
	inline double Rand()
	{
		return std::ldexp(RandUInt32(), -32);
	}

	//returns a value in the range [0.0,1.0) with full mantissa of randomness
	inline double RandFull()
	{
		uint64_t combined = (static_cast<uint64_t>(RandUInt32()) << 32) | static_cast<uint64_t>(RandUInt32());
		return std::ldexp(static_cast<double>(combined & ((static_cast<uint64_t>(1) << 53) - 1)), -53);
	}

	//returns a uint32_t random number
	uint32_t RandUInt32();

	inline size_t RandSize(size_t max_size)
	{
		if(max_size == 0)
			return 0;

		if(max_size < std::numeric_limits<uint32_t>::max())
			return (RandUInt32() % max_size);

		//else 64-bit
		size_t r = ((static_cast<size_t>(RandUInt32()) << 32) | RandUInt32());
		return r % max_size;
	}

	//returns a positive number chosen from the exponential distribution with specified mean
	inline double ExponentialRand(double mean)
	{
		return -std::log(1.0 - RandFull()) * mean;
	}

	//size of the random state as a string
	static constexpr size_t randStateStringifiedSizeInBytes = (sizeof(int64_t) * 2 + 1);

protected:

	//based on the published literature, burns through the minimum number of random numbers
	// to make sure the subsequent stream is good
	inline void BurnIn()
	{
		RandUInt32();
		RandUInt32();
	}

	//current state / seed of the random stream
	uint64_t increment;
	uint64_t state;
};

//class that operates like std::priority_queue but can clear and reserve buffers
//and works exactly the same across all platforms (Apple's implementation is known to operate differently than others)
template<class T, class Container = std::vector<T>, class Compare = std::less<typename Container::value_type>>
class FlexiblePriorityQueue
{
public:
	FlexiblePriorityQueue() = default;
	explicit FlexiblePriorityQueue(const Compare &compare) : comp(compare)
	{ }

	explicit FlexiblePriorityQueue(size_t count, const Compare &compare) : comp(compare)
	{
		c.reserve(count);
	}

	template<typename... Args> void emplace(Args &&...args)
	{
		c.emplace_back(std::forward<Args>(args)...);
		SiftUp(c.size() - 1);
	}

	void push(const T &value)
	{
		c.push_back(value);
		SiftUp(c.size() - 1);
	}

	void pop()
	{
		if(c.empty())
			return;

		//move last element to top and sift down
		c[0] = std::move(c.back());
		c.pop_back();
		if(!c.empty())
			SiftDown(0);
	}

	const T &top() const
	{
		return c[0];
	}

	size_t size() const
	{
		return c.size();
	}

	bool empty() const
	{
		return c.empty();
	}

	void Reserve(size_t reserve_size)
	{
		c.reserve(reserve_size);
	}

	void clear()
	{
		c.clear();
	}

	Compare &GetComparator()
	{
		return comp;
	}

private:
	void SiftUp(size_t index)
	{
		while(index > 0)
		{
			size_t parent = (index - 1) / 2;

			//use strict weak ordering; only swap if child is strictly greater than parent
			if(comp(c[parent], c[index]))
			{
				std::swap(c[index], c[parent]);
				index = parent;
			}
			else
			{
				break;
			}
		}
	}

	void SiftDown(size_t index)
	{
		size_t n = c.size();

		while(2 * index + 1 < n)
		{
			size_t child = 2 * index + 1;

			//if children are equal, child stays as the left child (2*index + 1)
			//only move to the right child if right is strictly greater than left
			if(child + 1 < n && comp(c[child], c[child + 1]))
				child++;

			//only swap if the chosen child is strictly greater than the parent
			if(comp(c[index], c[child]))
			{
				std::swap(c[index], c[child]);
				index = child;
			}
			else
			{
				break;
			}
		}
	}

	Container c;
	Compare comp;
};

//Priority queue that, when receiving values of equal priority, will randomize the order they are stored and popped off the queue
//Ties are broken by a key derived from each element's entity index and a seed,
// so for a given seed the result is independent of the order elements are pushed
//Requires the type QueueElementType to have both the < and == operators, and a GetEntityIndex method
//The constructor requires a seed
template<typename QueueElementType, typename ComparisonValueType>
class StochasticTieBreakingPriorityQueue
{
public:

	typedef std::vector<QueueElementType> PriorityQueueContainerType;

	StochasticTieBreakingPriorityQueue() :
		priorityQueue(StochasticTieBreakingComparator())
	{}

	//seeds the priority queue
	StochasticTieBreakingPriorityQueue(RandomStream* stream)
		: priorityQueue(StochasticTieBreakingComparator())
	{
		SetSeed(stream);
	}

	__forceinline void SetSeed(RandomStream* stream)
	{
		clear();
		priorityQueue.GetComparator().tieBreakSeed
			= (static_cast<uint64_t>(stream->RandUInt32()) << 32) | stream->RandUInt32();
	}

	__forceinline void SetIncludeAllThreshold(ComparisonValueType threshold)
	{
		includeAllThreshold = threshold;
	}

	__forceinline void Reserve(size_t reserve_size)
	{
		//reserve an extra element because pushing a value on the top and popping one off requires having an extra space
		priorityQueue.Reserve(reserve_size + 1);
	}

	__forceinline void clear()
	{
		priorityQueue.clear();
	}

	//resets the object, as well as the same effect of calling all appropriate the setters
	inline void Reset(RandomStream* stream, size_t reserve_size, ComparisonValueType threshold)
	{
		SetSeed(stream);
		Reserve(reserve_size);
		SetIncludeAllThreshold(threshold);
	}

	//these functions mimic their respective std::priority_queue functions
	__forceinline size_t Size()
	{
		return priorityQueue.size();
	}

	__forceinline const QueueElementType &Top() const
	{
		return priorityQueue.top();
	}

	__forceinline const bool TopMeetsThreshold() const
	{
		auto &top = Top();
		return (top > includeAllThreshold);
	}

	__forceinline void Push(const QueueElementType &val)
	{
		priorityQueue.push(val);
	}

	//like Push, but retains the current size of the priority queue
	//requires that there is at least one element in the priority queue
	//returns the top element after the push and pop has been completed
	template<bool expand_to_include_all_threshold = false>
	__forceinline const QueueElementType PushAndPop(const QueueElementType &val)
	{
		if constexpr(expand_to_include_all_threshold)
		{
			if(val <= includeAllThreshold)
			{
				Push(val);

				//make copy of the top and pop it
				auto top_value = Top();
				Pop();

				//if the next largest size is zero, then need to put the non-zero value back in sorted_results
				if(Top() <= includeAllThreshold)
				{
					Push(top_value);
					return top_value;
				}

				return Top();
			}
		}
		//not expanding to include threshold all below

		//if better than the top, including winning a tie break, then exchange it
		auto &top = priorityQueue.top();
		if(priorityQueue.GetComparator()(val, top))
		{
			priorityQueue.pop();
			priorityQueue.push(val);
		}
		//otherwise don't need to do anything, val is not better than the worst on the stack

		return priorityQueue.top();
	}

	__forceinline void Pop()
	{
		priorityQueue.pop();
	}

	__forceinline bool Empty()
	{
		return priorityQueue.empty();
	}

protected:

	//used to compare first by the value, second by the tie-break key if equal
	//tie-break keys are only computed when values are equal, since ties are rare
	class StochasticTieBreakingComparator
	{
	public:
		inline bool operator()(const QueueElementType &a, const QueueElementType &b) const
		{
			if(a == b)
				return GetTieBreakKey(a.GetEntityIndex()) < GetTieBreakKey(b.GetEntityIndex());
			return a < b;
		}

		//deterministically maps entity_index to a pseudorandom key based on tieBreakSeed
		//for a fixed seed this is a bijection over uint64_t (multiplying by an odd constant and xoring
		// with the seed are each invertible), so distinct indices never collide and ties are broken
		// independently of the order elements are pushed
		//relies on tieBreakSeed being well mixed, as it comes from a RandomStream
		__forceinline uint64_t GetTieBreakKey(uint64_t entity_index) const
		{
			uint64_t x = (entity_index * 0x9e3779b97f4a7c15ULL) ^ tieBreakSeed;
			return x * 0xd6e8feb86659fd93ULL;
		}

		uint64_t tieBreakSeed = 0;
	};

	FlexiblePriorityQueue<QueueElementType, PriorityQueueContainerType, StochasticTieBreakingComparator> priorityQueue;

	//threshold below which all elements should be kept by PushAndPopToThreshold
	ComparisonValueType includeAllThreshold;
};
