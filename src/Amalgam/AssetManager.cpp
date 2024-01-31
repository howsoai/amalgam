//project headers:
#include "AssetManager.h"

#include "Amalgam.h"
#include "BinaryPacking.h"
#include "EvaluableNode.h"
#include "FilenameEscapeProcessor.h"
#include "FileSupportCSV.h"
#include "FileSupportJSON.h"
#include "FileSupportYAML.h"
#include "PlatformSpecific.h"

//system headers:
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>

AssetManager asset_manager;

EvaluableNodeReference AssetManager::LoadResourcePath(std::string &resource_path,
	std::string &resource_base_path, std::string &file_type, EvaluableNodeManager *enm, bool escape_filename, LoadEntityStatus &status)
{
	//get file path based on the file loaded
	std::string path, file_base, extension;
	Platform_SeparatePathFileExtension(resource_path, path, file_base, extension);
	resource_base_path = path + file_base;

	//escape the string if necessary, otherwise just use the regular one
	std::string processed_resource_path;
	if(escape_filename)
	{
		resource_base_path = path + FilenameEscapeProcessor::SafeEscapeFilename(file_base);
		processed_resource_path = resource_base_path + "." + extension;
	}
	else
	{
		resource_base_path = path + file_base;
		processed_resource_path = resource_path;
	}

	if(file_type == "")
		file_type = extension;

	//load this entity based on file_type
	if(file_type == FILE_EXTENSION_AMALGAM || file_type == FILE_EXTENSION_AMLG_METADATA)
	{
		auto [code, code_success] = Platform_OpenFileAsString(processed_resource_path);
		if(!code_success)
		{
			if(file_type == FILE_EXTENSION_AMALGAM)
				std::cerr << code << std::endl;
			return EvaluableNodeReference::Null();
		}

		//check for byte order mark for UTF-8 that may optionally appear at the beginning of the file.
		// If it is present, remove it.  No other encoding standards besides ascii and UTF-8 are currently permitted.
		if(code.size() >= 3)
		{
			if(static_cast<uint8_t>(code[0]) == 0xEF && static_cast<uint8_t>(code[1]) == 0xBB && static_cast<uint8_t>(code[2]) == 0xBF)
				code.erase(0, 3);
		}

		if(!debugSources)
			return Parser::Parse(code, enm);
		else
			return Parser::Parse(code, enm, &resource_path);
	}
	else if(file_type == FILE_EXTENSION_JSON)
		return EvaluableNodeReference(EvaluableNodeJSONTranslation::Load(processed_resource_path, enm), true);
	else if(file_type == FILE_EXTENSION_YAML)
		return EvaluableNodeReference(EvaluableNodeYAMLTranslation::Load(processed_resource_path, enm), true);
	else if(file_type == FILE_EXTENSION_CSV)
		return EvaluableNodeReference(FileSupportCSV::Load(processed_resource_path, enm), true);
	else if(file_type == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
	{
		BinaryData compressed_data;
		if(!LoadFileToBuffer<BinaryData>(processed_resource_path, file_type, compressed_data))
			return EvaluableNodeReference::Null();

		OffsetIndex cur_offset = 0;
		auto strings = DecompressStrings(compressed_data, cur_offset);
		if(strings.size() == 0)
			return EvaluableNodeReference::Null();

		if(!debugSources)
			return Parser::Parse(strings[0], enm);
		else
			return Parser::Parse(strings[0], enm, &resource_path);
	}
	else //just load the file as a string
	{
		std::string s;
		if(LoadFileToBuffer<std::string>(processed_resource_path, file_type, s))
			return EvaluableNodeReference(enm->AllocNode(ENT_STRING, s), true);
		else
			return EvaluableNodeReference::Null();
	}
}

bool AssetManager::StoreResourcePath(EvaluableNode *code, std::string &resource_path,
	std::string &resource_base_path, std::string &file_type, EvaluableNodeManager *enm, bool escape_filename, bool sort_keys)
{
	//get file path based on the file being stored
	std::string path, file_base, extension;
	Platform_SeparatePathFileExtension(resource_path, path, file_base, extension);

	//escape the string if necessary, otherwise just use the regular one
	std::string processed_resource_path;
	if(escape_filename)
	{
		resource_base_path = path + FilenameEscapeProcessor::SafeEscapeFilename(file_base);
		processed_resource_path = resource_base_path + "." + extension;
	}
	else
	{
		resource_base_path = path + file_base;
		processed_resource_path = resource_path;
	}

	if(file_type == "")
		file_type = extension;

	//store the entity based on file_type
	if(file_type == FILE_EXTENSION_AMALGAM || file_type == FILE_EXTENSION_AMLG_METADATA)
	{
		std::ofstream outf(processed_resource_path, std::ios::out | std::ios::binary);
		if(!outf.good())
			return false;

		std::string code_string = Parser::Unparse(code, enm, true, true, sort_keys);
		outf.write(code_string.c_str(), code_string.size());
		outf.close();

		return true;
	}
	else if(file_type == FILE_EXTENSION_JSON)
		return EvaluableNodeJSONTranslation::Store(code, processed_resource_path, enm, sort_keys);
	else if(file_type == FILE_EXTENSION_YAML)
		return EvaluableNodeYAMLTranslation::Store(code, processed_resource_path, enm, sort_keys);
	else if(file_type == FILE_EXTENSION_CSV)
		return FileSupportCSV::Store(code, processed_resource_path, enm);
	else if(file_type == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
	{
		std::string code_string = Parser::Unparse(code, enm, false, true, sort_keys);

		//transforminto format needed for compression
		CompactHashMap<std::string, size_t> string_map;
		string_map[code_string] = 0;

		//compress and store
		BinaryData compressed_data = CompressStrings(string_map);
		if(StoreFileFromBuffer<BinaryData>(processed_resource_path, file_type, compressed_data))
			return EvaluableNodeReference(enm->AllocNode(ENT_TRUE), true);
		else
			return EvaluableNodeReference::Null();
	}
	else //binary string
	{
		std::string s = EvaluableNode::ToStringPreservingOpcodeType(code);
		if(StoreFileFromBuffer<std::string>(processed_resource_path, file_type, s))
			return EvaluableNodeReference(enm->AllocNode(ENT_TRUE), true);
		else
			return EvaluableNodeReference::Null();
	}

	return false;
}

Entity *AssetManager::LoadEntityFromResourcePath(std::string &resource_path, std::string &file_type,
	bool persistent, bool load_contained_entities, bool escape_filename, bool escape_contained_filenames,
	std::string default_random_seed, LoadEntityStatus &status)
{
	std::string resource_base_path;
	Entity *new_entity = new Entity();

	EvaluableNodeReference code = LoadResourcePath(resource_path, resource_base_path, file_type, &new_entity->evaluableNodeManager, escape_filename, status);
	if(code == nullptr || !status.loaded)
	{
		delete new_entity;
		return nullptr;
	}
	new_entity->SetRoot(code, true);

	//load any metadata like random seed
	std::string metadata_filename = resource_base_path + "." + FILE_EXTENSION_AMLG_METADATA;
	std::string metadata_base_path;
	std::string metadata_extension;
	EvaluableNode *metadata = LoadResourcePath(metadata_filename, metadata_base_path, metadata_extension, &new_entity->evaluableNodeManager, escape_filename, status);
	if(metadata != nullptr)
	{
		if(EvaluableNode::IsAssociativeArray(metadata))
		{
			EvaluableNode **seed = metadata->GetMappedChildNode(ENBISI_rand_seed);
			if(seed != nullptr)
				default_random_seed = EvaluableNode::ToStringPreservingOpcodeType(*seed);
		}
	}

	new_entity->SetRandomState(default_random_seed, true);

	if(persistent)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock lock(persistentEntitiesMutex);
	#endif
		persistentEntities[new_entity] = resource_path;
	}

	//load contained entities
	if(load_contained_entities)
	{
		//iterate over all files in directory
		resource_base_path.append("/");
		std::vector<std::string> file_names;
		Platform_GetFileNamesOfType(file_names, resource_base_path, file_type);
		for(auto &f : file_names)
		{
			std::string ce_path, ce_file_base, ce_extension;
			Platform_SeparatePathFileExtension(f, ce_path, ce_file_base, ce_extension);

			std::string entity_name;
			if(escape_contained_filenames)
				entity_name = FilenameEscapeProcessor::SafeUnescapeFilename(ce_file_base);
			else
				entity_name = ce_file_base;


			//don't escape filename again because it's already escaped in this loop
			std::string default_seed = new_entity->CreateRandomStreamFromStringAndRand(entity_name);
			std::string contained_resource_path = resource_base_path + ce_file_base + "." + ce_extension;
			Entity *contained_entity = LoadEntityFromResourcePath(contained_resource_path, file_type,
				false, true, false, escape_contained_filenames, default_seed, status);

			new_entity->AddContainedEntity(contained_entity, entity_name);
		}
	}

	return new_entity;
}

bool AssetManager::StoreEntityToResourcePath(Entity *entity, std::string &resource_path, std::string &file_type,
	bool update_persistence_location, bool store_contained_entities, bool escape_filename, bool escape_contained_filenames, bool sort_keys)
{
	if(entity == nullptr)
		return false;

	std::string resource_base_path;
	bool all_stored_successfully = AssetManager::StoreResourcePath(entity->GetRoot(),
		resource_path, resource_base_path, file_type, &entity->evaluableNodeManager, escape_filename, sort_keys);

	//store any metadata like random seed
	std::string metadata_filename = resource_base_path + "." + FILE_EXTENSION_AMLG_METADATA;
	EvaluableNode en_assoc(ENT_ASSOC);
	EvaluableNode en_rand_seed(ENT_STRING, entity->GetRandomState());
	en_assoc.SetMappedChildNode(ENBISI_rand_seed, &en_rand_seed);

	std::string metadata_base_path;
	std::string metadata_extension;
	//don't reescape the path here, since it has already been done
	StoreResourcePath(&en_assoc, metadata_filename, metadata_base_path, metadata_extension, &entity->evaluableNodeManager, false, sort_keys);

	//store contained entities
	if(store_contained_entities && entity->GetContainedEntities().size() > 0)
	{
		//create directory in case it doesn't exist
		std::filesystem::create_directories(resource_base_path);

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
			StoreEntityToResourcePath(contained_entity, new_resource_path, file_type, false, true, false, escape_contained_filenames, sort_keys);
		}
	}

	if(update_persistence_location)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock lock(persistentEntitiesMutex);
	#endif
		persistentEntities[entity] = resource_base_path + "." + file_type; //use escaped string
	}

	return all_stored_successfully;
}

void AssetManager::UpdateEntity(Entity *entity)
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
			StoreEntityToResourcePath(entity, new_path, extension, false, false, false, true, false);
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

void AssetManager::CreateEntity(Entity *entity)
{
	if(entity == nullptr)
		return;

#ifdef MULTITHREAD_INTERFACE
	Concurrency::ReadLock lock(persistentEntitiesMutex);
#endif
	//early out if no persistent entities
	if(persistentEntities.size() == 0)
		return;

	Entity *cur = entity->GetContainer();
	std::string slice_path;
	std::string filename;
	std::string extension = defaultEntityExtension;
	std::string traversal_path = "";
	std::string escaped_entity_id = FilenameEscapeProcessor::SafeEscapeFilename(entity->GetId());
	std::string id_suffix = "/" + escaped_entity_id + "." + defaultEntityExtension;
	while(cur != nullptr)
	{
		const auto &pe = persistentEntities.find(cur);
		if(pe != end(persistentEntities))
		{
			Platform_SeparatePathFileExtension(pe->second, slice_path, filename, extension);
			//create contained entity directory in case it doesn't currently exist
			std::string new_path = slice_path + filename + traversal_path;
			std::filesystem::create_directory(new_path);

			new_path += id_suffix;
			StoreEntityToResourcePath(entity, new_path, extension, false, true, false, true, false);
		}

		//don't need to continue and allocate extra traversal path if already at outermost entity
		Entity *cur_container = cur->GetContainer();
		if(cur_container == nullptr)
			break;

		escaped_entity_id = FilenameEscapeProcessor::SafeEscapeFilename(cur->GetId());
		traversal_path = "/" + escaped_entity_id + traversal_path;
		cur = cur_container;
	}
}

void AssetManager::SetRootPermission(Entity *entity, bool permission)
{
	if(entity == nullptr)
		return;

#ifdef MULTITHREAD_INTERFACE
	Concurrency::WriteLock lock(rootEntitiesMutex);
#endif

	if(permission)
		rootEntities.insert(entity);
	else
		rootEntities.erase(entity);
}

std::string AssetManager::GetEvaluableNodeSourceFromComments(EvaluableNode *en)
{
	std::string source;
	if(asset_manager.debugSources)
	{
		if(en->HasComments())
		{
			auto comment = en->GetCommentsString();
			auto first_line_end = comment.find('\n');
			if(first_line_end == std::string::npos)
				source = comment;
			else //copy up until newline
			{
				source = comment.substr(0, first_line_end);
				if(source.size() > 0 && source.back() == '\r')
					source.pop_back();
			}

			source += ": ";
		}
	}

	return source;
}

void AssetManager::DestroyPersistentEntity(Entity *entity)
{
	Entity *cur = entity;
	std::string slice_path;
	std::string filename;
	std::string extension;
	std::string traversal_path;

	//remove it as a persistent entity if it happened to be a direct one (erase won't do anything if it doesn't exist)
	persistentEntities.erase(entity);

	//delete any contained entities that are persistent
	for(auto contained_entity : entity->GetContainedEntities())
		DestroyPersistentEntity(contained_entity);

	//cover the case if any of this entity's containers were also persisted entities
	while(cur != nullptr)
	{
		const auto &pe = persistentEntities.find(cur);
		if(pe != end(persistentEntities))
		{
			//get metadata filename
			Platform_SeparatePathFileExtension(pe->second, slice_path, filename, extension);
			std::string total_filepath = slice_path + filename + traversal_path;

			//delete files
			std::filesystem::remove(total_filepath + "." + defaultEntityExtension);
			std::filesystem::remove(total_filepath + "." + FILE_EXTENSION_AMLG_METADATA);

			//remove directory and all contents if it exists (command will fail if it doesn't exist)
			std::filesystem::remove_all(total_filepath);
		}

		std::string escaped_entity_id = FilenameEscapeProcessor::SafeEscapeFilename(cur->GetId());
		traversal_path = "/" + escaped_entity_id + traversal_path;

		cur = cur->GetContainer();
	}
}

void AssetManager::RemoveRootPermissions(Entity *entity)
{
	//remove permissions on any contained entities
	for(auto contained_entity : entity->GetContainedEntities())
		RemoveRootPermissions(contained_entity);

	SetRootPermission(entity, false);
}
