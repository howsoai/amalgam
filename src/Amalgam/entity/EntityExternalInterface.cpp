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

EntityExternalInterface::LoadEntityStatus EntityExternalInterface::LoadEntity(std::string &handle, std::string &path, bool persistent, bool load_contained_entities,
	bool escape_filename, bool escape_contained_filenames, std::string &write_log_filename, std::string &print_log_filename, std::string rand_seed)
{
	LoadEntityStatus status;

	if(rand_seed.empty())
	{
		typedef std::chrono::steady_clock clk;
		auto t = std::chrono::duration_cast<std::chrono::milliseconds>(clk::now().time_since_epoch()).count();
		rand_seed = std::to_string(t);
	}

	std::string file_type = "";
	Entity *entity = asset_manager.LoadEntityFromResourcePath(path, file_type, persistent, load_contained_entities,
		escape_filename, escape_contained_filenames, rand_seed, nullptr, status);

	if(!status.loaded)
		return status;

	asset_manager.SetRootPermission(entity, true);

	PrintListener *pl = nullptr;
	std::vector<EntityWriteListener *> wl;

	if(!print_log_filename.empty())
		pl = new PrintListener(print_log_filename);

	if(!write_log_filename.empty())
	{
		EntityWriteListener *write_log = new EntityWriteListener(entity, false, write_log_filename);
		wl.push_back(write_log);
	}

	AddEntityBundle(handle, new EntityListenerBundle(entity, wl, pl));

	return status;
}

EntityExternalInterface::LoadEntityStatus EntityExternalInterface::VerifyEntity(std::string &path)
{
	std::ifstream f(path, std::fstream::binary | std::fstream::in);

	if(!f.good())
		return EntityExternalInterface::LoadEntityStatus(false, "Cannot open file", "");

	size_t header_size = 0;
	auto [error_string, version, success] = FileSupportCAML::ReadHeader(f, header_size);
	if(!success)
		return EntityExternalInterface::LoadEntityStatus(false, error_string, version);

	return EntityExternalInterface::LoadEntityStatus(false, "", version);
}

bool EntityExternalInterface::CloneEntity(std::string &handle, std::string &cloned_handle, std::string &path, bool persistent,
	std::string &write_log_filename, std::string &print_log_filename)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return false;

	if(bundle->entity == nullptr)
		return false;

	Entity *entity = new Entity(bundle->entity);

	PrintListener *pl = nullptr;
	std::vector<EntityWriteListener *> wl;

	if(!print_log_filename.empty())
		pl = new PrintListener(print_log_filename);

	if(!write_log_filename.empty())
	{
		EntityWriteListener *write_log = new EntityWriteListener(entity, false, write_log_filename);
		wl.push_back(write_log);
	}

	AddEntityBundle(cloned_handle, new EntityListenerBundle(entity, wl, pl));

	if(persistent)
		StoreEntity(cloned_handle, path, true, true);

	return true;
}

void EntityExternalInterface::StoreEntity(std::string &handle, std::string &path, bool update_persistence_location, bool store_contained_entities)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr || bundle->entity == nullptr)
		return;

	std::string file_type = "";
	EntityReadReference entity(bundle->entity);
	asset_manager.StoreEntityToResourcePath(entity, path, file_type, update_persistence_location, store_contained_entities, false, true, false);
}

void EntityExternalInterface::ExecuteEntity(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	ExecutionCycleCount max_num_steps = 0, num_steps_executed = 0;
	size_t max_num_nodes = 0, num_nodes_allocated = 0;
	bundle->entity->Execute(max_num_steps, num_steps_executed, max_num_nodes, num_nodes_allocated,
		label, nullptr, false, nullptr, &bundle->writeListeners, bundle->printListener);
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

EvaluableNode *NodifyNumberList(Entity *entity, double *arr, size_t len)
{
	EvaluableNodeManager *enm = &entity->evaluableNodeManager;
	EvaluableNode *list_node = enm->AllocNode(ENT_LIST);
	auto &children = list_node->GetOrderedChildNodes();
	children.resize(len);
	for(size_t i = 0; i < len; i++)
		children[i] = enm->AllocNode(arr[i]);

	return list_node;
}

EvaluableNode *NodifyNumberMatrix(Entity *entity, double *arr, size_t w, size_t h)
{
	EvaluableNodeManager *enm = &entity->evaluableNodeManager;
	EvaluableNode *matrix_node = enm->AllocNode(ENT_LIST);

	auto &children = matrix_node->GetOrderedChildNodes();
	children.resize(w);
	for(size_t x = 0; x < w; x++)
	{
		double *column = new double[h];
		for(size_t y = 0; y < h; y++)
			column[y] = arr[x*h + y];

		children[x] = NodifyNumberList(entity, column, h);
		delete [] column;
	}

	return matrix_node;
}

EvaluableNode *NodifyStringList(Entity *entity, char **arr, size_t len)
{
	EvaluableNodeManager *enm = &entity->evaluableNodeManager;
	EvaluableNode *list_node = enm->AllocNode(ENT_LIST);
	auto &children = list_node->GetOrderedChildNodes();
	children.resize(len);
	for(size_t i = 0; i < len; i++)
		children[i] = enm->AllocNode(ENT_STRING, arr[i]);

	return list_node;
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

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, nullptr, false);
	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(label_val);
	return (converted ? result : string_intern_pool.GetStringFromID(string_intern_pool.NOT_A_STRING_ID));
}

std::string EntityExternalInterface::ExecuteEntityJSON(std::string &handle, std::string &label, std::string_view json)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return "";

	EvaluableNodeManager &enm = bundle->entity->evaluableNodeManager;
	EvaluableNodeReference args = EvaluableNodeReference(EvaluableNodeJSONTranslation::JsonToEvaluableNode(&enm, json), true);

	auto call_stack = Interpreter::ConvertArgsToCallStack(args, enm);

	ExecutionCycleCount max_num_steps = 0, num_steps_executed = 0;
	size_t max_num_nodes = 0, num_nodes_allocated = 0;
	EvaluableNodeReference returned_value = bundle->entity->Execute(max_num_steps, num_steps_executed, max_num_nodes,
		num_nodes_allocated, label, call_stack, false, nullptr, &bundle->writeListeners, bundle->printListener);

	//ConvertArgsToCallStack always adds an outer list that is safe to free
	enm.FreeNode(call_stack);

	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(returned_value);
	enm.FreeNodeTreeIfPossible(returned_value);
	return (converted ? result : string_intern_pool.GetStringFromID(string_intern_pool.NOT_A_STRING_ID));
}

bool EntityExternalInterface::EntityListenerBundle::SetEntityValueAtLabel(std::string &label_name, EvaluableNodeReference new_value)
{
	StringInternPool::StringID label_sid = string_intern_pool.GetIDFromString(label_name);

	EntityWriteReference entity_wr(entity);
#ifdef MULTITHREAD_SUPPORT
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
