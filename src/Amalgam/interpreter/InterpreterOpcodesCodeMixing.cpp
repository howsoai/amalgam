//project headers:
#include "Interpreter.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EvaluableNodeTreeDifference.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"
#include "PerformanceProfiler.h"

//system headers:
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_MUTATE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto to_mutate = InterpretNodeForImmediateUse(ocn[0]);
	if(to_mutate == nullptr)
		return EvaluableNodeReference::Null();
	auto node_stack = CreateInterpreterNodeStackStateSaver(to_mutate);

	double mutation_rate = 0.00001;
	if(ocn.size() > 1)
		mutation_rate = InterpretNodeIntoNumberValue(ocn[1]);

	bool ow_exists = false;
	CompactHashMap<EvaluableNodeType, double> opcode_weights;
	if(ocn.size() > 2)
	{
		auto opcode_weights_node = InterpretNodeForImmediateUse(ocn[2]);
		if(!EvaluableNode::IsEmptyNode(opcode_weights_node))
		{
			ow_exists = true;
			for(auto &[node_id, node] : opcode_weights_node->GetMappedChildNodes())
				opcode_weights[GetEvaluableNodeTypeFromStringId(node_id)] = EvaluableNode::ToNumber(node);

			evaluableNodeManager->FreeNodeTreeIfPossible(opcode_weights_node);
		}
	}

	bool mtw_exists = false;
	CompactHashMap<StringInternPool::StringID, double> mutation_type_weights;
	if(ocn.size() > 3)
	{
		auto mutation_weights_node = InterpretNodeForImmediateUse(ocn[3]);
		if(!EvaluableNode::IsEmptyNode(mutation_weights_node))
		{
			mtw_exists = true;
			for(auto &[node_id, node] : mutation_weights_node->GetMappedChildNodes())
				mutation_type_weights[node_id] = EvaluableNode::ToNumber(node);

			evaluableNodeManager->FreeNodeTreeIfPossible(mutation_weights_node);
		}
	}

	//result contains the copied result which may incur replacements
	EvaluableNode *result = EvaluableNodeTreeManipulation::MutateTree(this, evaluableNodeManager, to_mutate, mutation_rate, mtw_exists ? &mutation_type_weights : nullptr, ow_exists ? &opcode_weights : nullptr);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, false);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_COMMONALITY(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool use_string_edit_distance = false;
	if(ocn.size() > 2)
		use_string_edit_distance = InterpretNodeIntoBoolValue(ocn[2]);

	//calculate edit distance based commonality if string edit distance true and both args are string literals
	if(use_string_edit_distance && (ocn[0]->GetType() == ENT_STRING && ocn[1]->GetType() == ENT_STRING))
	{
		size_t s1_len = 0;
		size_t s2_len = 0;
		auto edit_distance = EvaluableNodeTreeManipulation::EditDistance(ocn[0]->GetStringValue(), ocn[1]->GetStringValue(), s1_len, s2_len);
		auto commonality = static_cast<int64_t>(std::max(s1_len, s2_len) - edit_distance);
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(commonality), true);
	}

	//otherwise, treat both as nodes and calculate node commonality
	auto tree1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(tree1);

	auto tree2 = InterpretNodeForImmediateUse(ocn[1]);
	auto results = EvaluableNodeTreeManipulation::NumberOfSharedNodes(tree1, tree2);

	node_stack.PopEvaluableNode();

	evaluableNodeManager->FreeNodeTreeIfPossible(tree1);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree2);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(results.commonality), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EDIT_DISTANCE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool use_string_edit_distance = false;
	if(ocn.size() > 2)
		use_string_edit_distance = InterpretNodeIntoBoolValue(ocn[2]);

	//otherwise, treat both as nodes and calculate node edit distance
	auto tree1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(tree1);

	auto tree2 = InterpretNodeForImmediateUse(ocn[1]);

	double edit_distance = 0.0;
	//calculate string edit distance if string edit distance true and both args are string literals
	if(use_string_edit_distance
		&& tree1 != nullptr && tree2 != nullptr
		&& (tree1->GetType() == ENT_STRING && tree2->GetType() == ENT_STRING))
	{
		edit_distance = static_cast<double>(EvaluableNodeTreeManipulation::EditDistance(tree1->GetStringValue(), tree2->GetStringValue()));
	}
	else
	{
		edit_distance = EvaluableNodeTreeManipulation::EditDistance(tree1, tree2);
	}

	node_stack.PopEvaluableNode();

	evaluableNodeManager->FreeNodeTreeIfPossible(tree1);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree2);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(edit_distance), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PushEvaluableNode(n2);

	EvaluableNode *result = EvaluableNodeTreeManipulation::IntersectTrees(evaluableNodeManager, n1, n2);

	//both must be unique and both must be cycle free, otherwise there's a possibility of a cycle
	bool cycle_free = (n1.unique && n2.unique && !n1.GetNeedCycleCheck() && !n2.GetNeedCycleCheck());
	//if cycle, double-check everything
	if(!cycle_free)
		EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, (n1.unique && n2.unique));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(n1);
	
	auto n2 = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PushEvaluableNode(n2);

	EvaluableNode *result = EvaluableNodeTreeManipulation::UnionTrees(evaluableNodeManager, n1, n2);

	//both must be unique and both must be cycle free, otherwise there's a possibility of a cycle
	bool cycle_free = (n1.unique && n2.unique && !n1.GetNeedCycleCheck() && !n2.GetNeedCycleCheck());
	//if cycle, double-check everything
	if(!cycle_free)
		EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, (n1.unique && n2.unique));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIFFERENCE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PushEvaluableNode(n2);

	EvaluableNode *result = EvaluableNodeTreeDifference::DifferenceTrees(evaluableNodeManager, n1, n2);

	//both must be unique and both must be cycle free, otherwise there's a possibility of a cycle
	bool cycle_free = (n1.unique && n2.unique && !n1.GetNeedCycleCheck() && !n2.GetNeedCycleCheck());
	//if cycle, double-check everything
	if(!cycle_free)
		EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, (n1.unique && n2.unique));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	double blend2 = 0.5; //default to half
	if(ocn.size() > 2)
		blend2 = InterpretNodeIntoNumberValue(ocn[2]);

	double blend1 = 1.0 - blend2; //default to the remainder
	if(ocn.size() > 3)
	{
		blend1 = InterpretNodeIntoNumberValue(ocn[3]);
		//if have a third parameter, then use the fractions in order (so need to swap)
		std::swap(blend1, blend2);
	}

	double similar_mix_chance = 0.0;
	if(ocn.size() > 4)
		similar_mix_chance = InterpretNodeIntoNumberValue(ocn[4]);

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PushEvaluableNode(n2);

	EvaluableNode *result = EvaluableNodeTreeManipulation::MixTrees(randomStream.CreateOtherStreamViaRand(),
		evaluableNodeManager, n1, n2, blend1, blend2, similar_mix_chance);

	//both must be unique and both must be cycle free, otherwise there's a possibility of a cycle
	bool cycle_free = (n1.unique && n2.unique && !n1.GetNeedCycleCheck() && !n2.GetNeedCycleCheck());
	//if cycle, double-check everything
	if(!cycle_free)
		EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, (n1.unique && n2.unique));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX_LABELS(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	double blend2 = 0.5; //default to half
	if(ocn.size() > 2)
		blend2 = InterpretNodeIntoNumberValue(ocn[2]);

	double blend1 = 1.0 - blend2; //default to the remainder
	if(ocn.size() > 3)
	{
		blend1 = InterpretNodeIntoNumberValue(ocn[2]);
		//if have a third parameter, then use the fractions in order (so need to swap)
		std::swap(blend1, blend2);
	}

	auto n1 = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(n1);

	auto n2 = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PushEvaluableNode(n2);

	EvaluableNode *result = EvaluableNodeTreeManipulation::MixTreesByCommonLabels(this, evaluableNodeManager, n1, n2, randomStream, blend1, blend2);

	//both must be unique and both must be cycle free, otherwise there's a possibility of a cycle
	bool cycle_free = (n1.unique && n2.unique && !n1.GetNeedCycleCheck() && !n2.GetNeedCycleCheck());
	//if cycle, double-check everything
	if(!cycle_free)
		EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, (n1.unique && n2.unique));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TOTAL_ENTITY_SIZE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//TODO 10975: lock entire entity tree
	//get the id of the first source entity
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	if(source_entity == nullptr)
		return EvaluableNodeReference::Null();

	double size = static_cast<double>(source_entity->GetDeepSizeInNodes());
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(size), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_FLATTEN_ENTITY(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	bool include_rand_seeds = true;
	if(ocn.size() > 1)
		include_rand_seeds = InterpretNodeIntoBoolValue(ocn[1]);

	bool parallel_create = false;
	if(ocn.size() > 2)
		parallel_create = InterpretNodeIntoBoolValue(ocn[2]);

	//TODO 10975: lock entire entity tree
	//get the id of the first source entity
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	if(source_entity == nullptr)
		return EvaluableNodeReference::Null();
	
	return EntityManipulation::FlattenEntity(this, source_entity, include_rand_seeds, parallel_create);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MUTATE_ENTITY(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get the id of the first source entity
	//TODO 10975: change this to lock all entities at once
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity == nullptr || source_entity == curEntity)
		return EvaluableNodeReference::Null();

	//get mutation rate if applicable
	double mutation_rate = 0.00001;
	if(ocn.size() > 1)
		mutation_rate = InterpretNodeIntoNumberValue(ocn[1]);

	//get destination if applicable
	StringInternRef new_entity_id;
	Entity *destination_entity_parent = curEntity;
	if(ocn.size() > 2)
		InterpretNodeIntoDestinationEntity(ocn[2], destination_entity_parent, new_entity_id);
	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	bool ow_exists = false;
	CompactHashMap<EvaluableNodeType, double> opcode_weights;
	if(ocn.size() > 3)
	{
		auto opcode_weights_node = InterpretNodeForImmediateUse(ocn[3]);
		if(!EvaluableNode::IsEmptyNode(opcode_weights_node))
		{
			ow_exists = true;
			for(auto &[node_id, node] : opcode_weights_node->GetMappedChildNodes())
				opcode_weights[GetEvaluableNodeTypeFromStringId(node_id)] = EvaluableNode::ToNumber(node);

			evaluableNodeManager->FreeNodeTreeIfPossible(opcode_weights_node);
		}
	}

	bool mtw_exists = false;
	CompactHashMap<StringInternPool::StringID, double> mutation_type_weights;
	if(ocn.size() > 4)
	{
		auto mutation_weights_node = InterpretNodeForImmediateUse(ocn[4]);
		if(!EvaluableNode::IsEmptyNode(mutation_weights_node))
		{
			mtw_exists = true;
			for(auto &[node_id, node] : mutation_weights_node->GetMappedChildNodes())
				mutation_type_weights[node_id] = EvaluableNode::ToNumber(node);

			evaluableNodeManager->FreeNodeTreeIfPossible(mutation_weights_node);
		}
	}

	//create new entity by mutating
	Entity *new_entity = EntityManipulation::MutateEntity(this, source_entity, mutation_rate, mtw_exists ? &mutation_type_weights : nullptr, ow_exists ? &opcode_weights : nullptr);
	
	//accumulate usage
	if(!AllowUnlimitedExecutionNodes())
		curNumExecutionNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id), true);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathListFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_COMMONALITY_ENTITIES(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//TODO 10975: change this to lock all entities at once
	//get the id of the first source entity
	Entity *source_entity_1 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	if(source_entity_1 == nullptr)
		return EvaluableNodeReference::Null();

	//get the id of the second source entity
	Entity *source_entity_2 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[1]);
	if(source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	auto commonality = EntityManipulation::NumberOfSharedNodes(source_entity_1, source_entity_2);
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(commonality.commonality), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EDIT_DISTANCE_ENTITIES(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//TODO 10975: change this to lock all entities at once
	//get the id of the first source entity
	Entity *source_entity_1 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	if(source_entity_1 == nullptr)
		return EvaluableNodeReference::Null();

	//get the id of the second source entity
	Entity *source_entity_2 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[1]);
	if(source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	double edit_distance = EntityManipulation::EditDistance(source_entity_1, source_entity_2);
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(edit_distance), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT_ENTITIES(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//TODO 10975: change this to lock all entities at once
	//get the id of the first source entity
	Entity *source_entity_1 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity_1 == nullptr || source_entity_1 == curEntity)
		return EvaluableNodeReference::Null();

	//get the id of the second source entity
	Entity *source_entity_2 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[1]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity_2 == nullptr || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//get destination if applicable
	StringInternRef new_entity_id;
	Entity *destination_entity_parent = curEntity;
	if(ocn.size() > 2)
		InterpretNodeIntoDestinationEntity(ocn[2], destination_entity_parent, new_entity_id);
	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::IntersectEntities(this, source_entity_1, source_entity_2);

	//accumulate usage
	if(!AllowUnlimitedExecutionNodes())
		curNumExecutionNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id), true);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathListFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION_ENTITIES(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//TODO 10975: change this to lock all entities at once
	//get the id of the first source entity
	Entity *source_entity_1 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity_1 == nullptr || source_entity_1 == curEntity)
		return EvaluableNodeReference::Null();

	//get the id of the second source entity
	Entity *source_entity_2 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[1]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity_2 == nullptr || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//get destination if applicable
	StringInternRef new_entity_id;
	Entity *destination_entity_parent = curEntity;
	if(ocn.size() > 2)
		InterpretNodeIntoDestinationEntity(ocn[2], destination_entity_parent, new_entity_id);
	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::UnionEntities(this, source_entity_1, source_entity_2);

	//accumulate usage
	if(!AllowUnlimitedExecutionNodes())
		curNumExecutionNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id), true);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathListFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIFFERENCE_ENTITIES(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//TODO 10975: change this to lock all entities at once
	//get the id of the first source entity
	Entity *entity_1 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(entity_1 == nullptr || entity_1 == curEntity)
		return EvaluableNodeReference::Null();

	//get the id of the second source entity
	Entity *entity_2 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[1]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(entity_2 == nullptr || entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	return EntityManipulation::DifferenceEntities(this, entity_1, entity_2);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX_ENTITIES(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//TODO 10975: change this to lock all entities at once
	//get the id of the first source entity
	Entity *source_entity_1 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[0]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity_1 == nullptr || source_entity_1 == curEntity)
		return EvaluableNodeReference::Null();

	//get the id of the second source entity
	Entity *source_entity_2 = InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(ocn[1]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity_2 == nullptr || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	double blend2 = 0.5; //default to half
	if(ocn.size() > 2)
		blend2 = InterpretNodeIntoNumberValue(ocn[2]);

	double blend1 = 1.0 - blend2; //default to the remainder
	if(ocn.size() > 3)
	{
		blend1 = InterpretNodeIntoNumberValue(ocn[3]);
		//if have a third parameter, then use the fractions in order (so need to swap)
		std::swap(blend1, blend2);
	}

	double similar_mix_chance = 0.0;
	if(ocn.size() > 4)
		similar_mix_chance = InterpretNodeIntoNumberValue(ocn[4]);

	double fraction_unnamed_entities_to_mix = 0.2;
	if(ocn.size() > 5)
		fraction_unnamed_entities_to_mix = InterpretNodeIntoNumberValue(ocn[5]);

	//get destination if applicable
	StringInternRef new_entity_id;
	Entity *destination_entity_parent = curEntity;
	if(ocn.size() > 6)
		InterpretNodeIntoDestinationEntity(ocn[6], destination_entity_parent, new_entity_id);
	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::MixEntities(this, source_entity_1, source_entity_2,
		blend1, blend2, similar_mix_chance, fraction_unnamed_entities_to_mix);

	//accumulate usage
	if(!AllowUnlimitedExecutionNodes())
		curNumExecutionNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id), true);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathListFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}
