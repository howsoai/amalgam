//project headers:
#include "Interpreter.h"

#include "AssetManager.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EvaluableNodeTreeDifference.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"

//system headers:
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_MUTATE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto to_mutate = InterpretNodeForImmediateUse(ocn[0]);
	if(to_mutate == nullptr)
		to_mutate.SetReference(evaluableNodeManager->AllocNode(ENT_NULL));
	auto node_stack = CreateOpcodeStackStateSaver(to_mutate);

	double mutation_rate = 0.00001;
	if(ocn.size() > 1)
		mutation_rate = InterpretNodeIntoNumberValue(ocn[1]);

	bool ow_exists = false;
	CompactHashMap<EvaluableNodeType, double> opcode_weights;
	if(ocn.size() > 2)
	{
		auto opcode_weights_node = InterpretNodeForImmediateUse(ocn[2]);
		if(!EvaluableNode::IsNull(opcode_weights_node))
		{
			ow_exists = true;
			for(auto &[node_id, node] : opcode_weights_node->GetMappedChildNodes())
				opcode_weights[GetEvaluableNodeTypeFromStringId(node_id)] = EvaluableNode::ToNumber(node);

			evaluableNodeManager->FreeNodeTreeIfPossible(opcode_weights_node);
		}
	}

	bool mtw_exists = false;
	CompactHashMap<EvaluableNodeBuiltInStringId, double> mutation_type_weights;
	if(ocn.size() > 3)
	{
		auto mutation_weights_node = InterpretNodeForImmediateUse(ocn[3]);
		if(!EvaluableNode::IsNull(mutation_weights_node))
		{
			mtw_exists = true;
			for(auto &[node_id, node] : mutation_weights_node->GetMappedChildNodes())
			{
				auto bisid = GetBuiltInStringIdFromStringId(node_id);
				mutation_type_weights[bisid] = EvaluableNode::ToNumber(node);
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(mutation_weights_node);
		}
	}

	//result contains the copied result which may incur replacements
	EvaluableNode *result = EvaluableNodeTreeManipulation::MutateTree(this, evaluableNodeManager,
		to_mutate, mutation_rate, mtw_exists ? &mutation_type_weights : nullptr, ow_exists ? &opcode_weights : nullptr);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, true);
}

//TODO 24995: update queries and generalized distance, including docs and unit tests

EvaluableNodeReference Interpreter::InterpretNode_ENT_COMMONALITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool string_edit_distance = false;
	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_string_edit_distance, string_edit_distance);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	//calculate edit distance based commonality if string edit distance
	if(string_edit_distance)
	{
		size_t s1_len = 0;
		size_t s2_len = 0;
		auto edit_distance = EvaluableNodeTreeManipulation::EditDistance(
			EvaluableNode::ToString(ocn[0]), EvaluableNode::ToString(ocn[1]), s1_len, s2_len);
		auto commonality = static_cast<double>(std::max(s1_len, s2_len) - edit_distance);
		return AllocReturn(commonality, immediate_result);
	}

	//otherwise, treat both as nodes and calculate node commonality
	auto tree1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(tree1);

	auto tree2 = InterpretNodeForImmediateUse(ocn[1]);
	auto results = EvaluableNodeTreeManipulation::NumberOfSharedNodes(tree1, tree2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);

	node_stack.PopEvaluableNode();

	evaluableNodeManager->FreeNodeTreeIfPossible(tree1);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree2);

	return AllocReturn(results.commonality, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EDIT_DISTANCE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool string_edit_distance = false;
	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_string_edit_distance, string_edit_distance);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	//otherwise, treat both as nodes and calculate node edit distance
	auto tree1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(tree1);

	auto tree2 = InterpretNodeForImmediateUse(ocn[1]);

	double edit_distance = 0.0;
	//calculate string edit distance if string edit distance
	if(string_edit_distance)
	{
		edit_distance = static_cast<double>(EvaluableNodeTreeManipulation::EditDistance(
			EvaluableNode::ToString(tree1), EvaluableNode::ToString(tree2)));
	}
	else
	{
		edit_distance = EvaluableNodeTreeManipulation::EditDistance(tree1, tree2,
			types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	}

	node_stack.PopEvaluableNode();

	evaluableNodeManager->FreeNodeTreeIfPossible(tree1);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree2);

	return AllocReturn(edit_distance, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);

	EvaluableNode *result = EvaluableNodeTreeManipulation::IntersectTrees(evaluableNodeManager, n1, n2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	evaluableNodeManager->FreeNodeTreeIfPossible(n1);
	evaluableNodeManager->FreeNodeTreeIfPossible(n2);

	return EvaluableNodeReference(result, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(n1);
	
	auto n2 = InterpretNodeForImmediateUse(ocn[1]);

	EvaluableNode *result = EvaluableNodeTreeManipulation::UnionTrees(evaluableNodeManager, n1, n2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	evaluableNodeManager->FreeNodeTreeIfPossible(n1);
	evaluableNodeManager->FreeNodeTreeIfPossible(n2);

	return EvaluableNodeReference(result, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIFFERENCE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PushEvaluableNode(n2);

	EvaluableNode *result = EvaluableNodeTreeDifference::DifferenceTrees(evaluableNodeManager, n1, n2);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	return EvaluableNodeReference(result, (n1.unique && n2.unique));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	double blend2 = 0.5; //default to half
	if(ocn.size() > 2)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[2]);
		if(!FastIsNaN(new_value))
			blend2 = new_value;
	}

	double blend1 = 1.0 - blend2; //default to the remainder
	if(ocn.size() > 3)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[3]);
		if(!FastIsNaN(new_value))
			blend1 = new_value;

		//if have a third parameter, then use the fractions in order (so need to swap)
		std::swap(blend1, blend2);
	}

	//clamp both blend values to be nonnegative
	blend1 = std::max(0.0, blend1);
	blend2 = std::max(0.0, blend2);

	//stop if have nothing
	if(blend1 == 0.0 && blend2 == 0.0)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	double similar_mix_chance = 0.0;
	if(ocn.size() > 4)
	{
		auto params = InterpretNodeForImmediateUse(ocn[4]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_similar_mix_chance, similar_mix_chance);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);

	EvaluableNode *result = EvaluableNodeTreeManipulation::MixTrees(randomStream.CreateOtherStreamViaRand(),
		evaluableNodeManager, n1, n2, blend1, blend2, similar_mix_chance,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	evaluableNodeManager->FreeNodeTreeIfPossible(n1);
	evaluableNodeManager->FreeNodeTreeIfPossible(n2);

	return EvaluableNodeReference(result, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TOTAL_ENTITY_SIZE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	auto erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
	double size = static_cast<double>(entity->GetDeepSizeInNodes());
	return AllocReturn(size, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_FLATTEN_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	bool include_rand_seeds = true;
	if(ocn.size() > 1)
		include_rand_seeds = InterpretNodeIntoBoolValue(ocn[1]);

	bool parallel_create = false;
	if(ocn.size() > 2)
		parallel_create = InterpretNodeIntoBoolValue(ocn[2]);

	bool include_version = false;
	if(ocn.size() > 3)
		include_version = InterpretNodeIntoBoolValue(ocn[3]);

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	auto erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
	return EntityManipulation::FlattenEntity(evaluableNodeManager, entity, erbr,
		include_rand_seeds, parallel_create, include_version);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MUTATE_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get mutation rate if applicable
	double mutation_rate = 0.00001;
	if(ocn.size() > 1)
		mutation_rate = InterpretNodeIntoNumberValue(ocn[1]);

	bool ow_exists = false;
	CompactHashMap<EvaluableNodeType, double> opcode_weights;
	if(ocn.size() > 3)
	{
		auto opcode_weights_node = InterpretNodeForImmediateUse(ocn[3]);
		if(!EvaluableNode::IsNull(opcode_weights_node))
		{
			ow_exists = true;
			for(auto &[node_id, node] : opcode_weights_node->GetMappedChildNodes())
				opcode_weights[GetEvaluableNodeTypeFromStringId(node_id)] = EvaluableNode::ToNumber(node);

			evaluableNodeManager->FreeNodeTreeIfPossible(opcode_weights_node);
		}
	}

	bool mtw_exists = false;
	CompactHashMap<EvaluableNodeBuiltInStringId, double> mutation_type_weights;
	if(ocn.size() > 4)
	{
		auto mutation_weights_node = InterpretNodeForImmediateUse(ocn[4]);
		if(!EvaluableNode::IsNull(mutation_weights_node))
		{
			mtw_exists = true;
			for(auto &[node_id, node] : mutation_weights_node->GetMappedChildNodes())
			{
				auto bisid = GetBuiltInStringIdFromStringId(node_id);
				mutation_type_weights[bisid] = EvaluableNode::ToNumber(node);
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(mutation_weights_node);
		}
	}

	//retrieve the entities after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	//get the id of the first source entity
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity == nullptr || source_entity == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by mutating
	Entity *new_entity = EntityManipulation::MutateEntity(this, source_entity, mutation_rate,
		mtw_exists ? &mutation_type_weights : nullptr, ow_exists ? &opcode_weights : nullptr);
	
	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	//clear lock if applicable
	source_entity = EntityReadReference();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 2)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[2]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_COMMONALITY_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	auto commonality = EntityManipulation::NumberOfSharedNodes(source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	return AllocReturn(commonality.commonality, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EDIT_DISTANCE_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	double edit_distance = EntityManipulation::EditDistance(source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	return AllocReturn(edit_distance, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//need a source entity, and shouldn't copy self
	if(source_entity_1 == curEntity || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::IntersectEntities(this, source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);

	//no longer need entity references
	erbr.Clear();

	size_t num_new_entities = new_entity->GetTotalNumContainedEntitiesIncludingSelf();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 3)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[3]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr
		|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNodeForImmediateUse(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//need a source entity, and shouldn't copy self
	if(source_entity_1 == curEntity || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::UnionEntities(this, source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);

	//no longer need entity references
	erbr.Clear();

	size_t num_new_entities = new_entity->GetTotalNumContainedEntitiesIncludingSelf();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 3)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[3]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr
		|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIFFERENCE_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto [entity_1, entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(entity_1 == nullptr || entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//can't difference with self
	if(entity_1 == curEntity || entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	return EntityManipulation::DifferenceEntities(this, entity_1, entity_2);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	double blend2 = 0.5; //default to half
	if(ocn.size() > 2)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[2]);
		if(!FastIsNaN(new_value))
			blend2 = new_value;
	}

	double blend1 = 1.0 - blend2; //default to the remainder
	if(ocn.size() > 3)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[3]);
		if(!FastIsNaN(new_value))
			blend1 = new_value;

		//if have a third parameter, then use the fractions in order (so need to swap)
		std::swap(blend1, blend2);
	}

	//clamp both blend values to be nonnegative
	blend1 = std::max(0.0, blend1);
	blend2 = std::max(0.0, blend2);

	//stop if have nothing
	if(blend1 == 0.0 && blend2 == 0.0)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	double similar_mix_chance = 0.0;
	double unnamed_entity_mix_chance = 0.2;
	if(ocn.size() > 4)
	{
		auto params = InterpretNodeForImmediateUse(ocn[4]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_similar_mix_chance, similar_mix_chance);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_unnamed_entity_mix_chance, unnamed_entity_mix_chance);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//need a source entity, and shouldn't copy self
	if(source_entity_1 == curEntity || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::MixEntities(this, source_entity_1, source_entity_2, blend1, blend2,
		similar_mix_chance, types_must_match, nominal_numbers, nominal_strings, recursive_matching, unnamed_entity_mix_chance);

	//no longer need entity references
	erbr.Clear();

	size_t num_new_entities = new_entity->GetTotalNumContainedEntitiesIncludingSelf();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 7)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[7]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr
		|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}
