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
		//If an entity_path parameter was passed to LoadEntity, the path where the entity was
		//actually loaded.  If non-null, contains entity_path_len entries.  Both the entries
		//and the array itself need to be freed via DeleteString.
		char **entity_path;
		size_t entity_path_len;
	};

	//output from ExecuteEntityJsonPtrLogged
	struct ResultWithLog
	{
		char *json;
		char *log;
	};

	//loads the entity specified into handle
	AMALGAM_EXPORT LoadEntityStatus LoadEntity(char *handle, char *path, char *file_type,
		bool persistent, char *json_file_params, char *write_log_filename, char *print_log_filename,
		const char **entity_path, size_t entity_path_len);

	//loads the entity specified into handle, from a memory buffer
	AMALGAM_EXPORT LoadEntityStatus LoadEntityFromMemory(char *handle, void *data, size_t len, char *file_type,
		bool persistent, char *json_file_params, char *write_log_filename, char *print_log_filename,
		const char **entity_path, size_t entity_path_len);

	//verifies the entity specified by path. Uses LoadEntityStatus to return any errors and version
	AMALGAM_EXPORT LoadEntityStatus VerifyEntity(char *path);

	//returns a json object of the entity permissions
	//see the documentation for the get_entity_permissions opcode for details
	AMALGAM_EXPORT char *GetEntityPermissions(char *handle);

	//sets the entity's permissions to the object in json_permissions
	//see the documentation for the get_entity_permissions opcode for details
	AMALGAM_EXPORT bool SetEntityPermissions(char *handle, char *json_permissions);

	//clones the entity in handle to clone_handle
	//if persistent, then path, file_type, and json_file_params represent where and how it will be stored
	AMALGAM_EXPORT bool CloneEntity(char *handle, char *clone_handle, char *path,
		char *file_type, bool persistent, char *json_file_params,
		char *write_log_filename, char *print_log_filename);

	//stores the entity specified by handle into path
	AMALGAM_EXPORT bool StoreEntity(char *handle, char *path, char *file_type, bool persistent, char *json_file_params,
		const char **entity_path, size_t entity_path_len);

	//stores the entity specified into a memory buffer
	AMALGAM_EXPORT bool StoreEntityToMemory(char *handle, void **data_p, size_t *len_p, char *file_type,
		bool persistent, char *json_file_params, const char **entity_path, size_t entity_path_len);

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

	//changes the maximum number of threads to max_num_threads
	//if set to 0, will use however many cores are detected
	//if reducing the number of threads, this must be called from the main thread,
	//otherwise it will have no effect
	AMALGAM_EXPORT void SetMaxNumThreads(size_t max_num_threads);

	//for APIs that pass strings back, that memory needs to be cleaned up by the caller
	AMALGAM_EXPORT void DeleteString(char *p);
}
