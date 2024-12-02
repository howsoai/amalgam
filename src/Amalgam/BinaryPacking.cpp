//project headers:
#include "BinaryPacking.h"

//system headers:
#include <array>
#include <queue>

void UnparseIndexToCompactIndexAndAppend(BinaryData &bd_out, size_t oi)
{
	//start by stripping off the data of the least significant 7 bits
	uint8_t cur_byte = (oi & 0x7F);
	oi >>= 7;

	//as long as there are more bits in the index
	while(oi != 0)
	{
		//mark with most significant bit
		cur_byte |= 0x80;
		bd_out.push_back(cur_byte);

		//take off another 7 bits
		cur_byte = (oi & 0x7F);
		oi >>= 7;
	}
	bd_out.push_back(cur_byte);
}

size_t ParseCompactIndexToIndexAndAdvance(BinaryData &bd, size_t &bd_offset)
{
	size_t index = 0;
	for(int i = 0; bd_offset < bd.size(); i++, bd_offset++)
	{
		uint8_t cur_byte = bd[bd_offset];

		//if the most significant bit is set, then roll this on to the existing index
		bool last_byte = true;
		if(cur_byte & 0x80)
		{
			last_byte = false;
			cur_byte &= 0x7F;
		}

		//put the 7 bits onto the index
		index |= (static_cast<size_t>(cur_byte) << (7 * i));

		if(last_byte)
		{
			//advance for last byte
			bd_offset++;
			break;
		}
	}

	return index;
}

StringCodec::StringCodec(std::array<uint8_t, NUM_UINT8_VALUES> &byte_frequencies)
{
	//build the huffman_tree based on the byte frequencies
	huffmanTree = HuffmanTree<uint8_t>::BuildTreeFromValueFrequencies(byte_frequencies);
}

StringCodec::~StringCodec()
{
	if(huffmanTree != nullptr)
		delete huffmanTree;
}

BinaryData StringCodec::EncodeString(std::string &uncompressed_data)
{
	//build lookup table from huffman_tree

	//the code for each possibly representable value
	// for example, if 44 is the boolean vector 10, then it will have 10 at the 44th index
	//all valueCodes are initialized to an empty boolean array
	std::array<std::vector<bool>, NUM_UINT8_VALUES> valueCodes;

	//keep a double-ended queue to traverse the tree, building up the codes for each part of the tree
	std::deque<std::pair<HuffmanTree<uint8_t> *, std::vector<bool>>> remaining_nodes;
	remaining_nodes.emplace_back(huffmanTree, std::vector<bool>());

	//while more tree to convert
	while(!remaining_nodes.empty())
	{
		auto node = remaining_nodes.front().first;
		//explicitly make a copy to make sure there's not a reference being kept
		std::vector<bool> code(remaining_nodes.front().second);
		remaining_nodes.pop_front();

		auto left = node->left;
		auto right = node->right;

		//if not leaf node (a Huffman Tree node is either full or not)
		if(left != nullptr)
		{
			//make another copy of the code for the other child node
			std::vector<bool> code_copy(code);

			//append a 0 for left, 1 for right
			code.push_back(0);
			remaining_nodes.emplace_back(left, code);
			code_copy.push_back(1);
			remaining_nodes.emplace_back(right, code_copy);
		}
		else //leaf node
		{
			valueCodes[node->value] = code;
		}
	}

	//encode the data and store in compressed_data
	BinaryData compressed_data;
	//reserve some, probably not enough, but enough to get started
	compressed_data.reserve(1 + uncompressed_data.size() / 4);

	//the first byte stores the number of extra bits in the last byte, so skip it for encoding
	size_t ending_bit = 8;
	size_t cur_byte = 1;
	size_t cur_bit = 0;

	for(uint8_t c : uncompressed_data)
	{
		auto &value = valueCodes[c];
		size_t num_bits_to_add = value.size();

		//make sure there are enough bytes to hold everything
		// if one extra bit, then need a full extra byte, so add 7 bits to round up
		ending_bit += num_bits_to_add;
		compressed_data.resize((ending_bit + 7) / 8);

		for(auto bit : value)
		{
			//compressed_data is already initialized to zeros, so only need to set if true
			if(bit)
				compressed_data[cur_byte] |= (1 << cur_bit);

			cur_bit++;
			if(cur_bit == 8)
			{
				cur_bit = 0;
				cur_byte++;
			}
		}
	}

	//store number of extra bits in first byte
	compressed_data[0] = (ending_bit % 8);

	return compressed_data;
}

std::string StringCodec::DecodeString(BinaryData &compressed_data)
{
	//need at least one byte to represent the number of extra bits and another byte of actual value
	if(compressed_data.size() < 2)
		return std::string();

	//count out all the potentially available bits
	size_t end_bit = 8 * compressed_data.size();

	//number of extra bits is stored in the first byte
	if(compressed_data[0] != 0)
	{
		//if there is any number besides 0, then we need to remove 8 bits and add on whatever remains
		end_bit -= 8;
		end_bit += compressed_data[0];
	}
	//skip the first byte
	size_t start_bit = 8;

	//decompress the data
	std::string uncompressed_data;
	while(start_bit < end_bit)
		uncompressed_data.push_back(huffmanTree->LookUpCode(compressed_data, start_bit, end_bit));

	return uncompressed_data;
}

//counts the number of bytes within bd for each value
//returns an array where each index represents each of the possible NUM_INT8_VALUES values, and the value at each is the number found
static std::array<uint8_t, StringCodec::NUM_UINT8_VALUES> GetByteFrequencies(std::string &str)
{
	std::array<size_t, StringCodec::NUM_UINT8_VALUES> value_counts{};	//initialize to zero with value-initialization {}'s
	for(uint8_t b : str)
		value_counts[b]++;

	//get maximal count for any value
	size_t max_count = 0;
	for(auto v : value_counts)
		max_count = std::max(max_count, v);

	std::array<uint8_t, StringCodec::NUM_UINT8_VALUES> normalized_value_counts{};	//initialize to zero with value-initialization {}'s
	for(size_t i = 0; i < StringCodec::NUM_UINT8_VALUES; i++)
	{
		if(value_counts[i] == 0)
			continue;
		normalized_value_counts[i] = std::max(static_cast<uint8_t>(255 * value_counts[i] / max_count), static_cast<uint8_t>(1));	//make sure it has at least a value of 1 to be represented
	}

	return normalized_value_counts;
}

BinaryData CompressString(std::string &string_to_compress)
{
	BinaryData encoded_string_library;
	encoded_string_library.reserve(2 * StringCodec::NUM_UINT8_VALUES);	//reserve enough to two entries for every value in the worst case; this will be expanded later

	//create and store the frequency table for each possible byte value
	auto byte_frequencies = GetByteFrequencies(string_to_compress);
	for(size_t i = 0; i < StringCodec::NUM_UINT8_VALUES; i++)
	{
		//write value
		encoded_string_library.push_back(byte_frequencies[i]);

		//if zero, then run-length encoding compress
		if(byte_frequencies[i] == 0)
		{
			//count the number of additional zeros until next nonzero
			uint8_t num_additional_zeros = 0;
			while(i + 1 < StringCodec::NUM_UINT8_VALUES && byte_frequencies[i + 1] == 0)
			{
				num_additional_zeros++;
				i++;
			}
			encoded_string_library.push_back(num_additional_zeros);
			//next loop iteration will increment i and count the first zero
			continue;
		}
	}

	//compress string
	StringCodec codec(byte_frequencies);
	BinaryData encoded_strings = codec.EncodeString(string_to_compress);

	//write out compressed string
	UnparseIndexToCompactIndexAndAppend(encoded_string_library, encoded_strings.size());
	encoded_string_library.resize(encoded_string_library.size() + encoded_strings.size());
	std::copy(begin(encoded_strings), end(encoded_strings), end(encoded_string_library) - encoded_strings.size());

	return encoded_string_library;
}

std::string DecompressString(BinaryData &encoded_string_library)
{
	std::string decompressed_string;
	size_t cur_offset = 0;

	//read the frequency table for each possible byte value
	std::array<uint8_t, StringCodec::NUM_UINT8_VALUES> byte_frequencies{};	//initialize to zeros
	for(size_t i = 0; i < StringCodec::NUM_UINT8_VALUES && cur_offset < encoded_string_library.size(); i++)
	{
		byte_frequencies[i] = encoded_string_library[cur_offset++];

		//if 0, then run-length encoded
		if(byte_frequencies[i] == 0)
		{
			//fill in that many zeros, but don't write beyond buffer
			for(uint8_t num_additional_zeros = encoded_string_library[cur_offset++]; num_additional_zeros > 0 && i < StringCodec::NUM_UINT8_VALUES; num_additional_zeros--, i++)
				byte_frequencies[i] = 0;
		}
	}

	//decompress and concatenate all compressed blocks
	while(cur_offset < encoded_string_library.size())
	{
		//read encoded string
		size_t encoded_strings_size = ParseCompactIndexToIndexAndAdvance(encoded_string_library, cur_offset);
		//check if size past end of buffer
		if(cur_offset + encoded_strings_size > encoded_string_library.size())
			return decompressed_string;
		BinaryData encoded_strings(begin(encoded_string_library) + cur_offset, begin(encoded_string_library) + cur_offset + encoded_strings_size);
		cur_offset += encoded_strings_size;

		//decode compressed string buffer
		StringCodec ssc(byte_frequencies);
		std::string cur_decoded = ssc.DecodeString(encoded_strings);
		decompressed_string += cur_decoded;
	}
	
	return decompressed_string;
}
