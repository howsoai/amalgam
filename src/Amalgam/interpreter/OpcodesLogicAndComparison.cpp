//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Logic and Comparison";

static OpcodeInitializer _ENT_AND(ENT_AND, &Interpreter::InterpretNode_ENT_AND, []() {
	OpcodeDetails d;
	d.parameters = R"([bool condition1] [bool condition2] ... [bool conditionN])";
	d.returns = R"(any)";
	d.allowsConcurrency = true;
	d.description = R"(If all condition expressions are true, evaluates to `conditionN`.  Otherwise evaluates to false.)";
	d.examples = MakeAmalgamExamples({
		{R"&((and 1 4.8 "true" .true))&", R"(.true)"},
		{R"&((and 1 0 "true" .true))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 21.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_AND(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference cur = EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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
			{
				evaluableNodeManager->FreeNodeIfPossible(cur);
				return AllocReturn(false, immediate_result);
			}
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

static OpcodeInitializer _ENT_OR(ENT_OR, &Interpreter::InterpretNode_ENT_OR, []() {
	OpcodeDetails d;
	d.parameters = R"([bool condition1] [bool condition2] ... [bool conditionN])";
	d.returns = R"(any)";
	d.allowsConcurrency = true;
	d.description = R"(If all condition expressions are false, evaluates to false.  Otherwise evaluates to the first condition that is true.)";
	d.examples = MakeAmalgamExamples({
		{R"&((or .true .false))&", R"(.true)"},
		{R"&((or .false .false .false))&", R"(.false)"},
		{R"&((or 1 0 "true"))&", R"(1)"},
		{R"&((or 1 4.8 "true"))&", R"(1)"},
		{R"&((or 0 0 ""))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 12.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_OR(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference cur = EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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

static OpcodeInitializer _ENT_XOR(ENT_XOR, &Interpreter::InterpretNode_ENT_XOR, []() {
	OpcodeDetails d;
	d.parameters = R"([bool condition1] [bool condition2] ... [bool conditionN])";
	d.returns = R"(any)";
	d.allowsConcurrency = true;
	d.description = R"(If an even number of condition expressions are true, evaluates to false.  Otherwise evaluates to true.)";
	d.examples = MakeAmalgamExamples({
		{R"&((xor .true .true))&", R"(.false)"},
		{R"&((xor .true .false))&", R"(.true)"},
		{R"&((xor 1 4.8 "true"))&", R"(.true)"},
		{R"&((xor 1 0 "true"))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_XOR(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	size_t num_true = 0;

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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

static OpcodeInitializer _ENT_NOT(ENT_NOT, &Interpreter::InterpretNode_ENT_NOT, []() {
	OpcodeDetails d;
	d.parameters = R"(bool condition)";
	d.returns = R"(bool)";
	d.description = R"(Evaluates to false if `condition` is true, true if false.)";
	d.examples = MakeAmalgamExamples({
		{R"&((not .true))&", R"(.false)"},
		{R"&((not .false))&", R"(.true)"},
		{R"&((not 1))&", R"(.false)"},
		{R"&((not ""))&", R"(.true)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 12.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_NOT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNode(ocn[0], true);
	if(cur.IsImmediateValue())
	{
		bool is_true = cur.GetValue().GetValueAsBoolean();
		evaluableNodeManager->FreeNodeIfPossible(cur);
		return AllocReturn(!is_true, immediate_result);
	}
	else
	{
		bool is_true = EvaluableNode::ToBool(cur);
		evaluableNodeManager->FreeNodeTreeIfPossible(cur);
		return AllocReturn(!is_true, immediate_result);
	}
}

static OpcodeInitializer _ENT_EQUAL(ENT_EQUAL, &Interpreter::InterpretNode_ENT_EQUAL, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to true if the value of all nodes are equal, false otherwise. Values of null are considered equal, and any complex data structures will be traversed evaluated for deep equality.)";
	d.examples = MakeAmalgamExamples({
		{R"&((= 4 4 5))&", R"(.false)"},
		{R"&((= 4 4 4))&", R"(.true)"},
		{R"&((=
	(sqrt -1)
	(null)
))&", R"(.true)"},
			{R"&((= (null) (null)))&", R"(.true)"},
			{R"&((= .infinity .infinity))&", R"(.true)"},
			{R"&((= .infinity -.infinity))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 41.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_EQUAL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool processed_first_value = false;
	EvaluableNodeReference to_match = EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
		std::vector<EvaluableNodeReference> interpreted_nodes;
		if(InterpretEvaluableNodesConcurrently(en, ocn, interpreted_nodes))
		{
			bool return_value = true;

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
					return_value = false;
					break;
				}
			}

			for(auto &cur : interpreted_nodes)
				evaluableNodeManager->FreeNodeTreeIfPossible(cur);
			return AllocReturn(return_value, immediate_result);
		}
	}
#endif

	auto node_stack = CreateOpcodeStackStateSaver();

	for(auto &cn : ocn)
	{
		auto cur = InterpretNode(cn);

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

static OpcodeInitializer _ENT_NEQUAL(ENT_NEQUAL, &Interpreter::InterpretNode_ENT_NEQUAL, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to true if no two values are equal, false otherwise.  Values of null are considered equal, and any complex data structures will be traversed evaluated for deep equality.)";
	d.examples = MakeAmalgamExamples({
		{R"&((!= 4 4))&", R"(.false)"},
		{R"&((!= 4 5))&", R"(.true)"},
		{R"&((!= 4 4 5))&", R"(.false)"},
		{R"&((!= 4 4 4))&", R"(.false)"},
		{R"&((!= 4 4 "hello" 4))&", R"(.false)"},
		{R"&((!= 4 4 4 1 3 "hello"))&", R"(.false)"},
		{R"&((!=
	1
	2
	3
	4
	5
	6
	"hello"
))&", R"(.true)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 18.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_NEQUAL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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
	}
#endif

	//special (faster) case for comparing two
	if(ocn.size() == 2)
	{
		EvaluableNodeReference a = InterpretNode(ocn[0]);

		auto node_stack = CreateOpcodeStackStateSaver(a);
		EvaluableNodeReference b = InterpretNode(ocn[1]);

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
		values.push_back(InterpretNode(ocn[i]));
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

static OpcodeInitializer _ENT_LESS(ENT_LESS, &Interpreter::InterpretNode_ENT_LESS_and_LEQUAL, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to true if all values are in strict increasing order, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((< 4 5))&", R"(.true)"},
		{R"&((< 4 4))&", R"(.false)"},
		{R"&((< 4 5 6))&", R"(.true)"},
		{R"&((< 4 5 6 5))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 5.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_LEQUAL(ENT_LEQUAL, &Interpreter::InterpretNode_ENT_LESS_and_LEQUAL, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to true if all values are in nondecreasing order, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((<= 4 5))&", R"(.true)"},
		{R"&((<= 4 4))&", R"(.true)"},
		{R"&((<= 4 5 6))&", R"(.true)"},
		{R"&((<= 4 5 6 5))&", R"(.false)"},
		{R"&((<= (null) 2))&", R"(.false)"},
		{R"&((<= 2 (null)))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 5.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LESS_and_LEQUAL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//if none or one node, then there's no order
	if(ocn.size() < 2)
		return AllocReturn(false, immediate_result);

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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
	}
#endif

	auto prev = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(prev))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		return AllocReturn(false, immediate_result);
	}

	auto node_stack = CreateOpcodeStackStateSaver(prev);

	for(size_t i = 1; i < ocn.size(); i++)
	{
		//if not in strict increasing order, return false
		auto cur = InterpretNode(ocn[i]);

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

static OpcodeInitializer _ENT_GREATER(ENT_GREATER, &Interpreter::InterpretNode_ENT_GREATER_and_GEQUAL, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to true if all values are in strict decreasing order, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((> 6 5))&", R"(.true)"},
		{R"&((> 4 4))&", R"(.false)"},
		{R"&((> 6 5 4))&", R"(.true)"},
		{R"&((> 6 5 4 5))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 5.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_GEQUAL(ENT_GEQUAL, &Interpreter::InterpretNode_ENT_GREATER_and_GEQUAL, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to true if all values are in nonincreasing order, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((>= 6 5))&", R"(.true)"},
		{R"&((>= 4 4))&", R"(.true)"},
		{R"&((>= 6 5 4))&", R"(.true)"},
		{R"&((>= 6 5 4 5))&", R"(.false)"},
		{R"&((>= (null) 2))&", R"(.false)"},
		{R"&((>= 2 (null)))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 5.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GREATER_and_GEQUAL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//if none or one node, then it's in order
	if(ocn.size() < 2)
		return AllocReturn(false, immediate_result);

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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
	}
#endif

	auto prev = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(prev))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(prev);
		return AllocReturn(false, immediate_result);
	}

	auto node_stack = CreateOpcodeStackStateSaver(prev);

	for(size_t i = 1; i < ocn.size(); i++)
	{
		//if not in strict increasing order, return false
		auto cur = InterpretNode(ocn[i]);

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

static OpcodeInitializer _ENT_TYPE_EQUALS(ENT_TYPE_EQUALS, &Interpreter::InterpretNode_ENT_TYPE_EQUALS, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to true if all values are of the same data type, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((~ 1 4 5))&", R"(.true)"},
		{R"&((~ 1 4 "a"))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 2.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TYPE_EQUALS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool processed_first_value = false;
	EvaluableNodeReference to_match = EvaluableNodeReference::Null();
	EvaluableNodeType to_match_type = ENT_NULL;

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency())
	{
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
	}
#endif

	auto node_stack = CreateOpcodeStackStateSaver();

	for(auto &cn : ocn)
	{
		auto cur = InterpretNode(cn);

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

static OpcodeInitializer _ENT_TYPE_NEQUALS(ENT_TYPE_NEQUALS, &Interpreter::InterpretNode_ENT_TYPE_NEQUALS, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(bool)";
	d.description = R"(Evaluates to true if no two values are of the same data types, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((!~
	"true"
	"false"
	[3 2]
))&", R"(.false)"},
			{R"&((!~
	"true"
	1
	[3 2]
))&", R"(.true)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TYPE_NEQUALS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//special (faster) case for comparing two
	if(ocn.size() == 2)
	{
		EvaluableNodeReference a = InterpretNode(ocn[0]);
		EvaluableNodeType a_type = ENT_NULL;
		if(a != nullptr)
			a_type = a->GetType();

		auto node_stack = CreateOpcodeStackStateSaver(a);
		EvaluableNodeReference b = InterpretNode(ocn[1]);
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
		values[i] = InterpretNode(ocn[i]);
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
