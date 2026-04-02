//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"

//system headers:
#include <regex>


static std::string _opcode_group = "Container Manipulation";

static OpcodeInitializer _ENT_FIRST(ENT_FIRST, &Interpreter::InterpretNode_ENT_FIRST, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc|number|string data])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the first element of `data`.  If `data` is a list, it will be the first element.  If `data` is an assoc, it will evaluate to the first element by assoc storage, but order does not matter.  If `data` is a string, it will be the first character.  If `data` is a number, it will evaluate to 1 if nonzero, 0 if zero.)";
	d.examples = MakeAmalgamExamples({
		{R"&((first
	[4 9.2 "this"]
))&", R"(4)"},
			{R"&((first
	(associate "a" 1 "b" 2)
))&", R"(2)", R"(1|2)"},
			{R"&((first 3))&", R"(1)"},
			{R"&((first 0))&", R"(0)"},
			{R"&((first "abc"))&", R"("a")"},
			{R"&((first ""))&", R"((null))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 20.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_FIRST(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//get the "list" itself
	auto list = InterpretNodeForImmediateUse(ocn[0]);
	if(list == nullptr)
		return EvaluableNodeReference::Null();

	if(list->IsOrderedArray())
	{
		auto &list_ocn = list->GetOrderedChildNodesReference();
		if(list_ocn.size() > 0)
		{
			EvaluableNodeReference first(list_ocn[0], list.unique);
			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(size_t i = 1; i < list_ocn.size(); i++)
					evaluableNodeManager->FreeNodeTree(list_ocn[i]);
			}
			evaluableNodeManager->FreeNodeIfPossible(list);
			return first;
		}
	}
	else if(list->IsAssociativeArray())
	{
		auto &list_mcn = list->GetMappedChildNodesReference();
		if(list_mcn.size() > 0)
		{
			//keep reference to first of map before free rest of it
			const auto &first_itr = begin(list_mcn);
			EvaluableNode *first_en = first_itr->second;

			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(auto &[_, cn] : list_mcn)
				{
					if(cn != first_en)
						evaluableNodeManager->FreeNodeTree(cn);
				}
			}
			evaluableNodeManager->FreeNodeIfPossible(list);
			return EvaluableNodeReference(first_en, list.unique);
		}
	}
	else //if(list->IsImmediate())
	{
		if(DoesEvaluableNodeTypeUseStringData(list->GetType()))
		{
			auto sid = list->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID || sid == string_intern_pool.emptyStringId)
				return AllocReturn(StringInternPool::NOT_A_STRING_ID, immediate_result);

			auto &s = string_intern_pool.GetStringFromID(sid);
			size_t utf8_char_length = StringManipulation::GetUTF8CharacterLength(s, 0);
			std::string substring = s.substr(0, utf8_char_length);
			evaluableNodeManager->FreeNodeTreeIfPossible(list);

			return AllocReturn(substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			//return 1 if nonzero
			evaluableNodeManager->FreeNodeTreeIfPossible(list);
			return AllocReturn(1.0, immediate_result);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_TAIL(ENT_TAIL, &Interpreter::InterpretNode_ENT_TAIL, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc|number|string data] [number retain_count])";
	d.returns = R"(list)";
	d.description = R"(Evaluates to everything but the first element.  If `data` is a list, it will be a list of all but the first element.  If `data` is an assoc, it will evaluate to the assoc without the first element by assoc storage order, but order does not matter.  If `data` is a string, it will be all but the first character.  If `data` is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero.  If a `retain_count` is specified, it will be the number of elements to retain.  A positive number means from the end, a negative number means from the beginning.  The default value is -1 (all but the first element).)";
	d.examples = MakeAmalgamExamples({
		{R"&((tail
	[4 9.2 "this"]
))&", R"([9.2 "this"])"},
			{R"&((tail
	[1 2 3 4 5 6]
))&", R"([2 3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	2
))&", R"([5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	-2
))&", R"([3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	-6
))&", R"([])"},
			{R"&((tail
	[1 2 3 4 5 6]
	6
))&", R"([1 2 3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	10
))&", R"([1 2 3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	-10
))&", R"([])"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
))&", R"({
	a 1
	b 2
	c 3
	d 4
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	2
))&", R"({b 2 c 3})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-2
))&", R"({
	a 1
	b 2
	c 3
	d 4
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	10
))&", R"({
	a 1
	b 2
	c 3
	d 4
	e 5
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-10
))&", R"({})"},
			{R"&((tail 3))&", R"(2)"},
			{R"&((tail 0))&", R"(0)"},
			{R"&((tail "abcdef"))&", R"("bcdef")"},
			{R"&((tail "abcdef" 2))&", R"("ef")"},
			{R"&((tail "abcdef" -2))&", R"("cdef")"},
			{R"&((tail "abcdef" 6))&", R"("abcdef")"},
			{R"&((tail "abcdef" -6))&", R"("")"},
			{R"&((tail "abcdef" 10))&", R"("abcdef")"},
			{R"&((tail "abcdef" -10))&", R"("")"},
			{R"&((tail ""))&", R"((null))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 2.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TAIL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto list = InterpretNodeForImmediateUse(ocn[0]);
	if(list == nullptr)
		return EvaluableNodeReference::Null();

	auto node_stack = CreateOpcodeStackStateSaver(list);

	//default to tailing to all but the first element
	double tail_by = -1;
	if(ocn.size() > 1)
		tail_by = InterpretNodeIntoNumberValue(ocn[1]);

	if(list->IsOrderedArray())
	{
		if(list->GetOrderedChildNodesReference().size() > 0)
		{

			evaluableNodeManager->EnsureNodeIsModifiable(list, true);
			//swap on the stack in case list changed
			node_stack.PopEvaluableNode();
			node_stack.PushEvaluableNode(list);

			auto &list_ocn = list->GetOrderedChildNodesReference();
			//remove the first element(s)
			if(tail_by > 0 && tail_by < list_ocn.size())
			{
				double first_index = list_ocn.size() - tail_by;
				list_ocn.erase(begin(list_ocn), begin(list_ocn) + static_cast<size_t>(first_index));
			}
			else if(tail_by < 0)
			{
				//make sure have things to remove while keeping something in the list
				if(-tail_by < list_ocn.size())
					list_ocn.erase(begin(list_ocn), begin(list_ocn) + static_cast<size_t>(-tail_by));
				else //remove everything
					list_ocn.clear();
			}

			return list;
		}
	}
	else if(list->IsAssociativeArray())
	{
		if(list->GetMappedChildNodesReference().size() > 0)
		{
			evaluableNodeManager->EnsureNodeIsModifiable(list, true);
			//swap on the stack in case list changed
			node_stack.PopEvaluableNode();
			node_stack.PushEvaluableNode(list);

			//just remove the first, because it's more efficient and the order does not matter for maps
			size_t num_to_remove = 0;
			if(tail_by > 0 && tail_by < list->GetMappedChildNodesReference().size())
				num_to_remove = list->GetMappedChildNodesReference().size() - static_cast<size_t>(tail_by);
			else if(tail_by < 0)
				num_to_remove = static_cast<size_t>(-tail_by);

			//remove individually
			for(size_t i = 0; list->GetMappedChildNodesReference().size() > 0 && i < num_to_remove; i++)
			{
				const auto &mcn = list->GetMappedChildNodesReference();
				const auto &iter = begin(mcn);
				list->EraseMappedChildNode(iter->first);
			}

			return list;
		}
	}
	else //list->IsImmediate()
	{
		if(DoesEvaluableNodeTypeUseStringData(list->GetType()))
		{
			auto sid = list->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID || sid == string_intern_pool.emptyStringId)
				return AllocReturn(StringInternPool::NOT_A_STRING_ID, immediate_result);

			auto &s = string_intern_pool.GetStringFromID(sid);

			//remove the first element(s)
			size_t num_chars_to_drop = 0;
			if(tail_by > 0)
			{
				size_t num_characters = StringManipulation::GetNumUTF8Characters(s);
				//cap because can't remove a negative number of characters
				num_chars_to_drop = static_cast<size_t>(std::max<double>(0.0, num_characters - tail_by));
			}
			else if(tail_by < 0)
			{
				num_chars_to_drop = static_cast<size_t>(-tail_by);
			}

			//drop the number of characters before this length
			size_t utf8_start_offset = StringManipulation::GetNthUTF8CharacterOffset(s, num_chars_to_drop);

			std::string substring = s.substr(utf8_start_offset, s.size() - utf8_start_offset);
			evaluableNodeManager->FreeNodeTreeIfPossible(list);
			return AllocReturn(substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			evaluableNodeManager->FreeNodeTreeIfPossible(list);
			return AllocReturn(value - 1.0, immediate_result);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_LAST(ENT_LAST, &Interpreter::InterpretNode_ENT_LAST, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc|number|string data])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the last element of `data`.  If `data` is a list, it will be the last element.  If `data` is an assoc, it will evaluate to the first element by assoc storage, because order does not matter.  If `data` is a string, it will be the last character.  If `data` is a number, it will evaluate to 1 if nonzero, 0 if zero.)";
	d.examples = MakeAmalgamExamples({
		{R"&((last
	[4 9.2 "this"]
))&", R"("this")"},
			{R"&((last
	(associate "a" 1 "b" 2)
))&", R"(2)", R"(1|2)"},
			{R"&((last 3))&", R"(1)"},
			{R"&((last 0))&", R"(0)"},
			{R"&((last "abc"))&", R"("c")"},
			{R"&((last ""))&", R"((null))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 13.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LAST(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//get the list itself
	auto list = InterpretNodeForImmediateUse(ocn[0]);
	if(list == nullptr)
		return EvaluableNodeReference::Null();

	if(list->IsOrderedArray())
	{
		auto &list_ocn = list->GetOrderedChildNodesReference();
		if(list_ocn.size() > 0)
		{
			//keep reference to first before free rest of it
			EvaluableNodeReference last(list_ocn[list_ocn.size() - 1], list.unique);

			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(size_t i = 0; i < list_ocn.size() - 1; i++)
					evaluableNodeManager->FreeNodeTree(list_ocn[i]);
			}
			evaluableNodeManager->FreeNodeIfPossible(list);
			return last;
		}
	}
	else if(list->IsAssociativeArray())
	{
		auto &list_mcn = list->GetMappedChildNodesReference();
		if(list_mcn.size() > 0)
		{
			//just take the first, because it's more efficient and the order does not matter for maps
			//keep reference to first of map before free rest of it
			EvaluableNode *last_en = begin(list_mcn)->second;

			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(auto &[_, cn] : list_mcn)
				{
					if(cn != last_en)
						evaluableNodeManager->FreeNodeTree(cn);
				}
			}
			evaluableNodeManager->FreeNodeIfPossible(list);
			return EvaluableNodeReference(last_en, list.unique);
		}
	}
	else //list->IsImmediate()
	{
		if(DoesEvaluableNodeTypeUseStringData(list->GetType()))
		{
			auto sid = list->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID || sid == string_intern_pool.emptyStringId)
				return AllocReturn(StringInternPool::NOT_A_STRING_ID, immediate_result);

			auto &s = string_intern_pool.GetStringFromID(sid);

			auto [utf8_char_start_offset, utf8_char_length] = StringManipulation::GetLastUTF8CharacterOffsetAndLength(s);

			std::string substring = s.substr(utf8_char_start_offset, utf8_char_length);
			evaluableNodeManager->FreeNodeTreeIfPossible(list);

			return AllocReturn(substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			evaluableNodeManager->FreeNodeTreeIfPossible(list);
			return AllocReturn(1.0, immediate_result);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_TRUNC(ENT_TRUNC, &Interpreter::InterpretNode_ENT_TRUNC, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc|number|string data] [number retain_count])";
	d.returns = R"(list)";
	d.description = R"(Truncates, evaluates to everything in `data` but the last element. If `data` is a list, it will be a list of all but the last element.  If `data` is an assoc, it will evaluate to the assoc without the first element by assoc storage order, because order does not matter.  If `data` is a string, it will be all but the last character.  If `data` is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero. If `truncate_count` is specified, it will be the number of elements to retain.  A positive number means from the beginning, a negative number means from the end.  The default value is -1, indicating all but the last.)";
	d.examples = MakeAmalgamExamples({
		{R"&((trunc
	[4 9.2 "end"]
))&", R"([4 9.2])"},
			{R"&((trunc
	[1 2 3 4 5 6]
))&", R"([1 2 3 4 5])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	2
))&", R"([1 2])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	-2
))&", R"([1 2 3 4])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	-6
))&", R"([])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	6
))&", R"([1 2 3 4 5 6])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	10
))&", R"([1 2 3 4 5 6])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	-10
))&", R"([])"},
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
))&", R"({
	a 1
	c 3
	d 4
	e 5
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	2
))&", R"({e 5 f 6})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-2
))&", R"({
	c 3
	d 4
	e 5
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	10
))&", R"({
	a 1
	b 2
	c 3
	d 4
	e 5
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-10
))&", R"({})"},
			{R"&((trunc 3))&", R"(2)"},
			{R"&((trunc 0))&", R"(0)"},
			{R"&((trunc "abcdef"))&", R"("abcde")"},
			{R"&((trunc "abcdef" 2))&", R"("ab")"},
			{R"&((trunc "abcdef" -2))&", R"("abcd")"},
			{R"&((trunc "abcdef" 6))&", R"("abcdef")"},
			{R"&((trunc "abcdef" -6))&", R"("")"},
			{R"&((trunc "abcdef" 10))&", R"("abcdef")"},
			{R"&((trunc "abcdef" -10))&", R"("")"},
			{R"&((trunc ""))&", R"((null))"},

		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 5.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TRUNC(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto list = InterpretNodeForImmediateUse(ocn[0]);
	if(list == nullptr)
		return EvaluableNodeReference::Null();

	auto node_stack = CreateOpcodeStackStateSaver(list);

	//default to truncating to all but the last element
	double truncate_to = -1;
	if(ocn.size() > 1)
		truncate_to = InterpretNodeIntoNumberValue(ocn[1]);

	if(list->IsOrderedArray())
	{
		evaluableNodeManager->EnsureNodeIsModifiable(list, true);
		//swap on the stack in case list changed
		node_stack.PopEvaluableNode();
		node_stack.PushEvaluableNode(list);

		auto &list_ocn = list->GetOrderedChildNodesReference();

		//remove the last element(s)
		if(truncate_to > 0 && truncate_to < list_ocn.size())
		{
			list_ocn.erase(begin(list_ocn) + static_cast<size_t>(truncate_to), end(list_ocn));
		}
		else if(truncate_to < 0)
		{
			//make sure have things to remove while keeping something in the list
			if(-truncate_to < list_ocn.size())
			{
				size_t last_index = static_cast<size_t>(truncate_to + list_ocn.size());
				list_ocn.erase(begin(list_ocn) + last_index, end(list_ocn));
			}
			else //remove everything
				list_ocn.clear();
		}

		return list;
	}
	else if(list->IsAssociativeArray())
	{
		evaluableNodeManager->EnsureNodeIsModifiable(list, true);
		//swap on the stack in case list changed
		node_stack.PopEvaluableNode();
		node_stack.PushEvaluableNode(list);

		//just remove the first, because it's more efficient and the order does not matter for maps
		size_t num_to_remove = 0;
		if(truncate_to > 0 && truncate_to < list->GetMappedChildNodesReference().size())
			num_to_remove = list->GetMappedChildNodesReference().size() - static_cast<size_t>(truncate_to);
		else if(truncate_to < 0)
			num_to_remove = static_cast<size_t>(-truncate_to);

		//remove individually
		for(size_t i = 0; list->GetMappedChildNodesReference().size() > 0 && i < num_to_remove; i++)
		{
			const auto &mcn = list->GetMappedChildNodesReference();
			const auto &iter = begin(mcn);
			list->EraseMappedChildNode(iter->first);
		}

		return list;
	}
	else //if(list->IsImmediate())
	{
		if(DoesEvaluableNodeTypeUseStringData(list->GetType()))
		{
			auto sid = list->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID || sid == string_intern_pool.emptyStringId)
				return AllocReturn(StringInternPool::NOT_A_STRING_ID, immediate_result);

			auto &s = string_intern_pool.GetStringFromID(sid);

			//remove the last element(s)
			size_t num_chars_to_keep = 0;
			if(truncate_to > 0)
			{
				num_chars_to_keep = static_cast<size_t>(truncate_to);
			}
			else if(truncate_to < 0)
			{
				size_t num_characters = StringManipulation::GetNumUTF8Characters(s);

				//cap because can't remove a negative number of characters, and add truncate_to because truncate_to is negative (technically want a subtract)
				num_chars_to_keep = static_cast<size_t>(std::max<double>(0.0, num_characters + truncate_to));
			}

			//remove everything after after this length
			size_t utf8_end_offset = StringManipulation::GetNthUTF8CharacterOffset(s, num_chars_to_keep);
			std::string substring = s.substr(0, utf8_end_offset);
			evaluableNodeManager->FreeNodeTreeIfPossible(list);

			return AllocReturn(substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			//return (value - 1.0) if nonzero
			evaluableNodeManager->FreeNodeTreeIfPossible(list);
			return AllocReturn(value - 1.0, immediate_result);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_APPEND(ENT_APPEND, &Interpreter::InterpretNode_ENT_APPEND, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc|* collection1] [list|assoc|* collection2] ... [list|assoc|* collectionN])";
	d.returns = R"(list|assoc)";
	d.description = R"(Evaluates to a new list or assoc which merges all lists, `collection1` through `collectionN`, based on parameter order. If any assoc is passed in, then returns an assoc (lists will be automatically converted to an assoc with the indices as keys and the list elements as values). If a non-list and non-assoc is specified, then it just adds that one element to the list)";
	d.examples = MakeAmalgamExamples({
		{R"&((append
	[1 2 3]
	[4 5 6]
	[7 8 9]
))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	9
])"},
			{R"&((append
	[1 2 3]
	(associate "a" 4 "b" 5 "c" 6)
	[7 8 9]
	(associate "d" 10 "e" 11)
))&", R"({
	0 1
	1 2
	2 3
	3 7
	4 8
	5 9
	a 4
	b 5
	c 6
	d 10
	e 11
})"},
			{R"&((append
	[4 9.2 "this"]
	"end"
))&", R"([4 9.2 "this" "end"])"},
			{R"&((append
	(associate 0 4 1 9.2 2 "this")
	"end"
))&", R"({
	0 4
	1 9.2
	2 "this"
	3 "end"
})"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 18.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_APPEND(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//pull the first element and reuse its memory if possible;
	//this can drastically reduce memory and improve efficiency for flows that recurse on append
	bool first_append = true;
	EvaluableNodeReference new_list = InterpretNode(ocn[0]);
	if(new_list != nullptr
		&& (new_list->GetType() == ENT_LIST || new_list->GetType() == ENT_ASSOC))
	{
		evaluableNodeManager->EnsureNodeIsModifiable(new_list, true);
	}
	else
	{
		//put the immediate value in a list
		EvaluableNodeReference new_container(evaluableNodeManager->AllocNode(ENT_LIST), true);
		new_container->AppendOrderedChildNode(new_list);
		new_container.UpdatePropertiesBasedOnAttachedNode(new_list, true);
		first_append = false;

		new_list = new_container;
	}

	//iterate over the remaining elements
	auto node_stack = CreateOpcodeStackStateSaver(new_list);
	size_t new_list_cur_index = 0;
	for(size_t param_index = 1; param_index < ocn.size(); param_index++)
	{
		if(AreExecutionResourcesExhausted())
			return EvaluableNodeReference::Null();

		//get evaluated parameter
		auto new_elements = InterpretNode(ocn[param_index]);

		if(EvaluableNode::IsAssociativeArray(new_elements))
		{
			if(new_list->GetType() == ENT_LIST)
			{
				new_list->ConvertListToNumberedAssoc();
				new_list_cur_index = new_list->GetNumChildNodes();
			}

			auto &new_elements_mcn = new_elements->GetMappedChildNodesReference();
			if(new_elements_mcn.size() > 0)
			{
				new_list.UpdatePropertiesBasedOnAttachedNode(new_elements, first_append);
				for(auto &[node_to_insert_id, node_to_insert] : new_elements_mcn)
					new_list->SetMappedChildNode(node_to_insert_id, node_to_insert);
			}

			//don't need the top node anymore
			evaluableNodeManager->FreeNodeIfPossible(new_elements);
		}
		else if(new_elements != nullptr && new_elements->GetType() == ENT_LIST)
		{
			auto &new_elements_ocn = new_elements->GetOrderedChildNodesReference();
			if(new_elements_ocn.size() > 0)
			{
				new_list.UpdatePropertiesBasedOnAttachedNode(new_elements, first_append);
				if(new_list->GetType() == ENT_LIST)
				{
					new_list->GetOrderedChildNodesReference().insert(
						end(new_list->GetOrderedChildNodesReference()), begin(new_elements_ocn), end(new_elements_ocn));
				}
				else
				{
					//find the lowest unused index number
					for(size_t i = 0; i < new_elements_ocn.size(); i++, new_list_cur_index++)
					{
						//look for first index not used
						std::string index_string = EvaluableNode::NumberToString(new_list_cur_index, true);
						EvaluableNode **found = new_list->GetMappedChildNode(index_string);
						if(found != nullptr)
						{
							i--;	//try this again with the next index
							continue;
						}
						new_list->SetMappedChildNode(index_string, new_elements_ocn[i]);
					}
				}
			}

			//don't need the top node anymore
			evaluableNodeManager->FreeNodeIfPossible(new_elements);
		}
		else //not a map or list, just append the element singularly
		{
			new_list.UpdatePropertiesBasedOnAttachedNode(new_elements, first_append);
			if(new_list->GetType() == ENT_LIST)
			{
				new_list->AppendOrderedChildNode(new_elements);
			}
			else
			{
				//find the next unused index
				std::string index_string;
				do
				{
					index_string = EvaluableNode::NumberToString(new_list_cur_index++, true);
				} while(new_list->GetMappedChildNode(index_string) != nullptr);

				new_list->SetMappedChildNode(index_string, new_elements);
			}
		}

		first_append = false;
	} //for each child node to append

	return new_list;
}

static OpcodeInitializer _ENT_SIZE(ENT_SIZE, &Interpreter::InterpretNode_ENT_SIZE, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc|string collection] collection)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the size of the `collection` in number of elements.  If `collection` is a string, returns the length in UTF-8 characters.)";
	d.examples = MakeAmalgamExamples({
		{R"&((size
	[4 9.2 "this"]
))&", R"(3)"},
			{R"&((size
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
))&", R"(4)"},
			{R"&((size "hello"))&", R"(5)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 43.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SIZE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0], true);

	double size = 0;
	if(n.IsImmediateValue())
	{
		auto &value = n.GetValue();

		if(value.nodeType == ENIVT_BOOL)
			size = (value.nodeValue.boolValue ? 1 : 0);
		else if(value.nodeType == ENIVT_NUMBER)
			size = value.nodeValue.number;
		else if(value.nodeType == ENIVT_STRING_ID)
			size = static_cast<double>(StringManipulation::GetNumUTF8Characters(value.nodeValue.stringID->string));
		else if(value.nodeType == ENIVT_CODE && value.nodeValue.code != nullptr)
			size = static_cast<double>(value.nodeValue.code->GetNumChildNodes());

		evaluableNodeManager->FreeNodeIfPossible(n);
		return AllocReturn(size, immediate_result);
	}
	else if(n != nullptr)
	{
		if(n->GetType() == ENT_STRING)
		{
			auto &s = n->GetStringValue();
			size = static_cast<double>(StringManipulation::GetNumUTF8Characters(s));
		}
		else
		{
			size = static_cast<double>(n->GetNumChildNodes());
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(n);
	return AllocReturn(size, immediate_result);
}

static OpcodeInitializer _ENT_GET(ENT_GET, &Interpreter::InterpretNode_ENT_GET, []() {
	OpcodeDetails d;
	d.parameters = R"(* data [number|index|list walk_path_1] [number|string|list walk_path_2] ...)";
	d.returns = R"(any)";
	d.description = R"(Evaluates to `data` as traversed by the set of values specified by `walk_path_1', which can be any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values.  If multiple walk paths are specified, then `get` returns a list, where each element in the list is the respective element retrieved by the respective walk path.  If the walk path continues past the data structure, it will return a null.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get
	[4 9.2 "this"]
))&", R"([4 9.2 "this"])"},
			{R"&((get
	[4 9.2 "this"]
	1
))&", R"(9.2)"},
			{R"&((get
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"c"
))&", R"(3)"},
			{R"&((get
	[
		0
		1
		2
		3
		[
			0
			1
			2
			(associate "a" 1)
		]
	]
	[4 3 "a"]
))&", R"(1)"},
			{R"&((get
	[4 9.2 "this"]
	1
	2
))&", R"([9.2 "this"])"},
			{R"&((seq
	(declare
		{
			var {
					A (associate "B" 2)
					B 2
				}
		}
	)
	[
		(get
			var
			["A" "B"]
		)
		(get
			var
			["A" "C"]
		)
		(get
			var
			["B" "C"]
		)
	]
))&", R"([2 (null) (null)])"},
			{R"&((get
	{(null) 3}
	(null)
))&", R"(3)"},
			{R"&((let
	{
		complex_assoc {
				4 "number"
				[4] "list"
				{4 4} "assoc"
				"4" "string"
			}
	}
	[
		(get complex_assoc 4)
		(get complex_assoc "4")
		(get
			complex_assoc
			[
				[4]
			]
		)
		(get
			complex_assoc
			{4 4}
		)
		(sort (indices complex_assoc))
	]
))&", R"([
	"number"
	"string"
	"list"
	"assoc"
	[
		4
		[4]
		{4 4}
		"4"
	]
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 138.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t ocn_size = ocn.size();

	if(ocn_size < 1)
		return EvaluableNodeReference::Null();

	auto source = InterpretNodeForImmediateUse(ocn[0]);
	if(ocn_size < 2 || source == nullptr)
		return source;

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//if just a single index passed to get
	if(ocn_size == 2)
	{
		EvaluableNode **target = InterpretNodeIntoDestination(&source.GetReference(), ocn[1], false);

		node_stack.PopEvaluableNode();

		if(target == nullptr)
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(source);
			return EvaluableNodeReference::Null();
		}

		return EvaluableNodeReference(*target, source.unique);	//only know about the target that it has similar properties to the source
	}

	//else, return a list for everything retrieved via get
	EvaluableNodeReference retrieved_list(evaluableNodeManager->AllocNode(ENT_LIST), source.unique);
	retrieved_list->ReserveOrderedChildNodes(ocn_size - 1);
	node_stack.PushEvaluableNode(retrieved_list);

	for(size_t param_index = 1; param_index < ocn_size; param_index++)
	{
		EvaluableNode **target = InterpretNodeIntoDestination(&source.GetReference(), ocn[param_index], false);
		if(target != nullptr)
			retrieved_list->AppendOrderedChildNode(*target);
		else
			retrieved_list->AppendOrderedChildNode(nullptr);
	}

	//if one or fewer child nodes, the append function will have set the appropriate cycle check flag,
	// but if two or more nodes, then there could be duplicate nodes
	if(retrieved_list->GetNumChildNodes() > 1)
		retrieved_list->SetNeedCycleCheck(true);

	return retrieved_list;
}

static OpcodeInitializer _ENT_SET(ENT_SET, &Interpreter::InterpretNode_ENT_SET_and_REPLACE, []() {
	OpcodeDetails d;
	d.parameters = R"(* data [number|string|list walk_path1] [* new_value1] [number|string|list walk_path2] [* new_value2] ... [number|string|list walk_pathN] [* new_valueN])";
	d.returns = R"(any)";
	d.description = R"(Performs a deep copy on `data` (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values as a walk path of indices. `new_value1` to `new_valueN` represent a value that will be used to replace  whatever is in the location the preceding location parameter specifies.  If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc); however, it will not change the type of immediate values to an assoc or list. Note that `(target)` will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.)";
	d.examples = MakeAmalgamExamples({
		{R"&((set
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"e"
	5
))&", R"({
	4 "d"
	a 1
	b 2
	c 3
	e 5
})"},
			{R"&((set
	[0 1 2 3 4]
	2
	10
))&", R"([0 1 10 3 4])"},
			{R"&((set
	(associate "a" 1 "b" 2)
	"a"
	3
))&", R"({a 3 b 2})"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 3.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_REPLACE(ENT_REPLACE, &Interpreter::InterpretNode_ENT_SET_and_REPLACE, []() {
	OpcodeDetails d;
	d.parameters = R"(* data [number|string|list walk_path1] [* function1] [number|string|list walk_path2] [* function2] ... [number|string|list walk_pathN] [* functionN])";
	d.returns = R"(any)";
	d.description = R"(Performs a deep copy on `data` (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values. `function1` to `functionN` represent a function that will be used to replace in place of whatever is in the location of the corresponding walk_path, and will be passed the current node in (current_value).  The function can optionally be just be an immediate value or any code that can be evaluated.  If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc). Note that the `(target)` will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.)";
	d.examples = MakeAmalgamExamples({
		{R"&((replace
	[
		(associate "a" 13)
	]
))&", R"([
	{a 13}
])"},
			{R"&((replace
	[
		(associate "a" 1)
	]
	[2]
	1
	[0]
	[4 5 6]
))&", R"([
	[4 5 6]
	(null)
	1
])"},
			{R"&((replace
	[
		(associate "a" 1)
	]
	2
	1
	0
	[4 5 6]
))&", R"([
	[4 5 6]
	(null)
	1
])"},
			{R"&((replace
	[
		(associate "a" 1)
	]
	[0]
	(lambda
		(set (current_value) "b" 2)
	)
))&", R"([
	{a 1 b 2}
])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 5.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_and_REPLACE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto result = InterpretNode(ocn[0]);

	if(result == nullptr)
		result.SetReference(evaluableNodeManager->AllocNode(ENT_NULL), true);

	if(!result.unique)
		result = evaluableNodeManager->DeepAllocCopy(result);

	auto node_stack = CreateOpcodeStackStateSaver(result);

	bool result_flags_need_updates = false;

	//get each address/value pair to replace in result
	for(size_t replace_change_index = 1; replace_change_index + 1 < ocn.size(); replace_change_index += 2)
	{
		//find replacement location, make sure it's a valid target
		EvaluableNode *previous_result = result;
		EvaluableNode **copy_destination = InterpretNodeIntoDestination(&result.GetReference(), ocn[replace_change_index], true);
		//if the target changed, keep track of the proper reference
		if(result != previous_result)
		{
			node_stack.PopEvaluableNode();
			node_stack.PushEvaluableNode(result);
		}
		if(copy_destination == nullptr)
			continue;

		////////////////////
		//compute new value

		if(en->GetType() == ENT_SET)
		{
			//just in case copy_destination points to result
			auto new_value = InterpretNode(ocn[replace_change_index + 1]);

			if(*copy_destination != result) //normal replacement
			{
				if(result.unique && !result.GetNeedCycleCheck())
					evaluableNodeManager->FreeNodeTree(*copy_destination);
				*copy_destination = new_value;
			}
			else //replace the whole thing from the top
			{
				node_stack.PopEvaluableNode();
				*copy_destination = new_value;
				node_stack.PushEvaluableNode(result);
			}

			if(result.NeedAllFlagsRecheckedAfterNodeAttachedAndUpdateUniqueness(new_value))
				result_flags_need_updates = true;
		}
		else //en->GetType() == ENT_REPLACE
		{
			//replace copy_destination (a part of result) with the new value
			auto function = InterpretNodeForImmediateUse(ocn[replace_change_index + 1]);
			if(EvaluableNode::IsNull(function))
			{
				(*copy_destination) = nullptr;
				continue;
			}

			node_stack.PushEvaluableNode(function);
			PushNewConstructionContext(nullptr, result, EvaluableNodeImmediateValueWithType(), *copy_destination);

			EvaluableNodeReference new_value = InterpretNodeForImmediateUse(function);

			if(PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				result.unique = false;
				result.uniqueUnreferencedTopNode = false;
			}

			node_stack.PopEvaluableNode();

			if(*copy_destination != result) //normal replacement
			{
				(*copy_destination) = new_value;
			}
			else //replacing root, need to manage references to not leave stray memory
			{
				node_stack.PopEvaluableNode();
				result = new_value;
				node_stack.PushEvaluableNode(result);
			}

			//need to update flags because of execution happening between all
			if(result.NeedAllFlagsRecheckedAfterNodeAttachedAndUpdateUniqueness(new_value))
				EvaluableNodeManager::UpdateFlagsForNodeTree(result);
		}
	}

	if(result_flags_need_updates)
		EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	return result;
}

static OpcodeInitializer _ENT_INDICES(ENT_INDICES, &Interpreter::InterpretNode_ENT_INDICES, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc collection)";
	d.returns = R"(list of string|number)";
	d.description = R"(Evaluates to the list of strings or numbers that comprise the indices for the list or associative parameter `collection`.  It is guaranteed that the opcodes indices and values will evaluate and return elements in the same order when given the same node.)";
	d.examples = MakeAmalgamExamples({
		{R"&((sort
	(indices
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
	)
))&", R"([4 "a" "b" "c"])"},
			{R"&((indices
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
))&", R"([
	0
	1
	2
	3
	4
	5
	6
	7
])"},
			{R"&((indices
	(range 0 3)
))&", R"([0 1 2 3])"},
			{R"&((sort
	(indices
		(zip
			(range 0 3)
		)
	)
))&", R"([0 1 2 3])"},
			{R"&((sort
	(indices
		(zip
			[0 1 2 3]
		)
	)
))&", R"([0 1 2 3])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 12.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_INDICES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//get assoc array to look up
	auto container = InterpretNodeForImmediateUse(ocn[0]);
	EvaluableNodeReference index_list(evaluableNodeManager->AllocNode(ENT_LIST), true);

	if(container == nullptr)
		return index_list;

	auto &index_list_ocn = index_list->GetOrderedChildNodesReference();

	if(container->IsAssociativeArray())
	{
		auto &container_mcn = container->GetMappedChildNodesReference();
		index_list_ocn.reserve(container_mcn.size());
		for(auto &[node_id, _] : container_mcn)
		{
			EvaluableNodeReference key_node = Parser::ParseFromKeyStringId(node_id, evaluableNodeManager);
			index_list_ocn.push_back(key_node);
			index_list.UpdatePropertiesBasedOnAttachedNode(key_node);
		}
	}
	else if(container->IsOrderedArray())
	{
		size_t num_ordered_nodes = container->GetOrderedChildNodesReference().size();
		index_list_ocn.resize(num_ordered_nodes);
		for(size_t i = 0; i < num_ordered_nodes; i++)
			index_list_ocn[i] = evaluableNodeManager->AllocNode(static_cast<double>(i));
	}

	//none of the original container is needed
	evaluableNodeManager->FreeNodeTreeIfPossible(container);

	return index_list;
}

static OpcodeInitializer _ENT_VALUES(ENT_VALUES, &Interpreter::InterpretNode_ENT_VALUES, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc collection [bool only_unique_values])";
	d.returns = R"(list of any)";
	d.description = R"(Evaluates to the list of entities that comprise the values for the list or associative list `collection`.  If `only_unique_values` is true (defaults to false), then it will filter out any duplicate values and only return those that are unique, preserving their order of first appearance.  If `only_unique_values` is not true, then it is guaranteed that the opcodes indices and values will evaluate and return elements in the same order when given the same node.)";
	d.examples = MakeAmalgamExamples({
		{R"&((sort
	(values
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
	)
))&", R"([1 2 3 "d"])"},
			{R"&((values
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
))&", R"([
	"a"
	1
	"b"
	2
	"c"
	3
	4
	"d"
])"},
			{R"&((values
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
		1
		2
		3
		4
		"a"
		"b"
		"c"
	]
	.true
))&", R"([
	"a"
	1
	"b"
	2
	"c"
	3
	4
	"d"
])"},
			{R"&((sort
	(values
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
			"e"
			1
		)
		.true
	)
))&", R"([1 2 3 "d"])"},
			{R"&((values
	(append
		(range 1 20)
		(range 1 20)
	)
	.true
))&", R"([
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
	15
	16
	17
	18
	19
	20
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 9.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_VALUES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	bool only_unique_values = false;
	if(ocn.size() >= 2)
		only_unique_values = InterpretNodeIntoBoolValue(ocn[1]);

	auto container = InterpretNode(ocn[0]);

	//exit early if wrong type
	if(container == nullptr || container->IsImmediate())
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(container);
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);
	}

	if(!only_unique_values)
	{
		if(container->IsOrderedArray())
		{
			//if simple result, just return immediately
			if(container->GetType() == ENT_LIST && !container->HasMetadata())
				return container;

			if(container.uniqueUnreferencedTopNode && !container.GetNeedCycleCheck())
			{
				container->ClearMetadata();
				container->SetType(ENT_LIST, evaluableNodeManager, false);
				return container;
			}

			auto *result = evaluableNodeManager->AllocNode(ENT_LIST);
			auto &container_ocn = container->GetOrderedChildNodesReference();
			result->AppendOrderedChildNodes(container_ocn);

			if(container->GetNeedCycleCheck())
				result->SetNeedCycleCheck(true);

			return EvaluableNodeReference(result, false, true);
		}
		else //container->IsAssociativeArray()
		{
			if(container.uniqueUnreferencedTopNode && !container.GetNeedCycleCheck())
			{
				container->ClearMetadata();
				container->ConvertAssocToList();
				return container;
			}

			EvaluableNode *result = evaluableNodeManager->AllocNode(ENT_LIST);

			for(auto &[_, cn] : container->GetMappedChildNodesReference())
				result->AppendOrderedChildNode(cn);

			if(container->GetNeedCycleCheck())
				result->SetNeedCycleCheck(true);

			evaluableNodeManager->FreeNodeIfPossible(container);

			return EvaluableNodeReference(result, container.unique, true);
		}
	}
	else //only_unique_values
	{
		EvaluableNode *result = evaluableNodeManager->AllocNode(ENT_LIST);

		//if noncyclic data, simple container, and sufficiently few nodes for an n^2 comparison
		// just do the lower overhead check with more comparisons
		constexpr int max_num_for_n2_comparison = 10;
		if(!container->GetNeedCycleCheck()
			&& !container->IsAssociativeArray()
			&& container->GetNumChildNodes() < max_num_for_n2_comparison)
		{
			//use {}'s to initialize to false
			std::array<bool, max_num_for_n2_comparison> should_free{};

			auto &container_ocn = container->GetOrderedChildNodesReference();
			for(size_t i = 0; i < container_ocn.size(); i++)
			{
				//check everything prior
				bool value_exists = false;
				for(size_t j = 0; j < i; j++)
				{
					if(EvaluableNode::AreDeepEqual(container_ocn[i], container_ocn[j]))
					{
						value_exists = true;
						break;
					}
				}

				if(!value_exists)
					result->AppendOrderedChildNode(container_ocn[i]);
				else if(container.unique)
					should_free[i] = true;
			}

			//free any that are no longer needed
			for(size_t i = 0; i < container_ocn.size(); i++)
			{
				if(should_free[i])
				{
					EvaluableNodeReference enr(container_ocn[i], true);
					evaluableNodeManager->FreeNodeTree(enr);
				}
			}
		}
		else //use a hash-set and look up stringified values for collisions
		{
			//attempt to emplace/insert the unparsed node into values_in_existence, and if successful, append the value
			FastHashSet<std::string> values_in_existence;

			bool free_unused_nodes = (container.unique && !container.GetNeedCycleCheck());

			if(container->IsOrderedArray())
			{
				for(auto &n : container->GetOrderedChildNodesReference())
				{
					std::string str_value = Parser::UnparseToKeyString(n);
					if(values_in_existence.emplace(str_value).second)
					{
						result->AppendOrderedChildNode(n);
					}
					else if(free_unused_nodes)
					{
						EvaluableNodeReference enr(n, true);
						evaluableNodeManager->FreeNodeTree(enr);
					}
				}
			}
			else //container->IsAssociativeArray()
			{
				for(auto &[_, cn] : container->GetMappedChildNodesReference())
				{
					std::string str_value = Parser::UnparseToKeyString(cn);
					if(values_in_existence.emplace(str_value).second)
					{
						result->AppendOrderedChildNode(cn);
					}
					else if(free_unused_nodes)
					{
						EvaluableNodeReference enr(cn, true);
						evaluableNodeManager->FreeNodeTree(enr);
					}
				}
			}
		}

		//shouldn't have duplicated values, so don't need a cycle check on the top node

		evaluableNodeManager->FreeNodeIfPossible(container);
		return EvaluableNodeReference(result, container.unique, true);
	}
}

static OpcodeInitializer _ENT_CONTAINS_INDEX(ENT_CONTAINS_INDEX, &Interpreter::InterpretNode_ENT_CONTAINS_INDEX, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc collection string|number|list index)";
	d.returns = R"(bool)";
	d.description = R"(Evaluates to true if the index is in the `collection`.  If index is a string, it will attempt to look at `collection` as an assoc, if number, it will look at `collection` as a list.  If index is a list, it will traverse a via the elements in the list as a walk path, with each element .)";
	d.examples = MakeAmalgamExamples({
		{R"&((contains_index
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"c"
))&", R"(.true)"},
			{R"&((contains_index
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"m"
))&", R"(.false)"},
			{R"&((contains_index
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	2
))&", R"(.true)"},
			{R"&((contains_index
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	100
))&", R"(.false)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 20.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_INDEX(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//get assoc array to look up
	auto container = InterpretNodeForImmediateUse(ocn[0]);
	if(container == nullptr)
		return AllocReturn(false, immediate_result);

	auto node_stack = CreateOpcodeStackStateSaver(container);

	//get index to look up (will attempt to reuse this node below)
	auto index = InterpretNodeForImmediateUse(ocn[1]);

	EvaluableNode **target = TraverseToDestinationFromTraversalPathList(&container.GetReference(), index, false);
	bool found = (target != nullptr);

	evaluableNodeManager->FreeNodeTreeIfPossible(index);
	evaluableNodeManager->FreeNodeTreeIfPossible(container);
	return AllocReturn(found, immediate_result);
}

static OpcodeInitializer _ENT_CONTAINS_VALUE(ENT_CONTAINS_VALUE, &Interpreter::InterpretNode_ENT_CONTAINS_VALUE, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc|string collection_or_string string|number value)";
	d.returns = R"(bool)";
	d.description = R"(Evaluates to true if the `value` is contained in `collection_or_string`.  If `collection_or_string` is a string, then it uses `value` as a regular expression and evaluates to true if the regular expression matches.)";
	d.examples = MakeAmalgamExamples({
		{R"&((contains_value
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	1
))&", R"(.true)"},
			{R"&((contains_value
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	44
))&", R"(.false)"},
			{R"&((contains_value
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	"d"
))&", R"(.true)"},
			{R"&((contains_value
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	100
))&", R"(.false)"},
			{R"&((contains_value "hello world" ".*world"))&", R"(.true)"},
			{R"&((contains_value "abcdefg" "a.*g"))&", R"(.true)"},
			{R"&((contains_value "3.141" "[0-9]+\\.[0-9]+"))&", R"(.true)"},
			{R"&((contains_value "3.141" "\\d+\\.\\d+"))&", R"(.true)"},
			{R"&((contains_value "3.a141" "\\d+\\.\\d+"))&", R"(.false)"},
			{R"&((contains_value "abc\r\n123" "(.|\r)*\n.*"))&", R"(.true)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 77.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_VALUE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto container = InterpretNodeForImmediateUse(ocn[0]);

	if(container == nullptr)
		return AllocReturn(false, immediate_result);

	auto node_stack = CreateOpcodeStackStateSaver(container);

	//get value to look up (will attempt to reuse this node below)
	auto value = InterpretNodeForImmediateUse(ocn[1]);

	bool found = false;

	//try to find value
	if(container->IsAssociativeArray())
	{
		for(auto &[_, cn] : container->GetMappedChildNodesReference())
		{
			if(EvaluableNode::AreDeepEqual(cn, value))
			{
				found = true;
				break;
			}
		}
	}
	else if(container->IsOrderedArray())
	{
		for(auto &cn : container->GetOrderedChildNodesReference())
		{
			if(EvaluableNode::AreDeepEqual(cn, value))
			{
				found = true;
				break;
			}
		}
	}
	else if(container->GetType() == ENT_STRING && !EvaluableNode::IsNull(value))
	{
		//compute regular expression
		auto &s = container->GetStringValue();

		std::string value_as_str = EvaluableNode::ToString(value);

		//use nosubs to prevent unnecessary memory allocations since this is just matching
		std::regex rx;
		bool valid_rx = true;
		try
		{
			rx.assign(value_as_str, std::regex::ECMAScript | std::regex::nosubs);
		}
		catch(...)
		{
			valid_rx = false;
		}

		if(valid_rx && std::regex_match(s, rx))
			found = true;
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(value);
	evaluableNodeManager->FreeNodeTreeIfPossible(container);
	return AllocReturn(found, immediate_result);
}

static OpcodeInitializer _ENT_REMOVE(ENT_REMOVE, &Interpreter::InterpretNode_ENT_REMOVE, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc collection number|string|list index)";
	d.returns = R"(list|assoc)";
	d.description = R"(Removes the index-value pair with `index` being the index in assoc or index of `collection`, returning a new list or assoc with `index` removed.  If `index` is a list of numbers or strings, then it will remove each of the requested indices.  Negative numbered indices will count back from the end of a list.)";
	d.examples = MakeAmalgamExamples({
		{R"&((sort
	(remove
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
		4
	)
))&", R"([1 2 3])"},
			{R"&((remove
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	4
))&", R"([
	"a"
	1
	"b"
	2
	3
	4
	"d"
])"},
			{R"&((sort
	(remove
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
		[4 "a"]
	)
))&", R"([2 3])"},
			{R"&((remove
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	[4]
))&", R"([
	"a"
	1
	"b"
	2
	3
	4
	"d"
])"},
			{R"&((remove
	[0 1 2 3 4 5]
	[0 2]
))&", R"([1 3 4 5])"},
			{R"&((remove
	[0 1 2 3 4 5]
	-1
))&", R"([0 1 2 3 4])"},
			{R"&((remove
	[0 1 2 3 4 5]
	[0 -1]
))&", R"([1 2 3 4])"},
			{R"&((remove
	[0 1 2 3 4 5]
	[
		5
		0
		1
		2
		3
		4
		5
		6
	]
))&", R"([])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 6.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_REMOVE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto container = InterpretNode(ocn[0]);
	if(container == nullptr)
		return EvaluableNodeReference::Null();

	evaluableNodeManager->EnsureNodeIsModifiable(container, true);

	auto node_stack = CreateOpcodeStackStateSaver(container);

	//get indices (or index) to remove
	auto indices = InterpretNodeForImmediateUse(ocn[1], true);

	//used for deleting nodes if possible -- unique and cycle free
	EvaluableNodeReference removed_node = EvaluableNodeReference(static_cast<EvaluableNode *>(nullptr),
		container.unique && !container->GetNeedCycleCheck());

	//if not a list, then just remove individual element
	if(indices.IsImmediateValueType())
	{
		if(container->IsAssociativeArray())
		{
			StringInternPool::StringID key_sid = indices.GetValue().GetValueAsStringIDIfExists(true);
			removed_node.SetReference(container->EraseMappedChildNode(key_sid));
		}
		else if(container->IsOrderedArray())
		{
			double relative_pos = indices.GetValue().GetValueAsNumber();
			auto &container_ocn = container->GetOrderedChildNodesReference();

			//get relative position
			size_t actual_pos = 0;
			if(relative_pos >= 0)
				actual_pos = static_cast<size_t>(relative_pos);
			else
				actual_pos = static_cast<size_t>(container_ocn.size() + relative_pos);

			//if the position is valid, erase it
			if(actual_pos >= 0 && actual_pos < container_ocn.size())
			{
				removed_node.SetReference(container_ocn[actual_pos]);
				container_ocn.erase(begin(container_ocn) + actual_pos);
			}
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(removed_node);
	}
	else //remove all of the child nodes of the index
	{
		auto &indices_ocn = indices->GetOrderedChildNodes();

		if(container->IsAssociativeArray())
		{
			for(auto &cn : indices_ocn)
			{
				StringInternPool::StringID key_sid = EvaluableNode::ToStringIDIfExists(cn, true);
				removed_node.SetReference(container->EraseMappedChildNode(key_sid));
				evaluableNodeManager->FreeNodeTreeIfPossible(removed_node);
			}
		}
		else if(container->IsOrderedArray())
		{
			auto &container_ocn = container->GetOrderedChildNodesReference();

			//get valid indices to erase
			std::vector<size_t> indices_to_erase;
			indices_to_erase.reserve(indices_ocn.size());
			for(auto &cn : indices_ocn)
			{
				double relative_pos = EvaluableNode::ToNumber(cn);

				//get relative position
				size_t actual_pos = 0;
				if(relative_pos >= 0)
					actual_pos = static_cast<size_t>(relative_pos);
				else
					actual_pos = static_cast<size_t>(container_ocn.size() + relative_pos);

				//if the position is valid, mark it to be erased
				if(actual_pos >= 0 && actual_pos < container_ocn.size())
					indices_to_erase.push_back(actual_pos);
			}

			//sort reversed so the indices can be removed consistently and efficiently
			std::sort(begin(indices_to_erase), end(indices_to_erase), std::greater<>());

			//remove indices in revers order and free if possible
			for(size_t index : indices_to_erase)
			{
				//if there were any duplicate indices, skip them
				if(index >= container_ocn.size())
					continue;

				removed_node.SetReference(container_ocn[index]);
				container_ocn.erase(begin(container_ocn) + index);
				evaluableNodeManager->FreeNodeTreeIfPossible(removed_node);
			}
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(indices);

	return container;
}

static OpcodeInitializer _ENT_KEEP(ENT_KEEP, &Interpreter::InterpretNode_ENT_KEEP, []() {
	OpcodeDetails d;
	d.parameters = R"(list|assoc collection number|string|list index)";
	d.returns = R"(list|assoc)";
	d.description = R"(Keeps only the index-value pair with index being the index in `collection`, returning a new list or assoc with only that index.  If `index` is a list of numbers or strings, then it will only keep those requested indices.  Negative numbered indices will count back from the end of a list.)";
	d.examples = MakeAmalgamExamples({
		{R"&((keep
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	4
))&", R"({4 "d"})"},
			{R"&((keep
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	4
))&", R"(["c"])"},
			{R"&((sort
	(keep
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
		[4 "a"]
	)
))&", R"([1 "d"])"},
			{R"&((keep
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	[4 "a"]
))&", R"(["c"])"},
			{R"&((keep
	[0 1 2 3 4 5]
	[0 2]
))&", R"([0 2])"},
			{R"&((keep
	[0 1 2 3 4 5]
	-1
))&", R"([5])"},
			{R"&((keep
	[0 1 2 3 4 5]
	[0 -1]
))&", R"([0 5])"},
			{R"&((keep
	[0 1 2 3 4 5]
	[
		5
		0
		1
		2
		3
		4
		5
		6
	]
))&", R"([0 1 2 3 4 5])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 2.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_KEEP(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto container = InterpretNode(ocn[0]);
	if(container == nullptr)
		return EvaluableNodeReference::Null();

	evaluableNodeManager->EnsureNodeIsModifiable(container, true);

	auto node_stack = CreateOpcodeStackStateSaver(container);

	//get indices (or index) to keep
	auto indices = InterpretNodeForImmediateUse(ocn[1], true);

	//if immediate then just keep individual element
	if(indices.IsImmediateValueType())
	{
		if(container->IsAssociativeArray())
		{
			StringInternPool::StringID key_sid = indices.GetValue().GetValueAsStringIDWithReference(true);
			auto &container_mcn = container->GetMappedChildNodesReference();

			//find what should be kept, or clear key_sid if not found
			EvaluableNode *to_keep = nullptr;
			auto found_to_keep = container_mcn.find(key_sid);
			if(found_to_keep != end(container_mcn))
				to_keep = found_to_keep->second;
			else
			{
				string_intern_pool.DestroyStringReference(key_sid);
				key_sid = string_intern_pool.NOT_A_STRING_ID;
			}

			//free everything not kept if possible
			if(container.unique && !container->GetNeedCycleCheck())
			{
				for(auto &[cn_id, cn] : container_mcn)
				{
					if(cn_id != key_sid)
						evaluableNodeManager->FreeNodeTree(cn);
				}
			}

			//put to_keep back in (have the string reference from above)
			container->ClearMappedChildNodes();
			if(key_sid != string_intern_pool.NOT_A_STRING_ID)
				container_mcn.emplace(key_sid, to_keep);
		}
		else if(container->IsOrderedArray())
		{
			double relative_pos = indices.GetValue().GetValueAsNumber();
			auto &container_ocn = container->GetOrderedChildNodesReference();

			//get relative position
			size_t actual_pos = 0;
			if(relative_pos >= 0)
				actual_pos = static_cast<size_t>(relative_pos);
			else
				actual_pos = static_cast<size_t>(container_ocn.size() + relative_pos);

			//if the position is valid, erase everything but that position
			if(actual_pos >= 0 && actual_pos < container_ocn.size())
			{

				//free everything not kept if possible
				if(container.unique && !container->GetNeedCycleCheck())
				{
					for(size_t i = 0; i < container_ocn.size(); i++)
					{
						if(i != actual_pos)
							evaluableNodeManager->FreeNodeTree(container_ocn[i]);
					}
				}

				EvaluableNode *to_keep = container_ocn[actual_pos];
				container_ocn.clear();
				container_ocn.push_back(to_keep);
			}
		}
	}
	else //not immediate, keep all of the child nodes of the index
	{
		auto &indices_ocn = indices->GetOrderedChildNodes();
		if(container->IsAssociativeArray())
		{
			auto &container_mcn = container->GetMappedChildNodesReference();
			EvaluableNode::AssocType new_container;

			for(auto &cn : indices_ocn)
			{
				StringInternPool::StringID key_sid = EvaluableNode::ToStringIDIfExists(cn, true);

				//if found, move it over to the new container
				auto found_to_keep = container_mcn.find(key_sid);
				if(found_to_keep != end(container_mcn))
				{
					new_container.emplace(found_to_keep->first, found_to_keep->second);
					container_mcn.erase(found_to_keep);
				}
			}

			//anything left should be freed if possible
			if(container.unique && !container->GetNeedCycleCheck())
			{
				for(auto &[_, cn] : container_mcn)
					evaluableNodeManager->FreeNodeTree(cn);
			}
			string_intern_pool.DestroyStringReferences(container_mcn, [](auto &pair) { return pair.first;  });

			//put in place
			std::swap(container_mcn, new_container);
		}
		else if(container->IsOrderedArray())
		{
			auto &container_ocn = container->GetOrderedChildNodesReference();

			//get valid indices to keep
			std::vector<size_t> indices_to_keep;
			indices_to_keep.reserve(indices_ocn.size());
			for(auto &cn : indices_ocn)
			{
				double relative_pos = EvaluableNode::ToNumber(cn);
				if(FastIsNaN(relative_pos))
					continue;

				//get relative position
				size_t actual_pos = 0;
				if(relative_pos >= 0)
					actual_pos = static_cast<size_t>(relative_pos);
				else
					actual_pos = static_cast<size_t>(container_ocn.size() + relative_pos);

				//if the position is valid, mark it to be erased
				if(actual_pos >= 0 && actual_pos < container_ocn.size())
					indices_to_keep.push_back(actual_pos);
			}

			//sort to keep in order and remove duplicates
			std::sort(begin(indices_to_keep), end(indices_to_keep));

			std::vector<EvaluableNode *> new_container;
			new_container.reserve(indices_to_keep.size());

			//move indices over, but keep track of the previous one to skip duplicates
			size_t prev_index = std::numeric_limits<size_t>::max();
			for(size_t i = 0; i < indices_to_keep.size(); i++)
			{
				size_t index = indices_to_keep[i];

				if(index == prev_index)
					continue;

				new_container.push_back(container_ocn[index]);

				//set to null so it won't be cleared later
				container_ocn[index] = nullptr;

				prev_index = index;
			}

			//free anything left in original container
			if(container.unique && !container->GetNeedCycleCheck())
			{
				for(auto cn : container_ocn)
					evaluableNodeManager->FreeNodeTree(cn);
			}

			//put in place
			std::swap(container_ocn, new_container);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(indices);

	return container;
}
