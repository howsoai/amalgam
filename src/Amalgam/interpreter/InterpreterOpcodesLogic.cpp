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

EvaluableNodeReference Interpreter::InterpretNode_ENT_AND(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference cur = EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		for(auto &cn : interpreted_nodes)
		{
			//free the previous node if applicable
			evaluableNodeManager->FreeNodeTreeIfPossible(cur);

			cur = cn;

			if(!EvaluableNode::IsTrue(cur))
				return evaluableNodeManager->ReuseOrAllocNode(cur, ENT_FALSE);
		}

		return cur;
	}
#endif

	for(auto &cn : ocn)
	{
		//free the previous node if applicable
		evaluableNodeManager->FreeNodeTreeIfPossible(cur);

		cur = InterpretNode(cn);

		if(!EvaluableNode::IsTrue(cur))
			return ReuseOrAllocReturn(cur, false, immediate_result);
	}

	return cur;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_OR(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference cur = EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		for(auto &cn : interpreted_nodes)
		{
			//free the previous node if applicable
			evaluableNodeManager->FreeNodeTreeIfPossible(cur);

			cur = cn;

			//if it is a valid node and it is not zero, then return it
			if(EvaluableNode::IsTrue(cur))
				return cur;
		}

		return evaluableNodeManager->ReuseOrAllocNode(cur, ENT_FALSE);
	}
#endif

	for(auto &cn : ocn)
	{
		//free the previous node if applicable
		evaluableNodeManager->FreeNodeTreeIfPossible(cur);

		cur = InterpretNode(cn);

		if(EvaluableNode::IsTrue(cur))
			return cur;
	}

	return ReuseOrAllocReturn(cur, false, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_XOR(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	size_t num_true = 0;

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		for(auto &cur : interpreted_nodes)
		{
			//if it's true, count it
			if(EvaluableNode::IsTrue(cur))
				num_true++;

			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		}

		//if an odd number of true arguments, then return true
		bool result = (num_true % 2 == 1);
		return AllocReturn(result, immediate_result);
	}
#endif

	//count number of true values
	for(auto &cn : ocn)
	{
		if(InterpretNodeIntoBoolValue(cn))
			num_true++;
	}

	//if an odd number of true arguments, then return true
	bool result = (num_true % 2 == 1);
	return AllocReturn(result, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NOT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNodeForImmediateUse(ocn[0]);
	bool is_true = EvaluableNode::IsTrue(cur);
	return ReuseOrAllocReturn(cur, !is_true, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EQUAL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool processed_first_value = false;
	EvaluableNodeReference to_match = EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		for(auto &cur : interpreted_nodes)
		{
			//if haven't gotten a value yet, then use this as the first data
			if(!processed_first_value)
			{
				to_match = cur;
				processed_first_value = true;
				continue;
			}

			if(!EvaluableNode::AreDeepEqual(to_match, cur))
				return evaluableNodeManager->ReuseOrAllocOneOfNodes(to_match, cur, ENT_FALSE);

			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		}

		return evaluableNodeManager->ReuseOrAllocNode(to_match, ENT_TRUE);
	}
#endif

	auto node_stack = CreateInterpreterNodeStackStateSaver();

	for(auto &cn : ocn)
	{
		auto cur = InterpretNodeForImmediateUse(cn);

		//if haven't gotten a value yet, then use this as the first data
		if(!processed_first_value)
		{
			to_match = cur;
			node_stack.PushEvaluableNode(to_match);
			processed_first_value = true;
			continue;
		}

		if(!EvaluableNode::AreDeepEqual(to_match, cur))
			return ReuseOrAllocOneOfReturn(to_match, cur, false, immediate_result);

		evaluableNodeManager->FreeNodeTreeIfPossible(cur);
	}

	return ReuseOrAllocReturn(to_match, true, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NEQUAL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		bool all_not_equal = true;
		for(size_t i = 0; i < interpreted_nodes.size(); i++)
		{
			//don't compare versus self, and skip any previously compared against
			for(size_t j = i + 1; j < interpreted_nodes.size(); j++)
			{
				//if they're equal, then it fails
				if(EvaluableNode::AreDeepEqual(interpreted_nodes[i], interpreted_nodes[j]))
				{
					all_not_equal = false;

					//break out of loop
					i = interpreted_nodes.size();
					break;
				}
			}
		}

		for(size_t i = 0; i < interpreted_nodes.size(); i++)
			evaluableNodeManager->FreeNodeTreeIfPossible(interpreted_nodes[i]);

		return AllocReturn(all_not_equal, immediate_result);
	}
#endif

	//special (faster) case for comparing two
	if(ocn.size() == 2)
	{
		EvaluableNodeReference a = InterpretNodeForImmediateUse(ocn[0]);

		auto node_stack = CreateInterpreterNodeStackStateSaver(a);
		EvaluableNodeReference b = InterpretNodeForImmediateUse(ocn[1]);

		bool a_b_not_equal = (!EvaluableNode::AreDeepEqual(a, b));
		return ReuseOrAllocOneOfReturn(a, b, a_b_not_equal, immediate_result);
	}

	auto node_stack = CreateInterpreterNodeStackStateSaver();

	//get the value for each node
	std::vector<EvaluableNodeReference> values;
	values.reserve(ocn.size());
	for(size_t i = 0; i < ocn.size(); i++)
	{
		values.push_back(InterpretNodeForImmediateUse(ocn[i]));
		node_stack.PushEvaluableNode(values[i]);
	}

	bool all_not_equal = true;
	for(size_t i = 0; i < values.size(); i++)
	{
		//don't compare versus self, and skip any previously compared against
		for(size_t j = i + 1; j < values.size(); j++)
		{
			//if they're equal, then it fails
			if(EvaluableNode::AreDeepEqual(values[i], values[j]))
			{
				all_not_equal = false;

				//break out of loop
				i = values.size();
				break;
			}
		}
	}

	for(size_t i = 0; i < values.size(); i++)
		evaluableNodeManager->FreeNodeTreeIfPossible(values[i]);

	return AllocReturn(all_not_equal, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LESS_and_LEQUAL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//if none or one node, then there's no order
	if(ocn.size() < 2)
		return AllocReturn(false, immediate_result);

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		EvaluableNodeReference prev = interpreted_nodes[0];
		if(EvaluableNode::IsNaN(prev))
		{
			for(auto &n : interpreted_nodes)
				evaluableNodeManager->FreeNodeTreeIfPossible(n);

			return AllocReturn(false, immediate_result);
		}

		bool result = true;
		for(size_t i = 1; i < interpreted_nodes.size(); i++)
		{
			//if not in strict increasing order, return false
			auto &cur = interpreted_nodes[i];

			if(EvaluableNode::IsNaN(cur))
			{
				result = false;
				break;
			}

			if(!EvaluableNode::IsLessThan(prev, cur, en->GetType() == ENT_LEQUAL))
			{
				result = false;
				break;
			}
		}

		for(auto &n : interpreted_nodes)
			evaluableNodeManager->FreeNodeTreeIfPossible(n);

		return AllocReturn(result, immediate_result);
	}
#endif

	auto prev = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsEmptyNode(prev))
		return evaluableNodeManager->ReuseOrAllocNode(prev, ENT_FALSE);

	auto node_stack = CreateInterpreterNodeStackStateSaver(prev);

	for(size_t i = 1; i < ocn.size(); i++)
	{
		//if not in strict increasing order, return false
		auto cur = InterpretNodeForImmediateUse(ocn[i]);

		if(EvaluableNode::IsEmptyNode(cur)
				|| !EvaluableNode::IsLessThan(prev, cur, en->GetType() == ENT_LEQUAL))
			return ReuseOrAllocOneOfReturn(prev, cur, false, immediate_result);

		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		prev = cur;

		node_stack.PopEvaluableNode();
		node_stack.PushEvaluableNode(prev);
	}

	//nothing is out of order
	return ReuseOrAllocReturn(prev, true, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GREATER_and_GEQUAL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//if none or one node, then it's in order
	if(ocn.size() < 2)
		return AllocReturn(false, immediate_result);

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		EvaluableNodeReference prev = interpreted_nodes[0];
		if(EvaluableNode::IsNaN(prev))
		{
			for(auto &n : interpreted_nodes)
				evaluableNodeManager->FreeNodeTreeIfPossible(n);

			return AllocReturn(false, immediate_result);
		}

		bool result = true;
		for(size_t i = 1; i < interpreted_nodes.size(); i++)
		{
			//if not in strict increasing order, return false
			auto &cur = interpreted_nodes[i];

			if(EvaluableNode::IsNaN(cur))
			{
				result = false;
				break;
			}

			if(!EvaluableNode::IsLessThan(cur, prev, en->GetType() == ENT_GEQUAL))
			{
				result = false;
				break;
			}
		}

		for(auto &n : interpreted_nodes)
			evaluableNodeManager->FreeNodeTreeIfPossible(n);

		return AllocReturn(result, immediate_result);
	}
#endif

	auto prev = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsEmptyNode(prev))
		return evaluableNodeManager->ReuseOrAllocNode(prev, ENT_FALSE);

	auto node_stack = CreateInterpreterNodeStackStateSaver(prev);

	for(size_t i = 1; i < ocn.size(); i++)
	{
		//if not in strict increasing order, return false
		auto cur = InterpretNodeForImmediateUse(ocn[i]);

		if(EvaluableNode::IsEmptyNode(cur)
				|| !EvaluableNode::IsLessThan(cur, prev, en->GetType() == ENT_GEQUAL))
			return ReuseOrAllocOneOfReturn(prev, cur, false, immediate_result);

		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		prev = cur;

		node_stack.PopEvaluableNode();
		node_stack.PushEvaluableNode(prev);
	}

	//nothing is out of order
	return ReuseOrAllocReturn(prev, true, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TYPE_EQUALS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool processed_first_value = false;
	EvaluableNodeReference to_match = EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	std::vector<EvaluableNodeReference> interpreted_nodes;
	if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
	{
		for(auto &cur : interpreted_nodes)
		{
			//if haven't gotten a value yet, then use this as the first data
			if(!processed_first_value)
			{
				to_match = cur;
				processed_first_value = true;
				continue;
			}

			EvaluableNodeType cur_type = ENT_NULL;
			if(cur != nullptr)
				cur_type = cur->GetType();

			EvaluableNodeType to_match_type = ENT_NULL;
			if(to_match != nullptr)
				to_match_type = to_match->GetType();

			if(cur_type != to_match_type)
				return ReuseOrAllocOneOfReturn(to_match, cur, false, immediate_result);

			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		}

		return ReuseOrAllocReturn(to_match, true, immediate_result);
	}
#endif

	auto node_stack = CreateInterpreterNodeStackStateSaver();

	for(auto &cn : ocn)
	{
		auto cur = InterpretNodeForImmediateUse(cn);

		//if haven't gotten a value yet, then use this as the first data
		if(!processed_first_value)
		{
			to_match = cur;
			node_stack.PushEvaluableNode(to_match);
			processed_first_value = true;
			continue;
		}

		EvaluableNodeType cur_type = ENT_NULL;
		if(cur != nullptr)
			cur_type = cur->GetType();
		
		EvaluableNodeType to_match_type = ENT_NULL;
		if(to_match != nullptr)
			to_match_type = to_match->GetType();

		if(cur_type != to_match_type)
			return ReuseOrAllocOneOfReturn(to_match, cur, false, immediate_result);

		evaluableNodeManager->FreeNodeTreeIfPossible(cur);
	}

	return ReuseOrAllocReturn(to_match, true, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TYPE_NEQUALS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	std::vector<EvaluableNodeReference> values(ocn.size());

	auto node_stack = CreateInterpreterNodeStackStateSaver();

	//evaluate all nodes just once
	for(size_t i = 0; i < ocn.size(); i++)
	{
		values[i] = InterpretNodeForImmediateUse(ocn[i]);
		node_stack.PushEvaluableNode(values[i]);
	}

	bool all_not_equal = true;
	for(size_t i = 0; i < ocn.size(); i++)
	{
		//start at next higher, because comparisons are symmetric and don't need to compare with self
		for(size_t j = i + 1; j < ocn.size(); j++)
		{
			EvaluableNode *cur1 = values[i];
			EvaluableNode *cur2 = values[j];

			//if they're equal, then it fails
			if((cur1 == nullptr && cur2 == nullptr) || (cur1 != nullptr && cur2 != nullptr && cur1->GetType() == cur2->GetType()))
			{
				all_not_equal = false;

				//break out of loop
				i = ocn.size();
				break;
			}
		}
	}

	for(size_t i = 0; i < values.size(); i++)
		evaluableNodeManager->FreeNodeTreeIfPossible(values[i]);

	return AllocReturn(all_not_equal, immediate_result);
}
