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

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ANNOTATIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	std::string annotations(n->GetAnnotationsString());
	evaluableNodeManager->FreeNodeTreeIfPossible(n);
	return AllocReturn(annotations, immediate_result);
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ANNOTATIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the annotations
	auto [valid, new_annotations] = InterpretNodeIntoStringValue(ocn[1]);
	if(valid)
		source->SetAnnotationsString(new_annotations);
	else
		source->ClearAnnotations();

	return source;
}

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_COMMENTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	std::string comment(n->GetCommentsString());
	evaluableNodeManager->FreeNodeTreeIfPossible(n);
	return AllocReturn(comment, immediate_result);
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
	auto [valid, new_comments] = InterpretNodeIntoStringValue(ocn[1]);
	if(valid)
		source->SetCommentsString(new_comments);
	else
		source->ClearComments();

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
		evaluableNodeManager->EnsureNodeIsModifiable(n, false, false);

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
				auto last_iter(iter);

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
