#pragma once

//project headers:
#include "StringManipulation.h"

//system headers:
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

//cross-platform main functions
#define PLATFORM_MAIN_CONSOLE int main(int argc, char* argv[])

#define PLATFORM_ARGS_CONSOLE auto args = Platform_ArgvToStringViews(argc, argv);

#ifdef _WIN32
	#define OS_WINDOWS

	#define NOMINMAX
	#include <Windows.h>

	#define PLATFORM_MAIN_NO_CONSOLE int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow)

	#define PLATFORM_ARGS_NO_CONSOLE			\
		std::string arg_string(lpszCmdLine);	\
		auto args = StringManipulation::SplitArgString(arg_string);

#else
	#ifdef __linux__
		#define OS_LINUX
	#elif defined(__APPLE__) || defined(__MACH__)
		#define OS_MAC
	#endif

	#define PLATFORM_MAIN_NO_CONSOLE PLATFORM_MAIN_CONSOLE

	#define PLATFORM_ARGS_NO_CONSOLE PLATFORM_ARGS_CONSOLE

	//include signal to raise exception in linux
	#include <signal.h>

#endif

//defines __popcnt64 if it doesn't exist
//platform independent intrinsic for bit count on a 64-bit var
#if defined(__GNUC__)
	#define __popcnt64 __builtin_popcountll
#elif !defined(_MSC_VER)
	size_t __popcnt64(uint64_t x)
	{
		size_t bit_count = 0;
		while(x > 0)
		{
			if(x & 1)
				bit_count++;

			x <<= 1;
		}
		return bit_count;
	}
#endif

//returns the offset of the first bit set in x, starting at 0 as the least significant bit
inline size_t Platform_FindFirstBitSet(uint64_t x)
{
#if defined(__GNUC__)
	return __builtin_ctzll(x);
#elif defined(_MSC_VER)
	unsigned long bit;
	_BitScanForward64(&bit, x);
	return bit;
#else
	size_t bit = 0;
	while((x & (1ULL << bit)) == 0)
		bit++;
	return bit;
#endif
}

//returns the offset of the last bit set in x, starting at 63 as the most significant bit
inline size_t Platform_FindLastBitSet(uint64_t x)
{
#if defined(__GNUC__)
	//counts the number of leading zeros, so need to find the difference between that
	// and the number of digits to find the first 1
	//note that this is different behavior than the other two implementations below because of what is returned
	return 63 - __builtin_clzll(x);
#elif defined(_MSC_VER)
	unsigned long bit;
	_BitScanReverse64(&bit, x);
	return bit;
#else
	size_t bit = 63;
	while((x & (1ULL << bit)) == 0)
		bit--;
	return bit;
#endif
}

//changes argv into string_view for easier use
inline std::vector<std::string_view> Platform_ArgvToStringViews(int argc, char **argv)
{
	std::vector<std::string_view> args;
	args.reserve(argc);
	for(int i = 0; i < argc; i++)
		args.emplace_back(argv[i]);
	return args;
}

//attempts to open filename
//if successful, returns a string of data from the file and true
//if failure, returns an error message and false
inline std::pair<std::string, bool> Platform_OpenFileAsString(const std::string &filename)
{
	std::ifstream inf(filename, std::ios::in | std::ios::binary);
	std::string data;

	if(!inf.good())
	{
		data = "Error loading file " + filename;
		return std::make_pair(data, false);
	}

	inf.seekg(0, std::ios::end);
	size_t file_size = inf.tellg();
	if(file_size > 0)
	{
		data.resize(static_cast<size_t>(file_size));
		inf.seekg(0, std::ios::beg);
		inf.read(&data[0], data.size());
	}
	inf.close();

	return std::make_pair(data, true);
}

//converts the string to a double, and returns true if it was successful, false if not
// note1: std::from_chars is supposed to be supported in all C++17 compliant compilers but
//        is not. If upgrading to gcc-11 or beyond, this should be updated. AppleClang does
//        not currently have a working implementation on any version.
// note2: std::from_chars is more desireable than std::strtod because it is locale independent
// TODO 15993: Reevaluate when moving to C++20
inline std::pair<double, bool> Platform_StringToNumber(const std::string &s)
{
#ifdef OS_WINDOWS
	const char *first_char = s.c_str();
	const char *last_char = first_char + s.length();
	double value = 0.0;
	auto [ptr, ec] = std::from_chars(first_char, last_char, value);
	//if there was no parse error and nothing left on string, then it's a number
	if(ec == std::errc() && ptr == last_char)
		return std::make_pair(value, true);
	return std::make_pair(0.0, false);
#else
	const char *start_pointer = s.c_str();
	char *end_pointer = nullptr;
	double value = strtod(start_pointer, &end_pointer);
	//if didn't reach the end or grabbed nothing, then it's not a number
	if(*end_pointer != '\0' || end_pointer == start_pointer)
		return std::make_pair(0.0, false);
	return std::make_pair(value, true);
#endif
}

//Takes a string containing a combined path/filename.extension, and breaks it into each of: path, base_filename, and extension
void Platform_SeparatePathFileExtension(const std::string &combined, std::string &path, std::string &base_filename, std::string &extension);

//fills file_names with the respective files of the given path given the path and extension
// if get_directories is true, it will fetch directories
void Platform_GetFileNamesOfType(std::vector<std::string> &file_names, const std::string &path, const std::string &extension, bool get_directories = false);

//runs command returns everything sent to stdout
// any parameters should be included in the command
//successful_run is set to true if the program was able to be found and run
//exit_code is set to the exit code of the program
std::string Platform_RunSystemCommand(std::string command, bool &successful_run, int &exit_code);

//returns a path to the home directory for the platform
std::string Platform_GetHomeDirectory();

//returns true if resource is readable given whether must_exist is set.  Returns false if not, and sets error string to the reason
bool Platform_IsResourcePathAccessible(const std::string &resource_path, bool must_exist, std::string &error);

//generates cryptographically secure random data into buffer to specified length
void Platform_GenerateSecureRandomData(void *buffer, size_t length);

//performs localtime in a threadsafe manner
// returns true on success
bool Platform_ThreadsafeLocaltime(std::time_t time_value, std::tm &localized_time);

//sleeps for given amount time
void Platform_Sleep(std::chrono::microseconds sleep_time_usec);

//returns true if a debugger is present
bool Platform_IsDebuggerPresent();

//returns a string representing the name of the operating system
std::string Platform_GetOperatingSystemName();

#ifdef OS_MAC
// warnings thrown on OS_MAC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#endif

#define assert(expr) Platform_Assert(expr, __FILE__, __LINE__)

#ifdef OS_MAC
// warnings thrown on OS_MAC
#pragma GCC diagnostic pop
#endif

inline void Platform_Assert(bool expr, const char *file, int line)
{
	if(!expr)
	{
		std::cerr << "Runtime Exception: Debug Assertion Failed at line " << line << " of " << file << "\n";

	//platform dependent assertion function
	#ifdef _DEBUG

		#ifdef OS_WINDOWS
			_ASSERT(expr);
		#else
			raise(SIGTRAP);
		#endif
			exit(-1);

	#else
		if(Platform_IsDebuggerPresent())
		{
			//wait for user input
			std::string temp;
			std::getline(std::cin, temp);
		}
		exit(-1);
	#endif
	}
}
