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
	AMALGAM_EXPORT LoadEntityStatus LoadEntity(char *handle, char *path, bool persistent, bool load_contained_entities,
		bool escape_filename, bool escape_contained_filenames, char *write_log_filename, char *print_log_filename);

	//loads the entity specified into handle
	//TODO 19512: deprecated - legacy method to support wrappers that can't call LoadEntity returning a LoadEntityStatus yet
	AMALGAM_EXPORT bool LoadEntityLegacy(char *handle, char *path, bool persistent, bool load_contained_entities, char *write_log_filename, char *print_log_filename);

	//verifies the entity specified by path. Uses LoadEntityStatus to return any errors and version
	AMALGAM_EXPORT LoadEntityStatus VerifyEntity(char *path);

	//clones the entity in handle to clone_handle
	//if persistent, then path represents the location it will be persisted to
	AMALGAM_EXPORT bool CloneEntity(char *handle, char *clone_handle, char *path, bool persistent, char *write_log_filename, char *print_log_filename);

	//stores the entity specified by handle into path
	AMALGAM_EXPORT void   StoreEntity(char *handle, char *path, bool update_persistence_location = false, bool store_contained_entities = true);

	//executes label on handle
	AMALGAM_EXPORT void   ExecuteEntity(char *handle, char *label);

	//destroys the entity specified by handle
	AMALGAM_EXPORT void   DestroyEntity(char *handle);

	//sets the random seed for the entity specified by handle
	AMALGAM_EXPORT bool   SetRandomSeed(char *handle, char *rand_seed);

	//sets num_entities to the number of entities and allocates an array of string pointers for the handles loaded
	AMALGAM_EXPORT char **GetEntities(uint64_t *num_entities);

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
