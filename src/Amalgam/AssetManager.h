#pragma once

//project headers:
#include "AmalgamVersion.h"
#include "Entity.h"
#include "EntityManipulation.h"
#include "EvaluableNode.h"
#include "FilenameEscapeProcessor.h"
#include "FileSupportCAML.h"
#include "HashMaps.h"

//system headers:
#include <filesystem>
#include <string>
#include <tuple>

//forward declarations:
class ImportEntityStatus;

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
		std::string &file_type, EvaluableNodeManager *enm, bool escape_filename, ImportEntityStatus &status);

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

	//Loads an entity, including contained entities, etc. from the resource path specified
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity
	//if the resource does not have a metadata file, will use default_random_seed as its seed
	Entity *LoadEntityFromResourcePath(std::string &resource_path, std::string &file_type, bool persistent,
		bool load_contained_entities, bool escape_filename, bool escape_contained_filenames,
		std::string default_random_seed, Interpreter *calling_interpreter, ImportEntityStatus &status);

	//Stores an entity, including contained entities, etc. from the resource path specified
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity (will not make not persistent if was previously loaded as persistent)
	// if all_contained_entities is nullptr, then it will be populated, as read locks are necessary for entities in multithreading
	//returns true if successful
	template<typename EntityReferenceType = EntityReadReference>
	bool StoreEntityToResourcePath(Entity *entity, std::string &resource_path, std::string &file_type,
		bool update_persistence_location, bool store_contained_entities,
		bool escape_filename, bool escape_contained_filenames, bool sort_keys,
		bool include_rand_seeds = true, bool parallel_create = false,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
		if(entity == nullptr)
			return false;

		std::string resource_base_path;
		std::string complete_resource_path;
		PreprocessFileNameAndType(resource_path, file_type, escape_filename, resource_base_path, complete_resource_path);

		Entity::EntityReferenceBufferReference<EntityReferenceType> erbr;
		if(all_contained_entities == nullptr)
		{
			erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReferenceType>();
			all_contained_entities = &erbr;
		}

		if(file_type == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
		{
			EvaluableNodeReference flattened_entity = EntityManipulation::FlattenEntity(&entity->evaluableNodeManager,
				entity, *all_contained_entities, include_rand_seeds, parallel_create);

			bool all_stored_successfully = AssetManager::StoreResourcePathFromProcessedResourcePaths(flattened_entity,
				complete_resource_path, file_type, &entity->evaluableNodeManager, escape_filename, sort_keys);

			entity->evaluableNodeManager.FreeNodeTreeIfPossible(flattened_entity);
			return all_stored_successfully;
		}

		bool all_stored_successfully = AssetManager::StoreResourcePathFromProcessedResourcePaths(entity->GetRoot(),
			complete_resource_path, file_type, &entity->evaluableNodeManager, escape_filename, sort_keys);

		//store any metadata like random seed
		std::string metadata_filename = resource_base_path + "." + FILE_EXTENSION_AMLG_METADATA;
		EvaluableNode en_assoc(ENT_ASSOC);
		EvaluableNode en_rand_seed(ENT_STRING, entity->GetRandomState());
		EvaluableNode en_version(ENT_STRING, AMALGAM_VERSION_STRING);
		en_assoc.SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_rand_seed), &en_rand_seed);
		en_assoc.SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_version), &en_version);

		std::string metadata_extension = FILE_EXTENSION_AMLG_METADATA;
		//don't reescape the path here, since it has already been done
		StoreResourcePathFromProcessedResourcePaths(&en_assoc, metadata_filename, metadata_extension, &entity->evaluableNodeManager, false, sort_keys);

		//store contained entities
		if(store_contained_entities && entity->GetContainedEntities().size() > 0)
		{
			std::error_code ec;
			//create directory in case it doesn't exist
			std::filesystem::create_directories(resource_base_path, ec);

			//return that the directory could not be created
			if(ec)
				return false;

			//store any contained entities
			resource_base_path.append("/");
			for(auto contained_entity : entity->GetContainedEntities())
			{
				std::string new_resource_path;
				if(escape_contained_filenames)
				{
					const std::string &ce_escaped_filename = FilenameEscapeProcessor::SafeEscapeFilename(contained_entity->GetId());
					new_resource_path = resource_base_path + ce_escaped_filename + "." + file_type;
				}
				else
					new_resource_path = resource_base_path + contained_entity->GetId() + "." + file_type;

				//don't escape filename again because it's already escaped in this loop
				bool stored_successfully = StoreEntityToResourcePath(contained_entity, new_resource_path, file_type, false, true, false,
					escape_contained_filenames, sort_keys, include_rand_seeds, parallel_create);
				if(!stored_successfully)
					return false;
			}
		}

		if(update_persistence_location)
		{
			std::string new_persist_path = resource_base_path + "." + file_type;
			SetEntityPersistentPath(entity, new_persist_path);
		}

		return all_stored_successfully;
	}

	//Indicates that the entity has been written to or updated, and so if the asset is persistent, the persistent copy should be updated
	template<typename EntityReferenceType = EntityReadReference>
	void UpdateEntity(Entity *entity,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(persistentEntitiesMutex);
	#endif
		//early out if no persistent entities
		if(persistentEntities.size() == 0)
			return;

		Entity *cur = entity;
		std::string slice_path;
		std::string filename;
		std::string extension;
		std::string traversal_path;

		while(cur != nullptr)
		{
			const auto &pe = persistentEntities.find(cur);
			if(pe != end(persistentEntities))
			{
				Platform_SeparatePathFileExtension(pe->second, slice_path, filename, extension);
				std::string new_path = slice_path + filename + traversal_path + "." + extension;

				//the outermost file is already escaped, but persistent entities must be recursively escaped
				StoreEntityToResourcePath(entity, new_path, extension,
					false, false, false, true, false, true, false, all_contained_entities);
			}

			//don't need to continue and allocate extra traversal path if already at outermost entity
			Entity *cur_container = cur->GetContainer();
			if(cur_container == nullptr)
				break;

			std::string escaped_entity_id = FilenameEscapeProcessor::SafeEscapeFilename(cur->GetId());
			traversal_path = "/" + escaped_entity_id + traversal_path;
			cur = cur_container;
		}
	}

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
