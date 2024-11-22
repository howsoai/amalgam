//project headers:
#include "EntityWriteListener.h"
#include "EvaluableNodeTreeFunctions.h"

EntityWriteListener::EntityWriteListener(Entity *listening_entity, bool retain_writes, const std::string &filename)
{
	listeningEntity = listening_entity;

	if(retain_writes)
		storedWrites = listenerStorage.AllocNode(ENT_SEQUENCE);
	else
		storedWrites = nullptr;
	
	if(!filename.empty())
	{
		logFile.open(filename, std::ios::binary);
		logFile << "(" << GetStringFromEvaluableNodeType(ENT_SEQUENCE) << "\r\n";
	}
}

EntityWriteListener::~EntityWriteListener()
{
	if(logFile.is_open())
	{
		logFile << ")" << "\r\n";
		logFile.close();
	}
}

void EntityWriteListener::LogSystemCall(EvaluableNode *params)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	EvaluableNode *new_sys_call = listenerStorage.AllocNode(ENT_SYSTEM);
	new_sys_call->AppendOrderedChildNode(listenerStorage.DeepAllocCopy(params));

	LogNewEntry(new_sys_call);
}

void EntityWriteListener::LogPrint(std::string &print_string)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	EvaluableNode *new_print = listenerStorage.AllocNode(ENT_PRINT);
	new_print->AppendOrderedChildNode(listenerStorage.AllocNode(ENT_STRING, print_string));

	// don't flush because printing is handled in a bulk loop, the interpreter will manually flush afterwards
	LogNewEntry(new_print, false);
}

void EntityWriteListener::LogWriteLabelValueToEntity(Entity *entity,
	const StringInternPool::StringID label_name, EvaluableNode *value, bool direct_set)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	EvaluableNode *new_write = BuildNewWriteOperation(direct_set ? ENT_DIRECT_ASSIGN_TO_ENTITIES : ENT_ASSIGN_TO_ENTITIES, entity);

	EvaluableNode *assoc = listenerStorage.AllocNode(ENT_ASSOC);
	new_write->AppendOrderedChildNode(assoc);

	assoc->AppendOrderedChildNode(listenerStorage.AllocNode(ENT_STRING, label_name));
	assoc->AppendOrderedChildNode(listenerStorage.DeepAllocCopy(value, direct_set ? EvaluableNodeManager::ENMM_NO_CHANGE : EvaluableNodeManager::ENMM_REMOVE_ALL));

	LogNewEntry(new_write);
}

void EntityWriteListener::LogWriteLabelValuesToEntity(Entity *entity,
	EvaluableNode *label_value_pairs, bool accum_values, bool direct_set)
{
	//can only work with assoc arrays
	if(!EvaluableNode::IsAssociativeArray(label_value_pairs))
		return;

#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	auto node_type = ENT_ASSIGN_TO_ENTITIES;
	if(accum_values)
		node_type = ENT_ACCUM_TO_ENTITIES;
	else if(direct_set)
		node_type = ENT_DIRECT_ASSIGN_TO_ENTITIES;

	EvaluableNode *new_write = BuildNewWriteOperation(node_type, entity);

	EvaluableNode *assoc = listenerStorage.DeepAllocCopy(label_value_pairs, direct_set ? EvaluableNodeManager::ENMM_NO_CHANGE : EvaluableNodeManager::ENMM_REMOVE_ALL);
	new_write->AppendOrderedChildNode(assoc);

	LogNewEntry(new_write);
}

void EntityWriteListener::LogWriteToEntityRoot(Entity *entity)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	EvaluableNode *new_write = BuildNewWriteOperation(ENT_ASSIGN_ENTITY_ROOTS, entity);
	EvaluableNode *new_root = entity->GetRoot(&listenerStorage, EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT);

	new_write->AppendOrderedChildNode(new_root);

	LogNewEntry(new_write);
}

void EntityWriteListener::LogCreateEntity(Entity *new_entity)
{
	if(new_entity == nullptr)
		return;

#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	LogCreateEntityRecurse(new_entity);
}

void EntityWriteListener::LogDestroyEntity(Entity *destroyed_entity)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	EvaluableNode *new_destroy = BuildNewWriteOperation(ENT_DESTROY_ENTITIES, destroyed_entity);

	LogNewEntry(new_destroy);
}

void EntityWriteListener::LogSetEntityRandomSeed(Entity *entity, const std::string &rand_seed, bool deep_set)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	EvaluableNode *new_set = BuildNewWriteOperation(ENT_SET_ENTITY_RAND_SEED, entity);

	new_set->AppendOrderedChildNode(listenerStorage.AllocNode(ENT_STRING, rand_seed));

	if(!deep_set)
		new_set->AppendOrderedChildNode(listenerStorage.AllocNode(false));

	LogNewEntry(new_set);
}

void EntityWriteListener::FlushLogFile()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::SingleLock lock(mutex);
#endif

	if(logFile.is_open() && logFile.good())
		logFile.flush();
}

EvaluableNode *EntityWriteListener::BuildNewWriteOperation(EvaluableNodeType assign_type, Entity *target_entity)
{
	//create this code:
	// (direct_assign_to_entity *id list* (assoc *label name* *value*))
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

	EvaluableNodeReference new_entity_root_copy = new_entity->GetRoot(&listenerStorage);
	new_create->AppendOrderedChildNode(new_entity_root_copy);

	LogNewEntry(new_create);

	//log any nested created entities
	for(auto entity : new_entity->GetContainedEntities())
		LogCreateEntityRecurse(entity);
}

void EntityWriteListener::LogNewEntry(EvaluableNode *new_entry, bool flush)
{
	if(logFile.is_open() && logFile.good())
	{
		//one extra indentation because already have the sequence
		logFile << Parser::Unparse(new_entry, false, true, false) << "\r\n";
		if(flush)
			logFile.flush();
	}

	if(storedWrites == nullptr)
		listenerStorage.FreeAllNodes();
	else
		storedWrites->AppendOrderedChildNode(new_entry);
}
