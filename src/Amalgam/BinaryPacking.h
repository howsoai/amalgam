#pragma once

//system headers:
#include <string>
#include <vector>

typedef std::vector<uint8_t> BinaryData;

//Huffman Encoding implementation for compressing and decompressing data
template <typename value_type>
class HuffmanTree
{
public:
	constexpr HuffmanTree(value_type value, size_t value_frequency, size_t node_index,
		HuffmanTree<value_type> *left = nullptr, HuffmanTree<value_type> *right = nullptr)
		: value(value), valueFrequency(value_frequency), nodeIndex(node_index), left(left), right(right)
	{
	}

	~HuffmanTree()
	{
		delete left;
		delete right;
	}

	//number of bits per value based on the number of bytes long value_type is
	static constexpr int bitsPerValue = 8 * sizeof(value_type);

	static HuffmanTree<value_type> *BuildTreeFromValueFrequencies(
		std::array<value_type, std::numeric_limits<value_type>::max() + 1> &byte_frequencies)
	{
		size_t cur_node_index = 0;

		//start by building the leaf nodes
		std::priority_queue<HuffmanTree<value_type> *,
			std::vector<HuffmanTree<value_type> *>, HuffmanTree<value_type>::Compare > alphabet_heap;

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

	//for sorting HuffmanTree nodes by frequency
	class Compare
	{
	public:
		constexpr bool operator()(HuffmanTree<value_type> *a, HuffmanTree<value_type> *b)
		{
			//if valueFrequency is the same for both values, break tie by the value itself
			// to ensure consistent ordering across platforms and heap implementations
			if(a->valueFrequency == b->valueFrequency)
			{
				//if values are equal, break ties by node index
				if(a->value == b->value)
					return a->nodeIndex > b->nodeIndex;

				return a->value > b->value;
			}
			return a->valueFrequency > b->valueFrequency;
		}
	};

	//looks up the next value in the tree based from the bit string in bd from start_index up until end_index
	//increments start_index based on the length of the code consumed
	inline value_type LookUpCode(BinaryData &bd, size_t &start_index, size_t end_index)
	{
		auto node = this;

		size_t cur_byte = (start_index / bitsPerValue);
		size_t cur_bit = (start_index % bitsPerValue);

		while(start_index < end_index)
		{
			//if leaf node, then return value
			if(node->left == nullptr)
				return node->value;

			if(bd[cur_byte] & (1 << cur_bit))
				node = node->right;
			else
				node = node->left;

			start_index++;
			cur_bit++;
			if(cur_bit == bitsPerValue)
			{
				cur_bit = 0;
				cur_byte++;
			}
		}

		//if leaf node, then return value; need this again incase used up last bits
		if(node->left == nullptr)
			return node->value;

		//shouldn't make it here -- ran out of bits
		return 0;
	}

	//the value of this node in the HuffmanTree and its frequency
	value_type value;
	size_t valueFrequency;

	//node index used for breaking ties on value and valueFrequency to ensure
	// that Huffman trees are always generated identically regardless of priority queue implementation
	size_t nodeIndex;

	//rest of the tree
	HuffmanTree<value_type> *left;
	HuffmanTree<value_type> *right;
};

//class to compress and decompress bundles of strings
class StringCodec
{
public:

	const static size_t NUM_UINT8_VALUES = std::numeric_limits<uint8_t>::max() + 1;

	StringCodec(std::array<uint8_t, NUM_UINT8_VALUES> &byte_frequencies);

	~StringCodec();

	BinaryData EncodeString(std::string &uncompressed_data);

	std::string DecodeString(BinaryData &compressed_data);

	//Huffman tree to build and store between calls
	HuffmanTree<uint8_t> *huffmanTree;
};

//given string_map, map of string to index, where the indices are of the range from 0 to string_map.size(), compresses the strings into BinaryData
BinaryData CompressString(std::string &string_to_compress);

//given encoded_string returns the decompressed, decoded string
std::string DecompressString(BinaryData &encoded_string);
