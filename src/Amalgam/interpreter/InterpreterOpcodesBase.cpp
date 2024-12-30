//project headers:
#include "Interpreter.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "Concurrency.h"
#include "Cryptography.h"
#include "EntityManipulation.h"
#include "EntityWriteListener.h"
#include "EvaluableNodeManagement.h"
#include "EvaluableNodeTreeFunctions.h"
#include "PerformanceProfiler.h"

//system headers:
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <utility>

//Used only for deep debugging of entity memory and garbage collection
std::string GetEntityMemorySizeDiagnostics(Entity *e)
{
	if(e == nullptr)
		return "";

	static CompactHashMap<Entity *, size_t> entity_core_allocs;
	static CompactHashMap<Entity *, size_t> entity_temp_unused;

	//initialize to zero if not already in the list
	auto prev_used = entity_core_allocs.insert(std::make_pair(e, 0));
	auto prev_unused = entity_temp_unused.insert(std::make_pair(e, 0));

	size_t cur_used = e->evaluableNodeManager.GetNumberOfUsedNodes();
	size_t cur_unused = e->evaluableNodeManager.GetNumberOfUnusedNodes();

	std::string result;

	if(cur_used > prev_used.first->second || cur_unused > prev_unused.first->second)
	{
		result += e->GetId() + " (used, free): " + EvaluableNode::NumberToString(cur_used - prev_used.first->second) + ", "
			+ EvaluableNode::NumberToString(cur_unused - prev_unused.first->second) + "\n";

		prev_used.first->second = cur_used;
		prev_unused.first->second = cur_unused;
	}

	for(auto entity : e->GetContainedEntities())
		result += GetEntityMemorySizeDiagnostics(entity);

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SYSTEM(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);

	std::string command = InterpretNodeIntoStringValueEmptyNull(ocn[0]);

	if(writeListeners != nullptr)
	{
		for(auto &wl : *writeListeners)
			wl->LogSystemCall(ocn[0]);
	}

	if(command == "exit" && permissions.individualPermissions.system)
	{
		exit(0);
	}
	else if(command == "readline" && permissions.individualPermissions.stdIn)
	{
		std::string input;
		std::getline(std::cin, input);

		//exit if have no more input
		if(std::cin.bad() || std::cin.eof())
			exit(0);

		return AllocReturn(input, immediate_result);
	}
	else if(command == "printline" && ocn.size() > 1 && permissions.individualPermissions.stdOut)
	{
		std::string output = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
		printListener->LogPrint(output);
		printListener->FlushLogFile();
		return EvaluableNodeReference::Null();
	}
	else if(command == "cwd" && permissions.individualPermissions.environment)
	{
		//if no parameter specified, return the directory
		if(ocn.size() == 1)
		{
			auto path = std::filesystem::current_path();
			return AllocReturn(path.string(), immediate_result);
		}

		std::string directory = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
		std::filesystem::path path(directory);

		//try to set the directory
		std::error_code error;
		std::filesystem::current_path(directory, error);
		bool error_value = static_cast<bool>(error);
		return AllocReturn(error_value, immediate_result);
	}
	else if(command == "system" && ocn.size() > 1 && permissions.individualPermissions.system)
	{
		std::string sys_command = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

		bool successful_run = false;
		int exit_code = 0;
		std::string stdout_data = Platform_RunSystemCommand(sys_command, successful_run, exit_code);

		if(!successful_run)
			return EvaluableNodeReference::Null();

		EvaluableNode *list = evaluableNodeManager->AllocNode(ENT_LIST);
		list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(static_cast<double>(exit_code)));
		list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, stdout_data));

		return EvaluableNodeReference(list, true);
	}
	else if(command == "os" && permissions.individualPermissions.environment)
	{
		std::string os = Platform_GetOperatingSystemName();
		return AllocReturn(os, immediate_result);
	}
	else if(command == "sleep" && permissions.individualPermissions.system)
	{
		std::chrono::microseconds sleep_time_usec(1);
		if(ocn.size() > 1)
		{
			double sleep_time_sec = InterpretNodeIntoNumberValue(ocn[1]);
			sleep_time_usec = std::chrono::microseconds(static_cast<size_t>(1000000.0 * sleep_time_sec));
		}

		Platform_Sleep(sleep_time_usec);
	}
	else if(command == "version" && permissions.individualPermissions.environment)
	{
		std::string version_string = AMALGAM_VERSION_STRING;
		return AllocReturn(version_string, immediate_result);
	}
	else if(command == "version_compatible" && permissions.individualPermissions.environment)
	{
		if(ocn.size() < 2)
			return EvaluableNodeReference::Null();

		std::string version_requested = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
		auto [error_message, success] = AssetManager::ValidateVersionAgainstAmalgam(version_requested, false);
		EvaluableNode *result = evaluableNodeManager->AllocNode(success);
		result->SetComments(error_message);
		return EvaluableNodeReference(result, true);
	}
	else if(command == "est_mem_reserved" && permissions.individualPermissions.environment)
	{
		return AllocReturn(static_cast<double>(curEntity->GetEstimatedReservedDeepSizeInBytes()), immediate_result);
	}
	else if(command == "est_mem_used" && permissions.individualPermissions.environment)
	{
		return AllocReturn(static_cast<double>(curEntity->GetEstimatedUsedDeepSizeInBytes()), immediate_result);
	}
	else if(command == "mem_diagnostics" && permissions.individualPermissions.environment)
	{

	#ifdef MULTITHREAD_SUPPORT
		auto lock = curEntity->CreateEntityLock<Concurrency::ReadLock>();
	#endif

		return AllocReturn(GetEntityMemorySizeDiagnostics(curEntity), immediate_result);
	}
	else if(command == "validate" && permissions.individualPermissions.system)
	{
		VerifyEvaluableNodeIntegrity();
		return AllocReturn(true, immediate_result);
	}
	else if(command == "rand" && ocn.size() > 1 && permissions.individualPermissions.system)
	{
		double num_bytes_raw = InterpretNodeIntoNumberValue(ocn[1]);
		size_t num_bytes = 0;
		if(num_bytes_raw > 0)
			num_bytes = static_cast<size_t>(num_bytes_raw);

		std::string rand_data(num_bytes, '\0');
		Platform_GenerateSecureRandomData(&rand_data[0], num_bytes);

		return AllocReturn(rand_data, immediate_result);
	}
	else if(command == "sign_key_pair" && permissions.individualPermissions.system)
	{
		auto [public_key, secret_key] = GenerateSignatureKeyPair();
		EvaluableNode *list = evaluableNodeManager->AllocNode(ENT_LIST);
		auto &list_ocn = list->GetOrderedChildNodesReference();
		list_ocn.resize(2);
		list_ocn[0] = evaluableNodeManager->AllocNode(public_key);
		list_ocn[1] = evaluableNodeManager->AllocNode(secret_key);

		return EvaluableNodeReference(list, true);

	}
	else if(command == "encrypt_key_pair" && permissions.individualPermissions.system)
	{
		auto [public_key, secret_key] = GenerateEncryptionKeyPair();
		EvaluableNode *list = evaluableNodeManager->AllocNode(ENT_LIST);
		auto &list_ocn = list->GetOrderedChildNodesReference();
		list_ocn.resize(2);
		list_ocn[0] = evaluableNodeManager->AllocNode(public_key);
		list_ocn[1] = evaluableNodeManager->AllocNode(secret_key);

		return EvaluableNodeReference(list, true);
	}
	else if(command == "debugging_info" && permissions.individualPermissions.environment)
	{
		EvaluableNode *debugger_info = evaluableNodeManager->AllocNode(ENT_LIST);
		auto &list_ocn = debugger_info->GetOrderedChildNodesReference();
		list_ocn.resize(2);
		list_ocn[0] = evaluableNodeManager->AllocNode(Interpreter::GetDebuggingState());
		list_ocn[1] = evaluableNodeManager->AllocNode(asset_manager.debugSources);

		return EvaluableNodeReference(debugger_info, true);
	}
#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
	else if(command == "get_max_num_threads" && permissions.individualPermissions.environment)
	{
		double max_num_threads = static_cast<double>(Concurrency::GetMaxNumThreads());
		return AllocReturn(max_num_threads, immediate_result);
	}
	else if(command == "set_max_num_threads" && ocn.size() > 1 && permissions.individualPermissions.system)
	{
		double max_num_threads_raw = InterpretNodeIntoNumberValue(ocn[1]);
		size_t max_num_threads = 0;
		if(max_num_threads >= 0)
			max_num_threads = static_cast<size_t>(max_num_threads_raw);
		Concurrency::SetMaxNumThreads(max_num_threads);

		max_num_threads_raw = static_cast<double>(Concurrency::GetMaxNumThreads());
		return AllocReturn(max_num_threads_raw, immediate_result);
	}
#endif
	else if(command == "built_in_data" && permissions.individualPermissions.environment)
	{
		uint8_t built_in_data[] = AMALGAM_BUILT_IN_DATA;
		std::string built_in_data_s(reinterpret_cast<char *>(&built_in_data[0]), sizeof(built_in_data));
		return AllocReturn(built_in_data_s, immediate_result);
	}
	else if(permissions.individualPermissions.stdOut)
	{
		std::cerr << "Invalid system opcode command \"" << command << "\" invoked" << std::endl;
	}

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_DEFAULTS(EvaluableNode *en, bool immediate_result)
{
	if(en->GetOrderedChildNodes().size() == 0)
		return EvaluableNodeReference::Null();
	//get the string key
	std::string key = InterpretNodeIntoStringValueEmptyNull(en->GetOrderedChildNodes()[0]);

	if(key == "mutation_opcodes")
	{
		EvaluableNode *out_node = evaluableNodeManager->AllocNode(ENT_ASSOC);
		out_node->ReserveMappedChildNodes(EvaluableNodeTreeManipulation::evaluableNodeTypeProbabilities.size());
		for(auto &[node_type, node_prob] : EvaluableNodeTreeManipulation::evaluableNodeTypeProbabilities)
		{
			EvaluableNode *num_node = evaluableNodeManager->AllocNode(node_prob);

			StringInternPool::StringID node_type_sid = GetStringIdFromNodeType(node_type);
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_PARSE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool transactional_parse = false;
	if(ocn.size() > 1)
		transactional_parse = InterpretNodeIntoBoolValue(ocn[1]);

	bool return_warnings = false;
	if(ocn.size() > 2)
		return_warnings = InterpretNodeIntoBoolValue(ocn[2]);

	//get the string to parse
	auto [valid_string, to_parse] = InterpretNodeIntoStringValue(ocn[0]);
	if(!valid_string)
		return EvaluableNodeReference::Null();

	auto [node, warnings, char_with_error] = Parser::Parse(to_parse, evaluableNodeManager, transactional_parse);
	
	if(!return_warnings)
		return node;

	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);
	retval->ReserveOrderedChildNodes(2);
	retval->AppendOrderedChildNode(node);

	EvaluableNodeReference warning_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	retval->AppendOrderedChildNode(warning_list);

	auto &list_ocn = warning_list->GetOrderedChildNodesReference();
	list_ocn.resize(warnings.size());
	for(size_t i = 0; i < warnings.size(); i++)
		list_ocn[i] = evaluableNodeManager->AllocNode(ENT_STRING, warnings[i]);

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNPARSE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool pretty = false;
	if(ocn.size() > 1)
		pretty = InterpretNodeIntoBoolValue(ocn[1]);

	bool deterministic_order = false;
	if(ocn.size() > 2)
		deterministic_order = InterpretNodeIntoBoolValue(ocn[2]);

	auto tree = InterpretNodeForImmediateUse(ocn[0]);
	std::string s = Parser::Unparse(tree, pretty, true, deterministic_order);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree);

	return AllocReturn(s, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_IF(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_SEQUENCE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

		//request immediate values when not last, since any allocs for returns would be wasted
		//concludes won't be immediate
		result = InterpretNode(ocn[i], immediate_result || i + 1 < ocn_size);
	}
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_PARALLEL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

#ifdef MULTITHREAD_SUPPORT
	if(en->GetConcurrency() && ocn.size() > 1)
	{
		auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
		if(Concurrency::threadPool.AreThreadsAvailable())
		{
			size_t num_elements = ocn.size();

			ConcurrencyManager concurrency_manager(this, num_elements, enqueue_task_lock);

			//kick off interpreters
			for(size_t element_index = 0; element_index < num_elements; element_index++)
				concurrency_manager.EnqueueTask<EvaluableNodeReference>(ocn[element_index]);

			concurrency_manager.EndConcurrency();
			return EvaluableNodeReference::Null();
		}
	}
#endif

	for(auto &cn :ocn)
	{
		//don't need the result, so can ask for an immediate
		auto result = InterpretNodeForImmediateUse(cn, true);
		evaluableNodeManager->FreeNodeTreeIfPossible(result);
	}

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LAMBDA(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONCLUDE_and_RETURN(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	//if no parameter, then return itself for performance
	if(ocn.size() == 0)
		return EvaluableNodeReference(en, false);

	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
		return evaluableNodeManager->DeepAllocCopy(en, EvaluableNodeManager::ENMM_REMOVE_ALL);

	EvaluableNodeReference value = InterpretNode(ocn[0]);

	//need to evaluate its parameter and return a new node encapsulating it
	auto node_type = en->GetType();
	EvaluableNodeReference result(evaluableNodeManager->AllocNode(node_type), true);
	result->AppendOrderedChildNode(value);
	result.UpdatePropertiesBasedOnAttachedNode(value, true);

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsNull(function))
		return EvaluableNodeReference::Null();

	auto node_stack = CreateOpcodeStackStateSaver(function);

	if(_label_profiling_enabled && function->GetNumLabels() > 0)
		PerformanceProfiler::StartOperation(function->GetLabel(0), evaluableNodeManager->GetNumberOfUsedNodes());

	//if have an call stack context of variables specified, then use it
	EvaluableNodeReference new_context = EvaluableNodeReference::Null();
	if(en->GetOrderedChildNodes().size() > 1)
	{
		//can keep constant, but need the top node to be unique in case assignments are made
		new_context = InterpretNodeForImmediateUse(ocn[1]);
		evaluableNodeManager->EnsureNodeIsModifiable(new_context, EvaluableNodeManager::ENMM_REMOVE_ALL);
	}

	PushNewCallStack(new_context);

	//call the code
	auto result = InterpretNode(function, immediate_result);

	//all finished with new context, but can't free it in case returning something
	PopCallStack();

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, evaluableNodeManager);

	if(_label_profiling_enabled && function->GetNumLabels() > 0)
		PerformanceProfiler::EndOperation(evaluableNodeManager->GetNumberOfUsedNodes());

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_SANDBOXED(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto function = InterpretNodeForImmediateUse(ocn[0]);
	if(EvaluableNode::IsNull(function))
		return EvaluableNodeReference::Null();

	auto node_stack = CreateOpcodeStackStateSaver(function);

	PerformanceConstraints perf_constraints;
	PerformanceConstraints *perf_constraints_ptr = nullptr;
	if(PopulatePerformanceConstraintsFromParams(ocn, 2, perf_constraints))
		perf_constraints_ptr = &perf_constraints;

	if(_label_profiling_enabled && function->GetNumLabels() > 0)
		PerformanceProfiler::StartOperation(function->GetLabel(0), evaluableNodeManager->GetNumberOfUsedNodes());

	//if have a call stack context of variables specified, then use it
	EvaluableNodeReference args = EvaluableNodeReference::Null();
	if(en->GetOrderedChildNodes().size() > 1)
		args = InterpretNode(ocn[1]);

	//build call stack from parameters
	EvaluableNodeReference call_stack = ConvertArgsToCallStack(args, *evaluableNodeManager);
	node_stack.PushEvaluableNode(call_stack);

	PopulatePerformanceCounters(perf_constraints_ptr, nullptr);

	Interpreter sandbox(evaluableNodeManager, randomStream.CreateOtherStreamViaRand(),
		writeListeners, printListener, perf_constraints_ptr, nullptr, this);

#ifdef MULTITHREAD_SUPPORT
	//everything at this point is referenced on stacks; allow the sandbox to trigger a garbage collect without this interpreter blocking
	std::swap(memoryModificationLock, sandbox.memoryModificationLock);
#endif

	auto result = sandbox.ExecuteNode(function, call_stack);

#ifdef MULTITHREAD_SUPPORT
	//hand lock back to this interpreter
	std::swap(memoryModificationLock, sandbox.memoryModificationLock);
#endif

	evaluableNodeManager->FreeNode(call_stack->GetOrderedChildNodesReference()[0]);
	evaluableNodeManager->FreeNode(call_stack);

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, evaluableNodeManager);

	if(_label_profiling_enabled && function->GetNumLabels() > 0)
		PerformanceProfiler::EndOperation(evaluableNodeManager->GetNumberOfUsedNodes());

	if(performanceConstraints != nullptr)
		performanceConstraints->AccruePerformanceCounters(perf_constraints_ptr);

	if(perf_constraints_ptr != nullptr && perf_constraints_ptr->constraintsExceeded)
		return EvaluableNodeReference::Null();

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_WHILE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_LET(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t ocn_size = ocn.size();
	if(ocn_size == 0)
		return EvaluableNodeReference::Null();

	//add new context
	auto new_context = InterpretNodeForImmediateUse(ocn[0]);
	//can keep constant, but need the top node to be unique in case assignments are made
	evaluableNodeManager->EnsureNodeIsModifiable(new_context, EvaluableNodeManager::ENMM_REMOVE_ALL);
	PushNewCallStack(new_context);

	//run code
	EvaluableNodeReference result = EvaluableNodeReference::Null();
	for(size_t i = 1; i < ocn_size; i++)
	{
		if(result.IsNonNullNodeReference())
		{
			auto result_type = result->GetType();
			if(result_type == ENT_CONCLUDE)
			{
				PopCallStack();
				return RemoveTopConcludeOrReturnNode(result, evaluableNodeManager);
			}
			else if(result_type == ENT_RETURN)
			{
				PopCallStack();
				return result;
			}
		}

		//free from previous iteration
		evaluableNodeManager->FreeNodeTreeIfPossible(result);

		//request immediate values when not last, since any allocs for returns would be wasted
		//concludes won't be immediate
		result = InterpretNode(ocn[i], immediate_result || i + 1 < ocn_size);
	}

	//all finished with new context, but can't free it in case returning something
	PopCallStack();
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DECLARE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t ocn_size = ocn.size();
	if(ocn_size == 0)
		return EvaluableNodeReference::Null();

	//work on the node that is declaring the variables
	EvaluableNode *required_vars_node = ocn[0];
	if(required_vars_node != nullptr)
	{
		//transform into variables if possible
		EvaluableNodeReference required_vars;

		bool need_to_interpret = false;
		if(required_vars_node->GetIsIdempotent())
		{
			required_vars = EvaluableNodeReference(required_vars_node, false);
		}
		else if(required_vars_node->IsAssociativeArray())
		{
			required_vars = EvaluableNodeReference(required_vars_node, false);
			need_to_interpret = true;
		}
		else //just need to interpret
		{
			required_vars = InterpretNode(required_vars_node);
		}

		if(required_vars != nullptr && required_vars->IsAssociativeArray())
		{
		#ifdef MULTITHREAD_SUPPORT
			Concurrency::WriteLock write_lock;
			bool need_write_lock = (callStackMutex != nullptr && GetCallStackDepth() < callStackUniqueAccessStartingDepth);
			if(need_write_lock)
				LockWithoutBlockingGarbageCollection(*callStackMutex, write_lock, required_vars);
		#endif

			//get the current layer of the stack
			EvaluableNode *scope = GetCurrentCallStackContext();
			if(scope == nullptr)	//this shouldn't happen, but just in case it does
				return EvaluableNodeReference::Null();

			if(!need_to_interpret)
			{
				//check each of the required variables and put into the stack if appropriate
				for(auto &[cn_id, cn] : required_vars->GetMappedChildNodesReference())
				{
					auto [inserted, node_ptr] = scope->SetMappedChildNode(cn_id, cn, false);
					if(!inserted)
					{
						//if it can't insert the new variable because it already exists,
						// then try to free the default / new value that was attempted to be assigned
						if(required_vars.unique && !required_vars.GetNeedCycleCheck())
							evaluableNodeManager->FreeNodeTree(cn);
					}
				}
			}
			else //need_to_interpret
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
						if(!inserted)
						{
							//if it can't insert the new variable because it already exists,
							// then try to free the default / new value that was attempted to be assigned
							if(required_vars.unique && !required_vars.GetNeedCycleCheck())
								evaluableNodeManager->FreeNodeTree(cn);
						}
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
						EvaluableNodeReference value = InterpretNode(cn);

					#ifdef MULTITHREAD_SUPPORT
						//relock if needed before assigning the value
						if(need_write_lock)
							LockWithoutBlockingGarbageCollection(*callStackMutex, write_lock, required_vars);
					#endif

						scope->SetMappedChildNode(cn_id, value, false);
					}
				}
				if(PopConstructionContextAndGetExecutionSideEffectFlag())
					required_vars.unique = false;
			}

			//free the vars / assoc node
			evaluableNodeManager->FreeNodeIfPossible(required_vars);
		}
	}

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

		//request immediate values when not last, since any allocs for returns would be wasted
		//concludes won't be immediate
		result = InterpretNode(ocn[i], immediate_result || i + 1 < ocn_size);
	}

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_and_ACCUM(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
	size_t num_params = ocn.size();

	if(num_params < 1)
		return EvaluableNodeReference::Null();

	//make sure there's at least an callStack to use
	if(callStackNodes->size() < 1)
		return EvaluableNodeReference::Null();

	auto [any_constructions, initial_side_effect] = SetSideEffectsFlagsInConstructionStack();
	if(_opcode_profiling_enabled && any_constructions)
	{
		std::string variable_location = asset_manager.GetEvaluableNodeSourceFromComments(en);
		PerformanceProfiler::AccumulateTotalSideEffectMemoryWrites(variable_location);
		if(initial_side_effect)
			PerformanceProfiler::AccumulateInitialSideEffectMemoryWrites(variable_location);
	}

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

		if(assigned_vars == nullptr || !assigned_vars->IsAssociativeArray())
			return EvaluableNodeReference::Null();

		auto node_stack = CreateOpcodeStackStateSaver(assigned_vars);

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
					assigned_vars.unique = false;
			}

			//retrieve the symbol
			size_t destination_call_stack_index = 0;
			EvaluableNode **value_destination = nullptr;

		#ifdef MULTITHREAD_SUPPORT
			//attempt to get location, but only attempt locations unique to this thread
			value_destination = GetCallStackSymbolLocation(variable_sid, destination_call_stack_index, true, false);
			//if editing a shared variable, need to see if it is in a shared region of the stack,
			// need a write lock to the stack and variable
			Concurrency::WriteLock write_lock;
			if(callStackMutex != nullptr && value_destination == nullptr)
			{
				LockWithoutBlockingGarbageCollection(*callStackMutex, write_lock, variable_value_node);
				if(_opcode_profiling_enabled)
				{
					std::string variable_location = asset_manager.GetEvaluableNodeSourceFromComments(en);
					variable_location += string_intern_pool.GetStringFromID(variable_sid);
					PerformanceProfiler::AccumulateLockContentionCount(variable_location);
				}
			}
		#endif

			//in single threaded, this will just be true
			//in multithreaded, if variable was not found, then may need to create it
			if(value_destination == nullptr)
				value_destination = GetOrCreateCallStackSymbolLocation(variable_sid, destination_call_stack_index);

			if(accum)
			{
				//values should always be copied before changing, in case the value is used elsewhere, especially in another thread
				EvaluableNodeReference value_destination_node = evaluableNodeManager->DeepAllocCopy(*value_destination);
				variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, variable_value_node, evaluableNodeManager);
			}

			//assign back into the context_to_use
			*value_destination = variable_value_node;
		}

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
		auto new_value = InterpretNodeForImmediateUse(ocn[1]);

		//retrieve the symbol
		size_t destination_call_stack_index = 0;
		EvaluableNode **value_destination = nullptr;

	#ifdef MULTITHREAD_SUPPORT
		//attempt to get location, but only attempt locations unique to this thread
		value_destination = GetCallStackSymbolLocation(variable_sid, destination_call_stack_index, true, false);
		//if editing a shared variable, need to see if it is in a shared region of the stack,
		// need a write lock to the stack and variable
		Concurrency::WriteLock write_lock;
		if(callStackMutex != nullptr && value_destination == nullptr)
			LockWithoutBlockingGarbageCollection(*callStackMutex, write_lock, new_value);
	#endif

		//in single threaded, this will just be true
		//in multithreaded, if variable was not found, then may need to create it
		if(value_destination == nullptr)
			value_destination = GetOrCreateCallStackSymbolLocation(variable_sid, destination_call_stack_index);

		if(accum)
		{
			//values should always be copied before changing, in case the value is used elsewhere, especially in another thread
			EvaluableNodeReference value_destination_node = evaluableNodeManager->DeepAllocCopy(*value_destination);
			EvaluableNodeReference variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, new_value, evaluableNodeManager);

			//assign the new accumulation
			*value_destination = variable_value_node;
		}
		else
		{
			*value_destination = new_value;
		}
	}
	else //more than 2, need to make a copy and fill in as appropriate
	{
		//obtain all of the edits to make the edits transactionally at once when all are collected
		auto node_stack = CreateOpcodeStackStateSaver();
		auto &replacements = *node_stack.stack;
		size_t replacements_start_index = node_stack.originalStackSize;

		//keeps track of whether each address is unique so they can be freed if relevant
		std::vector<bool> is_value_unique;
		is_value_unique.reserve(num_params - 1);
		//keeps track of whether all new values assigned or accumed are unique, cycle free, etc.
		bool result_flags_need_updates = false;

		//get each address/value pair to replace in result
		for(size_t ocn_index = 1; ocn_index + 1 < num_params; ocn_index += 2)
		{
			if(AreExecutionResourcesExhausted())
				return EvaluableNodeReference::Null();

			auto address = InterpretNodeForImmediateUse(ocn[ocn_index]);
			node_stack.PushEvaluableNode(address);
			is_value_unique.push_back(address.unique);

			auto new_value = InterpretNodeForImmediateUse(ocn[ocn_index + 1]);
			node_stack.PushEvaluableNode(new_value);
			is_value_unique.push_back(new_value.unique);
		}
		size_t num_replacements = (num_params - 1) / 2;

		//retrieve the symbol
		size_t destination_call_stack_index = 0;
		EvaluableNode **value_destination = nullptr;

	#ifdef MULTITHREAD_SUPPORT
		//attempt to get location, but only attempt locations unique to this thread
		value_destination = GetCallStackSymbolLocation(variable_sid, destination_call_stack_index, true, false);
		//if editing a shared variable, need to see if it is in a shared region of the stack,
		// need a write lock to the stack and variable
		Concurrency::WriteLock write_lock;
		if(callStackMutex != nullptr && value_destination == nullptr)
			LockWithoutBlockingGarbageCollection(*callStackMutex, write_lock);
	#endif

		//in single threaded, this will just be true
		//in multithreaded, if variable was not found, then may need to create it
		if(value_destination == nullptr)
			value_destination = GetOrCreateCallStackSymbolLocation(variable_sid, destination_call_stack_index);

		//make a copy of value_replacement because not sure where else it may be used
		EvaluableNode *value_replacement = nullptr;
		if(*value_destination == nullptr)
			value_replacement = evaluableNodeManager->AllocNode(ENT_NULL);
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

			if(accum)
			{
				//create destination reference
				EvaluableNodeReference value_destination_node(*copy_destination, false);
				EvaluableNodeReference variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, new_value, evaluableNodeManager);

				//assign the new accumulation
				*copy_destination = variable_value_node;
			}
			else
			{
				*copy_destination = new_value;
			}

			bool need_cycle_check_after = false;
			bool is_idempotent_after = false;
			if((*copy_destination) != nullptr)
			{
				need_cycle_check_after = (*copy_destination)->GetNeedCycleCheck();
				is_idempotent_after = (*copy_destination)->GetIsIdempotent();
			}

			if(!new_value.unique
					|| need_cycle_check_before != need_cycle_check_after
					|| is_idempotent_before != is_idempotent_after)
				result_flags_need_updates = true;
		}

		if(result_flags_need_updates)
			EvaluableNodeManager::UpdateFlagsForNodeTree(value_replacement);
		*value_destination = value_replacement;
	}

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto to_lookup = InterpretNodeForImmediateUse(ocn[0]);

#ifdef MULTITHREAD_SUPPORT
	//accessing everything in the stack, so need exclusive access
	Concurrency::ReadLock lock;
	if(callStackMutex != nullptr)
		LockWithoutBlockingGarbageCollection(*callStackMutex, lock, to_lookup);
#endif

	//get the value(s)
	if(EvaluableNode::IsNull(to_lookup) || IsEvaluableNodeTypeImmediate(to_lookup->GetType()))
	{
		StringInternPool::StringID symbol_name_sid = EvaluableNode::ToStringIDIfExists(to_lookup, true);
		EvaluableNode* symbol_value = GetCallStackSymbol(symbol_name_sid);
		evaluableNodeManager->FreeNodeTreeIfPossible(to_lookup);
		return EvaluableNodeReference(symbol_value, false);
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

			cn = GetCallStackSymbol(cn_id);
		}

		return EvaluableNodeReference(to_lookup, false);
	}
	else //ordered params
	{
		evaluableNodeManager->EnsureNodeIsModifiable(to_lookup);

		//overwrite values in the ordered
		for(auto &cn : to_lookup->GetOrderedChildNodes())
		{
			StringInternPool::StringID symbol_name_sid = EvaluableNode::ToStringIDIfExists(cn, true);
			if(symbol_name_sid == StringInternPool::NOT_A_STRING_ID)
			{
				cn = nullptr;
				continue;
			}

			EvaluableNode *symbol_value = GetCallStackSymbol(symbol_name_sid);
			//if there are values passed in, free them to be clobbered
			EvaluableNodeReference cnr(cn, to_lookup.unique);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			cn = symbol_value;
		}

		return EvaluableNodeReference(to_lookup, false);
	}
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_and_REPLACE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
				result.unique = false;

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_TARGET(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	size_t depth = 0;
	if(ocn.size() > 0)
	{
		double value = InterpretNodeIntoNumberValue(ocn[0]);
		if(value >= 0)
			depth = static_cast<size_t>(value);
		else if(!FastIsNaN(value)) //null/nan should leave depth as 0, any negative value is an error
			return EvaluableNodeReference::Null();
	}

	//make sure have a large enough stack
	if(depth >= constructionStackIndicesAndUniqueness.size())
		return EvaluableNodeReference::Null();

	size_t offset = constructionStackNodes->size() - (constructionStackOffsetStride * depth) + constructionStackOffsetTarget;
	return EvaluableNodeReference( (*constructionStackNodes)[offset], false);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CURRENT_INDEX(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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
		return AllocReturn(enivwt.nodeValue.number, immediate_result);
	else if(enivwt.nodeType == ENIVT_STRING_ID)
		return Parser::ParseFromKeyStringId(enivwt.nodeValue.stringID, evaluableNodeManager);
	else
		return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CURRENT_VALUE(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

	size_t offset = constructionStackNodes->size() - (constructionStackOffsetStride * depth) + constructionStackOffsetCurrentValue;
	return EvaluableNodeReference( (*constructionStackNodes)[offset], false);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_PREVIOUS_RESULT(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_OPCODE_STACK(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	bool has_valid_depth = false;
	int64_t depth;
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
			stack_top_holder.SetOrderedChildNodes(*opcodeStackNodes);
			return evaluableNodeManager->DeepAllocCopy(&stack_top_holder);
		}
		else
		{
			EvaluableNodeReference stack_top_holder(evaluableNodeManager->AllocNode(ENT_LIST), true);
			auto &sth_ocn = stack_top_holder->GetOrderedChildNodesReference();
			sth_ocn.reserve(opcodeStackNodes->size());
			for(auto iter = begin(*opcodeStackNodes); iter != end(*opcodeStackNodes); ++iter)
			{
				EvaluableNode *cur_node = *iter;
				EvaluableNodeReference new_node(evaluableNodeManager->AllocNode(cur_node->GetType()), true);
				new_node->CopyMetadataFrom(cur_node);
				sth_ocn.push_back(new_node);
				stack_top_holder.UpdatePropertiesBasedOnAttachedNode(new_node);
			}
			return stack_top_holder;
		}
	}
	else
	{
		//only return one node from the opcode stack
		int64_t actual_offset;
		if(depth < 0)
			actual_offset = opcodeStackNodes->size() + depth;
		else
			actual_offset = depth;
			
		if(actual_offset < 0 || actual_offset >= static_cast<int64_t>(opcodeStackNodes->size()))
		{
			return EvaluableNodeReference::Null();
		}
		else
		{
			EvaluableNode *cur_node = *(end(*opcodeStackNodes) - actual_offset - 1);
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_STACK(EvaluableNode *en, bool immediate_result)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::ReadLock lock;
	if(callStackMutex != nullptr)
		LockWithoutBlockingGarbageCollection(*callStackMutex, lock);
#endif

	//can create this node on the stack because will be making a copy
	EvaluableNode stack_top_holder(ENT_LIST);
	stack_top_holder.SetOrderedChildNodes(*callStackNodes);
	return evaluableNodeManager->DeepAllocCopy(&stack_top_holder);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ARGS(EvaluableNode *en, bool immediate_result)
{
	size_t depth = 0;
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() > 0)
	{
		double value = InterpretNodeIntoNumberValue(ocn[0]);
		depth = static_cast<size_t>(value);
	}

	//make sure have a large enough stack
	if(callStackNodes->size() > depth)
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock;
		if(callStackMutex != nullptr && GetCallStackDepth() < callStackUniqueAccessStartingDepth)
			LockWithoutBlockingGarbageCollection(*callStackMutex, lock);
	#endif

		//0 index is top of stack
		EvaluableNode *args = (*callStackNodes)[callStackNodes->size() - (depth + 1)];
		//need to make a copy because when the call stack is popped, it will be freed
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(args), false);
	}
	else
		return EvaluableNodeReference::Null();
}

//given an assoc of StringID -> value representing the probability weight of each, and a random stream, it randomly selects from the assoc
// if it can't find an appropriate probability, it returns an empty string
// if normalize is true, then it will accumulate the probability and then normalize
static StringInternPool::StringID GetRandomWeightedKey(EvaluableNode::AssocType &assoc, RandomStream &rs, bool normalize)
{
	double probability_target = rs.RandFull();
	double accumulated_probability = 0.0;
	double total_probability = 1.0;

	if(normalize)
	{
		total_probability = 0;
		for(auto &[_, prob] : assoc)
			total_probability += std::max(0.0, EvaluableNode::ToNumber(prob, 0.0));

		//if no probabilities, just choose uniformly
		if(total_probability <= 0.0)
		{
			//find index to return
			size_t index_to_return = static_cast<size_t>(assoc.size() * probability_target);

			//iterate over pairs until find the index
			size_t cur_index = 0;
			for(auto &[prob_id, _] : assoc)
			{
				if(cur_index == index_to_return)
					return prob_id;

				cur_index++;
			}

			return StringInternPool::NOT_A_STRING_ID;
		}

		if(total_probability == std::numeric_limits<double>::infinity())
		{
			//start over, count infinities
			size_t inf_count = 0;
			for(auto &[_, prob] : assoc)
			{
				if(EvaluableNode::ToNumber(prob, 0.0) == std::numeric_limits<double>::infinity())
					inf_count++;
			}

			//get the infinity to use
			inf_count = static_cast<size_t>(inf_count * probability_target);

			//count down until the infinite pair is found
			for(auto &[prob_id, prob] : assoc)
			{
				if(EvaluableNode::ToNumber(prob, 0.0) == std::numeric_limits<double>::infinity())
				{
					if(inf_count == 0)
						return prob_id;
					inf_count--;
				}
			}

			//shouldn't make it here
			return StringInternPool::NOT_A_STRING_ID;
		}
	}

	for(auto &[prob_id, prob] : assoc)
	{
		accumulated_probability += (EvaluableNode::ToNumber(prob, 0.0) / total_probability);
		if(probability_target < accumulated_probability)
			return prob_id;
	}

	//probability mass didn't add up, just grab the first one with a probability greater than zero
	for(auto &[prob_id, prob] : assoc)
	{
		if(EvaluableNode::ToNumber(prob, 0.0) > 0)
			return prob_id;
	}

	//nothing valid to return
	return StringInternPool::NOT_A_STRING_ID;
}

//Generates an EvaluableNode containing a random value based on the random parameter param, using enm and random_stream
// if any part of param is preserved in the return value, then can_free_param will be set to false, otherwise it will be left alone
EvaluableNodeReference GenerateRandomValueBasedOnRandParam(EvaluableNodeReference param, Interpreter *interpreter,
	RandomStream &random_stream, bool &can_free_param, bool immediate_result)
{
	if(EvaluableNode::IsNull(param))
		return interpreter->AllocReturn(random_stream.RandFull(), immediate_result);

	if(param->GetNumChildNodes() > 0)
	{
		if(param->IsAssociativeArray())
		{
			StringInternPool::StringID id_selected = GetRandomWeightedKey(param->GetMappedChildNodesReference(),
				random_stream, true);
			return Parser::ParseFromKeyStringId(id_selected, interpreter->evaluableNodeManager);
		}
		else if(param->IsOrderedArray())
		{
			auto &ocn = param->GetOrderedChildNodesReference();
			size_t selection = random_stream.RandSize(ocn.size());
			can_free_param = false;
			return EvaluableNodeReference(ocn[selection], param.unique);
		}
	}
	else if(DoesEvaluableNodeTypeUseNumberData(param->GetType()))
	{
		double value = random_stream.RandFull() * param->GetNumberValueReference();
		return interpreter->AllocReturn(value, immediate_result);
	}

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_RAND(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
	{
		double r = randomStream.RandFull();
		return AllocReturn(r, immediate_result);
	}

	//get number to generate
	bool generate_list = false;
	size_t number_to_generate = 1;
	if(ocn.size() >= 2)
	{
		double num_value = InterpretNodeIntoNumberValue(ocn[1]);
		if(FastIsNaN(num_value) || num_value < 0)
			return EvaluableNodeReference::Null();
		number_to_generate = static_cast<size_t>(num_value);
		generate_list = true;
		//because generating a list, can no longer return an immediate
		immediate_result = false;
	}
	//make sure not eating up too much memory
	if(ConstrainedAllocatedNodes())
	{
		if(performanceConstraints->WouldNewAllocatedNodesExceedConstraint(
				evaluableNodeManager->GetNumberOfUsedNodes() + number_to_generate))
			return EvaluableNodeReference::Null();
	}

	//get whether it needs to be unique
	bool generate_unique_values = false;
	if(ocn.size() >= 3)
		generate_unique_values = InterpretNodeIntoBoolValue(ocn[2]);

	//get random param
	auto param = InterpretNodeForImmediateUse(ocn[0]);

	//if generating a single value
	if(!generate_list)
	{
		bool can_free_param = true;
		EvaluableNodeReference rand_value = GenerateRandomValueBasedOnRandParam(param,
				this, randomStream, can_free_param, immediate_result);

		if(can_free_param)
			evaluableNodeManager->FreeNodeTreeIfPossible(param);
		else
			evaluableNodeManager->FreeNodeIfPossible(param);
		return rand_value;
	}

	if(generate_unique_values && !EvaluableNode::IsNull(param) && param->GetNumChildNodes() > 0)
	{
		//clamp to the maximum number that can possibly be generated
		size_t num_elements = (param == nullptr ? 0 : param->GetNumChildNodes());
		number_to_generate = std::min(number_to_generate, num_elements);

		if(param->IsAssociativeArray())
		{
			//want to generate multiple values, so return a list
			EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);
			auto &retval_ocn = retval->GetOrderedChildNodesReference();
			retval_ocn.reserve(number_to_generate);

			//make a copy of all of the probabilities so they can be removed one at a time
			EvaluableNode::AssocType assoc(param->GetMappedChildNodesReference());

			for(size_t i = 0; i < number_to_generate; i++)
			{
				StringInternPool::StringID selected_sid = GetRandomWeightedKey(assoc, randomStream, true);
				EvaluableNodeReference selected_value = Parser::ParseFromKeyStringId(selected_sid, evaluableNodeManager);
				retval_ocn.push_back(selected_value);
				retval.UpdatePropertiesBasedOnAttachedNode(selected_value, i == 0);

				//remove the element so it won't be reselected
				assoc.erase(selected_sid);
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(param);
			return retval;
		}

		//want to generate multiple values, so return a list
		//try to reuse param if can so don't need to allocate more memory
		EvaluableNodeReference retval;
		if(param.unique)
		{
			retval = param;
		}
		else
		{
			retval = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_LIST), true);
			retval->SetOrderedChildNodes(param->GetOrderedChildNodesReference(),
				param->GetNeedCycleCheck(), param->GetIsIdempotent());

			retval.UpdatePropertiesBasedOnAttachedNode(param, true);
		}

		//shuffle ordered child nodes
		auto &retval_ocn = retval->GetOrderedChildNodesReference();
		for(size_t i = 0; i < number_to_generate; i++)
		{
			size_t to_swap_with = randomStream.RandSize(num_elements);
			std::swap(retval_ocn[i], retval_ocn[to_swap_with]);
		}

		//free unneeded nodes that weren't part of the shuffle
		if(param.unique && !param->GetNeedCycleCheck())
		{
			for(size_t i = number_to_generate; i < num_elements; i++)
				evaluableNodeManager->FreeNodeTree(retval_ocn[i]);
		}

		//get rid of unneeded extra nodes
		retval->SetOrderedChildNodesSize(number_to_generate);
		retval->ReleaseOrderedChildNodesExtraMemory();

		return retval;
	}

	//want to generate multiple values, so return a list
	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);

	//just generate a list of values with replacement; either generate_unique_values was not set or the distribution "always" generates unique values
	retval->ReserveOrderedChildNodes(number_to_generate);

	bool can_free_param = true;

	//get information to determine which mechanism to use to generate
	size_t num_weighted_values = 0;
	if(param != nullptr && param->IsAssociativeArray())
		num_weighted_values = param->GetMappedChildNodesReference().size();

	if(num_weighted_values > 0
		&& (number_to_generate > 10 || (number_to_generate > 3 && num_weighted_values > 200)))
	{
		//use fast repeated generation technique
		WeightedDiscreteRandomStreamTransform<StringInternPool::StringID,
			EvaluableNode::AssocType, EvaluableNodeAsDouble> wdrst(param->GetMappedChildNodesReference(), false);
		for(size_t i = 0; i < number_to_generate; i++)
		{
			EvaluableNodeReference rand_value(Parser::ParseFromKeyStringId(wdrst.WeightedDiscreteRand(randomStream), evaluableNodeManager));
			retval->AppendOrderedChildNode(rand_value);
		}
	}
	else //perform simple generation
	{
		for(size_t i = 0; i < number_to_generate; i++)
		{
			EvaluableNodeReference rand_value = GenerateRandomValueBasedOnRandParam(param,
				this, randomStream, can_free_param, immediate_result);
			retval->AppendOrderedChildNode(rand_value);
			retval.UpdatePropertiesBasedOnAttachedNode(rand_value, i == 0);
		}
	}

	if(can_free_param)
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(param);
	}
	else
	{
		//if used the parameters, a parameter might be used more than once
		retval->SetNeedCycleCheck(true);
		evaluableNodeManager->FreeNodeIfPossible(param);
	}

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_RAND_SEED(EvaluableNode *en, bool immediate_result)
{
	std::string rand_state_string = randomStream.GetState();
	return AllocReturn(rand_state_string, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_RAND_SEED(EvaluableNode *en, bool immediate_result)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto seed_node = InterpretNodeForImmediateUse(ocn[0]);
	std::string seed_string;
	if(seed_node != nullptr && seed_node->GetType() == ENT_STRING)
		seed_string = seed_node->GetStringValue();
	else
		seed_string = Parser::Unparse(seed_node, false, false, true);

	randomStream.SetState(seed_string);

	return seed_node;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SYSTEM_TIME(EvaluableNode *en, bool immediate_result)
{
	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.individualPermissions.environment)
		return EvaluableNodeReference::Null();

	std::chrono::time_point tp = std::chrono::system_clock::now();
	std::chrono::system_clock::duration duration_us = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
	std::chrono::duration<double, std::ratio<1>> double_duration_us = duration_us;
	double sec = double_duration_us.count();

	return AllocReturn(sec, immediate_result);
}

//error handling

EvaluableNodeReference Interpreter::InterpretNode_ENT_DEALLOCATED(EvaluableNode *en, bool immediate_result)
{
	std::cerr << "ERROR: attempt to use freed memory\n";
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(false);
#endif
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NOT_A_BUILT_IN_TYPE(EvaluableNode *en, bool immediate_result)
{
	std::cerr << "ERROR: encountered an invalid instruction\n";
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(false);
#endif
	return EvaluableNodeReference::Null();
}

void Interpreter::VerifyEvaluableNodeIntegrity()
{
	for(EvaluableNode *en : *callStackNodes)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en);

	for(EvaluableNode *en : *opcodeStackNodes)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en, nullptr, false);

	for(EvaluableNode *en : *constructionStackNodes)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en);

	if(curEntity != nullptr)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(curEntity->GetRoot());

	{
		auto &nr = evaluableNodeManager->GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleLock lock(nr.mutex);
	#endif
		for(auto &[en, _] : nr.nodesReferenced)
			EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en, nullptr, false);
	}

	if(callingInterpreter != nullptr)
		callingInterpreter->VerifyEvaluableNodeIntegrity();
}
