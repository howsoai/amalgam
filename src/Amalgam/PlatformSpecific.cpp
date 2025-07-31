//project headers:
#include "PlatformSpecific.h"

#include "Concurrency.h"

//system headers:
#include <algorithm>
#include <array>
#include <cfenv>
#include <cstring>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

//perform universal initialization
class PlatformSpecificStartup
{
public:
	PlatformSpecificStartup()
	{
		std::feclearexcept(FE_ALL_EXCEPT);
	}
};
PlatformSpecificStartup _platform_specific_startup;

#ifdef OS_WINDOWS

	#define NOMINMAX
	#include <Windows.h>

#else
	#include <cstdlib>
	#include <dirent.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <string>
	#include <unistd.h>
#endif

void Platform_SeparatePathFileExtension(const std::string &combined, std::string &path, std::string &base_filename, std::string &extension)
{
	if(combined.size() == 0)
		return;

	//get path
	path = combined;
	size_t first_forward_slash = path.rfind('/');
	size_t first_backslash = path.rfind('\\');
	size_t first_slash;

	if(first_forward_slash == std::string::npos && first_backslash == std::string::npos)
		first_slash = 0;
	else if(first_forward_slash != std::string::npos && first_backslash == std::string::npos)
		first_slash = first_forward_slash;
	else if(first_forward_slash == std::string::npos && first_backslash != std::string::npos)
		first_slash = first_backslash;
	else //grab whichever one is closer to the end of the string
		first_slash = std::max(first_forward_slash, first_backslash);

	if(first_slash == 0)
		path = std::string("./");
	else
	{
		first_slash++;  //keep the slash in the path
		path = combined.substr(0, first_slash);
	}

	//get extension
	std::string filename = combined.substr(first_slash, combined.size() - first_slash);
	size_t extension_position = filename.rfind('.');
	if(extension_position != std::string::npos)
	{
		base_filename = filename.substr(0, extension_position);
		if(filename.size() > extension_position)
			extension = filename.substr(extension_position + 1, filename.size() - (extension_position + 1)); //get rid of .
	}
	else
	{
		base_filename = filename;
		extension = "";
	}
}

void Platform_GetFileNamesOfType(std::vector<std::string> &file_names, const std::string &path, const std::string &extension, bool get_directories)
{
	if(path.empty())
		return;

	std::error_code ec;
	if(!std::filesystem::exists(path, ec))
		return;

	// Use directory_iterator with error code
	for(const auto &entry : std::filesystem::directory_iterator(path, ec))
	{
		//something errored on iterating over the entries in the directory
		if(ec)
			break;

		std::error_code entry_ec;
		bool is_dir = entry.is_directory(entry_ec);

		//if the directory entry failed, go on to next
		if(entry_ec)
			continue;

		if(get_directories != is_dir)
			continue;

		//if extension is "*" or "*.*", include all files
		if(extension == "*" || extension == "*.*")
		{
			file_names.emplace_back(entry.path().filename().string());
			continue;
		}

		//check file extension
		std::string current_ext = entry.path().extension().string();

		//remove the dot from extension for comparison
		std::string clean_ext = (!extension.empty() && extension.front() == '.')
			? extension.substr(1)
			: extension;

		//compare extensions (case-insensitive)
		if(current_ext.empty() && clean_ext.empty())
		{
			file_names.emplace_back(entry.path().filename().string());
		}
		else if(!current_ext.empty())
		{
			//remove the dot from current extension
			current_ext = current_ext.substr(1);

			if(current_ext == clean_ext)
				file_names.emplace_back(entry.path().filename().string());
		}
	}
}

std::string Platform_RunSystemCommand(std::string command, bool &successful_run, int &exit_code)
{
	FILE *p;
#ifdef OS_WINDOWS
	p = _popen(command.c_str(), "r");
#else
	p = popen(command.c_str(), "r");
#endif

	if(p == nullptr)
	{
		exit_code = 0;
		successful_run = false;
		return "";
	}

	successful_run = true;

	std::array<char, 1024> buffer;
	std::string stdout_data;
	while(!feof(p))
	{
		if(fgets(buffer.data(), static_cast<int>(buffer.size()), p) != nullptr)
			stdout_data += buffer.data();
	}

#ifdef OS_WINDOWS
	exit_code = _pclose(p);
#else
	exit_code = pclose(p);
#endif

	return stdout_data;
}

std::string Platform_GetHomeDirectory()
{
#if defined (WINDOWS) || defined (WIN32) || defined (_WIN32)
	static char buff[MAX_PATH];
	const DWORD ret = GetEnvironmentVariableA("USERPROFILE", &buff[0], MAX_PATH);
	if(ret == 0 || ret > MAX_PATH)
		return "";
	else
		return &buff[0];
#else
	return getenv("HOME");
#endif
}

bool Platform_IsResourcePathAccessible(const std::string &resource_path, bool must_exist, std::string &error)
{
	struct stat fileStatus;
	errno = 0;
	if(stat(resource_path.c_str(), &fileStatus) == -1) // == 0 ok; == -1 error
	{
		if(must_exist && errno == ENOENT)
			error = "Resource path does not exist, or path is an empty string.";
		else if(errno == ENOTDIR)
			error = "A component of the path is not a directory.";
		else if(errno == ELOOP)
			error = "Too many symbolic links encountered while traversing the path.";
		else if(errno == EACCES)
			error = "Permission denied.";
		else if(errno == ENAMETOOLONG)
			error = "File cannot be read.";
		else if(errno == EBADF)
			error = "Bad filename.";
		else
			error = "Could not access file.";

		return false;
	}

	return true;
}

void Platform_GenerateSecureRandomData(void *buffer, size_t length)
{
#ifdef OS_WINDOWS
	HCRYPTPROV hCryptProv;
	CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	CryptGenRandom(hCryptProv, static_cast<DWORD>(length), static_cast<BYTE *>(buffer));
	CryptReleaseContext(hCryptProv, 0);
#else
	std::ifstream fp("/dev/random", std::ios::in | std::ios::binary);
	if(fp.good())
		fp.read(static_cast<char *>(buffer), sizeof(uint8_t) * length);
	fp.close();
#endif
}

//performs localtime in a threadsafe manner
bool Platform_ThreadsafeLocaltime(std::time_t time_value, std::tm &localized_time)
{
#ifdef OS_WINDOWS
	return localtime_s(&localized_time, &time_value) == 0; //MS swaps the values and returns the wrong thing
#else // POSIX
	return ::localtime_r(&time_value, &localized_time) != nullptr;
#endif
}

void Platform_Sleep(std::chrono::microseconds sleep_time_usec)
{
	//std::this_thread lives in the thread header. Instead of including that for all builds, use OS
	// sleep functions when not in multi-threaded builds.
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE) || defined(_OPENMP)
	std::this_thread::sleep_for(sleep_time_usec);
#elif defined(OS_WINDOWS)
	auto sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time_usec).count();
	Sleep(static_cast<DWORD>(sleep_time));
#else
	usleep(sleep_time_usec.count());
#endif
}

bool Platform_IsDebuggerPresent()
{
#ifdef OS_WINDOWS
	return (IsDebuggerPresent() ? true : false);
#endif
	return false;
}

std::string Platform_GetOperatingSystemName()
{
#ifdef OS_WINDOWS
	return "Windows";
#endif

#ifdef OS_LINUX
	return "Linux";
#endif

#ifdef OS_MAC
	return "Darwin";
#endif

	return "Unknown";
}
