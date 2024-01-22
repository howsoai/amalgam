//project headers:
#include "FileSupportCAML.h"

#include "AmalgamVersion.h"

//system headers:
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ostream>
#include <string>

//magic number written at beginning of CAML file
static const uint8_t s_magic_number[] = { 'c', 'a', 'm', 'l' };

//current CAML version
static const uint32_t s_current_major = 1;
static const uint32_t s_current_minor = 0;
static const uint32_t s_current_patch = 0;

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
	if(!WriteBigEndian(stream, s_current_major))
		return false;
	if(!WriteBigEndian(stream, s_current_minor))
		return false;
	if(!WriteBigEndian(stream, s_current_patch))
		return false;

	return true;
}

std::pair<std::string, bool> FileSupportCAML::Validate(const std::string &path)
{
	std::ifstream f(path, std::fstream::binary | std::fstream::in);

	if(!f.good())
		return std::make_pair("Cannot open file", false);

	size_t header_size = 0;
	return ReadHeader(f, header_size);
}

std::pair<std::string, bool> FileSupportCAML::ReadHeader(std::ifstream &stream, size_t &header_size)
{
	uint8_t magic[4] = { 0 };
	if(!stream.read(reinterpret_cast<char *>(magic), sizeof(magic)))
		return std::make_pair("Cannot read magic number", false);
	header_size += sizeof(magic);

	auto num_bytes_read = stream.gcount();
	if(num_bytes_read != sizeof(magic))
		return std::make_pair("Cannot read magic number", false);
	else if(memcmp(magic, s_magic_number, sizeof(magic)) == 0)
	{
		uint32_t major = 0, minor = 0, patch = 0;
		if(!ReadVersion(stream, major, minor, patch))
			return std::make_pair("Cannot read version", false);
		header_size += sizeof(major) * 3;

		//validate version
		std::string dev_build = AMALGAM_VERSION_SUFFIX;
		if(!dev_build.empty())
			return std::make_pair("", true); //dev build
		else if(
			(major > s_current_major) ||
			(major == s_current_major && minor > s_current_minor) ||
			(major == s_current_major && minor == s_current_minor && patch > s_current_patch))
		{
			return std::make_pair("Reading newer version not supported", false);
		}
	}

	return std::make_pair("", true);
}

bool FileSupportCAML::WriteHeader(std::ofstream &stream)
{
	if(!stream.write(reinterpret_cast<const char *>(s_magic_number), sizeof(s_magic_number)))
		return false;

	return WriteVersion(stream);
}