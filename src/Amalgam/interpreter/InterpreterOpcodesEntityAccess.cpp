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

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	return AllocReturn(entity != nullptr, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	bool return_query_value = (en->GetType() == ENT_COMPUTE_ON_CONTAINED_ENTITIES);

	EvaluableNodeReference entity_id_path = EvaluableNodeReference::Null();
	auto &ocn = en->GetOrderedChildNodesReference();
	auto node_stack = CreateOpcodeStackStateSaver();

	//interpret and buffer nodes for querying conditions
	//can't use node_stack as the buffer because need to know details of whether it is freeable
	//since the condition nodes need to be kept around until after the query
	std::vector<EvaluableNodeReference> condition_nodes;
	for(size_t param_index = 0; param_index < ocn.size(); param_index++)
	{
		EvaluableNodeReference param_node = InterpretNodeForImmediateUse(ocn[param_index]);

		//see if first parameter is the entity id
		if(param_index == 0)
		{
			//detect whether it's a query
			bool is_query = true;
			if(EvaluableNode::IsNull(param_node))
			{
				is_query = false;
			}
			else if(!IsEvaluableNodeTypeQuery(param_node->GetType()))
			{
				if(param_node->GetType() == ENT_LIST)
				{
					auto &qp_ocn = param_node->GetOrderedChildNodesReference();
					//an empty list has the same outcome, so early skip
					if(qp_ocn.size() == 0)
						continue;

					if(!EvaluableNode::IsQuery(qp_ocn[0]))
						is_query = false;
				}
				else
				{
					is_query = false;
				}
			}

			if(!is_query)
			{
				entity_id_path = param_node;
				node_stack.PushEvaluableNode(entity_id_path);
				continue;
			}
		}

		//skip nulls so don't need to check later
		if(param_node == nullptr)
			continue;

		node_stack.PushEvaluableNode(param_node);
		condition_nodes.push_back(param_node);
	}

	//build conditions from condition_nodes
	//buffer to use as for parsing and querying conditions
	//one per thread to reuse memory
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
		static std::vector<EntityQueryCondition> conditions;

	conditions.clear();
	for(auto &cond_node : condition_nodes)
	{
		if(EvaluableNode::IsQuery(cond_node))
		{
			EvaluableNodeType type = cond_node->GetType();
			if(EntityQueryBuilder::IsEvaluableNodeTypeDistanceQuery(type))
				EntityQueryBuilder::BuildDistanceCondition(cond_node, type, conditions, randomStream);
			else
				EntityQueryBuilder::BuildNonDistanceCondition(cond_node, type, conditions, randomStream);
		}
		else if(cond_node->GetType() == ENT_LIST)
		{
			for(auto cn : cond_node->GetOrderedChildNodesReference())
			{
				if(!EvaluableNode::IsQuery(cn))
					continue;

				EvaluableNodeType type = cn->GetType();
				if(EntityQueryBuilder::IsEvaluableNodeTypeDistanceQuery(type))
					EntityQueryBuilder::BuildDistanceCondition(cn, type, conditions, randomStream);
				else
					EntityQueryBuilder::BuildNonDistanceCondition(cn, type, conditions, randomStream);
			}
		}
	}

	EntityReadReference source_entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(curEntity, entity_id_path);
	evaluableNodeManager->FreeNodeTreeIfPossible(entity_id_path);
	if(source_entity == nullptr)
	{
		for(auto &cond_node : condition_nodes)
			evaluableNodeManager->FreeNodeTreeIfPossible(cond_node);
		return EvaluableNodeReference::Null();
	}

	//if no query, just return all contained entities
	if(conditions.size() == 0)
	{
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

	//perform query
	auto result = EntityQueryCaches::GetEntitiesMatchingQuery(source_entity,
		conditions, evaluableNodeManager, return_query_value, immediate_result);

	for(auto &cond_node : condition_nodes)
		evaluableNodeManager->FreeNodeTreeIfPossible(cond_node);
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_QUERY_opcodes(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_LABEL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_DIRECT_ASSIGN_TO_ENTITIES_and_ACCUM_TO_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

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

		size_t num_new_nodes_allocated = 0;

		//TODO 21546: change this from false to the following line once entity writes can be modifed lock-free
		// IsEntitySafeForModification(target_entity);
		bool copy_entity = false;

		//pause if allocating to another entity
		EvaluableNodeManager::LocalAllocationBufferPause lab_pause;
		if(target_entity != curEntity)
			lab_pause = evaluableNodeManager->PauseLocalAllocationBuffer();

		auto [any_success, all_success] = target_entity->SetValuesAtLabels(
										assigned_vars, accum_assignment, direct, writeListeners,
										(ConstrainedAllocatedNodes() ? &num_new_nodes_allocated : nullptr), target_entity == curEntity, copy_entity);

		lab_pause.Resume();

		if(any_success)
		{
			if(ConstrainedAllocatedNodes())
				interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += num_new_nodes_allocated;

			if(target_entity == curEntity)
			{
				if(!assigned_vars.unique)
					SetSideEffectFlagsAndAccumulatePerformanceCounters(en);
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
		//clear write lock as soon as possible, but pull out pointer first to compare for gc
		Entity *target_entity_raw_ptr = target_entity;
		target_entity = EntityWriteReference();

		//if assigning to a different entity, it can be cleared
		if(target_entity_raw_ptr != curEntity)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE_FROM_ENTITY_and_DIRECT_RETRIEVE_FROM_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

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
				target_entity->GetValueAtLabelAsImmediateValue(label_sid, target_entity == curEntity, evaluableNodeManager).first, true);
		else
			value = target_entity->GetValueAtLabel(label_sid, evaluableNodeManager, direct, target_entity == curEntity).first;

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

			auto [value, _] = target_entity->GetValueAtLabel(cn_id, evaluableNodeManager, direct, target_entity == curEntity);

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
		auto &lookup_ocn = to_lookup->GetOrderedChildNodesReference();
		for(size_t i = 0; i < lookup_ocn.size(); i++)
		{
			auto &cn = lookup_ocn[i];
			StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(cn);

			//if there are values passed in, free them to be clobbered
			cnr.SetReference(cn);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			auto [value, _] = target_entity->GetValueAtLabel(label_sid, evaluableNodeManager, direct, target_entity == curEntity);

			cn = value;
			to_lookup.UpdatePropertiesBasedOnAttachedNode(value, i == 0);
		}

		return to_lookup;
	}
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_ENTITY_and_CALL_ENTITY_GET_CHANGES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

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

	InterpreterConstraints interpreter_constraints;
	InterpreterConstraints *interpreter_constraints_ptr = nullptr;
	if(PopulateInterpreterConstraintsFromParams(ocn, 3, interpreter_constraints, true))
		interpreter_constraints_ptr = &interpreter_constraints;

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
		get_changes_write_listeners.push_back(new EntityWriteListener(curEntity, std::unique_ptr<std::ostream>(), true));
		cur_write_listeners = &get_changes_write_listeners;
	}

	//get a write lock on the entity
	EntityReadReference called_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);

	if(called_entity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ce_enm = called_entity->evaluableNodeManager;

#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating scope stack, then can release the entity lock
	Concurrency::ReadLock enm_lock(ce_enm.memoryModificationMutex);
	called_entity.lock.unlock();
#endif

	EvaluableNodeReference scope_stack;
	if(called_entity == curEntity)
	{
		scope_stack = ConvertArgsToScopeStack(args, ce_enm);
		node_stack.PushEvaluableNode(scope_stack);
	}
	else
	{
		//copy arguments to called_entity, free args from this entity
		EvaluableNodeReference called_entity_args = ce_enm.DeepAllocCopy(args);
		node_stack.PopEvaluableNode();
		//don't put freed nodes in local allocation buffer, because that will increase memory churn
		evaluableNodeManager->FreeNodeTreeIfPossible(args, false);
		args = called_entity_args;

		scope_stack = ConvertArgsToScopeStack(args, ce_enm);
	}

	PopulatePerformanceCounters(interpreter_constraints_ptr, called_entity);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	EvaluableNodeReference result = called_entity->Execute(StringInternPool::StringID(entity_label_sid),
		scope_stack, called_entity == curEntity, this, cur_write_listeners, printListener, interpreter_constraints_ptr
	#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
	#endif
		);

	ce_enm.FreeNode(args);
	ce_enm.FreeNode(scope_stack);

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
		//don't put freed nodes in local allocation buffer, because that will increase memory churn
		ce_enm.FreeNodeTreeIfPossible(result, false);
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

	if(interpreterConstraints != nullptr)
		interpreterConstraints->AccruePerformanceCounters(interpreter_constraints_ptr);

	if(interpreter_constraints_ptr != nullptr && interpreter_constraints_ptr->constraintsExceeded)
		return BundleResultWithWarningsIfNeeded(EvaluableNodeReference::Null(), interpreter_constraints_ptr);

	return BundleResultWithWarningsIfNeeded(result, interpreter_constraints_ptr);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_CONTAINER(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

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

	InterpreterConstraints interpreter_constraints;
	InterpreterConstraints *interpreter_constraints_ptr = nullptr;
	if(PopulateInterpreterConstraintsFromParams(ocn, 2, interpreter_constraints))
		interpreter_constraints_ptr = &interpreter_constraints;

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
	//lock memory before allocating scope stack, then can release the entity lock
	Concurrency::ReadLock enm_lock(container->evaluableNodeManager.memoryModificationMutex);
	container.lock.unlock();
#endif

	//copy arguments to container, free args from this entity
	EvaluableNodeReference called_entity_args = container->evaluableNodeManager.DeepAllocCopy(args);
	//don't put freed nodes in local allocation buffer, because that will increase memory churn
	evaluableNodeManager->FreeNodeTreeIfPossible(args, false);

	EvaluableNodeReference scope_stack = ConvertArgsToScopeStack(called_entity_args, container->evaluableNodeManager);

	//add accessing_entity to arguments. If accessing_entity already specified (it shouldn't be), let garbage collection clean it up
	EvaluableNode *scope_stack_args = scope_stack->GetOrderedChildNodesReference()[0];
	scope_stack_args->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_accessing_entity),
		container->evaluableNodeManager.AllocNode(ENT_STRING, cur_entity_sid));

	PopulatePerformanceCounters(interpreter_constraints_ptr, container);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	EvaluableNodeReference result = container->Execute(container_label_sid,
		scope_stack, false, this, writeListeners, printListener, interpreter_constraints_ptr
	#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
	#endif
		);

	container->evaluableNodeManager.FreeNode(called_entity_args);
	container->evaluableNodeManager.FreeNode(scope_stack);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is executing again
	memoryModificationLock.lock();
#endif

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, &container->evaluableNodeManager);

	EvaluableNodeReference copied_result = evaluableNodeManager->DeepAllocCopy(result);
	//don't put freed nodes in local allocation buffer, because that will increase memory churn
	container->evaluableNodeManager.FreeNodeTreeIfPossible(result, false);

	if(_label_profiling_enabled)
		PerformanceProfiler::EndOperation(evaluableNodeManager->GetNumberOfUsedNodes());

	if(interpreterConstraints != nullptr)
		interpreterConstraints->AccruePerformanceCounters(interpreter_constraints_ptr);

	if(interpreter_constraints_ptr != nullptr && interpreter_constraints_ptr->constraintsExceeded)
		return BundleResultWithWarningsIfNeeded(EvaluableNodeReference::Null(), interpreter_constraints_ptr);

	return BundleResultWithWarningsIfNeeded(copied_result, interpreter_constraints_ptr);
}
