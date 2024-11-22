#pragma once

//project headers:
#include "AmalgamVersion.h"
#include "BinaryPacking.h"
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

	class AssetParameters;
	using AssetParametersRef = std::shared_ptr<AssetParameters>;

	//parameters that define how an asset is loaded and stored
	class AssetParameters
	{
	public:
		//initializes defaults for AssetParameters -- should specify whether it is an entity
		//resource_path specifies the path.  if file_type is empty string, then it will
		//attempt to extract the file_type from the file extension on the resource
		AssetParameters(std::string resource_path, std::string file_type, bool is_entity);

		//copy constructor
		AssetParameters(const AssetParameters &other)
			: topEntity(other.topEntity),
			writeListener(nullptr),
			resourcePath(other.resourcePath),
			resourceBasePath(other.resourceBasePath),
			resourceType(other.resourceType),
			extension(other.extension),
			includeRandSeeds(other.includeRandSeeds),
			escapeResourceName(other.escapeResourceName),
			escapeContainedResourceNames(other.escapeContainedResourceNames),
			transactional(other.transactional),
			prettyPrint(other.prettyPrint),
			sortKeys(other.sortKeys),
			flatten(other.flatten),
			parallelCreate(other.parallelCreate),
			executeOnLoad(other.executeOnLoad)
		{
		}

		//initializes in a way intended for contained entities for _resource_base_path, will inherit parameters
		//but update with the new resource_base_path
		inline AssetParametersRef CreateAssetParametersForContainedResourceByResourceBasePath(std::string _resource_base_path)
		{
			AssetParametersRef new_params = std::make_shared<AssetParameters>(*this);
			new_params->resourceBasePath = _resource_base_path;
			new_params->resourcePath = _resource_base_path + "." + extension;

			//since it is contained, overwrite escapeResourceName
			new_params->escapeResourceName = escapeContainedResourceNames;

			return new_params;
		}

		//initializes in a way intended for contained entities for the given contained entity_id, will inherit parameters
		//but update with the new resource_base_path
		inline AssetParametersRef CreateAssetParametersForContainedResourceByEntityId(const std::string &entity_id)
		{
			AssetParametersRef new_params = std::make_shared<AssetParameters>(*this);
			if(escapeContainedResourceNames)
			{
				std::string ce_escaped_filename = FilenameEscapeProcessor::SafeEscapeFilename(entity_id);
				new_params->resourceBasePath = resourceBasePath + "/" + ce_escaped_filename;
			}
			else
			{
				new_params->resourceBasePath = resourceBasePath + "/" + entity_id;
			}

			new_params->resourcePath = new_params->resourceBasePath + "." + extension;

			//since it is contained, overwrite escapeResourceName
			new_params->escapeResourceName = escapeContainedResourceNames;

			return new_params;
		}

		//initializes and returns new asset parameters for a file of the same name but different extension
		inline AssetParametersRef CreateAssetParametersForAssociatedResource(std::string resource_type)
		{
			AssetParametersRef new_params = std::make_shared<AssetParameters>(*this);
			new_params->resourceType = resource_type;
			new_params->resourcePath = resourceBasePath + "." + resource_type;
			return new_params;
		}

		//sets the parameters
		void SetParams(EvaluableNode::AssocType &params);

		//updates resources based on the parameters -- should be called after SetParams
		void UpdateResources();

		//top entity being stored or loaded
		Entity *topEntity;

		//TODO 22194: initialize this for store and load, and use as appropriate
		//write listener if persistent flattened entity
		std::unique_ptr<EntityWriteListener> writeListener;

		//location of the file
		std::string resourcePath;

		//base path of the file
		std::string resourceBasePath;

		//type of the file
		std::string resourceType;

		//extension of the file name, which may or may not be equal to the file type
		std::string extension;

		//storage and loading parameters
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
	EvaluableNodeReference LoadResource(AssetParameters *asset_params, EvaluableNodeManager *enm,
		EntityExternalInterface::LoadEntityStatus &status);

	//loads the resource specified by asset_params into entity via transactional execution
	//returns true on success
	bool LoadResourceViaTransactionalExecution(AssetParameters *asset_params, Entity *entity,
		Interpreter *calling_interpreter, EntityExternalInterface::LoadEntityStatus &status);

	//Stores the code to the resource specified in asset_params
	bool StoreResource(EvaluableNode *code, AssetParameters *asset_params, EvaluableNodeManager *enm);

	//Loads an entity, including contained entities, etc. from the resource specified
	// if persistent is true, then it will keep the resource updated based on any calls to UpdateEntity
	//if the resource does not have a metadata file, will use default_random_seed as its seed
	Entity *LoadEntityFromResource(AssetParametersRef &asset_params, bool persistent,
		std::string default_random_seed, Interpreter *calling_interpreter, EntityExternalInterface::LoadEntityStatus &status);

	//Flattens entity piece-by-piece in a manner to reduce memory when storing
	template<typename EntityReferenceType = EntityReadReference>
	bool FlattenAndStoreEntityToResource(Entity *entity, AssetParameters *asset_params,
		Entity::EntityReferenceBufferReference<EntityReferenceType> &all_contained_entities)
	{
		asset_params->topEntity = entity;

		EvaluableNode *top_entity_code = EntityManipulation::FlattenOnlyTopEntity(&entity->evaluableNodeManager,
			entity, asset_params->includeRandSeeds, true);
		std::string code_string = Parser::Unparse(top_entity_code, asset_params->prettyPrint, true, asset_params->sortKeys, true);
		entity->evaluableNodeManager.FreeNodeTree(top_entity_code);

		//loop over contained entities, freeing resources after each entity
		for(size_t i = 0; i < all_contained_entities->size(); i++)
		{
			auto &cur_entity = (*all_contained_entities)[i];
			EvaluableNode *create_entity_code = EntityManipulation::FlattenOnlyOneContainedEntity(
				&entity->evaluableNodeManager, cur_entity, entity, asset_params->includeRandSeeds, true);

			code_string += Parser::Unparse(create_entity_code,
				asset_params->prettyPrint, true, asset_params->sortKeys, false, 1);

			entity->evaluableNodeManager.FreeNodeTree(create_entity_code);
		}

		code_string += Parser::transactionTermination;

		bool all_stored_successfully = false;

		if(asset_params->resourceType == FILE_EXTENSION_AMALGAM || asset_params->resourceType == FILE_EXTENSION_AMLG_METADATA)
		{
			std::ofstream outf(asset_params->resourcePath, std::ios::out | std::ios::binary);
			if(outf.good())
			{		
				outf.write(code_string.c_str(), code_string.size());
				outf.close();
				all_stored_successfully = true;
			}
		}
		else if(asset_params->resourceType == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
		{
			//transform into format needed for compression
			CompactHashMap<std::string, size_t> string_map;
			string_map[code_string] = 0;

			//compress and store
			BinaryData compressed_data = CompressStrings(string_map);
			all_stored_successfully = StoreFileFromBuffer<BinaryData>(asset_params->resourcePath, asset_params->resourceType, compressed_data);
		}

		return all_stored_successfully;
	}

	//Stores an entity, including contained entities, etc. from the resource specified
	// if update_persistence is true, then it will consider the persistent parameter, otherwise it is ignored
	// if persistent is true, then it will keep the resource updated, if false it will clear persistence
	// if store_contained_entities is true, then it will also write all contained entities
	// if all_contained_entities is nullptr, then it will be populated, as read locks are necessary for entities in multithreading
	//returns true if successful
	template<typename EntityReferenceType = EntityReadReference>
	bool StoreEntityToResource(Entity *entity, AssetParametersRef &asset_params,
		bool update_persistence, bool persistent,
		bool store_contained_entities = true,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
		if(entity == nullptr)
			return false;

		Entity::EntityReferenceBufferReference<EntityReferenceType> erbr;
		if(all_contained_entities == nullptr)
		{
			if(store_contained_entities || asset_params->flatten)
				erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReferenceType>();
			else
				erbr.Clear();

			all_contained_entities = &erbr;
		}

		if(asset_params->flatten
			&& (asset_params->resourceType == FILE_EXTENSION_AMALGAM
				|| asset_params->resourceType == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE))
		{
			bool all_stored_successfully = FlattenAndStoreEntityToResource(entity, asset_params.get(), *all_contained_entities);

			if(update_persistence)
				SetEntityPersistenceForFlattenedEntity(entity, persistent ? asset_params : nullptr);
			return all_stored_successfully;
		}

		if(!StoreResource(entity->GetRoot(), asset_params.get(), &entity->evaluableNodeManager))
			return false;

		if(asset_params->resourceType == FILE_EXTENSION_AMALGAM)
		{
			//store any metadata like random seed
			AssetParametersRef metadata_asset_params = asset_params->CreateAssetParametersForAssociatedResource(FILE_EXTENSION_AMLG_METADATA);

			EvaluableNode en_assoc(ENT_ASSOC);
			EvaluableNode en_rand_seed(ENT_STRING, entity->GetRandomState());
			EvaluableNode en_version(ENT_STRING, AMALGAM_VERSION_STRING);
			en_assoc.SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_rand_seed), &en_rand_seed);
			en_assoc.SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_version), &en_version);

			StoreResource(&en_assoc, metadata_asset_params.get(), &entity->evaluableNodeManager);
		}

		//store contained entities
		if(entity->GetContainedEntities().size() > 0)
		{
			if(!EnsureEntityToResourceCanContainEntities(asset_params.get()))
				return false;

			//only actually store the contained entities if directed
			if(store_contained_entities)
			{
				//store any contained entities
				for(auto contained_entity : entity->GetContainedEntities())
				{
					AssetParametersRef ce_asset_params
						= asset_params->CreateAssetParametersForContainedResourceByEntityId(contained_entity->GetId());

					//don't escape filename again because it's already escaped in this loop
					bool stored_successfully = StoreEntityToResource(contained_entity, ce_asset_params,
						update_persistence, persistent, true, all_contained_entities);

					if(!stored_successfully)
						return false;
				}
			}
		}

		//update after done using asset_params, just in case it is deleted
		if(update_persistence)
			SetEntityPersistence(entity, persistent ? asset_params : nullptr);

		return true;
	}

	//indicates that the entity's random seed has been updated
	template<typename EntityReferenceType = EntityReadReference>
	inline void UpdateEntityRandomSeed(Entity *entity, const std::string &rand_seed, bool deep_set,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(persistentEntitiesMutex);
	#endif

		//if persistent store only this entity, since only it is getting updated
		auto pe_entry = persistentEntities.find(entity);
		if(pe_entry != end(persistentEntities))
		{
			auto &asset_params = pe_entry->second;
			//if the entity is flattened, then need to find top level container entity
			//that is persistent and store it out with all its contained entities
			if(asset_params->flatten)
			{
				if(asset_params->writeListener != nullptr)
					asset_params->writeListener->LogSetEntityRandomSeed(entity, rand_seed, deep_set);
			}
			else //just update the individual entity
			{
				StoreEntityToResource(entity, asset_params, false, true, false, all_contained_entities);
			}
		}
	}

	//indicates that the entity's label_name has been updated to value
	template<typename EntityReferenceType = EntityReadReference>
	inline void UpdateEntityLabelValue(Entity *entity,
		StringInternPool::StringID label_name, EvaluableNode *value, bool direct_set,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(persistentEntitiesMutex);
	#endif

		//if persistent store only this entity, since only it is getting updated
		auto pe_entry = persistentEntities.find(entity);
		if(pe_entry != end(persistentEntities))
		{
			auto &asset_params = pe_entry->second;
			//if the entity is flattened, then need to find top level container entity
			//that is persistent and store it out with all its contained entities
			if(asset_params->flatten)
			{
				if(asset_params->writeListener != nullptr)
					asset_params->writeListener->LogWriteLabelValueToEntity(entity, label_name, value, direct_set);
			}
			else //just update the individual entity
			{
				StoreEntityToResource(entity, asset_params, false, true, false, all_contained_entities);
			}
		}
	}

	//indicates that the entity's labels have been updated by the corresponding values
	template<typename EntityReferenceType = EntityReadReference>
	inline void UpdateEntityLabelValues(Entity *entity,
		EvaluableNode *label_value_pairs, bool accum_values, bool direct_set,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(persistentEntitiesMutex);
	#endif

		//if persistent store only this entity, since only it is getting updated
		auto pe_entry = persistentEntities.find(entity);
		if(pe_entry != end(persistentEntities))
		{
			auto &asset_params = pe_entry->second;
			//if the entity is flattened, then need to find top level container entity
			//that is persistent and store it out with all its contained entities
			if(asset_params->flatten)
			{
				if(asset_params->writeListener != nullptr)
					asset_params->writeListener->LogWriteLabelValuesToEntity(entity, label_value_pairs,
						accum_values, direct_set);
			}
			else //just update the individual entity
			{
				StoreEntityToResource(entity, asset_params, false, true, false, all_contained_entities);
			}
		}
	}

	//indicates that the entity's root has been updated
	template<typename EntityReferenceType = EntityReadReference>
	inline void UpdateEntityRoot(Entity *entity,
		Entity::EntityReferenceBufferReference<EntityReferenceType> *all_contained_entities = nullptr)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock lock(persistentEntitiesMutex);
	#endif

		//if persistent store only this entity, since only it is getting updated
		auto pe_entry = persistentEntities.find(entity);
		if(pe_entry != end(persistentEntities))
		{
			auto &asset_params = pe_entry->second;
			//if the entity is flattened, then need to find top level container entity
			//that is persistent and store it out with all its contained entities
			if(asset_params->flatten)
			{
				if(asset_params->writeListener != nullptr)
					asset_params->writeListener->LogWriteToEntityRoot(entity);
			}
			else //just update the individual entity
			{
				StoreEntityToResource(entity, asset_params, false, true, false, all_contained_entities);
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

	// Checks if this entity specifically has been loaded as persistent
	inline bool IsEntityDirectlyPersistent(Entity *entity)
	{	return persistentEntities.find(entity) != end(persistentEntities);	}

	inline bool DoesEntityHaveRootPermission(Entity *entity)
	{
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
	static bool StoreFileFromBuffer(const std::string &filename, std::string &file_type, const BufferType &b)
	{
		std::ofstream f(filename, std::fstream::binary | std::fstream::out);
		if(!f.good())
			return false;

		if(file_type == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
		{
			if(!FileSupportCAML::WriteHeader(f))
				return false;
		}

		f.write(reinterpret_cast<const char *>(&b[0]), sizeof(char) * b.size());
		return true;
	}

	//validates given asset version against Amalgam version
	//if successful: returns empty string and true
	//if failure: returns error message and false
	static std::pair<std::string, bool> ValidateVersionAgainstAmalgam(const std::string &version);

	//returns a string representing en's source, empty string if debugSources is false
	std::string GetEvaluableNodeSourceFromComments(EvaluableNode *en);

	//default extension to store new entities
	std::string defaultEntityExtension;

	//if true, will enable debugging the sources of loading nodes
	bool debugSources;

	//if true, will exclude current position details when stepping
	bool debugMinimal;

private:

	//sets the entity's persistent path
	//if asset_params is null, then it will clear persistence
	//assumes persistentEntitiesMutex is locked
	inline void SetEntityPersistence(Entity *entity, AssetParametersRef asset_params)
	{
		if(asset_params == nullptr)
			persistentEntities.erase(entity);
		else
			persistentEntities.insert_or_assign(entity, asset_params);
	}

	//sets the persistence for the entity and everything within it
	void SetEntityPersistenceForFlattenedEntity(Entity *entity, AssetParametersRef &asset_params)
	{
		SetEntityPersistence(entity, asset_params);
		for(auto contained_entity : entity->GetContainedEntities())
			SetEntityPersistenceForFlattenedEntity(contained_entity, asset_params);
	}

	//clears all entity persistence recursively, assumes persistentEntitiesMutex is locked before calling
	void DeepClearEntityPersistenceRecurse(Entity *entity)
	{
		persistentEntities.erase(entity);

		for(auto contained_entity : entity->GetContainedEntities())
			DeepClearEntityPersistenceRecurse(contained_entity);
	}

	//creates any directory required to contain entities for asset_params
	inline bool EnsureEntityToResourceCanContainEntities(AssetParameters *asset_params)
	{
		std::error_code ec;
		//create directory in case it doesn't exist
		std::filesystem::create_directories(asset_params->resourceBasePath, ec);

		//return that the directory could not be created
		if(ec)
		{
			std::cerr << "Error creating directory: " << ec.message() << std::endl;
			return false;
		}

		return true;
	}

	//recursively deletes persistent entities
	void DestroyPersistentEntity(Entity *entity);

	//recursively removes root permissions
	void RemoveRootPermissions(Entity *entity);

	//entities that need changes stored, and the resource paths to store them
	FastHashMap<Entity *, AssetParametersRef> persistentEntities;

	//entities that have root permissions
	Entity::EntitySetType rootEntities;

#ifdef MULTITHREAD_INTERFACE
	//mutexes for global data
	Concurrency::ReadWriteMutex persistentEntitiesMutex;
	Concurrency::ReadWriteMutex rootEntitiesMutex;
#endif
};
