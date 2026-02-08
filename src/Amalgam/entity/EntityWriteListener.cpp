//project headers:
#include "EntityWriteListener.h"
#include "EvaluableNodeTreeFunctions.h"

EntityWriteListener::EntityWriteListener(Entity *listening_entity, std::unique_ptr<std::ostream> &&transaction_file,
	bool retain_writes, bool _pretty, bool sort_keys) : logFile(std::move(transaction_file))
{
	listeningEntity = listening_entity;

	if(retain_writes)
		storedWrites = listenerStorage.AllocNode(ENT_SEQUENCE);
	else
		storedWrites = nullptr;

	fileSuffix = ")\r\n";
	pretty = _pretty;
	sortKeys = sort_keys;
	if(logFile != nullptr && logFile->good())
	{
		*logFile << "(" << GetStringFromEvaluableNodeType(ENT_SEQUENCE) << "\r\n";
	}
	huffmanTree = nullptr;
}

EntityWriteListener::EntityWriteListener(Entity *listening_entity,
	bool _pretty, bool sort_keys, std::unique_ptr<std::ostream> &&transaction_file, HuffmanTree<uint8_t> *huffman_tree) : logFile(std::move(transaction_file))
{
	listeningEntity = listening_entity;
	storedWrites = nullptr;

	auto new_entity_sid = GetStringIdFromBuiltInStringId(ENBISI_new_entity);
	if(pretty)
		fileSuffix = "\t";
	fileSuffix += new_entity_sid->string;

	if(pretty)
		fileSuffix += "\r\n)\r\n";
	else
		fileSuffix += ")";

	pretty = _pretty;
	sortKeys = sort_keys;

	huffmanTree = huffman_tree;
}

EntityWriteListener::~EntityWriteListener()
{
	if(logFile != nullptr)
	{
		if(huffmanTree == nullptr)
		{
			*logFile << fileSuffix;
		}
		else
		{
			auto to_append = CompressStringToAppend(fileSuffix, huffmanTree);
			logFile->write(reinterpret_cast<char *>(to_append.data()), to_append.size());

			delete huffmanTree;
		}
	}
}

void EntityWriteListener::LogSystemCall(EvaluableNode *params)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	EvaluableNode *new_sys_call = listenerStorage.AllocNode(ENT_SYSTEM);
	new_sys_call->AppendOrderedChildNode(listenerStorage.DeepAllocCopy(params));

	LogNewEntry(new_sys_call);
}

void EntityWriteListener::LogPrint(std::string &print_string)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	EvaluableNode *new_print = listenerStorage.AllocNode(ENT_PRINT);
	new_print->AppendOrderedChildNode(listenerStorage.AllocNode(ENT_STRING, print_string));

	// don't flush because printing is handled in a bulk loop, the interpreter will manually flush afterwards
	LogNewEntry(new_print, false);
}

void EntityWriteListener::LogWriteLabelValueToEntity(Entity *entity,
	const StringInternPool::StringID label_name, EvaluableNode *value)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	EvaluableNode *new_write = BuildNewWriteOperation(ENT_ASSIGN_TO_ENTITIES, entity);

	EvaluableNode *assoc = listenerStorage.AllocNode(ENT_ASSOC);
	new_write->AppendOrderedChildNode(assoc);

	assoc->AppendOrderedChildNode(listenerStorage.AllocNode(ENT_STRING, label_name));
	assoc->AppendOrderedChildNode(listenerStorage.DeepAllocCopy(value));

	LogNewEntry(new_write);
}

void EntityWriteListener::LogWriteLabelValuesToEntity(Entity *entity,
	EvaluableNode *label_value_pairs, bool accum_values)
{
	//can only work with assoc arrays
	if(!EvaluableNode::IsAssociativeArray(label_value_pairs))
		return;

#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	auto node_type = ENT_ASSIGN_TO_ENTITIES;
	if(accum_values)
		node_type = ENT_ACCUM_TO_ENTITIES;

	EvaluableNode *new_write = BuildNewWriteOperation(node_type, entity);

	EvaluableNode *assoc = listenerStorage.DeepAllocCopy(label_value_pairs);
	new_write->AppendOrderedChildNode(assoc);

	LogNewEntry(new_write);
}

void EntityWriteListener::LogRemoveLabesFromEntity(Entity *entity, EvaluableNode *labels)
{
	//can only work with ordered child nodes
	if(!EvaluableNode::IsOrderedArray(labels))
		return;

#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	EvaluableNode *new_write = BuildNewWriteOperation(ENT_REMOVE_FROM_ENTITIES, entity);

	EvaluableNode *list = listenerStorage.DeepAllocCopy(labels);
	new_write->AppendOrderedChildNode(list);

	LogNewEntry(new_write);
}

void EntityWriteListener::LogWriteToEntityRoot(Entity *entity)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif
	EvaluableNode *new_write = BuildNewWriteOperation(ENT_ASSIGN_ENTITY_ROOTS, entity);
	EvaluableNode *new_root = entity->GetRoot(&listenerStorage);
	EvaluableNode *new_lambda = listenerStorage.AllocNode(EvaluableNodeType::ENT_LAMBDA);
	new_lambda->AppendOrderedChildNode(new_root);
	new_write->AppendOrderedChildNode(new_lambda);

	LogNewEntry(new_write);
}

void EntityWriteListener::LogEntityAccumRoot(Entity *entity, EvaluableNodeReference accum_code)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif
	EvaluableNode *new_write = BuildNewWriteOperation(ENT_ACCUM_ENTITY_ROOTS, entity);
	EvaluableNode *new_lambda = listenerStorage.AllocNode(EvaluableNodeType::ENT_LAMBDA);
	new_lambda->AppendOrderedChildNode(listenerStorage.DeepAllocCopy(accum_code));
	new_write->AppendOrderedChildNode(new_lambda);

	LogNewEntry(new_write);
}

void EntityWriteListener::LogCreateEntity(Entity *new_entity)
{
	if(new_entity == nullptr)
		return;

#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	LogCreateEntityRecurse(new_entity);
}

void EntityWriteListener::LogDestroyEntity(Entity *destroyed_entity)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	EvaluableNode *new_destroy = BuildNewWriteOperation(ENT_DESTROY_ENTITIES, destroyed_entity);

	LogNewEntry(new_destroy);
}

void EntityWriteListener::LogSetEntityRandomSeed(Entity *entity, const std::string &rand_seed, bool deep_set)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	EvaluableNode *new_set = BuildNewWriteOperation(ENT_SET_ENTITY_RAND_SEED, entity);

	new_set->AppendOrderedChildNode(listenerStorage.AllocNode(ENT_STRING, rand_seed));

	if(!deep_set)
		new_set->AppendOrderedChildNode(listenerStorage.AllocNode(false));

	LogNewEntry(new_set);
}

void EntityWriteListener::LogSetEntityPermissions(Entity *entity,
		EntityPermissions permissions_to_set, EntityPermissions permission_values, bool deep_set)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	EvaluableNode *new_set = BuildNewWriteOperation(ENT_SET_ENTITY_PERMISSIONS, entity);

	EvaluableNode *assoc = listenerStorage.AllocNode(ENT_ASSOC);
	new_set->AppendOrderedChildNode(assoc);

	if(permissions_to_set.HasPermission(EntityPermissions::Permission::STD_OUT_AND_STD_ERR))
		new_set->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err),
			listenerStorage.AllocNode(permission_values.HasPermission(EntityPermissions::Permission::STD_OUT_AND_STD_ERR)));

	if(permissions_to_set.HasPermission(EntityPermissions::Permission::STD_IN))
		new_set->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_in),
			listenerStorage.AllocNode(permission_values.HasPermission(EntityPermissions::Permission::STD_IN)));

	if(permissions_to_set.HasPermission(EntityPermissions::Permission::LOAD))
		new_set->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_load),
			listenerStorage.AllocNode(permission_values.HasPermission(EntityPermissions::Permission::LOAD)));

	if(permissions_to_set.HasPermission(EntityPermissions::Permission::STORE))
		new_set->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_store),
			listenerStorage.AllocNode(permission_values.HasPermission(EntityPermissions::Permission::STORE)));

	if(permissions_to_set.HasPermission(EntityPermissions::Permission::ENVIRONMENT))
		new_set->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_environment),
			listenerStorage.AllocNode(permission_values.HasPermission(EntityPermissions::Permission::ENVIRONMENT)));

	if(permissions_to_set.HasPermission(EntityPermissions::Permission::ALTER_PERFORMANCE))
		new_set->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_alter_performance),
			listenerStorage.AllocNode(permission_values.HasPermission(EntityPermissions::Permission::ALTER_PERFORMANCE)));

	if(permissions_to_set.HasPermission(EntityPermissions::Permission::SYSTEM))
		new_set->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_system),
			listenerStorage.AllocNode(permission_values.HasPermission(EntityPermissions::Permission::SYSTEM)));

	if(!deep_set)
		new_set->AppendOrderedChildNode(listenerStorage.AllocNode(false));

	LogNewEntry(new_set);
}

void EntityWriteListener::FlushLogFile()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::Lock lock(mutex);
#endif

	if(logFile != nullptr && logFile->good())
		logFile->flush();
}

EvaluableNode *EntityWriteListener::BuildNewWriteOperation(EvaluableNodeType assign_type, Entity *target_entity)
{
	//create this code, though change assign_type as appropriate
	// (assign_to_entity *id list* (assoc *label name* *value*))
	EvaluableNode *new_write = listenerStorage.AllocNode(assign_type);

	if(target_entity != listeningEntity)
	{
		EvaluableNode *id_list = GetTraversalIDPathFromAToB(&listenerStorage, listeningEntity, target_entity);
		new_write->AppendOrderedChildNode(id_list);
	}

	return new_write;
}

void EntityWriteListener::LogCreateEntityRecurse(Entity *new_entity)
{
	EvaluableNode *new_create = BuildNewWriteOperation(ENT_CREATE_ENTITIES, new_entity);

	EvaluableNode *lambda_for_create = listenerStorage.AllocNode(ENT_LAMBDA);
	EvaluableNodeReference new_entity_root_copy = new_entity->GetRoot(&listenerStorage);
	lambda_for_create->AppendOrderedChildNode(new_entity_root_copy);

	//append to new_create last to make sure node flags are propagated up
	new_create->AppendOrderedChildNode(lambda_for_create);

	LogNewEntry(new_create);

	//log any nested created entities
	for(auto entity : new_entity->GetContainedEntities())
		LogCreateEntityRecurse(entity);
}

void EntityWriteListener::LogNewEntry(EvaluableNode *new_entry, bool flush)
{
	if(logFile != nullptr && logFile->good())
	{
		if(huffmanTree == nullptr)
		{
			//one extra indentation if pretty because already have the seq or declare
			*logFile << Parser::Unparse(new_entry, pretty, true, sortKeys, false, pretty ? 1 : 0);

			//append a new line if not already appended
			if(!pretty)
				*logFile << "\r\n";
		}
		else
		{
			//one extra indentation if pretty because already have the seq or declare
			std::string new_code = Parser::Unparse(new_entry, pretty, true, sortKeys, false, pretty ? 1 : 0);

			//append a new line if not already appended
			if(!pretty)
				new_code += "\r\n";

			auto to_append = CompressStringToAppend(new_code, huffmanTree);
			logFile->write(reinterpret_cast<char *>(to_append.data()), to_append.size());
		}

		if(flush)
			logFile->flush();
	}

	if(storedWrites == nullptr)
		listenerStorage.FreeAllNodes();
	else
		storedWrites->AppendOrderedChildNode(new_entry);
}
