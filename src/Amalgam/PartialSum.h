//project headers:

//system headers:
#include <vector>

//Class to store, accumulate, and merge/complete summations efficiently
class PartialSumCollection
{
public:
	//union of the types of data stored to reduce need for reinterpret_cast
	union SumOrMaskBucket
	{
		uint64_t mask;
		double sum;
	};

	PartialSumCollection()
	{
		numTerms = 0;
		numInstances = 0;
		numMaskBuckets = 1;
	}

	//defined to keep compatibility with stl containers
	using value_type = size_t;

	//iterator for walking along which partial sums have been filled in
	struct Iterator
	{
		__forceinline Iterator(size_t _index, size_t _bit, SumOrMaskBucket *value_location)
			: index(_index), valueLocation(value_location)
		{	}

		__forceinline Iterator operator =(const Iterator &other)
		{
			index = other.index;
			valueLocation = other.valueLocation;
			return *this;
		}

		__forceinline bool operator ==(const Iterator &other)
		{
			return index == other.index;
		}

		__forceinline bool operator !=(const Iterator &other)
		{
			return index != other.index;
		}

		__forceinline Iterator &operator ++()
		{
			index++;
			return *this;
		}

		//dereference operator
		__forceinline size_t operator *()
		{
			return index;
		}

		//returns true if current bit is set
		__forceinline bool IsIndexComputed()
		{
			size_t bit = (index % 64);
			size_t offset = (index / 64);
			return ( (valueLocation + offset)->mask & (1ULL << bit));
		}

		size_t index;

		//pointer to current value
		SumOrMaskBucket *valueLocation;
	};

	//clears all data in the collection
	void clear()
	{
		for(auto &v : buffer)
			v.mask = 0;
		numTerms = 0;
		numInstances = 0;
		numMaskBuckets = 1;
	}

	//resizes the buffer to accommodate the dimensions and instances specified and clears all data
	void ResizeAndClear(size_t num_dimensions, size_t num_instances)
	{
		numTerms = num_dimensions;
		numInstances = num_instances;
		//need a SumOrFeatureMask for each of up to 64 dimensions
		numMaskBuckets = ((num_dimensions + 63) / 64);

		bucketStride = numMaskBuckets + 1;

		//need one value for the sum and enough values to hold a bit per dimension
		//round up number of dimensions used 
		//unions are automatically defaulted to zero for all of their attributes
		buffer.clear();
		buffer.resize(bucketStride * num_instances);
	}

	//finds the bucket's bit for the specified index
	static __forceinline size_t GetBucketBitForIndex(size_t index)
	{
		return 1ULL << (index % 64);
	}

	//finds the bucket that contains the index
	static __forceinline size_t GetBucketForIndex(size_t index)
	{
		return index / 64 + 1;
	}

	//returns the bucket and bit for the specified dimension
	static __forceinline std::pair<size_t, size_t> GetAccumLocation(size_t dimension_index)
	{
		return std::make_pair(GetBucketForIndex(dimension_index), GetBucketBitForIndex(dimension_index));
	}

	//accumulates the specified value into the value specified by partial_sum_index
	// for the accum_location provided by GetAccumLocation
	__forceinline void Accum(size_t partial_sum_index, const std::pair<size_t, size_t> accum_location, double value)
	{
		size_t bucket_offset = bucketStride * partial_sum_index;
		buffer[bucket_offset].sum += value;
		buffer[bucket_offset + accum_location.first].mask |= accum_location.second;
	}

	//accumulates the value of zero into the value specified by partial_sum_index
	// for the accum_location provided by GetAccumLocation
	//just like Accum, but faster if the value is zero
	__forceinline void AccumZero(size_t partial_sum_index, const std::pair<size_t, size_t> accum_location)
	{
		size_t bucket_offset = bucketStride * partial_sum_index;
		buffer[bucket_offset + accum_location.first].mask |= accum_location.second;
	}

	//gets the number of populated buckets of the sum of index partial_sum_index
	__forceinline size_t GetNumFilled(size_t partial_sum_index)
	{
		size_t start_offset = bucketStride * partial_sum_index + 1;
		size_t end_offset = start_offset + numMaskBuckets;

		size_t num_set = 0;
		for(size_t offset = start_offset; offset < end_offset; offset++)
			num_set += __popcnt64(buffer[offset].mask);
		return num_set;
	}

	//gets the sum for the specified partial_sum_index
	__forceinline double GetSum(size_t partial_sum_index)
	{
		size_t bucket_offset = bucketStride * partial_sum_index;
		return buffer[bucket_offset].sum;
	}

	//performs both GetNumFilled and GetSum in one call
	__forceinline std::pair<size_t, double> GetNumFilledAndSum(size_t partial_sum_index)
	{
		size_t bucket_offset = bucketStride * partial_sum_index;
		double sum = buffer[bucket_offset].sum;

		size_t start_offset = bucket_offset + 1;
		size_t end_offset = start_offset + numMaskBuckets;

		size_t num_filled = 0;
		for(size_t offset = start_offset; offset < end_offset; offset++)
			num_filled += __popcnt64(buffer[offset].mask);

		return std::make_pair(num_filled, sum);
	}

	//sets the sum to the specified value
	__forceinline void SetSum(size_t partial_sum_index, double value)
	{
		size_t bucket_offset = bucketStride * partial_sum_index;
		buffer[bucket_offset].sum = value;
	}

	//returns an iterator for partial_sum_index
	__forceinline Iterator BeginPartialSumIndex(size_t partial_sum_index)
	{
		size_t offset = bucketStride * partial_sum_index + 1;
		return Iterator(0, 0, &buffer[offset]);
	}

	//returns true if the term of the sum at partial_sum_index and term_index has been accumulated yet, else false
	__forceinline bool IsIndexComputed(size_t partial_sum_index, size_t term_index)
	{
		size_t bucket = GetBucketForIndex(term_index);
		size_t mask = GetBucketBitForIndex(term_index);
		size_t offset = bucketStride * partial_sum_index + bucket;

		return buffer[offset].mask & mask;
	}

	///////////////////////
	//data storage

	//partial sum data
	//stored interleaved as (sum, numTermsCompleted, mask[numTerms])[numInstances]
	std::vector<SumOrMaskBucket> buffer;

	//number of dimensions
	size_t numTerms;

	//number of instances that need partial sums
	size_t numInstances;

	//a cached value computed based on numTerms
	// representing the length of each partial sum data block, excluding the sum
	// making the stride length numBuckets + 1
	size_t numMaskBuckets;

	//equal to numMaskBuckets + 1, accounting for the sum
	// cached purely for performance reasons
	size_t bucketStride;
};
