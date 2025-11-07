//project headers:
#include "Interpreter.h"

//system headers:

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

			if(!EvaluableNode::ToBool(cur))
			{
				evaluableNodeManager->FreeNodeTreeIfPossible(cur);
				return AllocReturn(false, immediate_result);
			}
		}

		return cur;
	}
#endif

	for(auto &cn : ocn)
	{
		//free the previous node if applicable
		evaluableNodeManager->FreeNodeTreeIfPossible(cur);

		cur = InterpretNode(cn, immediate_result);

		if(cur.IsImmediateValue())
		{
			if(!cur.GetValue().GetValueAsBoolean())
				return AllocReturn(false, immediate_result);
		}
		else
		{
			if(!EvaluableNode::ToBool(cur))
			{
				evaluableNodeManager->FreeNodeTreeIfPossible(cur);
				return AllocReturn(false, immediate_result);
			}
		}
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
			if(EvaluableNode::ToBool(cur))
				return cur;
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		return AllocReturn(false, immediate_result);
	}
#endif

	for(auto &cn : ocn)
	{
		//free the previous node if applicable
		evaluableNodeManager->FreeNodeTreeIfPossible(cur);

		cur = InterpretNode(cn, immediate_result);

		if(cur.GetValue().GetValueAsBoolean())
			return cur;
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(cur);
	return AllocReturn(false, immediate_result);
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
			if(EvaluableNode::ToBool(cur))
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

	auto cur = InterpretNodeForImmediateUse(ocn[0], true);
	if(cur.IsImmediateValue())
	{
		bool is_true = cur.GetValue().GetValueAsBoolean();
		return AllocReturn(!is_true, immediate_result);
	}
	else
	{
		bool is_true = EvaluableNode::ToBool(cur);
		evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		return AllocReturn(!is_true, immediate_result);
	}	
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
			{
				evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
				evaluableNodeManager->FreeNodeTreeIfPossible(cur);
				return AllocReturn(false, immediate_result);
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
		return AllocReturn(false, immediate_result);
	}
#endif

	auto node_stack = CreateOpcodeStackStateSaver();

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
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
			return AllocReturn(false, immediate_result);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(cur);
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
	return AllocReturn(true, immediate_result);
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

		auto node_stack = CreateOpcodeStackStateSaver(a);
		EvaluableNodeReference b = InterpretNodeForImmediateUse(ocn[1]);

		bool a_b_not_equal = (!EvaluableNode::AreDeepEqual(a, b));
		evaluableNodeManager->FreeNodeTreeIfPossible(a);
		evaluableNodeManager->FreeNodeTreeIfPossible(b);
		return AllocReturn(a_b_not_equal, immediate_result);
	}

	auto node_stack = CreateOpcodeStackStateSaver();

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
		if(EvaluableNode::IsNull(prev))
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

			if(EvaluableNode::IsNull(cur))
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
	if(EvaluableNode::IsNull(prev))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		return AllocReturn(false, immediate_result);
	}

	auto node_stack = CreateOpcodeStackStateSaver(prev);

	for(size_t i = 1; i < ocn.size(); i++)
	{
		//if not in strict increasing order, return false
		auto cur = InterpretNodeForImmediateUse(ocn[i]);

		if(EvaluableNode::IsNull(cur)
			|| !EvaluableNode::IsLessThan(prev, cur, en->GetType() == ENT_LEQUAL))
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(prev);
			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
			return AllocReturn(false, immediate_result);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		prev = cur;

		node_stack.PopEvaluableNode();
		node_stack.PushEvaluableNode(prev);
	}

	//nothing is out of order
	evaluableNodeManager->FreeNodeTreeIfPossible(prev);
	return AllocReturn(true, immediate_result);
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
		if(EvaluableNode::IsNull(prev))
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

			if(EvaluableNode::IsNull(cur))
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
	if(EvaluableNode::IsNull(prev))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		return AllocReturn(false, immediate_result);
	}

	auto node_stack = CreateOpcodeStackStateSaver(prev);

	for(size_t i = 1; i < ocn.size(); i++)
	{
		//if not in strict increasing order, return false
		auto cur = InterpretNodeForImmediateUse(ocn[i]);

		if(EvaluableNode::IsNull(cur)
			|| !EvaluableNode::IsLessThan(cur, prev, en->GetType() == ENT_GEQUAL))
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(prev);
			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
			return AllocReturn(false, immediate_result);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		prev = cur;

		node_stack.PopEvaluableNode();
		node_stack.PushEvaluableNode(prev);
	}

	//nothing is out of order
	evaluableNodeManager->FreeNodeTreeIfPossible(prev);
	return AllocReturn(true, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TYPE_EQUALS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool processed_first_value = false;
	EvaluableNodeReference to_match = EvaluableNodeReference::Null();
	EvaluableNodeType to_match_type = ENT_NULL;

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
				if(to_match != nullptr)
					to_match_type = to_match->GetType();
				processed_first_value = true;
				continue;
			}

			EvaluableNodeType cur_type = ENT_NULL;
			if(cur != nullptr)
				cur_type = cur->GetType();

			if(cur_type != to_match_type)
			{
				evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
				evaluableNodeManager->FreeNodeTreeIfPossible(cur);
				return AllocReturn(false, immediate_result);
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
		return AllocReturn(true, immediate_result);
	}
#endif

	auto node_stack = CreateOpcodeStackStateSaver();

	for(auto &cn : ocn)
	{
		auto cur = InterpretNodeForImmediateUse(cn);

		//if haven't gotten a value yet, then use this as the first data
		if(!processed_first_value)
		{
			to_match = cur;
			if(to_match != nullptr)
				to_match_type = to_match->GetType();
			node_stack.PushEvaluableNode(to_match);
			processed_first_value = true;
			continue;
		}

		EvaluableNodeType cur_type = ENT_NULL;
		if(cur != nullptr)
			cur_type = cur->GetType();

		if(cur_type != to_match_type)
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
			evaluableNodeManager->FreeNodeTreeIfPossible(cur);
			return AllocReturn(false, immediate_result);
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(cur);
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(to_match);
	return AllocReturn(true, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TYPE_NEQUALS(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//special (faster) case for comparing two
	if(ocn.size() == 2)
	{
		EvaluableNodeReference a = InterpretNodeForImmediateUse(ocn[0]);
		EvaluableNodeType a_type = ENT_NULL;
		if(a != nullptr)
			a_type = a->GetType();

		auto node_stack = CreateOpcodeStackStateSaver(a);
		EvaluableNodeReference b = InterpretNodeForImmediateUse(ocn[1]);
		EvaluableNodeType b_type = ENT_NULL;
		if(b != nullptr)
			b_type = b->GetType();

		evaluableNodeManager->FreeNodeTreeIfPossible(a);
		evaluableNodeManager->FreeNodeTreeIfPossible(b);
		return AllocReturn(a_type != b_type, immediate_result);
	}

	std::vector<EvaluableNodeReference> values(ocn.size());

	auto node_stack = CreateOpcodeStackStateSaver();

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

			EvaluableNodeType cur1_type = ENT_NULL;
			if(cur1 != nullptr)
				cur1_type = cur1->GetType();

			EvaluableNodeType cur2_type = ENT_NULL;
			if(cur2 != nullptr)
				cur2_type = cur2->GetType();

			//if they're equal, then it fails
			if(cur1_type == cur2_type)
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
