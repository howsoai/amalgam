//project headers:
#include "Interpreter.h"
#include "InterpreterConcurrencyManager.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Iteration and Container Transform";

static OpcodeInitializer _ENT_RANGE(ENT_RANGE, &Interpreter::InterpretNode_ENT_RANGE, []() {
	OpcodeDetails d;
	d.parameters = R"([* function] number low_endpoint number high_endpoint [number step_size])";
	d.returns = R"(list)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to a list with the range from `low_endpoint` to `high_endpoint`.  The default `step_size` is 1.  Evaluates to an empty list if the range is not valid.  If four arguments are specified, then `function` will be evaluated for each value in the range.)";
	d.examples = MakeAmalgamExamples({
		{R"&((range 0 10))&", R"([
	0
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
])"},
			{R"&((range 10 0))&", R"([
	10
	9
	8
	7
	6
	5
	4
	3
	2
	1
	0
])"},
			{R"&((range 0 5 0))&", R"([])"},
			{R"&((range 0 5 1))&", R"([0 1 2 3 4 5])"},
			{R"&((range 12 0 5 1))&", R"([12 12 12 12 12 12])"},
			{R"&((range
	(lambda
		(+ (current_index) 1)
	)
	0
	5
	1
))&", R"([1 2 3 4 5 6])"},
			{R"&(||(range
	(lambda
		(+ (current_index) 1)
	)
	0
	5
	1
))&", R"([1 2 3 4 5 6])"}
		});
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 4.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_RANGE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_params = ocn.size();

	if(num_params < 2)
		return EvaluableNodeReference::Null();

	//get the index of the start index based on how many parameters there are, if there is a function
	size_t index_of_start = (num_params < 4 ? 0 : 1);

	double range_start = InterpretNodeIntoNumberValue(ocn[index_of_start + 0]);
	double range_end = InterpretNodeIntoNumberValue(ocn[index_of_start + 1]);

	if(FastIsNaN(range_start) || FastIsNaN(range_end))
		return EvaluableNodeReference::Null();

	//default step size
	double range_step_size = 1;
	if(range_end < range_start)
		range_step_size = -1;

	//if specified step size, get and make sure it's ok
	if(num_params > 2)
	{
		range_step_size = InterpretNodeIntoNumberValue(ocn[index_of_start + 2]);
		if(FastIsNaN(range_step_size))
			return EvaluableNodeReference::Null();

		//if not a good size, return empty list
		if(!(range_start <= range_end && range_step_size > 0)
			&& !(range_end <= range_start && range_step_size < 0))
		{
			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);
		}
	}

	size_t num_nodes = static_cast<size_t>((range_end - range_start) / range_step_size) + 1;

	//make sure continuing would not use too much memory
	if(ConstrainedAllocatedNodes())
	{
		if(interpreterConstraints->WouldNewAllocatedNodesExceedConstraint(
			evaluableNodeManager->GetNumberOfUsedNodes() + num_nodes))
			return EvaluableNodeReference::Null();
	}

	//if no function, just return a list of numbers
	if(index_of_start == 0)
	{
		EvaluableNodeReference range_list(evaluableNodeManager->AllocNode(ENT_LIST), true);

		auto &range_list_ocn = range_list->GetOrderedChildNodesReference();
		range_list_ocn.resize(num_nodes);
		for(size_t i = 0; i < num_nodes; i++)
			range_list_ocn[i] = evaluableNodeManager->AllocNode(i * range_step_size + range_start);

		return range_list;
	}

	//if a function is specified, then set up appropriate data structures to call the function and move the indices for the index and value parameters
	EvaluableNodeReference function = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(function);

	if(immediate_result.NoValueRequested())
	{
	#ifdef MULTITHREAD_SUPPORT
		if(en->GetConcurrency() && num_nodes > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
			if(Concurrency::threadPool.AreThreadsAvailable())
			{
				InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

				for(size_t node_index = 0; node_index < num_nodes; node_index++)
					concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNode *>(function,
						EvaluableNodeImmediateValueWithType(node_index * range_step_size + range_start),
						nullptr);

				concurrency_manager.EndConcurrency();
				return nullptr;
			}
		}
	#endif

		PushNewConstructionContext(nullptr, nullptr, EvaluableNodeImmediateValueWithType(0.0), nullptr);

		for(size_t i = 0; i < num_nodes; i++)
		{
			//pass index of list to be mapped -- leave value at nullptr
			SetTopCurrentIndexInConstructionStack(i * range_step_size + range_start);

			EvaluableNodeReference element_result = InterpretNodeForImmediateUse(function);
			evaluableNodeManager->FreeNodeTreeIfPossible(element_result);
		}

		PopConstructionContextAndGetExecutionSideEffectFlag();
		
		return nullptr;
	}

	EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_LIST), true);
	auto &result_ocn = result->GetOrderedChildNodesReference();
	result_ocn.resize(num_nodes);

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency() && num_nodes > 1)
	{
		auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
		if(Concurrency::threadPool.AreThreadsAvailable())
		{
			node_stack.PushEvaluableNode(result);
			//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
			result->SetNeedCycleCheck(true);

			InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

			for(size_t node_index = 0; node_index < num_nodes; node_index++)
				concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNode *>(function,
					nullptr, result, EvaluableNodeImmediateValueWithType(node_index * range_step_size + range_start),
					nullptr, result_ocn[node_index]);

			concurrency_manager.EndConcurrency();

			concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(result);
			return result;
		}
	}
#endif

	PushNewConstructionContext(nullptr, result, EvaluableNodeImmediateValueWithType(0.0), nullptr);

	for(size_t i = 0; i < num_nodes; i++)
	{
		//pass index of list to be mapped -- leave value at nullptr
		SetTopCurrentIndexInConstructionStack(i * range_step_size + range_start);

		EvaluableNodeReference element_result = InterpretNode(function);
		result_ocn[i] = element_result;
		result.UpdatePropertiesBasedOnAttachedNode(element_result);
	}

	if(PopConstructionContextAndGetExecutionSideEffectFlag())
	{
		result.unique = false;
		result.uniqueUnreferencedTopNode = false;
	}

	return result;
}

static OpcodeInitializer _ENT_REWRITE(ENT_REWRITE, &Interpreter::InterpretNode_ENT_REWRITE, []() {
	OpcodeDetails d;
	d.parameters = R"(* function * target)";
	d.returns = R"(any)";
	d.description = R"(Rewrites `target` by applying the `function` in a bottom-up manner.  For each node in the `target` structure, it pushes a new target scope onto the target stack, with `(current_value)` being the current node and `(current_index)` being to the index to the current node relative to the node passed into rewrite accessed via target, and evaluates `function`.  Returns the resulting structure, after have been rewritten by function.  Note that there is a small performance overhead if `target` is a graph structure rather than a tree structure.)";
	d.examples = MakeAmalgamExamples({
		{R"&((rewrite
	(lambda
		(if
			(~ (current_value) 0)
			(+ (current_value) 1)
			(current_value)
		)
	)
	[
		(associate "a" 13)
	]
))&", R"([
	{a 14}
])"},
			{R"&(;rewrite all integer additions into multiplies and then fold constants
(rewrite
	(lambda
		
		;find any nodes with a + and where its list is filled to its size with integers
		(if
			(and
				(=
					(get_type (current_value))
					(lambda (+))
				)
				(=
					(size (current_value))
					(size
						(filter
							(lambda
								(~ (current_value) 0)
							)
							(current_value)
						)
					)
				)
			)
			(reduce
				(lambda
					(* (previous_result) (current_value))
				)
				(current_value)
			)
			(current_value)
		)
	)
	
	;original code with additions to be rewritten
	(lambda
		[
			(associate
				"a"
				(+
					3
					(+ 13 4 2)
				)
			)
		]
	)
))&", R"([
	(associate "a" 312)
])"},
			{R"&(;rewrite numbers as sums of position in the list and the number (all 8s)
(rewrite
	(lambda
		
		;find any nodes with a + and where its list is filled to its size with integers
		(if
			(=
				(get_type_string (current_value))
				"number"
			)
			(+
				(current_value)
				(get_value (current_index))
			)
			(current_value)
		)
	)
	
	;original code with additions to be rewritten
	(lambda
		[
			8
			7
			6
			5
			4
			3
			2
			1
			0
		]
	)
))&", R"([
	8
	8
	8
	8
	8
	8
	8
	8
	8
])"},
			{R"&((rewrite
	(lambda
		(if
			(and
				(=
					(get_type (current_value))
					(lambda (+))
				)
				(=
					(size (current_value))
					(size
						(filter
							(lambda
								(~ (current_value) 0)
							)
							(current_value)
						)
					)
				)
			)
			(reduce
				(lambda
					(+ (previous_result) (current_value))
				)
				(current_value)
			)
			(current_value)
		)
	)
	(lambda
		(+
			(+ 13 4)
			a
		)
	)
))&", R"((+ 17 a))"}
		});
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_REWRITE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsNull(function))
		return EvaluableNodeReference::Null();
	auto node_stack = CreateOpcodeStackStateSaver(function);

	//get tree and make a copy so it can be modified in-place
	auto to_modify = InterpretNode(ocn[1]);

	FastHashMap<EvaluableNode *, EvaluableNode *> original_node_to_new_node;
	PushNewConstructionContext(nullptr, nullptr, EvaluableNodeImmediateValueWithType(), to_modify);
	EvaluableNodeReference result = RewriteByFunction(function, to_modify, original_node_to_new_node);
	PopConstructionContextAndGetExecutionSideEffectFlag();

	//there's a chance many of the nodes marked as being not cycle free actually are
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);

	return result;
}

static OpcodeInitializer _ENT_MAP(ENT_MAP, &Interpreter::InterpretNode_ENT_MAP, []() {
	OpcodeDetails d;
	d.parameters = R"(* function [list|assoc collection1] [list|assoc collection2] ... [list|assoc collectionN])";
	d.returns = R"(list)";
	d.allowsConcurrency = true;
	d.description = R"(For each element in the collection, pushes a new target scope onto the stack, so that `(current_value)` accesses the element or elements in the list and `(current_index)` accesses the list or assoc index, with `(target)` representing the outer set of lists or assocs, and evaluates the function.  Returns the list of results, mapping the list via the specified `function`.  If multiple lists or assocs are specified, then it pulls from each list or assoc simultaneously (null if overrun or index does not exist) and `(current_value)` contains an array of the values in parameter order.  Note that concurrency is only available when more than one one collection is specified.)";
	d.examples = MakeAmalgamExamples({
					{R"&((map
	(lambda
		(* (current_value) 2)
	)
	[1 2 3 4]
))&", R"([2 4 6 8])"},
			{R"&((map
	(lambda
		(+ (current_value) (current_index))
	)
	[
		10
		1
		20
		2
		30
		3
		40
		4
	]
))&", R"([
	10
	2
	22
	5
	34
	8
	46
	11
])"},
			{R"&((map
	(lambda
		(+ (current_value) (current_index))
	)
	(associate
		10
		1
		20
		2
		30
		3
		40
		4
	)
))&", R"({
	10 11
	20 22
	30 33
	40 44
})"},
			{R"&((map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
		)
	)
	[1 2 3 4 5 6]
	[2 2 2 2 2 2]
))&", R"([3 4 5 6 7 8])"},
			{R"&((map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
		)
	)
	[1 2 3 4 5]
	[2 2 2 2 2 2]
))&", R"([3 4 5 6 7 .null])"},
			{R"&((map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
			(get (current_value) 2)
		)
	)
	(associate 0 0 1 1 "a" 3)
	(associate 0 1 "a" 4)
	[2 2 2 2]
))&", R"({
	0 3
	1 .null
	2 .null
	3 .null
	a .null
})"}
		});
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 39.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MAP(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(function);

	EvaluableNodeReference result = EvaluableNodeReference::Null();

	if(ocn.size() == 2)
	{
		//get list
		auto list = InterpretNode(ocn[1]);
		if(list == nullptr)
			return EvaluableNodeReference::Null();

		//create result_list as a copy of the current list, but without child nodes
		result = EvaluableNodeReference(evaluableNodeManager->AllocNode(list->GetType()), true);

		if(list->IsOrderedArray())
		{
			auto &list_ocn = list->GetOrderedChildNodesReference();
			size_t num_nodes = list_ocn.size();

			auto &result_ocn = result->GetOrderedChildNodesReference();
			result_ocn.resize(num_nodes);

		#ifdef MULTITHREAD_SUPPORT
			if(en->GetConcurrency() && num_nodes > 1)
			{
				auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
				if(Concurrency::threadPool.AreThreadsAvailable())
				{
					node_stack.PushEvaluableNode(list);
					node_stack.PushEvaluableNode(result);
					//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
					result->SetNeedCycleCheck(true);

					InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

					for(size_t node_index = 0; node_index < num_nodes; node_index++)
						concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNode *>(function,
							list, result, EvaluableNodeImmediateValueWithType(static_cast<double>(node_index)),
							list_ocn[node_index], result_ocn[node_index]);

					concurrency_manager.EndConcurrency();

					concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(result);
					if(result.unique && !concurrency_manager.HadSideEffects())
						evaluableNodeManager->FreeNodeTreeIfPossible(list);

					return result;
				}
			}
		#endif

			PushNewConstructionContext(list, result, EvaluableNodeImmediateValueWithType(0.0), nullptr);

			for(size_t i = 0; i < num_nodes; i++)
			{
				//pass value of list to be mapped
				SetTopCurrentIndexInConstructionStack(static_cast<double>(i));
				SetTopCurrentValueInConstructionStack(list_ocn[i]);

				EvaluableNodeReference element_result = InterpretNode(function);
				result_ocn[i] = element_result;
				result.UpdatePropertiesBasedOnAttachedNode(element_result);
			}

			if(PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				result.unique = false;
				result.uniqueUnreferencedTopNode = false;
			}
		}
		else if(list->IsAssociativeArray())
		{
			auto &list_mcn = list->GetMappedChildNodesReference();
			size_t num_nodes = list_mcn.size();

			//populate result_mcn with all a slot for each child node,
			//as do not want to change this allocation during potential concurrent execution
			//and because iterators may be invalidated when the map is changed
			auto &result_mcn = result->GetMappedChildNodesReference();
			result_mcn.reserve(num_nodes);
			for(auto &[sid, cn] : list_mcn)
				result_mcn.emplace(string_intern_pool.CreateStringReference(sid), nullptr);

		#ifdef MULTITHREAD_SUPPORT
			if(en->GetConcurrency() && num_nodes > 1)
			{
				auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
				if(Concurrency::threadPool.AreThreadsAvailable())
				{
					node_stack.PushEvaluableNode(list);
					node_stack.PushEvaluableNode(result);
					//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
					result->SetNeedCycleCheck(true);

					InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

					for(auto &[result_id, result_node] : result_mcn)
					{
						//get the original data element
						auto list_node_entry = list_mcn.find(result_id);
						EvaluableNode *list_node = nullptr;
						if(list_node_entry != end(list_mcn))
							list_node = list_node_entry->second;

						concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNode *>(function,
							list, result, EvaluableNodeImmediateValueWithType(result_id),
							list_node, result_node);
					}

					concurrency_manager.EndConcurrency();

					concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(result);
					if(result.unique && !concurrency_manager.HadSideEffects())
						evaluableNodeManager->FreeNodeTreeIfPossible(list);

					return result;
				}
			}
		#endif

			PushNewConstructionContext(list, result, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

			for(auto &[result_id, result_node] : result_mcn)
			{
				SetTopCurrentIndexInConstructionStack(result_id);

				//get the original data element
				auto list_node_entry = list_mcn.find(result_id);
				if(list_node_entry != end(list_mcn))
					SetTopCurrentValueInConstructionStack(list_node_entry->second);

				//keep the original type of element_result instead of directly assigning
				//in order to keep the node properties to be updated below
				EvaluableNodeReference element_result = InterpretNode(function);
				result_node = element_result;
				result.UpdatePropertiesBasedOnAttachedNode(element_result);
			}

			if(PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				result.unique = false;
				result.uniqueUnreferencedTopNode = false;
			}
		}

		//result will be marked if not unique if there were any side effects
		if(result.unique)
			evaluableNodeManager->FreeNodeTreeIfPossible(list);
	}
	else //multiple inputs
	{
		EvaluableNode *inputs_list_node = evaluableNodeManager->AllocNode(ENT_LIST);
		//set to need cycle check because don't know what will be attached
		inputs_list_node->SetNeedCycleCheck(true);
		inputs_list_node->SetOrderedChildNodesSize(ocn.size() - 1);
		auto &inputs = inputs_list_node->GetOrderedChildNodesReference();

		//process inputs, get size and whether needs to be associative array
		bool need_assoc = false;

		//note that all_keys will maintain references to each StringID that must be freed
		FastHashSet<StringInternPool::StringID> all_keys;	//only if have assoc
		size_t largest_size = 0; //only if have list

		node_stack.PushEvaluableNode(inputs_list_node);
		for(size_t i = 0; i < ocn.size() - 1; i++)
		{
			inputs[i] = InterpretNode(ocn[i + 1]);
			if(inputs[i] != nullptr)
			{
				if(!inputs[i]->IsAssociativeArray())
				{
					largest_size = std::max(largest_size, inputs[i]->GetOrderedChildNodes().size());
				}
				else
				{
					need_assoc = true;
					for(auto &[n_id, _] : inputs[i]->GetMappedChildNodes())
					{
						auto [inserted_node, inserted] = all_keys.insert(n_id);
						//if it was inserted, then need to keep track of the string reference
						if(inserted)
							string_intern_pool.CreateStringReference(n_id);
					}
				}
			}
		}
		node_stack.PopEvaluableNode();

		if(!need_assoc)
		{
			result = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);
			result->GetOrderedChildNodesReference().resize(largest_size);

			PushNewConstructionContext(inputs_list_node, result, EvaluableNodeImmediateValueWithType(0.0), nullptr);

			for(size_t index = 0; index < largest_size; index++)
			{
				//set index value
				SetTopCurrentIndexInConstructionStack(static_cast<double>(index));

				//combine input slices together into value
				EvaluableNode *input_slice = evaluableNodeManager->AllocNode(ENT_LIST);
				auto &is_ocn = input_slice->GetOrderedChildNodesReference();
				is_ocn.resize(inputs.size());
				for(size_t i = 0; i < inputs.size(); i++)
				{
					if(inputs[i] == nullptr || index >= inputs[i]->GetOrderedChildNodes().size())
					{
						is_ocn[i] = nullptr;
						continue;
					}
					is_ocn[i] = inputs[i]->GetOrderedChildNodes()[index];
				}
				SetTopCurrentValueInConstructionStack(input_slice);

				EvaluableNodeReference element_result = InterpretNode(function);
				result->GetOrderedChildNodesReference()[index] = element_result;
				result.UpdatePropertiesBasedOnAttachedNode(element_result);
			}

			if(PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				result.unique = false;
				result.uniqueUnreferencedTopNode = false;
			}
		}
		else //need associative array
		{
			result = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_ASSOC), true);
			result->ReserveMappedChildNodes(largest_size + all_keys.size());

			PushNewConstructionContext(inputs_list_node, result, EvaluableNodeImmediateValueWithType(0.0), nullptr);

			//do any numbers from lists first
			for(size_t index = 0; index < largest_size; index++)
			{
				//set index value
				SetTopCurrentIndexInConstructionStack(static_cast<double>(index));

				//combine input slices together into value
				EvaluableNode *input_slice = evaluableNodeManager->AllocNode(ENT_LIST);
				auto &is_ocn = input_slice->GetOrderedChildNodesReference();
				is_ocn.resize(inputs.size());
				for(size_t i = 0; i < inputs.size(); i++)
				{
					if(inputs[i] == nullptr)
					{
						is_ocn[i] = nullptr;
					}
					else if(inputs[i]->IsAssociativeArray())
					{
						const std::string index_string = EvaluableNode::NumberToString(index, true);
						EvaluableNode **found = inputs[i]->GetMappedChildNode(index_string);
						if(found != nullptr)
							is_ocn[i] = *found;
					}
					else //list
					{
						if(index < inputs[i]->GetOrderedChildNodes().size())
							is_ocn[i] = inputs[i]->GetOrderedChildNodes()[index];
					}
				}
				SetTopCurrentValueInConstructionStack(input_slice);

				EvaluableNodeReference element_result = InterpretNode(function);
				std::string index_string = EvaluableNode::NumberToString(index, true);
				result->SetMappedChildNode(index_string, element_result);
				result.UpdatePropertiesBasedOnAttachedNode(element_result);

				//remove from keys so it isn't clobbered when checking assoc keys
				StringInternPool::StringID index_sid = string_intern_pool.GetIDFromString(index_string);
				if(all_keys.erase(index_sid))
					string_intern_pool.DestroyStringReference(index_sid);
			}

			//now perform for all assocs
			for(auto &index_sid : all_keys)
			{
				//set index value
				SetTopCurrentIndexInConstructionStack(index_sid);

				//combine input slices together into value
				EvaluableNode *input_slice = evaluableNodeManager->AllocNode(ENT_LIST);
				auto &is_ocn = input_slice->GetOrderedChildNodesReference();
				is_ocn.resize(inputs.size());
				for(size_t i = 0; i < inputs.size(); i++)
				{
					//dealt with lists previously, only assoc in this pass
					if(!EvaluableNode::IsAssociativeArray(inputs[i]))
						is_ocn[i] = nullptr;
					else
					{
						EvaluableNode **found = inputs[i]->GetMappedChildNode(index_sid);
						if(found != nullptr)
							is_ocn[i] = *found;
					}
				}
				SetTopCurrentValueInConstructionStack(input_slice);

				EvaluableNodeReference element_result = InterpretNode(function);
				result->SetMappedChildNode(index_sid, element_result);
				result.UpdatePropertiesBasedOnAttachedNode(element_result);
			}

			if(PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				result.unique = false;
				result.uniqueUnreferencedTopNode = false;
			}

		} //needed to process as assoc array

		//free all references
		string_intern_pool.DestroyStringReferences(all_keys);
	}

	return result;
}

static OpcodeInitializer _ENT_FILTER(ENT_FILTER, &Interpreter::InterpretNode_ENT_FILTER, []() {
	OpcodeDetails d;
	d.parameters = R"([* function] list|assoc collection [bool match_on_value])";
	d.returns = R"(list|assoc)";
	d.allowsConcurrency = true;
	d.description = R"(For each element in the `collection`, pushes a new target scope onto the stack, so that `(current_value)` accesses the element in the list and `(current_index)` accesses the list or assoc index, with `(target)` representing the original list or assoc, and evaluates the function.  If `function` evaluates to true, then the element is put in a new list or assoc (matching the input type) that is returned.  If function is omitted, then it will remove any elements in the collection that are null.  The parameter match_on_value defaults to null, which will evaluate the function.  However, if match_on_value is true, it will only retain elements which equal the value in function and if match_on_value is false, it will retain elements which do not equal the value in function.  Using match_on_value and wrapping filter in a size opcode additionally acts as an efficient way to count the number of a specific element in a container.)";
	d.examples = MakeAmalgamExamples({
		{R"&((filter
	(lambda
		(> (current_value) 2)
	)
	[1 2 3 4]
))&", R"([3 4])"},
			{R"&((filter
	(lambda
		(< (current_index) 3)
	)
	[
		10
		1
		20
		2
		30
		3
		40
		4
	]
))&", R"([10 1 20])"},
			{R"&((filter
	(lambda
		(< (current_index) 20)
	)
	(associate
		10
		1
		20
		2
		30
		3
		40
		4
	)
))&", R"({10 1})"},
			{R"&((filter
	[
		10
		1
		20
		.null
		30
		.null
		.null
		40
		4
	]
))&", R"([10 1 20 30 40 4])"},
			{R"&((filter
	[
		10
		1
		20
		.null
		30
		""
		40
		4
	]
))&", R"([
	10
	1
	20
	30
	""
	40
	4
])"},
			{R"&((filter
	{
		a 10
		b 1
		c 20
		d ""
		e 30
		f 3
		g .null
		h 4
	}
))&", R"({
	a 10
	b 1
	c 20
	d ""
	e 30
	f 3
	h 4
})"},
			{R"&((filter
	{
		a 10
		b 1
		c 20
		d ""
		e 30
		f 3
		g .null
		h 4
	}
))&", R"({
	a 10
	b 1
	c 20
	d ""
	e 30
	f 3
	h 4
})"},
{ R"&((filter .null [.null 1 .null 2 .null 3] .false))&", R"([1 2 3])" },
{ R"&((filter .null {a .null b 1 c .null d 2 e .null f 3} .true))&", R"({a .null c .null e .null})" }
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 15.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_FILTER(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference function = EvaluableNodeReference::Null();
	auto node_stack = CreateOpcodeStackStateSaver();

	bool match_on_value = false;
	bool match_on_not_value = false;
	size_t list_index = 0;
	if(ocn.size() == 1)
	{
		//match on not null
		match_on_not_value = true;
	}
	else //ocn.size() > 1
	{
		list_index = 1;
		function = InterpretNodeForImmediateUse(ocn[0]);
		node_stack.PushEvaluableNode(function);

		if(ocn.size() > 2)
		{
			auto match_on_value_param = InterpretNodeForImmediateUse(ocn[2],
				EvaluableNodeRequestedValueTypes::Type::REQUEST_BOOL);
			auto &match_on_value_ref = match_on_value_param.GetValue();

			if(!match_on_value_ref.IsNull())
			{
				if(match_on_value_ref.GetValueAsBoolean())
					match_on_value = true;
				else
					match_on_not_value = true;
			}
		}
	}

	if(match_on_value || match_on_not_value)
	{
		//specialized path for immediate result just getting the count
		if(immediate_result.AnyImmediateType())
		{
			auto list = InterpretNode(ocn[list_index]);
			if(EvaluableNode::IsNull(list))
				return EvaluableNodeReference::Null();

			size_t num_elements_not_filtered = 0;
			if(list->IsAssociativeArray())
			{
				auto &list_mcn = list->GetMappedChildNodesReference();
				for(auto &[cn_id, cn] : list_mcn)
				{
					//want either to be equal or match_on_not_value, but not both or neither
					if(EvaluableNode::AreDeepEqual(cn, function) != match_on_not_value)
						num_elements_not_filtered++;
				}

			}
			else if(list->IsOrderedArray())
			{
				auto &list_ocn = list->GetOrderedChildNodesReference();
				for(auto &cn : list_ocn)
				{
					if(EvaluableNode::AreDeepEqual(cn, function) != match_on_not_value)
						num_elements_not_filtered++;
				}
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(list);
			return EvaluableNodeReference(static_cast<double>(num_elements_not_filtered));
		}

		auto list = InterpretNode(ocn[list_index]);
		if(EvaluableNode::IsNull(list))
			return EvaluableNodeReference::Null();

		EvaluableNodeReference result_list(list, list.unique, list.uniqueUnreferencedTopNode);

		//need to edit the list itself, so if not unique, make at least the top node unique
		evaluableNodeManager->EnsureNodeIsModifiable(result_list, true);

		if(result_list->IsAssociativeArray())
		{
			auto &result_list_mcn = result_list->GetMappedChildNodesReference();

			//can't erase from result_list_mcn while iterating because it may invalidate
			//iteration, need to collect those to remove and remove in a separate pass
			std::vector<StringInternPool::StringID> ids_to_remove;
			for(auto &[cn_id, cn] : result_list_mcn)
			{
				if(!(EvaluableNode::AreDeepEqual(cn, function) != match_on_not_value))
					ids_to_remove.push_back(cn_id);
			}

			if(result_list.unique && !result_list->GetNeedCycleCheck())
			{
				//FreeNodeTree and erase the key
				for(auto &id : ids_to_remove)
				{
					auto pair = result_list_mcn.find(id);
					evaluableNodeManager->FreeNodeTree(pair->second);
					result_list_mcn.erase(pair);
					string_intern_pool.DestroyStringReference(id);
				}
			}
			else //can't safely delete any nodes
			{
				for(auto &id : ids_to_remove)
				{
					result_list_mcn.erase(id);
					string_intern_pool.DestroyStringReference(id);
				}
			}
		}
		else if(result_list->IsOrderedArray())
		{
			auto &result_list_ocn = result_list->GetOrderedChildNodesReference();

			if(result_list.unique && !result_list->GetNeedCycleCheck())
			{
				//for any nodes to be erased, FreeNodeTree and erase the index
				for(size_t i = result_list_ocn.size(); i > 0; i--)
				{
					size_t index = i - 1;
					if(EvaluableNode::AreDeepEqual(result_list_ocn[index], function) != match_on_not_value)
						continue;

					evaluableNodeManager->FreeNodeTree(result_list_ocn[index]);
					result_list_ocn.erase(begin(result_list_ocn) + index);
				}
			}
			else //can't safely delete any nodes
			{
				auto new_end = std::remove_if(begin(result_list_ocn), end(result_list_ocn),
					[&function, match_on_not_value](EvaluableNode *en)
					{
						return !(EvaluableNode::AreDeepEqual(en, function) != match_on_not_value);
					});
				result_list_ocn.erase(new_end, end(result_list_ocn));
			}
		}

		return result_list;
	}

	//get list
	auto list = InterpretNode(ocn[list_index]);
	//if null, just return a new null, since it has no child nodes
	if(EvaluableNode::IsNull(list))
		return EvaluableNodeReference::Null();

	//create result_list as a copy of the current list, but without child nodes
	EvaluableNodeReference result_list(evaluableNodeManager->AllocNode(list->GetType()),
		list.unique, list.uniqueUnreferencedTopNode);
	result_list->SetNeedCycleCheck(list->GetNeedCycleCheck());
	result_list->SetIsIdempotent(list->GetIsIdempotent());
	bool had_side_effects = false;

	if(EvaluableNode::IsNull(function))
		return result_list;

	if(list->GetOrderedChildNodes().size() > 0)
	{
		auto &list_ocn = list->GetOrderedChildNodesReference();
		auto &result_ocn = result_list->GetOrderedChildNodesReference();

	#ifdef MULTITHREAD_SUPPORT
		size_t num_nodes = list_ocn.size();
		if(en->GetConcurrency() && num_nodes > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
			if(Concurrency::threadPool.AreThreadsAvailable())
			{
				node_stack.PushEvaluableNode(list);
				node_stack.PushEvaluableNode(result_list);
				//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
				result_list->SetNeedCycleCheck(true);

				std::vector<EvaluableNodeReference> evaluations(num_nodes);

				InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

				for(size_t node_index = 0; node_index < num_nodes; node_index++)
					concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNodeReference>(function,
						list, result_list, EvaluableNodeImmediateValueWithType(static_cast<double>(node_index)),
						list_ocn[node_index], evaluations[node_index]);

				concurrency_manager.EndConcurrency();

				concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(result_list);
				had_side_effects = concurrency_manager.HadSideEffects();

				//filter by those child nodes that are true
				for(size_t i = 0; i < num_nodes; i++)
				{
					if(EvaluableNode::ToBool(evaluations[i]))
						result_ocn.push_back(list_ocn[i]);

					//only free nodes if the result is still unique, and it won't be if it was accessed
					if(!had_side_effects)
						evaluableNodeManager->FreeNodeTreeIfPossible(evaluations[i]);
				}
			}
		}
		else
		#endif
			//need this in a block for multithreading above
		{
			PushNewConstructionContext(list, result_list, EvaluableNodeImmediateValueWithType(0.0), nullptr);

			//iterate over all child nodes
			for(size_t i = 0; i < list_ocn.size(); i++)
			{
				EvaluableNode *cur_value = list_ocn[i];

				SetTopCurrentIndexInConstructionStack(static_cast<double>(i));
				SetTopCurrentValueInConstructionStack(cur_value);

				//check current element
				if(InterpretNodeIntoBoolValue(function))
					result_ocn.push_back(cur_value);
			}

			had_side_effects = PopConstructionContextAndGetExecutionSideEffectFlag();
			if(had_side_effects)
			{
				result_list.unique = false;
				result_list.uniqueUnreferencedTopNode = false;
			}

			//free anything not in filtered list,
			// but only free nodes if the result is still unique, and it won't be if it was accessed
			// need to do this outside of the iteration loop in case anything is accessing the original list
			if(list.unique && !list->GetNeedCycleCheck() && !had_side_effects)
			{
				size_t result_index = 0;
				for(size_t i = 0; i < list_ocn.size(); i++)
				{
					//if there are still results left, check if it matches
					if(result_index < result_ocn.size() && list_ocn[i] == result_ocn[result_index])
						result_index++;
					else //free it
						evaluableNodeManager->FreeNodeTree(list_ocn[i]);
				}
			}
		}

		evaluableNodeManager->FreeNodeIfPossible(list);
		return result_list;
	}

	if(list->IsAssociativeArray())
	{
		auto &list_mcn = list->GetMappedChildNodesReference();

	#ifdef MULTITHREAD_SUPPORT
		size_t num_nodes = list_mcn.size();
		if(en->GetConcurrency() && num_nodes > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
			if(Concurrency::threadPool.AreThreadsAvailable())
			{
				node_stack.PushEvaluableNode(list);
				node_stack.PushEvaluableNode(result_list);
				//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
				result_list->SetNeedCycleCheck(true);

				std::vector<EvaluableNodeReference> evaluations(num_nodes);

				InterpreterConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

				//kick off interpreters
				size_t node_index = 0;
				for(auto &[node_id, node] : list_mcn)
					concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNodeReference>(function,
						list, result_list, EvaluableNodeImmediateValueWithType(node_id),
						node, evaluations[node_index++]);

				concurrency_manager.EndConcurrency();

				concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(result_list);
				had_side_effects = concurrency_manager.HadSideEffects();

				//iterate in same order with same node_index
				node_index = 0;
				for(auto &[node_id, node] : list_mcn)
				{
					if(EvaluableNode::ToBool(evaluations[node_index]))
						result_list->SetMappedChildNode(node_id, node);

					//only free nodes if the result is still unique, and it won't be if it was accessed
					if(!had_side_effects)
						evaluableNodeManager->FreeNodeTreeIfPossible(evaluations[node_index]);

					node_index++;
				}
			}
		}
		else
		#endif
		{
			PushNewConstructionContext(list, result_list, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

			//result_list is a copy of list, so it should already be the same size (no need to reserve)
			for(auto &[cn_id, cn] : list_mcn)
			{
				SetTopCurrentIndexInConstructionStack(cn_id);
				SetTopCurrentValueInConstructionStack(cn);

				//if contained, add to result_list (and let SetMappedChildNode create the string reference)
				if(InterpretNodeIntoBoolValue(function))
					result_list->SetMappedChildNode(cn_id, cn);
			}

			if(PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				result_list.unique = false;
				result_list.uniqueUnreferencedTopNode = false;
			}
		}
	}

	evaluableNodeManager->FreeNodeIfPossible(list);
	return result_list;
}

static OpcodeInitializer _ENT_WEAVE(ENT_WEAVE, &Interpreter::InterpretNode_ENT_WEAVE, []() {
	OpcodeDetails d;
	d.parameters = R"([* function] list|immediate values1 [list|immediate values2] [list|immediate values3]...)";
	d.returns = R"(list)";
	d.description = R"(Interleaves the values lists optionally by applying a function.  If only `values1` is passed in, then it evaluates to `values1`. If `values1` and `values2` are passed in, or, if more values are passed in but function is null, it interleaves the lists and extends the result to the length of the longest list, filling in the remainder with null.  If any of the value parameters are immediate, then it will repeat that immediate value when weaving.  If the `function` is specified and not null, it pushes a new target scope onto the stack, so that `(current_value)` accesses a list of elements to be woven together from the list, and `(current_index)` accesses the list or assoc index, with `(target)` representing the resulting list or assoc.  The `function` should evaluate to a list, and weave will evaluate to a concatenated list of all of the lists that the function evaluated to.)";
	d.examples = MakeAmalgamExamples({
		{R"&((weave
	[1 2 3]
))&", R"([1 2 3])"},
			{R"&((weave
	[1 3 5]
	[2 4 6]
))&", R"([1 2 3 4 5 6])"},
			{R"&((weave
	.null
	[2 4 6]
	.null
))&", R"([2 .null 4 .null 6 .null])"},
			{R"&((weave
	"a"
	[2 4 6]
))&", R"(["a" 2 @(target .true 0) 4 @(target .true 0) 6])"},
			{R"&((weave
	.null
	[1 4 7]
	[2 5 8]
	[3 6 9]
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
			{R"&((weave
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
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
])"},
			{R"&((weave
	(lambda (current_value))
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
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
])"},
			{R"&((weave
	(lambda
		(map
			(lambda
				(* 2 (current_value))
			)
			(current_value)
		)
	)
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
))&", R"([
	2
	4
	6
	8
	10
	12
	14
	16
	18
	20
	22
	24
])"},
			{R"&((weave
	(lambda
		[
			(apply
				"min"
				(current_value 1)
			)
		]
	)
	[1 3 4 5 5 6]
	[2 2 3 4 6 7]
))&", R"([1 2 3 4 5 6])"},
			{R"&((weave
	(lambda
		(if
			(<=
				(get (current_value) 0)
				4
			)
			[
				(apply
					"min"
					(current_value 1)
				)
			]
			(current_value)
		)
	)
	[1 3 4 5 5 6]
	[2 2 3 4 6 7]
))&", R"([
	1
	2
	3
	5
	4
	5
	6
	6
	7
])"},
			{R"&((weave
	(lambda
		(if
			(>=
				(first (current_value))
				3
			)
			[
				(first
					(current_value 1)
				)
			]
			[]
		)
	)
	[1 2 3 4 5]
	.null
))&", R"([3 4 5])"}
		});
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 1.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_WEAVE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	size_t num_params = ocn.size();
	if(num_params < 1)
		return EvaluableNodeReference::Null();

	//single list, return itself
	if(ocn.size() == 1)
		return InterpretNode(ocn[0]);

	//get the index of the first list to weave based on how many parameters there are
	size_t index_of_first_list = 0;

	auto node_stack = CreateOpcodeStackStateSaver();

	//if a function is specified, then set up appropriate data structures to call the function and move the indices for the index and value parameters
	EvaluableNodeReference function = EvaluableNodeReference::Null();
	if(num_params >= 3)
	{
		index_of_first_list++;

		//need to interpret node here in case function is actually a null
		// null is a special non-function for weave
		function = InterpretNodeForImmediateUse(ocn[0]);
		node_stack.PushEvaluableNode(function);
	}

	//interpret all the lists, need to keep those around that are nulls because it ensures that the nulls should be interleaved
	// when a function is not passed in and it ensures that index of the parameters matches the index of the _ variable
	std::vector<EvaluableNodeReference> lists(num_params - index_of_first_list);
	for(size_t list_index = index_of_first_list; list_index < num_params; list_index++)
	{
		lists[list_index - index_of_first_list] = InterpretNode(ocn[list_index]);
		node_stack.PushEvaluableNode(lists[list_index - index_of_first_list]);
	}

	//find the largest of all the lists and the total number of elements
	size_t maximum_list_size = 0;
	size_t total_num_elements = 0;
	for(auto &list : lists)
	{
		if(list != nullptr)
		{
			size_t num_elements = list->GetOrderedChildNodes().size();
			maximum_list_size = std::max(maximum_list_size, num_elements);
			total_num_elements += num_elements;
		}
	}

	//make sure that continuing would not use too much memory
	if(ConstrainedAllocatedNodes())
	{
		if(interpreterConstraints->WouldNewAllocatedNodesExceedConstraint(
			evaluableNodeManager->GetNumberOfUsedNodes() + total_num_elements))
			return EvaluableNodeReference::Null();
	}

	//the result
	EvaluableNodeReference woven_list(evaluableNodeManager->AllocNode(ENT_LIST), true);

	//just lists, interleave
	if(EvaluableNode::IsNull(function))
	{
		woven_list->ReserveOrderedChildNodes(total_num_elements);

		for(auto &list : lists)
		{
			if(list != nullptr && IsEvaluableNodeTypeImmediate(list->GetType()))
				woven_list->SetNeedCycleCheck(true);

			woven_list.UpdatePropertiesBasedOnAttachedNode(list);
		}

		//for every index, iterate over every list and if there is an element, put it in the woven list
		for(size_t list_index = 0; list_index < maximum_list_size; list_index++)
		{
			for(auto &list : lists)
			{
				//if immediate, then write out immediate
				if(list == nullptr || IsEvaluableNodeTypeImmediate(list->GetType()))
					woven_list->AppendOrderedChildNode(list);
				else if(list->GetOrderedChildNodes().size() > list_index) //only write out if list is long enough
					woven_list->AppendOrderedChildNode(list->GetOrderedChildNodes()[list_index]);
			}
		}

		return woven_list;
	}

	//for every index, iterate over every list and call the function
	for(size_t list_index = 0; list_index < maximum_list_size; list_index++)
	{
		//get all of the values
		EvaluableNode *list_index_values_node = evaluableNodeManager->AllocNode(ENT_LIST);
		list_index_values_node->ReserveOrderedChildNodes(lists.size());
		for(auto &list : lists)
		{
			//if immediate, then write out immediate
			if(list == nullptr || IsEvaluableNodeTypeImmediate(list->GetType()))
				list_index_values_node->AppendOrderedChildNode(list);
			else if(list->GetOrderedChildNodes().size() > list_index)
				list_index_values_node->AppendOrderedChildNode(list->GetOrderedChildNodes()[list_index]);
			else //there's no value, so append null so that at least the function can see it
				list_index_values_node->AppendOrderedChildNode(nullptr);
		}

		PushNewConstructionContext(nullptr, woven_list, EvaluableNodeImmediateValueWithType(static_cast<double>(list_index)), list_index_values_node);

		EvaluableNodeReference values_to_weave = InterpretNode(function);

		if(PopConstructionContextAndGetExecutionSideEffectFlag())
		{
			woven_list.unique = false;
			woven_list.uniqueUnreferencedTopNode = false;
		}

		if(EvaluableNode::IsNull(values_to_weave))
		{
			woven_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//append as if it were a list
		for(EvaluableNode *cn : values_to_weave->GetOrderedChildNodes())
			woven_list->AppendOrderedChildNode(cn);
		if(values_to_weave->GetOrderedChildNodes().size() > 0)
			woven_list.UpdatePropertiesBasedOnAttachedNode(values_to_weave);

		//the rest of the values have been copied over, so only the top node is potentially freeable
		evaluableNodeManager->FreeNodeIfPossible(values_to_weave);
	}

	return woven_list;
}

static OpcodeInitializer _ENT_REDUCE(ENT_REDUCE, &Interpreter::InterpretNode_ENT_REDUCE, []() {
	OpcodeDetails d;
	d.parameters = R"(* function list|assoc collection)";
	d.returns = R"(any)";
	d.description = R"(For each element in the `collection` after the first one, it evaluates `function` with a new scope on the stack where `(current_value)` accesses each of the elements from the `collection`, `(current_index)` accesses the list or assoc index and `(previous_result)` accesses the previously reduced result.  If the `collection` is empty, null is returned.  If the `collection` is of size one, the single element is returned.)";
	d.examples = MakeAmalgamExamples({
		{R"&((reduce
	(lambda
		(* (current_value) (previous_result))
	)
	[1 2 3 4]
))&", R"(24)"},
			{R"&((reduce
	(lambda
		(* (current_value) (previous_result))
	)
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
	)
))&", R"(24)"}
		});
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_REDUCE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsNull(function))
		return EvaluableNodeReference::Null();

	auto node_stack = CreateOpcodeStackStateSaver(function);

	//get list
	auto list = InterpretNode(ocn[1]);
	if(list == nullptr)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference previous_result = EvaluableNodeReference::Null();

	PushNewConstructionContext(list, nullptr, EvaluableNodeImmediateValueWithType(), nullptr, previous_result);

	if(list->IsAssociativeArray())
	{
		bool first_node = true;
		//iterate over list
		for(auto &[n_id, n] : list->GetMappedChildNodesReference())
		{
			//grab a value if first one
			if(first_node)
			{
				//inform that the first result is not unique; if no side effects and unique result, can free all at once
				previous_result = EvaluableNodeReference(n, false);
				first_node = false;
				continue;
			}

			SetTopCurrentIndexInConstructionStack(n_id);
			SetTopCurrentValueInConstructionStack(n);
			SetTopPreviousResultInConstructionStack(previous_result);
			previous_result = InterpretNode(function);
		}
	}
	else if(list->GetOrderedChildNodes().size() >= 1)
	{
		auto &list_ocn = list->GetOrderedChildNodesReference();
		//inform that the first result is not unique; if no side effects and unique result, can free all at once
		previous_result = EvaluableNodeReference(list_ocn[0], false);

		//iterate over list
		for(size_t i = 1; i < list_ocn.size(); i++)
		{
			SetTopCurrentIndexInConstructionStack(static_cast<double>(i));
			SetTopCurrentValueInConstructionStack(list_ocn[i]);
			SetTopPreviousResultInConstructionStack(previous_result);
			previous_result = InterpretNode(function);
		}
	}

	bool side_effects = PopConstructionContextAndGetExecutionSideEffectFlag();
	if(previous_result.unique && !side_effects)
		evaluableNodeManager->FreeNodeTreeIfPossible(list);

	return previous_result;
}

static OpcodeInitializer _ENT_ASSOCIATE(ENT_ASSOCIATE, &Interpreter::InterpretNode_ENT_ASSOCIATE, []() {
	OpcodeDetails d;
	d.parameters = R"([* index1] [* value1] [* index2] [* value2] ... [* indexN] [* valueN])";
	d.returns = R"(assoc)";
	d.allowsConcurrency = true;
	d.description = R"(Evaluates to the assoc, where each pair of parameters (e.g., `index1` and `value1`) comprises a index/value pair.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the assoc, the current index, and the current value.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unparse
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
))&", R"("{4 \"d\" a 1 b 2 c 3}")"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 4.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSOCIATE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//use stack to lock it in place, but copy it back to temporary before returning
	EvaluableNodeReference new_assoc(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_nodes = ocn.size();

	if(num_nodes > 0)
	{
		new_assoc->ReserveMappedChildNodes(num_nodes / 2);

	#ifdef MULTITHREAD_SUPPORT
		if(en->GetConcurrency() && num_nodes > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
			if(Concurrency::threadPool.AreThreadsAvailable())
			{
				auto node_stack = CreateOpcodeStackStateSaver(new_assoc);
				//set as needing cycle check; concurrency_manager will clear it if it is not needed when finished
				new_assoc->SetNeedCycleCheck(true);

				//get keys
				std::vector<StringInternPool::StringID> keys;
				keys.reserve(num_nodes / 2);

				for(size_t i = 0; i + 1 < num_nodes; i += 2)
					keys.push_back(InterpretNodeIntoStringIDValueWithReference(ocn[i]));

				std::vector<EvaluableNodeReference> results(num_nodes / 2);

				InterpreterConcurrencyManager concurrency_manager(this, num_nodes / 2, enqueue_task_lock);

				//kick off interpreters
				for(size_t node_index = 0; node_index + 1 < num_nodes; node_index += 2)
					concurrency_manager.EnqueueTaskWithConstructionStack<EvaluableNodeReference>(ocn[node_index + 1],
						en, new_assoc, EvaluableNodeImmediateValueWithType(keys[node_index / 2]),
						nullptr, results[node_index / 2]);

				concurrency_manager.EndConcurrency();

				concurrency_manager.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(new_assoc);

				//add results to assoc
				for(size_t i = 0; i < num_nodes / 2; i++)
				{
					auto key_sid = keys[i];
					auto &value = results[i];

					//add it to the list
					new_assoc->SetMappedChildNodeWithReferenceHandoff(key_sid, value);
				}

				return new_assoc;
			}
		}
	#endif

		//construction stack has a reference, so no KeepNodeReference isn't needed for anything referenced
		PushNewConstructionContext(en, new_assoc, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

		for(size_t i = 0; i < num_nodes; i += 2)
		{
			//get key
			StringInternPool::StringID key_sid = InterpretNodeIntoStringIDValueWithReference(ocn[i], true);

			SetTopCurrentIndexInConstructionStack(key_sid);

			//compute the value, but make sure have another node
			EvaluableNodeReference value = EvaluableNodeReference::Null();
			if(i + 1 < num_nodes)
				value = InterpretNode(ocn[i + 1]);

			//handoff the reference from index_value to the assoc
			new_assoc->SetMappedChildNodeWithReferenceHandoff(key_sid, value);
			new_assoc.UpdatePropertiesBasedOnAttachedNode(value);
		}

		if(PopConstructionContextAndGetExecutionSideEffectFlag())
		{
			new_assoc.unique = false;
			new_assoc.uniqueUnreferencedTopNode = false;
		}
	}

	return new_assoc;
}

static OpcodeInitializer _ENT_ZIP(ENT_ZIP, &Interpreter::InterpretNode_ENT_ZIP, []() {
	OpcodeDetails d;
	d.parameters = R"([* function] list indices [* values])";
	d.returns = R"(assoc)";
	d.description = R"(Evaluates to a new assoc where `indices` are the keys and `values` are the values, with corresponding positions in the list matched.  If the `values` is omitted and only one parameter is specified, then it will use nulls for each of the values.  If `values` is not a list, then all of the values in the assoc returned are set to the same value.  When two parameters are specified, it is the `indices` and `values`.  When three values are specified, it is the `function`, indices, and values.  The parameter `values` defaults to null and `function` defaults to `(lambda (current_value))`.  When there is a collision of indices, `function` is called with a of new target scope pushed onto the stack, so that `(current_value)` accesses a list of elements from the list, `(current_index)` accesses the list or assoc index if it is not already reduced, and `(target)` represents the original list or assoc.  When evaluating `function`, existing indices will be overwritten.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unparse
	(zip
		["a" "b" "c" "d"]
		[1 2 3 4]
	)
))&", R"("{a 1 b 2 c 3 d 4}")"},
			{R"&((unparse
	(zip
		["a" "b" "c" "d"]
	)
))&", R"("{a .null b .null c .null d .null}")"},
			{R"&((unparse
	(zip
		["a" "b" "c" "d"]
		3
	)
))&", R"("{a 3 b (target .true \"a\") c (target .true \"a\") d (target .true \"a\")}")"},
			{R"&((unparse
	(zip
		(lambda (current_value))
		["a" "b" "c" "d" "a"]
		[1 2 3 4 4]
	)
))&", R"("{a 4 b 2 c 3 d 4}")"},
			{R"&((unparse
	(zip
		(lambda
			(+
				(current_value 1)
				(current_value)
			)
		)
		["a" "b" "c" "d" "a"]
		[1 2 3 4 4]
	)
))&", R"("{a 5 b 2 c 3 d 4}")"},
			{R"&((unparse
	(zip
		(lambda
			(+
				(current_value 1)
				(current_value)
			)
		)
		["a" "b" "c" "d" "a"]
		1
	)
))&", R"("{a 2 b 1 c (target .true \"b\") d (target .true \"b\")}")"}
		});
	d.newTargetScope = true;
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 18.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ZIP(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	size_t num_params = ocn.size();
	if(num_params < 1)
		return EvaluableNodeReference::Null();

	//get the indices of the parameters based on how many there are
	size_t index_list_index = 0;
	size_t value_list_index = 1;

	auto node_stack = CreateOpcodeStackStateSaver();

	//if a function is specified, then set up appropriate data structures to call the function and move the indices for the index and value parameters
	EvaluableNodeReference function = EvaluableNodeReference::Null();
	if(num_params == 3)
	{
		index_list_index++;
		value_list_index++;

		function = InterpretNodeForImmediateUse(ocn[0]);
		node_stack.PushEvaluableNode(function);
	}

	//attempt to get indices, the keys of the assoc
	auto index_list = InterpretNodeForImmediateUse(ocn[index_list_index]);
	if(EvaluableNode::IsNull(index_list))
	{
		EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_ASSOC), true);
		return result;
	}

	//attempt to get the value(s) of the assoc
	EvaluableNodeReference value_list = EvaluableNodeReference::Null();
	if(ocn.size() > value_list_index)
	{
		node_stack.PushEvaluableNode(index_list);
		value_list = InterpretNode(ocn[value_list_index]);
		node_stack.PopEvaluableNode();
	}

	//set up the result
	EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_ASSOC), true);
	//values will be placed in, so it should be updated as if it will contain them all
	if(value_list != nullptr)
		result.UpdatePropertiesBasedOnAttachedNode(value_list, true);

	bool value_list_is_a_list = (value_list != nullptr && value_list->GetType() == ENT_LIST);
	bool free_value_list_node = false;

	if(!EvaluableNode::IsNull(function))
	{
		node_stack.PushEvaluableNode(index_list);
		node_stack.PushEvaluableNode(value_list);
	}
	else //not a function
	{
		if(value_list.unique
				&& value_list_is_a_list
				&& !value_list->GetNeedCycleCheck())
			free_value_list_node = true;
	}

	auto &index_list_ocn = index_list->GetOrderedChildNodes();
	result->ReserveMappedChildNodes(index_list_ocn.size());
	for(size_t i = 0; i < index_list_ocn.size(); i++)
	{
		//convert index to string
		EvaluableNode *index = index_list_ocn[i];

		//obtain the index, reusing the sid reference if possible
		StringInternPool::StringID index_sid = string_intern_pool.emptyStringId;
		if(index_list.unique)
			index_sid = EvaluableNode::ToStringIDTakingReferenceAndClearing(index, false, true);
		else
			index_sid = EvaluableNode::ToStringIDWithReference(index, true);

		//get value
		EvaluableNode *value = nullptr;
		if(value_list_is_a_list)
		{
			auto &vl_ocn = value_list->GetOrderedChildNodesReference();
			if(i < vl_ocn.size())
				value = vl_ocn[i];
		}
		else //not a list, so just use the value itself
		{
			value = value_list;
			//reusing the value, so can't be cycle free in the result
			result->SetNeedCycleCheck(true);
		}

		//if no function, then just put value into the appropriate slot for the index
		if(EvaluableNode::IsNull(function))
		{
			result->SetMappedChildNodeWithReferenceHandoff(index_sid, value, true);
		}
		else //has a function, so handle collisions appropriately
		{
			//try to insert without overwriting
			if(!result->SetMappedChildNodeWithReferenceHandoff(index_sid, value, false))
			{
				//collision occurred, so call function
				EvaluableNode **cur_value_ptr = result->GetOrCreateMappedChildNode(index_sid);

				PushNewConstructionContext(nullptr, result, EvaluableNodeImmediateValueWithType(index_sid), *cur_value_ptr);
				PushNewConstructionContext(nullptr, result, EvaluableNodeImmediateValueWithType(index_sid), value);

				EvaluableNodeReference collision_result = InterpretNode(function);

				if(PopConstructionContextAndGetExecutionSideEffectFlag())
				{
					result.unique = false;
					result.uniqueUnreferencedTopNode = false;
				}
				if(PopConstructionContextAndGetExecutionSideEffectFlag())
				{
					result.unique = false;
					result.uniqueUnreferencedTopNode = false;
				}

				*cur_value_ptr = collision_result;
				result.UpdatePropertiesBasedOnAttachedNode(collision_result);
			}
		}
	}

	//the index list has been converted to strings, so therefore can be freed
	evaluableNodeManager->FreeNodeTreeIfPossible(index_list);

	if(free_value_list_node)
		evaluableNodeManager->FreeNodeIfPossible(value_list);

	return result;
}

static OpcodeInitializer _ENT_UNZIP(ENT_UNZIP, &Interpreter::InterpretNode_ENT_UNZIP, []() {
	OpcodeDetails d;
	d.parameters = R"([list|assoc collection] list indices)";
	d.returns = R"(list)";
	d.description = R"(Evaluates to a new list, using `indices` to look up each value from the `collection` in the same order as each index is specified in `indices`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unzip
	[1 2 3]
	[0 -1 1]
))&", R"([1 3 2])"},
			{R"&((unzip
	(associate "a" 1 "b" 2 "c" 3)
	["a" "b"]
))&", R"([1 2])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 8.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNZIP(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto zipped = InterpretNode(ocn[0]);
	if(EvaluableNode::IsNull(zipped))
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);

	auto node_stack = CreateOpcodeStackStateSaver(zipped);
	auto index_list = InterpretNodeForImmediateUse(ocn[1]);
	node_stack.PopEvaluableNode();

	EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_LIST), true);

	if(EvaluableNode::IsNull(index_list))
		return result;

	auto &index_list_ocn = index_list->GetOrderedChildNodes();
	result.UpdatePropertiesBasedOnAttachedNode(zipped, true);
	size_t num_indices = index_list_ocn.size();
	//can't guarantee cycle free since an index could be duplicated
	if(num_indices > 1)
		result.SetNeedCycleCheck(true);

	auto &result_ocn = result->GetOrderedChildNodesReference();
	result_ocn.reserve(num_indices);

	if(EvaluableNode::IsAssociativeArray(zipped))
	{
		auto &zipped_mcn = zipped->GetMappedChildNodesReference();
		for(auto &index : index_list_ocn)
		{
			StringInternPool::StringID index_sid = EvaluableNode::ToStringIDIfExists(index, true);

			auto found_index = zipped_mcn.find(index_sid);
			if(found_index != end(zipped_mcn))
				result_ocn.push_back(found_index->second);
			else
				result_ocn.push_back(nullptr);
		}
	}
	else //ordered list
	{
		auto &zipped_ocn = zipped->GetOrderedChildNodes();
		for(auto &index : index_list_ocn)
		{
			double index_value = EvaluableNode::ToNumber(index);
			if(index_value < 0)
			{
				index_value += zipped_ocn.size();
				if(index_value < 0) //clamp at zero
					index_value = 0;
			}

			if(index_value < zipped_ocn.size())
				result_ocn.push_back(zipped_ocn[static_cast<size_t>(index_value)]);
			else
				result_ocn.push_back(nullptr);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(index_list);
	return result;
}

static OpcodeInitializer _ENT_REVERSE(ENT_REVERSE, &Interpreter::InterpretNode_ENT_REVERSE, []() {
	OpcodeDetails d;
	d.parameters = R"(list collection)";
	d.returns = R"(list)";
	d.description = R"(Returns a new list containing the `collection` with its elements in reversed order.)";
	d.examples = MakeAmalgamExamples({
		{R"&((reverse
	[1 2 3 4 5]
))&", R"([5 4 3 2 1])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_REVERSE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//get the list to reverse
	auto list = InterpretNode(ocn[0]);
	if(list == nullptr)
		return EvaluableNodeReference::Null();

	//make sure it is an editable copy
	evaluableNodeManager->EnsureNodeIsModifiable(list, true);

	auto &list_ocn = list->GetOrderedChildNodes();
	std::reverse(begin(list_ocn), end(list_ocn));

	return list;
}

static OpcodeInitializer _ENT_SORT(ENT_SORT, &Interpreter::InterpretNode_ENT_SORT, []() {
	OpcodeDetails d;
	d.parameters = R"([* function] list|assoc collection [number k])";
	d.returns = R"(list)";
	d.description = "Returns a new list containing the elements from `collection` sorted in increasing order, regardless of whether `collection` is an assoc or list.  If `function` is null or true it sorts ascending, if false it sorts descending, and if any other value it pushes a pair of new scope onto the stack with `(current_value)` and `(current_value 1)` accessing a pair of elements from the list, and evaluates `function`.  The function should return a number, positive if `(current_value)` is greater, negative if `(current_value 1)` is greater, or 0 if equal.  If `k` is specified in addition to `function` and not null, then it will only return the `k` smallest values sorted in order, or, if `k` is negative, it will return the highest `k` values using the absolute value of `k`.";
	d.examples = MakeAmalgamExamples({
		{R"&((sort
	[4 9 3 5 1]
))&", R"([1 3 4 5 9])"},
			{R"&((sort
	{
		a 4
		b 9
		c 3
		d 5
		e 1
	}
))&", R"([1 3 4 5 9])"},
			{R"&((sort
	[
		"n"
		"b"
		"hello"
		"soy"
		4
		1
		3.2
		[1 2 3]
	]
))&", R"([
	1
	3.2
	4
	[1 2 3]
	"b"
	"hello"
	"n"
	"soy"
])"},
			{R"&((sort
	[
		1
		"1x"
		"10"
		20
		"z2"
		"z10"
		"z100"
	]
))&", R"([
	1
	20
	"1x"
	"10"
	"z2"
	"z10"
	"z100"
])"},
			{R"&((sort
	[
		1
		"001x"
		"010"
		20
		"z002"
		"z010"
		"z100"
	]
))&", R"([
	1
	20
	"001x"
	"010"
	"z002"
	"z010"
	"z100"
])"},
			{R"&((sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
))&", R"([1 3 4 5 9])"},
			{R"&((sort
	(lambda
		(- (rand) (rand))
	)
	(range 0 10)
))&", R"([
	8
	10
	6
	9
	7
	5
	1
	0
	2
	4
	3
])"},
			{R"&((sort
	[
		"2020-06-08 lunes 11.33.36"
		"2020-06-08 lunes 11.32.47"
		"2020-06-08 lunes 11.32.49"
		"2020-06-08 lunes 11.32.37"
		"2020-06-08 lunes 11.33.48"
		"2020-06-08 lunes 11.33.40"
		"2020-06-08 lunes 11.33.45"
		"2020-06-08 lunes 11.33.42"
		"2020-06-08 lunes 11.33.47"
		"2020-06-08 lunes 11.33.43"
		"2020-06-08 lunes 11.33.38"
		"2020-06-08 lunes 11.33.39"
		"2020-06-08 lunes 11.32.36"
		"2020-06-08 lunes 11.32.38"
		"2020-06-08 lunes 11.33.37"
		"2020-06-08 lunes 11.32.58"
		"2020-06-08 lunes 11.33.44"
		"2020-06-08 lunes 11.32.48"
		"2020-06-08 lunes 11.32.46"
		"2020-06-08 lunes 11.32.57"
		"2020-06-08 lunes 11.33.41"
		"2020-06-08 lunes 11.32.39"
		"2020-06-08 lunes 11.32.59"
		"2020-06-08 lunes 11.32.56"
		"2020-06-08 lunes 11.33.46"
	]
))&", R"([
	"2020-06-08 lunes 11.32.36"
	"2020-06-08 lunes 11.32.37"
	"2020-06-08 lunes 11.32.38"
	"2020-06-08 lunes 11.32.39"
	"2020-06-08 lunes 11.32.46"
	"2020-06-08 lunes 11.32.47"
	"2020-06-08 lunes 11.32.48"
	"2020-06-08 lunes 11.32.49"
	"2020-06-08 lunes 11.32.56"
	"2020-06-08 lunes 11.32.57"
	"2020-06-08 lunes 11.32.58"
	"2020-06-08 lunes 11.32.59"
	"2020-06-08 lunes 11.33.36"
	"2020-06-08 lunes 11.33.37"
	"2020-06-08 lunes 11.33.38"
	"2020-06-08 lunes 11.33.39"
	"2020-06-08 lunes 11.33.40"
	"2020-06-08 lunes 11.33.41"
	"2020-06-08 lunes 11.33.42"
	"2020-06-08 lunes 11.33.43"
	"2020-06-08 lunes 11.33.44"
	"2020-06-08 lunes 11.33.45"
	"2020-06-08 lunes 11.33.46"
	"2020-06-08 lunes 11.33.47"
	"2020-06-08 lunes 11.33.48"
])"},
			{R"&((sort
	.null
	[4 9 3 5 1]
	2
))&", R"([1 3])"},
			{R"&((sort
	.null
	[4 9 3 5 1]
	-2
))&", R"([5 9])"},
			{R"&((sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
	2
))&", R"([1 3])"},
			{R"&((sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
	-2
))&", R"([9 5])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.newTargetScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 3.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SORT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	size_t list_index = (ocn.size() == 1 ? 0 : 1);

	EvaluableNodeReference function = EvaluableNodeReference::Null();
	EvaluableNodeType function_type = ENT_BOOL;
	bool ascending = true;

	size_t highest_k = 0;
	size_t lowest_k = 0;
	if(ocn.size() > 2)
	{
		double k = InterpretNodeIntoNumberValue(ocn[2]);
		if(k > 0)
			lowest_k = static_cast<size_t>(k);
		else if(k < 0)
			highest_k = static_cast<size_t>(-k);
		//else nan, leave both as zero
	}

	if(ocn.size() >= 2)
	{
		function = InterpretNodeForImmediateUse(ocn[0]);

		if(EvaluableNode::IsNull(function))
		{
			function_type = ENT_BOOL;
		}
		else
		{
			function_type = function->GetType();
			if(function_type == ENT_BOOL)
				ascending = EvaluableNode::ToBool(function);
		}
	}

	if(function_type == ENT_BOOL)
	{
		//get list
		auto list = InterpretNode(ocn[list_index]);
		if(EvaluableNode::IsNull(list))
			return EvaluableNodeReference::Null();

		//make sure it is a clean editable copy and all the data is in a list
		evaluableNodeManager->EnsureNodeIsModifiable(list, true);
		list->ClearMetadata();
		if(list->IsAssociativeArray())
			list->ConvertAssocToList();

		auto &list_ocn = list->GetOrderedChildNodes();

		if(highest_k > 0 && highest_k < list_ocn.size())
		{
			if(ascending)
				std::partial_sort(begin(list_ocn), begin(list_ocn) + highest_k,
					end(list_ocn), EvaluableNode::IsStrictlyGreaterThan);
			else
				std::partial_sort(begin(list_ocn), begin(list_ocn) + highest_k,
					end(list_ocn), EvaluableNode::IsStrictlyLessThan);

			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(size_t i = highest_k; i < list_ocn.size(); i++)
					evaluableNodeManager->FreeNodeTree(list_ocn[i]);
			}

			list_ocn.erase(begin(list_ocn) + highest_k, end(list_ocn));
			std::reverse(begin(list_ocn), end(list_ocn));
		}
		else if(lowest_k > 0 && lowest_k < list_ocn.size())
		{
			if(ascending)
				std::partial_sort(begin(list_ocn), begin(list_ocn) + lowest_k,
					end(list_ocn), EvaluableNode::IsStrictlyLessThan);
			else
				std::partial_sort(begin(list_ocn), begin(list_ocn) + lowest_k,
					end(list_ocn), EvaluableNode::IsStrictlyGreaterThan);

			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(size_t i = lowest_k; i < list_ocn.size(); i++)
					evaluableNodeManager->FreeNodeTree(list_ocn[i]);
			}

			list_ocn.erase(begin(list_ocn) + lowest_k, end(list_ocn));
		}
		else
		{
			if(ascending)
				std::sort(begin(list_ocn), end(list_ocn), EvaluableNode::IsStrictlyLessThan);
			else
				std::sort(begin(list_ocn), end(list_ocn), EvaluableNode::IsStrictlyGreaterThan);
		}

		return list;
	}
	else
	{
		auto node_stack = CreateOpcodeStackStateSaver(function);

		//get list
		auto list = InterpretNode(ocn[list_index]);
		if(EvaluableNode::IsNull(list))
			return EvaluableNodeReference::Null();

		//make sure it is an editable copy
		evaluableNodeManager->EnsureNodeIsModifiable(list, true);
		list->ClearMetadata();
		if(list->IsAssociativeArray())
			list->ConvertAssocToList();

		CustomEvaluableNodeComparator comparator(this, function, list);

		//sort list; can't use the C++ sort function because it requires weak ordering and will crash otherwise
		// the custom comparator does not guarantee this
		std::vector<EvaluableNode *> sorted = CustomEvaluableNodeOrderedChildNodesSort(list->GetOrderedChildNodes(), comparator);

		if(highest_k > 0 && highest_k < sorted.size())
		{
			sorted.erase(begin(sorted), begin(sorted) + (sorted.size() - highest_k));
			std::reverse(begin(sorted), end(sorted));
		}
		else if(lowest_k > 0 && lowest_k < sorted.size())
		{
			sorted.erase(begin(sorted) + lowest_k, end(sorted));
		}

		list->SetOrderedChildNodes(std::move(sorted), list->GetNeedCycleCheck(), list->GetIsIdempotent());

		if(comparator.DidAnyComparisonHaveExecutionSideEffects())
		{
			list.unique = false;
			list.uniqueUnreferencedTopNode = false;
		}

		return list;
	}
}

static OpcodeInitializer _ENT_CURRENT_INDEX(ENT_CURRENT_INDEX, &Interpreter::InterpretNode_ENT_CURRENT_INDEX, []() {
	OpcodeDetails d;
	d.parameters = R"([number stack_distance])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the index of the current node being iterated on within the current target.  If `stack_distance` is specified, it climbs back up the target stack that many levels.)";
	d.examples = MakeAmalgamExamples({
		{R"&([0 1 2 3 (current_index) 5])&", R"([0 1 2 3 4 5])"},
		{R"&([
	0
	1
	[
		0
		1
		2
		3
		(current_index 1)
		4
	]
])&", R"([
	0
	1
	[0 1 2 3 2 4]
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 31.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CURRENT_INDEX(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	size_t depth = 0;
	if(ocn.size() > 0)
	{
		double value = InterpretNodeIntoNumberValue(ocn[0]);
		if(value >= 0)
			depth = static_cast<size_t>(value);
		else
			return EvaluableNodeReference::Null();
	}

	//make sure have a large enough stack
	if(depth >= constructionStackIndicesAndUniqueness.size())
		return EvaluableNodeReference::Null();

	//depth is 1-based
	size_t offset = constructionStackIndicesAndUniqueness.size() - depth - 1;

	//build the index node to return
	EvaluableNodeImmediateValueWithType enivwt(constructionStackIndicesAndUniqueness[offset].index);
	if(enivwt.nodeType == ENIVT_NUMBER)
	{
		return AllocReturn(enivwt.nodeValue.number, immediate_result);
	}
	else if(enivwt.nodeType == ENIVT_STRING_ID)
	{
		if(immediate_result.AnyImmediateType())
		{
			//parse into key, which may be the same StringID if not escaped and desired to be in an immediate format
			auto cur_index_sid = Parser::ParseFromKeyStringIdToStringIdWithReference(enivwt.nodeValue.stringID);
			return EvaluableNodeReference(cur_index_sid, true);
		}
		return Parser::ParseFromKeyStringId(enivwt.nodeValue.stringID, evaluableNodeManager);
	}
	else
	{
		return EvaluableNodeReference::Null();
	}
}

static OpcodeInitializer _ENT_CURRENT_VALUE(ENT_CURRENT_VALUE, &Interpreter::InterpretNode_ENT_CURRENT_VALUE, []() {
	OpcodeDetails d;
	d.parameters = R"([number stack_distance])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the current node being iterated on within the current target.  If `stack_distance` is specified, it climbs back up the target stack that many levels.)";
	d.examples = MakeAmalgamExamples({
		{R"&((map
	(lambda
		(* 2 (current_value))
	)
	(range 0 4)
))&", R"([0 2 4 6 8])"},
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.frequencyPer10000Opcodes = 77.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CURRENT_VALUE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	size_t depth = 0;
	if(ocn.size() > 0)
	{
		double value = InterpretNodeIntoNumberValue(ocn[0]);
		if(value >= 0)
			depth = static_cast<size_t>(value);
		else
			return EvaluableNodeReference::Null();
	}

	//make sure have a large enough stack
	if(depth >= constructionStackIndicesAndUniqueness.size())
		return EvaluableNodeReference::Null();

	size_t offset = constructionStackNodes.size() - (constructionStackOffsetStride * depth) + constructionStackOffsetCurrentValue;
	return EvaluableNodeReference(constructionStackNodes[offset], false);
}

static OpcodeInitializer _ENT_PREVIOUS_RESULT(ENT_PREVIOUS_RESULT, &Interpreter::InterpretNode_ENT_PREVIOUS_RESULT, []() {
	OpcodeDetails d;
	d.parameters = R"([number stack_distance] [bool copy])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the resulting node of the previous iteration for applicable opcodes. If `stack_distance` is specified, it climbs back up the target stack that many levels.  If `copy` is true, which is false by default, then a copy of the resulting node of the previous iteration is returned, otherwise the result of the previous iteration is returned directly and consumed.)";
	d.examples = MakeAmalgamExamples({
		{R"&((while
	(< (current_index) 3)
	(append (previous_result) (current_index))
))&", R"([.null 0 1 2])"},
			{R"&((while
	(< (current_index) 3)
	(if
		(= (current_index) 0)
		3
		(append
			(previous_result 0 .true)
			(previous_result 0)
			(previous_result 0)
		)
	)
))&", R"([
	3
	3
	.null
	3
	3
	.null
	.null
])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_PREVIOUS_RESULT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	size_t depth = 0;
	if(ocn.size() > 0)
	{
		double value = InterpretNodeIntoNumberValue(ocn[0]);
		if(value >= 0)
			depth = static_cast<size_t>(value);
		else
			return EvaluableNodeReference::Null();
	}

	bool make_copy = false;
	if(ocn.size() > 1)
		//defaults to false if ENT_NULL
		make_copy = InterpretNodeIntoBoolValue(ocn[1]);

	//make sure have a large enough stack
	if(depth >= constructionStackIndicesAndUniqueness.size())
		return EvaluableNodeReference::Null();

	if(make_copy)
		return CopyPreviousResultInConstructionStack(depth);
	else
		return GetAndClearPreviousResultInConstructionStack(depth);
}
