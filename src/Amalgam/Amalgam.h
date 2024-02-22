#pragma once

//system headers:
#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER)
//Microsoft 
#define AMALGAM_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
//GCC
#define AMALGAM_EXPORT __attribute__((visibility("default")))
#else
#define AMALGAM_EXPORT
#endif

extern "C"
{
	//status from LoadEntity
	struct LoadEntityStatus
	{
		bool loaded;
		char *message;
		char *version;
	};

	//loads the entity specified into handle
	AMALGAM_EXPORT LoadEntityStatus LoadEntity(char *handle, char *path, bool persistent, bool load_contained_entities, char *write_log_filename, char *print_log_filename);

	//stores the entity specified by handle into path
	AMALGAM_EXPORT void   StoreEntity(char *handle, char *path, bool update_persistence_location = false, bool store_contained_entities = true);

	//executes label on handle
	AMALGAM_EXPORT void   ExecuteEntity(char *handle, char *label);

	//deletes the entity specified by handle
	AMALGAM_EXPORT void   DeleteEntity(char *handle);

	//sets the random seed for the entity specified by handle
	AMALGAM_EXPORT bool   SetRandomSeed(char *handle, char *rand_seed);

	//sets num_entities to the number of entities and allocates an array of string pointers for the handles loaded
	AMALGAM_EXPORT char **GetEntities(uint64_t *num_entities);

	AMALGAM_EXPORT double GetNumberValue(char *handle, char *label);
	AMALGAM_EXPORT void   AppendNumberValue(char *handle, char *label, double value);
	AMALGAM_EXPORT void   SetNumberValue(char *handle, char *label, double value);

	AMALGAM_EXPORT void   AppendStringValue(char *handle, char *label, char *value);
	AMALGAM_EXPORT void   SetStringValue(char *handle, char *label, char *value);

	AMALGAM_EXPORT double *GetNumberListPtr(char *handle, char *label);
	AMALGAM_EXPORT size_t GetNumberListLength(char *handle, char *label);
	AMALGAM_EXPORT void   GetNumberList(char *handle, char *label, double *out_list);
	AMALGAM_EXPORT void   AppendNumberList(char *handle, char *label, double *list, size_t len);
	AMALGAM_EXPORT void   SetNumberList(char *handle, char *label, double *list, size_t len);

	// IMPORTANT: GetStringList assumes that the char ** array is unallocated
	//            If there are allocated char *s inside, they will become inacessable and be a memory leak.
	AMALGAM_EXPORT size_t GetStringListLength(char *handle, char *label);
	AMALGAM_EXPORT wchar_t **GetStringListPtrWide(char *handle, char *label);
	AMALGAM_EXPORT char **GetStringListPtr(char *handle, char *label);
	AMALGAM_EXPORT void   AppendStringList(char *handle, char *label, char **list, size_t len);
	AMALGAM_EXPORT void   SetStringList(char *handle, char *label, char **list, size_t len);

	AMALGAM_EXPORT void   SetJSONToLabel(char *handle, char *label, char *json);
	
	AMALGAM_EXPORT wchar_t *GetJSONPtrFromLabelWide(char *handle, char *label);
	AMALGAM_EXPORT char *GetJSONPtrFromLabel(char *handle, char *label);

	AMALGAM_EXPORT wchar_t *ExecuteEntityJsonPtrWide(char *handle, char *label, char *json);
	AMALGAM_EXPORT char *ExecuteEntityJsonPtr(char *handle, char *label, char *json);

	AMALGAM_EXPORT wchar_t *GetVersionStringWide();
	AMALGAM_EXPORT char *GetVersionString();

	AMALGAM_EXPORT wchar_t* GetConcurrencyTypeStringWide();
	AMALGAM_EXPORT char* GetConcurrencyTypeString();

	AMALGAM_EXPORT void SetSBFDataStoreEnabled(bool enable_SBF_datastore);
	AMALGAM_EXPORT bool IsSBFDataStoreEnabled();
	AMALGAM_EXPORT size_t GetMaxNumThreads();
	AMALGAM_EXPORT void SetMaxNumThreads(size_t max_num_threads);

	//for APIs that pass strings back, that memory needs to be cleaned up by the caller
	AMALGAM_EXPORT void DeleteString(char *p);
}
