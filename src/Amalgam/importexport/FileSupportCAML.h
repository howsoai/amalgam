#pragma once

//system headers:
#include <fstream>
#include <ostream>
#include <string>
#include <utility>

namespace FileSupportCAML
{
	//read the header from the stream
	//if successfully: returns an empty string indicating no error, file version, and true
	//if failure: returns error message, file version, and false
	std::tuple<std::string, std::string, bool> ReadHeader(std::ifstream &stream, size_t &header_size);

	//write the header to the stream
	bool WriteHeader(std::ofstream &stream);
};