#pragma once

//system headers:
#include <string>
#include <vector>

typedef std::vector<uint8_t> BinaryData;

//given string_map, map of string to index, where the indices are of the range from 0 to string_map.size(), compresses the strings into BinaryData
BinaryData CompressString(std::string &string_to_compress);

//given encoded_string returns the decompressed, decoded string
std::string DecompressString(BinaryData &encoded_string);
