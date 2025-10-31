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

		//The below call is deprecated but medium risk since the buffer is generated within the function
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

	LoadEntityStatus LoadEntity(char *handle, char *path, char *file_type,
		bool persistent, char *json_file_params, char *write_log_filename, char *print_log_filename,
		const char **entity_path, size_t entity_path_len)
	{
		std::string h(handle);
		EntityExternalInterface::LoadSource ls(path);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		auto status = entint.LoadEntity(h, ls, ft, persistent, params, wlfname, plfname, eps);
		return ConvertLoadStatusToCStatus(status);
	}

	LoadEntityStatus LoadEntityFromMemory(char *handle, void *data, size_t len, char *file_type,
		bool persistent, char *json_file_params, char *write_log_filename, char *print_log_filename,
		const char **entity_path, size_t entity_path_len)
	{
		std::string h(handle);
		EntityExternalInterface::LoadSource ls(std::in_place_type<std::pair<void *, size_t>>, data, len);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		auto status = entint.LoadEntity(h, ls, ft, persistent, params, wlfname, plfname, eps);
		return ConvertLoadStatusToCStatus(status);
	}

	LoadEntityStatus VerifyEntity(char *path)
	{
		std::string p(path);
		auto status = entint.VerifyEntity(p);
		return ConvertLoadStatusToCStatus(status);
	}

	char *GetEntityPermissions(char *handle)
	{
		std::string h(handle);
		std::string ret = entint.GetEntityPermissions(h);
		return StringToCharPtr(ret);
	}

	bool SetEntityPermissions(char *handle, char *json_permissions)
	{
		std::string h(handle);
		std::string perms(json_permissions);
		return entint.SetEntityPermissions(h, perms);
	}

	bool CloneEntity(char *handle, char *clone_handle, char *path,
		char *file_type, bool persistent, char *json_file_params, char *write_log_filename, char *print_log_filename)
	{
		std::string h(handle);
		std::string ch(clone_handle);
		std::string p(path);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		return entint.CloneEntity(h, ch, p, ft, persistent, params, wlfname, plfname);
	}

	void StoreEntity(char *handle, char *path, char *file_type, bool persistent, char *json_file_params, const char **entity_path, size_t entity_path_len)
	{
		std::string h(handle);
		EntityExternalInterface::StoreSource ss(path);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		entint.StoreEntity(h, ss, ft, persistent, params, eps);
	}

	void StoreEntityToMemory(char *handle, void **data_p, size_t *len_p, char *file_type,
		bool persistent, char *json_file_params, const char **entity_path, size_t entity_path_len)
	{
		std::string h(handle);
		EntityExternalInterface::StoreSource ss(std::in_place_type<std::pair<void **, size_t *>>, data_p, len_p);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		entint.StoreEntity(h, ss, ft, persistent, params, eps);
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

	ResultWithLog ExecuteEntityJsonPtrLogged(char *handle, char *label, char *json)
	{
		std::string h(handle);
		std::string l(label);
		std::string_view j(json);
		std::pair<std::string, std::string> ret = entint.ExecuteEntityJSONLogged(h, l, j);
		ResultWithLog rwl;
		rwl.json = StringToCharPtr(ret.first);
		rwl.log = StringToCharPtr(ret.second);
		return rwl;
	}

	void ExecuteEntity(char *handle, char *label)
	{
		std::string h(handle);
		std::string l(label);

		entint.ExecuteEntity(h, l);
	}

	char *EvalOnEntity(char *handle, char *amlg)
	{
		std::string h(handle);
		std::string a(amlg);
		std::string ret = entint.EvalOnEntity(h, a);
		return StringToCharPtr(ret);
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
