//project headers:
#include "Interpreter.h"

#include "EvaluableNodeTreeFunctions.h"

//system headers:
#include <algorithm>
#include <regex>

EvaluableNodeReference Interpreter::InterpretNode_ENT_REWRITE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_MAP(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

					ConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

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

					ConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

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
		auto &inputs = inputs_list_node->GetOrderedChildNodes();

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
			result->GetOrderedChildNodes().resize(largest_size);

			PushNewConstructionContext(inputs_list_node, result, EvaluableNodeImmediateValueWithType(0.0), nullptr);

			for(size_t index = 0; index < largest_size; index++)
			{
				//set index value
				SetTopCurrentIndexInConstructionStack(static_cast<double>(index));

				//combine input slices together into value
				EvaluableNode *input_slice = evaluableNodeManager->AllocNode(ENT_LIST);
				auto &is_ocn = input_slice->GetOrderedChildNodes();
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
				result->GetOrderedChildNodes()[index] = element_result;
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
				auto &is_ocn = input_slice->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_FILTER(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	if(ocn.size() == 1)
	{
		//specialized path for immediate result just getting the count
		if(immediate_result)
		{
			auto list = InterpretNodeForImmediateUse(ocn[0]);
			if(list == nullptr)
				return EvaluableNodeReference::Null();

			size_t num_elements_not_filtered = 0;
			if(list->IsAssociativeArray())
			{
				auto &list_mcn = list->GetMappedChildNodesReference();
				for(auto &[cn_id, cn] : list_mcn)
				{
					if(!EvaluableNode::IsNull(cn))
						num_elements_not_filtered++;
				}
			}
			else if(list->IsOrderedArray())
			{
				auto &list_ocn = list->GetOrderedChildNodesReference();
				for(auto &cn : list_ocn)
				{
					if(!EvaluableNode::IsNull(cn))
						num_elements_not_filtered++;
				}
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(list);
			return EvaluableNodeReference(static_cast<double>(num_elements_not_filtered));
		}

		auto list = InterpretNode(ocn[0]);
		if(list == nullptr)
			return EvaluableNodeReference::Null();

		EvaluableNodeReference result_list(list, list.unique, list.uniqueUnreferencedTopNode);

		//need to edit the list itself, so if not unique, make at least the top node unique
		evaluableNodeManager->EnsureNodeIsModifiable(result_list);

		if(result_list->IsAssociativeArray())
		{
			auto &result_list_mcn = result_list->GetMappedChildNodesReference();

			std::vector<StringInternPool::StringID> ids_to_remove;
			for(auto &[cn_id, cn] : result_list_mcn)
			{
				if(EvaluableNode::IsNull(cn))
					ids_to_remove.push_back(cn_id);
			}

			string_intern_pool.DestroyStringReferences(ids_to_remove);
			if(result_list.unique && !result_list->GetNeedCycleCheck())
			{
				//FreeNodeTree and erase the key
				for(auto &id : ids_to_remove)
				{
					auto pair = result_list_mcn.find(id);
					evaluableNodeManager->FreeNodeTree(pair->second);
					result_list_mcn.erase(pair);
				}
			}
			else //can't safely delete any nodes
			{
				for(auto &id : ids_to_remove)
					result_list_mcn.erase(id);
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
					if(!EvaluableNode::IsNull(result_list_ocn[index]))
						continue;

					evaluableNodeManager->FreeNodeTree(result_list_ocn[index]);
					result_list_ocn.erase(begin(result_list_ocn) + index);
				}
			}
			else //can't safely delete any nodes
			{
				auto new_end = std::remove_if(begin(result_list_ocn), end(result_list_ocn),
					[](EvaluableNode *en) { return EvaluableNode::IsNull(en); });
				result_list_ocn.erase(new_end, end(result_list_ocn));
			}
		}

		return result_list;
	}

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(function);

	//get list
	auto list = InterpretNode(ocn[1]);
	//if null, just return a new null, since it has no child nodes
	if(list == nullptr)
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
		auto &list_ocn = list->GetOrderedChildNodes();
		auto &result_ocn = result_list->GetOrderedChildNodes();

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

				ConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

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

				ConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_WEAVE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_REDUCE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
		auto &list_ocn = list->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_APPLY(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
	bool transient_source_node = (!DoesOpcodeHaveSideEffects(new_type)
		&& GetOpcodeNewValueReturnType(new_type) == ONVRT_NEW_VALUE);
	EvaluableNodeReference source;

	if(transient_source_node)
		source = InterpretNodeForImmediateUse(ocn[1]);
	else
		source = InterpretNode(ocn[1]);

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

	if(transient_source_node)
		evaluableNodeManager->FreeNodeTreeIfPossible(source);

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_REVERSE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//get the list to reverse
	auto list = InterpretNode(ocn[0]);
	if(list == nullptr)
		return EvaluableNodeReference::Null();

	//make sure it is an editable copy
	evaluableNodeManager->EnsureNodeIsModifiable(list);

	auto &list_ocn = list->GetOrderedChildNodes();
	std::reverse(begin(list_ocn), end(list_ocn));

	return list;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SORT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	size_t list_index = (ocn.size() == 1 ? 0 : 1);

	EvaluableNodeReference function = EvaluableNodeReference::Null();
	size_t highest_k = 0;
	size_t lowest_k = 0;

	if(ocn.size() == 3)
	{
		double k = InterpretNodeIntoNumberValue(ocn[2]);
		if(k > 0)
			lowest_k = static_cast<size_t>(k);
		else if(k < 0)
			highest_k = static_cast<size_t>(-k);
		//else nan, leave both as zero
	}
	
	if(ocn.size() >= 2)
		function = InterpretNodeForImmediateUse(ocn[0]);

	if(EvaluableNode::IsNull(function))
	{
		//get list
		auto list = InterpretNode(ocn[list_index]);
		if(EvaluableNode::IsNull(list))
			return EvaluableNodeReference::Null();

		//make sure it is an editable copy
		evaluableNodeManager->EnsureNodeIsModifiable(list);

		auto &list_ocn = list->GetOrderedChildNodes();

		if(highest_k > 0 && highest_k < list_ocn.size())
		{
			std::partial_sort(begin(list_ocn),
				begin(list_ocn) + highest_k,
				end(list_ocn), EvaluableNode::IsStrictlyGreaterThan);

			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(size_t i = highest_k; i < list_ocn.size(); i++)
					evaluableNodeManager->FreeNodeTree(list_ocn[i]);
			}

			list_ocn.erase(begin(list_ocn) + highest_k, end(list_ocn));
		}
		else if(lowest_k > 0 && lowest_k < list_ocn.size())
		{
			std::partial_sort(begin(list_ocn), begin(list_ocn) + lowest_k,
				end(list_ocn), EvaluableNode::IsStrictlyLessThan);

			if(list.unique && !list->GetNeedCycleCheck())
			{
				for(size_t i = lowest_k; i < list_ocn.size(); i++)
					evaluableNodeManager->FreeNodeTree(list_ocn[i]);
			}

			list_ocn.erase(begin(list_ocn) + lowest_k, end(list_ocn));
		}
		else
		{
			std::sort(begin(list_ocn), end(list_ocn), EvaluableNode::IsStrictlyLessThan);
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
		evaluableNodeManager->EnsureNodeIsModifiable(list);

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

		list->SetOrderedChildNodes(sorted, list->GetNeedCycleCheck(), list->GetIsIdempotent());

		if(comparator.DidAnyComparisonHaveExecutionSideEffects())
		{
			list.unique = false;
			list.uniqueUnreferencedTopNode = false;
		}

		return list;
	}
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_INDICES(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//get assoc array to look up
	auto container = InterpretNodeForImmediateUse(ocn[0]);

	if(container == nullptr)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);

	EvaluableNodeReference index_list;

	if(container->IsAssociativeArray())
	{
		auto &container_mcn = container->GetMappedChildNodesReference();
		index_list.SetReference(evaluableNodeManager->AllocNode(ENT_LIST));

		auto &index_list_ocn = index_list->GetOrderedChildNodesReference();
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
		index_list.SetReference(evaluableNodeManager->AllocNode(ENT_LIST));

		auto &index_list_ocn = index_list->GetOrderedChildNodesReference();
		index_list_ocn.resize(num_ordered_nodes);
		for(size_t i = 0; i < num_ordered_nodes; i++)
			index_list_ocn[i] = evaluableNodeManager->AllocNode(static_cast<double>(i));
	}
	else //no child nodes, just alloc an empty list
		index_list.SetReference(evaluableNodeManager->AllocNode(ENT_LIST));

	//none of the original container is needed
	evaluableNodeManager->FreeNodeTreeIfPossible(container);

	return index_list;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_VALUES(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	bool only_unique_values = false;
	if(ocn.size() >= 2)
		only_unique_values = InterpretNodeIntoBoolValue(ocn[1]);

	//get assoc array to look up
	auto container = InterpretNode(ocn[0]);

	//make new list containing the values
	EvaluableNode *result = evaluableNodeManager->AllocNode(ENT_LIST);

	if(container == nullptr)
		return EvaluableNodeReference(result, true);

	if(!only_unique_values)
	{
		result->ReserveOrderedChildNodes(container->GetNumChildNodes());
		if(container->IsOrderedArray())
		{
			auto &container_ocn = container->GetOrderedChildNodesReference();
			result->AppendOrderedChildNodes(container_ocn);
		}
		else if(container->IsAssociativeArray())
		{
			for(auto &[_, cn] : container->GetMappedChildNodesReference())
				result->AppendOrderedChildNode(cn);
		}

		//just in case there are duplicated values, ensure cycle check is propagated
		if(container->GetNeedCycleCheck())
			result->GetNeedCycleCheck();
	}
	else //only_unique_values
	{
		//if noncyclic data, simple container, and sufficiently few nodes for an n^2 comparison
		// just do the lower overhead check with more comparisons
		if(!container->GetNeedCycleCheck() && !container->IsAssociativeArray() && container->GetNumChildNodes() < 10)
		{
			auto &container_ocn = container->GetOrderedChildNodes();
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
			}
		}
		else //use a hash-set and look up stringified values for collisions
		{
			//attempt to emplace/insert the unparsed node into values_in_existence, and if successful, append the value
			FastHashSet<std::string> values_in_existence;

			if(container->IsOrderedArray())
			{
				for(auto &n : container->GetOrderedChildNodesReference())
				{
					std::string str_value = Parser::UnparseToKeyString(n);
					if(values_in_existence.emplace(str_value).second)
						result->AppendOrderedChildNode(n);
				}
			}
			else if(container->IsAssociativeArray())
			{
				for(auto &[_, cn] : container->GetMappedChildNodesReference())
				{
					std::string str_value = Parser::UnparseToKeyString(cn);
					if(values_in_existence.emplace(str_value).second)
						result->AppendOrderedChildNode(cn);
				}
			}

		}

		//shouldn't have duplicated values, so don't need a cycle check on the top node
	}

	//the container itself isn't needed
	evaluableNodeManager->FreeNodeIfPossible(container);

	return EvaluableNodeReference(result, container.unique, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_INDEX(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_VALUE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//get assoc array to look up
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
		try {
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_REMOVE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//get assoc array to look up
	auto container = InterpretNode(ocn[0]);
	if(container == nullptr)
		return EvaluableNodeReference::Null();
	//make sure it's editable
	evaluableNodeManager->EnsureNodeIsModifiable(container);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_KEEP(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//get assoc array to look up
	auto container = InterpretNode(ocn[0]);
	if(container == nullptr)
		return EvaluableNodeReference::Null();
	//make sure it's editable
	evaluableNodeManager->EnsureNodeIsModifiable(container);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSOCIATE(EvaluableNode *en, bool immediate_result)
{
	//use stack to lock it in place, but copy it back to temporary before returning
	EvaluableNodeReference new_assoc(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

	auto &ocn = en->GetOrderedChildNodes();
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

				ConcurrencyManager concurrency_manager(this, num_nodes / 2, enqueue_task_lock);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_ZIP(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNZIP(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
