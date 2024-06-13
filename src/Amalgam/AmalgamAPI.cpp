//project headers:
#include "Amalgam.h"
#include "AmalgamVersion.h"
#include "Concurrency.h"
#include "EntityExternalInterface.h"
#include "EntityQueries.h"

//system headers:
#include <string>

//Workaround because GCC doesn't support strcpy_s
// TODO 15993: Reevaluate when moving to C++20
#if defined(__GNUC__)
#define strcpy_s(dest, size, source) {strncpy( (dest), (source), (size)); (dest)[(size) - 1] = '\0'; }
#endif

EntityExternalInterface entint;

//binary's concurrency build type
std::string ConcurrencyType()
{
	return
#if defined(MULTITHREAD_SUPPORT)
		"MultiThreaded"
#elif defined(_OPENMP)
		"OpenMP"
#elif defined(MULTITHREAD_SUPPORT) && defined(_OPENMP)
		"MultiThreaded+OpenMP"
#else
		"SingleThreaded"
#endif
		;
}

extern "C"
{
	// ************************************
	// helper functions (not in API)
	// ************************************

	// WARNING: when using StringToCharPtr & StringToWCharPtr, ownership
	//          of the memory is returned to caller. When sending strings
	//          across the library boundary, the callers must free the
	//          memory using 'DeleteString', otherwise a leak occurs.

	char *StringToCharPtr(std::string& value)
	{
		char *out = new char[value.length() + 1];
		strcpy_s(out, value.length() + 1, value.c_str());
		return out;
	}

	wchar_t *StringToWCharPtr(std::string& value)
	{
		std::wstring widestr = std::wstring(value.begin(), value.end());
		widestr += (wchar_t)0;
		wchar_t *wct = new wchar_t[widestr.length()];

		//The below call is depricated but medium risk since the buffer is generated within the function
		//and length of the string is tracked. This still could pose a vulnerability with malicious unicode
		//however and an alternative that returns with minimal amount of allocations that secure should
		//be explored. wcsncpy_s was explored as an option but is not guaranteed to exist in the STL for
		//linux.
	#ifdef _MSC_VER
	#pragma warning( push )
	#pragma warning( disable: 4996 )
	#endif
		wcsncpy(wct, widestr.c_str(), widestr.length());
	#ifdef _MSC_VER
	#pragma warning( pop )
	#endif
		return wct;
	}

	LoadEntityStatus ConvertLoadStatusToCStatus(EntityExternalInterface::LoadEntityStatus &status)
	{
		return {
			status.loaded,
			StringToCharPtr(status.message),
			StringToCharPtr(status.version)
		};
	}

	// ************************************
	// api methods
	// ************************************

	LoadEntityStatus LoadEntity(char *handle, char *path, bool persistent, bool load_contained_entities,
		bool escape_filename, bool escape_contained_filenames, char *write_log_filename, char *print_log_filename)
	{
		std::string h(handle);
		std::string p(path);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		auto status = entint.LoadEntity(h, p, persistent, load_contained_entities, escape_filename, escape_contained_filenames, wlfname, plfname);
		return ConvertLoadStatusToCStatus(status);
	}

	bool LoadEntityLegacy(char *handle, char *path, bool persistent, bool load_contained_entities, char *write_log_filename, char *print_log_filename)
	{
		auto status = LoadEntity(handle, path, persistent, load_contained_entities, false, false, write_log_filename, print_log_filename);

		delete[] status.message;
		delete[] status.version;

		return status.loaded;
	}

	LoadEntityStatus VerifyEntity(char *path)
	{
		std::string p(path);
		auto status = entint.VerifyEntity(p);
		return ConvertLoadStatusToCStatus(status);
	}

	bool CloneEntity(char *handle, char *clone_handle, char *path, bool persistent, char *write_log_filename, char *print_log_filename)
	{
		std::string h(handle);
		std::string ch(clone_handle);
		std::string p(path);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		return entint.CloneEntity(h, ch, p, persistent, wlfname, plfname);
	}

	void StoreEntity(char *handle, char *path, bool update_persistence_location, bool store_contained_entities)
	{
		std::string h(handle);
		std::string p(path);

		entint.StoreEntity(h, p, update_persistence_location, store_contained_entities);
	}

	void SetJSONToLabel(char *handle, char *label, char *json)
	{
		std::string h(handle);
		std::string l(label);
		std::string_view j(json);

		entint.SetJSONToLabel(h, l, j);
	}

	wchar_t *GetJSONPtrFromLabelWide(char *handle, char *label)
	{
		std::string h(handle);
		std::string l(label);
		std::string ret = entint.GetJSONFromLabel(h, l);
		return StringToWCharPtr(ret);
	}

	char *GetJSONPtrFromLabel(char *handle, char *label)
	{
		std::string h(handle);
		std::string l(label);
		std::string ret = entint.GetJSONFromLabel(h, l);
		return StringToCharPtr(ret);
	}

	wchar_t *GetVersionStringWide()
	{
		std::string version(AMALGAM_VERSION_STRING);
		return StringToWCharPtr(version);
	}

	char *GetVersionString()
	{
		std::string version(AMALGAM_VERSION_STRING);
		return StringToCharPtr(version);
	}

	wchar_t *GetConcurrencyTypeStringWide()
	{
		std::string ct = ConcurrencyType();
		return StringToWCharPtr(ct);
	}

	char *GetConcurrencyTypeString()
	{
		std::string ct = ConcurrencyType();
		return StringToCharPtr(ct);
	}

	wchar_t *ExecuteEntityJsonPtrWide(char *handle, char *label, char *json)
	{
		std::string h(handle);
		std::string l(label);
		std::string_view j(json);
		std::string ret = entint.ExecuteEntityJSON(h, l, j);
		return StringToWCharPtr(ret);
	}

	char *ExecuteEntityJsonPtr(char *handle, char *label, char *json)
	{
		std::string h(handle);
		std::string l(label);
		std::string_view j(json);
		std::string ret = entint.ExecuteEntityJSON(h, l, j);
		return StringToCharPtr(ret);
	}

	void ExecuteEntity(char *handle, char *label)
	{
		std::string h(handle);
		std::string l(label);

		entint.ExecuteEntity(h, l);
	}

	void DestroyEntity(char *handle)
	{
		std::string h(handle);
		entint.DestroyEntity(h);
	}

	bool SetRandomSeed(char *handle, char *rand_seed)
	{
		std::string h(handle);
		std::string s(rand_seed);
		return entint.SetRandomSeed(h, s);
	}

	char **GetEntities(uint64_t *num_entities)
	{
		std::vector<std::string> entities = entint.GetEntities();
		*num_entities = entities.size();
		char **return_entities = new char *[entities.size()];
		for(size_t i = 0; i < entities.size(); i++)
		{
			auto &handle = entities[i];
			char *new_string = new char[handle.size() + 1];
			for(size_t j = 0; j < handle.size(); j++)
				new_string[j] = handle[j];
			new_string[handle.size()] = '\0';

			return_entities[i] = new_string;
		}

		return return_entities;
	}

	void DeleteString(char *p)
	{
		delete[] p;
	}

	// ************************************
	// Amalgam Engine Flags
	// ************************************

	void SetSBFDataStoreEnabled(bool enable_SBF_datastore)
	{
		_enable_SBF_datastore = enable_SBF_datastore;
	}

	bool IsSBFDataStoreEnabled()
	{
		return _enable_SBF_datastore;
	}

	size_t GetMaxNumThreads()
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
		return Concurrency::GetMaxNumThreads();
	#else
		return 1;
	#endif
	}

	void SetMaxNumThreads(size_t max_num_threads)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
		Concurrency::SetMaxNumThreads(max_num_threads);
	#endif
	}
}
