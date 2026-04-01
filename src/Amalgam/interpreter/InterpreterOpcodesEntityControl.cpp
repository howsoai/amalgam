//project headers:
#include "Interpreter.h"

#include "AssetManager.h"
#include "EntityExternalInterface.h"
#include "EvaluableNodeTreeFunctions.h"

//system headers:

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_ANNOTATIONS_and_GET_ENTITY_COMMENTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

	bool get_entity_comments = (en->GetType() == ENT_GET_ENTITY_COMMENTS);

	StringInternPool::StringID label_sid = StringInternPool::NOT_A_STRING_ID;
	if(ocn.size() > 1)
		label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[1]);

	bool deep_comments_or_annotations = false;
	if(ocn.size() > 2)
		deep_comments_or_annotations = InterpretNodeIntoBoolValue(ocn[2]);

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
		if(!deep_comments_or_annotations)
		{
			EvaluableNode *root = target_entity->GetRoot();
			auto entity_description = (get_entity_comments ? root->GetCommentsString() : root->GetAnnotationsString());
			//if the top node doesn't have a description, try to obtain from the node with the null key
			if(entity_description.empty())
			{
				EvaluableNode **null_code = root->GetMappedChildNode(string_intern_pool.NOT_A_STRING_ID);
				if(null_code != nullptr)
					entity_description = (get_entity_comments ? EvaluableNode::GetCommentsString(*null_code) : EvaluableNode::GetAnnotationsString(*null_code));
			}
			return AllocReturn(entity_description, immediate_result);
		}

		EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

		//collect comments or annotations of each label
		target_entity->IterateFunctionOverLabels(
			[this, &retval, get_entity_comments]
			(StringInternPool::StringID label_sid, EvaluableNode *node)
			{
				//only include publicly facing labels
				if(Entity::IsLabelValidAndPublic(label_sid))
					retval->SetMappedChildNode(label_sid,
						evaluableNodeManager->AllocNode(get_entity_comments ? EvaluableNode::GetCommentsString(node) : EvaluableNode::GetAnnotationsString(node)));
			}
		);
		
		return retval;
	}

	auto label_value = target_entity->GetValueAtLabel(label_sid, nullptr).first;
	if(label_value == nullptr)
		return EvaluableNodeReference::Null();

	//has valid label
	if(!deep_comments_or_annotations)
		return AllocReturn(get_entity_comments ? label_value->GetCommentsString() : label_value->GetAnnotationsString(), immediate_result);

	//make sure a function based on declare that has parameters
	if(label_value->GetType() != ENT_DECLARE || label_value->GetOrderedChildNodes().size() < 1)
		return EvaluableNodeReference::Null();

	//the first element is an assoc of the parameters, the second element is the return value
	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);
	
	//if the vars are already initialized, then pull the comments or annotations from their values
	EvaluableNode *vars = label_value->GetOrderedChildNodes()[0];
	if(!EvaluableNode::IsAssociativeArray(vars))
		return retval;

	auto &retval_ocn = retval->GetOrderedChildNodesReference();
	retval_ocn.resize(2);

	//deep_comments_or_annotations of label, so get the parameters and their respective labels
	EvaluableNodeReference params_list(evaluableNodeManager->AllocNode(ENT_ASSOC), true);
	retval_ocn[0] = params_list;

	//get return comments or annotations
	retval_ocn[1] = evaluableNodeManager->AllocNode(
		get_entity_comments ? vars->GetCommentsString() : vars->GetAnnotationsString());

	auto &mcn = vars->GetMappedChildNodesReference();
	params_list->ReserveMappedChildNodes(mcn.size());

	//create the string references all at once and hand off
	for(auto &[cn_id, cn] : mcn)
	{
		//create list with comment and default value
		EvaluableNodeReference param_info(evaluableNodeManager->AllocNode(ENT_LIST), true);
		auto &param_info_ocn = param_info->GetOrderedChildNodesReference();
		param_info_ocn.resize(2);
		param_info_ocn[0] = evaluableNodeManager->AllocNode(
			get_entity_comments ? EvaluableNode::GetCommentsString(cn) : EvaluableNode::GetAnnotationsString(cn));
		param_info_ocn[1] = evaluableNodeManager->DeepAllocCopy(cn, false);

		//add to the params
		params_list->SetMappedChildNode(cn_id, param_info);
	}

	//ensure flags are updated since the node was already attached
	retval.UpdatePropertiesBasedOnAttachedNode(params_list);

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE_ENTITY_ROOT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();
	auto &ocn = en->GetOrderedChildNodesReference();

	//retrieve the entity after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	EntityReadReference target_entity;
	if(ocn.size() > 0)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();


	return target_entity->GetRoot(evaluableNodeManager);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_ENTITY_ROOTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

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

		//pause if allocating to another entity
		EvaluableNodeManager::LocalAllocationBufferPause lab_pause;
		if(target_entity != curEntity)
			lab_pause = evaluableNodeManager->PauseLocalAllocationBuffer();

		size_t prev_size = 0;
		if(ConstrainedAllocatedNodes())
			prev_size = target_entity->GetSizeInNodes();

		target_entity->SetRoot(new_code, false, writeListeners);

		if(ConstrainedAllocatedNodes())
		{
			size_t cur_size = target_entity->GetSizeInNodes();
			//don't get credit for freeing memory, but do count toward memory consumed
			if(cur_size > prev_size)
				interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += cur_size - prev_size;
		}

		lab_pause.Resume();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_RAND_SEED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ENTITY_RAND_SEED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
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
	auto seed_node = InterpretNode(ocn[num_params > 1 ? 1 : 0]);
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_PERMISSIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	EntityReadReference entity;
	if(ocn.size() > 0)
		entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		entity = EntityReadReference(curEntity);

	auto entity_permissions = asset_manager.GetEntityPermissions(entity);
	//clear lock
	entity = EntityReadReference();

	return EvaluableNodeReference(entity_permissions.GetPermissionsAsEvaluableNode(evaluableNodeManager), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ENTITY_PERMISSIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_params = ocn.size();

	if(num_params < 2)
		return EvaluableNodeReference::Null();

	//retrieve parameter to determine whether to deep set the seeds, if applicable
	bool deep_set = true;
	if(num_params > 2)
		deep_set = InterpretNodeIntoBoolValue(ocn[2], true);

	EvaluableNodeReference permissions_en = InterpretNodeForImmediateUse(ocn[1]);

	auto [permissions_to_set, permission_values] = ExecutionPermissions::EvaluableNodeToPermissions(permissions_en);

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




