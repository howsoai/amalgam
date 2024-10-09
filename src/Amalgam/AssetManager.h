#pragma once

//project headers:
#include "AmalgamVersion.h"
#include "Entity.h"
#include "EntityExternalInterface.h"
#include "EntityManipulation.h"
#include "EvaluableNode.h"
#include "FilenameEscapeProcessor.h"
#include "FileSupportCAML.h"
#include "HashMaps.h"

//system headers:
#include <filesystem>
#include <string>
#include <tuple>

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

	//parameters that define how an asset is loaded and stored
	struct AssetParameters
	{
		//initializes defaults for AssetParameters -- should specify whether it is an entity
		void Initialize(bool is_entity);

		//sets the parameters based on whether 
		void SetParams(EvaluableNode::AssocType &params);

		std::string resource;
		std::string fileType;
		bool includeRandSeeds;
		bool escapeFilename;
		bool escapeContainedFilenames;
		bool transactional;
		bool prettyPrint;
		bool sortKeys;
		bool flatten;
		bool parallelCreate;
		bool executeOnLoad;
	};

	//Returns the code to the corresponding entity specified by asset_params using enm
	//Additionally returns the updated resource_base_path for the file, as well as the status
	EvaluableNodeReference LoadResourcePath(AssetParameters &asset_params, EvaluableNodeManager *enm,
		std::string &resource_base_path, EntityExternalInterface::LoadEntityStatus &status);

	//Stores the code to the corresponding resource path
	// sets resource_base_path to the resource path without the extension
	static bool StoreResourcePath(EvaluableNode *code, AssetParameters &asset_params, std::string &resource_base_path,
		EvaluableNodeManager *enm)
	{
		std::string extension, complete_resource_path;
		PreprocessFileNameAndType(asset_params.resource, asset_params.escapeFilename, extension, resource_base_path, complete_resource_path);

		return StoreResourcePathFromProcessedResourcePaths(code, complete_resource_path,
			file_type, enm, escape_filename, sort_keys);
	}

	static bool StoreResourcePathFromProcessedResourcePaths(EvaluableNode *code, std::string &complete_resource_path,
		std::string &file_type, EvaluableNodeManager *enm, bool escape_filename, bool sort_keys, bool pretty_print);

	//Loads an entity, including contained entities, etc. from the resource path specified
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity
	//if the resource does not have a metadata file, will use default_random_seed as its seed
	Entity *LoadEntityFromResourcePath(std::string &resource_path, std::string &file_type, bool persistent,
		bool load_contained_entities, bool escape_filename, bool escape_contained_filenames,
		std::string default_random_seed, Interpreter *calling_interpreter, EntityExternalInterface::LoadEntityStatus &status);

	//Stores an entity, including contained entities, etc. from the resource path specified
	//if file_type is not an empty string, it will use the specified file_type instead of the filename's extension
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity (will not make not persistent if was previously loaded as persistent)
	// if all_contained_entities is nullptr, then it will be populated, as read locks are necessary for entities in multithreading
	//returns true if successful
	template<typename EntityReferenceType = EntityReadReference>
	bool StoreEntityToResourcePath(Entity *entity, std::string &resource_path, std::string &file_type,
		bool update_persistence_location, bool store_contained_entities,
		bool escape_filename, bool escape_contained_filenames, bool pretty_print, bool sort_keys,
		bool include_rand_seeds = true, bool flatten = true, bool parallel_create = false,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
		if(entity == nullptr)
			return false;

		std::string resource_base_path;
		std::string complete_resource_path;
		PreprocessFileNameAndType(resource_path, escape_filename, file_type, resource_base_path, complete_resource_path);

		Entity::EntityReferenceBufferReference<EntityReferenceType> erbr;
		if(all_contained_entities == nullptr)
		{
			erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReferenceType>();
			all_contained_entities = &erbr;
		}

		if( (file_type == FILE_EXTENSION_AMALGAM || == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE) && flatten)
		{
			EvaluableNodeReference flattened_entity = EntityManipulation::FlattenEntity(&entity->evaluableNodeManager,
				entity, *all_contained_entities, include_rand_seeds, parallel_create);

			bool all_stored_successfully = AssetManager::StoreResourcePathFromProcessedResourcePaths(flattened_entity,
				complete_resource_path, file_type, &entity->evaluableNodeManager, escape_filename, sort_keys, pretty_print);

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
		StoreResourcePathFromProcessedResourcePaths(&en_assoc, metadata_filename, metadata_extension, &entity->evaluableNodeManager, false, sort_keys, pretty_print);

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

				//TODO 21711: change what is stored here to include flags
				//don't escape filename again because it's already escaped in this loop
				bool stored_successfully = StoreEntityToResourcePath(contained_entity, new_resource_path, file_type,
					false, true, escape_filename, escape_contained_filenames, pretty_print, sort_keys,
					include_rand_seeds, flatten, parallel_create);

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
					false, false, false, true, false, false, true, false, false, all_contained_entities);
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
	inline void SetEntityPersistentPath(Entity *entity, AssetParameters &asset_params)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock lock(persistentEntitiesMutex);
	#endif
		if(asset_params.resource.empty())
			persistentEntities.erase(entity);
		else
			persistentEntities.emplace(entity, asset_params);
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

	//using resource_path as the semantically intended path,
	// escaping the resource path if escape_resource_path is true
	// populates resource_base_path, complete_resource_path, and extension
	static void PreprocessFileNameAndType(std::string &resource_path, bool escape_resource_path,
		std::string &extension, std::string &resource_base_path, std::string &complete_resource_path);

	//entities that need changes stored, and the resource paths to store them
	CompactHashMap<Entity *, std::unique_ptr<AssetParameters>> persistentEntities;

	//entities that have root permissions
	Entity::EntitySetType rootEntities;

#ifdef MULTITHREAD_INTERFACE
	//mutexes for global data
	Concurrency::ReadWriteMutex persistentEntitiesMutex;
	Concurrency::ReadWriteMutex rootEntitiesMutex;
#endif
};
