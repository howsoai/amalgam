#pragma once

//system headers:
#include <fstream>
#include <ostream>
#include <string>
#include <utility>

namespace FileSupportCAML
{
	//validate file
	std::pair<std::string, bool> Validate(const std::string &path);

	//read the header from the stream
	std::pair<std::string, bool> ReadHeader(std::ifstream &stream, size_t &header_size);

	//write the header to the stream
	bool WriteHeader(std::ofstream &stream);
};