#pragma once

//project headers:
#include "StringManipulation.h"

//system headers:
#include <charconv>
#include <chrono>
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
		return {data, false};
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

	return {data, true};
}

#if defined(__APPLE__)
#include <xlocale.h>
#endif

//converts the string to a double, and returns true if it was successful, false if not
// note1: std::from_chars is supposed to be supported in all C++17 compliant compilers but
//        is not. In particular, AppleClang nor WASM builds currently have a working implementation
// note2: std::from_chars is more desirable than std::strtod because it is locale independent
template<typename StringType>
inline std::pair<double, bool> Platform_StringToNumber(const StringType &s)
{
	//check if the compiler supports floating-point std::from_chars
#if defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L)
	const char *first_char = s.data();
	const char *last_char = first_char + s.size();
	double value = 0.0;
	auto [ptr, ec] = std::from_chars(first_char, last_char, value);

	//if there was no parse error and nothing left on string, then it's a number
	if(ec == std::errc() && ptr == last_char)
		return {value, true};
	return {0.0, false};
#else
	// FALLBACK FOR APPLECLANG, WASM, AND OLDER PLATFORMS

	//need to ensure has null terminator
	std::string zero_terminated_copy(s);
	const char *start_pointer = zero_terminated_copy.data();
	char *end_pointer = nullptr;
	double value = 0.0;

#if defined(__APPLE__)
	//cache the C locale per-thread to eliminate the global penalty
	thread_local locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
	value = strtod_l(start_pointer, &end_pointer, c_locale);

#elif defined(__EMSCRIPTEN__) || defined(__wasm__)
	//WASM targets exclusively execute within a fixed "C" environment.
	//std::strtod is completely thread-safe here and lacks global states.
	value = std::strtod(start_pointer, &end_pointer);

#else
	// Default POSIX fallback using thread-safe locale context
	static locale_t global_c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
	value = strtod_l(start_pointer, &end_pointer, global_c_locale);
#endif

	// If didn't reach the end or grabbed nothing, then it's not a number
	if(end_pointer == start_pointer || end_pointer != (start_pointer + s.size()))
		return {0.0, false};

	return {value, true};
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

#define AmlgAssert(expr) Platform_Assert(expr, __FILE__, __LINE__)

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
	#ifdef OS_WINDOWS
		_ASSERT(expr);
	#else
		raise(SIGTRAP);
	#endif

		if(Platform_IsDebuggerPresent())
		{
			//wait for user input in case the _ASSERT above was optimized out
			std::string temp;
			std::getline(std::cin, temp);
		}

		exit(-1);
	}
}
