#pragma once

//system headers:
#include <iostream>
#include <string>

//TODO:18866 - should this just be a namespace a free functions? Probably...
class FileSupportCAML
{
public:

	//validate CAML header
	static bool IsValidCAMLHeader(const std::string &filepath);

	//read the CAML header from the stream
	static bool ReadHeader(std::ifstream &file, size_t &header_size);

	//write the CAML header to the stream
	static bool WriteHeader(std::ofstream &file);
};