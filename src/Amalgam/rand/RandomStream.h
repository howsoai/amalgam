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
//  O’Neill, Melissa E. "PCG: A family of simple fast space-efficient statistically good algorithms
//  for random number generation." ACM Transactions on Mathematical Software(2014).
//More info at https://www.pcg-random.org
class RandomStream
{
public:
	constexpr RandomStream()
		: increment(0), state(0)
	{	}

	RandomStream(const std::string initial_state);

	constexpr RandomStream(const RandomStream &stream)
		: increment(stream.increment),  state(stream.state)
	{	}

	//gets the current state of the random stream in string form
	std::string GetState();

	//sets (seeds) the current state of the random stream based on string
	void SetState(const std::string &new_state);

	//returns a random seed based on this stream's current state and seed_string parameter
	std::string CreateOtherStreamStateViaString(const std::string &seed_string);

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

//class to enable std::priority_queue to be able to clear and reserve buffers, but requires containers that
//support those operations
template<class T, class Container = std::vector<T>, class Compare = std::less<typename Container::value_type> >
class FlexiblePriorityQueue : public std::priority_queue<T, Container, Compare>
{
public:
	//inherit all constructors
	using std::priority_queue<T, Container, Compare>::priority_queue;

	__forceinline void Reserve(size_t reserve_size)
	{
		//this-> is needed for some compilers to give access due to how the STL is implemented
		this->c.reserve(reserve_size);
	}

	__forceinline void clear()
	{
		//this-> is needed for some compilers to give access due to how the STL is implemented
		this->c.clear();
	}
};

//Priority queue that, when receiving values of equal priority, will randomize the order they are stored and popped off the queue
//Requires the type QueueElementType to have both the < and == operators
//The constructor requires a seed
template<typename QueueElementType, typename ComparisonValueType>
class StochasticTieBreakingPriorityQueue
{
public:

	typedef std::vector<std::pair<QueueElementType, uint32_t>> PriorityQueueContainerType;

	StochasticTieBreakingPriorityQueue() : 
		priorityQueue(StochasticTieBreakingComparator())
	{	}

	//seeds the priority queue
	StochasticTieBreakingPriorityQueue(std::string seed)
		: priorityQueue(StochasticTieBreakingComparator()), randomStream(seed)
	{	}

	StochasticTieBreakingPriorityQueue(RandomStream stream)
		: priorityQueue(StochasticTieBreakingComparator()), randomStream(stream)
	{	}

	__forceinline void SetSeed(std::string seed)
	{
		randomStream.SetState(seed);
	}

	__forceinline void SetStream(RandomStream stream)
	{
		randomStream = stream;
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
	inline void Reset(RandomStream stream, size_t reserve_size, ComparisonValueType threshold)
	{
		SetStream(stream);
		clear();
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
		return priorityQueue.top().first;
	}

	__forceinline void Push(const QueueElementType &val)
	{
		priorityQueue.emplace(val, randomStream.RandUInt32());
	}

	//like Push, but retains the current size of the priority queue
	//requires that there is at least one element in the priority queue
	//returns the top element after the push and pop has been completed
	__forceinline const QueueElementType &PushAndPop(const QueueElementType &val)
	{
		auto &top = priorityQueue.top();
		if(val < top.first)
		{
			//better, so exchange it
			priorityQueue.pop();
			priorityQueue.emplace(val, randomStream.RandUInt32());

			return priorityQueue.top().first;
		}
		else if(val == top.first)
		{
			//good enough to consider for top, check random
			uint32_t r = randomStream.RandUInt32();

			//if won the random selection, then push it on the stack
			if(r < top.second)
			{
				priorityQueue.pop();
				priorityQueue.emplace(val, r);

				//return new top of stack
				return priorityQueue.top().first;
			}

			//current top of stack won, return current top
		}
		//otherwise don't need to do anything, val is not better than the worst on the stack

		return top.first;
	}

	//like PushAndPop, except will keep all values up to the specified threshold
	__forceinline const QueueElementType PushAndPopToThreshold(const QueueElementType &val)
	{
		if(val > includeAllThreshold)
			return PushAndPop(val);

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

	__forceinline void Pop()
	{
		priorityQueue.pop();
	}

	__forceinline bool Empty()
	{
		return priorityQueue.empty();
	}

protected:

	//used to compare first by the value, second by the random number if equal
	class StochasticTieBreakingComparator
	{
	public:
		constexpr bool operator()(const std::pair<QueueElementType, uint32_t> &a, const std::pair<QueueElementType, uint32_t> &b)
		{
			if(a.first == b.first)
				return a.second < b.second;
			return a.first < b.first;
		}
	};

	FlexiblePriorityQueue<std::pair<QueueElementType, uint32_t>, PriorityQueueContainerType, StochasticTieBreakingComparator> priorityQueue;

	//threshold below which all elements should be kept by PushAndPopToThreshold
	ComparisonValueType includeAllThreshold;

	RandomStream randomStream;
};
