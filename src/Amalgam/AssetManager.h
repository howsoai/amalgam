#pragma once

//project headers:
#include "Entity.h"
#include "EntityExternalInterface.h"
#include "EvaluableNode.h"
#include "FileSupportCAML.h"
#include "HashMaps.h"

//system headers:
#include <string>

const std::string FILE_EXTENSION_AMLG_METADATA("mdam");
const std::string FILE_EXTENSION_AMALGAM("amlg");
const std::string FILE_EXTENSION_JSON("json");
const std::string FILE_EXTENSION_YAML("yaml");
const std::string FILE_EXTENSION_CSV("csv");
const std::string FILE_EXTENSION_COMPRESSED_AMALGAM_CODE("caml");

//forward declarations:
class AssetManager;

extern AssetManager asset_manager;

class AssetManager
{
public:
	AssetManager()
		: defaultEntityExtension(FILE_EXTENSION_AMALGAM), debugSources(false), debugMinimal(false)
	{	}

	//Returns the code to the corresponding entity by resource_path
	// sets resource_base_path to the resource path without the extension
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	EvaluableNodeReference LoadResourcePath(std::string &resource_path, std::string &resource_base_path,
		std::string &file_type, EvaluableNodeManager *enm, bool escape_filename, EntityExternalInterface::LoadEntityStatus &status);

	//Stores the code to the corresponding resource path
	// sets resource_base_path to the resource path without the extension, and extension accordingly
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	static bool StoreResourcePath(EvaluableNode *code, std::string &resource_path, std::string &resource_base_path,
		std::string &file_type, EvaluableNodeManager *enm, bool escape_filename, bool sort_keys)
	{
		std::string complete_resource_path;
		PreprocessFileNameAndType(resource_path, file_type, escape_filename, resource_base_path, complete_resource_path);

		return StoreResourcePathFromProcessedResourcePaths(code, complete_resource_path,
			file_type, enm, escape_filename, sort_keys);
	}

	static bool StoreResourcePathFromProcessedResourcePaths(EvaluableNode *code, std::string &complete_resource_path,
		std::string &file_type, EvaluableNodeManager *enm, bool escape_filename, bool sort_keys);

	//Loads an entity, including contained entites, etc. from the resource path specified
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity
	//if the resource does not have a metadata file, will use default_random_seed as its seed
	Entity *LoadEntityFromResourcePath(std::string &resource_path, std::string &file_type, bool persistent, bool load_contained_entities,
		bool escape_filename, bool escape_contained_filenames, std::string default_random_seed, EntityExternalInterface::LoadEntityStatus &status);

	//Stores an entity, including contained entites, etc. from the resource path specified
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity (will not make not persistent if was previously loaded as persistent)
	//returns true if successful
	bool StoreEntityToResourcePath(Entity *entity, std::string &resource_path, std::string &file_type,
		bool update_persistence_location, bool store_contained_entities,
		bool escape_filename, bool escape_contained_filenames, bool sort_keys,
		bool include_rand_seeds = true, bool parallel_create = false);

	//Indicates that the entity has been written to or updated, and so if the asset is persistent, the persistent copy should be updated
	void UpdateEntity(Entity *entity);
	void CreateEntity(Entity *entity);
	inline void DestroyEntity(Entity *entity)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock lock(persistentEntitiesMutex);
	#endif

		RemoveRootPermissions(entity);

		if(persistentEntities.size() > 0)
			DestroyPersistentEntity(entity);
	}

	//sets the entity's root permission to permission
	void SetRootPermission(Entity *entity, bool permission);

	// Checks if this entity or one of its containers is persistent
	inline bool IsEntityIndirectlyPersistent(Entity *entity)
	{
		Entity *cur = entity;

	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(persistentEntitiesMutex);
	#endif

		while(cur != nullptr)
		{
			if(persistentEntities.find(cur) != end(persistentEntities))
				return true;
		}

		return false;
	}

	// Checks if this entity specifically has been loaded as persistent
	inline bool IsEntityDirectlyPersistent(Entity *entity)
	{	return persistentEntities.find(entity) != end(persistentEntities);	}

	//sets the entity's persistent path
	inline void SetEntityPersistentPath(Entity *entity, std::string &resource_path)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock lock(persistentEntitiesMutex);
	#endif
		if(resource_path.empty())
			persistentEntities.erase(entity);
		else
			persistentEntities[entity] = resource_path;
	}

	inline bool DoesEntityHaveRootPermission(Entity *entity)
	{
		if(entity == nullptr)
			return false;

	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(rootEntitiesMutex);
	#endif

		return rootEntities.find(entity) != end(rootEntities);
	}

	//loads filename into the buffer specified by b (of type BufferType of elements BufferElementType)
	//if successful, returns no error message, file version (if available), and true
	//if failure, returns error message, file version (if available) and false
	template<typename BufferType>
	static std::tuple<std::string, std::string, bool> LoadFileToBuffer(const std::string &filename, std::string &file_type, BufferType &b)
	{
		std::ifstream f(filename, std::fstream::binary | std::fstream::in);

		if(!f.good())
			return std::make_tuple("Cannot open file", "", false);

		size_t header_size = 0;
		std::string file_version;
		if(file_type == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
		{
			auto [error_string, version, success] = FileSupportCAML::ReadHeader(f, header_size);
			if(!success)
				return std::make_tuple(error_string, version, false);
			else
				file_version = version;
		}

		f.seekg(0, std::ios::end);
		b.reserve(static_cast<std::streamoff>(f.tellg()) - header_size);
		f.seekg(header_size, std::ios::beg);

		b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
		return std::make_tuple("", file_version, true);
	}

	//stores buffer b (of type BufferType of elements BufferElementType) into the filename, returns true if successful, false if not
	template<typename BufferType>
	static bool StoreFileFromBuffer(const std::string &filename, std::string &file_type, BufferType &b)
	{
		std::ofstream f(filename, std::fstream::binary | std::fstream::out);
		if(!f.good())
			return false;

		if(file_type == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
		{
			if(!FileSupportCAML::WriteHeader(f))
				return false;
		}

		f.write(reinterpret_cast<char *>(&b[0]), sizeof(char) * b.size());
		return true;
	}

	//validates given asset version against Amalgam version
	//if successful: returns empty string and true
	//if failure: returns error message and false
	static std::pair<std::string, bool> ValidateVersionAgainstAmalgam(std::string &version);

	//returns a string representing en's source, empty string if debugSources is false
	std::string GetEvaluableNodeSourceFromComments(EvaluableNode *en);

	//default extension to store new entities
	std::string defaultEntityExtension;

	//if true, will enable debugging the sources of loading nodes
	bool debugSources;

	//if true, will exclude current position details when stepping
	bool debugMinimal;

private:

	//recursively deletes persistent entities
	void DestroyPersistentEntity(Entity *entity);

	//recursively removes root permissions
	void RemoveRootPermissions(Entity *entity);

	//using resource_path as the semantically intended path, populates resource_base_path and complete_resource_path,
	// and populates file_type if it is unspecified (empty string)
	//escapes the resource path if escape_resource_path is true
	static void PreprocessFileNameAndType(std::string &resource_path,
		std::string &file_type, bool escape_resource_path,
		std::string &resource_base_path, std::string &complete_resource_path);

	//entities that need changes stored, and the resource paths to store them
	CompactHashMap<Entity *, std::string> persistentEntities;

	//entities that have root permissions
	Entity::EntitySetType rootEntities;

#ifdef MULTITHREAD_INTERFACE
	//mutexes for global data
	Concurrency::ReadWriteMutex persistentEntitiesMutex;
	Concurrency::ReadWriteMutex rootEntitiesMutex;
#endif
};
