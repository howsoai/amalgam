#pragma once

//system headers:
#include <iostream>
#include <string>

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