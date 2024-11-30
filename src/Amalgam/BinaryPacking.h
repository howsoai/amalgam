#pragma once

//project headers:
#include "HashMaps.h"

//system headers:
#include <string>
#include <vector>

typedef uint64_t OffsetIndex;

typedef std::vector<uint8_t> BinaryData;

//Appends the offset index oi to BinaryData
void UnparseIndexToCompactIndexAndAppend(BinaryData &bd_out, OffsetIndex oi);

//Parses the BinaryData starting from the offset bd_offset until it has a full index or has reached the end of the binary data.  bd_offset is advanced to the end of the 
OffsetIndex ParseCompactIndexToIndexAndAdvance(BinaryData &bd, OffsetIndex &bd_offset);

//given string_map, map of string to index, where the indices are of the range from 0 to string_map.size(), compresses the strings into BinaryData
BinaryData CompressString(std::string &string_to_compress);

//given encoded_string starting an cur_offset, advances cur_offset to the end of the encoded_string and returns a vector of strings decompressed from the encoded_string
std::string DecompressString(BinaryData &encoded_string, OffsetIndex &cur_offset);
