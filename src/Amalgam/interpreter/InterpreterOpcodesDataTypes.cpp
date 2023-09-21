//project headers:
#include "Interpreter.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "Cryptography.h"
#include "DateTimeFormat.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"
#include "EvaluableNodeTreeDifference.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EntityWriteListener.h"
#include "FileSupportJSON.h"
#include "FileSupportYAML.h"
#include "PerformanceProfiler.h"
#include "PlatformSpecific.h"

//system headers:
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <utility>

EvaluableNodeReference Interpreter::InterpretNode_ENT_TRUE(EvaluableNode *en)
{
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_TRUE), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_FALSE(EvaluableNode *en)
{
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_FALSE), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NULL(EvaluableNode *en)
{
	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_LIST(EvaluableNode *en)
{
	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
		return evaluableNodeManager->DeepAllocCopy(en, EvaluableNodeManager::ENMM_REMOVE_ALL);

	EvaluableNodeReference new_list(evaluableNodeManager->AllocNode(ENT_LIST), true);

	auto &ocn = en->GetOrderedChildNodes();
	size_t num_nodes = ocn.size();
	if(num_nodes > 0)
	{
		new_list->ReserveOrderedChildNodes(num_nodes);

	#ifdef MULTITHREAD_SUPPORT
		if(en->GetConcurrency() && num_nodes > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.BeginEnqueueBatchTask();
			if(enqueue_task_lock.AreThreadsAvailable())
			{
				auto node_stack = CreateInterpreterNodeStackStateSaver(new_list);

				ConcurrencyManager concurrency_manager(this, num_nodes);

				//kick off interpreters
				for(size_t node_index = 0; node_index < num_nodes; node_index++)
					concurrency_manager.PushTaskToResultFuturesWithConstructionStack(ocn[node_index], en, new_list,
						EvaluableNodeImmediateValueWithType(static_cast<double>(node_index)), nullptr);

				enqueue_task_lock.Unlock();

				concurrency_manager.EndConcurrency();

				for(auto &value : concurrency_manager.GetResultsAndFreeReferences())
				{
					//add it to the list
					new_list->AppendOrderedChildNode(value);
					new_list.UpdatePropertiesBasedOnAttachedNode(value);
				}

				return new_list;
			}
		}
	#endif

		//construction stack has a reference, so no KeepNodeReference isn't needed for anything referenced
		PushNewConstructionContext(en, new_list, EvaluableNodeImmediateValueWithType(0.0), nullptr);

		for(size_t i = 0; i < ocn.size(); i++)
		{
			SetTopTargetValueIndexInConstructionStack(static_cast<double>(i));

			auto value = InterpretNode(ocn[i]);
			//add it to the list
			new_list->AppendOrderedChildNode(value);
			new_list.UpdatePropertiesBasedOnAttachedNode(value);
		}

		PopConstructionContext();
	}

	return new_list;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSOC(EvaluableNode *en)
{
	//if idempotent, can just return a copy without any metadata
	if(en->GetIsIdempotent())
	{
		EvaluableNodeReference retval = evaluableNodeManager->DeepAllocCopy(en, EvaluableNodeManager::ENMM_REMOVE_ALL);
		return retval;
	}

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
			auto enqueue_task_lock = Concurrency::threadPool.BeginEnqueueBatchTask();
			if(enqueue_task_lock.AreThreadsAvailable())
			{
				auto node_stack = CreateInterpreterNodeStackStateSaver(new_assoc);
				ConcurrencyManager concurrency_manager(this, num_nodes);

				//kick off interpreters
				for(auto &[cn_id, cn] : new_mcn)
					concurrency_manager.PushTaskToResultFuturesWithConstructionStack(cn, en, new_assoc, EvaluableNodeImmediateValueWithType(cn_id), nullptr);

				enqueue_task_lock.Unlock();
				concurrency_manager.EndConcurrency();

				//add results to assoc
				auto results = concurrency_manager.GetResultsAndFreeReferences();
				//will iterate in the same order as above
				size_t result_index = 0;
				for(auto &[_, cn] : new_mcn)
				{
					auto &value = results[result_index++];

					//add it to the list
					cn = value;
					new_assoc.UpdatePropertiesBasedOnAttachedNode(value);
				}

				return new_assoc;
			}
		}
	#endif

		//construction stack has a reference, so no KeepNodeReference isn't needed for anything referenced
		PushNewConstructionContext(en, new_assoc, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

		for(auto &[cn_id, cn] : new_mcn)
		{
			SetTopTargetValueIndexInConstructionStack(cn_id);

			//compute the value
			EvaluableNodeReference element_result = InterpretNode(cn);

			cn = element_result;
			new_assoc.UpdatePropertiesBasedOnAttachedNode(element_result);
		}

		PopConstructionContext();
	}

	return new_assoc;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_NUMBER(EvaluableNode *en)
{
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(en->GetNumberValueReference()), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_STRING(EvaluableNode *en)
{
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, en->GetStringIDReference()), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SYMBOL(EvaluableNode *en)
{
	StringInternPool::StringID sid = EvaluableNode::ToStringIDIfExists(en);
	if(sid == StringInternPool::NOT_A_STRING_ID)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference value(GetExecutionContextSymbol(sid), false);
	if(value != nullptr)
		return value;

	//if didn't find it in the stack, try it in the labels
	EntityReadReference cur_entity_ref(curEntity);
	if(cur_entity_ref != nullptr)
		return cur_entity_ref->GetValueAtLabel(sid, nullptr, true, true);

	return EvaluableNodeReference::Null();
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_TYPE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNodeForImmediateUse(ocn[0]);
	EvaluableNodeType type = ENT_NULL;
	if(cur != nullptr)
		type = cur->GetType();
	evaluableNodeManager->FreeNodeTreeIfPossible(cur);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(type), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_TYPE_STRING(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNodeForImmediateUse(ocn[0]);
	EvaluableNodeType type = ENT_NULL;
	if(cur != nullptr)
		type = cur->GetType();
	evaluableNodeManager->FreeNodeTreeIfPossible(cur);

	std::string type_string = GetStringFromEvaluableNodeType(type, true);
	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, type_string), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_TYPE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//get the target
	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);

	if(!source.unique)
		source.reference = evaluableNodeManager->AllocNode(source);

	auto node_stack = CreateInterpreterNodeStackStateSaver(source);

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

	source->SetType(new_type, evaluableNodeManager);

	return source;
}

//reinterprets a char value to DestinationType
template<typename DestinationType, typename SourceType = uint8_t>
constexpr DestinationType ExpandCharStorage(char &value)
{
	return static_cast<DestinationType>(reinterpret_cast<SourceType &>(value));
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_FORMAT(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 3)
		return EvaluableNodeReference::Null();

	StringInternPool::StringID from_type = InterpretNodeIntoStringIDValueWithReference(ocn[1]);
	StringInternPool::StringID to_type = InterpretNodeIntoStringIDValueWithReference(ocn[2]);

	auto node_stack = CreateInterpreterNodeStackStateSaver();
	bool node_stack_needs_popping = false;

	EvaluableNodeReference from_params;
	if(ocn.size() > 3)
	{
		from_params = InterpretNodeForImmediateUse(ocn[3]);
		node_stack.PushEvaluableNode(from_params);
		node_stack_needs_popping = true;
	}

	bool use_code = false;
	EvaluableNodeReference code_value;

	bool use_number = false;
	double number_value = 0;

	bool use_uint_number = false;
	uint64_t uint_number_value = 0;

	bool use_int_number = false;
	int64_t int_number_value = 0;

	bool use_string = false;
	std::string string_value = "";

	const std::string date_string("date:");

	if(from_type == GetStringIdFromNodeTypeFromString(ENT_NUMBER))
	{
		use_number = true;
		number_value = InterpretNodeIntoNumberValue(ocn[0]);
	}
	else if(from_type == ENBISI_code)
	{
		use_code = true;
		code_value = InterpretNodeForImmediateUse(ocn[0]);
	}
	else //base on string type
	{
		string_value = InterpretNodeIntoStringValueEmptyNull(ocn[0]);

		if(from_type == GetStringIdFromNodeTypeFromString(ENT_STRING))
		{
			use_string = true;
		}
		else if(from_type == ENBISI_Base16)
		{
			use_string = true;
			string_value = StringManipulation::Base16ToBinaryString(string_value);
		}
		else if(from_type == ENBISI_Base64)
		{
			use_string = true;
			string_value = StringManipulation::Base64ToBinaryString(string_value);
		}
		else if(from_type == ENBISI_uint8 || from_type == ENBISI_UINT8)
		{
			use_uint_number = true;
			uint_number_value = reinterpret_cast<uint8_t &>(string_value[0]);
		}
		else if(from_type == ENBISI_int8 || from_type == ENBISI_INT8)
		{
			use_int_number = true;
			int_number_value = ExpandCharStorage<int64_t, int8_t>(string_value[0]);
		}
		else if(from_type == ENBISI_uint16)
		{
			use_uint_number = true;
			if(string_value.size() >= 2)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[0]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8);
		}
		else if(from_type == ENBISI_UINT16)
		{
			use_uint_number = true;
			if(string_value.size() >= 2)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[1]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8);
		}
		else if(from_type == ENBISI_int16)
		{
			use_int_number = true;
			if(string_value.size() >= 2) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[0]) | (ExpandCharStorage<int64_t, int8_t>(string_value[1]) << 8);
		}
		else if(from_type == ENBISI_INT16)
		{
			use_int_number = true;
			if(string_value.size() >= 2) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[1]) | (ExpandCharStorage<int64_t, int8_t>(string_value[0]) << 8);
		}
		else if(from_type == ENBISI_uint32)
		{
			use_uint_number = true;
			if(string_value.size() >= 4)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[0]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8)
					| (ExpandCharStorage<uint64_t>(string_value[2]) << 16) | (ExpandCharStorage<uint64_t>(string_value[3]) << 24);
		}
		else if(from_type == ENBISI_UINT32)
		{
			use_uint_number = true;
			if(string_value.size() >= 4)
				uint_number_value = ExpandCharStorage<uint64_t>(string_value[3]) | (ExpandCharStorage<uint64_t>(string_value[2]) << 8)
					| (ExpandCharStorage<uint64_t>(string_value[1]) << 16) | (ExpandCharStorage<uint64_t>(string_value[0]) << 24);
		}
		else if(from_type == ENBISI_int32)
		{
			use_int_number = true;
			if(string_value.size() >= 4) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[0]) | (ExpandCharStorage<int64_t>(string_value[1]) << 8)
				| (ExpandCharStorage<int64_t>(string_value[2]) << 16) | (ExpandCharStorage<int64_t, int8_t>(string_value[3]) << 24);
		}
		else if(from_type == ENBISI_INT32)
		{
			use_int_number = true;
			if(string_value.size() >= 4) //sign extend the most significant byte
				int_number_value = ExpandCharStorage<int64_t>(string_value[3]) | (ExpandCharStorage<int64_t>(string_value[2]) << 8)
					| (ExpandCharStorage<int64_t>(string_value[1]) << 16) | (ExpandCharStorage<int64_t, int8_t>(string_value[0]) << 24);
		}
		else if(from_type == ENBISI_uint64)
		{
			use_uint_number = true;
			if(string_value.size() >= 8)
				uint_number_value =
					ExpandCharStorage<uint64_t>(string_value[0]) | (ExpandCharStorage<uint64_t>(string_value[1]) << 8)
					| (ExpandCharStorage<uint64_t>(string_value[2]) << 16) | (ExpandCharStorage<uint64_t>(string_value[3]) << 24)
					| (ExpandCharStorage<uint64_t>(string_value[4]) << 32) | (ExpandCharStorage<uint64_t>(string_value[5]) << 40)
					| (ExpandCharStorage<uint64_t>(string_value[6]) << 48) | (ExpandCharStorage<uint64_t>(string_value[7]) << 56);
		}
		else if(from_type == ENBISI_UINT64)
		{
			use_uint_number = true;
			if(string_value.size() >= 8)
				uint_number_value =
					ExpandCharStorage<uint64_t>(string_value[7]) | (ExpandCharStorage<uint64_t>(string_value[6]) << 8)
					| (ExpandCharStorage<uint64_t>(string_value[5]) << 16) | (ExpandCharStorage<uint64_t>(string_value[4]) << 24)
					| (ExpandCharStorage<uint64_t>(string_value[3]) << 32) | (ExpandCharStorage<uint64_t>(string_value[2]) << 40)
					| (ExpandCharStorage<uint64_t>(string_value[1]) << 48) | (ExpandCharStorage<uint64_t>(string_value[0]) << 56);
		}
		else if(from_type == ENBISI_int64)
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
		else if(from_type == ENBISI_INT64)
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
		else if(from_type == ENBISI_float)
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
		else if(from_type == ENBISI_FLOAT)
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
		else if(from_type == ENBISI_double)
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
		else if(from_type == ENBISI_DOUBLE)
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
		else if(from_type == ENBISI_json)
		{
			use_code = true;
			code_value = EvaluableNodeReference(EvaluableNodeJSONTranslation::JsonToEvaluableNode(evaluableNodeManager, string_value), true);
		}
		else if(from_type == ENBISI_yaml)
		{
			use_code = true;
			code_value = EvaluableNodeReference(EvaluableNodeYAMLTranslation::YamlToEvaluableNode(evaluableNodeManager, string_value), true);
		}
		else //need to parse the string
		{
			const auto &from_type_str = string_intern_pool.GetStringFromID(from_type);

			//see if it starts with the date string
			if(from_type_str.compare(0, date_string.size(), date_string) == 0)
			{
				std::string locale;
				std::string timezone;
				if(EvaluableNode::IsAssociativeArray(from_params))
				{
					auto &mcn = from_params->GetMappedChildNodesReference();

					auto found_locale = mcn.find(ENBISI_locale);
					if(found_locale != end(mcn))
						locale = EvaluableNode::ToString(found_locale->second);

					auto found_timezone = mcn.find(ENBISI_timezone);
					if(found_timezone != end(mcn))
						timezone = EvaluableNode::ToString(found_timezone->second);
				}

				use_number = true;
				number_value = GetNumSecondsSinceEpochFromDateTimeString(string_value, from_type_str.c_str() + date_string.size(), locale, timezone);
			}
		}
	}

	//have everything from from_type, so no longer need the reference
	if(node_stack_needs_popping)
		node_stack.PopEvaluableNode();
	string_intern_pool.DestroyStringReference(from_type);
	evaluableNodeManager->FreeNodeTreeIfPossible(from_params);

	EvaluableNodeReference to_params;
	if(ocn.size() > 4)
		to_params = InterpretNodeForImmediateUse(ocn[4]);

	//convert
	if(to_type == GetStringIdFromNodeTypeFromString(ENT_NUMBER))
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

		string_intern_pool.DestroyStringReference(to_type);
		evaluableNodeManager->FreeNodeTreeIfPossible(to_params);

		//didn't return code_value, so can free it
		evaluableNodeManager->FreeNodeTreeIfPossible(code_value);

		return EvaluableNodeReference(evaluableNodeManager->AllocNode(number_value), true);
	}
	else if(to_type == ENBISI_code)
	{
		string_intern_pool.DestroyStringReference(to_type);
		evaluableNodeManager->FreeNodeTreeIfPossible(to_params);
		return code_value;
	}
	else if(to_type == GetStringIdFromNodeTypeFromString(ENT_STRING))
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

				auto found_sort_keys = mcn.find(ENBISI_sort_keys);
				if(found_sort_keys != end(mcn))
					sort_keys = EvaluableNode::IsTrue(found_sort_keys->second);
			}

			string_value = Parser::Unparse(code_value, evaluableNodeManager, false, true, sort_keys);
		}
	}
	else if(to_type == ENBISI_Base16 || to_type == ENBISI_Base64)
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
			string_value = Parser::Unparse(code_value, evaluableNodeManager, false);

		if(to_type == ENBISI_Base16)
			string_value = StringManipulation::BinaryStringToBase16(string_value);
		else //Base64
			string_value = StringManipulation::BinaryStringToBase64(string_value);
	}
	else if(to_type == ENBISI_uint8 || to_type == ENBISI_UINT8)
	{
		if(use_number)				string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To1ByteString(static_cast<uint8_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_int8 || to_type == ENBISI_INT8)
	{
		if(use_number)				string_value = StringManipulation::To1ByteString(static_cast<int8_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To1ByteString(static_cast<int8_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To1ByteString(static_cast<int8_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To1ByteString(static_cast<int8_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_uint16)
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<uint16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_UINT16)
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringBigEndian(static_cast<uint16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_int16)
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringLittleEndian(static_cast<int16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_INT16)
	{
		if(use_number)				string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To2ByteStringBigEndian(static_cast<int16_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_uint32)
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<uint32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_UINT32)
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringBigEndian(static_cast<uint32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_int32)
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<int32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_INT32)
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringBigEndian(static_cast<int32_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_uint64)
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<uint64_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_UINT64)
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringBigEndian(static_cast<uint64_t>(EvaluableNode::ToNumber(code_value)));

	}
	else if(to_type == ENBISI_int64)
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<int64_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_INT64)
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringBigEndian(static_cast<int64_t>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_float)
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringLittleEndian(static_cast<float>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_FLOAT)
	{
		if(use_number)				string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To4ByteStringBigEndian(static_cast<float>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_double)
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringLittleEndian(static_cast<double>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_DOUBLE)
	{
		if(use_number)				string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(number_value));
		else if(use_uint_number)	string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(uint_number_value));
		else if(use_int_number)		string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(int_number_value));
		else if(use_code)			string_value = StringManipulation::To8ByteStringBigEndian(static_cast<double>(EvaluableNode::ToNumber(code_value)));
	}
	else if(to_type == ENBISI_json)
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
			string_value = EvaluableNodeJSONTranslation::EvaluableNodeToJson(&en_str);
		}
		else if(use_code)
		{
			bool sort_keys = false;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();

				auto found_sort_keys = mcn.find(ENBISI_sort_keys);
				if(found_sort_keys != end(mcn))
					sort_keys = EvaluableNode::IsTrue(found_sort_keys->second);
			}

			string_value = EvaluableNodeJSONTranslation::EvaluableNodeToJson(code_value, sort_keys);
		}
	}
	else if(to_type == ENBISI_yaml)
	{
		if(use_number)
		{
			EvaluableNode value(number_value);
			string_value = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&value);
		}
		else if(use_uint_number)
		{
			EvaluableNode value(static_cast<double>(uint_number_value));
			string_value = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&value);
		}
		else if(use_int_number)
		{
			EvaluableNode value(static_cast<double>(int_number_value));
			string_value = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&value);
		}
		else if(use_string)
		{
			EvaluableNode en_str(ENT_STRING, string_value);
			string_value = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(&en_str);
		}
		else if(use_code)
		{
			bool sort_keys = false;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();

				auto found_sort_keys = mcn.find(ENBISI_sort_keys);
				if(found_sort_keys != end(mcn))
					sort_keys = EvaluableNode::IsTrue(found_sort_keys->second);
			}

			string_value = EvaluableNodeYAMLTranslation::EvaluableNodeToYaml(code_value, sort_keys);
		}
	}
	else //need to parse the string
	{
		const auto &to_type_str = string_intern_pool.GetStringFromID(to_type);

		//if it starts with the date string
		if(to_type_str.compare(0, date_string.size(), date_string) == 0)
		{
			std::string locale;
			std::string timezone;
			if(EvaluableNode::IsAssociativeArray(to_params))
			{
				auto &mcn = to_params->GetMappedChildNodesReference();

				auto found_locale = mcn.find(ENBISI_locale);
				if(found_locale != end(mcn))
					locale = EvaluableNode::ToString(found_locale->second);

				auto found_timezone = mcn.find(ENBISI_timezone);
				if(found_timezone != end(mcn))
					timezone = EvaluableNode::ToString(found_timezone->second);
			}

			double num_secs_from_epoch = 0.0;
			if(use_number)				num_secs_from_epoch = number_value;
			else if(use_uint_number)	num_secs_from_epoch = static_cast<double>(uint_number_value);
			else if(use_int_number)		num_secs_from_epoch = static_cast<double>(int_number_value);
			else if(use_code)			num_secs_from_epoch = static_cast<double>(EvaluableNode::ToNumber(code_value));

			string_value = GetDateTimeStringFromNumSecondsSinceEpoch(num_secs_from_epoch, to_type_str.c_str() + date_string.size(), locale, timezone);
		}
	}

	string_intern_pool.DestroyStringReference(to_type);
	evaluableNodeManager->FreeNodeTreeIfPossible(to_params);

	//didn't return code_value, so can free it
	evaluableNodeManager->FreeNodeTreeIfPossible(code_value);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, string_value), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_LABELS(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference n = InterpretNodeForImmediateUse(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	size_t num_labels = n->GetNumLabels();

	//make list of labels
	EvaluableNodeReference result(evaluableNodeManager->AllocListNodeWithOrderedChildNodes(ENT_STRING, num_labels), true);
	auto &result_ocn = result->GetOrderedChildNodes();

	//because labels can be stored in different ways, it is just easiest to iterate
	// rather than to get a reference to each string id
	for(size_t i = 0; i < num_labels; i++)
		result_ocn[i]->SetStringID(n->GetLabelStringId(i));

	evaluableNodeManager->FreeNodeTreeIfPossible(n);
	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ALL_LABELS(EvaluableNode *en)
{
	EvaluableNodeReference n = EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() > 0)
		n = InterpretNodeForImmediateUse(ocn[0]);

	EvaluableNodeReference result(evaluableNodeManager->AllocNode(ENT_ASSOC), n.unique);

	auto label_sids_to_nodes = EvaluableNodeTreeManipulation::RetrieveLabelIndexesFromTree(n.reference);

	string_intern_pool.CreateStringReferences(label_sids_to_nodes, [](auto it) { return it.first; });
	result->ReserveMappedChildNodes(label_sids_to_nodes.size());
	for(auto &[node_id, node] : label_sids_to_nodes)
		result->SetMappedChildNodeWithReferenceHandoff(node_id, node);

	//can't guarantee there weren't any cycles if more than one label
	if(label_sids_to_nodes.size() > 1)
		result->SetNeedCycleCheck(true);

	return result;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_LABELS(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);

	if(!source.unique)
		source.reference = evaluableNodeManager->AllocNode(source);

	auto node_stack = CreateInterpreterNodeStackStateSaver(source);

	//get the labels
	auto labels_node = InterpretNodeForImmediateUse(ocn[1]);
	if(labels_node != nullptr && labels_node->GetType() != ENT_LIST)
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(labels_node);
		return source;
	}

	source->ClearLabels();

	//if adding labels, then grab from the provided list
	if(labels_node != nullptr)
	{
		for(auto &e : labels_node->GetOrderedChildNodes())
		{
			if(e != nullptr)
			{
				StringInternPool::StringID label_sid = EvaluableNode::ToStringIDWithReference(e);
				source->AppendLabelStringId(label_sid, true);
			}
		}
	}
	evaluableNodeManager->FreeNodeTreeIfPossible(labels_node);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ZIP_LABELS(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto label_list = InterpretNodeForImmediateUse(ocn[0]);
	auto node_stack = CreateInterpreterNodeStackStateSaver(label_list);

	auto source = InterpretNode(ocn[1]);

	//if no label list, or no source or source is immediate, then just return the source
	if(label_list == nullptr || !label_list->IsOrderedArray()
			|| source == nullptr || !source->IsOrderedArray())
		return source;

	node_stack.PopEvaluableNode();

	//make copy to populate with copies of the child nodes
	//start assuming that the copy will be unique, but set to not unique if any chance the assumption
	// might not hold
	EvaluableNodeReference retval = source;
	if(!source.unique)
		retval = EvaluableNodeReference(evaluableNodeManager->AllocNode(source), true);

	auto &label_list_ocn = label_list->GetOrderedChildNodesReference();

	//copy over labels, but keep track if all are unique
	auto &retval_ocn = retval->GetOrderedChildNodesReference();
	for(size_t i = 0; i < retval_ocn.size(); i++)
	{
		//no more labels to add, so just reuse the existing nodes
		if(i >= label_list_ocn.size())
		{
			retval.unique = false;
			break;
		}

		StringInternPool::StringID label_sid = EvaluableNode::ToStringIDWithReference(label_list_ocn[i]);

		EvaluableNode *cur_value = retval_ocn[i];
		if(!source.unique || cur_value == nullptr)
		{
			//make a copy of the node to set the label on
			if(cur_value == nullptr)
			{
				cur_value = evaluableNodeManager->AllocNode(ENT_NULL);
			}
			else
			{
				cur_value = evaluableNodeManager->AllocNode(cur_value);

				//if the node has child nodes, then can't guarantee uniqueness
				if(cur_value->GetNumChildNodes() > 0)
					retval.unique = false;
			}

			retval_ocn[i] = cur_value;
		}

		//if cur_value has appeared before as a child node, then it will have at least one other label
		//if it has a label, it could have been a previous child node, therefore can't guarantee uniqueness
		if(cur_value->GetNumLabels() > 0)
			retval.unique = false;

		cur_value->AppendLabelStringId(label_sid, true);
	}

	evaluableNodeManager->FreeNodeTreeIfPossible(label_list);

	//if all child nodes are unique, then it doesn't need a cycle check
	if(retval.unique)
		retval->SetNeedCycleCheck(false);

	return retval;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_COMMENTS(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	StringInternPool::StringID comments_sid = n->GetCommentsStringId();
	evaluableNodeManager->FreeNodeTreeIfPossible(n);

	if(comments_sid == StringInternPool::NOT_A_STRING_ID)
		return EvaluableNodeReference::Null();

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, comments_sid), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_COMMENTS(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);
	if(!source.unique)
		source.reference = evaluableNodeManager->AllocNode(source);

	auto node_stack = CreateInterpreterNodeStackStateSaver(source);

	//get the comments
	StringInternPool::StringID new_comments_sid = InterpretNodeIntoStringIDValueWithReference(ocn[1]);
	source->SetCommentsStringId(new_comments_sid, true);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_CONCURRENCY(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();
	auto n = InterpretNodeForImmediateUse(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(n->GetConcurrency() ? ENT_TRUE : ENT_FALSE), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_CONCURRENCY(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);
	else if(!source.unique)
		source.reference = evaluableNodeManager->AllocNode(source);

	auto node_stack = CreateInterpreterNodeStackStateSaver(source);

	//get the concurrent flag
	bool concurrency = InterpretNodeIntoBoolValue(ocn[1]);
	source->SetConcurrency(concurrency);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_VALUE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();
	auto n = InterpretNode(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	if(n.unique)
	{
		n->ClearMetadata();
	}
	else
	{
		n.reference = evaluableNodeManager->AllocNode(n, EvaluableNodeManager::ENMM_REMOVE_ALL);
		if(n->GetNumChildNodes() == 0)
			n.unique = true;
	}

	return n;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_VALUE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);
	if(!source.unique)
		source.reference = evaluableNodeManager->AllocNode(source);

	auto node_stack = CreateInterpreterNodeStackStateSaver(source);

	//get the new value
	auto value_node = InterpretNode(ocn[1]);
	source->CopyValueFrom(value_node);
	source.UpdatePropertiesBasedOnAttachedNode(value_node);

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_EXPLODE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto [valid, str] = InterpretNodeIntoStringValue(ocn[0]);
	if(!valid)
		return EvaluableNodeReference::Null();

	EvaluableNode *result = evaluableNodeManager->AllocNode(ENT_LIST);
	auto node_stack = CreateInterpreterNodeStackStateSaver(result);

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_SPLIT(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);
	auto node_stack = CreateInterpreterNodeStackStateSaver(retval);

	//if only one element, nothing to split on, just return the string in a list
	if(ocn.size() == 1)
	{
		EvaluableNode *str_node = InterpretNodeIntoUniqueStringIDValueEvaluableNode(ocn[0]);
		retval->AppendOrderedChildNode(str_node);
		return retval;
	}

	//have at least two parameters
	auto [valid_string_to_split, string_to_split] = InterpretNodeIntoStringValue(ocn[0]);
	if(!valid_string_to_split)
	{
		retval->SetType(ENT_STRING, evaluableNodeManager);
		retval->SetStringID(string_intern_pool.NOT_A_STRING_ID);
		return retval;
	}

	auto [valid_split_value, split_value] = InterpretNodeIntoStringValue(ocn[1]);
	if(!valid_split_value)
	{
		retval->SetType(ENT_STRING, evaluableNodeManager);
		retval->SetStringID(string_intern_pool.NOT_A_STRING_ID);
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_SUBSTR(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	//if only string as the parameter, just return a new copy of the string
	if(ocn.size() == 1)
	{
		return EvaluableNodeReference(evaluableNodeManager->AllocNodeWithReferenceHandoff(ENT_STRING,
			EvaluableNode::ToStringIDWithReference(ocn[0])), true);
	}

	//have at least 2 params
	auto [valid_string_to_substr, string_to_substr] = InterpretNodeIntoStringValue(ocn[0]);
	if(!valid_string_to_substr)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, string_intern_pool.NOT_A_STRING_ID), true);

	bool replace_string = false;
	std::string replacement_string;
	if(ocn.size() >= 4 && !EvaluableNode::IsNull(ocn[3]))
	{
		replace_string = true;
		auto [valid_replacement_string, temp_replacement_string] = InterpretNodeIntoStringValue(ocn[3]);
		//because otherwise previous line becomes clunky
		std::swap(replacement_string, temp_replacement_string);

		if(!valid_replacement_string)
			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, string_intern_pool.NOT_A_STRING_ID), true);
	}

	EvaluableNodeReference substr_node = InterpretNodeForImmediateUse(ocn[1]);
	if(EvaluableNode::IsNull(substr_node))
	{
		evaluableNodeManager->FreeNodeTreeIfPossible(substr_node);
		return EvaluableNodeReference::Null();
	}

	//if a number, then go by offset
	if(substr_node->IsNativelyNumeric())
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

			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, rebuilt_string), true);
		}
		else //return just the substring
		{
			std::string substr;
			if(start_offset < string_to_substr.size() && end_offset > start_offset)
				substr = string_to_substr.substr(start_offset, end_offset - start_offset);

			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, substr), true);
		}
	}
	else if(substr_node->GetType() == ENT_STRING)
	{
		//make a copy of the string so the node can be freed
		//(if this is a performance cost found in profiling, it can be fixed with more logic)
		std::string regex_str = substr_node->GetStringValue();
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
				return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, string_to_substr), true);
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

			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, updated_string), true);
		}
		else //finding matches
		{
			EvaluableNodeReference param_node;
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
					if(pnsid == ENBISI_all)
					{
						first_match_only = false;
						full_matches = true;
					}
					else if(pnsid == ENBISI_submatches)
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
					return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, string_intern_pool.NOT_A_STRING_ID), true);
				}

				std::sregex_token_iterator iter(begin(string_to_substr), end(string_to_substr), rx);
				std::sregex_token_iterator rx_end;
				if(iter == rx_end)
				{
					//not found
					return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, string_intern_pool.NOT_A_STRING_ID), true);
				}
				else
				{
					std::string value = *iter;
					return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, value), true);
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
			else if (submatches)
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

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONCAT(EvaluableNode *en)
{
	//build string from all child nodes
	auto &ocn = en->GetOrderedChildNodes();

	//if only one parameter is specified, do a fast shortcut
	if(ocn.size() == 1)
		return EvaluableNodeReference(InterpretNodeIntoUniqueStringIDValueEvaluableNode(ocn[0]), true);

	std::string s;
	for(auto &cn : ocn)
	{
		auto [valid, cur_string] = InterpretNodeIntoStringValue(cn);
		if(!valid)
			return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, string_intern_pool.NOT_A_STRING_ID), true);

		//want to exit early if out of resources because
		// this opcode can chew through memory with string concatenation via returned nulls
		if(AreExecutionResourcesExhausted())
			return EvaluableNodeReference::Null();

		//since UTF-8, don't need to do any conversions to concatenate
		s += cur_string;
	}

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, s), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CRYPTO_SIGN(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	std::string message = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string secret_key = InterpretNodeIntoStringValueEmptyNull(ocn[1]);

	std::string signature = SignMessage(message, secret_key);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, signature), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_CRYPTO_SIGN_VERIFY(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();
	if(ocn.size() < 3)
		return EvaluableNodeReference::Null();

	std::string message = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	std::string public_key = InterpretNodeIntoStringValueEmptyNull(ocn[1]);
	std::string signature = InterpretNodeIntoStringValueEmptyNull(ocn[2]);

	bool valid_sig = IsSignatureValid(message, public_key, signature);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(valid_sig ? ENT_TRUE : ENT_FALSE), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_ENCRYPT(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();
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
	if(key_2 == "")
		cyphertext = EncryptMessage(plaintext, key_1, nonce);
	else //use public key encryption
		cyphertext = EncryptMessage(plaintext, key_1, key_2, nonce);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, cyphertext), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_DECRYPT(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();
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
	if(key_2 == "")
		plaintext = DecryptMessage(cyphertext, key_1, nonce);
	else //use public key encryption
		plaintext = DecryptMessage(cyphertext, key_1, key_2, nonce);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_STRING, plaintext), true);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_PRINT(EvaluableNode *en)
{
	for(auto &cn : en->GetOrderedChildNodes())
	{
		auto cur = InterpretNodeForImmediateUse(cn);

		std::string s;
		if(cur == nullptr)
		{
			s = "(null)";
		}
		else if(IsEvaluableNodeTypeImmediate(cur->GetType()))
		{
			if(DoesEvaluableNodeTypeUseStringData(cur->GetType()))
				s = cur->GetStringValue();
			else if(DoesEvaluableNodeTypeUseNumberData(cur->GetType()))
				s = EvaluableNode::NumberToString(cur->GetNumberValueReference());
			else
				s = EvaluableNode::ToString(cur);
		}
		else
		{
			s = Parser::Unparse(cur, evaluableNodeManager, true, true, true);
		}

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

EvaluableNodeReference Interpreter::InterpretNode_ENT_TOTAL_SIZE(EvaluableNode *en)
{
	auto &ocn = en->GetOrderedChildNodes();

	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto cur = InterpretNodeForImmediateUse(ocn[0]);
	size_t total_size = EvaluableNode::GetDeepSize(cur);
	evaluableNodeManager->FreeNodeTreeIfPossible(cur);

	return EvaluableNodeReference(evaluableNodeManager->AllocNode(static_cast<double>(total_size)), true);
}