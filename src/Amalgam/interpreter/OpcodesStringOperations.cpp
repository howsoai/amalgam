//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"

//system headers:
#include <regex>

static std::string _opcode_group = "String Operations";

static OpcodeInitializer _ENT_EXPLODE(ENT_EXPLODE, &Interpreter::InterpretNode_ENT_EXPLODE, []() {
	OpcodeDetails d;
	d.parameters = R"(string str [number stride])";
	d.returns = R"(list of string)";
	d.description = R"(Explodes `str` into the pieces that make it up.  If `stride` is zero or unspecified, then it explodes `str` by character per UTF-8 parsing.  If `stride` is specified, then it breaks it into chunks of that many bytes.  For example, a `stride` of 1 would break it into bytes, whereas a `stride` of 4 would break it into 32-bit chunks.)";
	d.examples = MakeAmalgamExamples({
		{R"&((explode "abcdefghi"))&", R"([
	"a"
	"b"
	"c"
	"d"
	"e"
	"f"
	"g"
	"h"
	"i"
])"},
			{R"&((explode "abcdefghi" 1))&", R"([
	"a"
	"b"
	"c"
	"d"
	"e"
	"f"
	"g"
	"h"
	"i"
])"},
			{R"&((explode "abcdefghi" 2))&", R"(["ab" "cd" "ef" "gh" "i"])"},
			{R"&((explode "abcdefghi" 3))&", R"(["abc" "def" "ghi"])"},
			{R"&((explode "abcdefghi" 4))&", R"(["abcd" "efgh" "i"])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

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

static OpcodeInitializer _ENT_SPLIT(ENT_SPLIT, &Interpreter::InterpretNode_ENT_SPLIT, []() {
	OpcodeDetails d;
	d.parameters = R"(string str [string split_string] [number max_split_count] [number stride])";
	d.returns = R"(list of string)";
	d.description = R"(Splits `str` into a list of strings based on `split_string`, which is handled as a regular expression.  Any data matching `split_string` will not be included in any of the resulting strings.  If `max_split_count` is provided and greater than zero, it will only split up to that many times.  If `stride` is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If `stride` is specified and a value other than zero, then it does not use `split_string` as a regular expression but rather a string, and it breaks the result into chunks of that many bytes.  For example, a `stride` of 1 would break it into bytes, whereas a `stride` of 4 would break it into 32-bit chunks.)";
	d.examples = MakeAmalgamExamples({
		{R"&((split "hello world"))&", R"(["hello world"])"},
		{R"&((split "hello world" " "))&", R"(["hello" "world"])"},
		{R"&((split "hello\r\nworld\r\n!" "\r\n"))&", R"(["hello" "world" "!"])"},
		{R"&((split "hello world !" "\\s" 1))&", R"(["hello" "world !"])"},
		{R"&((split "hello to the world" "to" .null 2))&", R"(["hello " " the world"])"},
		{R"&((split "abcdefgij"))&", R"(["abcdefgij"])"},
		{R"&((split "abc de fghij" " "))&", R"(["abc" "de" "fghij"])"},
		{R"&((split "abc\r\nde\r\nfghij" "\r\n"))&", R"(["abc" "de" "fghij"])"},
		{R"&((split "abc de fghij" " " 1))&", R"(["abc" "de fghij"])"},
		{R"&((split "abc de fghij" " de " .null 4))&", R"(["abc de fghij"])"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

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
		try
		{
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

static OpcodeInitializer _ENT_SUBSTR(ENT_SUBSTR, &Interpreter::InterpretNode_ENT_SUBSTR, []() {
	OpcodeDetails d;
	d.parameters = R"(string str [number|string location] [number|string param] [string replacement] [number stride])";
	d.returns = R"(string | list of string | list of list of string)";
	d.description = R"(Finds a substring `str`.  If `location` is a number, then evaluates to a new string representing the substring starting at the offset specified by `location`.  If `location` is a string, then it will treat `location` as a regular expression.  If `param` is specified, then it may change the interpretation of `location`.  If `param` is specified and `location` is a number it will go until that length beyond the offset specified by `location`.  If `param` is specified and `location` is a regular expression, `param` will represent one of the following: if null or "first", then it will return the first match of the regular expression; or if `param` is a number or the string "all", then substr will evaluate to a list of up to param matches (which may be infinite yielding the same result as "all").  If `param` is a negative number or the string "submatches", then it will return a list of list of strings, for each match up to the count of the negative number or all matches.  If `param` is "submatches", each inner list will represent the full regular expression match followed by each submatch as captured by parenthesis in the regular expression, ordered from an outer to inner, left-to-right manner.  If `location` is a negative number, then it will measure from the end of the string rather than the beginning.  If `replacement` is specified and not null, it will return the original string rather than the substring, but the substring will be replaced by replacement regardless of what `location` is.  And if replacement is specified, then it will override some of the logic for the `param` type and always return just a string and not a list.  If `stride` is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If `stride` is specified, then it breaks it into chunks of that many bytes.  For example, a `stride` of 1 would break it into bytes, whereas a `stride` of 4 would break it into 32-bit chunks.)";
	d.examples = MakeAmalgamExamples({
		{R"&((substr "hello world"))&", R"("hello world")"},
		{R"&((substr "hello world" 1))&", R"("ello world")"},
		{R"&((substr "hello world" 1 8))&", R"("ello wo")"},
		{R"&((substr "hello world" 1 100))&", R"("ello world")"},
		{R"&((substr "hello world" 1 -1))&", R"("ello worl")"},
		{R"&((substr "hello world" -4 -1))&", R"("orl")"},
		{R"&((substr "hello world" -4 -1 .null 1))&", R"("orl")"},
		{R"&((substr "hello world" 1 3 "x"))&", R"("hxlo world")"},
		{R"&((substr "hello world" "(e|o)"))&", R"("e")"},
		{R"&((substr "hello world" "[h|w](e|o)"))&", R"("he")"},
		{R"&((substr "hello world" "[h|w](e|o)" 1))&", R"(["he"])"},
		{R"&((substr "hello world" "[h|w](e|o)" "all"))&", R"(["he" "wo"])"},
		{R"&((substr "hello world" "(([h|w])(e|o))" "all"))&", R"(["he" "wo"])"},
		{R"&((substr "hello world" "[h|w](e|o)" -1))&", R"([
	["he" "e"]
])"},
			{R"&((substr "hello world" "[h|w](e|o)" "submatches"))&", R"([
	["he" "e"]
	["wo" "o"]
])"},
			{R"&((substr "hello world" "(([h|w])(e|o))" "submatches"))&", R"([
	["he" "he" "h" "e"]
	["wo" "wo" "w" "o"]
])"},
			{R"&((substr "hello world" "(?:([h|w])(?:e|o))" "submatches"))&", R"([
	["he" "h"]
	["wo" "w"]
])"},
			{R"&(;invalid syntax test
(substr "hello world" "(?([h|w])(?:e|o))" "submatches"))&", R"([])"},
			{R"&((substr "hello world" "(e|o)" .null "[$&]"))&", R"("h[e]ll[o] w[o]rld")"},
			{R"&((substr "hello world" "(e|o)" 2 "[$&]"))&", R"("h[e]ll[o] world")"},
			{R"&((substr "abcdefgijk"))&", R"("abcdefgijk")"},
			{R"&((substr "abcdefgijk" 1))&", R"("bcdefgijk")"},
			{R"&((substr "abcdefgijk" 1 8))&", R"("bcdefgi")"},
			{R"&((substr "abcdefgijk" 1 100))&", R"("bcdefgijk")"},
			{R"&((substr "abcdefgijk" 1 -1))&", R"("bcdefgij")"},
			{R"&((substr "abcdefgijk" -4 -1))&", R"("gij")"},
			{R"&((substr "abcdefgijk" -4 -1 .null 1))&", R"("gij")"},
			{R"&((substr "abcdefgijk" 1 3 "x"))&", R"("axdefgijk")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

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
			try
			{
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
				try
				{
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
				try
				{
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
				try
				{
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

static OpcodeInitializer _ENT_CONCAT(ENT_CONCAT, &Interpreter::InterpretNode_ENT_CONCAT, []() {
	OpcodeDetails d;
	d.parameters = R"([string str1] [string str2] ... [string strN])";
	d.returns = R"(string)";
	d.description = R"(Concatenates all strings and evaluates to the single resulting string.)";
	d.examples = MakeAmalgamExamples({
		{R"&((concat "hello" " " "world"))&", R"("hello world")"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 10.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

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
				|| (interpreterConstraints != nullptr && s.size() > interpreterConstraints->maxNumAllocatedNodes))
			return EvaluableNodeReference::Null();

		//since UTF-8, don't need to do any conversions to concatenate
		s += cur_string;
	}

	return AllocReturn(s, immediate_result);
}

static OpcodeInitializer _ENT_PARSE(ENT_PARSE, &Interpreter::InterpretNode_ENT_PARSE, []() {
	OpcodeDetails d;
	d.parameters = R"(string str [bool transactional] [bool return_warnings])";
	d.returns = R"(any)";
	d.description = R"(String `str` is parsed into code, and the result is returned.  If `transactional` is false, the default, it will attempt to parse the whole string and will return the closest code possible if there are any parse issues.  If `transactional` is true, it will parse the string transactionally, meaning that any node that has a parse error or is incomplete will be omitted along with all child nodes except for the top node.  If any performance constraints are given or `return_warnings` is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is an assoc mapping all warnings to their number of occurrences, and perf_constraint violation is a string denoting the constraint exceeded (or .null if none)), unless `return_warnings` is false, in which case just the value will be returned.)";
	d.examples = MakeAmalgamExamples({
		{R"&((parse "(seq (+ 1 2))" .true)))&", R"&((seq
	(+ 1 2)
))&"},
			{R"&((parse "(seq (+ 1 2) (+ " .true)))&", R"&((seq
	(+ 1 2)
))&"},
			{R"&((parse "(seq (+ 1 2) (+ " .false .true))")&", R"([
	(seq
		(+ 1 2)
		(+)
	)
	["Warning: 2 missing closing parenthesis at line 1, column 17"]
])"},
			{R"&((parse "(seq (+ 1 2) (+ " .true .true))")&", R"([
	(seq
		(+ 1 2)
	)
	["Warning: 1 missing closing parenthesis at line 1, column 17"]
])"},
			{R"&((parse "(seq (+ 1 2) (+ (a ) 3) " .true .true))")&", R"([
	(seq
		(+ 1 2)
	)
	["Warning: Invalid opcode \"a\"; transforming to apply opcode using the invalid opcode type at line 1, column 19"]
])"},
			{R"&((parse "(6)")))&", R"((apply "6"))"},
			{R"&((parse "(not_an_opcode)")))&", R"((apply "not_an_opcode"))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_PARSE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
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

	auto [node, warnings, char_with_error, code_complete]
		= Parser::Parse(to_parse, evaluableNodeManager, transactional_parse);

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

static OpcodeInitializer _ENT_UNPARSE(ENT_UNPARSE, &Interpreter::InterpretNode_ENT_UNPARSE, []() {
	OpcodeDetails d;
	d.parameters = R"(code c [bool pretty_print] [bool sort_keys] [bool include_attributes])";
	d.returns = R"(string)";
	d.description = R"(Code is unparsed and the representative string is returned. If `pretty_print` is true, the output will be in pretty-print format, otherwise by default it will be inlined.  If `sort_keys` is true, the default, then it will print assoc structures and anything that could come in different orders in a natural sorted order by key, otherwise it will default to whatever order it is stored in memory.  If `include_attributes` is true, it will print out attributes like comments, but by default it will not.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unparse (parse "(print \"hello\")")))&", R"&("(print \"hello\")")&"},
		{R"&((parse (unparse (list (sqrt -1) .null .infinity -.infinity))))&", R"&([.null .null .infinity -.infinity])&"},
		{R"&((unparse (associate "a" 1 "b" 2 "c" (list "alpha" "beta" "gamma"))))&", R"&("{a 1 b 2 c [\"alpha\" \"beta\" \"gamma\"]}")&"},
		{R"&((unparse (associate "a" 1 "b" 2 "c" (list "alpha" "beta" "gamma")) .true))&", R"&("{\r\n\ta 1\r\n\tb 2\r\n\tc [\"alpha\" \"beta\" \"gamma\"]\r\n}\r\n")&"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNPARSE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	bool pretty = false;
	if(ocn.size() > 1)
		pretty = InterpretNodeIntoBoolValue(ocn[1]);

	bool deterministic_order = true;
	if(ocn.size() > 2)
		deterministic_order = InterpretNodeIntoBoolValue(ocn[2], true);

	bool include_attributes = false;
	if(ocn.size() > 3)
		include_attributes = InterpretNodeIntoBoolValue(ocn[3]);

	size_t max_length = std::numeric_limits<size_t>::max();
	if(interpreterConstraints != nullptr)
		max_length = interpreterConstraints->maxNumAllocatedNodes;

	auto tree = InterpretNodeForImmediateUse(ocn[0]);
	std::string s = Parser::Unparse(tree, pretty, include_attributes, deterministic_order, false, false, max_length);
	evaluableNodeManager->FreeNodeTreeIfPossible(tree);

	return AllocReturn(s, immediate_result);
}
