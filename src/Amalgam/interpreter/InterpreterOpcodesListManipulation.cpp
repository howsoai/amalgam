//project headers:
#include "Interpreter.h"

//system headers:
#include <utility>









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

			ConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

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
