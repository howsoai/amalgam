//project headers:
#include "EntityExternalInterface.h"

#include "AssetManager.h"
#include "Entity.h"
#include "EntityWriteListener.h"
#include "FileSupportCAML.h"
#include "FileSupportJSON.h"
#include "Interpreter.h"

//system headers:
#include <string>
#include <vector>

EntityExternalInterface::LoadEntityStatus::LoadEntityStatus()
{
	SetStatus(true);
}

EntityExternalInterface::LoadEntityStatus::LoadEntityStatus(bool loaded, std::string message, std::string version)
{
	SetStatus(loaded, message, version);
}

void EntityExternalInterface::LoadEntityStatus::SetStatus(bool loaded_in, std::string message_in, std::string version_in)
{
	loaded = loaded_in;
	message = std::move(message_in);
	version = std::move(version_in);
}

// Convert an API-level entity path as a vector of string to an Amalgam node.
// The returned node is owned by the passed node manager.
static EvaluableNode *AssembleEntityPath(EvaluableNodeManager &enm, const std::vector<std::string> entity_path)
{
	EvaluableNode *id_path = enm.AllocNode(ENT_LIST);
	for(std::string path_item : entity_path)
	{
		EvaluableNode *id_path_item = enm.AllocNode(ENT_STRING, path_item);
		id_path->AppendOrderedChildNode(id_path_item);
	}
	return id_path;
}

EntityExternalInterface::LoadEntityStatus EntityExternalInterface::LoadEntity(std::string &handle, const EntityExternalInterface::LoadSource &source,
	std::string file_type, bool persistent, std::string_view json_file_params,
	std::string &write_log_filename, std::string &print_log_filename, const std::vector<std::string> &entity_path, std::string rand_seed)
{
	LoadEntityStatus status;
	// If loading into a sub-entity:
	// Existing entity to replace if id_sid is invalid, or existing parent container
	EntityReadReference container;
	// If valid, container is the parent entity and this is the name of a new child
	StringRef id_sid;

	if(!entity_path.empty())
	{
		// We're trying to load into an embedded entity.  First find the existing root
		auto bundle = FindEntityBundle(handle);
		if(bundle == nullptr || bundle->entity == nullptr)
		{
			status.SetStatus(false, "root entity does not exist");
			return status;
		}
		// Then look up the entity path within the root
		EvaluableNode *id_path = AssembleEntityPath(bundle->entity->evaluableNodeManager, entity_path);
		EntityReadReference always_null;
		std::tie(always_null, container) = TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityReadReference>(bundle->entity, id_path, &id_sid);
		bundle->entity->evaluableNodeManager.FreeNodeTree(id_path);
		if(container == nullptr)
		{
			// The parent node (that is, the path up to but not including the last element) doesn't exist.
			status.SetStatus(false, "invalid entity path");
			return status;
		}
		// If id_sid is invalid, then container is the node itself.
		// If id_sid is valid, then container is the parent.
	}

	if(rand_seed.empty())
	{
		rand_seed.resize(RandomStream::randStateStringifiedSizeInBytes);
		Platform_GenerateSecureRandomData(rand_seed.data(), RandomStream::randStateStringifiedSizeInBytes);
	}

	AssetManager::AssetParametersRef asset_params;
	if(std::holds_alternative<std::string>(source))
		asset_params = std::make_shared<AssetManager::AssetParameters>(std::get<std::string>(source), file_type, true);
	else
	{
		auto [data, len] = std::get<std::pair<void *, size_t>>(source);
		char *cdata = reinterpret_cast<char *>(data);
		asset_params = std::make_shared<AssetManager::AssetParameters>(std::string(), file_type, true);
		asset_params->resourceContents.assign(cdata, cdata + len);
		asset_params->toMemory = true;
	}

	if(json_file_params.size() > 0)
	{
		EvaluableNodeManager temp_enm;
		EvaluableNode *file_params = EvaluableNodeJSONTranslation::JsonToEvaluableNode(&temp_enm, json_file_params);

		if(EvaluableNode::IsAssociativeArray(file_params))
			asset_params->SetParams(file_params->GetMappedChildNodesReference());
	}

	asset_params->UpdateResources();

	Entity *entity = asset_manager.LoadEntityFromResource(asset_params, persistent, rand_seed, nullptr, status);

	if(!status.loaded)
		return status;

	PrintListener *pl = nullptr;
	std::vector<EntityWriteListener *> wl;

	if(!print_log_filename.empty())
		pl = new PrintListener(print_log_filename);

	if(!write_log_filename.empty())
	{
		std::unique_ptr<std::ostream> log_file = std::make_unique<std::ofstream>(write_log_filename, std::ios::binary);
		EntityWriteListener *write_log = new EntityWriteListener(entity, std::move(log_file), false, false, false);
		wl.push_back(write_log);
	}

	if(container != nullptr)
	{
		// assigning to a child entity
		// Are we replacing an existing entity?
		if(id_sid == StringInternPool::NOT_A_STRING_ID)
		{
			// We need to find the parent, then delete the child.
			id_sid.SetIDAndCreateReference(container->GetIdStringId());
			Entity *parent = container->GetContainer();
			if(parent == nullptr)
			{
				// This should never happen
				status.SetStatus(false, "embedded entity does not have a parent");
				delete entity;
				return status;
			}
			container = EntityReadReference(parent);
			parent->RemoveContainedEntity(id_sid, &wl);
		}
		// Now the parent definitely does not contain the child, and the ID is definitely valid, so add it
		container->AddContainedEntity(entity, static_cast<StringInternPool::StringID>(id_sid), &wl);
	}
	else
		// new top-level entity
		AddEntityBundle(handle, new EntityListenerBundle(entity, wl, pl));

	return status;
}

EntityExternalInterface::LoadEntityStatus EntityExternalInterface::VerifyEntity(std::string &path)
{
	auto [error_string, version, success] = AssetManager::GetFileStatus(path);
	if(!success)
		return EntityExternalInterface::LoadEntityStatus(false, error_string, version);

	return EntityExternalInterface::LoadEntityStatus(true, "", version);
}

std::string EntityExternalInterface::GetEntityPermissions(std::string &handle)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return "null";

	Entity *entity = bundle->entity;
	if(entity == nullptr)
		return "null";

	auto permissions = asset_manager.GetEntityPermissions(entity);
	auto permissions_en = permissions.GetPermissionsAsEvaluableNode(&entity->evaluableNodeManager);

	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(permissions_en);

	entity->evaluableNodeManager.FreeNodeTree(permissions_en);
	if(converted)
		return result;
	return "null";
}

bool EntityExternalInterface::SetEntityPermissions(std::string &handle, std::string &json_permissions)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return false;

	Entity *entity = bundle->entity;
	if(entity == nullptr)
		return false;

	EvaluableNode *permissions_en = EvaluableNodeJSONTranslation::JsonToEvaluableNode(
		&entity->evaluableNodeManager, json_permissions);

	auto [permissions_to_set, permission_values] = EntityPermissions::EvaluableNodeToPermissions(permissions_en);
	entity->SetPermissions(permissions_to_set, permission_values, true);
	entity->evaluableNodeManager.FreeNodeTree(permissions_en);
	return true;
}

bool EntityExternalInterface::CloneEntity(std::string &handle, std::string &cloned_handle, std::string &path,
	std::string file_type, bool persistent, std::string_view json_file_params,
	std::string &write_log_filename, std::string &print_log_filename)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return false;

	if(bundle->entity == nullptr)
		return false;

	Entity *entity = new Entity(bundle->entity);

	AssetManager::AssetParametersRef asset_params
		= std::make_shared<AssetManager::AssetParameters>(path, file_type, true);

	if(json_file_params.size() > 0)
	{
		auto &enm = bundle->entity->evaluableNodeManager;
		EvaluableNode *file_params = EvaluableNodeJSONTranslation::JsonToEvaluableNode(&enm, json_file_params);

		if(EvaluableNode::IsAssociativeArray(file_params))
			asset_params->SetParams(file_params->GetMappedChildNodesReference());
	}

	asset_params->UpdateResources();

	PrintListener *pl = nullptr;
	std::vector<EntityWriteListener *> wl;

	if(!print_log_filename.empty())
		pl = new PrintListener(print_log_filename);

	if(!write_log_filename.empty())
	{
		std::unique_ptr<std::ostream> log_file = std::make_unique<std::ofstream>(write_log_filename, std::ios::binary);
		EntityWriteListener *write_log = new EntityWriteListener(entity, std::move(log_file), false, false, false);
		wl.push_back(write_log);
	}

	AddEntityBundle(cloned_handle, new EntityListenerBundle(entity, wl, pl));

	if(persistent)
		asset_manager.StoreEntityToResource(entity, asset_params, true, persistent);

	return true;
}

void EntityExternalInterface::StoreEntity(std::string &handle, const EntityExternalInterface::StoreSource &source,
	std::string file_type, bool persistent, std::string_view json_file_params, const std::vector<std::string> &entity_path)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr || bundle->entity == nullptr)
		return;

	EntityReadReference entity(bundle->entity);
	if(entity_path.empty())
		entity = bundle->entity;
	else
	{
		// We're actually working on an embedded entity.
		EvaluableNode *id_path = AssembleEntityPath(bundle->entity->evaluableNodeManager, entity_path);
		// Retrieve it.
		entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(bundle->entity, id_path);
		bundle->entity->evaluableNodeManager.FreeNodeTree(id_path);
		if(entity == nullptr)
			// ...didn't find it.
			return;
	}

	AssetManager::AssetParametersRef asset_params;
	if(std::holds_alternative<std::string>(source))
		asset_params = std::make_shared<AssetManager::AssetParameters>(std::get<std::string>(source), file_type, true);
	else
	{
		asset_params = std::make_shared<AssetManager::AssetParameters>(std::string(), file_type, true);
		asset_params->flatten = true;
		asset_params->toMemory = true;
	}

	if(json_file_params.size() > 0)
	{
		auto &enm = bundle->entity->evaluableNodeManager;
		EvaluableNode *file_params = EvaluableNodeJSONTranslation::JsonToEvaluableNode(&enm, json_file_params);

		if(EvaluableNode::IsAssociativeArray(file_params))
			asset_params->SetParams(file_params->GetMappedChildNodesReference());

		enm.FreeNodeTree(file_params);
	}
	asset_params->UpdateResources();

	asset_manager.StoreEntityToResource(entity, asset_params, true, persistent);
	if(std::holds_alternative<std::pair<void **, size_t *>>(source))
	{
		auto [datap, lenp] = std::get<std::pair<void **, size_t *>>(source);
		std::string &result = asset_params->resourceContents;
		*datap = new char[result.size() + 1];
		memcpy(*datap, result.c_str(), result.size() + 1);
		*lenp = result.size();
	}
}

void EntityExternalInterface::ExecuteEntity(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	bundle->entity->Execute(label, nullptr, false, nullptr, &bundle->writeListeners, bundle->printListener);
}

void EntityExternalInterface::DestroyEntity(std::string &handle)
{
	EraseEntityBundle(handle);
}

bool EntityExternalInterface::SetRandomSeed(std::string &handle, std::string &rand_seed)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return false;

	if(bundle->entity == nullptr)
		return false;

	bundle->entity->SetRandomState(rand_seed, true, &bundle->writeListeners);
	return true;
}

std::vector<std::string> EntityExternalInterface::GetEntities()
{
	std::vector<std::string> entities;
#ifdef MULTITHREAD_INTERFACE
	Concurrency::ReadLock read_lock(mutex);
#endif

	entities.reserve(handleToBundle.size());
	for(auto &[bundle_handle, _] : handleToBundle)
		entities.push_back(bundle_handle);

	return entities;
}

bool EntityExternalInterface::SetJSONToLabel(std::string &handle, std::string &label, std::string_view json)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return false;

	EvaluableNode *node = EvaluableNodeJSONTranslation::JsonToEvaluableNode(&bundle->entity->evaluableNodeManager, json);
	EvaluableNodeReference node_reference(node, true);
	bool success = bundle->SetEntityValueAtLabel(label, node_reference);
	return success;
}

std::string EntityExternalInterface::GetJSONFromLabel(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return "";

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, nullptr, false).first;
	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(label_val);
	return (converted ? result : string_intern_pool.GetStringFromID(string_intern_pool.NOT_A_STRING_ID));
}

std::string EntityExternalInterface::ExecuteEntityJSON(std::string &handle, std::string &label, std::string_view json)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return "";

	EvaluableNodeManager &enm = bundle->entity->evaluableNodeManager;
#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating scope stack
	Concurrency::ReadLock enm_lock(enm.memoryModificationMutex);
#endif
	EvaluableNodeReference args = EvaluableNodeReference(EvaluableNodeJSONTranslation::JsonToEvaluableNode(&enm, json), true);

	auto scope_stack = Interpreter::ConvertArgsToScopeStack(args, enm);

	EvaluableNodeReference returned_value = bundle->entity->Execute(label, scope_stack, false, nullptr,
		&bundle->writeListeners, bundle->printListener, nullptr
#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
#endif
	);

	enm.FreeNode(args);
	enm.FreeNode(scope_stack);

	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(returned_value);
	enm.FreeNodeTreeIfPossible(returned_value);
	return (converted ? result : string_intern_pool.GetStringFromID(string_intern_pool.NOT_A_STRING_ID));
}

std::pair<std::string, std::string> EntityExternalInterface::ExecuteEntityJSONLogged(const std::string &handle, const std::string &label, std::string_view json)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return std::pair("", "");

	EntityWriteListener logger(bundle->entity, std::unique_ptr<std::ostream>(), true);
	std::vector<EntityWriteListener *> listeners(bundle->writeListeners);
	listeners.push_back(&logger);

	EvaluableNodeManager &enm = bundle->entity->evaluableNodeManager;
#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating scope stack
	Concurrency::ReadLock enm_lock(enm.memoryModificationMutex);
#endif
	EvaluableNodeReference args = EvaluableNodeReference(EvaluableNodeJSONTranslation::JsonToEvaluableNode(&enm, json), true);

	auto scope_stack = Interpreter::ConvertArgsToScopeStack(args, enm);

	EvaluableNodeReference returned_value = bundle->entity->Execute(label, scope_stack, false, nullptr,
		&listeners, bundle->printListener, nullptr
#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
#endif
	);
	enm.FreeNode(args);
	enm.FreeNode(scope_stack);

	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(returned_value);
	enm.FreeNodeTreeIfPossible(returned_value);
	std::string json_out = converted ? result : string_intern_pool.GetStringFromID(string_intern_pool.NOT_A_STRING_ID);

	std::string log = Parser::Unparse(logger.GetWrites(), false);

	return std::pair(json_out, log);
}

std::string EntityExternalInterface::EvalOnEntity(const std::string &handle, const std::string &amlg)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return "";

	EvaluableNodeManager &enm = bundle->entity->evaluableNodeManager;
#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating scope stack
	Concurrency::ReadLock enm_lock(enm.memoryModificationMutex);
#endif

	auto [code, warnings, offset] = Parser::Parse(amlg, &enm);
	if(code == nullptr)
		return "";

	EvaluableNodeReference args = EvaluableNodeReference::Null();
	auto scope_stack = Interpreter::ConvertArgsToScopeStack(args, enm);

	EvaluableNodeReference returned_value = bundle->entity->ExecuteCodeAsEntity(code, scope_stack, nullptr,
		&bundle->writeListeners, bundle->printListener, nullptr
#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
#endif
	);

	enm.FreeNode(args);
	enm.FreeNode(scope_stack);
	enm.FreeNodeTreeIfPossible(code);

	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(returned_value);
	enm.FreeNodeTreeIfPossible(returned_value);
	return (converted ? result : string_intern_pool.GetStringFromID(string_intern_pool.NOT_A_STRING_ID));
}

bool EntityExternalInterface::EntityListenerBundle::SetEntityValueAtLabel(std::string &label_name, EvaluableNodeReference new_value)
{
	StringInternPool::StringID label_sid = string_intern_pool.GetIDFromString(label_name);

	EntityWriteReference entity_wr(entity);
#ifdef MULTITHREAD_INTERFACE
	//make a full copy of the entity in case any other threads are operating on it
	entity->SetRoot(entity->GetRoot(), false);
#endif

	bool success = entity->SetValueAtLabel(label_sid, new_value, false, &writeListeners);

	entity->evaluableNodeManager.FreeNodeTreeIfPossible(new_value);

	return success;
}

EntityExternalInterface::EntityListenerBundle::~EntityListenerBundle()
{
	if(entity != nullptr)
	{
		asset_manager.DestroyEntity(entity);
		delete entity;
	}

	if(printListener != nullptr)
		delete printListener;
	if(writeListeners.size() > 0 && writeListeners[0] != nullptr)
		delete writeListeners[0];
}
