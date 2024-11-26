//project headers:
#include "AssetManager.h"

#include "Amalgam.h"
#include "BinaryPacking.h"
#include "EntityExternalInterface.h"
#include "EvaluableNode.h"
#include "FileSupportCSV.h"
#include "FileSupportJSON.h"
#include "FileSupportYAML.h"
#include "Interpreter.h"
#include "PlatformSpecific.h"

//system headers:
#include <ctime>
#include <fstream>
#include <iostream>

AssetManager asset_manager;

AssetManager::AssetParameters::AssetParameters(std::string resource_path, std::string file_type, bool is_entity)
{
	resourcePath = resource_path;
	resourceType = file_type;

	if(resourceType == "")
	{
		std::string path, file_base;
		Platform_SeparatePathFileExtension(resourcePath, path, file_base, resourceType);
	}

	if(resourceType == FILE_EXTENSION_AMLG_METADATA || resourceType == FILE_EXTENSION_AMALGAM)
	{
		includeRandSeeds = false;
		escapeResourceName = false;
		escapeContainedResourceNames = true;
		transactional = false;
		prettyPrint = true;
		sortKeys = true;
		flatten = false;
		parallelCreate = false;
		executeOnLoad = false;
	}
	else if(resourceType == FILE_EXTENSION_JSON || resourceType == FILE_EXTENSION_YAML
		|| resourceType == FILE_EXTENSION_CSV)
	{
		includeRandSeeds = false;
		escapeResourceName = false;
		escapeContainedResourceNames = false;
		transactional = false;
		prettyPrint = false;
		sortKeys = true;
		flatten = false;
		parallelCreate = false;
		executeOnLoad = false;
	}
	else if(resourceType == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
	{
		includeRandSeeds = is_entity;
		escapeResourceName = false;
		escapeContainedResourceNames = false;
		transactional = false;
		prettyPrint = false;
		sortKeys = false;
		flatten = is_entity;
		parallelCreate = false;
		executeOnLoad = is_entity;
	}
	else
	{
		includeRandSeeds = is_entity;
		escapeResourceName = false;
		escapeContainedResourceNames = false;
		transactional = false;
		prettyPrint = false;
		sortKeys = false;
		flatten = is_entity;
		parallelCreate = false;
		executeOnLoad = is_entity;
	}
}

void AssetManager::AssetParameters::SetParams(EvaluableNode::AssocType &params)
{
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_include_rand_seeds, includeRandSeeds);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_escape_resource_name, escapeResourceName);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_escape_contained_resource_names, escapeContainedResourceNames);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_transactional, transactional);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_pretty_print, prettyPrint);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_sort_keys, sortKeys);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_flatten, flatten);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_parallel_create, parallelCreate);
	EvaluableNode::GetValueFromMappedChildNodesReference(params, ENBISI_execute_on_load, executeOnLoad);
}

void AssetManager::AssetParameters::UpdateResources()
{
	//get file path based on the file being stored
	std::string path, file_base;
	Platform_SeparatePathFileExtension(resourcePath, path, file_base, extension);

	//escape the string if necessary, otherwise just use the regular one
	if(escapeResourceName)
	{
		resourceBasePath = path + FilenameEscapeProcessor::SafeEscapeFilename(file_base);
		resourcePath = resourceBasePath + "." + extension;
	}
	else //resourcePath stays the same
	{
		resourceBasePath = path + file_base;
	}
}

EvaluableNodeReference AssetManager::LoadResource(AssetParameters &asset_params, EvaluableNodeManager *enm,
	EntityExternalInterface::LoadEntityStatus &status)
{
	//load this entity based on file_type
	if(asset_params.resourceType == FILE_EXTENSION_AMALGAM || asset_params.resourceType == FILE_EXTENSION_AMLG_METADATA)
	{
		auto [code, code_success] = Platform_OpenFileAsString(asset_params.resourcePath);
		if(!code_success)
		{
			status.SetStatus(false, code);
			if(asset_params.resourceType == FILE_EXTENSION_AMALGAM)
				std::cerr << code << std::endl;
			return EvaluableNodeReference::Null();
		}

		StringManipulation::RemoveBOMFromUTF8String(code);

		auto [node, warnings, char_with_error] = Parser::Parse(code, enm, asset_params.transactional, &asset_params.resourcePath, debugSources);
		for(auto &w : warnings)
			std::cerr << w << std::endl;
		return node;
	}
	else if(asset_params.resourceType == FILE_EXTENSION_JSON)
	{
		return EvaluableNodeReference(EvaluableNodeJSONTranslation::Load(asset_params.resourcePath, enm, status), true);
	}
	else if(asset_params.resourceType == FILE_EXTENSION_YAML)
	{
		return EvaluableNodeReference(EvaluableNodeYAMLTranslation::Load(asset_params.resourcePath, enm, status), true);
	}
	else if(asset_params.resourceType == FILE_EXTENSION_CSV)
	{
		return EvaluableNodeReference(FileSupportCSV::Load(asset_params.resourcePath, enm, status), true);
	}
	else if(asset_params.resourceType == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
	{
		BinaryData compressed_data;
		auto [error_msg, version, success] = LoadFileToBuffer<BinaryData>(asset_params.resourcePath, asset_params.resourceType, compressed_data);
		if(!success)
		{
			status.SetStatus(false, error_msg, version);
			return EvaluableNodeReference::Null();
		}

		OffsetIndex cur_offset = 0;
		auto strings = DecompressStrings(compressed_data, cur_offset);
		if(strings.size() == 0)
			return EvaluableNodeReference::Null();

		auto [node, warnings, char_with_error] = Parser::Parse(strings[0], enm, asset_params.transactional,
			&asset_params.resourcePath, debugSources);
		for(auto &w : warnings)
			std::cerr << w << std::endl;
		return node;
	}
	else //just load the file as a string
	{
		std::string s;
		auto [error_msg, version, success] = LoadFileToBuffer<std::string>(asset_params.resourcePath, asset_params.resourceType, s);
		if(success)
			return EvaluableNodeReference(enm->AllocNode(ENT_STRING, s), true);
		else
		{
			status.SetStatus(false, error_msg, version);
			return EvaluableNodeReference::Null();
		}
	}
}

bool AssetManager::LoadResourceViaTransactionalExecution(AssetParameters &asset_params, Entity *entity,
	Interpreter *calling_interpreter, EntityExternalInterface::LoadEntityStatus &status)
{
	std::string code_string;
	if(asset_params.resourceType == FILE_EXTENSION_AMALGAM)
	{
		bool code_success = false;
		std::tie(code_string, code_success) = Platform_OpenFileAsString(asset_params.resourcePath);
		if(!code_success)
		{
			status.SetStatus(false, code_string);
			if(asset_params.resourceType == FILE_EXTENSION_AMALGAM)
				std::cerr << code_string << std::endl;
			return false;
		}
	}
	else if(asset_params.resourceType == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
	{
		BinaryData compressed_data;
		auto [error_msg, version, success] = LoadFileToBuffer<BinaryData>(asset_params.resourcePath, asset_params.resourceType, compressed_data);
		if(!success)
		{
			status.SetStatus(false, error_msg, version);
			return false;
		}

		OffsetIndex cur_offset = 0;
		auto strings = DecompressStrings(compressed_data, cur_offset);
		if(strings.size() == 0)
			return false;

		code_string = std::move(strings[0]);
	}

	StringManipulation::RemoveBOMFromUTF8String(code_string);

	Parser parser(code_string, &entity->evaluableNodeManager, true, &asset_params.resourcePath, debugSources);
	auto [first_node, first_node_warnings, first_node_char_with_error] = parser.ParseFirstNode();
	for(auto &w : first_node_warnings)
		std::cerr << w << std::endl;

	//make sure it's a valid executable type
	if(EvaluableNode::IsNull(first_node) || !first_node->IsOrderedArray())
		return false;

	EvaluableNodeReference args = EvaluableNodeReference(entity->evaluableNodeManager.AllocNode(ENT_ASSOC), true);
	args->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_create_new_entity), entity->evaluableNodeManager.AllocNode(false));
	auto call_stack = Interpreter::ConvertArgsToCallStack(args, entity->evaluableNodeManager);

	auto first_node_type = first_node->GetType();
	if(first_node_type == ENT_LET || first_node_type == ENT_DECLARE)
	{
		auto [assoc_node, assoc_warnings, assoc_char_with_error] = parser.ParseNextTransactionalBlock();
		for(auto &w : assoc_warnings)
			std::cerr << w << std::endl;

		if(!EvaluableNode::IsNull(first_node) && first_node->IsAssociativeArray())
		{
			if(first_node_type == ENT_LET)
			{
				call_stack->AppendOrderedChildNode(assoc_node);
			}
			else //first_node_type == ENT_DECLARE
			{
				first_node->AppendOrderedChildNode(assoc_node);
				entity->ExecuteCodeAsEntity(first_node, call_stack, calling_interpreter);
			}
		}
	}

	entity->evaluableNodeManager.FreeNode(first_node);

	while(!parser.ParsedAllTransactionalBlocks())
	{
		auto [node, warnings, char_with_error] = parser.ParseNextTransactionalBlock();
		for(auto &w : warnings)
			std::cerr << w << std::endl;

		entity->ExecuteCodeAsEntity(node, call_stack, calling_interpreter);
	}
	
	entity->evaluableNodeManager.FreeNode(call_stack->GetOrderedChildNodesReference()[0]);
	entity->evaluableNodeManager.FreeNode(call_stack);

	return true;
}

bool AssetManager::StoreResource(EvaluableNode *code, AssetParameters &asset_params, EvaluableNodeManager *enm)
{
	//store the entity based on file_type
	if(asset_params.resourceType == FILE_EXTENSION_AMALGAM || asset_params.resourceType == FILE_EXTENSION_AMLG_METADATA)
	{
		std::ofstream outf(asset_params.resourcePath, std::ios::out | std::ios::binary);
		if(!outf.good())
			return false;

		std::string code_string = Parser::Unparse(code, asset_params.prettyPrint, true, asset_params.sortKeys);
		outf.write(code_string.c_str(), code_string.size());
		outf.close();

		return true;
	}
	else if(asset_params.resourceType == FILE_EXTENSION_JSON)
	{
		return EvaluableNodeJSONTranslation::Store(code, asset_params.resourcePath, enm, asset_params.sortKeys);
	}
	else if(asset_params.resourceType == FILE_EXTENSION_YAML)
	{
		return EvaluableNodeYAMLTranslation::Store(code, asset_params.resourcePath, enm, asset_params.sortKeys);
	}
	else if(asset_params.resourceType == FILE_EXTENSION_CSV)
	{
		return FileSupportCSV::Store(code, asset_params.resourcePath, enm);
	}
	else if(asset_params.resourceType == FILE_EXTENSION_COMPRESSED_AMALGAM_CODE)
	{
		std::string code_string = Parser::Unparse(code, asset_params.prettyPrint, true, asset_params.sortKeys);

		//transform into format needed for compression
		CompactHashMap<std::string, size_t> string_map;
		string_map[code_string] = 0;

		//compress and store
		BinaryData compressed_data = CompressStrings(string_map);
		return StoreFileFromBuffer<BinaryData>(asset_params.resourcePath, asset_params.resourceType, compressed_data);
	}
	else //binary string
	{
		if(code == nullptr || code->GetType() != ENT_STRING)
			return false;

		const std::string &s = code->GetStringValue();
		return StoreFileFromBuffer<std::string>(asset_params.resourcePath, asset_params.resourceType, s);
	}

	return false;
}

Entity *AssetManager::LoadEntityFromResource(AssetParameters &asset_params, bool persistent,
	std::string default_random_seed, Interpreter *calling_interpreter, EntityExternalInterface::LoadEntityStatus &status)
{
	Entity *new_entity = new Entity();
	new_entity->SetRandomState(default_random_seed, true);

	if(asset_params.executeOnLoad && asset_params.transactional)
	{
		if(!LoadResourceViaTransactionalExecution(asset_params, new_entity, calling_interpreter, status))
		{
			delete new_entity;
			return nullptr;
		}

		if(persistent)
		{
			delete new_entity;
			return nullptr;
		}
			//SetEntityPersistenceForFlattenedEntity(new_entity, &asset_params);

		return new_entity;
	}

	EvaluableNodeReference code = LoadResource(asset_params, &new_entity->evaluableNodeManager, status);

	if(!status.loaded)
	{
		delete new_entity;
		return nullptr;
	}

	if(asset_params.executeOnLoad)
	{
		EvaluableNodeReference args = EvaluableNodeReference(new_entity->evaluableNodeManager.AllocNode(ENT_ASSOC), true);
		args->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_create_new_entity), new_entity->evaluableNodeManager.AllocNode(false));
		auto call_stack = Interpreter::ConvertArgsToCallStack(args, new_entity->evaluableNodeManager);

		new_entity->ExecuteCodeAsEntity(code, call_stack, calling_interpreter);

		new_entity->evaluableNodeManager.FreeNode(call_stack->GetOrderedChildNodesReference()[0]);
		new_entity->evaluableNodeManager.FreeNode(call_stack);

		if(persistent)
		{
			delete new_entity;
			return nullptr;
		}
		//	SetEntityPersistenceForFlattenedEntity(new_entity, &asset_params);

		return new_entity;
	}

	new_entity->SetRoot(code, true);

	if(asset_params.resourceType == FILE_EXTENSION_AMALGAM)
	{
		//load any metadata like random seed
		AssetParameters metadata_asset_params = asset_params.CreateAssetParametersForAssociatedResource(FILE_EXTENSION_AMLG_METADATA);
		EntityExternalInterface::LoadEntityStatus metadata_status;
		EvaluableNodeReference metadata = LoadResource(metadata_asset_params, &new_entity->evaluableNodeManager, metadata_status);
		if(metadata_status.loaded)
		{
			if(EvaluableNode::IsAssociativeArray(metadata))
			{
				EvaluableNode **seed = metadata->GetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_rand_seed));
				if(seed != nullptr && (*seed)->GetType() == ENT_STRING)
				{
					default_random_seed = (*seed)->GetStringValue();
					new_entity->SetRandomState(default_random_seed, true);
				}

				EvaluableNode **version = metadata->GetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_version));
				if(version != nullptr && (*version)->GetType() == ENT_STRING)
				{
					const std::string &version_str = (*version)->GetStringValue();
					auto [error_message, success] = AssetManager::ValidateVersionAgainstAmalgam(version_str);
					if(!success)
					{
						status.SetStatus(false, error_message, version_str);
						delete new_entity;
						return nullptr;
					}
				}
			}

			new_entity->evaluableNodeManager.FreeNodeTree(metadata);
		}
	}

	if(persistent)
		SetEntityPersistence(new_entity, &asset_params);

	//load contained entities

	//iterate over all files in directory
	std::string contained_entities_directory = asset_params.resourceBasePath + "/";
	std::vector<std::string> file_names;
	Platform_GetFileNamesOfType(file_names, contained_entities_directory, asset_params.extension);
	for(auto &f : file_names)
	{
		std::string ce_path, ce_file_base, ce_extension;
		Platform_SeparatePathFileExtension(f, ce_path, ce_file_base, ce_extension);

		std::string entity_name;
		if(asset_params.escapeContainedResourceNames)
			entity_name = FilenameEscapeProcessor::SafeUnescapeFilename(ce_file_base);
		else
			entity_name = ce_file_base;

		std::string default_seed = new_entity->CreateRandomStreamFromStringAndRand(entity_name);

		std::string ce_resource_base_path = contained_entities_directory + ce_file_base;
		AssetParameters ce_asset_params
			= asset_params.CreateAssetParametersForContainedResourceByResourceBasePath(ce_resource_base_path);
		
		Entity *contained_entity = LoadEntityFromResource(ce_asset_params, persistent,
			default_seed, calling_interpreter, status);

		if(!status.loaded)
		{
			delete new_entity;
			return nullptr;
		}

		new_entity->AddContainedEntity(contained_entity, entity_name);
	}

	return new_entity;
}

void AssetManager::CreateEntity(Entity *entity)
{
	if(entity == nullptr)
		return;

#ifdef MULTITHREAD_INTERFACE
	Concurrency::ReadLock lock(persistentEntitiesMutex);
#endif

	Entity *container = entity->GetContainer();
	auto pe_entry = persistentEntities.find(container);
	if(pe_entry == end(persistentEntities))
		return;
	auto &container_asset_params = *pe_entry->second;

	//if flattened, then just need to update it or the appropriate container
	if(container_asset_params.flatten)
	{
		UpdateEntity(container);
	}
	else
	{
		AssetParameters ce_asset_params
			= container_asset_params.CreateAssetParametersForContainedResourceByEntityId(entity->GetId());

		EnsureEntityToResourceCanContainEntities(container_asset_params);
		StoreEntityToResource(entity, ce_asset_params, true, true, false);
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

std::pair<std::string, bool> AssetManager::ValidateVersionAgainstAmalgam(const std::string &version)
{
	auto sem_ver = StringManipulation::Split(version, '-'); //split on postfix
	auto version_split = StringManipulation::Split(sem_ver[0], '.'); //ignore postfix
	if(version_split.size() != 3)
		return std::make_pair("Invalid version number", false);

	uint32_t major = atoi(version_split[0].c_str());
	uint32_t minor = atoi(version_split[1].c_str());
	uint32_t patch = atoi(version_split[2].c_str());
	auto dev_build = std::string(AMALGAM_VERSION_SUFFIX);
	if(!dev_build.empty()
		|| (AMALGAM_VERSION_MAJOR == 0 && AMALGAM_VERSION_MINOR == 0 && AMALGAM_VERSION_PATCH == 0))
	{
		// dev builds don't check versions
	}
	else if(major == 0 && minor == 0 && patch == 0)
	{
		std::string warn_msg = "Warning: parsing Amalgam generated from an unversioned debug build";
		std::cerr << warn_msg << ", version=" << version << std::endl;
	}
	else if(
		(major > AMALGAM_VERSION_MAJOR) ||
		(major == AMALGAM_VERSION_MAJOR && minor > AMALGAM_VERSION_MINOR) ||
		(major == AMALGAM_VERSION_MAJOR && minor == AMALGAM_VERSION_MINOR && patch > AMALGAM_VERSION_PATCH))
	{
		std::string err_msg = "Parsing Amalgam that is more recent than the current version is not supported";
		std::cerr << err_msg << ", version=" << version << std::endl;
		return std::make_pair(err_msg, false);
	}
	else if(AMALGAM_VERSION_MAJOR > major)
	{
		std::string err_msg = "Parsing Amalgam that is older than the current major version is not supported";
		std::cerr << err_msg << ", version=" << version << std::endl;
		return std::make_pair(err_msg, false);
	}

	return std::make_pair("", true);
}

std::string AssetManager::GetEvaluableNodeSourceFromComments(EvaluableNode *en)
{
	std::string source;
	if(asset_manager.debugSources)
	{
		if(en->HasComments())
		{
			auto &comment = en->GetCommentsString();
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
	auto pe_entry = persistentEntities.find(entity);
	if(pe_entry == end(persistentEntities))
		return;
	auto &asset_params = *pe_entry->second;

	//if flattened, then just need to update it or the appropriate container
	if(asset_params.flatten)
	{
		UpdateEntity(entity);
	}
	else
	{
		std::error_code ec;

		//delete files
		std::filesystem::remove(asset_params.resourcePath, ec);
		if(ec)
			std::cerr << "Could not remove file: " << asset_params.resourcePath << std::endl;

		if(asset_params.resourceType == FILE_EXTENSION_AMALGAM)
			std::filesystem::remove(asset_params.resourceBasePath + "." + FILE_EXTENSION_AMLG_METADATA, ec);

		//remove directory and all contents if it exists
		std::filesystem::remove_all(asset_params.resourceBasePath, ec);

		DeepClearEntityPersistenceRecurse(entity);
	}
}

void AssetManager::RemoveRootPermissions(Entity *entity)
{
	//remove permissions on any contained entities
	for(auto contained_entity : entity->GetContainedEntities())
		RemoveRootPermissions(contained_entity);

	SetRootPermission(entity, false);
}
