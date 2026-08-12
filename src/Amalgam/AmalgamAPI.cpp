//project headers:
#include "Amalgam.h"
#include "AmalgamVersion.h"
#include "Concurrency.h"
#include "EntityExternalInterface.h"
#include "EntityQueries.h"
#include "PlatformSpecific.h"

//system headers:
#include <string>

EntityExternalInterface entint;

//binary's concurrency build type
std::string ConcurrencyType()
{
	return
	#if defined(MULTITHREAD_SUPPORT) && defined(_OPENMP)
		"MultiThreaded+OpenMP"
	#elif !defined(MULTITHREAD_SUPPORT) && defined(_OPENMP)
		"SingleThreaded+OpenMP"
	#elif defined(MULTITHREAD_SUPPORT)
		"MultiThreaded"
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

	wchar_t *StringToWCharPtr(std::string &value)
	{
		std::wstring widestr = std::wstring(value.begin(), value.end());
		widestr += (wchar_t)0;
		wchar_t *wct = new wchar_t[widestr.size()];

		//The below call is deprecated but medium risk since the buffer is generated within the function
		//and length of the string is tracked. This still could pose a vulnerability with malicious unicode
		//however and an alternative that returns with minimal amount of allocations that secure should
		//be explored. wcsncpy_s was explored as an option but is not guaranteed to exist in the STL for
		//linux.
	#ifdef _MSC_VER
	#pragma warning( push )
	#pragma warning( disable: 4996 )
	#endif
		wcsncpy(wct, widestr.c_str(), widestr.size());
	#ifdef _MSC_VER
	#pragma warning( pop )
	#endif
		return wct;
	}

	LoadEntityStatus ConvertLoadStatusToCStatus(EntityExternalInterface::LoadEntityStatus &status)
	{
		size_t entity_path_len = status.entity_path.size();
		char **entity_path = nullptr;
		if(entity_path_len > 0)
		{
			//We need to return a pointer that can be passed back to DestroyEntity().  That
			//believes it is being handed char[], so allocate exactly a char[] (as though we were
			//using plain-C malloc()) and then turn that into the pointer type we need.
			char *entity_path_alloc = new char[sizeof(char *) * entity_path_len];
			entity_path = reinterpret_cast<char **>(entity_path_alloc);
			for(size_t i = 0; i < entity_path_len; i++)
				entity_path[i] = strdup(status.entity_path[i].c_str());
		}
		return {
			status.loaded,
			strdup(status.message.c_str()),
			strdup(status.version.c_str()),
			entity_path,
			entity_path_len
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
		std::string p(path);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		auto status = entint.LoadEntity(h, EntityExternalInterface::LoadFromFile{ p }, ft, persistent, params, wlfname, plfname, eps);
		return ConvertLoadStatusToCStatus(status);
	}

	LoadEntityStatus LoadEntityFromMemory(char *handle, void *data, size_t len, char *file_type,
		bool persistent, char *json_file_params, char *write_log_filename, char *print_log_filename,
		const char **entity_path, size_t entity_path_len)
	{
		std::string h(handle);
		std::string d(static_cast<const char *>(data), len);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		auto status = entint.LoadEntity(h, EntityExternalInterface::LoadFromMemory{ std::move(d) }, ft, persistent, params, wlfname, plfname, eps);
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
		return strdup(ret.c_str());
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

	bool StoreEntity(char *handle, char *path, char *file_type, bool persistent, char *json_file_params, const char **entity_path, size_t entity_path_len)
	{
		std::string h(handle);
		std::string p(path);
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		return entint.StoreEntity(h, EntityExternalInterface::StoreToFile{ p }, ft, persistent, params, eps);
	}

	bool StoreEntityToMemory(char *handle, void **data_p, size_t *len_p, char *file_type,
		bool persistent, char *json_file_params, const char **entity_path, size_t entity_path_len)
	{
		std::string h(handle);
		std::string d;
		std::string ft(file_type);
		std::string_view params(json_file_params);
		std::vector<std::string> eps(entity_path, entity_path + entity_path_len);
		bool success = entint.StoreEntity(h, EntityExternalInterface::StoreToMemory{ d }, ft, persistent, params, eps);
		// This is the same fundamental API as strdup(.c_str()) above; the caller needs to
		// DeleteString() on the result.
		char *out = new char[d.size() + 1];
		std::memcpy(out, d.data(), d.size());
		out[d.size()] = '\0';
		*data_p = out;
		*len_p = d.size();
		return success;
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
		return strdup(ret.c_str());
	}

	wchar_t *GetVersionStringWide()
	{
		std::string version(AMALGAM_VERSION_STRING);
		return StringToWCharPtr(version);
	}

	char *GetVersionString()
	{
		std::string version(AMALGAM_VERSION_STRING);
		return strdup(version.c_str());
	}

	wchar_t *GetConcurrencyTypeStringWide()
	{
		std::string ct = ConcurrencyType();
		return StringToWCharPtr(ct);
	}

	char *GetConcurrencyTypeString()
	{
		std::string ct = ConcurrencyType();
		return strdup(ct.c_str());
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
		return strdup(ret.c_str());
	}

	ResultWithLog ExecuteEntityJsonPtrLogged(char *handle, char *label, char *json)
	{
		std::string h(handle);
		std::string l(label);
		std::string_view j(json);
		std::pair<std::string, std::string> ret = entint.ExecuteEntityJSONLogged(h, l, j);
		ResultWithLog rwl;
		rwl.json = strdup(ret.first.c_str());
		rwl.log = strdup(ret.second.c_str());
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
		return strdup(ret.c_str());
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
