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

	//output from ExecuteEntityJsonPtrLogged
	struct ResultWithLog
	{
		char *json;
		char *log;
	};

	//loads the entity specified into handle
	AMALGAM_EXPORT LoadEntityStatus LoadEntity(char *handle, char *path, char *file_type,
		bool persistent, char *json_file_params, char *write_log_filename, char *print_log_filename);

	//verifies the entity specified by path. Uses LoadEntityStatus to return any errors and version
	AMALGAM_EXPORT LoadEntityStatus VerifyEntity(char *path);

	//returns a json object of the entity permissions
	//see the documentation for the get_entity_permissions opcode for details
	AMALGAM_EXPORT char *GetEntityPermissions(char *handle);

	//sets the entity's permissions to the object in json_permissions
	//see the documentation for the get_entity_permissions opcode for details
	AMALGAM_EXPORT void SetEntityPermissions(char *handle, char *json_permissions);

	//clones the entity in handle to clone_handle
	//if persistent, then path, file_type, and json_file_params represent where and how it will be stored
	AMALGAM_EXPORT bool CloneEntity(char *handle, char *clone_handle, char *path,
		char *file_type, bool persistent, char *json_file_params,
		char *write_log_filename, char *print_log_filename);

	//stores the entity specified by handle into path
	AMALGAM_EXPORT void StoreEntity(char *handle, char *path, char *file_type, bool persistent, char *json_file_params);

	//executes label on handle
	AMALGAM_EXPORT void ExecuteEntity(char *handle, char *label);

	//destroys the entity specified by handle
	AMALGAM_EXPORT void DestroyEntity(char *handle);

	//sets the random seed for the entity specified by handle
	AMALGAM_EXPORT bool SetRandomSeed(char *handle, char *rand_seed);

	//sets num_entities to the number of entities and allocates an array of string pointers for the handles loaded
	AMALGAM_EXPORT char **GetEntities(uint64_t *num_entities);

	AMALGAM_EXPORT void SetJSONToLabel(char *handle, char *label, char *json);

	AMALGAM_EXPORT wchar_t *GetJSONPtrFromLabelWide(char *handle, char *label);
	AMALGAM_EXPORT char *GetJSONPtrFromLabel(char *handle, char *label);

	AMALGAM_EXPORT wchar_t *ExecuteEntityJsonPtrWide(char *handle, char *label, char *json);
	AMALGAM_EXPORT char *ExecuteEntityJsonPtr(char *handle, char *label, char *json);
	AMALGAM_EXPORT ResultWithLog ExecuteEntityJsonPtrLogged(char *handle, char *label, char *json);

	AMALGAM_EXPORT char *EvalOnEntity(char *handle, char *amlg);

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
