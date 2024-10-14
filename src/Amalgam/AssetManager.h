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
		//_resource specifies the path.  if file_type is empty string, then it will
		//attempt to extract the file_type from the file extension on the resource
		AssetParameters(std::string _resource, std::string file_type, bool is_entity);

		//initializes in a way intended for contained entities, will inherit parameters
		//but update with the new resource, as well as flags for contained entities
		AssetParameters(std::string _resource, AssetParameters &inherit_from);

		//initializes and returns new asset parameters for a file of the same name but different extension
		inline AssetParameters CreateAssetParametersForAssociatedResource(std::string resource_type)
		{
			AssetParameters new_params(*this);
			new_params.resourceType = resource_type;
			new_params.resource = resourceBasePath + "." + resource_type;
			return new_params;
		}

		//sets the parameters and updates resources based on params
		void SetParamsAndUpdateResources(EvaluableNode::AssocType &params);

		std::string resource;
		std::string resourceBasePath;
		std::string resourceType;
		std::string extension;
		bool includeRandSeeds;
		bool escapeResourceName;
		bool escapeContainedResourceNames;
		bool transactional;
		bool prettyPrint;
		bool sortKeys;
		bool flatten;
		bool parallelCreate;
		bool executeOnLoad;
	};

	//Returns the code to the corresponding entity specified by asset_params using enm
	//Additionally returns the updated resource_base_path for the file, as well as the status
	EvaluableNodeReference LoadResource(AssetParameters &asset_params, EvaluableNodeManager *enm,
		EntityExternalInterface::LoadEntityStatus &status);

	//Stores the code to the resource specified in asset_params
	bool StoreResource(EvaluableNode *code, AssetParameters &asset_params, EvaluableNodeManager *enm);

	//Loads an entity, including contained entities, etc. from the resource specified
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity
	//if the resource does not have a metadata file, will use default_random_seed as its seed
	Entity *LoadEntityFromResource(AssetParameters &asset_params, bool persistent,
		std::string default_random_seed, Interpreter *calling_interpreter, EntityExternalInterface::LoadEntityStatus &status);

	//Stores an entity, including contained entities, etc. from the resource specified
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity (will not make not persistent if was previously loaded as persistent)
	// if all_contained_entities is nullptr, then it will be populated, as read locks are necessary for entities in multithreading
	//returns true if successful
	template<typename EntityReferenceType = EntityReadReference>
	bool StoreEntityToResource(Entity *entity, AssetParameters &asset_params, bool persistent,
		bool store_contained_entities = true,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
		if(entity == nullptr)
			return false;

		Entity::EntityReferenceBufferReference<EntityReferenceType> erbr;
		if(all_contained_entities == nullptr)
		{
			if(store_contained_entities || asset_params.flatten)
				erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReferenceType>();
			else
				erbr->bufferReference.emplace_back(EntityReferenceType(entity));

			all_contained_entities = &erbr;
		}

		//TODO 21711: finish the rest of this method

		if( (file_type == FILE_EXTENSION_AMALGAM || == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE) && flatten)
		{
			EvaluableNodeReference flattened_entity = EntityManipulation::FlattenEntity(&entity->evaluableNodeManager,
				entity, *all_contained_entities, include_rand_seeds, parallel_create);

			bool all_stored_successfully = AssetManager::StoreResourcePathFromProcessedResourcePath(flattened_entity,
				processed_resource_path, file_type, &entity->evaluableNodeManager, escape_filename, sort_keys, pretty_print);

			entity->evaluableNodeManager.FreeNodeTreeIfPossible(flattened_entity);
			return all_stored_successfully;
		}

		bool all_stored_successfully = AssetManager::StoreResourcePathFromProcessedResourcePath(entity->GetRoot(),
			processed_resource_path, file_type, &entity->evaluableNodeManager, escape_filename, sort_keys);

		//store any metadata like random seed
		std::string metadata_filename = resource_base_path + "." + FILE_EXTENSION_AMLG_METADATA;
		EvaluableNode en_assoc(ENT_ASSOC);
		EvaluableNode en_rand_seed(ENT_STRING, entity->GetRandomState());
		EvaluableNode en_version(ENT_STRING, AMALGAM_VERSION_STRING);
		en_assoc.SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_rand_seed), &en_rand_seed);
		en_assoc.SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_version), &en_version);

		std::string metadata_extension = FILE_EXTENSION_AMLG_METADATA;
		//don't reescape the path here, since it has already been done
		StoreResourcePathFromProcessedResourcePath(&en_assoc, metadata_filename, metadata_extension, &entity->evaluableNodeManager, false, sort_keys, pretty_print);

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
				bool stored_successfully = StoreEntityToResource(contained_entity, new_resource_path, file_type,
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
		if(entity == nullptr)
			return;

	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(persistentEntitiesMutex);
	#endif

		//if persistent store only this entity, since only it is getting updated
		auto pe_entry = persistentEntities.find(entity);
		if(pe_entry != end(persistentEntities))
		{
			//if the entity is flattened, then need to find top level container entity
			//that is persistent and store it out with all its contained entities
			if(pe_entry->second->flatten)
			{
				Entity *container = pe->first->GetContainer();

				while(true)
				{
					if(container == nullptr)
					{
						StoreEntityToResource(entity, *pe_entry->second, true, false, all_contained_entities);
						break;
					}

					auto container_pe_entry = persistentEntities.find(container);
					if(container_pe == end(persistentEntities))
					{
						StoreEntityToResource(entity, *pe_entry->second, true, false, all_contained_entities);
						break;
					}

					entity = container;
					pe_entry = container_pe;
				}
			}
			else //just update the individual entity
			{
				StoreEntityToResource(entity, *asset_params, true, false, all_contained_entities);
			}
		}
	}

	void CreateEntity(Entity *entity);

	inline void DestroyEntity(Entity *entity)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock lock(persistentEntitiesMutex);
	#endif

		RemoveRootPermissions(entity);

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
	inline void SetEntityPersistentPath(Entity *entity, std::string &processed_resource_path,
		AssetParameters *asset_params = nullptr)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock lock(persistentEntitiesMutex);
	#endif

		if(processed_resource_path.empty() || asset_params == nullptr)
			persistentEntities.erase(entity);
		else
			persistentEntities.emplace(entity, std::make_unique<AssetParameters>(asset_params));
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

	//entities that need changes stored, and the resource paths to store them
	FastHashMap<Entity *, std::unique_ptr<AssetParameters>> persistentEntities;

	//entities that have root permissions
	Entity::EntitySetType rootEntities;

#ifdef MULTITHREAD_INTERFACE
	//mutexes for global data
	Concurrency::ReadWriteMutex persistentEntitiesMutex;
	Concurrency::ReadWriteMutex rootEntitiesMutex;
#endif
};
