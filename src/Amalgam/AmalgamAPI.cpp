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

	char* StringToCharPtr(std::string& value)
	{
		char* out = new char[value.length() + 1];
		strcpy_s(out, value.length() + 1, value.c_str());
		return out;
	}

	wchar_t* StringToWCharPtr(std::string& value)
	{
		std::wstring widestr = std::wstring(value.begin(), value.end());
		widestr += (wchar_t)0;
		wchar_t* wct = new wchar_t[widestr.length()];

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
		LoadEntityStatus c_status = {
			status.loaded,
			StringToCharPtr(status.message),
			StringToCharPtr(status.version)
		};
		return c_status;
	}

	// ************************************
	// api methods
	// ************************************

	LoadEntityStatus LoadEntity(char *handle, char *path, bool persistent, bool load_contained_entities, char *write_log_filename, char *print_log_filename)
	{
		std::string h(handle);
		std::string p(path);
		std::string wlfname(write_log_filename);
		std::string plfname(print_log_filename);
		auto status = entint.LoadEntity(h, p, persistent, load_contained_entities, wlfname, plfname);
		return ConvertLoadStatusToCStatus(status);
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

	wchar_t* GetConcurrencyTypeStringWide()
	{
		std::string ct = ConcurrencyType();
		return StringToWCharPtr(ct);
	}

	char* GetConcurrencyTypeString()
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

	void DeleteEntity(char *handle)
	{
		std::string h(handle);
		entint.DeleteEntity(h);
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

	// ************************************
	// get, set, and append numbers
	// ************************************

	double GetNumberValue(char *handle, char *label)
	{
		std::string h(handle);
		std::string l(label);

		return entint.GetNumber(h, l);
	}

	void AppendNumberValue(char *handle, char *label, double value)
	{
		std::string h(handle);
		std::string l(label);

		entint.AppendToLabel(h, l, value);
	}

	void SetNumberValue(char *handle, char *label, double value)
	{
		std::string h(handle);
		std::string l(label);

		entint.SetLabel(h, l, value);
	}

	void AppendStringValue(char *handle, char *label, char *value)
	{
		std::string h(handle);
		std::string l(label);
		std::string v(value);

		entint.AppendToLabel(h, l, v);
	}

	void SetStringValue(char *handle, char *label, char *value)
	{
		std::string h(handle);
		std::string l(label);
		std::string v(value);

		entint.SetLabel(h, l, v);
	}

	void AppendNumberList(char *handle, char *label, double *list, size_t len)
	{
		std::string h(handle);
		std::string l(label);

		for(size_t i = 0; i < len; i++)
		{
			entint.AppendToLabel(h, l, list[i]);
		}
	}

	void SetNumberList(char *handle, char *label, double *list, size_t len)
	{
		std::string h(handle);
		std::string l(label);

		entint.SetNumberList(h, l, list, len);
	}

	void SetNumberMatrix(char *handle, char *label, double *list, size_t rows, size_t columns)
	{
		std::string h(handle);
		std::string l(label);

		entint.SetNumberMatrix(h, l, list, rows, columns);
	}

	size_t GetNumberListLength(char *handle, char *label) 
	{
		std::string h(handle);
		std::string l(label);

		return entint.GetNumberListLength(h, l);
	}

	size_t GetNumberMatrixWidth(char *handle, char *label) 
	{
		std::string h(handle);
		std::string l(label);

		return entint.GetNumberMatrixWidth(h, l);
	}

	size_t GetNumberMatrixHeight(char *handle, char *label) 
	{
		std::string h(handle);
		std::string l(label);

		return entint.GetNumberMatrixHeight(h, l);
	}

	double *GetNumberListPtr(char *handle, char *label) 
	{
		std::string h(handle);
		std::string l(label);

		size_t len = GetNumberListLength(handle, label);

		double *ret = new double[len];

		entint.GetNumberList(h, l, ret, len);
		return ret;
	}

	double *GetNumberMatrixPtr(char *handle, char *label) 
	{
		std::string h(handle);
		std::string l(label);

		size_t width = GetNumberMatrixWidth(handle, label);
		size_t height = GetNumberMatrixHeight(handle, label);

		double *ret = new double[width*height];

		entint.GetNumberMatrix(h, l, ret, width, height);
		return ret;
	}

	void GetNumberList(char *handle, char *label, double *out_list)
	{
		std::string h(handle);
		std::string l(label);

		size_t len = GetNumberListLength(handle, label);

		entint.GetNumberList(h, l, out_list, len);
	}

	void AppendStringList(char *handle, char *label, char **list, size_t len)
	{
		std::string h(handle);
		std::string l(label);

		for(size_t i = 0; i < len; i++)
		{
			std::string to_append(list[i]);
			entint.AppendToLabel(h, l, to_append);
		}
	}

	void SetStringList(char *handle, char *label, char **list, size_t len)
	{
		std::string h(handle);
		std::string l(label);

		entint.SetStringList(h, l, list, len);
	}

	size_t GetStringListLength(char *handle, char *label) 
	{
		std::string h(handle);
		std::string l(label);

		return entint.GetStringListLength(h, l);
	}

	// IMPORTANT: GetStringList assumes that the char ** array is unallocated
	//            If there are allocated char *s inside, they will become inacessable and be a memory leak.
	wchar_t **GetStringListPtrWide(char *handle, char *label)
	{
		std::string h(handle);
		std::string l(label);

		size_t len = GetStringListLength(handle, label);

		std::string *str_list = new std::string[len];

		entint.GetStringList(h, l, str_list, len);

		wchar_t **wct = new wchar_t *[len];
		for(size_t i = 0; i < len; i++)
		{
			wct[i] = StringToWCharPtr(str_list[i]);
		}

		return wct;
	}

	// IMPORTANT: GetStringList assumes that the char ** array is unallocated
	//            If there are allocated char *s inside, they will become inacessable and be a memory leak.
	char **GetStringListPtr(char *handle, char *label)
	{
		std::string h(handle);
		std::string l(label);

		size_t len = GetStringListLength(handle, label);

		std::string *str_list = new std::string[len];

		entint.GetStringList(h, l, str_list, len);

		char **ct = new char *[len];
		for(size_t i = 0; i < len; i++)
			ct[i] = StringToCharPtr(str_list[i]);

		return ct;
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
