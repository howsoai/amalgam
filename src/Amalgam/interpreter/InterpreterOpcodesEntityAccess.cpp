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
		if(immediate_result.AnyImmediateType())
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
