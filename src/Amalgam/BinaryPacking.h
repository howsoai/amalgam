#pragma once

//system headers:
#include <limits>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

typedef std::vector<uint8_t> BinaryData;

//Huffman Encoding implementation for compressing and decompressing data
template<typename value_type>
class HuffmanTree
{
public:
	constexpr HuffmanTree(value_type value, size_t value_frequency, size_t node_index,
		HuffmanTree<value_type> *left = nullptr, HuffmanTree<value_type> *right = nullptr)
		: value(value), valueFrequency(value_frequency), nodeIndex(node_index), left(left), right(right)
	{
	}

	inline ~HuffmanTree()
	{
		delete left;
		delete right;
	}

	//number of bits per value based on the number of bytes long value_type is
	static constexpr int bitsPerValue = 8 * sizeof(value_type);

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

//compresses string_to_compress into BinaryData and returns the huffman tree to encode it
//caller is responsible for deleting the huffman tree
std::pair<BinaryData, HuffmanTree<uint8_t> *> CompressString(std::string &string_to_compress);

//like CompressString, but uses a huffman_tree to generate a string that can be appended to a previous compressed string
BinaryData CompressStringToAppend(std::string &string_to_compress, HuffmanTree<uint8_t> *huffman_tree);

//given encoded_string returns the decompressed, decoded string
std::string DecompressString(BinaryData &encoded_string);
