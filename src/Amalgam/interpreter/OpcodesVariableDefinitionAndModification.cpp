//project headers:
#include "AssetManager.h"
#include "DateTimeFormat.h"
#include "FileSupportJSON.h"
#include "FileSupportYAML.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "PerformanceProfiler.h"

static std::string _opcode_group = "Variable Definition and Modification";

static OpcodeInitializer _ENT_SYMBOL(ENT_SYMBOL, &Interpreter::InterpretNode_ENT_SYMBOL, []() {
	OpcodeDetails d;
	d.parameters = R"()";
	d.returns = R"(*)";
	d.description = R"(A string representing an internal symbol, a variable.)";
	d.examples = MakeAmalgamExamples({
		{R"&((let
	{foo 1}
	foo
))&", R"(1)"},
			{R"&(not_defined)&", R"(.null)"},
			{R"&((lambda foo))&", R"(foo)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::NONE;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 4329.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SYMBOL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	StringInternPool::StringID sid = en->GetStringIDReference();
	if(sid == StringInternPool::NOT_A_STRING_ID)
		return EvaluableNodeReference::Null();

	//when retrieving symbol, only need to retain the node if it's not an immediate type
	bool retain_node = !immediate_result.AnyImmediateType();
	auto [symbol_value, found] = GetScopeStackSymbol(sid, retain_node);
	if(found)
		return EvaluableNodeReference::CoerceNonUniqueEvaluableNodeToImmediateIfPossible(symbol_value, immediate_result);

	//if didn't find it in the stack, try it in the labels
	//don't need to lock the entity since it's already executing on it
	if(curEntity != nullptr)
	{ 
		auto [label_value, label_found] = curEntity->GetValueAtLabel(sid, evaluableNodeManager, immediate_result, true);
		if(label_found)
			return label_value;
	}

	EmitOrLogUndefinedVariableWarningIfNeeded(sid, en);

	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_LET(ENT_LET, &Interpreter::InterpretNode_ENT_LET, []() {
	OpcodeDetails d;
	d.parameters = R"(assoc variables [code code1] [code code2] ... [code codeN])";
	d.returns = R"(any)";
	d.description = R"(Pushes the key-value pairs of `variables` onto the scope stack so that they become the new variables, then runs each code block sequentially, evaluating to the last code block run, unless it encounters a `conclude` or `return`, in which case it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  Note that the last step will not consume a concluded value.)";
	d.examples = MakeAmalgamExamples({
		{R"&((let
	{x 4 y 6}
	(+ x y)
))&", R"(10)"},
			{R"&((let
	{x 4 y 6}
	(declare
		{x 5 z 1}
		(+ x y z)
	)
))&", R"(11)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
	d.newScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 26.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LET(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t ocn_size = ocn.size();
	if(ocn_size == 0)
		return EvaluableNodeReference::Null();

	InterpretAndPushNewScopeStackNode(ocn[0]);

	//run code
	EvaluableNodeReference result = EvaluableNodeReference::Null();
	for(size_t i = 1; i < ocn_size; i++)
	{
		if(result.IsNonNullNodeReference())
		{
			auto result_type = result->GetType();
			if(result_type == ENT_CONCLUDE)
			{
				PopScopeStack(result.unique);
				return RemoveTopConcludeOrReturnNode(result, evaluableNodeManager);
			}
			else if(result_type == ENT_RETURN)
			{
				PopScopeStack(result.unique);
				return result;
			}
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

	//all finished with new context, but can't free it in case returning something
	PopScopeStack(result.unique);
	return result;
}

static OpcodeInitializer _ENT_DECLARE(ENT_DECLARE, &Interpreter::InterpretNode_ENT_DECLARE, []() {
	OpcodeDetails d;
	d.parameters = R"(assoc variables [code code1] [code code2] ... [code codeN])";
	d.returns = R"(any)";
	d.description = R"(For each key-value pair of `variables`, if not already in the current context in the scope stack, it will define them.  Then it runs each code block sequentially, evaluating to the last code block run, unless it encounters a `conclude` or `return`, in which case it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  Note that the last step will not consume a concluded value.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(declare
		{x 7}
		(accum "x" 1)
	)
	(declare
		{x 4}
	)
	x
))&", R"(8)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.hasSideEffects = true;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 49.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

#ifdef MULTITHREAD_SUPPORT
static inline void RecordStackLockForProfiling(EvaluableNode *en, StringInternPool::StringID variable_sid)
{
	if(Interpreter::_opcode_profiling_enabled)
	{
		std::string variable_location = asset_manager.GetEvaluableNodeSourceFromComments(en);
		variable_location += string_intern_pool.GetStringFromID(variable_sid);
		PerformanceProfiler::AccumulateLockContentionCount(variable_location);
	}
}
#endif

EvaluableNodeReference Interpreter::InterpretNode_ENT_DECLARE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t ocn_size = ocn.size();
	if(ocn_size == 0)
		return EvaluableNodeReference::Null();

	//work on the node that is declaring the variables
	EvaluableNode *required_vars_node = ocn[0];
	//transform into variables if possible
	EvaluableNodeReference required_vars = EvaluableNodeReference::Null();
	bool need_to_interpret_required_vars = false;
	if(required_vars_node != nullptr)
	{
		if(required_vars_node->IsAssociativeArray())
		{
			required_vars = EvaluableNodeReference(required_vars_node, false);
			need_to_interpret_required_vars = !required_vars_node->GetIsIdempotent();
		}
		else //just need to interpret
		{
			required_vars = InterpretNodeForImmediateUse(required_vars_node);
		}
	}

	bool any_nonunique_assignments = false;
	if(EvaluableNode::IsAssociativeArray(required_vars))
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleLock write_lock;
		bool need_write_lock = HasSharedScopeStackTop();
		if(need_write_lock)
		{
			LockScopeStackTop(write_lock, required_vars);
			RecordStackLockForProfiling(en, string_intern_pool.NOT_A_STRING_ID);
		}
	#endif

		//get the current layer of the stack
		EvaluableNode *scope = GetCurrentScopeStackContext();
		if(scope == nullptr)	//this shouldn't happen, but just in case it does
			return EvaluableNodeReference::Null();

		if(!need_to_interpret_required_vars)
		{
			//check each of the required variables and put into the stack if appropriate
			for(auto &[cn_id, cn] : required_vars->GetMappedChildNodesReference())
			{
				auto [inserted, node_ptr] = scope->SetMappedChildNode(cn_id, cn, false);
				if(inserted)
				{
					//not unique so just set to true
					any_nonunique_assignments = true;

					if(cn != nullptr)
						cn->SetIsFreeable(required_vars.unique);
				}
				else
				{
					//if it can't insert the new variable because it already exists,
					// then try to free the default / new value that was attempted to be assigned
					if(required_vars.unique && !required_vars.GetNeedCycleCheck())
						evaluableNodeManager->FreeNodeTree(cn);
				}
			}
		}
		else //need_to_interpret_required_vars
		{
			auto &scope_mcn = scope->GetMappedChildNodesReference();

			PushNewConstructionContext(required_vars, nullptr,
				EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

			//check each of the required variables and put into the stack if appropriate
			for(auto &[cn_id, cn] : required_vars->GetMappedChildNodesReference())
			{
				if(cn == nullptr || cn->GetIsIdempotent())
				{
					auto [inserted, node_ptr] = scope->SetMappedChildNode(cn_id, cn, false);
					if(inserted)
						any_nonunique_assignments = true;
					//if not inserted, don't need to free it since it wasn't interpreted
				}
				else //need to interpret
				{
					//don't need to do anything if the variable already exists
					//but can't insert the variable here because it will mask definitions further up the stack that
					//may be used in the declare
					if(scope_mcn.find(cn_id) != end(scope_mcn))
						continue;

				#ifdef MULTITHREAD_SUPPORT
					//unlock before interpreting
					if(need_write_lock)
						write_lock.unlock();
				#endif

					SetTopCurrentIndexInConstructionStack(cn_id);
					EvaluableNodeReference value = InterpretNodeForImmediateUse(cn);

					//mark if not unique
					any_nonunique_assignments |= !value.unique;

				#ifdef MULTITHREAD_SUPPORT
					//relock if needed before assigning the value
					if(need_write_lock)
					{
						LockScopeStackTop(write_lock, required_vars);
					}
					else
					#endif
						//only set unread if writing to parts of the stack that aren't shared
						if(value != nullptr)
							value->SetIsFreeable(value.unique);

					scope->SetMappedChildNode(cn_id, value, false);
				}
			}

			if(PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				required_vars.unique = false;
				required_vars.uniqueUnreferencedTopNode = false;
			}
		}

		//free the vars / assoc node
		evaluableNodeManager->FreeNodeIfPossible(required_vars);
	}
	else //not an assoc
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(required_vars);
	}

	if(any_nonunique_assignments)
		SetSideEffectFlagsAndAccumulatePerformanceCounters(en);

	//used to store the result or clear if possible
	EvaluableNodeReference result = EvaluableNodeReference::Null();

	//run code
	for(size_t i = 1; i < ocn_size; i++)
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

static OpcodeInitializer _ENT_ASSIGN(ENT_ASSIGN, &Interpreter::InterpretNode_ENT_ASSIGN_and_ACCUM, []() {
	OpcodeDetails d;
	d.parameters = R"(assoc|string variables [number index1|string index1|list walk_path1|* new_value1] [* new_value1] [number index2|string index2|list walk_path2] [* new_value2] ...)";
	d.returns = R"(.null)";
	d.description = R"(If `variables` is an assoc, then for each key-value pair it assigns the value to the variable represented by the key found by tracing upward on the stack.  If a variable is not found, it will create a variable on the top of the stack with that name.  If `variables` is a string and there are two parameters, it will assign the second parameter to the variable represented by the first.  If `variables` is a string and there are three or more parameters, then it will find the variable by tracing up the stack and then use each pair of walk_path and new_value to assign new_value to that part of the variable's structure.)";
	d.examples = MakeAmalgamExamples({
		{R"&((let
	{x 0}
	(assign {x 10} )
	x
))&", R"(10)"},
			{R"&((seq
	(assign "x" 20)
	x
))&", R"(20)"},
			{R"&((seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(assign
		"x"
		[1]
		"not 1"
	)
	x
))&", R"([
	0
	"not 1"
	2
	{a 1 b 2 c 3}
])"},
			{R"&((seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(assign
		"x"
		[3 "c"]
		["c attribute"]
		[3 "a"]
		["a attribute"]
	)
	x
))&", R"([
	0
	1
	2
	{
		a ["a attribute"]
		b 2
		c ["c attribute"]
	}
])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
	d.hasSideEffects = true;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 61.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_ACCUM(ENT_ACCUM, &Interpreter::InterpretNode_ENT_ASSIGN_and_ACCUM, []() {
	OpcodeDetails d;
	d.parameters = R"(assoc|string variables [number index1|string index1|list walk_path1] [* accum_value1] [number index2|string index2|list walk_path2] [* accum_value2] ...)";
	d.returns = R"(.null)";
	d.description = R"(If `variables` is an assoc, then for each key-value pair of data, it assigns the value of the pair accumulated with the current value of the variable represented by the key on the stack, and stores the result in the variable.  It searches for the variable name tracing up the stack to find the variable. If the variable is not found, it will create a variable on the top of the stack.  Accumulation is performed differently based on the type.  For numeric values it adds, for strings it concatenates, for lists and assocs it appends.  If `variables` is a string and there are two parameters, then it will accum the second parameter to the variable represented by the first.  If `variables` is a string and there are three or more parameters, then it will find the variable by tracing up the stack and then use each pair of the corresponding walk path and accum value to that part of the variable's structure.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(assign
		{x 10}
	)
	(accum
		{x 1}
	)
	x
))&", R"(11)"},
			{R"&((declare
	{
		accum_assoc (associate "a" 1 "b" 2)
		accum_list [1 2 3]
		accum_string "abc"
	}
	(accum
		{accum_string "def"}
	)
	(accum
		{
			accum_list [4 5 6]
		}
	)
	(accum
		{
			accum_list (associate "7" 8)
		}
	)
	(accum
		{
			accum_assoc (associate "c" 3 "d" 4)
		}
	)
	(accum
		{
			accum_assoc ["e" 5]
		}
	)
	[accum_string accum_list accum_assoc]
))&", R"([
	"abcdef"
	[
		1
		2
		3
		4
		5
		6
		"7"
		8
	]
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
])"},
			{R"&((seq
		(assign "x" 1)
		(accum "x" [] 4)
		x
))&", R"(5)"},
			{R"&((seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(accum
		"x"
		[1]
		1
	)
	x
))&", R"([
	0
	2
	2
	{a 1 b 2 c 3}
])"},
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
	d.hasSideEffects = true;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 11.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_and_ACCUM(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_params = ocn.size();

	if(num_params < 1)
		return EvaluableNodeReference::Null();

	//make sure there's at least a scopeStack to use
	if(scopeStack.size() < 1)
		return EvaluableNodeReference::Null();

	bool accum = (en->GetType() == ENT_ACCUM);

	//if only one parameter, then assume it is an assoc of variables to accum or assign
	if(num_params == 1)
	{
		EvaluableNode *assigned_vars_node = ocn[0];
		if(assigned_vars_node == nullptr)
			return EvaluableNodeReference::Null();

		EvaluableNodeReference assigned_vars;
		bool need_to_interpret = false;
		if(assigned_vars_node->GetIsIdempotent())
		{
			assigned_vars = EvaluableNodeReference(assigned_vars_node, false);
		}
		else if(assigned_vars_node->IsAssociativeArray())
		{
			assigned_vars = EvaluableNodeReference(assigned_vars_node, false);
			need_to_interpret = true;
		}
		else //just need to interpret
		{
			assigned_vars = InterpretNode(assigned_vars_node);
		}

		if(!EvaluableNode::IsAssociativeArray(assigned_vars))
			return EvaluableNodeReference::Null();

		auto node_stack = CreateOpcodeStackStateSaver(assigned_vars);

		bool any_nonunique_assignments = false;

		//iterate over every variable being assigned
		for(auto &[cn_id, cn] : assigned_vars->GetMappedChildNodesReference())
		{
			StringInternPool::StringID variable_sid = cn_id;
			if(variable_sid == StringInternPool::NOT_A_STRING_ID)
				continue;

			//evaluate the value
			EvaluableNodeReference variable_value_node(cn, assigned_vars.unique);
			if(need_to_interpret && cn != nullptr && !cn->GetIsIdempotent())
			{
				PushNewConstructionContext(assigned_vars, assigned_vars, EvaluableNodeImmediateValueWithType(variable_sid), nullptr);
				variable_value_node = InterpretNode(cn);
				if(PopConstructionContextAndGetExecutionSideEffectFlag())
				{
					assigned_vars.unique = false;
					assigned_vars.uniqueUnreferencedTopNode = false;
				}
			}

			//retrieve the symbol location
		#ifdef MULTITHREAD_SUPPORT
			//need to save variable_value_node because GetScopeStackSymbolLocationWithLock
			// may collect garbage while waiting for the lock
			node_stack.PushEvaluableNode(variable_value_node);

			Concurrency::SingleLock write_lock;
			auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocationWithLock(variable_sid, true, write_lock);
			if(write_lock.owns_lock())
				RecordStackLockForProfiling(en, variable_sid);

			node_stack.PopEvaluableNode();
		#else
			auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocation(variable_sid, true, false);
		#endif

			if(accum && !EvaluableNode::IsNull(*value_destination))
			{
				if(is_freeable)
				{
					EvaluableNodeReference value_destination_node(*value_destination, true);
					variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, variable_value_node, evaluableNodeManager);
				}
				else
				{
					//values should always be copied before changing, in case the value is used elsewhere, especially in another thread
					EvaluableNodeReference value_destination_node = evaluableNodeManager->DeepAllocCopy(*value_destination);
					variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, variable_value_node, evaluableNodeManager);
				}
			}
			else if(is_freeable)
			{
				EvaluableNodeReference value_destination_node(*value_destination, true);
				evaluableNodeManager->FreeNodeTreeIfPossible(value_destination_node);
			}

			any_nonunique_assignments |= !variable_value_node.unique;
			//if writing to an outer scope, can't guarantee the memory at this scope can be freed
			any_nonunique_assignments |= !top_of_stack;

			//need to set whether freeable in case a variable's value is assigned to another variable
			if(variable_value_node != nullptr)
				variable_value_node->SetIsFreeable(variable_value_node.unique);

			//assign back into the context_to_use
			*value_destination = variable_value_node;
		}

		if(any_nonunique_assignments)
			SetSideEffectFlagsAndAccumulatePerformanceCounters(en);

		return EvaluableNodeReference::Null();
	}

	//using a single variable
	StringRef variable_sid;
	variable_sid.SetIDWithReferenceHandoff(InterpretNodeIntoStringIDValueWithReference(ocn[0], true));
	if(variable_sid == StringInternPool::NOT_A_STRING_ID)
		return EvaluableNodeReference::Null();

	//if only 2 params and not accumulating, then just assign/accum the destination
	if(num_params == 2)
	{
		auto new_value = InterpretNode(ocn[1]);

		//retrieve the symbol location
	#ifdef MULTITHREAD_SUPPORT
		//need to save variable_value_node because GetScopeStackSymbolLocationWithLock
		// may collect garbage while waiting for the lock
		//use a scope here to make it automatically destruct
		auto node_stack = CreateOpcodeStackStateSaver(new_value);

		Concurrency::SingleLock write_lock;
		auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocationWithLock(variable_sid, true, write_lock);
		if(write_lock.owns_lock())
			RecordStackLockForProfiling(en, variable_sid);

		node_stack.PopEvaluableNode();
	#else
		auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocation(variable_sid, true, false);
	#endif

		if(accum && !EvaluableNode::IsNull(*value_destination))
		{
			EvaluableNodeReference variable_value_node;
			if(is_freeable)
			{
				EvaluableNodeReference value_destination_node(*value_destination, true);
				variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, new_value, evaluableNodeManager);
			}
			else
			{
				//values should always be copied before changing, in case the value is used elsewhere, especially in another thread
				//because of the deep copy, do not need to call SetSideEffectFlagsAndAccumulatePerformanceCounters(en);
				EvaluableNodeReference value_destination_node = evaluableNodeManager->DeepAllocCopy(*value_destination);
				variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, new_value, evaluableNodeManager);
			}

			//assign the new accumulation
			*value_destination = variable_value_node;
		}
		else
		{
			if(is_freeable)
			{
				EvaluableNodeReference value_destination_node(*value_destination, true);
				evaluableNodeManager->FreeNodeTreeIfPossible(value_destination_node);
			}

			*value_destination = new_value;

			//if writing to an outer scope, can't guarantee the memory at this scope can be freed
			if(!new_value.unique || !top_of_stack)
				SetSideEffectFlagsAndAccumulatePerformanceCounters(en);
		}

		return EvaluableNodeReference::Null();
	}

	//more than 2 parameters; make a deep copy and update the portions of it
	//obtain all of the edits to make the edits transactionally at once when all are collected
	auto node_stack = CreateOpcodeStackStateSaver();
	auto &replacements = *node_stack.stack;
	size_t replacements_start_index = node_stack.originalStackSize;

	//keeps track of whether each address is unique so they can be freed if relevant
	std::vector<bool> is_value_unique;
	is_value_unique.resize(num_params - 1);
	//keeps track of whether all new values assigned or accumed are unique, cycle free, etc.
	bool result_flags_need_updates = false;

	bool any_nonunique_assignments = false;

	//get each address/value pair to replace in result
	//note the indexing is different for ocn and is_value_unique; the latter needs to subtract 1
	for(size_t ocn_index = 1; ocn_index + 1 < num_params; ocn_index += 2)
	{
		if(AreExecutionResourcesExhausted())
			return EvaluableNodeReference::Null();

		auto address = InterpretNodeForImmediateUse(ocn[ocn_index]);
		node_stack.PushEvaluableNode(address);
		is_value_unique[ocn_index - 1] = address.unique;

		auto new_value = InterpretNode(ocn[ocn_index + 1]);
		node_stack.PushEvaluableNode(new_value);
		is_value_unique[ocn_index] = new_value.unique;
	}
	size_t num_replacements = (num_params - 1) / 2;

	//retrieve the symbol location
#ifdef MULTITHREAD_SUPPORT
	//node_stack already has everything saved in case garbage collection is called in GetScopeStackSymbolLocationWithLock
	Concurrency::SingleLock write_lock;
	auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocationWithLock(variable_sid, true, write_lock);
	if(write_lock.owns_lock())
		RecordStackLockForProfiling(en, variable_sid);
#else
	auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocation(variable_sid, true, false);
#endif

	//if writing to an outer scope, can't guarantee the memory at this scope can be freed
	any_nonunique_assignments |= !top_of_stack;

	//make a copy of value_replacement because not sure where else it may be used
	EvaluableNode *value_replacement = nullptr;
	if(*value_destination == nullptr)
		value_replacement = evaluableNodeManager->AllocNode(ENT_NULL);
	else if(is_freeable)
		value_replacement = *value_destination;
	else
		value_replacement = evaluableNodeManager->DeepAllocCopy(*value_destination);

	//replace each in order, traversing as it goes along
	//this is safe because it is all on a copy, and each traversal must be done one at a time as to not
	//invalidate addresses from other traversals in case containers are expanded via reallocation of memory
	for(size_t index = 0; index < num_replacements; index++)
	{
		EvaluableNodeReference address(replacements[replacements_start_index + 2 * index], is_value_unique[2 * index]);
		EvaluableNode **copy_destination = TraverseToDestinationFromTraversalPathList(&value_replacement, address, true);
		evaluableNodeManager->FreeNodeTreeIfPossible(address);

		EvaluableNodeReference new_value(replacements[replacements_start_index + 2 * index + 1], is_value_unique[2 * index + 1]);
		if(copy_destination == nullptr)
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(new_value);
			continue;
		}

		bool need_cycle_check_before = false;
		bool is_idempotent_before = false;
		if((*copy_destination) != nullptr)
		{
			need_cycle_check_before = (*copy_destination)->GetNeedCycleCheck();
			is_idempotent_before = (*copy_destination)->GetIsIdempotent();
		}

		bool updated_node_unique = true;
		//if accum'ing into a null, then just treat as an assignment
		if(accum && !EvaluableNode::IsNull(*copy_destination))
		{
			//create destination reference; the logic above has already made a copy if it wasn't freeable
			//so the destination can be treated as unique
			EvaluableNodeReference value_destination_node(*copy_destination, true);
			EvaluableNodeReference variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, new_value, evaluableNodeManager);

			//assign the new accumulation
			*copy_destination = variable_value_node;
			updated_node_unique = variable_value_node.unique;
		}
		else
		{
			*copy_destination = new_value;
			updated_node_unique = new_value.unique;
		}

		bool need_cycle_check_after = false;
		bool is_idempotent_after = false;
		if((*copy_destination) != nullptr)
		{
			need_cycle_check_after = (*copy_destination)->GetNeedCycleCheck();
			is_idempotent_after = (*copy_destination)->GetIsIdempotent();
		}

		if(!updated_node_unique)
		{
			if(need_cycle_check_before != need_cycle_check_after || is_idempotent_before != is_idempotent_after)
				result_flags_need_updates = true;

			any_nonunique_assignments = true;
		}
	}

	if(result_flags_need_updates)
		EvaluableNodeManager::UpdateFlagsForNodeTree(value_replacement);
	*value_destination = value_replacement;

	if(any_nonunique_assignments)
		SetSideEffectFlagsAndAccumulatePerformanceCounters(en);

	return EvaluableNodeReference::Null();
}

static OpcodeInitializer _ENT_ASSIGN_IF_EQUAL(ENT_ASSIGN_IF_EQUAL, &Interpreter::InterpretNode_ENT_ASSIGN_IF_EQUAL, []() {
	OpcodeDetails d;
	d.parameters = R"(string variable * value_to_compare * value_to_assign)";
	d.returns = R"(bool)";
	d.description = R"(Compares the value in variable to value_to_compare, and if equal, assigns the variable atomically to value_to_assign.  Returns true if the value in variable is equal to value_to_compare and the assignment was successful, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((let
	{lock 0}
	(declare { success (assign_if_equal "lock" 0 1) })
	[success lock]
))&", R"([.true 1])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::POSITION;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 3.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_IF_EQUAL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//TODO 25398: implement this
	bool all_unassigned = true;
	for(auto &to_unassign : en->GetOrderedChildNodesReference())
	{
		auto string_node_to_unassign = InterpretNodeForImmediateUse(to_unassign);
		StringInternPool::StringID variable_sid = EvaluableNode::ToStringIDIfExists(string_node_to_unassign, true);

		//retrieve the symbol location
	#ifdef MULTITHREAD_SUPPORT
		//need to save variable_value_node because GetScopeStackSymbolLocationWithLock
		// may collect garbage while waiting for the lock, and it would be possible that variable_sid is removed
		//use a scope here to make it automatically destruct
		auto node_stack = CreateOpcodeStackStateSaver(string_node_to_unassign);

		Concurrency::SingleLock write_lock;
		auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocationWithLock(variable_sid, true, write_lock);
		if(write_lock.owns_lock())
			RecordStackLockForProfiling(en, variable_sid);

		node_stack.PopEvaluableNode();
	#else
		auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocation(variable_sid, true, false);
	#endif

		scope->erase(variable_sid);
	}

	return AllocReturn(all_unassigned, immediate_result);
}

static OpcodeInitializer _ENT_RETRIEVE(ENT_RETRIEVE, &Interpreter::InterpretNode_ENT_RETRIEVE, []() {
	OpcodeDetails d;
	d.parameters = R"([string|list|assoc variables])";
	d.returns = R"(any)";
	d.description = R"(If `variables` is a string, then it gets the value on the stack specified by the string.  If `variables` is a list, it returns a list of the values on the stack specified by each element of the list interpreted as a string.  If `variables` is an assoc, it returns an assoc with the indices of the assoc which was passed in with the values being the appropriate values on the stack for each index.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(assign
		{a 1}
	)
	(retrieve "a")
))&", R"(1)"},
			{R"&((seq
	(assign
		{a 1 b 2}
	)
	[
		(retrieve "a")
		(retrieve
			["a" "b"]
		)
		(retrieve
			(zip
				["a" "b"]
			)
		)
	]
))&", R"([
	1
	[@(target .true 0) 2]
	{a @(target .true 0) b @(target .true [1 1])}
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto to_lookup = InterpretNodeForImmediateUse(ocn[0]);

	//get the value(s)
	if(EvaluableNode::IsNull(to_lookup) || IsEvaluableNodeTypeImmediate(to_lookup->GetType()))
	{
		StringInternPool::StringID symbol_name_sid = EvaluableNode::ToStringIDIfExists(to_lookup, true);

		//when retrieving symbol, only need to retain the node if it's not an immediate type
		bool retain_node = !immediate_result.AnyImmediateType();
		auto [symbol_value, found] = GetScopeStackSymbol(symbol_name_sid, retain_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(to_lookup);
		return EvaluableNodeReference::CoerceNonUniqueEvaluableNodeToImmediateIfPossible(symbol_value, immediate_result);
	}
	else if(to_lookup->IsAssociativeArray())
	{
		//need to return an assoc, so see if need to make copy
		evaluableNodeManager->EnsureNodeIsModifiable(to_lookup);

		//overwrite values in the ordered
		for(auto &[cn_id, cn] : to_lookup->GetMappedChildNodesReference())
		{
			//if there are values passed in, free them to be clobbered
			EvaluableNodeReference cnr(cn, to_lookup.unique);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			bool found = false;
			std::tie(cn, found) = GetScopeStackSymbol(cn_id, true);
		}

		return EvaluableNodeReference(to_lookup, false);
	}
	else //ordered params
	{
		evaluableNodeManager->EnsureNodeIsModifiable(to_lookup);

		//overwrite values in the ordered
		for(auto &cn : to_lookup->GetOrderedChildNodesReference())
		{
			StringInternPool::StringID symbol_name_sid = EvaluableNode::ToStringIDIfExists(cn, true);
			if(symbol_name_sid == StringInternPool::NOT_A_STRING_ID)
			{
				cn = nullptr;
				continue;
			}

			auto [symbol_value, found] = GetScopeStackSymbol(symbol_name_sid, true);
			//if there are values passed in, free them to be clobbered
			EvaluableNodeReference cnr(cn, to_lookup.unique);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			cn = symbol_value;
		}

		return EvaluableNodeReference(to_lookup, false);
	}
}

static OpcodeInitializer _ENT_EXISTS(ENT_EXISTS, &Interpreter::InterpretNode_ENT_EXISTS, []() {
	OpcodeDetails d;
	d.parameters = R"(string variable)";
	d.returns = R"(bool)";
	d.description = R"(Returns true if variable exists within visibility, false if it does not.)";
	d.examples = MakeAmalgamExamples({
		{R"&((let
	{foo 1}
	[(exists "foo") (exists "bar")]
))&", R"([.true .false])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::POSITION;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_EXISTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto to_lookup = InterpretNodeForImmediateUse(ocn[0]);
	StringInternPool::StringID symbol_name_sid = EvaluableNode::ToStringIDIfExists(to_lookup, true);

	auto [symbol_value, found] = GetScopeStackSymbol(symbol_name_sid, false);

	evaluableNodeManager->FreeNodeTreeIfPossible(to_lookup);
	return AllocReturn(found, immediate_result);
}

static OpcodeInitializer _ENT_UNASSIGN(ENT_UNASSIGN, &Interpreter::InterpretNode_ENT_UNASSIGN, []() {
	OpcodeDetails d;
	d.parameters = R"(string variable1 [string variable2] ... [string variableN])";
	d.returns = R"(bool)";
	d.description = R"(Removes all variables that are parameters from the stack.  Returns true all variables previously existed and were unassigned.)";
	d.examples = MakeAmalgamExamples({
		{R"&((let
	{foo 1}
	(unassign "foo")
	(exists "foo")
))&", R"(.false)"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNASSIGN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	bool all_unassigned = true;
	for(auto &to_unassign : en->GetOrderedChildNodesReference())
	{
		auto string_node_to_unassign = InterpretNodeForImmediateUse(to_unassign);
		StringInternPool::StringID variable_sid = EvaluableNode::ToStringIDIfExists(string_node_to_unassign, true);

		//retrieve the symbol location
	#ifdef MULTITHREAD_SUPPORT
		//need to save variable_value_node because GetScopeStackSymbolLocationWithLock
		// may collect garbage while waiting for the lock, and it would be possible that variable_sid is removed
		//use a scope here to make it automatically destruct
		auto node_stack = CreateOpcodeStackStateSaver(string_node_to_unassign);

		Concurrency::SingleLock write_lock;
		auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocationWithLock(variable_sid, true, write_lock);
		if(write_lock.owns_lock())
			RecordStackLockForProfiling(en, variable_sid);

		node_stack.PopEvaluableNode();
	#else
		auto [value_destination, scope, top_of_stack, is_freeable] = GetScopeStackSymbolLocation(variable_sid, true, false);
	#endif

		scope->erase(variable_sid);
	}

	return AllocReturn(all_unassigned, immediate_result);
}

static OpcodeInitializer _ENT_TARGET(ENT_TARGET, &Interpreter::InterpretNode_ENT_TARGET, []() {
	OpcodeDetails d;
	d.parameters = R"([number|bool stack_distance] [number|string|list walk_path])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the node being created, referenced by the parameters by target.  Useful for serializing graph data structures or looking up data during iteration.  If `stack_distance` is a number, it climbs back up the target stack that many levels.  If `stack_distance` is a boolean, then `.true` indicates the top of the stack and `.false` indicates the bottom.  If `walk_path` is specified, it will walk from the node at `stack_distance` to the corresponding target.  If building an object, specifying `stack_distance` to true is often useful for accessing or traversing the top-level elements.)";
	d.examples = MakeAmalgamExamples({
		{R"&([
	1
	2
	3
	(target 0 1)
	4
])&", R"([1 2 3 @(target .true 1) 4])"},
			{R"&([
	0
	1
	2
	3
	(+
		(target 0 1)
	)
	4
])&", R"([0 1 2 3 1 4])"},
			{R"&([
	0
	1
	2
	3
	[
		0
		1
		2
		3
		(+
			(target 1 1)
		)
		4
	]
])&", R"([
	0
	1
	2
	3
	[0 1 2 3 1 4]
])"},
			{R"&({
	a 0
	b 1
	c 2
	d 3
	e [
			0
			1
			2
			3
			(+
				(target 1 "a")
			)
			4
		]
})&", R"({
	a 0
	b 1
	c 2
	d 3
	e [0 1 2 3 0 4]
})"},
			{R"&((call_sandboxed {
	a 0
	b 1
	c 2
	d 3
	e [
			[
				0
				1
				2
				3
				(+
					(target .true "a")
				)
				4
			]
		]
}))&", R"({
	a 0
	b 1
	c 2
	d 3
	e [
			[0 1 2 3 0 4]
		]
})"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TARGET(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	size_t depth = 0;
	if(ocn.size() > 0)
	{
		auto result = InterpretNodeForImmediateUse(ocn[0], true);
		if(result.IsImmediateValue())
		{
			double value_number = result.GetValue().GetValueAsNumber();
			evaluableNodeManager->FreeNodeIfPossible(result);

			if(value_number >= 0)
				depth = static_cast<size_t>(value_number);
			else if(!FastIsNaN(value_number)) //null/nan should leave depth as 0, any negative value is an error
				return EvaluableNodeReference::Null();
		}
		else
		{
			auto node_type = ENT_NULL;
			EvaluableNode *result_node = result.GetReference();
			if(result_node != nullptr)
				node_type = result_node->GetType();

			if(node_type == ENT_NUMBER)
			{
				double value_number = result->GetNumberValueReference();
				if(value_number >= 0)
					depth = static_cast<size_t>(value_number);
				else if(!FastIsNaN(value_number)) //null/nan should leave depth as 0, any negative value is an error
					return EvaluableNodeReference::Null();
			}
			else if(node_type == ENT_BOOL && result->GetBoolValueReference())
			{
				//select the top of the stack
				depth = constructionStack.size() - 1;
			}
			else if(node_type != ENT_BOOL)
			{
				return EvaluableNodeReference::Null();
			}
		}
	}

	//make sure have a large enough stack
	if(depth >= constructionStack.size())
		return EvaluableNodeReference::Null();

	size_t offset = constructionStack.size() - 1 - depth;

	if(ocn.size() > 1)
	{
		//if there's a second parameter, try to look up the walk path
		EvaluableNode **target = InterpretNodeIntoDestination(&constructionStack[offset].target, ocn[1], false);
		if(target == nullptr)
			return EvaluableNodeReference::Null();

		return EvaluableNodeReference(*target, false);
	}

	return EvaluableNodeReference(constructionStack[offset].target, false);
}

static OpcodeInitializer _ENT_STACK(ENT_STACK, &Interpreter::InterpretNode_ENT_STACK, []() {
	OpcodeDetails d;
	d.parameters = R"( )";
	d.returns = R"(list of assoc)";
	d.description = R"(Evaluates to the current execution context, also known as the scope stack, containing all of the variables for each layer of the stack.)";
	d.examples = MakeAmalgamExamples({
		{R"&((stack))&", R"([{}])"},
		{R"&((call
	(lambda
		(let
			{a 1}
			(stack)
		)
	)
	{x 1}
))&", R"([
	{}
	{x 1}
	{a 1}
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_STACK(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//make a copy in case anything is changed in the future
	return EvaluableNodeReference(MakeCopyOfScopeStack(), true);
}

static OpcodeInitializer _ENT_ARGS(ENT_ARGS, &Interpreter::InterpretNode_ENT_ARGS, []() {
	OpcodeDetails d;
	d.parameters = R"([number stack_distance])";
	d.returns = R"(assoc)";
	d.description = R"(Evaluates to the top context of the stack, the current execution context, or scope stack, known as the arguments.  If `stack_distance` is specified, then it evaluates to the context that many layers up the stack.)";
	d.examples = MakeAmalgamExamples({
		{R"&((call
	(lambda
		(let
			(associate "bbb" 3)
			[
				(args)
				(args 1)
			]
		)
	)
	{x 1}
))&", R"([
	{bbb 3}
	{x 1}
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ARGS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	size_t depth = 0;
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() > 0)
	{
		double value = InterpretNodeIntoNumberValue(ocn[0]);
		depth = static_cast<size_t>(value);
	}

	EvaluableNode *arg_node = GetScopeStackGivenDepth(depth);
	if(arg_node == nullptr)
		return EvaluableNodeReference::Null();

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(arg_node), false, true);
}

static OpcodeInitializer _ENT_GET_TYPE(ENT_GET_TYPE, &Interpreter::InterpretNode_ENT_GET_TYPE, []() {
	OpcodeDetails d;
	d.parameters = R"(* node)";
	d.returns = R"(any)";
	d.description = R"(Returns a node of the type corresponding to the node.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get_type
	(lambda
		(+ 3 4)
	)
))&", R"((+))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_TYPE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNodeForImmediateUse(ocn[0]);
	EvaluableNodeType type = ENT_NULL;
	if(cur != nullptr)
		type = cur->GetType();
	evaluableNodeManager->FreeNodeTreeIfPossible(cur);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(type), true);
}

static OpcodeInitializer _ENT_GET_TYPE_STRING(ENT_GET_TYPE_STRING, &Interpreter::InterpretNode_ENT_GET_TYPE_STRING, []() {
	OpcodeDetails d;
	d.parameters = R"(* node)";
	d.returns = R"(string)";
	d.description = R"(Returns a string that represents the type corresponding to the node.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get_type_string
	(lambda
		(+ 3 4)
	)
))&", R"("+")"},
			{R"&((get_type_string "hello"))&", R"("string")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_TYPE_STRING(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNodeForImmediateUse(ocn[0]);
	EvaluableNodeType type = ENT_NULL;
	if(cur != nullptr)
		type = cur->GetType();
	evaluableNodeManager->FreeNodeTreeIfPossible(cur);

	std::string type_string = GetStringFromEvaluableNodeType(type, true);
	return AllocReturn(type_string, immediate_result);
}

static OpcodeInitializer _ENT_SET_TYPE(ENT_SET_TYPE, &Interpreter::InterpretNode_ENT_SET_TYPE, []() {
	OpcodeDetails d;
	d.parameters = R"(* node [string|* type])";
	d.returns = R"(any)";
	d.description = R"(Creates a copy of `node`, setting the type of the node of to `type`.  If `type` is a string, it will look that up as the type, or if `type` is a node that is not a string, it will set the type to match the top node of `type`.  It will convert opcode parameters as necessary.)";
	d.examples = MakeAmalgamExamples({
		{R"&((set_type
	(lambda
		(+ 3 4)
	)
	"-"
))&", R"((- 3 4))"},
			{R"&((sort
	(set_type
		(associate "a" 4 "b" 3)
		"list"
	)
))&", R"([3 4 "a" "b"])"},
			{R"&((sort
	(set_type
		(associate "a" 4 "b" 3)
		[]
	)
))&", R"([3 4 "a" "b"])"},
			{R"&((unparse
	(set_type
		["a" 4 "b" 3]
		"assoc"
	)
))&", R"("{a 4 b 3}")"},
			{R"&((call
	(set_type
		[1 0.5 "3.2" 4]
		"+"
	)
))&", R"(8.7)"},
			{R"&((set_type
	[
		(set_annotations
			(lambda
				(+ 3 4)
			)
			"react"
		)
	]
	"unordered_list"
))&", R"((unordered_list
	
	#react
	(+ 3 4)
))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_TYPE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//get the target
	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);

	evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the type to set
	EvaluableNodeType new_type = ENT_NULL;
	auto type_node = InterpretNodeForImmediateUse(ocn[1]);
	if(type_node != nullptr)
	{
		if(type_node->GetType() == ENT_STRING)
		{
			StringInternPool::StringID sid = type_node->GetStringID();
			new_type = GetEvaluableNodeTypeFromStringId(sid);
		}
		else
			new_type = type_node->GetType();
	}
	evaluableNodeManager->FreeNodeTreeIfPossible(type_node);

	if(new_type == ENT_NOT_A_BUILT_IN_TYPE)
		new_type = ENT_NULL;

	source->SetType(new_type, evaluableNodeManager, true);

	return source;
}

static OpcodeInitializer _ENT_FORMAT(ENT_FORMAT, &Interpreter::InterpretNode_ENT_FORMAT, []() {
	OpcodeDetails d;
	d.parameters = R"(* data string from_format string to_format [assoc from_params] [assoc to_params])";
	d.returns = R"(any)";
	d.description = R"(Converts data from `from_format` into `to_format`.  Supported language types are "number", "string", and "code", where code represents everything beyond number and string.  Beyond the supported language types, additional formats that are stored in a binary string.  The additional formats are "base16", "base64", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float32", "float64", ">int8", ">uint8", ">int16", ">uint16", ">int32", ">uint32", ">int64", ">uint64", ">float32", ">float64", "<int8", "<uint8", "<int16", "<uint16", "<int32", "<uint32", "<int64", "<uint64", "<float32", "<float64", "json", "yaml", "date", and "time" (though date and time are special cases).  Binary types starting with a "<" represent little endian, binary types starting with a ">" represent big endian, and binary types without either will be the endianness of the machine.  Binary types will be handled as strings.  The "date" type requires additional information.  Following "date" or "time" is a colon, followed by a standard strftime date or time format string.  If `from_params` or `to_params` are specified, then it will apply the appropriate from or to as appropriate.  If the format is either "string", "json", or "yaml", then the key "sort_keys" can be used to specify a boolean value, if true, then it will sort the keys, otherwise the default behavior is to emit the keys based on memory layout.  If the format is date or time, then the to or from params can be an assoc with "locale" as an optional key.  If date then "time_zone" is also allowed.  The locale is provided, then it will leverage operating system support to apply appropriate formatting, such as en_US.  Note that UTF-8 is assumed and automatically added to the locale.  If no locale is specified, then the default will be used.  If converting to or from dates, if "time_zone" is specified, it will use the standard time_zone name, if unspecified or empty string, it will assume the current time zone.)";
	d.examples = MakeAmalgamExamples({
		{R"&((map
	(lambda
		(format (current_value) "int8" "number")
	)
	(explode "abcdefg" 1)
))&", R"([
	97
	98
	99
	100
	101
	102
	103
])"},
			{R"&((format 65 "number" "int8"))&", R"("A")"},
			{R"&((format
	(format -100 "number" "float64")
	"float64"
	"number"
))&", R"(-100)"},
			{R"&((format
	(format -100 "number" "float32")
	"float32"
	"number"
))&", R"(-100)"},
			{R"&((format
	(format 100 "number" "uint32")
	"uint32"
	"number"
))&", R"(100)"},
			{R"&((format
	(format 123456789 "number" ">uint32")
	"<uint32"
	"number"
))&", R"(365779719)"},
			{R"&((format
	(format 123456789 "number" ">uint32")
	">uint32"
	"number"
))&", R"(123456789)"},
			{R"&((format
	(format 14294967296 "number" "uint64")
	"uint64"
	"number"
))&", R"(14294967296)"},
			{R"&((format "A" "int8" "number"))&", R"(65)"},
			{R"&((format -100 "float32" "number"))&", R"(6.409830999309918e-10)"},
			{R"&((format 65 "uint8" "string"))&", R"("54")"},
			{R"&((format 254 "uint8" "base16"))&", R"("32")"},
			{R"&((format "AAA" "string" "base16"))&", R"("414141")"},
			{R"&((format "414141" "base16" "string"))&", R"("AAA")"},
			{R"&((format "Many hands make light work." "string" "base64"))&", R"("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu")"},
			{R"&((format "Many hands make light work.." "string" "base64"))&", R"("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLg==")"},
			{R"&((format "Many hands make light work..." "string" "base64"))&", R"("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLi4=")"},
			{R"&((format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu" "base64" "string"))&", R"("Many hands make light work.")"},
			{R"&((format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLg==" "base64" "string"))&", R"("Many hands make light work..")"},
			{R"&((format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLi4=" "base64" "string"))&", R"("Many hands make light work...")"},
			{R"&((format "[{\"a\" : 3, \"b\" : 4}, {\"c\" : \"c\"}]" "json" "code"))&", R"([
	{a 3 b 4}
	{c "c"}
])"},
			{R"&((format
	[
		{a 3 b 4}
		{c "c" d .null}
	]
	"code"
	"json"
	.null
	{sort_keys .true}
))&", R"("[{\"a\":3,\"b\":4},{\"c\":\"c\",\"d\":null}]")"},
			{R"&((format
	{
		a 1
		b 2
		c 3
		d 4
		e ["a" "b" .null .infinity]
	}
	"code"
	"yaml"
	.null
	{sort_keys .true}
))&", R"("a: 1\nb: 2\nc: 3\nd: 4\ne:\n  - a\n  - b\n  - \n  - .inf\n")"},
			{R"&((format "a: 1" "yaml" "code"))&", R"({a 1})"},
			{R"&((format 1591503779.1 "number" "date:%Y-%m-%d-%H.%M.%S"))&", R"("2020-06-07-00.22.59.1000000")", R"("2020-06-07-00.22.59.100+")"},
			{R"&((format 1591503779 "number" "date:%F %T"))&", R"("2020-06-07 00:22:59")", R"("2020-06-07 \d\d:22:59")"},
			{R"&((format "Feb 2014" "date:%b %Y" "number"))&", R"(1391230800)", R"(139\d+)"},
			{R"&((format "2014-Feb" "date:%Y-%h" "number"))&", R"(1391230800)", R"(139\d+)"},
			{R"&((format "02/2014" "date:%m/%Y" "number"))&", R"(1391230800)", R"(139\d+)"},
			{R"&((format 1591505665002 "number" "date:%F %T"))&", R"("-6053-05-28 00:24:29")", R"(".*?-\d\d-\d\d \d\d:\d\d:\d\d")"},
			{R"&((format 1591330905 "number" "date:%F %T"))&", R"("2020-06-05 00:21:45")", R"("2020-06-0\d \d\d:21:45")"},
			{R"&((format 1591330905 "number" "date:%c %Z"))&", R"("06/05/20 00:21:45 EDT")", R"("06/05/\d\d \d\d:21:45 \w+")"},
			{R"&((format 1591330905 "number" "date:%S"))&", R"("45")"},
			{R"&((format 1591330905 "number" "date:%Oe"))&", R"(" 5")"},
			{R"&((format 1591330905 "number" "date:%s"))&", R"(" s")"},
			{R"&((format 1591330905 "number" "date:%s%"))&", R"(" s")"},
			{R"&((format 1591330905 "number" "date:%a%b%c%d%e%f"))&", R"("FriJun06/05/20 00:21:4505 5 f")", ".+f"},
			{R"&((format
	"abcd"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "es_ES"}
))&", R"("jueves, ene. 01, 1970")", R"("jueves, ene.* 01, 1970")" },
			{R"&((format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "etete123"}
))&", R"("Sunday, Jun 07, 2020")"},
			{R"&((format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "notalocale"}
	{locale "es_ES"}
))&", R"("domingo, jun. 07, 2020")", R"("domingo, jun.* 07, 2020")" },
			{R"&((format "2020-06-07" "date:%Y-%m-%d" "number"))&", R"(1591502400)", R"(159\d+)" },
			{R"&((format "2020-06-07" "date:%Y-%m-%d" "date:%b %d, %Y"))&", R"("Jun 07, 2020")"},
			{R"&((format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "es_ES"}
))&", R"("domingo, jun. 07, 2020")", R"("domingo, jun.* 07, 2020")" },
			{R"&((format "1970-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number"))&", R"(664428)", R"(6\d+)" },
			{R"&((format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number"))&", R"(-314954772)", R"(-3149\d+)" },
			{R"&((format
	(format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
	"number"
	"date:%Y-%m-%d %H.%M.%S"
))&", R"("1960-01-08 11.33.48")"},
			{R"&((format
	(+
		0.01
		(format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
	)
	"number"
	"date:%Y-%m-%d %H.%M.%S"
))&", R"("1960-01-08 11.33.48.0100000")", R"("1960-01-08 11.33.48.010+")" },
			{R"&((format "13:22:44" "time:%H:%M:%S" "number"))&", R"(48164)"},
			{R"&((format
	"13:22:44"
	"time:%H:%M:%S"
	"number"
	{locale "en_US"}
))&", R"(48164)"},
			{R"&((format "10:22:44" "time:%H:%M:%S" "number"))&", R"(37364)"},
			{R"&((format "10:22:44am" "time:%I:%M:%S%p" "number"))&", R"(37364)", R"(\d+)" },
			{R"&((format "10:22:44.33am" "time:%I:%M:%S%p" "number"))&", R"(37364.33)", R"(\d+)"},
			{R"&((format "10:22:44" "time:%I:%M:%S" "number"))&", R"(0)"},
			{R"&((format "10:22:44" "time:%qqq:%qqq:%qqq" "number"))&", R"(0)"},
			{R"&((format
	"13:22:44"
	"time:%H:%M:%S"
	"number"
	{locale "notalocale"}
))&", R"(48164)"},
			{R"&((format 48164 "number" "time:%H:%M:%S"))&", R"("13:22:44")"},
			{R"&((format
	48164
	"number"
	"time:%I:%M:%S%p"
	.null
	{locale "es_ES"}
))&", R"("01:22:44PM")"},
			{R"&((format 37364.33 "number" "time:%I:%M:%S%p"))&", R"("10:22:44.3300000AM")", R"("10:22:44.330+AM")" },
			{R"&((format 0 "number" "time:%I:%M:%S%p"))&", R"("12:00:00AM")"},
			{R"&((format .null "number" "time:%I:%M:%S%p"))&", R"("12:00:00AM")"},
			{R"&((format .infinity "number" "time:%I:%M:%S%p"))&", R"("12:00:00AM")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 2.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

//reinterprets a char value to DestinationType
template<typename DestinationType, typename SourceType = uint8_t>
constexpr DestinationType ExpandCharStorage(char &value)
{
	return static_cast<DestinationType>(reinterpret_cast<SourceType &>(value));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_FORMAT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 3)
		return EvaluableNodeReference::Null();

	StringRef from_type, to_type;
	from_type.SetIDWithReferenceHandoff(InterpretNodeIntoStringIDValueWithReference(ocn[1]));
	to_type.SetIDWithReferenceHandoff(InterpretNodeIntoStringIDValueWithReference(ocn[2]));

	auto node_stack = CreateOpcodeStackStateSaver();
	bool node_stack_needs_popping = false;

	EvaluableNodeReference from_params = EvaluableNodeReference::Null();
	if(ocn.size() > 3)
	{
		from_params = InterpretNodeForImmediateUse(ocn[3]);
		node_stack.PushEvaluableNode(from_params);
		node_stack_needs_popping = true;
	}

	bool use_code = false;
	EvaluableNodeReference code_value = EvaluableNodeReference::Null();

	bool use_number = false;
	double number_value = 0;

	bool use_uint_number = false;
	uint64_t uint_number_value = 0;

	bool use_int_number = false;
	int64_t int_number_value = 0;

	bool use_string = false;
	std::string string_value = "";
	bool valid_string_value = true;

	const std::string date_string("date:");
	const std::string time_string("time:");

	//TODO: when moving to C++20, can change to use std::endian::native
	static bool big_endian = (*reinterpret_cast<char *>(new int32_t(0x12345678)) == 0x12);

	if(from_type == GetStringIdFromNodeType(ENT_NUMBER))
	{
		use_number = true;
		number_value = InterpretNodeIntoNumberValue(ocn[0]);
	}
	else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_code))
	{
		use_code = true;
		code_value = InterpretNodeForImmediateUse(ocn[0]);
	}
	else //base on string type
	{
		string_value = InterpretNodeIntoStringValueEmptyNull(ocn[0]);

		if(from_type == GetStringIdFromNodeType(ENT_STRING))
		{
			use_string = true;
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_base16))
		{
			use_string = true;
			string_value = StringManipulation::Base16ToBinaryString(string_value);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_base64))
		{
			use_string = true;
			string_value = StringManipulation::Base64ToBinaryString(string_value);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_uint8)
			|| from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint8)
			|| from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint8))
		{
			use_uint_number = true;
			uint_number_value = reinterpret_cast<uint8_t &>(string_value[0]);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_int8)
			|| from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int8)
			|| from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int8))
		{
			use_int_number = true;
			int_number_value = ExpandCharStorage<int64_t, int8_t>(string_value[0]);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint16)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_uint16)))
		{
			use_uint_number = true;
			if(string_value.size() >= 2)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[0]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint16)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_uint16)))
		{
			use_uint_number = true;
			if(string_value.size() >= 2)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[1]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int16)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_int16)))
		{
			use_int_number = true;
			if(string_value.size() >= 2) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[0]) | (ExpandCharStorage<int64_t, int8_t>(string_value[1]) << 8);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int16)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_int16)))
		{
			use_int_number = true;
			if(string_value.size() >= 2) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[1]) | (ExpandCharStorage<int64_t, int8_t>(string_value[0]) << 8);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint32)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_uint32)))
		{
			use_uint_number = true;
			if(string_value.size() >= 4)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[0]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8)
				| (ExpandCharStorage<uint64_t>(string_value[2]) << 16) | (ExpandCharStorage<uint64_t>(string_value[3]) << 24);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint32)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_uint32)))
		{
			use_uint_number = true;
			if(string_value.size() >= 4)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[3]) | (ExpandCharStorage<uint64_t>(string_value[2]) << 8)
				| (ExpandCharStorage<uint64_t>(string_value[1]) << 16) | (ExpandCharStorage<uint64_t>(string_value[0]) << 24);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int32)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_int32)))
		{
			use_int_number = true;
			if(string_value.size() >= 4) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[0]) | (ExpandCharStorage<int64_t>(string_value[1]) << 8)
				| (ExpandCharStorage<int64_t>(string_value[2]) << 16) | (ExpandCharStorage<int64_t, int8_t>(string_value[3]) << 24);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int32)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_int32)))
		{
			use_int_number = true;
			if(string_value.size() >= 4) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[3]) | (ExpandCharStorage<int64_t>(string_value[2]) << 8)
				| (ExpandCharStorage<int64_t>(string_value[1]) << 16) | (ExpandCharStorage<int64_t, int8_t>(string_value[0]) << 24);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint64)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_uint64)))
		{
			use_uint_number = true;
			if(string_value.size() >= 8)
				uint_number_value =
				ExpandCharStorage<uint64_t>(string_value[0]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8)
				| (ExpandCharStorage<uint64_t>(string_value[2]) << 16) | (ExpandCharStorage<uint64_t>(string_value[3]) << 24)
				| (ExpandCharStorage<uint64_t>(string_value[4]) << 32) | (ExpandCharStorage<uint64_t>(string_value[5]) << 40)
				| (ExpandCharStorage<uint64_t>(string_value[6]) << 48) | (ExpandCharStorage<uint64_t>(string_value[7]) << 56);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint64)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_uint64)))
		{
			use_uint_number = true;
			if(string_value.size() >= 8)
				uint_number_value =
				ExpandCharStorage<uint64_t>(string_value[7]) | (ExpandCharStorage<uint64_t>(string_value[6]) << 8)
				| (ExpandCharStorage<uint64_t>(string_value[5]) << 16) | (ExpandCharStorage<uint64_t>(string_value[4]) << 24)
				| (ExpandCharStorage<uint64_t>(string_value[3]) << 32) | (ExpandCharStorage<uint64_t>(string_value[2]) << 40)
				| (ExpandCharStorage<uint64_t>(string_value[1]) << 48) | (ExpandCharStorage<uint64_t>(string_value[0]) << 56);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int64)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_int64)))
		{
			use_int_number = true;
			if(string_value.size() >= 8)
			{
				uint_number_value =
					ExpandCharStorage<int64_t>(string_value[0]) | (ExpandCharStorage<int64_t>(string_value[1]) << 8)
					| (ExpandCharStorage<int64_t>(string_value[2]) << 16) | (ExpandCharStorage<int64_t>(string_value[3]) << 24)
					| (ExpandCharStorage<int64_t>(string_value[4]) << 32) | (ExpandCharStorage<int64_t>(string_value[5]) << 40)
					| (ExpandCharStorage<int64_t>(string_value[6]) << 48) | (ExpandCharStorage<int64_t>(string_value[7]) << 56);
				int_number_value = reinterpret_cast<int64_t &>(uint_number_value);
			}
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int64)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_int64)))
		{
			use_int_number = true;
			if(string_value.size() >= 8)
			{
				uint_number_value =
					ExpandCharStorage<int64_t>(string_value[7]) | (ExpandCharStorage<int64_t>(string_value[6]) << 8)
					| (ExpandCharStorage<int64_t>(string_value[5]) << 16) | (ExpandCharStorage<int64_t>(string_value[4]) << 24)
					| (ExpandCharStorage<int64_t>(string_value[3]) << 32) | (ExpandCharStorage<int64_t>(string_value[2]) << 40)
					| (ExpandCharStorage<int64_t>(string_value[1]) << 48) | (ExpandCharStorage<int64_t>(string_value[0]) << 56);
				int_number_value = reinterpret_cast<int64_t &>(uint_number_value);
			}
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_float32)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_float32)))
		{
			use_number = true;
			if(string_value.size() >= 4)
			{
				uint32_t temp =
					ExpandCharStorage<uint32_t>(string_value[0]) | (ExpandCharStorage<uint32_t>(string_value[1]) << 8)
					| (ExpandCharStorage<uint32_t>(string_value[2]) << 16) | (ExpandCharStorage<uint32_t>(string_value[3]) << 24);
				number_value = reinterpret_cast<float &>(temp);
			}
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_float32)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_float32)))
		{
			use_number = true;
			if(string_value.size() >= 4)
			{
				uint32_t temp =
					ExpandCharStorage<uint32_t>(string_value[3]) | (ExpandCharStorage<uint32_t>(string_value[2]) << 8)
					| (ExpandCharStorage<uint32_t>(string_value[1]) << 16) | (ExpandCharStorage<uint32_t>(string_value[0]) << 24);
				number_value = reinterpret_cast<float &>(temp);
			}
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_lt_float64)
			|| (!big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_float64)))
		{
			use_number = true;
			if(string_value.size() >= 8)
			{
				uint_number_value =
					ExpandCharStorage<uint64_t>(string_value[0]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8)
					| (ExpandCharStorage<uint64_t>(string_value[2]) << 16) | (ExpandCharStorage<uint64_t>(string_value[3]) << 24)
					| (ExpandCharStorage<uint64_t>(string_value[4]) << 32) | (ExpandCharStorage<uint64_t>(string_value[5]) << 40)
					| (ExpandCharStorage<uint64_t>(string_value[6]) << 48) | (ExpandCharStorage<uint64_t>(string_value[7]) << 56);
				number_value = reinterpret_cast<double &>(uint_number_value);
			}
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_gt_float64)
			|| (big_endian && from_type == GetStringIdFromBuiltInStringId(ENBISI_float64)))
		{
			use_number = true;
			if(string_value.size() >= 8)
			{
				uint_number_value =
					ExpandCharStorage<uint64_t>(string_value[7]) | (ExpandCharStorage<uint64_t>(string_value[6]) << 8)
					| (ExpandCharStorage<uint64_t>(string_value[5]) << 16) | (ExpandCharStorage<uint64_t>(string_value[4]) << 24)
					| (ExpandCharStorage<uint64_t>(string_value[3]) << 32) | (ExpandCharStorage<uint64_t>(string_value[2]) << 40)
					| (ExpandCharStorage<uint64_t>(string_value[1]) << 48) | (ExpandCharStorage<uint64_t>(string_value[0]) << 56);
				number_value = reinterpret_cast<double &>(uint_number_value);
			}
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_json))
		{
			use_code = true;
			code_value = EvaluableNodeReference(EvaluableNodeJSONTranslation::JsonToEvaluableNode(evaluableNodeManager, string_value), true);
		}
		else if(from_type == GetStringIdFromBuiltInStringId(ENBISI_yaml))
		{
			use_code = true;
			code_value = EvaluableNodeReference(EvaluableNodeYAMLTranslation::YamlToEvaluableNode(evaluableNodeManager, string_value), true);
		}
		else //need to parse the string
		{
			auto &from_type_str = string_intern_pool.GetStringFromID(from_type);

			//see if it starts with the date or time string
			if(from_type_str.compare(0, date_string.size(), date_string) == 0)
			{
				std::string locale;
				std::string timezone;
				if(EvaluableNode::IsAssociativeArray(from_params))
				{
					auto &mcn = from_params->GetMappedChildNodesReference();
					EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_locale, locale);
					EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_time_zone, timezone);
				}

				use_number = true;
				number_value = GetNumSecondsSinceEpochFromDateTimeString(string_value, from_type_str.c_str() + date_string.size(), locale, timezone);
			}
			else if(from_type_str.compare(0, time_string.size(), time_string) == 0)
			{
				std::string locale;
				if(EvaluableNode::IsAssociativeArray(from_params))
				{
					auto &mcn = from_params->GetMappedChildNodesReference();
					EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_locale, locale);
				}

				use_number = true;
				number_value = GetNumSecondsSinceMidnight(string_value, from_type_str.c_str() + time_string.size(), locale);

			}
		}
	}

	//have everything from from_type, so no longer need the reference
	if(node_stack_needs_popping)
		node_stack.PopEvaluableNode();
	evaluableNodeManager->FreeNodeTreeIfPossible(from_params);

	EvaluableNodeReference to_params = EvaluableNodeReference::Null();
	if(ocn.size() > 4)
		to_params = InterpretNodeForImmediateUse(ocn[4]);

	//convert
	if(to_type == GetStringIdFromNodeType(ENT_NUMBER))
	{
		//don't need to do anything if use_number
		if(use_uint_number)
			number_value = static_cast<double>(uint_number_value);
		else if(use_int_number)
			number_value = static_cast<double>(int_number_value);
		else if(use_string)
		{
			auto [converted_value, success] = Platform_StringToNumber(string_value);
			if(success)
				number_value = converted_value;
		}
		else if(use_code)
			number_value = EvaluableNode::ToNumber(code_value);

		evaluableNodeManager->FreeNodeTreeIfPossible(to_params);
		evaluableNodeManager->FreeNodeTreeIfPossible(code_value);
		return AllocReturn(number_value, immediate_result);
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_code))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(to_params);
		return code_value;
	}
	else if(to_type == GetStringIdFromNodeType(ENT_STRING))
	{
		//don't need to do anything if use_string
		if(use_number)
			string_value = EvaluableNode::NumberToString(number_value);
		else if(use_uint_number)
			string_value = EvaluableNode::NumberToString(static_cast<size_t>(uint_number_value));
		else if(use_int_number)
			string_value = EvaluableNode::NumberToString(static_cast<double>(int_number_value));
		else if(use_code)
		{
			bool sort_keys = false;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();
				EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_sort_keys, sort_keys);
			}

			string_value = Parser::Unparse(code_value, false, false, sort_keys);
		}
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_base16) || to_type == GetStringIdFromBuiltInStringId(ENBISI_base64))
	{
		if(use_number)
		{
			string_value = StringManipulation::To8ByteStringLittleEndian(number_value);
		}
		else if(use_int_number)
		{
			if(int_number_value >= std::numeric_limits<int8_t>::min()
					&& int_number_value <= std::numeric_limits<int8_t>::max())
				string_value = StringManipulation::To1ByteString(static_cast<int8_t>(int_number_value));
			else if(int_number_value >= std::numeric_limits<int16_t>::min()
					&& int_number_value <= std::numeric_limits<int16_t>::max())
				string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(int_number_value));
			else if(int_number_value >= std::numeric_limits<int32_t>::min()
					&& int_number_value <= std::numeric_limits<int32_t>::max())
				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(int_number_value));
			else
				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(int_number_value));
		}
		else if(use_uint_number)
		{
			if(uint_number_value <= std::numeric_limits<uint8_t>::max())
				string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(uint_number_value));
			else if(uint_number_value <= std::numeric_limits<uint16_t>::max())
				string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(uint_number_value));
			else if(uint_number_value <= std::numeric_limits<uint32_t>::max())
				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(uint_number_value));
			else
				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(uint_number_value));
		}
		//else use_string or use_code

		//if using code, just reuse string value
		if(use_code)
			string_value = Parser::Unparse(code_value, false, false, true);

		if(to_type == GetStringIdFromBuiltInStringId(ENBISI_base16))
			string_value = StringManipulation::BinaryStringToBase16(string_value);
		else //Base64
			string_value = StringManipulation::BinaryStringToBase64(string_value);
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_uint8)
		|| to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint8)
		|| to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint8))
	{
		if(use_number)				string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_int8)
		|| to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int8)
		|| to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int8))
	{
		if(use_number)				string_value = StringManipulation::To1ByteString(static_cast<int8_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To1ByteString(static_cast<int8_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To1ByteString(static_cast<int8_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To1ByteString(static_cast<int8_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint16)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_uint16)))
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint16)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_uint16)))
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int16)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_int16)))
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int16)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_int16)))
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint32)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_uint32)))
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint32)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_uint32)))
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int32)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_int32)))
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int32)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_int32)))
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_uint64)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_uint64)))
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_uint64)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_uint64)))
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(EvaluableNode::ToNumber(code_value)));

	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_int64)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_int64)))
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_int64)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_uint64)))
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_float32)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_float32)))
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_float32)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_float32)))
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_lt_float64)
		|| (!big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_float64)))
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_gt_float64)
		|| (big_endian && to_type == GetStringIdFromBuiltInStringId(ENBISI_float64)))
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_json))
	{
		if(use_number)
			string_value = EvaluableNode::NumberToString(number_value);
		else if(use_uint_number)
			string_value = EvaluableNode::NumberToString(static_cast<size_t>(uint_number_value));
		else if(use_int_number)
			string_value = EvaluableNode::NumberToString(static_cast<double>(int_number_value));
		else if(use_string)
		{
			EvaluableNode en_str(ENT_STRING, string_value);
			std::tie(string_value, valid_string_value) = EvaluableNodeJSONTranslation::EvaluableNodeToJson(&en_str);
		}
		else if(use_code)
		{
			bool sort_keys = false;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();
				EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_sort_keys, sort_keys);
			}

			std::tie(string_value, valid_string_value) = EvaluableNodeJSONTranslation::EvaluableNodeToJson(code_value, sort_keys);
		}
	}
	else if(to_type == GetStringIdFromBuiltInStringId(ENBISI_yaml))
	{
		if(use_number)
		{
			EvaluableNode value(number_value);
			std::tie(string_value, valid_string_value) = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&value);
		}
		else if(use_uint_number)
		{
			EvaluableNode value(static_cast<double>(uint_number_value));
			std::tie(string_value, valid_string_value) = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&value);
		}
		else if(use_int_number)
		{
			EvaluableNode value(static_cast<double>(int_number_value));
			std::tie(string_value, valid_string_value) = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&value);
		}
		else if(use_string)
		{
			EvaluableNode en_str(ENT_STRING, string_value);
			std::tie(string_value, valid_string_value) = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&en_str);
		}
		else if(use_code)
		{
			bool sort_keys = false;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();
				EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_sort_keys, sort_keys);
			}

			std::tie(string_value, valid_string_value) = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(code_value, sort_keys);
		}
	}
	else //need to parse the string
	{
		auto &to_type_str = string_intern_pool.GetStringFromID(to_type);

		//if it starts with the date or time string
		if(to_type_str.compare(0, date_string.size(), date_string) == 0)
		{
			std::string locale;
			std::string timezone;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();
				EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_locale, locale);
				EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_time_zone, timezone);
			}

			double num_secs_from_epoch = 0.0;
			if(use_number)				num_secs_from_epoch = number_value;
			else if(use_uint_number)	num_secs_from_epoch = static_cast<double>(uint_number_value);
			else if(use_int_number)		num_secs_from_epoch = static_cast<double>(int_number_value);
			else if(use_code)			num_secs_from_epoch = static_cast<double>(EvaluableNode::ToNumber(code_value));

			string_value = GetDateTimeStringFromNumSecondsSinceEpoch(num_secs_from_epoch, to_type_str.c_str() + date_string.size(), locale, timezone);
		}
		else if(to_type_str.compare(0, time_string.size(), time_string) == 0)
		{
			std::string locale;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();
				EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_locale, locale);
			}

			double num_secs_from_midnight = 0.0;
			if(use_number)				num_secs_from_midnight = number_value;
			else if(use_uint_number)	num_secs_from_midnight = static_cast<double>(uint_number_value);
			else if(use_int_number)		num_secs_from_midnight = static_cast<double>(int_number_value);
			else if(use_code)			num_secs_from_midnight = static_cast<double>(EvaluableNode::ToNumber(code_value));

			string_value = GetTimeStringFromNumSecondsSinceMidnight(num_secs_from_midnight, to_type_str.c_str() + time_string.size(), locale);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(to_params);
	evaluableNodeManager->FreeNodeTreeIfPossible(code_value);
	if(!valid_string_value)
		return AllocReturn(string_intern_pool.NOT_A_STRING_ID, immediate_result);
	return AllocReturn(string_value, immediate_result);
}
