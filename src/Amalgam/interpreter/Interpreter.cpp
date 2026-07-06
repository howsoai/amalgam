//project headers:
#include "AssetManager.h"
#include "EvaluableNodeTreeFunctions.h"
#include "Interpreter.h"
#include "InterpreterConcurrencyManager.h"
#include "OpcodeDetails.h"
#include "PerformanceProfiler.h"
#include "StringInternPool.h"

//system headers:
#include <utility>

UninitializedArray<Interpreter::OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> Interpreter::_opcodes;
EvaluableNodeReference Interpreter::_null_reference = EvaluableNodeReference::Null();

Interpreter::Interpreter(EvaluableNodeManager *enm, RandomStream rand_stream,
	std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
	InterpreterConstraints *interpreter_constraints, Entity *t, Interpreter *calling_interpreter)
{
	interpreterConstraints = interpreter_constraints;

	randomStream = rand_stream;
	curEntity = t;
	callingInterpreter = calling_interpreter;
	writeListeners = write_listeners;
	printListener = print_listener;

	evaluableNodeManager = enm;
#ifdef MULTITHREAD_SUPPORT
	bottomOfScopeStack = true;
#endif
}

EvaluableNodeReference Interpreter::ExecuteNode(EvaluableNode *en,
	std::vector<EvaluableNode *> *scope_stack,
	std::vector<EvaluableNode *> *opcode_stack, std::vector<ConstructionStackEntry> *construction_stack,
	EvaluableNodeRequestedValueTypes immediate_result
#ifdef MULTITHREAD_SUPPORT
	, bool new_scope_stack
#endif
)
{
	//use specified or create new scopeStack
	if(scope_stack == nullptr)
	{
	#ifdef MULTITHREAD_SUPPORT
		bottomOfScopeStack = new_scope_stack;
		if(new_scope_stack)
		{
	#endif

			EvaluableNode *new_context_entry = evaluableNodeManager->AllocNode(ENT_ASSOC);
			new_context_entry->SetNeedCycleCheck(true);
			scopeStack.clear();
			scopeStack.push_back(new_context_entry);
	#ifdef MULTITHREAD_SUPPORT
		}
	#endif
	}
	else
	{
		scopeStack = std::move(*scope_stack);
	}

	if(opcode_stack == nullptr)
		opcodeStackNodes.clear();
	else
		opcodeStackNodes = std::move(*opcode_stack);

	if(construction_stack == nullptr)
		constructionStack.clear();
	else
		constructionStack = std::move(*construction_stack);

	evaluableNodeManager->AddActiveInterpreter(this);
	auto retval = InterpretNode(en, immediate_result);
	evaluableNodeManager->RemoveActiveInterpreter(this);

	return retval;
}

void Interpreter::InterpretAndPushNewScopeStackNode(EvaluableNode *new_scope_node)
{
	EvaluableNodeReference new_scope = EvaluableNodeReference::Null();
	bool need_to_interpret_new_scope = false;
	if(new_scope_node != nullptr)
	{
		if(new_scope_node->IsAssociativeArray())
		{
			new_scope = EvaluableNodeReference(new_scope_node, false);
			need_to_interpret_new_scope = !new_scope_node->GetIsIdempotent();
		}
		else //just need to interpret
		{
			new_scope = InterpretNodeWithoutCopyingImmediates(new_scope_node);
		}
	}

	if(EvaluableNode::IsAssociativeArray(new_scope))
	{
		evaluableNodeManager->EnsureNodeIsModifiable(new_scope, true, false);
		new_scope->SetIsFreeable(true);
		//just in case a variable is added which needs cycle checks
		new_scope->SetNeedCycleCheck(true);

		if(!need_to_interpret_new_scope)
		{
			auto &new_scope_mcn = new_scope->GetMappedChildNodesReference();
			if(new_scope.unique)
			{
				for(auto &[id, cn] : new_scope_mcn)
				{
					if(cn != nullptr)
						cn->SetIsFreeable(true);
				}
			}
			else //!new_scope.unique
			{
				if(new_scope_mcn.size() > 0)
				{
					//set not freeable in case referenced elsewhere
					for(auto &[id, cn] : new_scope_mcn)
					{
						if(cn != nullptr)
						#ifdef MULTITHREAD_SUPPORT
							//not unique, so should set atomically if other threads may be accessing it
							cn->SetIsFreeableAtomic(false);
						#else
							cn->SetIsFreeable(false);
						#endif
					}
				}
			}
		}
		else //need_to_interpret_new_scope
		{
			//need to interpret nodes
			PushNewConstructionContext(new_scope_node, new_scope,
					EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

			for(auto &[cn_id, cn] : new_scope->GetMappedChildNodesReference())
			{
				if(cn == nullptr || cn->GetIsIdempotent())
					continue;

				//need to interpret
				SetTopCurrentIndexInConstructionStack(cn_id);
				EvaluableNodeReference value = InterpretNodeWithoutCopyingImmediates(cn);

				if(value != nullptr)
				{
					if(value.unique)
						value->SetIsFreeable(true);
					else
					#ifdef MULTITHREAD_SUPPORT
						//not unique, so should set atomically if other threads may be accessing it
						value->SetIsFreeableAtomic(false);
					#else
						value->SetIsFreeable(false);
					#endif
				}

				cn = value;
				new_scope->UpdateFlagsBasedOnNewChildNode(cn);
			}

			//if there was a side-effect, then need to make another copy of the context in case something is referencing it
			if(PopConstructionContextAndGetExecutionSideEffectFlag() || !new_scope->GetIsFreeable())
			{
				new_scope = EvaluableNodeReference(evaluableNodeManager->AllocNode(new_scope, false), false, true);
				new_scope->SetIsFreeable(true);
			}
		}
	}
	else //not assoc, make a new one
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(new_scope);
		new_scope = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

		//start to check things as being freeable unless cleared/invalidated later
		new_scope->SetIsFreeable(true);
		//just in case a variable is added which needs cycle checks
		new_scope->SetNeedCycleCheck(true);
	}

	scopeStack.push_back(new_scope);
}

//pops the top context off the stack
//if returning_unique_value, then can potentially free the whole scope
void Interpreter::PopScopeStack(bool returning_unique_value)
{
	EvaluableNode *scope = scopeStack.back();

	if(returning_unique_value && scope->GetIsFreeable())
	{
		for(auto &[id, cn] : scope->GetMappedChildNodesReference())
		{
			if(cn != nullptr && cn->GetIsFreeable())
				evaluableNodeManager->FreeNodeTree(cn);
		}
	}

	if(scope->GetIsFreeable())
		evaluableNodeManager->FreeNode(scope);
	scopeStack.pop_back();
}

EvaluableNode *Interpreter::GetScopeStackGivenDepth(size_t depth
#ifdef MULTITHREAD_SUPPORT
	, bool use_atomic_when_setting_access_flag
#endif
)
{
	EvaluableNode *scope_stack = nullptr;
	size_t ss_size = scopeStack.size();
	if(ss_size > depth)
		scope_stack = scopeStack[ss_size - (depth + 1)];

#ifdef MULTITHREAD_SUPPORT
	//need to search further down the stack if appropriate
	if(!bottomOfScopeStack && callingInterpreter != nullptr)
		scope_stack = callingInterpreter->GetScopeStackGivenDepth(depth - ss_size, true);
#endif

	if(scope_stack != nullptr)
	{
	#ifdef MULTITHREAD_SUPPORT
		if(use_atomic_when_setting_access_flag)
			scope_stack->SetIsFreeableAtomic(false);
		else
	#endif
			scope_stack->SetIsFreeable(false);
	}

	return scope_stack;
}

EvaluableNode *Interpreter::MakeCopyOfScopeStack()
{
	EvaluableNode stack_top_holder(ENT_LIST);
	stack_top_holder.SetOrderedChildNodes(begin(scopeStack), end(scopeStack), true, false);
	//set flags conservatively before copy
	stack_top_holder.SetNeedCycleCheck(true);
	stack_top_holder.SetIsIdempotent(false);

	EvaluableNodeReference copied_stack = evaluableNodeManager->DeepAllocCopy(&stack_top_holder);

#ifdef MULTITHREAD_SUPPORT
	//copy the rest of the stack if there is more
	if(!bottomOfScopeStack)
	{
		auto &stack_nodes_ocn = copied_stack->GetOrderedChildNodesReference();
		for(Interpreter *interp = callingInterpreter; interp != nullptr; interp = interp->callingInterpreter)
		{
			stack_nodes_ocn.insert(begin(stack_nodes_ocn), scopeStack.size(), nullptr);
			for(size_t i = 0; i < scopeStack.size(); i++)
				stack_nodes_ocn[i] = evaluableNodeManager->DeepAllocCopy(scopeStack[i]);

			if(interp->bottomOfScopeStack)
				break;
		}
	}
#endif

	return copied_stack;
}

void Interpreter::SetSideEffectFlagsAndAccumulatePerformanceCounters(EvaluableNode *node)
{
	auto [any_constructions, initial_side_effect] = SetSideEffectsFlags();
	if(_opcode_profiling_enabled && any_constructions)
	{
		std::string variable_location = asset_manager.GetEvaluableNodeSourceFromComments(node);
		PerformanceProfiler::AccumulateTotalSideEffectMemoryWrites(variable_location);
		if(initial_side_effect)
			PerformanceProfiler::AccumulateInitialSideEffectMemoryWrites(variable_location);
	}
}

EvaluableNodeReference Interpreter::InterpretNode(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(EvaluableNode::IsNull(en))
		return EvaluableNodeReference::Null();

	//reference this node before we collect garbage
	//CreateOpcodeStackStateSaver is a bit expensive for this frequently called function
	//especially because only one node is kept
	opcodeStackNodes.push_back(en);

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	CollectGarbage();

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	//get corresponding opcode taking into account resource constraints
	EvaluableNodeType ent = (!AreExecutionResourcesExhausted(true) ? en->GetType() : ENT_NULL);

	auto oc = _opcodes[ent];
	EvaluableNodeReference retval = (this->*oc)(en, immediate_result);

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	//finished with opcode
	opcodeStackNodes.pop_back();

	return retval;
}

EvaluableNode *Interpreter::GetCurrentScopeStackContext()
{
	//this should not happen, but just in case
	if(scopeStack.size() < 1)
		return nullptr;

	return scopeStack.back();
}

std::pair<bool, std::string> Interpreter::InterpretNodeIntoStringValue(EvaluableNode *n, bool key_string)
{
	if(EvaluableNode::IsNull(n))
		return std::make_pair(false, "");

	//shortcut if the node has what is being asked
	if(n->GetType() == ENT_STRING)
		return std::make_pair(true, n->GetStringValue());

	auto result = InterpretNodeForImmediateUse(n,
		key_string ? EvaluableNodeRequestedValueTypes::Type::KEY_STRING_ID_OR_NULL
		: EvaluableNodeRequestedValueTypes::Type::STRING_ID_OR_NULL);
	auto &result_value = result.GetValue();

	auto [valid, str] = result_value.GetValueAsString(key_string);
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return std::make_pair(valid, str);
}

StringInternPool::StringID Interpreter::InterpretNodeIntoStringIDValueIfExists(EvaluableNode *n, bool key_string)
{
	//shortcut if the node has what is being asked
	if(n != nullptr && n->GetType() == ENT_STRING)
		return n->GetStringID();

	auto result = InterpretNodeForImmediateUse(n, EvaluableNodeRequestedValueTypes::Type::EXISTING_STRING_ID_OR_NULL);
	auto &result_value = result.GetValue();

	auto sid = result_value.GetValueAsStringIDIfExists(key_string);
	//ID already exists outside of this, so not expecting to keep this reference
	evaluableNodeManager->FreeNodeTreeIfPossible(result);
	return sid;
}

StringInternPool::StringID Interpreter::InterpretNodeIntoStringIDValueWithReference(EvaluableNode *n, bool key_string)
{
	//shortcut if the node has what is being asked
	if(n != nullptr && n->GetType() == ENT_STRING)
		return string_intern_pool.CreateStringReference(n->GetStringID());

	auto result = InterpretNodeForImmediateUse(n,
		key_string ? EvaluableNodeRequestedValueTypes::Type::KEY_STRING_ID_OR_NULL
		: EvaluableNodeRequestedValueTypes::Type::STRING_ID_OR_NULL);

	if(result.IsImmediateValue())
	{
		auto &result_value = result.GetValue();

		//reuse the reference if it has one
		if(result_value.nodeType == ENIVT_STRING_ID)
			return result_value.nodeValue.stringID;

		//create new reference
		return result_value.GetValueAsStringIDWithReference(key_string);
	}
	else //not immediate
	{
		//if have a unique string, then just grab the string's reference instead of creating a new one
		if(result.unique)
		{
			StringInternPool::StringID result_sid = string_intern_pool.NOT_A_STRING_ID;
			if(result != nullptr && result->GetType() == ENT_STRING)
				result_sid = result->GetAndClearStringIDWithReference();
			else
				result_sid = EvaluableNode::ToStringIDWithReference(result, key_string);

			evaluableNodeManager->FreeNodeTree(result);
			return result_sid;
		}
		else //not unique, so can't free
		{
			return EvaluableNode::ToStringIDWithReference(result, key_string);
		}
	}
}

EvaluableNodeReference Interpreter::InterpretNodeIntoUniqueStringIDValueEvaluableNode(
	EvaluableNode *n, EvaluableNodeRequestedValueTypes immediate_result)
{
	//if can skip InterpretNode, then just allocate the string
	if(n == nullptr || n->GetIsIdempotent()
		|| n->GetType() == ENT_STRING || n->GetType() == ENT_BOOL || n->GetType() == ENT_NUMBER)
	{
		auto sid = EvaluableNode::ToStringIDWithReference(n);

		if(immediate_result.AnyPrimitiveImmediateType())
			return EvaluableNodeReference(sid, true);
		else
			return EvaluableNodeReference(evaluableNodeManager->AllocNodeWithReferenceHandoff(ENT_STRING,
				sid), true);
	}

	auto result = InterpretNode(n);

	if(result == nullptr || !result.unique)
		return EvaluableNodeReference(evaluableNodeManager->AllocNodeWithReferenceHandoff(ENT_STRING,
			EvaluableNode::ToStringIDWithReference(result)), true);

	result->ClearMetadata();

	auto type = result->GetType();
	if(type != ENT_STRING && type != ENT_NULL)
		result->SetType(ENT_STRING, true);

	return result;
}

double Interpreter::InterpretNodeIntoNumberValue(EvaluableNode *n)
{
	if(n != nullptr && n->GetType() == ENT_NUMBER)
		return n->GetNumberValueReference();

	auto result = InterpretNodeForImmediateUse(n, EvaluableNodeRequestedValueTypes::Type::NUMBER_OR_NULL);
	auto &result_value = result.GetValue();

	double value = result_value.GetValueAsNumber();
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return value;
}

EvaluableNodeReference Interpreter::InterpretNodeIntoUniqueNumberValueOrNullEvaluableNode(EvaluableNode *n)
{
	if(n == nullptr || n->GetIsIdempotent())
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(EvaluableNode::ToNumber(n)), true);

	auto result = InterpretNode(n);

	if(result == nullptr || !result.unique)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(EvaluableNode::ToNumber(result)), true);

	result->ClearMetadata();

	auto type = result->GetType();
	if(type != ENT_NUMBER && type != ENT_NULL)
		result->SetType(ENT_NUMBER, true);

	return result;
}

bool Interpreter::InterpretNodeIntoBoolValue(EvaluableNode *n, bool value_if_null)
{
	if(n != nullptr && n->GetType() == ENT_BOOL)
		return n->GetBoolValueReference();

	auto result = InterpretNodeForImmediateUse(n, EvaluableNodeRequestedValueTypes::Type::BOOL_OR_NULL);
	auto &result_value = result.GetValue();

	bool value = result_value.GetValueAsBoolean(value_if_null);
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return value;
}

std::pair<EntityWriteReference, StringRef> Interpreter::InterpretNodeIntoDestinationEntity(EvaluableNode *n)
{
	EvaluableNodeReference destination_entity_id_path = InterpretNodeForImmediateUse(n);

	StringRef new_entity_id;
	auto [entity, entity_container] = TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityWriteReference>(
			curEntity, destination_entity_id_path, &new_entity_id);

	evaluableNodeManager->FreeNodeTreeIfPossible(destination_entity_id_path);

	//if it already exists, then place inside it
	if(entity != nullptr)
		return std::make_pair(std::move(entity), StringRef());
	else //return the container
		return std::make_pair(std::move(entity_container), new_entity_id);
}

EvaluableNode **Interpreter::TraverseToDestinationFromTraversalPathList(EvaluableNode **source, EvaluableNodeReference &tpl, bool create_destination_if_necessary)
{
	EvaluableNode **address_list;
	//default list length to 1
	size_t address_list_length = 1;

	//if it's an actual address list, then use it
	if(!EvaluableNode::IsNull(tpl) && DoesEvaluableNodeTypeUseOrderedData(tpl->GetType()))
	{
		auto &ocn = tpl->GetOrderedChildNodesReference();
		address_list = ocn.data();
		address_list_length = ocn.size();
	}
	else //it's only a single value; use default list length of 1
	{
		address_list = &tpl.GetReference();
	}

	size_t max_num_nodes = 0;
	if(ConstrainedAllocatedNodes())
		max_num_nodes = interpreterConstraints->GetRemainingNumAllocatedNodes(evaluableNodeManager->GetNumberOfUsedNodes());

	EvaluableNode **destination = GetRelativeEvaluableNodeFromTraversalPathList(source, address_list, address_list_length, create_destination_if_necessary ? evaluableNodeManager : nullptr, max_num_nodes);

	return destination;
}

EvaluableNodeReference Interpreter::RewriteByFunction(EvaluableNodeReference function,
	EvaluableNode *tree, FastHashMap<EvaluableNode *, EvaluableNode *> &original_node_to_new_node)
{
	EvaluableNodeReference cur_node(tree, false);

	if(tree != nullptr)
	{
		//attempt to insert; if found, return previous value
		auto [existing_record, inserted] = original_node_to_new_node.emplace(tree, static_cast<EvaluableNode *>(nullptr));
		if(!inserted)
			return EvaluableNodeReference(existing_record->second, false);

		cur_node = EvaluableNodeReference(evaluableNodeManager->AllocNode(tree), true);
		existing_record->second = cur_node;

		if(cur_node->IsAssociativeArray())
		{
			PushNewConstructionContext(nullptr, cur_node, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

			for(auto &[e_id, e] : cur_node->GetMappedChildNodesReference())
			{
				SetTopCurrentIndexInConstructionStack(e_id);
				SetTopCurrentValueInConstructionStack(e);
				auto new_e = RewriteByFunction(function, e, original_node_to_new_node);

				cur_node.UpdatePropertiesBasedOnAttachedNode(new_e);
				e = new_e;
			}
			if(PopConstructionContextAndGetExecutionSideEffectFlag())
				cur_node->SetNeedCycleCheck(true);
		}
		else if(cur_node->IsOrderedArray())
		{
			auto &ocn = cur_node->GetOrderedChildNodesReference();
			if(ocn.size() > 0)
			{
				PushNewConstructionContext(nullptr, cur_node, EvaluableNodeImmediateValueWithType(0.0), nullptr);

				for(size_t i = 0; i < ocn.size(); i++)
				{
					SetTopCurrentIndexInConstructionStack(static_cast<double>(i));
					SetTopCurrentValueInConstructionStack(ocn[i]);
					auto new_e = RewriteByFunction(function, ocn[i], original_node_to_new_node);
					cur_node.UpdatePropertiesBasedOnAttachedNode(new_e);
					ocn[i] = new_e;
				}

				if(PopConstructionContextAndGetExecutionSideEffectFlag())
					cur_node->SetNeedCycleCheck(true);
			}
		}
	}

	SetTopCurrentValueInConstructionStack(cur_node);
	return InterpretNode(function);
}

void Interpreter::PopulateInterpreterConstraintsFromParams(EvaluableNode::OrderedType &params,
	size_t perf_constraint_param_offset, InterpreterConstraints &interpreter_constraints, bool calling_entity)
{
	interpreter_constraints.constraintViolation = InterpreterConstraints::ViolationType::NoViolation;

	if(params.size() <= perf_constraint_param_offset)
		return;

	//check if caller specified override of the default warning collections behavior
	//do this before the rest of the constraints so can early out
	if(params.size() > perf_constraint_param_offset + 1)
		interpreter_constraints.collectWarnings
			= InterpretNodeIntoBoolValue(params[perf_constraint_param_offset + 1], false);

	EvaluableNodeReference constraints = InterpretNodeForImmediateUse(params[perf_constraint_param_offset]);

	//if any form of false (including null), don't add constraints
	if(!EvaluableNode::ToBool(constraints))
		return;

	//default to constrained execution
	interpreter_constraints.maxNodeOperations = 10000;
	interpreter_constraints.maxNumAllocatedNodes = 5000;
	interpreter_constraints.maxOperationDepth = 50;
	interpreter_constraints.writeAccess = false;

	if(calling_entity)
	{
		interpreter_constraints.constrainMaxContainedEntities = true;
		interpreter_constraints.maxContainedEntities = 5;
		interpreter_constraints.constrainMaxContainedEntityDepth = true;
		interpreter_constraints.maxContainedEntityDepth = 3;
		interpreter_constraints.maxEntityIdLength = 20;
	}
	else
	{
		//default to not read local data
		interpreter_constraints.readAccess = false;
	}

	if(constraints->IsAssociativeArray())
	{
		auto &mcn = constraints->GetMappedChildNodesReference();

		EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_max_node_operations,
			interpreter_constraints.maxNodeOperations);
		EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_max_node_allocations,
			interpreter_constraints.maxNumAllocatedNodes);
		EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_max_operation_depth,
			interpreter_constraints.maxOperationDepth);
		EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_read_access,
			interpreter_constraints.readAccess);
		EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_write_access,
			interpreter_constraints.writeAccess);

		if(calling_entity)
		{
			auto found_value = mcn.find(GetStringIdFromBuiltInStringId(ENBISI_max_contained_entities));
			if(found_value != end(mcn))
			{
				interpreter_constraints.constrainMaxContainedEntities = true;
				interpreter_constraints.maxContainedEntities
					= static_cast<size_t>(EvaluableNode::ToNumber(found_value->second));
			}

			found_value = mcn.find(GetStringIdFromBuiltInStringId(ENBISI_max_contained_entity_depth));
			if(found_value != end(mcn))
			{
				interpreter_constraints.constrainMaxContainedEntityDepth = true;
				interpreter_constraints.maxContainedEntityDepth
					= static_cast<size_t>(EvaluableNode::ToNumber(found_value->second));
			}

			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_max_entity_id_length,
				interpreter_constraints.maxEntityIdLength);
		}
	}
}

void Interpreter::PopulatePerformanceCounters(InterpreterConstraints *interpreter_constraints, Entity *entity_to_constrain_from)
{
	if(interpreter_constraints == nullptr)
		return;

	interpreter_constraints->constraintsExceeded = false;

	if(interpreterConstraints != nullptr)
	{
		if(interpreterConstraints->ConstrainedExecutionSteps())
		{
			ExecutionCycleCount remaining_steps = interpreterConstraints->GetRemainingNumExecutionSteps();
			if(remaining_steps > 0)
			{
				if(interpreter_constraints->ConstrainedExecutionSteps())
					interpreter_constraints->maxNodeOperations = std::min(
						interpreter_constraints->maxNodeOperations, remaining_steps);
				else
					interpreter_constraints->maxNodeOperations = remaining_steps;
			}
			else //out of resources, ensure nothing will run (can't use 0 for maxNodeOperations)
			{
				interpreter_constraints->maxNodeOperations = 1;
				interpreter_constraints->curExecutionStep = 1;
				interpreter_constraints->constraintsExceeded = true;
				interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ExecutionStep;
			}
		}

		if(interpreterConstraints->ConstrainedAllocatedNodes())
		{
			size_t remaining_allocs = interpreterConstraints->GetRemainingNumAllocatedNodes(
				evaluableNodeManager->GetNumberOfUsedNodes());
			if(remaining_allocs > 0)
			{
				if(interpreter_constraints->ConstrainedAllocatedNodes())
					interpreter_constraints->maxNumAllocatedNodes = std::min(
						interpreter_constraints->maxNumAllocatedNodes, remaining_allocs);
				else
					interpreter_constraints->maxNumAllocatedNodes = remaining_allocs;
			}
			else //out of resources, ensure nothing will run (can't use 0 for maxNumAllocatedNodes)
			{
				interpreter_constraints->maxNumAllocatedNodes = 1;
				interpreter_constraints->constraintsExceeded = true;
				interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::NodeAllocation;
			}
		}
	}

	if(interpreter_constraints->ConstrainedAllocatedNodes())
	{
	#ifdef MULTITHREAD_SUPPORT
		//if multiple threads, the other threads could be eating into this
		interpreter_constraints->maxNumAllocatedNodes *= Concurrency::threadPool.GetNumActiveThreads();
	#endif

		//offset the max appropriately
		interpreter_constraints->maxNumAllocatedNodes += evaluableNodeManager->GetNumberOfUsedNodes();
	}

	//handle opcode execution depth
	if(interpreterConstraints != nullptr && interpreterConstraints->ConstrainedOpcodeExecutionDepth())
	{
		size_t remaining_depth = interpreterConstraints->GetRemainingOpcodeExecutionDepth(
			opcodeStackNodes.size());

		if(remaining_depth > 1)
		{
			if(interpreter_constraints->ConstrainedOpcodeExecutionDepth())
				interpreter_constraints->maxOperationDepth = std::min(
					interpreter_constraints->maxOperationDepth, remaining_depth);
			else
				interpreter_constraints->maxOperationDepth = remaining_depth;
		}
		else //out of resources, ensure nothing will run (can't use 0 for maxOperationDepth)
		{
			interpreter_constraints->maxOperationDepth = 1;
			interpreter_constraints->constraintsExceeded = true;
			interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ExecutionDepth;
		}
	}

	if(entity_to_constrain_from == nullptr)
		return;

	interpreter_constraints->entityToConstrainFrom = entity_to_constrain_from;

	if(interpreterConstraints != nullptr)
	{
		if(interpreterConstraints->constrainMaxContainedEntities && interpreterConstraints->entityToConstrainFrom != nullptr)
		{
			interpreter_constraints->constrainMaxContainedEntities = true;

			//if calling a contained entity, figure out how many this one can create
			size_t max_entities = interpreterConstraints->maxContainedEntities;
			if(interpreterConstraints->entityToConstrainFrom->DoesDeepContainEntity(interpreter_constraints->entityToConstrainFrom))
			{
				auto erbr = interpreterConstraints->entityToConstrainFrom->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
				size_t container_total_entities = erbr->size();
				erbr.Clear();
				erbr = interpreter_constraints->entityToConstrainFrom->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
				size_t contained_total_entities = erbr->size();
				erbr.Clear();

				if(container_total_entities >= interpreterConstraints->maxContainedEntities)
				{
					max_entities = 0;
					interpreter_constraints->constraintsExceeded = true;
					interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ContainedEntitiesDepth;
				}
				else
				{
					max_entities = interpreterConstraints->maxContainedEntities - (container_total_entities - contained_total_entities);
				}
			}

			interpreter_constraints->maxContainedEntities = std::min(interpreter_constraints->maxContainedEntities, max_entities);
		}

		if(interpreterConstraints->constrainMaxContainedEntityDepth && interpreterConstraints->entityToConstrainFrom != nullptr)
		{
			interpreter_constraints->constrainMaxContainedEntityDepth = true;

			size_t max_depth = interpreterConstraints->maxContainedEntityDepth;
			size_t cur_depth = 0;
			if(interpreterConstraints->entityToConstrainFrom->DoesDeepContainEntity(interpreter_constraints->entityToConstrainFrom))
			{
				for(Entity *cur_entity = interpreter_constraints->entityToConstrainFrom;
						cur_entity != interpreterConstraints->entityToConstrainFrom;
						cur_entity = cur_entity->GetContainer())
					cur_depth++;
			}

			if(cur_depth >= max_depth)
			{
				interpreter_constraints->maxContainedEntityDepth = 0;
				interpreter_constraints->constraintsExceeded = true;
				interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ContainedEntitiesDepth;
			}
			else
			{
				interpreter_constraints->maxContainedEntityDepth = std::min(interpreter_constraints->maxContainedEntityDepth,
					max_depth - cur_depth);
			}
		}

		if(interpreterConstraints->maxEntityIdLength > 0)
		{
			if(interpreter_constraints->maxEntityIdLength > 0)
				interpreter_constraints->maxEntityIdLength = std::min(interpreter_constraints->maxEntityIdLength,
					interpreterConstraints->maxEntityIdLength);
		}

		if(!interpreterConstraints->readAccess)
			interpreter_constraints->readAccess = false;

		if(!interpreterConstraints->writeAccess)
			interpreter_constraints->writeAccess = false;
	}
}


#ifdef MULTITHREAD_SUPPORT

bool Interpreter::InterpretEvaluableNodesConcurrently(EvaluableNode *parent_node,
	EvaluableNode::OrderedType &nodes, std::vector<EvaluableNodeReference> &interpreted_nodes,
	EvaluableNodeRequestedValueTypes immediate_results)
{
	if(!parent_node->GetConcurrency())
		return false;

	size_t num_tasks = nodes.size();
	if(num_tasks < 2)
		return false;

	auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
	if(!Concurrency::threadPool.AreThreadsAvailable())
		return false;

	InterpreterConcurrencyManager concurrency_manager(this, num_tasks, enqueue_task_lock);

	interpreted_nodes.resize(num_tasks);

	//kick off interpreters
	for(size_t i = 0; i < num_tasks; i++)
		concurrency_manager.EnqueueTask<EvaluableNodeReference>(nodes[i], &interpreted_nodes[i], immediate_results);

	concurrency_manager.EndConcurrency();
	return true;
}

Interpreter *Interpreter::LockScopeStackTop(Concurrency::SingleLock &lock, EvaluableNode *en_to_preserve,
	Interpreter *executing_interpreter)
{
	if(scopeStack.size() == 0 && callingInterpreter != nullptr)
		return callingInterpreter->LockScopeStackTop(lock, en_to_preserve,
			executing_interpreter == nullptr ? this : executing_interpreter);

	if(scopeStackMutex.get() != nullptr)
	{
		if(executing_interpreter != nullptr)
			executing_interpreter->LockMutexWithoutBlockingGarbageCollection(lock, *scopeStackMutex, en_to_preserve);
		else
			LockMutexWithoutBlockingGarbageCollection(lock, *scopeStackMutex, en_to_preserve);
	}

	return this;
}

#endif

static EvaluableNodeReference ConstraintViolationToString(InterpreterConstraints::ViolationType violation, EvaluableNodeManager *evaluable_node_manager)
{
	switch(violation)
	{
	case InterpreterConstraints::ViolationType::NoViolation:
		return EvaluableNodeReference::Null();
	case InterpreterConstraints::ViolationType::ContainedEntitiesDepth:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Contained entities depth exceeded")), true);
	case InterpreterConstraints::ViolationType::ContainedEntitiesNumber:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Contained entities number limit exceeded")), true);
	case InterpreterConstraints::ViolationType::ExecutionDepth:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Execution depth exceeded")), true);
	case InterpreterConstraints::ViolationType::ExecutionStep:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Execution step limit exceeded")), true);
	case InterpreterConstraints::ViolationType::NodeAllocation:
		return EvaluableNodeReference(evaluable_node_manager->AllocNode(std::string("Node allocation limit exceeded")), true);
	default:
		//cases should be exhaustive, so this is unreachable
		AmlgAssert(false);
	}

	AmlgAssert(false);
	return ""; //unreachable
}

void Interpreter::EmitOrLogUndefinedVariableWarningIfNeeded(StringInternPool::StringID not_found_variable_sid, EvaluableNode *en)
{
	std::string warning = "";

	warning.append("Warning: undefined symbol " + not_found_variable_sid->string);

	if(asset_manager.debugSources && en->HasComments())
	{
		std::string_view comment_string = en->GetCommentsString();
		size_t newline_index = comment_string.find("\n");

		std::string comment_string_first_line;

		if(newline_index != std::string::npos)
			comment_string_first_line = comment_string.substr(0, newline_index + 1);
		else
			comment_string_first_line = comment_string;

		warning.append(" at " + comment_string_first_line);
	}

	if(interpreterConstraints != nullptr)
	{
		if(interpreterConstraints->collectWarnings)
			interpreterConstraints->AddWarning(std::move(warning));
	}
	else if(asset_manager.warnOnUndefined)
	{
		ExecutionPermissions entity_permissions = asset_manager.GetEntityPermissions(curEntity);
		if(entity_permissions.HasPermission(ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR))
			std::cerr << warning << std::endl;
	}
}

EvaluableNodeReference Interpreter::BundleResultWithWarningsAndChangesIfNeeded(
	EvaluableNodeReference result, InterpreterConstraints *interpreter_constraints, EvaluableNodeReference changes)
{
	if( (interpreter_constraints == nullptr || !interpreter_constraints->collectWarnings) && changes.IsNull())
		return result;

	EvaluableNodeReference result_tuple(evaluableNodeManager->AllocNode(ENT_LIST), true);
	auto &result_tuple_ocn = result_tuple->GetOrderedChildNodesReference();
	result_tuple_ocn.push_back(result);
	result_tuple.UpdatePropertiesBasedOnAttachedNode(result);

	if(interpreter_constraints != nullptr && interpreter_constraints->collectWarnings)
	{
		EvaluableNodeReference warning_assoc = CreateAssocOfNumbersFromIteratorAndFunctions(
			interpreter_constraints->warnings, [](std::pair<std::string, size_t> warning_count)
		{ return warning_count.first; }, [](std::pair<std::string, size_t> warning_count)
		{ return static_cast<double>(warning_count.second); }, evaluableNodeManager);

		EvaluableNodeReference constraint_violation_string = ConstraintViolationToString(interpreter_constraints->constraintViolation, evaluableNodeManager);

		result_tuple_ocn.push_back(warning_assoc);
		result_tuple_ocn.push_back(constraint_violation_string);
		result_tuple.UpdatePropertiesBasedOnAttachedNode(warning_assoc);
		result_tuple.UpdatePropertiesBasedOnAttachedNode(constraint_violation_string);
	}

	if(!changes.IsNull())
	{
		result_tuple_ocn.push_back(changes);
		result_tuple.UpdatePropertiesBasedOnAttachedNode(changes);
	}

	return result_tuple;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DEALLOCATED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::cerr << "ERROR: attempt to use freed memory\n";
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(false);
#endif
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNINITIALIZED(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::cerr << "ERROR: encountered an uninitialized node\n";
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(false);
#endif
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NOT_A_BUILT_IN_TYPE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::cerr << "ERROR: encountered an invalid instruction\n";
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(false);
#endif
	return EvaluableNodeReference::Null();
}

void Interpreter::VerifyEvaluableNodeIntegrity()
{
	evaluableNodeManager->VerifyEvaluableNodeIntegrityForAllReferencedNodes();

	//traverse stack to next calling evaluableNodeManager so don't duplicate validation effort on the same one
	auto next_calling_interpreter_on_other_enm = callingInterpreter;
	while(next_calling_interpreter_on_other_enm != nullptr)
	{
		if(next_calling_interpreter_on_other_enm->evaluableNodeManager != evaluableNodeManager)
			break;

		next_calling_interpreter_on_other_enm = next_calling_interpreter_on_other_enm->callingInterpreter;
	}

	if(next_calling_interpreter_on_other_enm != nullptr)
		next_calling_interpreter_on_other_enm->VerifyEvaluableNodeIntegrity();
}
