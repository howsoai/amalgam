//project headers:
#include "Interpreter.h"

#include "AssetManager.h"
#include "EntityQueries.h"
#include "EntityQueryBuilder.h"
#include "EntityQueryCaches.h"
#include "EntityWriteListener.h"
#include "EvaluableNodeTreeDifference.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"
#include "PerformanceProfiler.h"

//system headers:
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_ENTITY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	return AllocReturn(entity != nullptr, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	bool return_query_value = (en->GetType() == ENT_COMPUTE_ON_CONTAINED_ENTITIES);

	//parameters to search entities for
	EvaluableNodeReference query_params = EvaluableNodeReference::Null();
	EvaluableNodeReference entity_id_path = EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 1)
	{
		query_params = InterpretNodeForImmediateUse(ocn[0]);

		//detect whether it's a query
		bool is_query = true;
		if(EvaluableNode::IsNull(query_params))
		{
			is_query = false;
		}
		else if(!IsEvaluableNodeTypeQuery(query_params->GetType()))
		{
			if(query_params->GetType() == ENT_LIST)
			{
				auto &qp_ocn = query_params->GetOrderedChildNodesReference();
				if(qp_ocn.size() == 0)
					is_query = false;
				else if(!EvaluableNode::IsQuery(qp_ocn[0]))
					is_query = false;
			}
			else
			{
				is_query = false;
			}
		}

		if(!is_query)
			std::swap(entity_id_path, query_params);
	}
	else if(ocn.size() >= 2)
	{
		entity_id_path = InterpretNodeForImmediateUse(ocn[0]);
		auto node_stack = CreateOpcodeStackStateSaver(entity_id_path);
		query_params = InterpretNodeForImmediateUse(ocn[1]);
	}

	//if no query, just return all contained entities
	if(EvaluableNode::IsNull(query_params))
	{
		//in case the null was created
		evaluableNodeManager->FreeNodeTreeIfPossible(query_params);

		EntityReadReference source_entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(curEntity, entity_id_path);
		evaluableNodeManager->FreeNodeTreeIfPossible(entity_id_path);
		if(source_entity == nullptr)
			return EvaluableNodeReference::Null();

		auto &contained_entities = source_entity->GetContainedEntities();

		//if only looking for how many entities are contained, quickly exit
		if(immediate_result)
			return EvaluableNodeReference(static_cast<double>(contained_entities.size()));

		//new list containing the contained entity ids to return
		EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_LIST), true);
		auto &result_ocn = result->GetOrderedChildNodesReference();
		result_ocn.resize(contained_entities.size());

		//create the string references all at once and hand off
		for(size_t i = 0; i < contained_entities.size(); i++)
			result_ocn[i] = evaluableNodeManager->AllocNode(ENT_STRING, contained_entities[i]->GetIdStringId());

		//if not using SBFDS, make sure always return in the same order for consistency, regardless of cashing, hashing, etc.
		//if using SBFDS, then the order is assumed to not matter for other queries, so don't pay the cost of sorting here
		if(!_enable_SBF_datastore)
			std::sort(begin(result->GetOrderedChildNodes()), end(result->GetOrderedChildNodes()), EvaluableNode::IsStrictlyLessThan);

		return result;
	}

	//parse ordered child nodes into conditions
	conditionsBuffer.clear();
	for(auto &cn : query_params->GetOrderedChildNodes())
	{
		if(cn == nullptr)
			continue;

		EvaluableNodeType type = cn->GetType();
		switch(type)
		{
			case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
			case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
			case ENT_COMPUTE_ENTITY_CONVICTIONS:
			case ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE:
			case ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS:
			case ENT_COMPUTE_ENTITY_KL_DIVERGENCES:
				EntityQueryBuilder::BuildDistanceCondition(cn, type, conditionsBuffer, randomStream);
				break;

			default:
				EntityQueryBuilder::BuildNonDistanceCondition(cn, type, conditionsBuffer, randomStream);
				break;
		}
	}

	//if not a valid query, return nullptr
	if(conditionsBuffer.size() == 0)
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(entity_id_path);
		evaluableNodeManager->FreeNodeTreeIfPossible(query_params);
		return EvaluableNodeReference::Null();
	}

	EntityReadReference source_entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(curEntity, entity_id_path);
	evaluableNodeManager->FreeNodeTreeIfPossible(entity_id_path);
	if(source_entity == nullptr)
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(query_params);
		return EvaluableNodeReference::Null();
	}

	//perform query
	auto result = EntityQueryCaches::GetEntitiesMatchingQuery(source_entity,
		conditionsBuffer, evaluableNodeManager, return_query_value, immediate_result);

	//free query_params after the query just in case query_params is the only place that a given string id exists,
	//so the value isn't swapped out
	evaluableNodeManager->FreeNodeTreeIfPossible(query_params);
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes(EvaluableNode *en, bool immediate_result)
{
	//use stack to lock it in place, but copy it back to temporary before returning
	EvaluableNodeReference query_command(evaluableNodeManager->AllocNode(en->GetType()), true);

	auto node_stack = CreateOpcodeStackStateSaver(query_command);

	//propagate concurrency
	if(en->GetConcurrency())
		query_command->SetConcurrency(true);

	auto &ocn = en->GetOrderedChildNodesReference();
	query_command->ReserveOrderedChildNodes(ocn.size());
	auto &qc_ocn = query_command->GetOrderedChildNodesReference();
	for(size_t i = 0; i < ocn.size(); i++)
	{
		auto value = InterpretNode(ocn[i]);
		qc_ocn.push_back(value);
		query_command.UpdatePropertiesBasedOnAttachedNode(value, i == 0);
	}

	return query_command;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_LABEL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get label to look up
	size_t label_param_index = (ocn.size() > 1 ? 1 : 0);
	//don't need an extra reference because will be false anyway if the string doesn't exist
	StringInternPool::StringID label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[label_param_index]);
	if(label_sid == StringInternPool::NOT_A_STRING_ID)
		return EvaluableNodeReference::Null();

	//get the id of the entity
	EntityReadReference target_entity;
	if(ocn.size() > 1)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	//if no entity, clean up assignment assoc
	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	//make sure not trying to access a private label
	if(target_entity != curEntity && Entity::IsLabelPrivate(label_sid))
		return EvaluableNodeReference::Null();

	return AllocReturn(target_entity->DoesLabelExist(label_sid), immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_DIRECT_ASSIGN_TO_ENTITIES_and_ACCUM_TO_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();

	bool direct = (en->GetType() == ENT_DIRECT_ASSIGN_TO_ENTITIES);
	bool accum_assignment = (en->GetType() == ENT_ACCUM_TO_ENTITIES);

	bool all_assignments_successful = true;
	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get variables to assign
		size_t assoc_param_index = (i + 1 < ocn.size() ? i + 1 : i);
		auto assigned_vars = InterpretNode(ocn[assoc_param_index]);

		if(assigned_vars == nullptr || assigned_vars->GetType() != ENT_ASSOC)
		{
			all_assignments_successful = false;
			evaluableNodeManager->FreeNodeTreeIfPossible(assigned_vars);
			continue;
		}
		auto node_stack = CreateOpcodeStackStateSaver(assigned_vars);

		EntityWriteReference target_entity;
		if(i + 1 < ocn.size())
			target_entity = InterpretNodeIntoRelativeSourceEntityWriteReference(ocn[i]);
		else
			target_entity = EntityWriteReference(curEntity);

		//if no entity, can't successfully assign
		if(target_entity == nullptr)
		{
			all_assignments_successful = false;
			evaluableNodeManager->FreeNodeTreeIfPossible(assigned_vars);
			continue;
		}

		//need to make a copy if it's not safe for modification in case any other threads try to read from it
		bool copy_entity = !IsEntitySafeForModification(target_entity);

		size_t num_new_nodes_allocated = 0;
		auto [any_success, all_success] = target_entity->SetValuesAtLabels(
										assigned_vars, accum_assignment, direct, writeListeners,
										(ConstrainedAllocatedNodes() ? &num_new_nodes_allocated : nullptr),
										target_entity == curEntity, copy_entity);

		if(any_success)
		{
			if(ConstrainedAllocatedNodes())
				performanceConstraints->curNumAllocatedNodesAllocatedToEntities += num_new_nodes_allocated;

			if(target_entity == curEntity)
			{
				auto [any_constructions, initial_side_effect] = SetSideEffectsFlagsInConstructionStack();
				if(_opcode_profiling_enabled && any_constructions)
				{
					std::string variable_location = asset_manager.GetEvaluableNodeSourceFromComments(en);
					PerformanceProfiler::AccumulateTotalSideEffectMemoryWrites(variable_location);
					if(initial_side_effect)
						PerformanceProfiler::AccumulateInitialSideEffectMemoryWrites(variable_location);
				}
			}
			else
			{
			#ifdef AMALGAM_MEMORY_INTEGRITY
				VerifyEvaluableNodeIntegrity();
			#endif

			target_entity->CollectGarbageWithEntityWriteReference();
			
			#ifdef AMALGAM_MEMORY_INTEGRITY
				VerifyEvaluableNodeIntegrity();
			#endif
			}
		}
		//clear write lock as soon as possible
		target_entity = EntityWriteReference();

		//if assigning to a different entity, it can be cleared
		if(target_entity != curEntity)
		{
			node_stack.PopEvaluableNode();
			evaluableNodeManager->FreeNodeTreeIfPossible(assigned_vars);
		}

		if(!all_success)
			all_assignments_successful = false;

		//check this at the end of each iteration in case need to exit
		if(AreExecutionResourcesExhausted())
			return EvaluableNodeReference::Null();
	}

	return AllocReturn(all_assignments_successful, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE_FROM_ENTITY_and_DIRECT_RETRIEVE_FROM_ENTITY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get lookup reference
	size_t lookup_param_index = (ocn.size() > 1 ? 1 : 0);
	auto to_lookup = InterpretNodeForImmediateUse(ocn[lookup_param_index]);
	auto node_stack = CreateOpcodeStackStateSaver(to_lookup);

	bool direct = (en->GetType() == ENT_DIRECT_RETRIEVE_FROM_ENTITY);

	//get the id of the source to check
	EntityReadReference target_entity;
	if(ocn.size() > 1)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	//get the value(s)
	if(to_lookup == nullptr || to_lookup->IsImmediate())
	{
		StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(to_lookup);
		EvaluableNodeReference value;
		if(immediate_result)
			value.SetReference(
				target_entity->GetValueAtLabelAsImmediateValue(label_sid, target_entity == curEntity, evaluableNodeManager), true);
		else
			value = target_entity->GetValueAtLabel(label_sid, evaluableNodeManager, direct, target_entity == curEntity);
		evaluableNodeManager->FreeNodeTreeIfPossible(to_lookup);

		return value;
	}
	else if(to_lookup->IsAssociativeArray())
	{
		//reference to keep track of to_lookup nodes to free
		EvaluableNodeReference cnr(static_cast<EvaluableNode *>(nullptr), to_lookup.unique);

		evaluableNodeManager->EnsureNodeIsModifiable(to_lookup);

		//overwrite values in the ordered
		bool first_node = true;
		for(auto &[cn_id, cn] : to_lookup->GetMappedChildNodesReference())
		{
			//if there are values passed in, free them to be clobbered
			cnr.SetReference(cn);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			EvaluableNodeReference value = target_entity->GetValueAtLabel(cn_id, evaluableNodeManager, direct, target_entity == curEntity);

			cn = value;
			to_lookup.UpdatePropertiesBasedOnAttachedNode(value, first_node);
			first_node = false;
		}

		return to_lookup;
	}
	else //ordered params
	{
		//reference to keep track of to_lookup nodes to free
		EvaluableNodeReference cnr(static_cast<EvaluableNode *>(nullptr), to_lookup.unique);

		evaluableNodeManager->EnsureNodeIsModifiable(to_lookup);

		//overwrite values in the ordered
		auto &lookup_ocn = to_lookup->GetOrderedChildNodes();
		for(size_t i = 0; i < lookup_ocn.size(); i++)
		{
			auto &cn = lookup_ocn[i];
			StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(cn);

			//if there are values passed in, free them to be clobbered
			cnr.SetReference(cn);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			EvaluableNodeReference value = target_entity->GetValueAtLabel(label_sid, evaluableNodeManager, direct, target_entity == curEntity);

			cn = value;
			to_lookup.UpdatePropertiesBasedOnAttachedNode(value, i == 0);
		}

		return to_lookup;
	}
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_ENTITY_and_CALL_ENTITY_GET_CHANGES(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to check within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	StringRef entity_label_sid;
	if(ocn.size() > 1)
		entity_label_sid.SetIDWithReferenceHandoff(InterpretNodeIntoStringIDValueWithReference(ocn[1]));

	if(_label_profiling_enabled)
		PerformanceProfiler::StartOperation(string_intern_pool.GetStringFromID(entity_label_sid),
			evaluableNodeManager->GetNumberOfUsedNodes());

	PerformanceConstraints perf_constraints;
	PerformanceConstraints *perf_constraints_ptr = nullptr;
	if(PopulatePerformanceConstraintsFromParams(ocn, 3, perf_constraints, true))
		perf_constraints_ptr = &perf_constraints;

	//attempt to get arguments
	EvaluableNodeReference args = EvaluableNodeReference::Null();
	if(ocn.size() > 2)
		args = InterpretNodeForImmediateUse(ocn[2]);
	
	auto node_stack = CreateOpcodeStackStateSaver(args);

	//current pointer to write listeners
	std::vector<EntityWriteListener *> *cur_write_listeners = writeListeners;
	//another storage container in case getting entity changes
	std::vector<EntityWriteListener *> get_changes_write_listeners;
	if(en->GetType() == ENT_CALL_ENTITY_GET_CHANGES)
	{
		//add on extra listener and set pointer to this buffer
		// keep the copying here in this if statement so don't need to make copies when not calling ENT_CALL_ENTITY_GET_CHANGES
		if(writeListeners != nullptr)
			get_changes_write_listeners = *writeListeners;
		get_changes_write_listeners.push_back(new EntityWriteListener(curEntity, true));
		cur_write_listeners = &get_changes_write_listeners;
	}

	//get a write lock on the entity
	EntityReadReference called_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);

	if(called_entity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ce_enm = called_entity->evaluableNodeManager;

#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating call stack, then can release the entity lock
	Concurrency::ReadLock enm_lock(ce_enm.memoryModificationMutex);
	called_entity.lock.unlock();
#endif

	EvaluableNodeReference call_stack;
	if(called_entity == curEntity)
	{
		call_stack = ConvertArgsToCallStack(args, ce_enm);
		node_stack.PushEvaluableNode(call_stack);
	}
	else
	{
		//copy arguments to called_entity, free args from this entity
		EvaluableNodeReference called_entity_args = ce_enm.DeepAllocCopy(args);
		node_stack.PopEvaluableNode();
		evaluableNodeManager->FreeNodeTreeIfPossible(args);

		call_stack = ConvertArgsToCallStack(called_entity_args, ce_enm);
	}

	PopulatePerformanceCounters(perf_constraints_ptr, called_entity);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	EvaluableNodeReference result = called_entity->Execute(StringInternPool::StringID(entity_label_sid),
		call_stack, called_entity == curEntity, this, cur_write_listeners, printListener, perf_constraints_ptr
	#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
	#endif
		);

	ce_enm.FreeNode(call_stack->GetOrderedChildNodesReference()[0]);
	ce_enm.FreeNode(call_stack);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is executing again
	memoryModificationLock.lock();
#endif

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, &called_entity->evaluableNodeManager);

	if(called_entity != curEntity)
	{
		EvaluableNodeReference copied_result = evaluableNodeManager->DeepAllocCopy(result);
		ce_enm.FreeNodeTreeIfPossible(result);
		result = copied_result;
	}

	if(en->GetType() == ENT_CALL_ENTITY_GET_CHANGES)
	{
		EntityWriteListener *wl = get_changes_write_listeners.back();
		EvaluableNode *writes = wl->GetWrites();

		EvaluableNode *list = evaluableNodeManager->AllocNode(ENT_LIST);
		//copy the data out of the write listener
		list->AppendOrderedChildNode(result);
		list->AppendOrderedChildNode(evaluableNodeManager->DeepAllocCopy(writes));

		//delete the write listener and all of its memory
		delete wl;

		result.SetReference(list);
		result.SetNeedCycleCheck(true);	//can't count on that due to things written in the write listener
		result->SetIsIdempotent(false);
	}

	if(_label_profiling_enabled)
		PerformanceProfiler::EndOperation(evaluableNodeManager->GetNumberOfUsedNodes());

	if(performanceConstraints != nullptr)
		performanceConstraints->AccruePerformanceCounters(perf_constraints_ptr);

	if(perf_constraints_ptr != nullptr && perf_constraints_ptr->constraintsExceeded)
		return EvaluableNodeReference::Null();

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_CONTAINER(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a containing Entity to call
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto container_label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[0]);
	if(container_label_sid == string_intern_pool.NOT_A_STRING_ID
			|| !Entity::IsLabelAccessibleToContainedEntities(container_label_sid))
		return EvaluableNodeReference::Null();

	if(_label_profiling_enabled)
		PerformanceProfiler::StartOperation(string_intern_pool.GetStringFromID(container_label_sid),
			evaluableNodeManager->GetNumberOfUsedNodes());

	PerformanceConstraints perf_constraints;
	PerformanceConstraints *perf_constraints_ptr = nullptr;
	if(PopulatePerformanceConstraintsFromParams(ocn, 2, perf_constraints))
		perf_constraints_ptr = &perf_constraints;

	//attempt to get arguments
	EvaluableNodeReference args = EvaluableNodeReference::Null();
	if(ocn.size() > 1)
		args = InterpretNodeForImmediateUse(ocn[1]);

	//obtain a lock on the container
	EntityReadReference cur_entity(curEntity);
	StringInternPool::StringID cur_entity_sid = curEntity->GetIdStringId();
	EntityReadReference container(curEntity->GetContainer());
	if(container == nullptr)
		return EvaluableNodeReference::Null();
	//don't need the curEntity as a reference anymore -- can free the lock
	cur_entity = EntityReadReference();

#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating call stack, then can release the entity lock
	Concurrency::ReadLock enm_lock(container->evaluableNodeManager.memoryModificationMutex);
	container.lock.unlock();
#endif

	//copy arguments to container, free args from this entity
	EvaluableNodeReference called_entity_args = container->evaluableNodeManager.DeepAllocCopy(args);
	evaluableNodeManager->FreeNodeTreeIfPossible(args);

	EvaluableNodeReference call_stack = ConvertArgsToCallStack(called_entity_args, container->evaluableNodeManager);

	//add accessing_entity to arguments. If accessing_entity already specified (it shouldn't be), let garbage collection clean it up
	EvaluableNode *call_stack_args = call_stack->GetOrderedChildNodesReference()[0];
	call_stack_args->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_accessing_entity),
		container->evaluableNodeManager.AllocNode(ENT_STRING, cur_entity_sid));

	PopulatePerformanceCounters(perf_constraints_ptr, container);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	EvaluableNodeReference result = container->Execute(container_label_sid,
		call_stack, false, this, writeListeners, printListener, perf_constraints_ptr
	#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
	#endif
		);

	container->evaluableNodeManager.FreeNode(call_stack->GetOrderedChildNodesReference()[0]);
	container->evaluableNodeManager.FreeNode(call_stack);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is executing again
	memoryModificationLock.lock();
#endif

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, &container->evaluableNodeManager);

	EvaluableNodeReference copied_result = evaluableNodeManager->DeepAllocCopy(result);
	container->evaluableNodeManager.FreeNodeTreeIfPossible(result);

	if(_label_profiling_enabled)
		PerformanceProfiler::EndOperation(evaluableNodeManager->GetNumberOfUsedNodes());

	if(performanceConstraints != nullptr)
		performanceConstraints->AccruePerformanceCounters(perf_constraints_ptr);

	if(perf_constraints_ptr != nullptr && perf_constraints_ptr->constraintsExceeded)
		return EvaluableNodeReference::Null();

	return copied_result;
}
