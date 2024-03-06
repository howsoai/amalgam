//project headers:
#include "FileSupportCAML.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"

//system headers:
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>

//magic number written at beginning of CAML file
static const uint8_t s_magic_number[] = { 'c', 'a', 'm', 'l' };

bool ReadBigEndian(std::ifstream &stream, uint32_t &val)
{
	uint8_t buffer[4] = { 0 };
	if(!stream.read(reinterpret_cast<char *>(buffer), sizeof(uint32_t)))
		return false;

	auto num_bytes_read = stream.gcount();
	if(num_bytes_read != sizeof(uint32_t))
		return false;

	val = static_cast<uint32_t>((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);

	return true;
}

bool WriteBigEndian(std::ofstream &stream, const uint32_t &val)
{
	uint8_t buffer[4] = { 0 };
	buffer[0] = (val >> 24) & 0xFF;
	buffer[1] = (val >> 16) & 0xFF;
	buffer[2] = (val >> 8) & 0xFF;
	buffer[3] = val & 0xFF;
	stream.write(reinterpret_cast<char *>(buffer), sizeof(buffer));

	return true;
}

bool ReadVersion(std::ifstream &stream, uint32_t &major, uint32_t &minor, uint32_t &patch)
{
	if(!ReadBigEndian(stream, major))
		return false;
	if(!ReadBigEndian(stream, minor))
		return false;
	if(!ReadBigEndian(stream, patch))
		return false;

	return true;
}

bool WriteVersion(std::ofstream &stream)
{
	if(!WriteBigEndian(stream, AMALGAM_VERSION_MAJOR))
		return false;
	if(!WriteBigEndian(stream, AMALGAM_VERSION_MINOR))
		return false;
	if(!WriteBigEndian(stream, AMALGAM_VERSION_PATCH))
		return false;

	return true;
}

std::tuple<std::string, std::string, bool> FileSupportCAML::ReadHeader(std::ifstream &stream, size_t &header_size)
{
	uint8_t magic[4] = { 0 };
	if(!stream.read(reinterpret_cast<char *>(magic), sizeof(magic)))
		return std::make_tuple("Cannot read CAML header", "", false);
	header_size += sizeof(magic);

	auto num_bytes_read = stream.gcount();
	if(num_bytes_read != sizeof(magic))
		return std::make_tuple("Cannot read CAML header", "", false);
	else if(memcmp(magic, s_magic_number, sizeof(magic)) == 0)
	{
		uint32_t major = 0, minor = 0, patch = 0;
		if(!ReadVersion(stream, major, minor, patch))
			return std::make_tuple("Cannot read CAML version", "", false);
		header_size += sizeof(major) * 3;
		std::string version = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);

		//validate version
		auto [error_message, success] = AssetManager::ValidateVersionAgainstAmalgam(version);
		if(!success)
			return std::make_tuple(error_message, version, false);
	}
	else
	{
		return std::make_tuple("CAML does not contain a valid header", "", false);
	}

	return std::make_tuple("", "", true);
}

bool FileSupportCAML::WriteHeader(std::ofstream &stream)
{
	if(!stream.write(reinterpret_cast<const char *>(s_magic_number), sizeof(s_magic_number)))
		return false;

	return WriteVersion(stream);
}