//project headers:
#include "EntityExecution.h"
#include "Entity.h"
#include "Interpreter.h"

EvaluableNodeReference EntityExecution::ExecuteEntity(Entity &entity,
	StringInternPool::StringID label_sid,
	EvaluableNode *call_stack, bool on_self, Interpreter *calling_interpreter,
	std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
	PerformanceConstraints *performance_constraints
#ifdef MULTITHREAD_SUPPORT
	, Concurrency::ReadLock *enm_lock
#endif
)
{
	if(!on_self && entity.IsLabelPrivate(label_sid))
		return EvaluableNodeReference(nullptr, true);
	
	auto &evaluableNodeManager = entity.evaluableNodeManager;
	
	EvaluableNode *node_to_execute = nullptr;
	if(label_sid == string_intern_pool.NOT_A_STRING_ID)   //if not specified, then use root
		node_to_execute = evaluableNodeManager.GetRootNode();
	else //get code at label
	{
		auto &labelIndex = entity.GetLabelIndex();
		const auto &label = labelIndex.find(label_sid);
		
		if(label != end(labelIndex))
			node_to_execute = label->second;
	}
	
	//if label not found or no code, can't do anything
	if(node_to_execute == nullptr)
		return EvaluableNodeReference::Null();
	
	auto &randomStream = entity.GetRandomStreamRef();
	Interpreter interpreter(&evaluableNodeManager, randomStream.CreateOtherStreamViaRand(),
		write_listeners, print_listener, performance_constraints, &entity, calling_interpreter);
	
#ifdef MULTITHREAD_SUPPORT
	if(enm_lock == nullptr)
		interpreter.memoryModificationLock = Concurrency::ReadLock(evaluableNodeManager.memoryModificationMutex);
	else
		interpreter.memoryModificationLock = std::move(*enm_lock);
#endif
	
	EvaluableNodeReference retval = interpreter.ExecuteNode(node_to_execute, call_stack);
	
#ifdef MULTITHREAD_SUPPORT
	if(enm_lock != nullptr)
		*enm_lock = std::move(interpreter.memoryModificationLock);
#endif
	
	return retval;
}

void EntityExecution::ExecuteEntity(std::string &handle, std::string &label)
{
	auto bundle = FindEntityBundle(handle);
	if(bundle == nullptr)
		return;

	ExecuteEntity(*bundle->entity,
		label, nullptr, false, nullptr, &bundle->writeListeners, bundle->printListener);
}
