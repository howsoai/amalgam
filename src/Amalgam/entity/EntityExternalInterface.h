#pragma once

//project headers:
#include "EntityExecution.h"

//system headers:
#include <string>
#include <vector>

//forward declarations:
class ImportEntityStatus;
class Interpreter;

/*
 * This class constitutes the C++ backing for the C API, and is fully functional as a C++ API.
 *
 * Amalgam functions through the use of "Entities" which will have a predetermined set of "labels".
 * Loading an .amlg file with the LoadEntity command will assign the entity to a given handle.
 * The majority of the methods provided here allow manipulation of data associated with a label within an entity.
 * Some labels will be loaded with functions which can be executed (refer to the instructions for the entity you loaded).
 */

class EntityExternalInterface : public EntityExecution
{
public:

	ImportEntityStatus LoadEntity(std::string &handle, std::string &path, bool persistent, bool load_contained_entities,
		bool escape_filename, bool escape_contained_filenames, std::string &write_log_filename, std::string &print_log_filename,
		std::string rand_seed = std::string(""));
	ImportEntityStatus VerifyEntity(std::string &path);

	bool CloneEntity(std::string &handle, std::string &cloned_handle, std::string &path, bool persistent,
		std::string &write_log_filename, std::string &print_log_filename);

	void StoreEntity(std::string &handle, std::string &path, bool update_persistence_location, bool store_contained_entities);

	void DestroyEntity(std::string &handle);
	bool SetRandomSeed(std::string &handle, std::string &rand_seed);
	std::vector<std::string> GetEntities();

	bool SetJSONToLabel(std::string &handle, std::string &label, std::string_view json);
	std::string GetJSONFromLabel(std::string &handle, std::string &label);
	std::string ExecuteEntityJSON(std::string &handle, std::string &label, std::string_view json);
};
