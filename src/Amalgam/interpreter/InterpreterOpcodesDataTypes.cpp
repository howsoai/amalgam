//project headers:
#include "Interpreter.h"

#include "AssetManager.h"
#include "Cryptography.h"
#include "DateTimeFormat.h"
#include "EvaluableNodeTreeManipulation.h"
#include "EntityWriteListener.h"
#include "FileSupportJSON.h"
#include "FileSupportYAML.h"
#include "PlatformSpecific.h"

//system headers:
#include <regex>
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_NULL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LIST_and_UNORDERED_LIST(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
		return evaluableNodeManager->DeepAllocCopy(en, EvaluableNodeManager::ENMM_REMOVE_ALL);

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

				ConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSOC(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
		return evaluableNodeManager->DeepAllocCopy(en, EvaluableNodeManager::ENMM_REMOVE_ALL);

	//create a new assoc from the previous
	EvaluableNodeReference new_assoc(evaluableNodeManager->AllocNode(en, EvaluableNodeManager::ENMM_REMOVE_ALL), true);

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

				ConcurrencyManager concurrency_manager(this, num_nodes, enqueue_task_lock);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_BOOL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	bool value = en->GetBoolValueReference();
	return AllocReturn(value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NUMBER(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	double value = en->GetNumberValueReference();
	return AllocReturn(value, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_STRING(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	StringInternPool::StringID value = en->GetStringIDReference();
	return AllocReturn(value, immediate_result);
}

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
		auto [label_value, label_found] = curEntity->GetValueAtLabel(sid, nullptr, true, immediate_result, true);
		if(label_found)
			return label_value;
	}

	EmitOrLogUndefinedVariableWarningIfNeeded(sid, en);

	return EvaluableNodeReference::Null();
}

void Interpreter::EmitOrLogUndefinedVariableWarningIfNeeded(StringInternPool::StringID not_found_variable_sid, EvaluableNode *en)
{
	std::string warning = "";

	warning.append("Warning: undefined symbol " + not_found_variable_sid->string);

	if(asset_manager.debugSources && en->HasComments())
	{
		std::string comment_string = en->GetCommentsString();
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
		EntityPermissions entity_permissions = asset_manager.GetEntityPermissions(curEntity);
		if(entity_permissions.HasPermission(EntityPermissions::Permission::STD_OUT_AND_STD_ERR))
			std::cerr << warning << std::endl;
	}
}

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_LABELS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference n = InterpretNodeForImmediateUse(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	size_t num_labels = n->GetNumLabels();

	//make list of labels
	EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_LIST), true);
	auto &result_ocn = result->GetOrderedChildNodesReference();
	result_ocn.resize(num_labels);

	//because labels can be stored in different ways, it is just easiest to iterate
	// rather than to get a reference to each string id
	for(size_t i = 0; i < num_labels; i++)
		result_ocn[i] = evaluableNodeManager->AllocNode(ENT_STRING, n->GetLabelStringId(i));

	evaluableNodeManager->FreeNodeTreeIfPossible(n);
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ALL_LABELS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	EvaluableNodeReference n = EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() > 0)
		n = InterpretNodeForImmediateUse(ocn[0]);

	EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_ASSOC), n.unique, true);

	auto [label_sids_to_nodes, _] = EvaluableNodeTreeManipulation::RetrieveLabelIndexesFromTree(n);

	result->ReserveMappedChildNodes(label_sids_to_nodes.size());
	for(auto &[node_id, node] : label_sids_to_nodes)
		result->SetMappedChildNode(node_id, node);

	//can't guarantee there weren't any cycles if more than one label
	if(label_sids_to_nodes.size() > 1)
		result->SetNeedCycleCheck(true);

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_LABELS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);

	evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the labels
	auto label_list = InterpretNodeForImmediateUse(ocn[1]);
	if(label_list != nullptr && label_list->GetType() != ENT_LIST)
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(label_list);
		return source;
	}

	source->ClearLabels();

	//if adding labels, then grab from the provided list
	if(label_list != nullptr)
	{
		for(auto &e : label_list->GetOrderedChildNodes())
		{
			if(e != nullptr)
			{
				//obtain the label, reusing the sid reference if possible
				StringInternPool::StringID label_sid = string_intern_pool.emptyStringId;
				if(label_list.unique)
					label_sid = EvaluableNode::ToStringIDTakingReferenceAndClearing(e);
				else
					label_sid = EvaluableNode::ToStringIDWithReference(e);

				if(label_sid != string_intern_pool.NOT_A_STRING_ID)
					source->AppendLabelStringId(label_sid, true);
			}
		}
	}
	evaluableNodeManager->FreeNodeTreeIfPossible(label_list);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ZIP_LABELS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto label_list = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateOpcodeStackStateSaver(label_list);

	auto source = InterpretNode(ocn[1]);

	//if no label list, or no source or source is immediate, then just return the source
	if(EvaluableNode::IsNull(label_list) || !label_list->IsOrderedArray()
			|| EvaluableNode::IsNull(source) || !source->IsOrderedArray())
		return source;

	node_stack.PopEvaluableNode();

	//make copy to populate with copies of the child nodes
	//start assuming that the copy will be unique, but set to not unique if any chance the assumption
	// might not hold
	EvaluableNodeReference retval = source;
	evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto &label_list_ocn = label_list->GetOrderedChildNodesReference();

	//copy over labels to each child node, allocating a new child node if needed
	auto &retval_ocn = retval->GetOrderedChildNodesReference();
	for(size_t i = 0; i < retval_ocn.size(); i++)
	{
		//no more labels to add, so just leave the existing nodes
		if(i >= label_list_ocn.size())
			break;

		//make sure the child node can have a label appended
		if(retval_ocn[i] == nullptr)
			retval_ocn[i] = evaluableNodeManager->AllocNode(ENT_NULL);
		else if(!source.unique)
			retval_ocn[i] = evaluableNodeManager->AllocNode(retval_ocn[i]);

		//obtain the label, reusing the sid reference if possible
		StringInternPool::StringID label_sid = string_intern_pool.emptyStringId;
		if(label_list.unique)
			label_sid = EvaluableNode::ToStringIDTakingReferenceAndClearing(label_list_ocn[i]);
		else
			label_sid = EvaluableNode::ToStringIDWithReference(label_list_ocn[i]);

		retval_ocn[i]->AppendLabelStringId(label_sid, true);
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(label_list);

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_COMMENTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	StringInternPool::StringID comments_sid = n->GetCommentsStringId();
	evaluableNodeManager->FreeNodeTreeIfPossible(n);
	return AllocReturn(comments_sid, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_COMMENTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the comments
	StringInternPool::StringID new_comments_sid = InterpretNodeIntoStringIDValueWithReference(ocn[1]);
	source->SetCommentsStringId(new_comments_sid, true);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_CONCURRENCY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();
	auto n = InterpretNodeForImmediateUse(ocn[0]);
	
	return AllocReturn(n != nullptr && n->GetConcurrency(), immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_CONCURRENCY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);
	else
		evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the concurrent flag
	bool concurrency = InterpretNodeIntoBoolValue(ocn[1]);
	source->SetConcurrency(concurrency);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_VALUE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();
	auto n = InterpretNode(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	if(n.uniqueUnreferencedTopNode)
		n->ClearMetadata();
	else
		evaluableNodeManager->EnsureNodeIsModifiable(n, false, EvaluableNodeManager::ENMM_REMOVE_ALL);

	return n;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_VALUE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);
	else
		evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the new value
	auto value_node = InterpretNode(ocn[1]);
	source->CopyValueFrom(value_node);
	source.UpdatePropertiesBasedOnAttachedNode(value_node, true);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EXPLODE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto [valid, str] = InterpretNodeIntoStringValue(ocn[0]);
	if(!valid)
		return EvaluableNodeReference::Null();

	EvaluableNode *result = evaluableNodeManager->AllocNode(ENT_LIST);
	auto node_stack = CreateOpcodeStackStateSaver(result);

	//a stride of 0 means use variable width utf-8
	size_t stride = 0;
	if(ocn.size() > 1)
	{
		double raw_stride = InterpretNodeIntoNumberValue(ocn[1]);
		if(raw_stride > 0)
			stride = static_cast<size_t>(raw_stride);
	}

	if(stride == 0)
	{
		//pessimistically reserve enough space assuming worst case of each byte being its own character
		result->ReserveOrderedChildNodes(str.size());

		size_t utf8_char_start_offset = 0;
		while(utf8_char_start_offset < str.size())
		{
			size_t utf8_char_length = StringManipulation::GetUTF8CharacterLength(str, utf8_char_start_offset);
			//done if no more characters
			if(utf8_char_length == 0)
				break;

			//create a new node for each character in the string
			result->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, str.substr(utf8_char_start_offset, utf8_char_length)));

			utf8_char_start_offset += utf8_char_length;
		}
	}
	else //nonzero stride
	{
		//reserve enough space, and round up for any remainder
		result->ReserveOrderedChildNodes((str.size() + (stride - 1)) / stride);

		while(str.size() >= stride)
		{
			std::string substr(begin(str), begin(str) + stride);
			result->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, substr));

			str.erase(0, stride);
		}

		//some left over, but less than stride, so just append
		if(str.size() > 0)
			result->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, str));

	}

	return EvaluableNodeReference(result, true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SPLIT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);
	auto node_stack = CreateOpcodeStackStateSaver(retval);

	//if only one element, nothing to split on, just return the string in a list
	if(ocn.size() == 1)
	{
		auto str_node = InterpretNodeIntoUniqueStringIDValueEvaluableNode(ocn[0]);
		retval->AppendOrderedChildNode(str_node);
		return retval;
	}

	//have at least two parameters
	auto [valid_string_to_split, string_to_split] = InterpretNodeIntoStringValue(ocn[0]);
	if(!valid_string_to_split)
	{
		retval->SetType(ENT_NULL, nullptr, false);
		return retval;
	}

	auto [valid_split_value, split_value] = InterpretNodeIntoStringValue(ocn[1]);
	if(!valid_split_value)
	{
		retval->SetType(ENT_NULL, nullptr, false);
		return retval;
	}

	double max_split_count = std::numeric_limits<double>::infinity();
	if(ocn.size() >= 3)
	{
		//only use the value if it's greater than zero
		double max_split_count_value = InterpretNodeIntoNumberValue(ocn[2]);
		if(max_split_count_value > 0)
			max_split_count = max_split_count_value;
	}

	//a stride of 0 means use variable width utf-8
	size_t stride = 0;
	if(ocn.size() >= 4)
	{
		double raw_stride = InterpretNodeIntoNumberValue(ocn[3]);
		if(raw_stride > 0)
			stride = static_cast<size_t>(raw_stride);
	}

	//if stride is 0, then use regex
	if(stride == 0)
	{
		//use nosubs to prevent unnecessary memory allocations since this is just matching
		std::regex rx;
		try {
			rx.assign(split_value, std::regex::ECMAScript | std::regex::nosubs);
		}
		catch(...)
		{
			return retval;
		}

		//-1 argument indicates splitting rather than matching
		std::sregex_token_iterator iter(begin(string_to_split), end(string_to_split), rx, -1);
		std::sregex_token_iterator rx_end;

		//split the string
		size_t num_split = 0;
		for(; iter != rx_end && num_split < max_split_count; ++iter, num_split++)
		{
			std::string value = *iter;
			retval->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, value));
		}

		//ran out of split count, need to include the last bit
		if(num_split == max_split_count && iter != rx_end)
		{
			//determine offset of the beginning of the leftover part of the string not matched
			//do this separately because it's nontrivial to get types to match
			auto pos = (*iter).first - begin(string_to_split);
			std::string value(begin(string_to_split) + pos, end(string_to_split));
			retval->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, value));
		}
	}
	else //not regex
	{
		size_t cur_segment_start = 0;
		size_t cur_segment_end = 0;
		size_t string_to_split_len = string_to_split.length();
		size_t split_value_len = split_value.length();

		while(cur_segment_end < string_to_split_len && max_split_count > 0)
		{
			size_t cur_match_position = cur_segment_end;
			size_t cur_split_position = 0;

			//advance forward through the split string
			while(cur_split_position < split_value_len
				&& string_to_split[cur_match_position] == split_value[cur_split_position])
			{
				cur_match_position += stride;
				cur_split_position += stride;
			}

			//if found the string
			if(cur_split_position >= split_value_len)
			{
				std::string value(begin(string_to_split) + cur_segment_start,
					begin(string_to_split) + cur_match_position - cur_split_position);
				retval->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, value));

				cur_segment_end = cur_match_position;
				cur_segment_start = cur_match_position;

				//if infinite, won't count against
				max_split_count -= 1;
			}
			else //didn't find the string, move forward one character
			{
				cur_segment_end += stride;
			}
		}

		//attach last segment if it exists
		if(cur_segment_start < string_to_split_len)
			retval->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING,
				std::string(begin(string_to_split) + cur_segment_start, end(string_to_split))));
	}

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SUBSTR(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//if only string as the parameter, just return a new copy of the string
	if(ocn.size() == 1)
		return InterpretNodeIntoUniqueStringIDValueEvaluableNode(ocn[0], immediate_result);

	//have at least 2 params
	auto [valid_string_to_substr, string_to_substr] = InterpretNodeIntoStringValue(ocn[0]);
	if(!valid_string_to_substr)
		return AllocReturn(string_intern_pool.NOT_A_STRING_ID, immediate_result);

	bool replace_string = false;
	std::string replacement_string;
	if(ocn.size() >= 4 && !EvaluableNode::IsNull(ocn[3]))
	{
		replace_string = true;
		auto [valid_replacement_string, temp_replacement_string] = InterpretNodeIntoStringValue(ocn[3]);
		//because otherwise previous line becomes clunky
		std::swap(replacement_string, temp_replacement_string);

		if(!valid_replacement_string)
			return AllocReturn(string_intern_pool.NOT_A_STRING_ID, immediate_result);
	}

	EvaluableNodeReference substr_node = InterpretNodeForImmediateUse(ocn[1]);
	if(EvaluableNode::IsNull(substr_node))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(substr_node);
		return EvaluableNodeReference::Null();
	}

	//if a number, then go by offset
	if(substr_node->IsNumericOrNull())
	{
		double start_offset_raw = EvaluableNode::ToNumber(substr_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(substr_node);

		double length_raw = static_cast<double>(string_to_substr.size());
		if(ocn.size() >= 3)
			length_raw = InterpretNodeIntoNumberValue(ocn[2]);

		//a stride of 0 means use variable width utf-8
		size_t stride = 0;
		if(ocn.size() >= 5)
		{
			double raw_stride = InterpretNodeIntoNumberValue(ocn[4]);
			if(raw_stride > 0)
				stride = static_cast<size_t>(raw_stride);
		}

		//get start of substring
		size_t start_offset = 0;
		if(start_offset_raw >= 0)
		{
			if(stride == 0)
				start_offset = StringManipulation::GetNthUTF8CharacterOffset(string_to_substr, static_cast<size_t>(start_offset_raw));
			else
				start_offset = stride * static_cast<size_t>(start_offset_raw);
		}
		else if(start_offset_raw < 0)
		{
			if(stride == 0)
				start_offset = StringManipulation::GetNthLastUTF8CharacterOffset(string_to_substr, static_cast<size_t>(-start_offset_raw));
			else
			{
				size_t backward_offset = stride * static_cast<size_t>(-start_offset_raw);
				if(backward_offset < string_to_substr.size())
					start_offset = (string_to_substr.size() - backward_offset);
			}
		}
		//if failed both ifs then must be nan, so leave default

		//get end of substring
		size_t end_offset = string_to_substr.size();
		//only need to do end processing if have a value smaller than the length
		if(length_raw < end_offset)
		{
			if(length_raw >= 0)
			{
				if(stride == 0)
					end_offset = StringManipulation::GetNthUTF8CharacterOffset(std::string_view(&string_to_substr[start_offset]), static_cast<size_t>(length_raw));
				else
					end_offset = start_offset + stride * static_cast<size_t>(length_raw);
			}
			else if(length_raw < 0)
			{
				if(stride == 0)
				{
					end_offset = start_offset + StringManipulation::GetNthLastUTF8CharacterOffset(std::string_view(&string_to_substr[start_offset]),
						static_cast<size_t>(-length_raw));
				}
				else
				{
					size_t backward_offset = stride * static_cast<size_t>(-length_raw);
					if(backward_offset < string_to_substr.size())
						end_offset = (string_to_substr.size() - backward_offset);
				}
			}
			//if failed both ifs then must be nan, so leave default
		}

		if(replace_string)
		{
			std::string rebuilt_string;
			if(start_offset < string_to_substr.size())
				rebuilt_string += string_to_substr.substr(0, start_offset);

			rebuilt_string += replacement_string;
			if(end_offset < string_to_substr.size())
				rebuilt_string += string_to_substr.substr(end_offset);

			return AllocReturn(rebuilt_string, immediate_result);
		}
		else //return just the substring
		{
			std::string substr;
			if(start_offset < string_to_substr.size() && end_offset > start_offset)
				substr = string_to_substr.substr(start_offset, end_offset - start_offset);

			return AllocReturn(substr, immediate_result);
		}
	}
	else if(substr_node->GetType() == ENT_STRING)
	{
		//make a copy of the string so the node can be freed
		//(if this is a performance cost found in profiling, it can be fixed with more logic)
		auto &regex_str = substr_node->GetStringValue();
		evaluableNodeManager->FreeNodeTreeIfPossible(substr_node);

		if(replace_string)
		{
			double max_match_count = std::numeric_limits<double>::infinity();
			if(ocn.size() >= 3)
			{
				//only use the value if it's greater than zero
				double max_match_count_value = InterpretNodeIntoNumberValue(ocn[2]);
				if(max_match_count_value > 0)
					max_match_count = max_match_count_value;
			}

			std::regex rx;
			try {
				rx.assign(regex_str, std::regex::ECMAScript);
			}
			catch(...)
			{
				//bad regex, so nothing was replaced, just return original
				return AllocReturn(string_to_substr, immediate_result);
			}

			std::string updated_string;
			if(max_match_count == std::numeric_limits<double>::infinity())
			{
				updated_string = std::regex_replace(string_to_substr, rx, replacement_string);
			}
			else //need to count matches
			{
				auto out = std::back_inserter(updated_string);
				auto iter = std::sregex_iterator(begin(string_to_substr), end(string_to_substr), rx);
				auto end = std::sregex_iterator();
				auto last_iter = iter;

				for(size_t n = static_cast<size_t>(max_match_count); n > 0 && iter != end; ++iter, n--)
				{
					//copy out the replacement
					out = std::copy(iter->prefix().first, iter->prefix().second, out);
					out = iter->format(out, replacement_string);
					last_iter = iter;
				}

				//reset out to the full string
				out = std::copy(last_iter->suffix().first, last_iter->suffix().second, out);
			}

			return AllocReturn(updated_string, immediate_result);
		}
		else //finding matches
		{
			EvaluableNodeReference param_node = EvaluableNodeReference::Null();
			if(ocn.size() >= 3)
				param_node = InterpretNodeForImmediateUse(ocn[2]);

			//these three options are mutually exclusive
			//if true, returns first full match as a string
			bool first_match_only = true;
			//if true, returns full matches up to match_count
			bool full_matches = false;
			//if true, returns all submatches up to match_count
			bool submatches = false;
			//maximum number of matches allowed
			double max_match_count = std::numeric_limits<double>::infinity();

			if(!EvaluableNode::IsNull(param_node))
			{
				if(param_node->GetType() == ENT_STRING)
				{
					auto pnsid = param_node->GetStringIDReference();
					if(pnsid == GetStringIdFromBuiltInStringId(ENBISI_all))
					{
						first_match_only = false;
						full_matches = true;
					}
					else if(pnsid == GetStringIdFromBuiltInStringId(ENBISI_submatches))
					{
						first_match_only = false;
						submatches = true;
					}
				}
				else
				{
					double param_num = EvaluableNode::ToNumber(param_node);
					if(param_num >= 0)
					{
						first_match_only = false;
						full_matches = true;
						max_match_count = param_num;
					}
					else if(param_num < 0)
					{
						first_match_only = false;
						submatches = true;
						max_match_count = -param_num;
					}
					//else NaN -- leave defaults

				}

				evaluableNodeManager->FreeNodeTreeIfPossible(param_node);
			}

			if(first_match_only)
			{
				//find first match, don't need submatches
				std::regex rx;
				try {
					rx.assign(regex_str, std::regex::ECMAScript | std::regex::nosubs);
				}
				catch(...)
				{
					//bad regex, return same as not found
					return AllocReturn(string_intern_pool.NOT_A_STRING_ID, immediate_result);
				}

				std::sregex_token_iterator iter(begin(string_to_substr), end(string_to_substr), rx);
				std::sregex_token_iterator rx_end;
				if(iter == rx_end)
				{
					//not found
					return AllocReturn(string_intern_pool.NOT_A_STRING_ID, immediate_result);
				}
				else
				{
					std::string value = *iter;
					return AllocReturn(value, immediate_result);
				}
			}
			else if(full_matches)
			{
				EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);

				//find all the matches, don't need submatches
				std::regex rx;
				try {
					rx.assign(regex_str, std::regex::ECMAScript | std::regex::nosubs);
				}
				catch(...)
				{
					return retval;
				}

				size_t num_split = 0;
				std::sregex_token_iterator iter(begin(string_to_substr), end(string_to_substr), rx);
				std::sregex_token_iterator rx_end;
				for(; iter != rx_end && num_split < max_match_count; ++iter, num_split++)
				{
					std::string value = *iter;
					retval->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, value));
				}

				return retval;
			}
			else if(submatches)
			{
				EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);

				std::regex rx;
				try {
					rx.assign(regex_str, std::regex::ECMAScript);
				}
				catch(...)
				{
					return retval;
				}

				std::sregex_iterator iter(begin(string_to_substr), end(string_to_substr), rx);
				std::sregex_iterator rx_end;

				//find all the matches
				size_t num_split = 0;
				for(; iter != rx_end && num_split < max_match_count; ++iter, num_split++)
				{
					EvaluableNode *cur_match_elements = evaluableNodeManager->AllocNode(ENT_LIST);
					retval->AppendOrderedChildNode(cur_match_elements);

					for(std::string s : *iter)
						cur_match_elements->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, s));
				}

				return retval;
			}
			else //not a valid match state
			{
				return EvaluableNodeReference::Null();
			}
		}
	}
	else //not a valid substr
	{
		return EvaluableNodeReference::Null();
	}
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONCAT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//build string from all child nodes
	auto &ocn = en->GetOrderedChildNodesReference();

	//if only one parameter is specified, do a fast shortcut
	if(ocn.size() == 1)
		return InterpretNodeIntoUniqueStringIDValueEvaluableNode(ocn[0], immediate_result);

	std::string s;
	for(auto &cn : ocn)
	{
		auto [valid, cur_string] = InterpretNodeIntoStringValue(cn);
		if(!valid)
			return AllocReturn(string_intern_pool.NOT_A_STRING_ID, immediate_result);

		//want to exit early if out of resources because
		// this opcode can chew through memory with string concatenation via returned nulls
		if(AreExecutionResourcesExhausted()
				|| (interpreterConstraints != nullptr && s.size() > interpreterConstraints->maxNumAllocatedNodes) )
			return EvaluableNodeReference::Null();

		//since UTF-8, don't need to do any conversions to concatenate
		s += cur_string;
	}

	return AllocReturn(s, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CRYPTO_SIGN(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	std::string message = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string secret_key = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

	std::string signature = SignMessage(message, secret_key);

	return AllocReturn(signature, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CRYPTO_SIGN_VERIFY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 3)
		return EvaluableNodeReference::Null();

	std::string message = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string public_key = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
	std::string signature = InterpretNodeIntoStringValueEmptyNull(ocn[2]);

	bool valid_sig = IsSignatureValid(message, public_key, signature);

	return AllocReturn(valid_sig, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ENCRYPT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	std::string plaintext = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string key_1 = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

	std::string nonce = "";
	if(ocn.size() >= 3)
		nonce = InterpretNodeIntoStringValueEmptyNull(ocn[2]);

	std::string key_2 = "";
	if(ocn.size() >= 4)
		key_2 = InterpretNodeIntoStringValueEmptyNull(ocn[3]);

	std::string cyphertext = "";

	//if no second key, then use symmetric key encryption
	if(key_2.empty())
		cyphertext = EncryptMessage(plaintext, key_1, nonce);
	else //use public key encryption
		cyphertext = EncryptMessage(plaintext, key_1, key_2, nonce);

	return AllocReturn(cyphertext, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DECRYPT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	std::string cyphertext = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string key_1 = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

	std::string nonce = "";
	if(ocn.size() >= 3)
		nonce = InterpretNodeIntoStringValueEmptyNull(ocn[2]);

	std::string key_2 = "";
	if(ocn.size() >= 4)
		key_2 = InterpretNodeIntoStringValueEmptyNull(ocn[3]);

	std::string plaintext = "";

	//if no second key, then use symmetric key encryption
	if(key_2.empty())
		plaintext = DecryptMessage(cyphertext, key_1, nonce);
	else //use public key encryption
		plaintext = DecryptMessage(cyphertext, key_1, key_2, nonce);

	return AllocReturn(plaintext, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_PRINT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(EntityPermissions::Permission::STD_OUT_AND_STD_ERR))
		return EvaluableNodeReference::Null();

	for(auto &cn : en->GetOrderedChildNodesReference())
	{
		auto cur = InterpretNodeForImmediateUse(cn);

		std::string s;
		if(cur == nullptr)
			s = "(null)";
		else if(DoesEvaluableNodeTypeUseBoolData(cur->GetType()))
			s = EvaluableNode::BoolToString(cur->GetBoolValueReference());
		else if(DoesEvaluableNodeTypeUseStringData(cur->GetType()))
			s = cur->GetStringValue();
		else if(DoesEvaluableNodeTypeUseNumberData(cur->GetType()))
			s = EvaluableNode::NumberToString(cur->GetNumberValueReference());
		else
			s = Parser::Unparse(cur, true, true, true);

		evaluableNodeManager->FreeNodeTreeIfPossible(cur);

		if(writeListeners != nullptr)
		{
			for(auto &wl : *writeListeners)
				wl->LogPrint(s);
		}
		if(printListener != nullptr)
			printListener->LogPrint(s);
	}

	if(writeListeners != nullptr)
	{
		for(auto &wl : *writeListeners)
			wl->FlushLogFile();
	}
	if(printListener != nullptr)
		printListener->FlushLogFile();

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_TOTAL_SIZE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	double total_size = static_cast<double>(EvaluableNode::GetDeepSize(n));
	evaluableNodeManager->FreeNodeTreeIfPossible(n);

	return AllocReturn(total_size, immediate_result);
}
