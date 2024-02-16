//project headers:
#include "EntityExternalInterface.h"

#include "Amalgam.h"
#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "Entity.h"
#include "EntityWriteListener.h"
#include "FileSupportCAML.h"
#include "FileSupportJSON.h"
#include "Interpreter.h"
#include "PlatformSpecific.h"

//system headers:
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

LoadEntityStatus::LoadEntityStatus()
{
	SetStatus(true);
}

LoadEntityStatus::LoadEntityStatus(bool loaded, std::string message, std::string version)
{
	SetStatus(loaded, message, version);
}

void LoadEntityStatus::SetStatus(bool loaded_in, std::string message_in, std::string version_in)
{
	loaded = loaded_in;
	message = std::move(message_in);
	version = std::move(version_in);
}

LoadEntityStatus EntityExternalInterface::LoadEntity(std::string &handle, std::string &path, bool persistent, bool load_contained_entities,
	std::string &write_log_filename, std::string &print_log_filename, std::string rand_seed)
{
	LoadEntityStatus status;

	if(rand_seed == "")
	{
		typedef std::chrono::steady_clock clk;
		auto t = std::chrono::duration_cast<std::chrono::milliseconds>(clk::now().time_since_epoch()).count();
		rand_seed = std::to_string(t);
	}

	std::string file_type = "";
	Entity *entity = asset_manager.LoadEntityFromResourcePath(path, file_type, persistent, load_contained_entities, false, true, rand_seed, status);
	if(!status.loaded)
		return status;

	asset_manager.SetRootPermission(entity, true);

	PrintListener *pl = nullptr;
	std::vector<EntityWriteListener *> wl;

	if(print_log_filename != "")
		pl = new PrintListener(print_log_filename);

	if(write_log_filename != "")
	{
		EntityWriteListener *write_log = new EntityWriteListener(entity, false, write_log_filename);
		wl.push_back(write_log);
	}

	AddEntityBundle(handle, new EntityListenerBundle(entity, wl, pl));

	return status;
}

void EntityExternalInterface::StoreEntity(std::string &handle, std::string &path, bool update_persistence_location, bool store_contained_entities)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	Entity *entity = bundle->entity;
	if(entity == nullptr)
		return;

	std::string file_type = "";
	asset_manager.StoreEntityToResourcePath(entity, path, file_type, update_persistence_location, store_contained_entities, false, true, false);
}

void EntityExternalInterface::ExecuteEntity(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	ExecutionCycleCount max_num_steps = 0, num_steps_executed = 0;
	size_t max_num_nodes = 0, num_nodes_allocated = 0;
	bundle->entity->Execute(max_num_steps, num_steps_executed, max_num_nodes, num_nodes_allocated, &bundle->writeListeners, bundle->printListener,
		nullptr, false,
	#ifdef MULTITHREAD_SUPPORT
		nullptr, nullptr,
	#endif
		label);
}

void EntityExternalInterface::DeleteEntity(std::string &handle)
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

void EntityExternalInterface::AppendToLabel(std::string &handle, std::string &label, double value)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	//get the label
	EvaluableNodeReference label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		//modify local copy
		label_val->AppendOrderedChildNode(bundle->entity->evaluableNodeManager.AllocNode(value));

		//overwrite the label with the modified copy
		bundle->SetEntityValueAtLabel(label, label_val);
	}
	else
	{
		// wrap the existing and new element in a list
		EvaluableNode list(ENT_LIST);
		EvaluableNode initial_value(EvaluableNode::ToNumber(label_val));
		EvaluableNode parsed_input(value);
		list.AppendOrderedChildNode(&initial_value);
		list.AppendOrderedChildNode(&parsed_input);

		// overwrite the label with the list
		EvaluableNodeReference list_reference(&list, false);
		bundle->SetEntityValueAtLabel(label, list_reference);
	}
}

void EntityExternalInterface::AppendToLabel(std::string &handle, std::string &label, std::string &value)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNodeReference label_val(bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false), false);

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		//modify local copy
		label_val->AppendOrderedChildNode(bundle->entity->evaluableNodeManager.AllocNode(ENT_STRING, value));

		//overwrite the label with the modified copy
		bundle->SetEntityValueAtLabel(label, label_val);
	}
	else //need to transform it into a list
	{
		//wrap the existing and new element in a list
		//can use local stack instead of heap because the entity will copy anyway
		EvaluableNode list(ENT_LIST);
		EvaluableNode initial_value(ENT_STRING, EvaluableNode::ToStringPreservingOpcodeType(label_val));
		EvaluableNode parsed_input(ENT_STRING, value);
		list.AppendOrderedChildNode(&initial_value);
		list.AppendOrderedChildNode(&parsed_input);

		// overwrite the label with the list
		EvaluableNodeReference list_reference(&list, false);
		bundle->SetEntityValueAtLabel(label, list_reference);
	}
}

void EntityExternalInterface::SetLabel(std::string &handle, std::string &label, double value)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode parsed_input(value);
	EvaluableNodeReference parsed_input_reference(&parsed_input, false);
	bundle->SetEntityValueAtLabel(label, parsed_input_reference);
}

void EntityExternalInterface::SetLabel(std::string &handle, std::string &label, std::string &value)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode parsed_input(ENT_STRING, value);
	EvaluableNodeReference parsed_input_reference(&parsed_input, false);
	bundle->SetEntityValueAtLabel(label, parsed_input_reference);
}

double EntityExternalInterface::GetNumber(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return std::numeric_limits<double>::quiet_NaN();

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	//Ensure you grab the return value before releasing resources
	double ret = EvaluableNode::ToNumber(label_val);
	return ret;
}

std::string EntityExternalInterface::GetString(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return "";

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	//Ensure you grab the return value before releasing resources
	std::string ret = EvaluableNode::ToStringPreservingOpcodeType(label_val);
	return ret;
}

std::string EntityExternalInterface::GetStringFromList(std::string &handle, std::string &label, size_t index)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return "";

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	std::string ret = "";

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		if(index < children.size())
			ret = EvaluableNode::ToStringPreservingOpcodeType(children[index]);
	}
	else
	{
		ret = EvaluableNode::ToStringPreservingOpcodeType(label_val);
	}

	return ret;
}

// ************************************
// get, set, and append lists
// ************************************

size_t EntityExternalInterface::GetNumberListLength(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return 0;

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(label_val == nullptr)
		return 0;

	size_t ret = 1;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		ret = children.size();
	}

	return ret;
}

void EntityExternalInterface::GetNumberList(std::string &handle, std::string &label, double *out_arr, size_t len)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(label_val == nullptr)
		return;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		size_t min = std::min(children.size(), len);
		for(size_t i = 0; i < min; i++)
			out_arr[i] = EvaluableNode::ToNumber(children[i]);
	}
	else
	{
		out_arr[0] = EvaluableNode::ToNumber(label_val);
	}
}

void EntityExternalInterface::GetNumberList(EvaluableNode *label_val, double *out_arr, size_t len)
{
	if(label_val == nullptr)
		return;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		size_t min = std::min(children.size(), len);
		for(size_t i = 0; i < min; i++)
			out_arr[i] = EvaluableNode::ToNumber(children[i]);
	}
	else
	{
		out_arr[0] = EvaluableNode::ToNumber(label_val);
	}
}

size_t EntityExternalInterface::GetNumberMatrixWidth(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return 0;

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(label_val == nullptr)
		return 0;

	std::size_t ret = 1;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		ret = children.size();
	}

	return ret;
}

size_t EntityExternalInterface::GetNumberMatrixHeight(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return 0;

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(label_val == nullptr)
		return 0;

	std::size_t ret = 1;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes()[0]->GetOrderedChildNodes();
		ret = children.size();
	}

	return ret;
}

void EntityExternalInterface::GetNumberMatrix(std::string &handle, std::string &label, double *out_arr, size_t w, size_t h)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(label_val == nullptr)
		return;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		double *column = new double[h];
		for(size_t x = 0; x < w; x++)
		{
			GetNumberList(children[x], column, h);
			for(size_t y = 0; y < h; y++)
				out_arr[x*h + y] = column[y];
		}
		delete [] column;
	}
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

void EntityExternalInterface::SetNumberList(std::string &handle, std::string &label, double *arr, size_t len)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode *list_node = NodifyNumberList(bundle->entity, arr, len);
	EvaluableNodeReference list_node_reference(list_node, true);
	bundle->SetEntityValueAtLabel(label, list_node_reference);
}

void EntityExternalInterface::SetNumberMatrix(std::string &handle, std::string &label, double *arr, size_t w, size_t h)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode *list_node = NodifyNumberMatrix(bundle->entity, arr, w, h);
	EvaluableNodeReference list_node_reference(list_node, true);
	bundle->SetEntityValueAtLabel(label, list_node_reference);
}

void EntityExternalInterface::AppendNumberList(std::string &handle, std::string &label, double *arr, size_t len)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode *list_node = NodifyNumberList(bundle->entity, arr, len);

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);
	label_val->AppendOrderedChildNode(list_node);
}

size_t EntityExternalInterface::GetStringListLength(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return 0;

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(label_val == nullptr)
		return 0;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		return children.size();
	}

	return 1;
}

void EntityExternalInterface::GetStringList(std::string &handle, std::string &label, std::string *out_arr, size_t len)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode *label_val = bundle->entity->GetValueAtLabel(label, &bundle->entity->evaluableNodeManager, false);

	if(label_val == nullptr)
		return;

	if(EvaluableNode::IsOrderedArray(label_val))
	{
		auto &children = label_val->GetOrderedChildNodes();
		size_t min = std::min(children.size(), len);
		for(size_t i = 0; i < min; i++)
			out_arr[i] = EvaluableNode::ToStringPreservingOpcodeType(children[i]);
	}
	else
	{
		out_arr[0] = EvaluableNode::ToStringPreservingOpcodeType(label_val);
	}
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

void EntityExternalInterface::SetStringList(std::string &handle, std::string &label, char **arr, size_t len)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	EvaluableNode *list_node = NodifyStringList(bundle->entity, arr, len);
	EvaluableNodeReference list_node_reference(list_node, true);
	bundle->SetEntityValueAtLabel(label, list_node_reference);
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
		num_nodes_allocated, &bundle->writeListeners, bundle->printListener, call_stack, false,
	#ifdef MULTITHREAD_SUPPORT
		nullptr, nullptr,
	#endif
		label);

	//ConvertArgsToCallStack always adds an outer list that is safe to free
	enm.FreeNode(call_stack);

	auto [result, converted] = EvaluableNodeJSONTranslation::EvaluableNodeToJson(returned_value);
	enm.FreeNodeTreeIfPossible(returned_value);
	return (converted ? result : string_intern_pool.GetStringFromID(string_intern_pool.NOT_A_STRING_ID));
}

bool EntityExternalInterface::EntityListenerBundle::SetEntityValueAtLabel(std::string &label_name, EvaluableNodeReference new_value)
{
	StringInternPool::StringID label_sid = string_intern_pool.GetIDFromString(label_name);

#ifdef MULTITHREAD_SUPPORT
	auto write_lock = entity->CreateEntityLock<Concurrency::WriteLock>();
	entity->SetRoot(entity->GetRoot(), false);
#endif

	bool success = entity->SetValueAtLabel(label_sid, new_value, false, &writeListeners);

	entity->evaluableNodeManager.FreeNodeTreeIfPossible(new_value);

	return success;
}
