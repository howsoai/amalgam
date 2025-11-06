#pragma once

//system headers:
#include <istream>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>

namespace FileSupportCAML
{
	//read the header from the stream
	//if success: returns an empty string indicating no error, file version, and true
	//if failure: returns error message, file version, and false
	std::tuple<std::string, std::string, bool> ReadHeader(std::istream &stream, size_t &header_size);

	//write the header to the stream
	bool WriteHeader(std::ostream &stream);
};