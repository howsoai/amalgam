//project headers:
#include "FileSupportCAML.h"

#include "AmalgamVersion.h"

//system headers:
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

static bool ReadBigEndianUInt32(std::ifstream &file, uint32_t &val)
{
	uint8_t buffer[4] = { 0 };
	if(!file.read(reinterpret_cast<char *>(buffer), sizeof(uint32_t)))
		return false;

	auto num_bytes_read = file.gcount();
	if(num_bytes_read != sizeof(uint32_t))
		return false;

	val = static_cast<uint32_t>((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);

	return true;
}

static bool WriteBigEndianUInt32(std::ofstream &file, const uint32_t &val)
{
	uint8_t buffer[4] = { 0 };
	buffer[0] = (val >> 24) & 0xFF;
	buffer[1] = (val >> 16) & 0xFF;
	buffer[2] = (val >> 8) & 0xFF;
	buffer[3] = val & 0xFF;
	file.write(reinterpret_cast<char *>(buffer), sizeof(buffer));

	return true;
}

static bool ReadVersion(std::ifstream &file, uint32_t &major, uint32_t &minor, uint32_t &patch)
{
	if(!ReadBigEndianUInt32(file, major))
		return false;
	if(!ReadBigEndianUInt32(file, minor))
		return false;
	if(!ReadBigEndianUInt32(file, patch))
		return false;

	return true;
}

static bool WriteVersion(std::ofstream &file)
{
	if(!WriteBigEndianUInt32(file, s_current_major))
		return false;
	if(!WriteBigEndianUInt32(file, s_current_minor))
		return false;
	if(!WriteBigEndianUInt32(file, s_current_patch))
		return false;

	return true;
}

bool FileSupportCAML::IsValidCAMLHeader(const std::string &filepath)
{
	std::ifstream f(filepath, std::fstream::binary | std::fstream::in);

	if(!f.good())
		return false;

	size_t header_size = 0;
	return ReadHeader(f, header_size);
}

bool FileSupportCAML::ReadHeader(std::ifstream &file, size_t &header_size)
{
	uint8_t magic[4] = { 0 };
	if(!file.read(reinterpret_cast<char *>(magic), sizeof(magic)))
		return false;
	header_size += sizeof(magic);

	auto num_bytes_read = file.gcount();
	if(num_bytes_read != sizeof(magic))
		return false;
	else if(memcmp(magic, s_magic_number, sizeof(magic)) == 0)
	{
		uint32_t major = 0, minor = 0, patch = 0;
		if(!ReadVersion(file, major, minor, patch))
			return false;
		header_size += sizeof(major) * 3;

		//validate version
		std::string dev_build = AMALGAM_VERSION_SUFFIX;
		if(!dev_build.empty())
			return true; //dev build
		else if(
			(major > s_current_major) ||
			(major == s_current_major && minor > s_current_minor) ||
			(major == s_current_major && minor == s_current_minor && patch > s_current_patch))
		{
			return false; //newer version
		}
	}

	return true;
}

bool FileSupportCAML::WriteHeader(std::ofstream &file)
{
	if(!file.write(reinterpret_cast<const char *>(s_magic_number), sizeof(s_magic_number)))
		return false;

	return WriteVersion(file);
}