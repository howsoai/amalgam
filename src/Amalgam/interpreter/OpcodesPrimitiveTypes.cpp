//project headers:
#include "Interpreter.h"
#include "InterpreterConcurrencyManager.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Primitive Types";

static OpcodeInitializer _ENT_NULL(ENT_NULL, &Interpreter::InterpretNode_ENT_NULL, []() {
	OpcodeDetails d;
	d.parameters = R"()";
	d.returns = R"(null)";
	d.description = R"(Evaluates to the immediate null value, regardless of any parameters.)";
	d.examples = MakeAmalgamExamples({
		{R"&((null))&", R"((null))"},
		{R"&((lambda
	(null
		(+ 3 5)
		7
	)
))&", R"((null
	(+ 3 5)
	7
))"},
			{R"&((lambda
	
	#annotation
	(null)
))&", R"(#annotation
(null))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 81.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_NULL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_BOOL(ENT_BOOL, &Interpreter::InterpretNode_ENT_BOOL, []() {
	OpcodeDetails d;
	d.parameters = R"()";
	d.returns = R"(bool)";
	d.description = R"(A 64-bit floating point value)";
	d.examples = MakeAmalgamExamples({
		{R"&(.true)&", R"(.true)"},
		{R"&(.false)&", R"(.false)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 73.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_BOOL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	bool value = en->GetBoolValueReference();
	return AllocReturn(value, immediate_result);
}

static OpcodeInitializer _ENT_NUMBER(ENT_NUMBER, &Interpreter::InterpretNode_ENT_NUMBER, []() {
	OpcodeDetails d;
	d.parameters = R"()";
	d.returns = R"(number)";
	d.description = R"(A 64-bit floating point value)";
	d.examples = MakeAmalgamExamples({
		{R"&(1)&", R"(1)"},
		{R"&(1.5)&", R"(1.5)"},
		{R"&(6.02214076e+23)&", R"(6.02214076e+23)"},
		{R"&(.infinity)&", R"(.infinity)"},
		{R"&((-
	(* 3 .infinity)
))&", R"(-.infinity)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 2545.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_NUMBER(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	double value = en->GetNumberValueReference();
	return AllocReturn(value, immediate_result);
}

static OpcodeInitializer _ENT_STRING(ENT_STRING, &Interpreter::InterpretNode_ENT_STRING, []() {
	OpcodeDetails d;
	d.parameters = R"()";
	d.returns = R"(string)";
	d.description = R"(A string.  Many opcodes assume UTF-8 formatted strings, but many, such as `format`, can work with any bytes.  Any non double-quote character is considered valid.)";
	d.examples = MakeAmalgamExamples({
		{R"&("hello")&", R"("hello")"},
		{R"&("\tHello\n\"Hello\"")&", R"("\tHello\n\"Hello\"")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 766.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_STRING(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	StringInternPool::StringID value = en->GetStringIDReference();
	return AllocReturn(value, immediate_result);
}


static OpcodeInitializer _ENT_LIST(ENT_LIST, &Interpreter::InterpretNode_ENT_LIST_and_UNORDERED_LIST, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(list)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to a list with the parameters as elements.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the list itself, the current index, and the current value.  If `[]`'s are used instead of parenthesis, the keyword `list` may be omitted.  `[]` are considered identical to `(list)`.)";
	d.examples = MakeAmalgamExamples({
		{R"&(["a" 1 "b"])&", R"(["a" 1 "b"])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 484.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_UNORDERED_LIST(ENT_UNORDERED_LIST, &Interpreter::InterpretNode_ENT_LIST_and_UNORDERED_LIST, []() {
	OpcodeDetails d;
	d.parameters = R"([* node1] [* node2] ... [* nodeN])";
	d.returns = R"(unordered_list)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to the list specified by parameters as elements.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the unordered list itself, the current index, and the current value.  It operates like a list, except any operations that would normally consider a list's order.  For example, union, intersect, and mix, will consider the values unordered.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unordered_list 4 4 5))&", R"((unordered_list 4 4 5))"},
		{R"&((unordered_list
	(unordered_list 4 4 5)
	(unordered_list 4 5 6)
))&", R"((unordered_list
	(unordered_list 4 4 5)
	(unordered_list 4 5 6)
))"},
			{R"&((=
	(unordered_list 4 4 5)
	(unordered_list 4 4 5)
))&", R"(.true)"},
			{R"&((=
	(unordered_list 4 4 5)
	(unordered_list 4 4 6)
))&", R"(.false)"},
			{R"&((=
	(unordered_list 4 4 5)
	(unordered_list 4 4 5)
))&", R"(.true)"},
			{R"&((=
	(set_type
		(range 0 100)
		"unordered_list"
	)
	(set_type
		(reverse
			(range 0 100)
		)
		"unordered_list"
	)
))&", R"(.true)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 5.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LIST_and_UNORDERED_LIST(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
		return evaluableNodeManager->DeepAllocCopy(en, false);

	EvaluableNodeReference new_list(evaluableNodeManager->AllocNode(en->GetType()), true);

	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_nodes = ocn.size();
	if(num_nodes > 0)
	{
		auto &new_list_ocn = new_list->GetOrderedChildNodesReference();
		new_list_ocn.resize(num_nodes);

	#ifdef MULTITHREAD_SUPPORT
		if(en->GetConcurrency() && num_nodes > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
			if(Concurrency::threadPool.AreThreadsAvailable())
			{
				auto node_stack = CreateOpcodeStackStateSaver(new_list);
				//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
				new_list->SetNeedCycleCheck(true);

				InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

				//kick off interpreters
				for(size_t node_index = 0; node_index < num_nodes; node_index++)
					concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNode *>(ocn[node_index], en,
						new_list, EvaluableNodeImmediateValueWithType(static_cast<double>(node_index)), nullptr,
						new_list_ocn[node_index]);

				concurrency_manager.EndConcurrency();

				concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(new_list);
				return new_list;
			}
		}
	#endif

		//construction stack has a reference, so no KeepNodeReference isn't needed for anything referenced
		PushNewConstructionContext(en, new_list, EvaluableNodeImmediateValueWithType(0.0), nullptr);

		for(size_t i = 0; i < ocn.size(); i++)
		{
			SetTopCurrentIndexInConstructionStack(static_cast<double>(i));

			auto value = InterpretNode(ocn[i]);
			//add it to the list
			new_list_ocn[i] = value;
			new_list.UpdatePropertiesBasedOnAttachedNode(value);
		}

		if(PopConstructionContextAndGetExecutionSideEffectFlag())
		{
			new_list.unique = false;
			new_list.uniqueUnreferencedTopNode = false;
		}
	}

	return new_list;
}

static OpcodeInitializer _ENT_ASSOC(ENT_ASSOC, &Interpreter::InterpretNode_ENT_ASSOC, []() {
	OpcodeDetails d;
	d.parameters = R"([bstring index1] [* value1] [bstring index1] [* value2] ...)";
	d.returns = R"(assoc)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to an associative list, where each pair of parameters (e.g., `index1` and `value1`) comprises a index-value pair.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the assoc, the current index, and the current value.  If any of the bareword strings (bstrings) do not have reserved characters or whitespace, then quotes are optional; if whitespace or reserved characters are present, then quotes are required.  If `{}`'s are used instead of parenthesis, the keyword assoc may be omitted.  `{}` are considered identical to `(assoc)`)";
	d.examples = MakeAmalgamExamples({
		{R"&((unparse
	{b 2 c 3}
))&", R"("{b 2 c 3}")"},
			{R"&((unparse
	{(null) 0 (+ 1 2) 3}
))&", R"("{(null) 0 (+ 1 2) 3}")"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 352.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSOC(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
		return evaluableNodeManager->DeepAllocCopy(en, false);

	//create a new assoc from the previous
	EvaluableNodeReference new_assoc(evaluableNodeManager->AllocNode(en, false), true);

	//copy of the original evaluable node's mcn
	auto &new_mcn = new_assoc->GetMappedChildNodesReference();
	size_t num_nodes = new_mcn.size();

	if(num_nodes > 0)
	{

	#ifdef MULTITHREAD_SUPPORT
		if(en->GetConcurrency() && num_nodes > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
			if(Concurrency::threadPool.AreThreadsAvailable())
			{
				auto node_stack = CreateOpcodeStackStateSaver(new_assoc);
				//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
				new_assoc->SetNeedCycleCheck(true);

				InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

				//kick off interpreters
				for(auto &[cn_id, cn] : new_mcn)
					concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNode *>(cn,
						en, new_assoc, EvaluableNodeImmediateValueWithType(cn_id), nullptr, cn);

				concurrency_manager.EndConcurrency();

				concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(new_assoc);
				return new_assoc;
			}
		}
	#endif

		//construction stack has a reference, so no KeepNodeReference isn't needed for anything referenced
		PushNewConstructionContext(en, new_assoc, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

		for(auto &[cn_id, cn] : new_mcn)
		{
			SetTopCurrentIndexInConstructionStack(cn_id);

			//compute the value
			EvaluableNodeReference element_result = InterpretNode(cn);

			cn = element_result;
			new_assoc.UpdatePropertiesBasedOnAttachedNode(element_result);
		}

		if(PopConstructionContextAndGetExecutionSideEffectFlag())
		{
			new_assoc.unique = false;
			new_assoc.uniqueUnreferencedTopNode = false;
		}
	}

	return new_assoc;
}
