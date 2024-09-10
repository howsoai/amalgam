//project headers:
#include "RandomStream.h"

#include "murmurhash3/MurmurHash3.h"

//system headers:
#include <algorithm>
#include <cstring>

#define RANDOM_STATE_SIZE (sizeof(int64_t) * 2 + 1)

RandomStream::RandomStream(const std::string initial_state)
{
	increment = 0;
	state = 0;
	SetState(initial_state);
}

std::string RandomStream::GetState()
{
	char s[RANDOM_STATE_SIZE];
	s[0] = static_cast<char>(255 & (state >> 56));
	s[1] = static_cast<char>(255 & (state >> 48));
	s[2] = static_cast<char>(255 & (state >> 40));
	s[3] = static_cast<char>(255 & (state >> 32));
	s[4] = static_cast<char>(255 & (state >> 24));
	s[5] = static_cast<char>(255 & (state >> 16));
	s[6] = static_cast<char>(255 & (state >>  8));
	s[7] = static_cast<char>(255 & (state >>  0));

	s[ 8] = static_cast<char>(255 & (increment >> 56));
	s[ 9] = static_cast<char>(255 & (increment >> 48));
	s[10] = static_cast<char>(255 & (increment >> 40));
	s[11] = static_cast<char>(255 & (increment >> 32));
	s[12] = static_cast<char>(255 & (increment >> 24));
	s[13] = static_cast<char>(255 & (increment >> 16));
	s[14] = static_cast<char>(255 & (increment >> 8));
	s[15] = static_cast<char>(255 & (increment >> 0));

	//use an in-band way of indicating whether the seed has been validated
	// the worst that will happen is the random number generator will yield two zeros in a row
	//so in the 1 in ~4 billion chance that the last part of the state is all 1's, it will yield that
	// anomalous set of random numbers
	//this class will always ensure that the state has been initialized
	s[16] = static_cast<uint8_t>(0xFF);

	return std::string(&s[0], RANDOM_STATE_SIZE);
}

void RandomStream::SetState(const std::string &new_state)
{
	uint8_t s[RANDOM_STATE_SIZE];
	std::memset(&s[0], 0, RANDOM_STATE_SIZE);
	std::memcpy(&s[0], new_state.c_str(), std::min(new_state.size(), RANDOM_STATE_SIZE));

	state =	  (static_cast<uint64_t>(s[0]) << 56) | (static_cast<uint64_t>(s[1]) << 48)
			| (static_cast<uint64_t>(s[2]) << 40) | (static_cast<uint64_t>(s[3]) << 32)
			| (static_cast<uint64_t>(s[4]) << 24) | (static_cast<uint64_t>(s[5]) << 16)
			| (static_cast<uint64_t>(s[6]) <<  8) | static_cast<uint64_t>(s[7]);

	increment =	  (static_cast<uint64_t>(s[8 + 0]) << 56) | (static_cast<uint64_t>(s[8 + 1]) << 48)
				| (static_cast<uint64_t>(s[8 + 2]) << 40) | (static_cast<uint64_t>(s[8 + 3]) << 32)
				| (static_cast<uint64_t>(s[8 + 4]) << 24) | (static_cast<uint64_t>(s[8 + 5]) << 16)
				| (static_cast<uint64_t>(s[8 + 6]) <<  8) | static_cast<uint64_t>(s[8 + 7]);

	//if the state hasn't been declared as initialized, burn through exactly two random numbers to
	// prevent make sure it is in a good state based on the paper cited in this class
	if(s[16] != 0xFF)
		BurnIn();
}

std::string RandomStream::CreateOtherStreamStateViaString(const std::string &seed_string)
{
	char s[RANDOM_STATE_SIZE];
	std::memset(&s[0], 0, RANDOM_STATE_SIZE);
	MurmurHash3_x64_128(seed_string.c_str(), static_cast<int>(seed_string.size()), static_cast<uint32_t>(state & 0xFFFFFFFF), &s[0]);

	//randomize the hash based on the current random state
	*(reinterpret_cast<uint64_t *>(&s[0])) ^= state;
	*(reinterpret_cast<uint64_t *>(&s[sizeof(uint64_t)])) ^= increment;

	return std::string(&s[0], RANDOM_STATE_SIZE);
}

RandomStream RandomStream::CreateOtherStreamViaString(const std::string &seed_string)
{
	RandomStream new_stream;

	char s[RANDOM_STATE_SIZE];
	std::memset(&s[0], 0, RANDOM_STATE_SIZE);
	MurmurHash3_x64_128(seed_string.c_str(), static_cast<int>(seed_string.size()), static_cast<uint32_t>(state & 0xFFFFFFFF), &s[0]);

	//randomize the hash based on the current random state
	new_stream.state = ( *(reinterpret_cast<uint64_t *>(&s[0])) ^ state );
	new_stream.increment = ( *(reinterpret_cast<uint64_t *>(&s[sizeof(uint64_t)])) ^ increment );

	new_stream.BurnIn();

	return new_stream;
}

RandomStream RandomStream::CreateOtherStreamViaRand()
{
	RandomStream new_stream;
	new_stream.state = ((static_cast<uint64_t>(RandUInt32()) << 32) | RandUInt32());
	new_stream.increment = ((static_cast<uint64_t>(RandUInt32()) << 32) | RandUInt32());
	new_stream.BurnIn();

	return new_stream;
}

uint32_t RandomStream::RandUInt32()
{
	//perform PCG random number generation
	//based on this: www.pcg-random.org/download.html
	uint64_t multiplier_64 = 6364136223846793005ULL;
	uint64_t old_value = state;
	state = old_value * multiplier_64 + (increment | 1);

	//DXSM permutation: double xor shift multiply
	uint32_t hi = static_cast<uint32_t>(state >> 32);
	uint32_t lo = static_cast<uint32_t>(state | 1);
	hi ^= hi >> 16;
	uint32_t multiplier_32 = 747796405U;
	hi *= multiplier_32;
	hi ^= hi >> 24;
	hi *= lo;
	return hi;
}
