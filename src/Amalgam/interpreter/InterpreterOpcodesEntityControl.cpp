//project headers:
#include "Interpreter.h"

#include "AssetManager.h"
#include "EntityExternalInterface.h"
#include "EvaluableNodeTreeFunctions.h"

//system headers:

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_COMMENTS(EvaluableNode *en, bool immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();

	StringInternPool::StringID label_sid = StringInternPool::NOT_A_STRING_ID;
	if(ocn.size() > 1)
		label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[1]);

	bool deep_comments = false;
	if(ocn.size() > 2)
		deep_comments = InterpretNodeIntoBoolValue(ocn[2]);

	//retrieve the entity after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	EntityReadReference target_entity;
	if(ocn.size() > 0)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	if(label_sid == StringInternPool::NOT_A_STRING_ID)
	{
		if(!deep_comments)
			return AllocReturn(EvaluableNode::GetCommentsStringId(target_entity->GetRoot()), immediate_result);

		EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

		//collect comments of each label
		target_entity->IterateFunctionOverLabels(
			[this, &retval]
			(StringInternPool::StringID label_sid, EvaluableNode *node)
			{
				//only include publicly facing labels
				if(Entity::IsLabelValidAndPublic(label_sid))
					retval->SetMappedChildNode(label_sid, evaluableNodeManager->AllocNode(ENT_STRING, EvaluableNode::GetCommentsStringId(node)));
			}
		);
		
		return retval;
	}

	auto label_value = target_entity->GetValueAtLabel(label_sid, nullptr, true).first;
	if(label_value == nullptr)
		return EvaluableNodeReference::Null();

	//has valid label
	if(!deep_comments)
		return AllocReturn(label_value->GetCommentsStringId(), immediate_result);

	//make sure a function based on declare that has parameters
	if(label_value->GetType() != ENT_DECLARE || label_value->GetOrderedChildNodes().size() < 1)
		return EvaluableNodeReference::Null();

	//the first element is an assoc of the parameters, the second element is the return value
	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);
	
	//if the vars are already initialized, then pull the comments from their values
	EvaluableNode *vars = label_value->GetOrderedChildNodes()[0];
	if(!EvaluableNode::IsAssociativeArray(vars))
		return retval;

	auto &retval_ocn = retval->GetOrderedChildNodesReference();
	retval_ocn.resize(2);

	//deep_comments of label, so get the parameters and their respective labels
	EvaluableNodeReference params_list(evaluableNodeManager->AllocNode(ENT_ASSOC), true);
	retval_ocn[0] = params_list;

	//get return comments
	retval_ocn[1] = evaluableNodeManager->AllocNode(ENT_STRING, vars->GetCommentsStringId());

	auto &mcn = vars->GetMappedChildNodesReference();
	params_list->ReserveMappedChildNodes(mcn.size());

	//create the string references all at once and hand off
	for(auto &[cn_id, cn] : mcn)
	{
		//create list with comment and default value
		EvaluableNodeReference param_info(evaluableNodeManager->AllocNode(ENT_LIST), true);
		auto &param_info_ocn = param_info->GetOrderedChildNodesReference();
		param_info_ocn.resize(2);
		param_info_ocn[0] = evaluableNodeManager->AllocNode(ENT_STRING, EvaluableNode::GetCommentsStringId(cn));
		param_info_ocn[1] = evaluableNodeManager->DeepAllocCopy(cn, EvaluableNodeManager::ENMM_REMOVE_ALL);

		//add to the params
		params_list->SetMappedChildNode(cn_id, param_info);
	}

	//ensure flags are updated since the node was already attached
	retval.UpdatePropertiesBasedOnAttachedNode(params_list);

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE_ENTITY_ROOT(EvaluableNode *en, bool immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();
	auto &ocn = en->GetOrderedChildNodes();

	//get second parameter if exists
	auto label_escape_increment = EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT;
	if(ocn.size() > 1)
	{
		auto value = InterpretNodeIntoNumberValue(ocn[1]);
		if(value)
			label_escape_increment = EvaluableNodeManager::ENMM_NO_CHANGE;
	}

	//retrieve the entity after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	EntityReadReference target_entity;
	if(ocn.size() > 0)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();


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
			new_code = InterpretNodeForImmediateUse(ocn[i + 1]);
		else
			new_code = InterpretNodeForImmediateUse(ocn[i]);
		auto node_stack = CreateOpcodeStackStateSaver(new_code);

		EntityWriteReference target_entity;
		if(i + 1 < ocn.size())
		{
			target_entity = InterpretNodeIntoRelativeSourceEntityWriteReference(ocn[i]);

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
			if(ConstrainedAllocatedNodes())
				interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += EvaluableNode::GetDeepSize(new_code);
		}
		else
		{
			size_t prev_size = 0;
			if(ConstrainedAllocatedNodes())
				prev_size = target_entity->GetSizeInNodes();

			target_entity->SetRoot(new_code, false, EvaluableNodeManager::ENMM_LABEL_ESCAPE_DECREMENT, writeListeners);

			if(ConstrainedAllocatedNodes())
			{
				size_t cur_size = target_entity->GetSizeInNodes();
				//don't get credit for freeing memory, but do count toward memory consumed
				if(cur_size > prev_size)
					interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += cur_size - prev_size;
			}
		}

		if(target_entity != curEntity)
		{
			//don't need to set side effects because the data was copied, not directly assigned
		#ifdef AMALGAM_MEMORY_INTEGRITY
			VerifyEvaluableNodeIntegrity();
		#endif

			target_entity->CollectGarbageWithEntityWriteReference();

		#ifdef AMALGAM_MEMORY_INTEGRITY
			VerifyEvaluableNodeIntegrity();
		#endif
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(new_code);
	}

	return AllocReturn(all_assignments_successful, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_RAND_SEED(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	EntityReadReference entity;
	if(ocn.size() > 0)
		entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else 
		entity = EntityReadReference(curEntity);

	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	std::string rand_state_string = entity->GetRandomState();

	return AllocReturn(rand_state_string, immediate_result);
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
		seed_string = Parser::Unparse(seed_node, false, false, true);
	auto node_stack = CreateOpcodeStackStateSaver(seed_node);

	//get the entity
	EntityWriteReference entity;
	if(num_params > 1)
		entity = InterpretNodeIntoRelativeSourceEntityWriteReference(ocn[0]);
	else
		entity = EntityWriteReference(curEntity);

	if(entity == nullptr)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(deep_set)
	{
		auto contained_entities = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityWriteReference>();
		if(contained_entities == nullptr)
			return EvaluableNodeReference::Null();

		entity->SetRandomState(seed_string, true, writeListeners, &contained_entities);
	}
	else
#endif
		entity->SetRandomState(seed_string, deep_set, writeListeners);

	return seed_node;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_PERMISSIONS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	auto all_permissions = EntityPermissions::AllPermissions();
	if(permissions.allPermissions != all_permissions.allPermissions)
		return EvaluableNodeReference::Null();

	EntityReadReference entity;
	if(ocn.size() > 0)
		entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		entity = EntityReadReference(curEntity);

	auto entity_permissions = asset_manager.GetEntityPermissions(entity);
	//clear lock
	entity = EntityReadReference();

	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_ASSOC), true);
	retval->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err),
		evaluableNodeManager->AllocNode(entity_permissions.individualPermissions.stdOutAndStdErr));
	retval->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_in),
		evaluableNodeManager->AllocNode(entity_permissions.individualPermissions.stdIn));
	retval->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_load),
		evaluableNodeManager->AllocNode(entity_permissions.individualPermissions.load));
	retval->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_store),
		evaluableNodeManager->AllocNode(entity_permissions.individualPermissions.store));
	retval->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_environment),
		evaluableNodeManager->AllocNode(entity_permissions.individualPermissions.environment));
	retval->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_alter_performance),
		evaluableNodeManager->AllocNode(entity_permissions.individualPermissions.alterPerformance));
	retval->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_system),
		evaluableNodeManager->AllocNode(entity_permissions.individualPermissions.system));

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ENTITY_PERMISSIONS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t num_params = ocn.size();

	if(num_params < 2)
		return EvaluableNodeReference::Null();

	//retrieve parameter to determine whether to deep set the seeds, if applicable
	bool deep_set = true;
	if(num_params > 2)
		deep_set = InterpretNodeIntoBoolValue(ocn[2], true);

	EvaluableNodeReference permissions_en = InterpretNodeForImmediateUse(ocn[1]);

	EntityPermissions permissions_to_set;
	EntityPermissions permission_values;
	if(EvaluableNode::IsAssociativeArray(permissions_en))
	{
		for(auto [permission_type, allow_en] : permissions_en->GetMappedChildNodes())
		{
			bool allow = EvaluableNode::IsTrue(allow_en);
			if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err))
			{
				permissions_to_set.individualPermissions.stdOutAndStdErr = true;
				permission_values.individualPermissions.stdOutAndStdErr = allow;
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_in))
			{
				permissions_to_set.individualPermissions.stdIn = true;
				permission_values.individualPermissions.stdIn = allow;
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_load))
			{
				permissions_to_set.individualPermissions.load = true;
				permission_values.individualPermissions.load = allow;
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_store))
			{
				permissions_to_set.individualPermissions.store = true;
				permission_values.individualPermissions.store = allow;
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_environment))
			{
				permissions_to_set.individualPermissions.environment = true;
				permission_values.individualPermissions.environment = allow;
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_alter_performance))
			{
				permissions_to_set.individualPermissions.alterPerformance = true;
				permission_values.individualPermissions.alterPerformance = allow;
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_system))
			{
				permissions_to_set.individualPermissions.system = true;
				permission_values.individualPermissions.system = allow;
			}
		}
	}
	else if(EvaluableNode::IsTrue(permissions_en))
	{
		permissions_to_set = EntityPermissions::AllPermissions();
		permission_values = EntityPermissions::AllPermissions();
	}
	//else false, leave permissions empty

	//any permissions set by this entity need to be filtered by the current entity's permissions
	auto current_entity_permissions = asset_manager.GetEntityPermissions(curEntity);
	permissions_to_set.allPermissions &= current_entity_permissions.allPermissions;
	permission_values.allPermissions &= current_entity_permissions.allPermissions;

	//get the id of the entity
	auto id_node = InterpretNode(ocn[0]);
	EntityWriteReference entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityWriteReference>(curEntity, id_node);

	if(entity == nullptr)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(deep_set)
	{
		auto contained_entities = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityWriteReference>();
		if(contained_entities == nullptr)
			return EvaluableNodeReference::Null();

		entity->SetPermissions(permissions_to_set, permission_values, true, writeListeners, &contained_entities);
	}
	else
#endif
		entity->SetPermissions(permissions_to_set, permission_values, deep_set, writeListeners);

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
	auto node_stack = CreateOpcodeStackStateSaver(new_entity_ids_list);

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//code will be the last parameter
		EvaluableNodeReference root = EvaluableNodeReference::Null();
		if(i + 1 == ocn.size())
			root = InterpretNodeForImmediateUse(ocn[i]);
		else
			root = InterpretNodeForImmediateUse(ocn[i + 1]);

		//get destination if applicable
		EntityWriteReference entity_container;
		StringRef new_entity_id;
		if(i + 1 < ocn.size())
		{
			node_stack.PushEvaluableNode(root);
			std::tie(entity_container, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[i]);
			node_stack.PopEvaluableNode();
		}
		else
		{
			entity_container = EntityWriteReference(curEntity);
		}

		if(entity_container == nullptr || !CanCreateNewEntityFromConstraints(entity_container, new_entity_id))
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		auto &new_entity_id_string = string_intern_pool.GetStringFromID(new_entity_id);
		std::string rand_state = entity_container->CreateRandomStreamFromStringAndRand(new_entity_id_string);

		//create new entity
		Entity *new_entity = new Entity(root, rand_state, EvaluableNodeManager::ENMM_LABEL_ESCAPE_DECREMENT);

		//accumulate usage
		if(ConstrainedAllocatedNodes())
			interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

		entity_container->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

		if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
		{
			delete new_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		if(entity_container == curEntity)
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
	auto node_stack = CreateOpcodeStackStateSaver(new_entity_ids_list);

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get the id of the source entity
		EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[i]);
		if(source_entity == nullptr)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		auto erbr = source_entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
		size_t num_new_entities = erbr->size();

		//create new entity
		Entity *new_entity = new Entity(source_entity);

		//clear previous locks
		source_entity = EntityReadReference();
		erbr.Clear();

		//get destination if applicable
		EntityWriteReference destination_entity_parent;
		StringRef new_entity_id;
		if(i + 1 < ocn.size())
			std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[i + 1]);

		if(destination_entity_parent == nullptr
			|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
		{
			delete new_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//accumulate usage
		if(ConstrainedAllocatedNodes())
			interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

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
	auto node_stack = CreateOpcodeStackStateSaver(new_entity_ids_list);

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get the id of the source entity
		auto source_id_node = InterpretNodeForImmediateUse(ocn[i]);

		auto [source_entity, source_entity_parent]
			= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityWriteReference>(curEntity, source_id_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(source_id_node);

		if(source_entity == nullptr || source_entity_parent == nullptr || source_entity == curEntity)
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
		source_entity_parent->RemoveContainedEntity(source_entity->GetIdStringId(), writeListeners);

		//clear lock if applicable
		source_entity_parent = EntityWriteReference();

		//get destination if applicable
		EntityWriteReference destination_entity_parent;
		StringRef new_entity_id;
		if(i + 1 < ocn.size())
			std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[i + 1]);
		else
			destination_entity_parent = EntityWriteReference(curEntity);

		if(destination_entity_parent == nullptr)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			delete source_entity;
			continue;
		}

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

	bool all_destroys_successful = true;
	for(auto &cn : en->GetOrderedChildNodes())
	{
		//get the id of the source entity
		auto id_node = InterpretNodeForImmediateUse(cn);
		auto [entity, entity_container]
			= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityWriteReference>(curEntity, id_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(id_node);

		//need a valid entity that isn't itself or currently has execution
		if(entity == nullptr || entity == curEntity || entity->IsEntityCurrentlyBeingExecuted())
		{
			all_destroys_successful = false;
			continue;
		}

		//lock all entities
		auto contained_entities = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityWriteReference>();
		if(contained_entities == nullptr)
		{
			all_destroys_successful = false;
			continue;
		}

		if(entity_container != nullptr)
			entity_container->RemoveContainedEntity(entity->GetIdStringId(), writeListeners);

		contained_entities.Clear();

	#ifdef MULTITHREAD_SUPPORT
		//free entity write lock before calling delete
		entity.lock.unlock();
	#endif

		//accumulate usage -- gain back freed resources
		if(ConstrainedAllocatedNodes())
			interpreterConstraints->curNumAllocatedNodesAllocatedToEntities -= entity->GetDeepSizeInNodes();

		delete entity;
	}

	return AllocReturn(all_destroys_successful, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOAD(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.individualPermissions.load)
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	std::string file_type = "";
	if(ocn.size() > 1)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[1]);
		if(valid)
			file_type = file_type_temp;
	}

	AssetManager::AssetParameters asset_params(path, file_type, false);

	if(ocn.size() > 2)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[2]);

		if(EvaluableNode::IsAssociativeArray(params))
			asset_params.SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params.UpdateResources();

	EntityExternalInterface::LoadEntityStatus status;
	return asset_manager.LoadResource(&asset_params, evaluableNodeManager, status);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOAD_ENTITY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.individualPermissions.load)
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	std::string file_type = "";
	if(ocn.size() > 2)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[2]);
		if(valid)
			file_type = file_type_temp;
	}

	bool persistent = false;
	if(ocn.size() > 3)
		persistent = InterpretNodeIntoBoolValue(ocn[3]);

	AssetManager::AssetParametersRef asset_params
		= std::make_shared<AssetManager::AssetParameters>(path, file_type, true);
	if(ocn.size() > 4)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[4]);

		if(EvaluableNode::IsAssociativeArray(params))
			asset_params->SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params->UpdateResources();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 1)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[1]);

	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	EntityExternalInterface::LoadEntityStatus status;
	std::string random_seed = destination_entity_parent->CreateRandomStreamFromStringAndRand(asset_params->resourcePath);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	Entity *loaded_entity = asset_manager.LoadEntityFromResource(asset_params, persistent, random_seed, this, status);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is executing again
	memoryModificationLock.lock();
#endif

	//handle errors
	if(!status.loaded)
		return EvaluableNodeReference::Null();

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += loaded_entity->GetDeepSizeInNodes();

	//put it in the destination
	destination_entity_parent->AddContainedEntityViaReference(loaded_entity, new_entity_id, writeListeners);

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id path
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, loaded_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_STORE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.individualPermissions.store)
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	auto to_store = InterpretNodeForImmediateUse(ocn[1]);
	auto node_stack = CreateOpcodeStackStateSaver(to_store);

	std::string file_type = "";
	if(ocn.size() > 2)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[2]);
		if(valid)
			file_type = file_type_temp;
	}

	AssetManager::AssetParameters asset_params(path, file_type, false);
	if(ocn.size() > 3)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[3]);
		
		if(EvaluableNode::IsAssociativeArray(params))
			asset_params.SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params.UpdateResources();

	bool successful_save = asset_manager.StoreResource(to_store, &asset_params, evaluableNodeManager);
	evaluableNodeManager->FreeNodeTreeIfPossible(to_store);

	return AllocReturn(successful_save, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_STORE_ENTITY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.individualPermissions.store)
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	std::string file_type = "";
	if(ocn.size() > 2)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[2]);
		if(valid)
			file_type = file_type_temp;
	}

	bool update_persistence = false;
	bool persistent = false;
	if(ocn.size() > 3)
	{
		auto persistence_node = InterpretNodeForImmediateUse(ocn[3]);
		if(!EvaluableNode::IsNull(persistence_node))
		{
			update_persistence = true;
			persistent = EvaluableNode::IsTrue(persistence_node);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(persistence_node);
	}

	AssetManager::AssetParametersRef asset_params
		= std::make_shared<AssetManager::AssetParameters>(path, file_type, true);
	if(ocn.size() > 4)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[4]);

		if(EvaluableNode::IsAssociativeArray(params))
			asset_params->SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params->UpdateResources();

	//get the id of the source entity to store.  Don't need to keep the reference because it won't be used once the source entity pointer is looked up
	//retrieve the entity after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	//StoreEntityToResource will read lock all contained entities appropriately
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[1]);
	if(source_entity == nullptr || source_entity == curEntity)
		return EvaluableNodeReference::Null();

	bool stored_successfully = asset_manager.StoreEntityToResource(source_entity, asset_params,
		update_persistence, persistent);

	return AllocReturn(stored_successfully, immediate_result);
}
