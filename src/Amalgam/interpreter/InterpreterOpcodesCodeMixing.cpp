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

EvaluableNodeReference Interpreter::InterpretNode_ENT_MUTATE(EvaluableNode *en, bool immediate_result)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_COMMONALITY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool use_string_edit_distance = false;
	if(ocn.size() > 2)
		use_string_edit_distance = InterpretNodeIntoBoolValue(ocn[2]);

	bool recursive_matching = true;
	if(ocn.size() > 3)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[3]);

	//calculate edit distance based commonality if string edit distance true and both args are string literals
	if(use_string_edit_distance && (ocn[0]->GetType() == ENT_STRING && ocn[1]->GetType() == ENT_STRING))
	{
		size_t s1_len = 0;
		size_t s2_len = 0;
		auto edit_distance = EvaluableNodeTreeManipulation::EditDistance(
			ocn[0]->GetStringValue(), ocn[1]->GetStringValue(), s1_len, s2_len);
		auto commonality = static_cast<double>(std::max(s1_len, s2_len) - edit_distance);
		return AllocReturn(commonality, immediate_result);
	}

	//otherwise, treat both as nodes and calculate node commonality
	auto tree1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(tree1);

	auto tree2 = InterpretNodeForImmediateUse(ocn[1]);
	auto results = EvaluableNodeTreeManipulation::NumberOfSharedNodes(tree1, tree2, false, recursive_matching);

	node_stack.PopEvaluableNode();

	evaluableNodeManager->FreeNodeTreeIfPossible(tree1);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree2);

	return AllocReturn(results.commonality, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EDIT_DISTANCE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool use_string_edit_distance = false;
	if(ocn.size() > 2)
		use_string_edit_distance = InterpretNodeIntoBoolValue(ocn[2]);

	bool recursive_matching = true;
	if(ocn.size() > 3)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[3]);

	//otherwise, treat both as nodes and calculate node edit distance
	auto tree1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(tree1);

	auto tree2 = InterpretNodeForImmediateUse(ocn[1]);

	double edit_distance = 0.0;
	//calculate string edit distance if string edit distance true and both args are string literals
	if(use_string_edit_distance
		&& tree1 != nullptr && tree2 != nullptr
		&& (tree1->GetType() == ENT_STRING && tree2->GetType() == ENT_STRING))
	{
		edit_distance = static_cast<double>(EvaluableNodeTreeManipulation::EditDistance(
			tree1->GetStringValue(), tree2->GetStringValue()));
	}
	else
	{
		edit_distance = EvaluableNodeTreeManipulation::EditDistance(tree1, tree2, false, recursive_matching);
	}

	node_stack.PopEvaluableNode();

	evaluableNodeManager->FreeNodeTreeIfPossible(tree1);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree2);

	return AllocReturn(edit_distance, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool recursive_matching = true;
	if(ocn.size() > 2)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[2]);

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);

	EvaluableNode *result = EvaluableNodeTreeManipulation::IntersectTrees(
		evaluableNodeManager, n1, n2, recursive_matching);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	evaluableNodeManager->FreeNodeTreeIfPossible(n1);
	evaluableNodeManager->FreeNodeTreeIfPossible(n2);

	return EvaluableNodeReference(result, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool recursive_matching = true;
	if(ocn.size() > 2)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[2]);

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(n1);
	
	auto n2 = InterpretNodeForImmediateUse(ocn[1]);

	EvaluableNode *result = EvaluableNodeTreeManipulation::UnionTrees(
		evaluableNodeManager, n1, n2, recursive_matching);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	evaluableNodeManager->FreeNodeTreeIfPossible(n1);
	evaluableNodeManager->FreeNodeTreeIfPossible(n2);

	return EvaluableNodeReference(result, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIFFERENCE(EvaluableNode *en, bool immediate_result)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX(EvaluableNode *en, bool immediate_result)
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

	double similar_mix_chance = 0.0;
	if(ocn.size() > 4)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[4]);
		if(!FastIsNaN(new_value))
			similar_mix_chance = new_value;
	}

	bool recursive_matching = true;
	if(ocn.size() > 5)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[5]);

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);

	EvaluableNode *result = EvaluableNodeTreeManipulation::MixTrees(randomStream.CreateOtherStreamViaRand(),
		evaluableNodeManager, n1, n2, blend1, blend2, similar_mix_chance, recursive_matching);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	evaluableNodeManager->FreeNodeTreeIfPossible(n1);
	evaluableNodeManager->FreeNodeTreeIfPossible(n2);

	return EvaluableNodeReference(result, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TOTAL_ENTITY_SIZE(EvaluableNode *en, bool immediate_result)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_FLATTEN_ENTITY(EvaluableNode *en, bool immediate_result)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_MUTATE_ENTITY(EvaluableNode *en, bool immediate_result)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_COMMONALITY_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool recursive_matching = true;
	if(ocn.size() > 2)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[2]);

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	auto commonality = EntityManipulation::NumberOfSharedNodes(source_entity_1, source_entity_2, false, recursive_matching);
	return AllocReturn(commonality.commonality, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EDIT_DISTANCE_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool recursive_matching = true;
	if(ocn.size() > 2)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[2]);

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	double edit_distance = EntityManipulation::EditDistance(source_entity_1, source_entity_2, false, recursive_matching);
	return AllocReturn(edit_distance, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool recursive_matching = true;
	if(ocn.size() > 2)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[2]);

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
	Entity *new_entity = EntityManipulation::IntersectEntities(this,
		source_entity_1, source_entity_2, recursive_matching);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION_ENTITIES(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool recursive_matching = true;
	if(ocn.size() > 2)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[2]);

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
	Entity *new_entity = EntityManipulation::UnionEntities(this,
		source_entity_1, source_entity_2, recursive_matching);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIFFERENCE_ENTITIES(EvaluableNode *en, bool immediate_result)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX_ENTITIES(EvaluableNode *en, bool immediate_result)
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

	double similar_mix_chance = 0.0;
	if(ocn.size() > 4)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[4]);
		if(!FastIsNaN(new_value))
			similar_mix_chance = new_value;
	}

	bool recursive_matching = true;
	if(ocn.size() > 5)
		recursive_matching = InterpretNodeIntoBoolValue(ocn[5]);

	double fraction_unnamed_entities_to_mix = 0.2;
	if(ocn.size() > 6)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[6]);
		if(!FastIsNaN(new_value))
			fraction_unnamed_entities_to_mix = new_value;
	}

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//need a source entity, and shouldn't copy self
	if(source_entity_1 == curEntity || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::MixEntities(this, source_entity_1, source_entity_2,
		blend1, blend2, similar_mix_chance, recursive_matching, fraction_unnamed_entities_to_mix);

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
