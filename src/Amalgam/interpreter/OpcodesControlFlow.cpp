//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "PerformanceProfiler.h"

static std::string _opcode_group = "Control Flow";

static OpcodeInitializer _ENT_IF(ENT_IF, &Interpreter::InterpretNode_ENT_IF, []() {
	OpcodeDetails d;
	d.parameters = R"([bool condition1] [code then1] [bool condition2] [code then2] ... [bool conditionN] [code thenN] [code else])";
	d.returns = R"(any)";
	d.description = R"(If `condition1` is true, then it will evaluate to the then1 argument.  Otherwise `condition2` will be checked, repeating for every pair.  If there is an odd number of parameters, the last is the final 'else', and will be evaluated as that if all conditions are false.  If there is an even number of parameters and none are true, then evaluates to null.)";
	d.examples = MakeAmalgamExamples({
		{R"&((if 1 "if 1"))&", R"("if 1")"},
		{R"&((if 0 "not this one" "if 2"))&", R"("if 2")"},
		{R"&((if
	.null 1
	0 2
	0 3
	4
 ))&", R"(4)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 111.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_IF(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_cn = ocn.size();

	//step every two parameters as condition-expression pairs
	for(size_t condition_num = 0; condition_num + 1 < num_cn; condition_num += 2)
	{
		if(InterpretNodeIntoBoolValue(ocn[condition_num]))
			return InterpretNode(ocn[condition_num + 1], immediate_result);
	}

	//if made it here and one more condition, then it hit the last "else" branch, so exit evaluating to the else
	if(num_cn & 1)
		return InterpretNode(ocn[num_cn - 1], immediate_result);

	//none were true
	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_SEQUENCE(ENT_SEQUENCE, &Interpreter::InterpretNode_ENT_SEQUENCE, []() {
	OpcodeDetails d;
	d.parameters = R"([code c1] [code c2] ... [code cN])";
	d.returns = R"(any)";
	d.description = R"(Runs each code block sequentially.  Evaluates to the result of the last code block run, unless it encounters a conclude or return in an earlier step, in which case it will halt processing and evaluate to the value returned by conclude or propagate the return.  Note that the last step will not consume a concluded value (see conclude opcode).)";
	d.examples = MakeAmalgamExamples({
		{R"((seq 1 2 3))", R"(3)"},
		{R"((seq
	(declare {a 1})
	(accum "a" 1)
	a
))", R"(2)"},
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 15.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SEQUENCE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t ocn_size = ocn.size();

	EvaluableNodeReference result = EvaluableNodeReference::Null();
	for(size_t i = 0; i < ocn_size; i++)
	{
		if(result.IsNonNullNodeReference())
		{
			auto result_type = result->GetType();
			if(result_type == ENT_CONCLUDE)
				return RemoveTopConcludeOrReturnNode(result, evaluableNodeManager);
			else if(result_type == ENT_RETURN)
				return result;
		}

		//free from previous iteration
		evaluableNodeManager->FreeNodeTreeIfPossible(result);

		//request immediate result when not last, since any allocs for returns would be wasted
		//concludes won't be immediate
		EvaluableNodeRequestedValueTypes cur_immediate(EvaluableNodeRequestedValueTypes::Type::NULL_VALUE);
		if(i + 1 == ocn_size)
			cur_immediate = immediate_result;
		result = InterpretNode(ocn[i], cur_immediate);
	}
	return result;
}

static OpcodeInitializer _ENT_LAMBDA(ENT_LAMBDA, &Interpreter::InterpretNode_ENT_LAMBDA, []() {
	OpcodeDetails d;
	d.parameters = R"(* function [bool evaluate_and_wrap])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the code specified without evaluating it.  Useful for referencing functions or handling data without evaluating it.  The parameter `evaluate_and_wrap` defaults to false, but if it is true, it will evaluate the function, but then return the result wrapped in a lambda opcode.)";
	d.examples = MakeAmalgamExamples({
		{R"((lambda (+ 1 2)))", R"((+ 1 2))"},
		{R"((seq
	(declare {foo (lambda (+ y 1))})
	(call foo {y 1})
))", R"(2)"},
			{R"((lambda (+ 1 2) .true ))", R"((lambda 3))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 58.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LAMBDA(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t ocn_size = ocn.size();
	if(ocn_size == 0)
	{
		return EvaluableNodeReference::Null();
	}
	else if(ocn_size == 1 || !EvaluableNode::ToBool(ocn[1]))
	{
		//if only one parameter or second parameter isn't true, just return the result
		return EvaluableNodeReference(ocn[0], false);
	}
	else //evaluate and then wrap in a lambda
	{
		EvaluableNodeReference evaluated_value = InterpretNode(ocn[0]);

		//need to evaluate its parameter and return a new node encapsulating it
		EvaluableNodeReference lambda(evaluableNodeManager->AllocNode(ENT_LAMBDA), true);
		lambda->AppendOrderedChildNode(evaluated_value);
		lambda.UpdatePropertiesBasedOnAttachedNode(evaluated_value, true);

		return lambda;
	}
}

static OpcodeInitializer _ENT_CALL(ENT_CALL, &Interpreter::InterpretNode_ENT_CALL, []() {
	OpcodeDetails d;
	d.parameters = R"(* function [assoc arguments])";
	d.returns = R"(any)";
	d.description = R"(Evaluates `function` after pushing the `arguments` assoc onto the scope stack.)";
	d.examples = MakeAmalgamExamples({
		{R"&((let
	{
		foo (lambda
				(declare
					{x 6}
					(+ x 2)
				)
			)
	}
	(call
		foo
		{x 3}
	)
))&", R"(5)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.newScope = true;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 112.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsNull(function))
		return EvaluableNodeReference::Null();

	if(function.GetIsIdempotent())
	{
		if(!function.unique)
			function = evaluableNodeManager->DeepAllocCopy(function, false);
		if(function.IsNonNullNodeReference() && function->GetType() == ENT_RETURN)
			function = RemoveTopConcludeOrReturnNode(function, evaluableNodeManager);
		return function;
	}

	auto node_stack = CreateOpcodeStackStateSaver(function);

	bool profiling_call = false;
	if(_label_profiling_enabled && curEntity != nullptr)
	{
		auto [label_sid, found] = curEntity->GetLabelForNodeIfExists(function);
		size_t num_nodes = evaluableNodeManager->GetNumberOfUsedNodes();
		if(label_sid != string_intern_pool.NOT_A_STRING_ID)
			PerformanceProfiler::StartOperation(label_sid->string, num_nodes);
		else
			PerformanceProfiler::StartOperation("", num_nodes);
		profiling_call = true;
	}

	//if have an scope stack context of variables specified, then use it
	EvaluableNodeReference new_context = EvaluableNodeReference::Null();
	if(ocn.size() > 1)
	{
		//can keep constant, but need the top node to be unique in case assignments are made
		new_context = InterpretNodeForImmediateUse(ocn[1]);
		evaluableNodeManager->EnsureNodeIsModifiable(new_context, false, false);
	}

	PushNewScopeStack(new_context);

	//call the code
	auto result = InterpretNode(function, immediate_result);

	PopScopeStack(result.unique);

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, evaluableNodeManager);

	if(profiling_call)
		PerformanceProfiler::EndOperation(evaluableNodeManager->GetNumberOfUsedNodes());

	return result;
}

static OpcodeInitializer _ENT_CALL_SANDBOXED(ENT_CALL_SANDBOXED, &Interpreter::InterpretNode_ENT_CALL_SANDBOXED, []() {
	OpcodeDetails d;
	d.parameters = R"(* function assoc arguments [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [bool return_warnings])";
	d.returns = R"(any)";
	d.description = R"(Evaluates the code specified by function, isolating it from everything except for arguments, which is used as a single layer of the scope stack.  This is useful when evaluating code passed by other entities that may or may not be trusted.  Opcodes run from within call_sandboxed that require any form of permissions will not perform any action and will evaluate to null.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed. If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted, up to the limits of the current calling context. If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If `max_node_allocations` is 0 or infinite and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if `max_node_allocations` is specified while call_sandboxed is being called in a multithreaded environment, if the collective memory from all the related threads exceeds the average memory specified by call_sandboxed, that may trigger a memory limit for the call_sandboxed.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called. If `return_warnings` is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  If `return_warnings` is false, just the value will be returned.)";
	d.examples = MakeAmalgamExamples({
		{R"&((call_sandboxed
	(lambda
		(+
			(+ y 4)
			4
		)
	)
	{y 3}
	.null
	.null
	50
))&", R"([11 {} .null])"},
			{R"&((call_sandboxed
	(lambda
		(+
			(+ y 4)
			4
		)
	)
	{y 3}
	.null
	.null
	1
))&", R"([.null {} "Execution depth exceeded"])"},
			{R"&((call_sandboxed
	(lambda
		(call_sandboxed
			(lambda
				(+
					(+ y 4)
					4
				)
			)
			{y 3}
			.null
			.null
			2
		)
	)
	{y 3}
	.null
	.null
	50
))&", R"([
	[.null {} "Execution depth exceeded"]
	{}
	.null
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.newScope = true;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_SANDBOXED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsNull(function))
		return EvaluableNodeReference::Null();

	if(function.GetIsIdempotent())
	{
		if(!function.unique)
			function = evaluableNodeManager->DeepAllocCopy(function, false);
		if(function.IsNonNullNodeReference() && function->GetType() == ENT_RETURN)
			function = RemoveTopConcludeOrReturnNode(function, evaluableNodeManager);
		return function;
	}

	auto node_stack = CreateOpcodeStackStateSaver(function);

	InterpreterConstraints interpreter_constraints;
	InterpreterConstraints *interpreter_constraints_ptr = nullptr;

	if(PopulateInterpreterConstraintsFromParams(ocn, 2, interpreter_constraints))
		interpreter_constraints_ptr = &interpreter_constraints;

	//need to return a more complex data structure, can't return immediate
	if(interpreter_constraints_ptr != nullptr && interpreter_constraints_ptr->collectWarnings)
		immediate_result = EvaluableNodeRequestedValueTypes::Type::NONE;

	//if have a scope stack context of variables specified, then use it
	EvaluableNodeReference args = EvaluableNodeReference::Null();
	if(ocn.size() > 1)
		args = InterpretNode(ocn[1]);

	//build scope stack from parameters
	auto scope_stack = ConvertArgsToScopeStack(args, *evaluableNodeManager);

	PopulatePerformanceCounters(interpreter_constraints_ptr, nullptr);

	Interpreter sandbox(evaluableNodeManager, randomStream.CreateOtherStreamViaRand(),
		writeListeners, printListener, interpreter_constraints_ptr, nullptr, this);

#ifdef MULTITHREAD_SUPPORT
	// everything at this point is referenced on stacks; allow the sandbox to trigger a garbage collect without this interpreter blocking
	std::swap(memoryModificationLock, sandbox.memoryModificationLock);
#endif

	//improve performance by managing the stacks here
	auto result = sandbox.ExecuteNode(function, &scope_stack, nullptr, nullptr,
		nullptr, immediate_result);

#ifdef MULTITHREAD_SUPPORT
	//hand lock back to this interpreter
	std::swap(memoryModificationLock, sandbox.memoryModificationLock);
#endif

	if(result.unique)
		evaluableNodeManager->FreeNodeTreeIfPossible(args);
	else //it's possible some value is returned, can only free top node
		evaluableNodeManager->FreeNodeIfPossible(args);

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, evaluableNodeManager);

	if(interpreterConstraints != nullptr)
		interpreterConstraints->AccruePerformanceCounters(interpreter_constraints_ptr);

	//if only want results, return them
	if(!interpreter_constraints.collectWarnings)
	{
		if(interpreter_constraints_ptr != nullptr && interpreter_constraints.constraintsExceeded)
			return EvaluableNodeReference::Null();
		return result;
	}

	if(interpreter_constraints_ptr != nullptr && interpreter_constraints.constraintsExceeded)
		return BundleResultWithWarningsIfNeeded(EvaluableNodeReference::Null(), interpreter_constraints_ptr);

	return BundleResultWithWarningsIfNeeded(result,
		interpreter_constraints_ptr != nullptr ? interpreter_constraints_ptr : &interpreter_constraints);
}

static OpcodeInitializer _ENT_WHILE(ENT_WHILE, &Interpreter::InterpretNode_ENT_WHILE, []() {
	OpcodeDetails d;
	d.parameters = R"(bool condition [code code1] [code code2] ... [code codeN])";
	d.returns = R"(any)";
	d.description = R"(Each time the `condition` evaluates to true, it runs each of code sequentially, looping. Evaluates to the last `codeN` or null if the `condition` was initially false or if it encounters a `conclude` or `return`, it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  For each iteration of the loop, it pushes a new target scope onto the target stack, with `(current_index)` being the iteration count, and `(previous_result)` being the last evaluated `codeN` of the previous loop.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(assign
		{i 1}
	)
	(while
		(< i 10)
		(accum
			{i 1}
		)
	)
	i
))&", R"(10)"},
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 2.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_WHILE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t ocn_size = ocn.size();
	if(ocn_size == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference previous_result = EvaluableNodeReference::Null();

	PushNewConstructionContext(nullptr, nullptr, EvaluableNodeImmediateValueWithType(0.0), nullptr);

	size_t loop_iteration = 0;
	for(;;)
	{
		SetTopCurrentIndexInConstructionStack(static_cast<double>(loop_iteration++));
		SetTopPreviousResultInConstructionStack(previous_result);

		//keep the result before testing condition
		if(!InterpretNodeIntoBoolValue(ocn[0]))
			break;

		//count an extra cycle for each loop
		//this ensures that even if all of the nodes are immediate, it'll still count the performance
		if(AreExecutionResourcesExhausted(true))
		{
			PopConstructionContextAndGetExecutionSideEffectFlag();
			return EvaluableNodeReference::Null();
		}

		//run each step within the loop
		EvaluableNodeReference new_result = EvaluableNodeReference::Null();
		for(size_t i = 1; i < ocn_size; i++)
		{
			//request immediate values when not last, since any allocs for returns would be wasted
			//concludes won't be immediate
			//but because previous_result may be used, that can't be immediate, so the last param
			//cannot be evaluated as immediate
			new_result = InterpretNode(ocn[i], i + 1 < ocn_size);

			if(new_result.IsNonNullNodeReference())
			{
				auto new_result_type = new_result->GetType();
				if(new_result_type == ENT_CONCLUDE || new_result_type == ENT_RETURN)
				{
					//if previous result is unconsumed, free if possible
					previous_result = GetAndClearPreviousResultInConstructionStack(0);
					evaluableNodeManager->FreeNodeTreeIfPossible(previous_result);

					PopConstructionContextAndGetExecutionSideEffectFlag();

					if(new_result_type == ENT_CONCLUDE)
						return RemoveTopConcludeOrReturnNode(new_result, evaluableNodeManager);
					else if(new_result_type == ENT_RETURN)
						return new_result;
				}
			}

			//don't free the last new_result
			if(i + 1 < ocn_size)
				evaluableNodeManager->FreeNodeTreeIfPossible(new_result);
		}

		//if previous result is unconsumed, free if possible
		previous_result = GetAndClearPreviousResultInConstructionStack(0);
		evaluableNodeManager->FreeNodeTreeIfPossible(previous_result);

		previous_result = new_result;
	}

	PopConstructionContextAndGetExecutionSideEffectFlag();
	return previous_result;
}

static OpcodeInitializer _ENT_CONCLUDE(ENT_CONCLUDE, &Interpreter::InterpretNode_ENT_CONCLUDE_and_RETURN, []() {
	OpcodeDetails d;
	d.parameters = R"(* conclusion)";
	d.returns = R"(any)";
	d.description = R"(Evaluates to `conclusion` wrapped in a `conclude` opcode.  If a step in a `seq`, `let`, `declare`, or `while` evaluates to a `conclude` (excluding variable declarations for `let` and `declare`, the last step in `set`, `let`, and `declare`, or the condition of `while`), then it will conclude the execution and evaluate to the value `conclusion`.  Note that conclude opcodes may be nested to break out of outer opcodes.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	"seq1"
	(conclude "success")
	"seq2"
))&", R"("success")"},
			{R"&((while
	(< 1 100)
	"while1"
	(conclude "success")
	"while2"
))&", R"("success")"},
			{R"&((let
	{a 1}
	"let1"
	(conclude "success")
	"let2"
))&", R"("success")"},
			{R"&((declare
	{abcd 1}
	"declare1"
	(conclude "success")
	"declare2"
))&", R"("success")"},
			{R"&((seq
	1
	(declare
		{}
		(while
			1
			(if .true (conclude))
		)
		4
	)
	2
))&", R"(2)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 9.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_RETURN(ENT_RETURN, &Interpreter::InterpretNode_ENT_CONCLUDE_and_RETURN, []() {
	OpcodeDetails d;
	d.parameters = R"(* return_value)";
	d.returns = R"(any)";
	d.description = R"(Evaluates to `return_value` wrapped in a `return` opcode.  If a step in a `seq`, `let`, `declare`, or `while` evaluates to a return (excluding variable declarations for `let` and `declare`, the last step in `set`, `let`, and `declare`, or the condition of `while`), then it will conclude the execution and evaluate to the `return` opcode with its `return_value`.  This means it will continue to conclude each level up the stack until it reaches any kind of call opcode, including `call`, `call_sandboxed`, `call_entity`, `call_entity_get_changes`, or `call_container`, at which point it will evaluate to `return_value`.  Note that return opcodes may be nested to break out of multiple calls.)";
	d.examples = MakeAmalgamExamples({
		{R"&((call
	(seq
		1
		2
		(seq
			(return 3)
			4
		)
		5
	)
))&", R"(3)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.potentiallyIdempotent = true;
	d.frequencyPer10000Opcodes = 11.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONCLUDE_and_RETURN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	//if no parameter, then return itself for performance
	if(ocn.size() == 0)
		return EvaluableNodeReference(en, false);

	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
		return evaluableNodeManager->DeepAllocCopy(en, false);

	EvaluableNodeReference value = InterpretNode(ocn[0]);

	//need to evaluate its parameter and return a new node encapsulating it
	auto node_type = en->GetType();
	EvaluableNodeReference result(evaluableNodeManager->AllocNode(node_type), true);
	result->AppendOrderedChildNode(value);
	result.UpdatePropertiesBasedOnAttachedNode(value, true);

	return result;
}

static OpcodeInitializer _ENT_APPLY(ENT_APPLY, &Interpreter::InterpretNode_ENT_APPLY, []() {
	OpcodeDetails d;
	d.parameters = R"(* to_apply [list|assoc collection])";
	d.returns = R"(any)";
	d.description = R"(Creates a new list of the values of the elements of the `collection`, applies the type specified by `to_apply`, which is either the type corresponding to a string or the type of `to_apply`, and then evaluates it.  If `to_apply` has any parameters, i.e., it is a node with one or more elements, these are prepended to the `collection` as the first parameters.  When no extra parameters are passed, it is a more efficient equivalent to `(call (set_type type collection))`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((apply
	(lambda (+))
	[1 2 3 4]
))&", R"(10)"},
			{R"&((apply
	(lambda
		(+ 5)
	)
	[1 2 3 4]
))&", R"(15)"},
			{R"&((apply
	"+"
	[1 2 3 4]
))&", R"(10)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 12.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_APPLY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//can't interpret for immediate use in case the node has child nodes that will be prepended
	auto type_node = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(type_node))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(type_node);
		return EvaluableNodeReference::Null();
	}

	//get the type to set
	EvaluableNodeType new_type = ENT_NULL;
	if(type_node->GetType() == ENT_STRING)
	{
		auto new_type_sid = type_node->GetStringIDReference();
		new_type = GetEvaluableNodeTypeFromStringId(new_type_sid);
	}
	else
	{
		new_type = type_node->GetType();
	}

	if(!IsEvaluableNodeTypeValid(new_type))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(type_node);
		return EvaluableNodeReference::Null();
	}

	auto node_stack = CreateOpcodeStackStateSaver(type_node);

	//if new_type doesn't affect anything and always creates a new value, then
	//don't need to maintain source (can be interpreted as immediate) and can free it
	auto opcode_new_value_return_type = GetOpcodeNewValueReturnType(new_type);
	bool may_opcode_cause_node_update_in_current_entity = MayOpcodeCauseNodeUpdateInCurrentEntity(new_type);

	EvaluableNodeReference source;
	if(may_opcode_cause_node_update_in_current_entity
			|| opcode_new_value_return_type == OpcodeDetails::OpcodeReturnNewnessType::PARTIAL
			|| opcode_new_value_return_type == OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL
			|| opcode_new_value_return_type == OpcodeDetails::OpcodeReturnNewnessType::EXISTING)
		source = InterpretNode(ocn[1]);
	else //returns a new value without affecting anything else, can call potentially faster interpret
		source = InterpretNodeForImmediateUse(ocn[1]);

	//change source type
	if(source == nullptr)
		source.SetReference(evaluableNodeManager->AllocNode(ENT_NULL));
	evaluableNodeManager->EnsureNodeIsModifiable(source);
	source->SetType(new_type, evaluableNodeManager, true);

	//prepend any params
	if(source->IsOrderedArray())
	{
		auto &type_node_ocn = type_node->GetOrderedChildNodes();
		if(type_node_ocn.size() > 0)
		{
			auto &source_ocn = source->GetOrderedChildNodesReference();
			source_ocn.insert(
				begin(source_ocn), begin(type_node_ocn), end(type_node_ocn));
			source.UpdatePropertiesBasedOnAttachedNode(type_node);

			//can transfer ownership of the nodes, so can be freed below
			if(type_node.unique && !type_node->GetNeedCycleCheck())
				type_node_ocn.clear();
		}
	}
	evaluableNodeManager->FreeNodeTreeIfPossible(type_node);
	node_stack.PopEvaluableNode();

	//apply the new type, using whether or not it was a unique reference,
	//passing through whether an immediate_result is desired
	EvaluableNodeReference result = InterpretNode(source, immediate_result);

	//can free the source if none of its data can be referenced elsewhere
	if(!may_opcode_cause_node_update_in_current_entity && result.unique)
		evaluableNodeManager->FreeNodeTreeIfPossible(source);

	return result;
}

static OpcodeInitializer _ENT_OPCODE_STACK(ENT_OPCODE_STACK, &Interpreter::InterpretNode_ENT_OPCODE_STACK, []() {
	OpcodeDetails d;
	d.parameters = R"([number stack_distance] [bool no_child_nodes])";
	d.returns = R"(list of any)";
	d.description = R"(Evaluates to the list of opcodes that make up the call stack or a single opcode within the call stack.  If `stack_distance` is specified, then a copy of the node at that specified depth is returned, otherwise the list of all opcodes in opcode stack are returned. Negative values for `stack_distance` specify the depth from the top of the stack and positive values specify the depth from the bottom.  If `no_child_nodes` is true, then only the root node(s) are returned, otherwise the returned node(s) are deep-copied.)";
	d.examples = MakeAmalgamExamples({
		{R"&((size (opcode_stack)))&", R"(2)"},
		{R"&((seq
	(seq
		(opcode_stack 2)
	)
))&", R"((seq
	(seq
		(opcode_stack 2)
	)
))"},
			{R"&((seq
	(seq
		(opcode_stack -1 .true)
	)
))&", R"((seq))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_OPCODE_STACK(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	bool has_valid_depth = false;
	int64_t depth = 0;
	if(ocn.size() > 0)
	{
		double value = InterpretNodeIntoNumberValue(ocn[0]);
		if(!FastIsNaN(value))
		{
			has_valid_depth = true;
			depth = static_cast<int64_t>(value);
		}
	}

	bool no_child_nodes = false;
	if(ocn.size() > 1)
		no_child_nodes = InterpretNodeIntoBoolValue(ocn[1], false);

	if(!has_valid_depth)
	{
		//return the whole opcode stack
		//can create this node on the stack because will be making a copy
		if(!no_child_nodes)
		{
			EvaluableNode stack_top_holder(ENT_LIST);
			stack_top_holder.SetOrderedChildNodes(opcodeStackNodes);
			return evaluableNodeManager->DeepAllocCopy(&stack_top_holder);
		}
		else
		{
			EvaluableNodeReference stack_top_holder(evaluableNodeManager->AllocNode(ENT_LIST), true);
			auto &sth_ocn = stack_top_holder->GetOrderedChildNodesReference();
			sth_ocn.reserve(opcodeStackNodes.size());
			bool first_node = true;
			for(auto iter = begin(opcodeStackNodes); iter != end(opcodeStackNodes); ++iter)
			{
				EvaluableNode *cur_node = *iter;
				EvaluableNodeReference new_node(evaluableNodeManager->AllocNode(cur_node->GetType()), true);
				new_node->CopyMetadataFrom(cur_node);
				sth_ocn.push_back(new_node);
				stack_top_holder.UpdatePropertiesBasedOnAttachedNode(new_node, first_node);
				first_node = false;
			}
			return stack_top_holder;
		}
	}
	else
	{
		//only return one node from the opcode stack
		int64_t actual_offset;
		if(depth < 0)
			actual_offset = opcodeStackNodes.size() + depth;
		else
			actual_offset = depth;

		if(actual_offset < 0 || actual_offset >= static_cast<int64_t>(opcodeStackNodes.size()))
		{
			return EvaluableNodeReference::Null();
		}
		else
		{
			EvaluableNode *cur_node = *(end(opcodeStackNodes) - actual_offset - 1);
			if(!no_child_nodes)
			{
				return evaluableNodeManager->DeepAllocCopy(cur_node);
			}
			else
			{
				//only copy top level node
				EvaluableNodeReference new_node(evaluableNodeManager->AllocNode(cur_node->GetType()), true);
				new_node->CopyMetadataFrom(cur_node);
				return new_node;
			}
		}
	}
}
