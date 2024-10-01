//project headers:
#include "Interpreter.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EvaluableNodeTreeDifference.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"
#include "PerformanceProfiler.h"

//system headers:
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_FIRST(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

				evaluableNodeManager->FreeNode(list);
			}
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

				evaluableNodeManager->FreeNode(list);
			}

			return EvaluableNodeReference(first_en, list.unique);
		}
	}
	else //if(list->IsImmediate())
	{
		if(DoesEvaluableNodeTypeUseStringData(list->GetType()))
		{
			auto sid = list->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID  || sid == string_intern_pool.emptyStringId)
				return AllocReturn(StringInternPool::NOT_A_STRING_ID, immediate_result);

			std::string s = string_intern_pool.GetStringFromID(sid);
			size_t utf8_char_length = StringManipulation::GetUTF8CharacterLength(s, 0);
			std::string substring = s.substr(0, utf8_char_length);
			return ReuseOrAllocReturn(list, substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			//return 1 if nonzero
			return ReuseOrAllocReturn(list, 1.0, immediate_result);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TAIL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
			if(!list.unique)
			{
				//make a copy so can edit node
				evaluableNodeManager->EnsureNodeIsModifiable(list);
				node_stack.PopEvaluableNode();
				node_stack.PushEvaluableNode(list);
			}

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
			if(!list.unique)
			{
				//make a copy so can edit node
				evaluableNodeManager->EnsureNodeIsModifiable(list);
				node_stack.PopEvaluableNode();
				node_stack.PushEvaluableNode(list);
			}

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
			if(sid == string_intern_pool.NOT_A_STRING_ID  || sid == string_intern_pool.emptyStringId)
				return AllocReturn(StringInternPool::NOT_A_STRING_ID, immediate_result);

			std::string s = string_intern_pool.GetStringFromID(sid);

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
			return ReuseOrAllocReturn(list, substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			return ReuseOrAllocReturn(list, value - 1.0, immediate_result);
		}
	}
	
	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LAST(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

				evaluableNodeManager->FreeNode(list);
			}
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

				evaluableNodeManager->FreeNode(list);
			}

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

			std::string s = string_intern_pool.GetStringFromID(sid);

			auto [utf8_char_start_offset, utf8_char_length] = StringManipulation::GetLastUTF8CharacterOffsetAndLength(s);

			std::string substring = s.substr(utf8_char_start_offset, utf8_char_length);
			return ReuseOrAllocReturn(list, substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			return ReuseOrAllocReturn(list, 1.0, immediate_result);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TRUNC(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
		if(!list.unique)
		{
			//make a copy so can edit node
			evaluableNodeManager->EnsureNodeIsModifiable(list);
			node_stack.PopEvaluableNode();
			node_stack.PushEvaluableNode(list);
		}

		auto &list_ocn = list->GetOrderedChildNodesReference();

		//remove the last element(s)
		if(truncate_to > 0 && truncate_to < list_ocn.size())
		{
			list->GetOrderedChildNodes().erase(begin(list_ocn) + static_cast<size_t>(truncate_to), end(list_ocn));
		}
		else if(truncate_to < 0)
		{
			//make sure have things to remove while keeping something in the list
			if(-truncate_to < list_ocn.size())
			{
				size_t last_index = static_cast<size_t>(truncate_to + list->GetOrderedChildNodes().size());
				list_ocn.erase(begin(list_ocn) + last_index, end(list_ocn));
			}
			else //remove everything
				list_ocn.clear();
		}

		return list;
	}
	else if(list->IsAssociativeArray())
	{
		if(!list.unique)
		{
			//make a copy so can edit node
			evaluableNodeManager->EnsureNodeIsModifiable(list);
			node_stack.PopEvaluableNode();
			node_stack.PushEvaluableNode(list);
		}

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
			if(sid == string_intern_pool.NOT_A_STRING_ID  || sid == string_intern_pool.emptyStringId)
				return AllocReturn(StringInternPool::NOT_A_STRING_ID, immediate_result);

			std::string s = string_intern_pool.GetStringFromID(sid);

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
			return ReuseOrAllocReturn(list, substring, immediate_result);
		}

		if(DoesEvaluableNodeTypeUseNumberData(list->GetType()))
		{
			//return 0 if zero
			double value = list->GetNumberValueReference();
			if(value == 0.0)
				return list;

			//return (value - 1.0) if nonzero
			return ReuseOrAllocReturn(list, value - 1.0, immediate_result);
		}
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(list);
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_APPEND(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference new_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	auto node_stack = CreateOpcodeStackStateSaver(new_list);

	size_t new_list_cur_index = 0;
	bool first_append = true;
	for(auto &param : ocn)
	{
		if(AreExecutionResourcesExhausted())
			return EvaluableNodeReference::Null();

		//get evaluated parameter
		auto new_elements = InterpretNode(param);

		if(EvaluableNode::IsAssociativeArray(new_elements))
		{
			if(new_list->GetType() == ENT_LIST)
				new_list->ConvertOrderedListToNumberedAssoc();

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
					new_list->GetOrderedChildNodes().insert(
						end(new_list->GetOrderedChildNodes()), begin(new_elements_ocn), end(new_elements_ocn));
				}
				else
				{
					//find the lowest unused index number
					for(size_t i = 0; i < new_elements_ocn.size(); i++, new_list_cur_index++)
					{
						//look for first index not used
						std::string index_string = EvaluableNode::NumberToString(new_list_cur_index);
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
				do {
					index_string = EvaluableNode::NumberToString(static_cast<size_t>(new_list_cur_index++));
				} while(new_list->GetMappedChildNode(index_string) != nullptr);

				new_list->SetMappedChildNode(index_string, new_elements);
			}
		}

		first_append = false;
	} //for each child node to append

	return new_list;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SIZE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNodeForImmediateUse(ocn[0]);
	size_t size = 0;
	if(cur != nullptr)
	{
		if(cur->GetType() == ENT_STRING)
		{
			auto s = cur->GetStringValue();
			size = StringManipulation::GetNumUTF8Characters(s);
		}
		else
		{
			size = cur->GetNumChildNodes();
		}
	}

	return ReuseOrAllocReturn(cur, static_cast<double>(size), immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RANGE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

	//make sure not eating up too much memory
	if(ConstrainedAllocatedNodes())
	{
		if(performanceConstraints->WouldNewAllocatedNodesExceedConstraint(
				evaluableNodeManager->GetNumberOfUsedNodes() + num_nodes))
		return EvaluableNodeReference::Null();
	}

	//if no function, just return a list of numbers
	if(index_of_start == 0)
	{
		EvaluableNodeReference range_list(evaluableNodeManager->AllocListNodeWithOrderedChildNodes(ENT_NUMBER, num_nodes), true);

		auto &range_list_ocn = range_list->GetOrderedChildNodesReference();
		for(size_t i = 0; i < num_nodes; i++)
			range_list_ocn[i]->SetTypeViaNumberValue(i * range_step_size + range_start);

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
		result.unique = false;

	return result;
}
