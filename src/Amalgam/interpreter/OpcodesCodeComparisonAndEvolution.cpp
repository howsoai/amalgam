//project headers:

#include "EvaluableNodeTreeDifference.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Code Comparison and Evolution";

static OpcodeInitializer _ENT_TOTAL_SIZE(ENT_TOTAL_SIZE, &Interpreter::InterpretNode_ENT_TOTAL_SIZE, []() {
	OpcodeDetails d;
	d.parameters = R"(* node)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the total count of all of the nodes referenced directly or indirectly by `node`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((total_size
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		[5 6]
	]
))&", R"(10)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TOTAL_SIZE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	double total_size = static_cast<double>(EvaluableNode::GetDeepSize(n));
	evaluableNodeManager->FreeNodeTreeIfPossible(n);

	return AllocReturn(total_size, immediate_result);
}

static OpcodeInitializer _ENT_MUTATE(ENT_MUTATE, &Interpreter::InterpretNode_ENT_MUTATE, []() {
	OpcodeDetails d;
	d.parameters = R"(* node [number mutation_rate] [assoc mutation_weights] [assoc operation_type] [preserve_type_depth])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to a mutated version of `node`.  The `mutation_rate` can range from 0.0 to 1.0 and defaulting to 0.00001, and indicates the probability that any node will experience a mutation.  The parameter `mutation_weights` is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The parameter `operation_type` is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings "change_type", "delete", "insert", "swap_elements", "deep_copy_elements", and "delete_elements".  If `preserve_type_depth` is specified, it will retain the types of node down to and including whatever depth is specified, and defaults to 0 indicating that none of the structure needs to be preserved.)";
	d.examples = MakeAmalgamExamples({
		{R"&((mutate
	(lambda
		[
			1
			2
			3
			4
			5
			6
			7
			8
			9
			10
			11
			12
			13
			14
			(associate "a" 1 "b" 2)
		]
	)
	0.4
))&", R"([
	1
	(and)
	3
	{}
	5
	6
	(tail)
	(get)
	(acos)
	(floor)
	(let)
	12
	zbiqZH
	14
	(associate (null))
])",
//accept anything since mutation can do anything
".*"},
{R"&((mutate
	(lambda
		[
			1
			2
			3
			4
			(associate "alpha" 5 "beta" 6)
			(associate
				"nest"
				(associate
					"count"
					[7 8 9]
				)
				"end"
				[10 11 12]
			)
		]
	)
	0.2
	(associate "+" 0.5 "-" 0.3 "*" 0.2)
	(associate "change_type" 0.08 "delete" 0.02 "insert" 0.9)
))&", R"([
	1
	(-)
	3
	(-)
	(associate "alpha" 5 (+) 6)
	(associate
		"nest"
		(associate
			"count"
			[(*) 8 9]
		)
		"end"
		[(*) 11 12]
	)
])",
//accept anything since mutation can do anything
".*"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

	size_t preserve_type_depth = 0;
	if(ocn.size() > 4)
		preserve_type_depth = static_cast<size_t>(std::max(0.0, InterpretNodeIntoNumberValue(ocn[4])));

	//result contains the copied result which may incur replacements
	EvaluableNode *result = EvaluableNodeTreeManipulation::MutateTree(this, evaluableNodeManager,
		to_mutate, mutation_rate, mtw_exists ? &mutation_type_weights : nullptr,
		ow_exists ? &opcode_weights : nullptr, preserve_type_depth);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	return EvaluableNodeReference(result, true);
}

static OpcodeInitializer _ENT_GET_MUTATION_DEFAULTS(ENT_GET_MUTATION_DEFAULTS, &Interpreter::InterpretNode_ENT_GET_MUTATION_DEFAULTS, []() {
	OpcodeDetails d;
	d.parameters = R"(string value_type)";
	d.returns = R"(any)";
	d.description = R"(Retrieves the default values of `value_type` for mutation, either "mutation_opcodes" or "mutation_types")";
	d.examples = MakeAmalgamExamples({
		{R"((get_mutation_defaults "mutation_types"))", R"({
	change_type 0.29
	deep_copy_elements 0.07
	delete 0.1
	delete_elements 0.05
	insert 0.25
	swap_elements 0.24
})"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_MUTATION_DEFAULTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();
	//get the string key
	std::string key = InterpretNodeIntoStringValueEmptyNull(ocn[0]);

	if(key == "mutation_opcodes")
	{
		EvaluableNode *out_node = evaluableNodeManager->AllocNode(ENT_ASSOC);
		out_node->ReserveMappedChildNodes(NUM_VALID_ENT_OPCODES);
		for(size_t node_type = 0; node_type < NUM_VALID_ENT_OPCODES; node_type++)
		{
			EvaluableNode *num_node = evaluableNodeManager->AllocNode(_opcode_details[node_type].frequencyPer10000Opcodes);

			StringInternPool::StringID node_type_sid = GetStringIdFromNodeType(static_cast<EvaluableNodeType>(node_type));
			out_node->SetMappedChildNode(node_type_sid, num_node);
		}

		return EvaluableNodeReference(out_node, true);
	}

	if(key == "mutation_types")
	{
		EvaluableNode *out_node = evaluableNodeManager->AllocNode(ENT_ASSOC);
		out_node->ReserveMappedChildNodes(EvaluableNodeTreeManipulation::mutationOperationTypeProbabilities.size());
		for(auto &[op_type, op_prob] : EvaluableNodeTreeManipulation::mutationOperationTypeProbabilities)
		{
			EvaluableNode *num_node = evaluableNodeManager->AllocNode(op_prob);

			StringInternPool::StringID op_type_sid = GetStringIdFromBuiltInStringId(op_type);
			out_node->SetMappedChildNode(op_type_sid, num_node);
		}

		return EvaluableNodeReference(out_node, true);
	}

	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_COMMONALITY(ENT_COMMONALITY, &Interpreter::InterpretNode_ENT_COMMONALITY, []() {
	OpcodeDetails d;
	d.parameters = R"(* node1 * node2 [assoc params])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the total count of all of the nodes referenced within `node1` and `node2` that are equivalent.  The assoc `params` can contain the keys "string_edit_distance", "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "use_string_edit_distance" is true (default is false), it will assume `node1` and `node2` as string literals and compute via string edit distance.  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
	d.examples = MakeAmalgamExamples({
		{R"&((commonality
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"(3)"},
			{R"&((commonality
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"(15)"},
			{R"&((commonality .infinity 3))&", R"(0.125)"},
			{R"&((commonality
	(null)
	3
	{types_must_match .false}
))&", R"(0.125)"},
			{R"&((commonality .infinity .infinity))&", R"(1)"},
			{R"&((commonality .infinity -.infinity))&", R"(0.125)"},
			{R"&((commonality "hello" "hello"))&", R"(1)"},
			{R"&((commonality
	"hello"
	"hello"
	{string_edit_distance .true}
))&", R"(5)"},
			{R"&((commonality
	"hello"
	"el"
	{nominal_strings .false}
))&", R"(0.49099467997549845)"},
			{R"&((commonality
	"hello"
	"el"
	{string_edit_distance .true}
))&", R"(2)"},
			{R"&((commonality
	"el"
	"hello"
	{string_edit_distance .true}
))&", R"(2)"},
			{R"&((commonality
	(lambda
		{a 1 b 2 c 3}
	)
	(lambda
		(if
			x
			{a 1 b 2 c 3}
			.false
		)
	)
))&", R"(4)"},
			{R"&((commonality
	[1 2 3]
	[
		[1 2 3]
	]
))&", R"(4)"},
			{R"&((commonality
	[1 2 3]
	(lambda
		(null 1 2 3)
	)
	{types_must_match .false}
))&", R"(3.125)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_EDIT_DISTANCE(ENT_EDIT_DISTANCE, &Interpreter::InterpretNode_ENT_EDIT_DISTANCE, []() {
	OpcodeDetails d;
	d.parameters = R"(* node1 * node2 [assoc params])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the number of nodes that are different between `node1` and `node2`. The assoc `params` can contain the keys "string_edit_distance", "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "use_string_edit_distance" is true (default is false), it will assume `node1` and `node2` as string literals and compute via string edit distance.  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
	d.examples = MakeAmalgamExamples({
		{R"&((edit_distance
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"(3)"},
			{R"&((edit_distance
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"(2)"},
			{R"&((edit_distance "hello" "hello"))&", R"(0)"},
			{R"&((edit_distance
	"hello"
	"hello"
	{string_edit_distance .true}
))&", R"(0)"},
			{R"&((edit_distance
	"hello"
	"el"
	{nominal_strings .false}
))&", R"(1.018010640049003)"},
			{R"&((edit_distance
	"hello"
	"el"
	{string_edit_distance .true}
))&", R"(3)"},
			{R"&((edit_distance
	"el"
	"hello"
	{string_edit_distance .true}
))&", R"(3)"},
			{R"&((edit_distance
	[1 2 3]
	(lambda
		(unordered_list
			[1 2 3]
		)
	)
))&", R"(1)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_INTERSECT(ENT_INTERSECT, &Interpreter::InterpretNode_ENT_INTERSECT, []() {
	OpcodeDetails d;
	d.parameters = R"(* node1 * node2 [assoc params])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to whatever is common between `node1` and `node2` exclusive.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
	d.examples = MakeAmalgamExamples({
		{R"&((intersect
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "a" 3 "b" 4)
	]
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "c" 3 "b" 4)
	]
))&", R"([
	1
	(- 4 2)
	{b 4}
])"},
			{R"&((intersect
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"((seq 2 1))"},
			{R"&((intersect
	(lambda
		(unordered_list (get_entity_comments) 1 2)
	)
	(lambda
		(unordered_list (get_entity_comments) 1 2 4)
	)
))&", R"((unordered_list (get_entity_comments) 1 2))"},
			{R"&((intersect
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"([
	1
	2
	3
	{b 4}
	(if
		true
		1
		(unordered_list (get_entity_comments) 1)
	)
	[5 6]
])"},
			{R"&((intersect
	(lambda
		[
			1
			(associate "a" 3 "b" 4)
		]
	)
	(lambda
		[
			1
			(associate "c" 3 "b" 4)
		]
	)
))&", R"([
	1
	(associate (null) 3 "b" 4)
])"},
			{R"&((intersect
	(lambda
		(replace 4 2 6 1 7)
	)
	(lambda
		(replace 4 1 7 2 6)
	)
))&", R"((replace 4 2 6 1 7))"},
			{R"&((unparse
	(intersect
		(lambda
			[
				
				;comment 1
				;comment 2
				;comment 3
				1
				3
				5
				7
				9
				11
				13
			]
		)
		(lambda
			[
				
				;comment 2
				;comment 3
				;comment 4
				1
				4
				6
				8
				10
				12
				14
			]
		)
	)
	.true
	.true
	.true
))&", R"("[\r\n\t\r\n\t;comment 2\r\n\t;comment 3\r\n\t1\r\n]\r\n")"},
			{R"&((intersect
	[1 2 3]
	[
		[1 2 3]
	]
))&", R"([1 2 3])"},
			{R"&((intersect
	[1 2 3]
	[
		[1 2 3]
	]
	{recursive_matching .false}
))&", R"([])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = true;
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

static OpcodeInitializer _ENT_UNION(ENT_UNION, &Interpreter::InterpretNode_ENT_UNION, []() {
	OpcodeDetails d;
	d.parameters = R"(* node1 * node2 [assoc params])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to whatever is inclusive when merging `node1` and `node2`.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
	d.examples = MakeAmalgamExamples({
		{R"&((union
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"((seq 2 (get_entity_comments) 1 4 (get_entity_comments)))"},
			{R"&((union
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "a" 3 "b" 4)
	]
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "c" 3 "b" 4)
	]
))&", R"([
	1
	(- 4 2)
	{a 3 b 4 c 3}
])"},
			{R"&((union
	(lambda
		(unordered_list (get_entity_comments) 1 2)
	)
	(lambda
		(unordered_list (get_entity_comments) 1 2 4)
	)
))&", R"((unordered_list (get_entity_comments) 1 2 4))"},
			{R"&((union
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"([
	1
	2
	3
	{a 3 b 4 c 3}
	(if
		true
		1
		(unordered_list (get_entity_comments) 1)
	)
	[5 6]
])"},
			{R"&((union
	(lambda
		[
			1
			(associate "a" 3 "b" 4)
		]
	)
	(lambda
		[
			1
			(associate "c" 3 "b" 4)
		]
	)
))&", R"([
	1
	(associate (null) 3 "b" 4)
])"},
			{R"&((union
	[3 2]
	[3 4]
))&", R"([3 4 2])", R"(\[(?:3 4 2|3 2 4)\])"},
			{R"&((union
	[2 3]
	[3 2 4]
))&", R"([3 2 4 3])", R"(\[(?:3 4 2 3|2 3 2 4)\])" },
			{R"&((unparse
	(union
		(lambda
			[
				
				;comment 1
				;comment 2
				;comment 3
				1
				2
				3
				5
				7
				9
				11
				13
			]
		)
		(lambda
			[
				
				;comment 2
				;comment 3
				;comment 4
				1
				
				;comment x
				2
				4
				6
				8
				10
				12
				14
			]
		)
	)
	.true
	.true
	.true
))&", R"("[\r\n\t\r\n\t;comment 1\r\n\t;comment 2\r\n\t;comment 3\r\n\t;comment 4\r\n\t1\r\n\t\r\n\t;comment x\r\n\t2\r\n\t4\r\n\t3\r\n\t6\r\n\t5\r\n\t8\r\n\t7\r\n\t10\r\n\t9\r\n\t12\r\n\t11\r\n\t14\r\n\t13\r\n]\r\n")",
R"("\[\\r\\n\\t\\r\\n\\t;comment 1\\r\\n\\t;comment 2\\r\\n\\t;comment 3\\r\\n\\t;comment 4\\r\\n\\t1\\r\\n\\t\\r\\n\\t;comment x\\r\\n\\t2\\r\\n\\t.*")" },
			{R"&((union
	[1 2 3]
	[
		[1 2 3]
	]
))&", R"([
	[1 2 3]
])"},
			{R"&((union
	[
		[1 2 3]
	]
	[1 2 3]
))&", R"([
	[1 2 3]
])"},
			{R"&((union
	[1 2 3]
	(lambda
		[
			[1 2 3]
		]
	)
))&", R"([
	[1 2 3]
])"},
			{R"&((union
	[1 2 3]
	(lambda
		[
			[1 2 3]
		]
	)
	{recursive_matching .false}
))&", R"([
	1
	2
	3
	[1 2 3]
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = true;
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

static OpcodeInitializer _ENT_DIFFERENCE(ENT_DIFFERENCE, &Interpreter::InterpretNode_ENT_DIFFERENCE, []() {
	OpcodeDetails d;
	d.parameters = R"(* node1 * node2)";
	d.returns = R"(any)";
	d.description = R"(Finds the difference between `node1` and `node2`, and generates code that, if evaluated passing `node1` as its parameter "_", would turn it into `node2`.  Useful for finding a small difference of what needs to be changed to apply it to new (and possibly slightly different) data or code.)";
	d.examples = MakeAmalgamExamples({
		{R"&((difference
	(lambda
		{
			a 1
			b 2
			c 4
			d 7
			e 10
			f 12
			g 13
		}
	)
	(lambda
		[
			a
			2
			c
			4
			d
			6
			q
			8
			e
			10
			f
			12
			g
			14
		]
	)
))&", R"((declare
	{_ (null)}
	(replace
		_
		[]
		(lambda
			[
				a
				2
				c
				4
				d
				6
				q
				8
				e
				10
				f
				12
				g
				14
			]
		)
	)
))"},
			{R"&((difference
	{
		a 1
		b 2
		c 4
		d 7
		e 10
		f 12
		g 13
	}
	{
		a 2
		c 4
		d 6
		e 10
		f 12
		g 14
		q 8
	}
))&", R"((declare
	{_ (null)}
	(replace
		_
		[]
		(lambda
			{
				a 2
				c (get
						(current_value 1)
						"c"
					)
				d 6
				e (get
						(current_value 1)
						"e"
					)
				f (get
						(current_value 1)
						"f"
					)
				g 14
				q 8
			}
		)
	)
))"},
			{R"&((difference
	(lambda
		[
			1
			2
			4
			7
			10
			12
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
))&", R"((declare
	{_ (null)}
	(replace
		_
		[]
		(lambda
			[
				(get
					(current_value 1)
					1
				)
				(get
					(current_value 1)
					2
				)
				6
				8
				(get
					(current_value 1)
					4
				)
				(get
					(current_value 1)
					5
				)
				14
			]
		)
	)
))"},
			{R"&((unparse
	(difference
		(lambda
			{
				a 1
				b 2
				c 4
				d 7
				e 10
				f 12
				g 13
			}
		)
		(lambda
			{
				a 2
				c 4
				d 6
				e 10
				f 12
				g 14
				q 8
			}
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{\r\n\t\t\t\ta 2\r\n\t\t\t\tc (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"c\"\r\n\t\t\t\t\t)\r\n\t\t\t\td 6\r\n\t\t\t\te (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"e\"\r\n\t\t\t\t\t)\r\n\t\t\t\tf (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"f\"\r\n\t\t\t\t\t)\r\n\t\t\t\tg 14\r\n\t\t\t\tq 8\r\n\t\t\t}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(lambda
			(associate
				a
				1
				g
				[1 2]
			)
		)
		(lambda
			(associate
				a
				2
				g
				[1 4]
			)
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[3]\r\n\t\t(lambda\r\n\t\t\t[\r\n\t\t\t\t(get\r\n\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t0\r\n\t\t\t\t)\r\n\t\t\t\t4\r\n\t\t\t]\r\n\t\t)\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t(set_type\r\n\t\t\t\t[\r\n\t\t\t\t\ta\r\n\t\t\t\t\t2\r\n\t\t\t\t\tg\r\n\t\t\t\t\t(get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t3\r\n\t\t\t\t\t)\r\n\t\t\t\t]\r\n\t\t\t\t\"associate\"\r\n\t\t\t)\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(zip
			[1 2 3 4 5]
		)
		(append
			(zip
				[2 6 5]
			)
			{a 1}
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{\r\n\t\t\t\t2 (null)\r\n\t\t\t\t5 (null)\r\n\t\t\t\t6 (null)\r\n\t\t\t\ta 1\r\n\t\t\t}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(zip
			[1 2 3 4 5]
		)
		(zip
			[2 6 5]
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{2 (null) 5 (null) 6 (null)}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(zip
			[1 2 5]
		)
		(zip
			[2 6 5]
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{2 (null) 5 (null) 6 (null)}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((let
	{
		x (lambda
				[
					6
					[1 2]
				]
			)
		y (lambda
				[
					7
					[1 4]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
))&", R"([
	7
	[1 4]
])"},
			{R"&((let
	{
		x (lambda
				[
					(+ 0 1)
					[1 2]
				]
			)
		y (lambda
				[
					(+ 7 8)
					[1 4]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
))&", R"([
	(+ 7 8)
	[1 4]
])"},
			{R"&((let
	{
		x (lambda
				[
					6
					[
						["a" "b"]
						1
						2
					]
				]
			)
		y (lambda
				[
					7
					[
						["a" "x"]
						1
						4
					]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
))&", R"([
	7
	[
		["a" "x"]
		1
		4
	]
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_MIX(ENT_MIX, &Interpreter::InterpretNode_ENT_MIX, []() {
	OpcodeDetails d;
	d.parameters = R"(* node1 * node2 [number keep_chance_node1] [number keep_chance_node2] [assoc params])";
	d.returns = R"(any)";
	d.description = R"(Performs a union operation on `node1` and `node2`, but randomly ignores nodes from one or the other if the nodes are not equal.  If only `keep_chance_node1` is specified, `keep_chance_node2` defaults to 1 - `keep_chance_node1`. `keep_chance_node1` specifies the probability that a node from `node1` will be kept, and `keep_chance_node2` the probability that a node from `node2` will be kept.  `keep_chance_node1` + `keep_chance_node2` should be between 1 and 2, as there are two objects being merged, otherwise the values will be normalized.  `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", "recursive_matching", and "similar_mix_chance".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  "similar_mix_chance" is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number and string values based on `keep_chance_node1` and `keep_chance_node2`, and defaults to 0.0.  If "similar_mix_chance" is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.)";
	d.examples = MakeAmalgamExamples({
		{R"&((mix
	(lambda
		[
			1
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	0
))&", R"([1 3 4 9 11 14])",
//accept anything since mutation can do anything
".*"},
{R"&((mix
	(lambda
		[
	
			;comment 1
			;comment 2
			;comment 3
			1
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			
			;comment 2
			;comment 3
			;comment 4
			1
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance 0}
))&", R"([
	
	;comment 1
	;comment 2
	;comment 3
	;comment 4
	1
	4
	3
	5
	9
	11
	14
])",
//accept anything since mutation can do anything
".*"},
{R"&((mix
	(lambda
		[
			1
			2
			(associate "a" 3 "b" 4)
			(lambda
				(if
					true
					1
					(unordered_list (get_entity_comments) 1)
				)
			)
			[5 6]
		]
	)
	(lambda
		[
			1
			5
			3
			(associate "a" 3 "b" 4)
			(lambda
				(if
					false
					1
					(unordered_list
						(get_entity_comments)
						(lambda
							[2 9]
						)
					)
				)
			)
		]
	)
	0.8
	0.8
	{similar_mix_chance 0.5}
))&", R"([
	1
	5
	3
	(associate "a" 3 "b" 4)
	(lambda
		(if
			true
			1
			(unordered_list
				(get_entity_comments)
				(lambda
					[2 9]
				)
			)
		)
	)
	[5]
])",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	(lambda
		[
			1
			2
			(associate "a" 3 "b" 4)
			(lambda
				(if
					true
					1
					(unordered_list (get_entity_comments) 1)
				)
			)
			[5 6]
		]
	)
	(lambda
		[
			1
			5
			3
			{a 3 b 4}
			(lambda
				(if
					false
					1
					(seq
						(get_entity_comments)
						(lambda
							[2 9]
						)
					)
				)
			)
		]
	)
	0.8
	0.8
	{similar_mix_chance 1}
))&", R"([
	1
	2.5
	{a 3 b 4}
	(associate "a" 3 "b" 4)
	(lambda
		(if
			true
			1
			(seq
				(get_entity_comments)
				(lambda
					[2 9]
				)
			)
		)
	)
	[5]
])",
//accept anything since mutation can do anything
".*"},
{R"&((mix
	(lambda
		[
			.true
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance 1}
))&", R"([
	.true
	3
	5
	7.5
	9.5
	11.5
	13.5
])",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	(lambda
		[
			.true
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance -1}
))&", R"([3 5 2 4 12 11])",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance -1}
))&", R"(4)",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance -0.8}
))&", R"(4)",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance 0.5}
))&", R"(1)",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance 1}
))&", R"(2.5)",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
))&", R"("abcdexyz")",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
))&", R"("abcdexyz")",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
))&", R"("abcdexyz")",
//accept anything since mutation can do anything
".*" },
{R"&((mix
	{
		a [0 1]
		b [1 2]
		c [2 3]
	}
	{
		a [0 1]
		b [1 2]
		w [2 3]
		x [3 4]
		y [4 5]
		z [5 6]
	}
	0.5
	0.5
	{recursive_matching .false}
))&", R"({
	a [0 1]
	b [1 2]
	w [2]
	z [5]
})",
//accept anything since mutation can do anything
".*" }
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

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
