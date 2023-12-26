//project headers:
#include "Interpreter.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"
#include "EvaluableNodeTreeDifference.h"
#include "PerformanceProfiler.h"

//system headers:
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_COMMENTS(EvaluableNode *en, bool immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();

	EntityReadReference target_entity;
	if(ocn.size() > 0)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	StringInternPool::StringID label_sid = StringInternPool::NOT_A_STRING_ID;
	if(ocn.size() > 1)
		label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[1]);

	bool deep_comments = false;
	if(ocn.size() > 2)
		deep_comments = InterpretNodeIntoBoolValue(ocn[2]);

	if(label_sid == StringInternPool::NOT_A_STRING_ID)
	{
		if(!deep_comments)
			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, EvaluableNode::GetCommentsStringId(target_entity->GetRoot())), true);

		EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

		//collect comments of each label
		target_entity->IterateFunctionOverLabels(
			[this, &retval]
			(StringInternPool::StringID label_sid, EvaluableNode *node)
			{
				//don't include those that are only inwardly facing
				if(!Entity::IsLabelAccessibleToContainedEntities(label_sid))
					retval->SetMappedChildNode(label_sid, evaluableNodeManager->AllocNode(ENT_STRING, EvaluableNode::GetCommentsStringId(node)));
			}
		);
		
		return retval;
	}

	auto label_value = target_entity->GetValueAtLabel(label_sid, nullptr, true);

	//has valid label
	if(!deep_comments)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, label_value->GetCommentsStringId()), true);

	//make sure a function based on declare that has parameters
	if(label_value == nullptr || label_value->GetType() != ENT_DECLARE || label_value->GetOrderedChildNodes().size() < 1)
		return EvaluableNodeReference::Null();

	//deep_comments of label, so get the parameters and their respective labels
	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

	EvaluableNode *vars = label_value->GetOrderedChildNodes()[0];
	
	//if the vars are already initialized, then pull the comments from their values
	if(EvaluableNode::IsAssociativeArray(vars))
	{
		auto &mcn = vars->GetMappedChildNodesReference();
		retval->ReserveMappedChildNodes(mcn.size());

		//create the string references all at once and hand off
		string_intern_pool.CreateStringReferences(mcn, [](auto it) { return it.first; });
		for(auto &[cn_id, cn] : mcn)
			retval->SetMappedChildNodeWithReferenceHandoff(cn_id, evaluableNodeManager->AllocNode(ENT_STRING, EvaluableNode::GetCommentsStringId(cn)));

		return retval;
	}

	//the vars are not initialized, which means the comments are on the parameters
	retval->ReserveMappedChildNodes(vars->GetOrderedChildNodes().size() / 2);
	for(size_t index = 0; index < vars->GetOrderedChildNodes().size(); index += 2)
	{
		EvaluableNode *variable_name_node = vars->GetOrderedChildNodes()[index];
		StringInternPool::StringID sid = EvaluableNode::ToStringIDIfExists(variable_name_node);
		if(sid == StringInternPool::NOT_A_STRING_ID)
			continue;

		retval->SetMappedChildNode(sid, evaluableNodeManager->AllocNode(ENT_STRING, EvaluableNode::GetCommentsStringId(variable_name_node)));
	}

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE_ENTITY_ROOT(EvaluableNode *en, bool immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get entity by id parameter if exists
	EntityReadReference target_entity;
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() > 0)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	else 
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	//get second parameter if exists
	auto label_escape_increment = EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT;
	if(ocn.size() > 1)
	{
		auto value = InterpretNodeIntoNumberValue(ocn[1]);
		if(value)
			label_escape_increment = EvaluableNodeManager::ENMM_NO_CHANGE;
	}

	return target_entity->GetRoot(evaluableNodeManager, label_escape_increment);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_ENTITY_ROOTS_and_ACCUM_ENTITY_ROOTS(EvaluableNode *en, bool immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();

	bool accum = (en->GetType() == ENT_ACCUM_ENTITY_ROOTS);
	bool all_assignments_successful = true;

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get value to assign first before getting the entity in case it needs to be locked
		EvaluableNodeReference new_code = EvaluableNodeReference::Null();
		if(i + 1 < ocn.size())
			new_code = InterpretNode(ocn[i + 1]);
		else
			new_code = InterpretNode(ocn[i]);
		auto node_stack = CreateInterpreterNodeStackStateSaver(new_code);

		EntityWriteReference target_entity;
		if(i + 1 < ocn.size())
		{
			target_entity = InterpretNodeIntoRelativeSourceEntityWriteReferenceFromInterpretedEvaluableNodeIDPath(ocn[i]);

			//if didn't find an entity, then use current one
			if(target_entity == nullptr)
			{
				all_assignments_successful = false;
				evaluableNodeManager->FreeNodeTreeIfPossible(new_code);
				continue;
			}
		}
		else
		{
			target_entity = EntityWriteReference(curEntity);
		}

		if(accum)
		{
			target_entity->AccumRoot(new_code, false, EvaluableNodeManager::ENMM_LABEL_ESCAPE_DECREMENT, writeListeners);
			
			//accumulate new node usage
			if(!AllowUnlimitedExecutionNodes())
				curNumExecutionNodesAllocatedToEntities += EvaluableNode::GetDeepSize(new_code);
		}
		else
		{
			size_t prev_size = 0;
			if(!AllowUnlimitedExecutionNodes())
				prev_size = target_entity->GetSizeInNodes();

			target_entity->SetRoot(new_code, false, EvaluableNodeManager::ENMM_LABEL_ESCAPE_DECREMENT, writeListeners);

			if(!AllowUnlimitedExecutionNodes())
			{
				size_t cur_size = target_entity->GetSizeInNodes();
				//don't get credit for freeing memory, but do count toward memory consumed
				if(cur_size > prev_size)
					curNumExecutionNodesAllocatedToEntities += cur_size - prev_size;
			}
		}

		target_entity->evaluableNodeManager.AdvanceGarbageCollectionTrigger();

	#ifdef MULTITHREAD_SUPPORT
		target_entity->CollectGarbage(&memoryModificationLock);
	#else
		target_entity->CollectGarbage();
	#endif
	}

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(all_assignments_successful ? ENT_TRUE : ENT_FALSE), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_RAND_SEED(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to retrieve others from
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get the id of the entity
	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	std::string rand_state_string = entity->GetRandomState();

	if(immediate_result)
		return EvaluableNodeReference(rand_state_string);
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, rand_state_string), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ENTITY_RAND_SEED(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t num_params = ocn.size();

	if(num_params < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to retrieve others from
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//retrieve parameter to determine whether to deep set the seeds, if applicable
	bool deep_set = true;
	if(num_params == 3)
		deep_set = InterpretNodeIntoBoolValue(ocn[2], true);

	//the opcode parameter index of the seed
	auto seed_node = InterpretNodeForImmediateUse(ocn[num_params > 1 ? 1 : 0]);
	std::string seed_string;
	if(seed_node != nullptr && seed_node->GetType() == ENT_STRING)
		seed_string = seed_node->GetStringValue();
	else
		seed_string = Parser::Unparse(seed_node, evaluableNodeManager, false, false, true);
	auto node_stack = CreateInterpreterNodeStackStateSaver(seed_node);

	//get the entity
	EntityWriteReference entity;
	if(num_params > 1)
		entity = InterpretNodeIntoRelativeSourceEntityWriteReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	else
		entity = EntityWriteReference(curEntity);

	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	entity->SetRandomState(seed_string, deep_set, writeListeners);

	return seed_node;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_ROOT_PERMISSION(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	if(!asset_manager.DoesEntityHaveRootPermission(curEntity))
		return EvaluableNodeReference::Null();

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	return AllocReturn(asset_manager.DoesEntityHaveRootPermission(entity), immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ENTITY_ROOT_PERMISSION(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	if(!asset_manager.DoesEntityHaveRootPermission(curEntity))
		return EvaluableNodeReference::Null();

	bool permission = InterpretNodeIntoBoolValue(ocn[1]);

	//get the id of the entity
	auto id_node = InterpretNode(ocn[0]);
	EntityWriteReference entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityWriteReference>(curEntity, id_node);

	asset_manager.SetRootPermission(entity, permission);

	return id_node;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CREATE_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();

	EvaluableNodeReference new_entity_ids_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	new_entity_ids_list->ReserveOrderedChildNodes((ocn.size() + 1) / 2);
	auto node_stack = CreateInterpreterNodeStackStateSaver(new_entity_ids_list);

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//code will be the last parameter
		EvaluableNodeReference root = EvaluableNodeReference::Null();
		if(i + 1 == ocn.size())
			root = InterpretNodeForImmediateUse(ocn[i]);
		else
			root = InterpretNodeForImmediateUse(ocn[i + 1]);

		//get destination if applicable
		StringInternRef new_entity_id;
		Entity *destination_entity_parent = curEntity;
		if(i + 1 < ocn.size())
		{
			node_stack.PushEvaluableNode(root);
			InterpretNodeIntoDestinationEntity(ocn[i], destination_entity_parent, new_entity_id);
			node_stack.PopEvaluableNode();
		}

		if(destination_entity_parent == nullptr)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//create new entity
		Entity *new_entity = new Entity();
		new_entity->SetRoot(root, false, EvaluableNodeManager::ENMM_LABEL_ESCAPE_DECREMENT);

		//accumulate usage
		if(!AllowUnlimitedExecutionNodes())
			curNumExecutionNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

		const std::string &new_entity_id_string = string_intern_pool.GetStringFromID(new_entity_id);
		new_entity->SetRandomState(destination_entity_parent->CreateRandomStreamFromStringAndRand(new_entity_id_string), false);

		destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

		if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
		{
			delete new_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		if(destination_entity_parent == curEntity)
			new_entity_ids_list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id));
		else //need an id path
			new_entity_ids_list->AppendOrderedChildNode(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity));
	}

	return new_entity_ids_list;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CLONE_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();

	EvaluableNodeReference new_entity_ids_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	new_entity_ids_list->ReserveOrderedChildNodes((ocn.size() + 1) / 2);
	auto node_stack = CreateInterpreterNodeStackStateSaver(new_entity_ids_list);

	//TODO 10975: change this to lock all entities at once
	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get the id of the source entity
		Entity *source_entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[i]);
		//need a source entity, and can't copy self! (that could cause badness)
		if(source_entity == nullptr || source_entity == curEntity)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//get destination if applicable
		StringInternRef new_entity_id;
		Entity *destination_entity_parent = curEntity;
		if(i + 1 < ocn.size())
			InterpretNodeIntoDestinationEntity(ocn[i + 1], destination_entity_parent, new_entity_id);
		if(destination_entity_parent == nullptr)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//create new entity
		Entity *new_entity = new Entity(source_entity);

		//accumulate usage
		if(!AllowUnlimitedExecutionNodes())
			curNumExecutionNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

		destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

		if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
		{
			delete new_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		if(destination_entity_parent == curEntity)
			new_entity_ids_list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id));
		else //need an id path
			new_entity_ids_list->AppendOrderedChildNode(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity));
	}

	return new_entity_ids_list;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MOVE_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();

	EvaluableNodeReference new_entity_ids_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	new_entity_ids_list->ReserveOrderedChildNodes((ocn.size() + 1) / 2);
	auto node_stack = CreateInterpreterNodeStackStateSaver(new_entity_ids_list);

	//TODO 10975: change this to lock the entities
	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get the id of the source entity
		auto source_id_node = InterpretNodeForImmediateUse(ocn[i]);

		StringInternRef source_entity_id;
		Entity *source_entity_parent = nullptr, *source_entity = nullptr;
		TraverseToEntityViaEvaluableNodeIDPath(curEntity, source_id_node, source_entity_parent, source_entity_id, source_entity);
		evaluableNodeManager->FreeNodeTreeIfPossible(source_id_node);

		if(source_entity == nullptr || source_entity_parent == nullptr || source_entity == curEntity)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//get destination if applicable
		StringInternRef new_entity_id;
		Entity *destination_entity_parent = curEntity;
		if(i + 1 < ocn.size())
			InterpretNodeIntoDestinationEntity(ocn[i + 1], destination_entity_parent, new_entity_id);
		if(destination_entity_parent == nullptr)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//can't move if being executed
		if(source_entity->IsEntityCurrentlyBeingExecuted())
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//remove source entity from its parent
		source_entity_parent->RemoveContainedEntity(source_entity_id, writeListeners);

		//put it in the destination
		destination_entity_parent->AddContainedEntityViaReference(source_entity, new_entity_id, writeListeners);

		if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
		{
			delete source_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		if(destination_entity_parent == curEntity)
			new_entity_ids_list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id));
		else //need an id path
			new_entity_ids_list->AppendOrderedChildNode(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, source_entity));
	}

	return new_entity_ids_list;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DESTROY_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//TODO 10975: change this to lock all entities at once
	bool all_destroys_successful = true;
	for(auto &cn : en->GetOrderedChildNodes())
	{
		//get the id of the source entity
		auto source_id_node = InterpretNodeForImmediateUse(cn);
		Entity *source_entity = nullptr, *source_entity_parent = nullptr;
		StringInternRef source_id;
		TraverseToEntityViaEvaluableNodeIDPath(curEntity, source_id_node, source_entity_parent, source_id, source_entity);
		evaluableNodeManager->FreeNodeTreeIfPossible(source_id_node);

		//need a source entity, and can't destroy self! (that could cause badness)
		if(source_entity == nullptr || source_entity == curEntity)
		{
			all_destroys_successful = false;
			continue;
		}

		if(source_entity->IsEntityCurrentlyBeingExecuted())
		{
			all_destroys_successful = false;
			continue;
		}

		if(source_entity_parent != nullptr)
			source_entity_parent->RemoveContainedEntity(source_id, writeListeners);

		delete source_entity;
	}

	return AllocReturn(all_destroys_successful, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOAD(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	if(!asset_manager.DoesEntityHaveRootPermission(curEntity))
		return EvaluableNodeReference::Null();

	std::string resource_name = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(resource_name == "")
		return EvaluableNodeReference::Null();

	bool escape_filename = false;
	if(ocn.size() >= 2)
		escape_filename = InterpretNodeIntoBoolValue(ocn[1], false);

	std::string file_type = "";
	if(ocn.size() >= 3)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[2]);
		if(valid)
			file_type = file_type_temp;
	}

	std::string resource_base_path;
	return asset_manager.LoadResourcePath(resource_name, resource_base_path, file_type, evaluableNodeManager, escape_filename);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOAD_ENTITY_and_LOAD_PERSISTENT_ENTITY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	if(!asset_manager.DoesEntityHaveRootPermission(curEntity))
		return EvaluableNodeReference::Null();

	std::string resource_name = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(resource_name == "")
		return EvaluableNodeReference::Null();

	//get destination if applicable
	StringInternRef new_entity_id;
	Entity *destination_entity_parent = curEntity;
	if(ocn.size() >= 2)
		InterpretNodeIntoDestinationEntity(ocn[1], destination_entity_parent, new_entity_id);
	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	bool escape_filename = false;
	if(ocn.size() >= 3)
		escape_filename = InterpretNodeIntoBoolValue(ocn[2], false);

	bool escape_contained_filenames = true;
	if(ocn.size() >= 4)
		escape_contained_filenames = InterpretNodeIntoBoolValue(ocn[3], true);

	bool persistent = (en->GetType() == ENT_LOAD_PERSISTENT_ENTITY);
	if(persistent)
		escape_contained_filenames = true;

	//persistent doesn't allow file_type
	std::string file_type = "";
	if(!persistent && ocn.size() >= 5)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[4]);
		if(valid)
			file_type = file_type_temp;
	}

	std::string random_seed = destination_entity_parent->CreateRandomStreamFromStringAndRand(resource_name);
	Entity *loaded_entity = asset_manager.LoadEntityFromResourcePath(resource_name, file_type,
		persistent, true, escape_filename, escape_contained_filenames, random_seed);

	//handle errors
	if(loaded_entity == nullptr)
		return EvaluableNodeReference::Null();
	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete loaded_entity;
		return EvaluableNodeReference::Null();
	}

	//accumulate usage
	if(!AllowUnlimitedExecutionNodes())
		curNumExecutionNodesAllocatedToEntities += loaded_entity->GetDeepSizeInNodes();

	//put it in the destination
	destination_entity_parent->AddContainedEntityViaReference(loaded_entity, new_entity_id, writeListeners);

	if(destination_entity_parent == curEntity)
	{
		if(immediate_result)
			return EvaluableNodeReference(static_cast<StringInternPool::StringID>(new_entity_id));
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id), true);
	}
	else //need to return an id path
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, loaded_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_STORE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	if(!asset_manager.DoesEntityHaveRootPermission(curEntity))
		return EvaluableNodeReference::Null();

	std::string resource_name = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(resource_name == "")
		return EvaluableNodeReference::Null();

	auto to_store = InterpretNodeForImmediateUse(ocn[1]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(to_store);

	bool escape_filename = false;
	if(ocn.size() >= 3)
		escape_filename = InterpretNodeIntoBoolValue(ocn[2], false);

	std::string file_type = "";
	if(ocn.size() >= 4)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[3]);
		if(valid)
			file_type = file_type_temp;
	}

	bool sort_keys = false;
	if(ocn.size() >= 5)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[4]);
		
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();

			auto found_sort_keys = mcn.find(ENBISI_sort_keys);
			if(found_sort_keys != end(mcn))
				sort_keys = EvaluableNode::IsTrue(found_sort_keys->second);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	std::string resource_base_path;
	bool successful_save = asset_manager.StoreResourcePath(to_store,
		resource_name, resource_base_path, file_type, evaluableNodeManager, escape_filename, sort_keys);

	evaluableNodeManager->FreeNodeTreeIfPossible(to_store);

	if(immediate_result)
		return EvaluableNodeReference(successful_save);
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(successful_save ? ENT_TRUE : ENT_FALSE), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_STORE_ENTITY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	if(!asset_manager.DoesEntityHaveRootPermission(curEntity))
		return EvaluableNodeReference::Null();

	std::string resource_name = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(resource_name == "")
		return EvaluableNodeReference::Null();

	//TODO 10975: lock entire entity tree
	//get the id of the source entity to store.  Don't need to keep the reference because it won't be used once the source entety pointer is looked up
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[1]);

	if(source_entity == nullptr || source_entity == curEntity)
		return EvaluableNodeReference::Null();

	bool escape_filename = false;
	if(ocn.size() >= 3)
		escape_filename = InterpretNodeIntoBoolValue(ocn[2], false);

	bool escape_contained_filenames = true;
	if(ocn.size() >= 4)
		escape_contained_filenames = InterpretNodeIntoBoolValue(ocn[3], true);

	std::string file_type = "";
	if(ocn.size() >= 5)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[4]);
		if(valid)
			file_type = file_type_temp;
	}

	bool sort_keys = false;
	if(ocn.size() >= 6)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[5]);

		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();

			auto found_sort_keys = mcn.find(ENBISI_sort_keys);
			if(found_sort_keys != end(mcn))
				sort_keys = EvaluableNode::IsTrue(found_sort_keys->second);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	bool stored_successfully = asset_manager.StoreEntityToResourcePath(source_entity, resource_name, file_type,
		false, true, escape_filename, escape_contained_filenames, sort_keys);

	return AllocReturn(stored_successfully, immediate_result);
}
