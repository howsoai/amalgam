//project headers:
#include "BinaryPacking.h"

//system headers:
#include <array>
#include <queue>

const static size_t NUM_UINT8_VALUES = std::numeric_limits<uint8_t>::max() + 1;

template<typename value_type>
HuffmanTree<value_type> *BuildTreeFromValueFrequencies(
	std::array<value_type, std::numeric_limits<value_type>::max() + 1> &byte_frequencies)
{
	size_t cur_node_index = 0;

	//start by building the leaf nodes
	std::priority_queue<HuffmanTree<value_type> *,
		std::vector<HuffmanTree<value_type> *>, typename HuffmanTree<value_type>::Compare > alphabet_heap;

	//create all the leaf nodes and add them to the priority queue
	for(size_t i = 0; i < byte_frequencies.size(); i++)
	{
		auto leaf = new HuffmanTree<value_type>(static_cast<value_type>(i), byte_frequencies[i], cur_node_index++);
		alphabet_heap.push(leaf);
	}

	//Merge leaf nodes with lowest values until have just one at the top
	HuffmanTree<value_type> *huffman_tree = nullptr;
	while(alphabet_heap.size() > 1)
	{
		auto left = alphabet_heap.top();
		alphabet_heap.pop();
		auto right = alphabet_heap.top();
		alphabet_heap.pop();

		//since non-leaf nodes aren't used for encoding, just use the value 0
		huffman_tree = new HuffmanTree<value_type>(0, left->valueFrequency + right->valueFrequency, cur_node_index++, left, right);
		alphabet_heap.push(huffman_tree);
	}

	return huffman_tree;
}

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

BinaryData EncodeStringFromHuffmanTree(std::string &uncompressed_data, HuffmanTree<uint8_t> *huffman_tree)
{
	//build lookup table from huffman_tree

	//the code for each possibly representable value
	// for example, if 44 is the boolean vector 10, then it will have 10 at the 44th index
	//all valueCodes are initialized to an empty boolean array
	std::array<std::vector<bool>, NUM_UINT8_VALUES> valueCodes;

	//keep a double-ended queue to traverse the tree, building up the codes for each part of the tree
	std::deque<std::pair<HuffmanTree<uint8_t> *, std::vector<bool>>> remaining_nodes;
	remaining_nodes.emplace_back(huffman_tree, std::vector<bool>());

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

std::string DecodeStringFromHuffmanTree(BinaryData &compressed_data, HuffmanTree<uint8_t> *huffman_tree)
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
		uncompressed_data.push_back(huffman_tree->LookUpCode(compressed_data, start_bit, end_bit));

	return uncompressed_data;
}

//counts the number of bytes within bd for each value
//returns an array where each index represents each of the possible NUM_INT8_VALUES values, and the value at each is the number found
static std::array<uint8_t, NUM_UINT8_VALUES> GetByteFrequencies(std::string &str)
{
	std::array<size_t, NUM_UINT8_VALUES> value_counts{};	//initialize to zero with value-initialization {}'s
	for(uint8_t b : str)
		value_counts[b]++;

	//get maximal count for any value
	size_t max_count = 0;
	for(auto v : value_counts)
		max_count = std::max(max_count, v);

	std::array<uint8_t, NUM_UINT8_VALUES> normalized_value_counts{};	//initialize to zero with value-initialization {}'s
	for(size_t i = 0; i < NUM_UINT8_VALUES; i++)
	{
		if(value_counts[i] == 0)
			continue;
		normalized_value_counts[i] = std::max(static_cast<uint8_t>(255 * value_counts[i] / max_count), static_cast<uint8_t>(1));	//make sure it has at least a value of 1 to be represented
	}

	return normalized_value_counts;
}

std::pair<BinaryData, HuffmanTree<uint8_t> *> CompressString(std::string &string_to_compress)
{
	BinaryData encoded_string_with_header;
	encoded_string_with_header.reserve(2 * NUM_UINT8_VALUES);	//reserve enough to two entries for every value in the worst case; this will be expanded later

	//create and store the frequency table for each possible byte value
	auto byte_frequencies = GetByteFrequencies(string_to_compress);
	for(size_t i = 0; i < NUM_UINT8_VALUES; i++)
	{
		//write value
		encoded_string_with_header.push_back(byte_frequencies[i]);

		//if zero, then run-length encoding compress
		if(byte_frequencies[i] == 0)
		{
			//count the number of additional zeros until next nonzero
			uint8_t num_additional_zeros = 0;
			while(i + 1 < NUM_UINT8_VALUES && byte_frequencies[i + 1] == 0)
			{
				num_additional_zeros++;
				i++;
			}
			encoded_string_with_header.push_back(num_additional_zeros);
			//next loop iteration will increment i and count the first zero
			continue;
		}
	}

	//compress string
	HuffmanTree<uint8_t> *huffman_tree = BuildTreeFromValueFrequencies<uint8_t>(byte_frequencies);
	BinaryData encoded_string = EncodeStringFromHuffmanTree(string_to_compress, huffman_tree);

	//write out compressed string
	UnparseIndexToCompactIndexAndAppend(encoded_string_with_header, encoded_string.size());
	encoded_string_with_header.resize(encoded_string_with_header.size() + encoded_string.size());
	std::copy(begin(encoded_string), end(encoded_string), end(encoded_string_with_header) - encoded_string.size());

	return std::make_pair(encoded_string_with_header, huffman_tree);
}

BinaryData CompressStringToAppend(std::string &string_to_compress, HuffmanTree<uint8_t> *huffman_tree)
{
	BinaryData encoded_string = EncodeStringFromHuffmanTree(string_to_compress, huffman_tree);

	BinaryData encoded_string_with_header;
	UnparseIndexToCompactIndexAndAppend(encoded_string_with_header, encoded_string.size());
	encoded_string_with_header.resize(encoded_string_with_header.size() + encoded_string.size());
	std::copy(begin(encoded_string), end(encoded_string), end(encoded_string_with_header) - encoded_string.size());
	return encoded_string_with_header;
}

std::string DecompressString(BinaryData &encoded_string_library)
{
	std::string decompressed_string;
	size_t cur_offset = 0;

	//read the frequency table for each possible byte value
	std::array<uint8_t, NUM_UINT8_VALUES> byte_frequencies{};	//initialize to zeros
	for(size_t i = 0; i < NUM_UINT8_VALUES && cur_offset < encoded_string_library.size(); i++)
	{
		byte_frequencies[i] = encoded_string_library[cur_offset++];

		//if 0, then run-length encoded
		if(byte_frequencies[i] == 0)
		{
			//fill in that many zeros, but don't write beyond buffer
			for(uint8_t num_additional_zeros = encoded_string_library[cur_offset++]; num_additional_zeros > 0 && i < NUM_UINT8_VALUES; num_additional_zeros--, i++)
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
		HuffmanTree<uint8_t> *huffman_tree = BuildTreeFromValueFrequencies<uint8_t>(byte_frequencies);
		std::string cur_decoded = DecodeStringFromHuffmanTree(encoded_strings, huffman_tree);
		decompressed_string += cur_decoded;
	}
	
	return decompressed_string;
}
