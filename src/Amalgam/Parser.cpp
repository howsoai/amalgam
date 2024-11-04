//project headers:
#include "Parser.h"

#include "EvaluableNode.h"
#include "EvaluableNodeTreeFunctions.h"
#include "StringManipulation.h"

//system headers:
#include <cctype>
#include <filesystem>

Parser::Parser()
{
	pos = 0;
	lineNumber = 0;
	lineStartPos = 0;
	numOpenParenthesis = 0;
	originalSource = "";
	topNode = nullptr;
	charOffsetStartOfLastCompletedCode = std::numeric_limits<size_t>::max();
}

Parser::Parser(std::string_view code_string, EvaluableNodeManager *enm,
	bool transactional_parse, std::string *original_source, bool debug_sources)
{
	code = code_string;
	pos = 0;
	lineNumber = 0;
	lineStartPos = 0;
	numOpenParenthesis = 0;

	if(original_source != nullptr)
	{
		//convert source to minimal absolute path
		std::filesystem::path p = *original_source;
		try
		{
			originalSource = std::filesystem::canonical(p).string();
		}
		catch(...)
		{
			//file doesn't exist, or was some other form of resource, just use original
			originalSource = *original_source;
		}
	}

	debugSources = debug_sources;
	topNode = nullptr;
	evaluableNodeManager = enm;
	transactionalParse = transactional_parse;
	charOffsetStartOfLastCompletedCode = std::numeric_limits<size_t>::max();
}

std::string Parser::Backslashify(const std::string &s)
{
	if(s.size() == 0)
		return std::string();

	//copy into string b, Backslashifying
	std::string b;
	//give it two extra characters, the worst highly likely case for needing backslashes (e.g., surrounded by quotes)
	b.reserve(s.size() + 2);
	for(auto c : s)
	{
		switch(c)
		{
		case '\0':
			b.append("\\0");
			break;
		case '\\':
			b.append("\\\\");
			break;
		case '"':
			b.append("\\\"");
			break;
		case '\t':
			b.append("\\t");
			break;
		case '\n':
			b.append("\\n");
			break;
		case '\r':
			b.append("\\r");
			break;
		default:
			b.push_back(c);
			break;
		}
	}

	return b;
}

std::tuple<EvaluableNodeReference, std::vector<std::string>, size_t>
	Parser::Parse(std::string_view code_string,
		EvaluableNodeManager *enm, bool transactional_parse,  std::string *original_source, bool debug_sources)
{
	Parser pt(code_string, enm, transactional_parse, original_source, debug_sources);

	pt.ParseCode();

	pt.PreevaluateNodes();

	return std::make_tuple(EvaluableNodeReference(pt.topNode, true),
		std::move(pt.warnings),
		pt.charOffsetStartOfLastCompletedCode);
}

std::tuple<EvaluableNodeReference, std::vector<std::string>, size_t> Parser::ParseFirstNode()
{
	EvaluableNode *n = GetNextToken(nullptr);

	return std::make_tuple(EvaluableNodeReference(n, true),
		std::move(warnings),
		charOffsetStartOfLastCompletedCode);
}

std::tuple<EvaluableNodeReference, std::vector<std::string>, size_t> Parser::ParseNextTransactionalBlock()
{
	topNode = nullptr;
	preevaluationNodes.clear();
	parentNodes.clear();

	ParseCode();

	PreevaluateNodes();

	return std::make_tuple(EvaluableNodeReference(topNode, true),
		std::move(warnings),
		charOffsetStartOfLastCompletedCode);
}

std::string Parser::Unparse(EvaluableNode *tree, EvaluableNodeManager *enm,
	bool expanded_whitespace, bool emit_attributes, bool sort_keys,
	bool first_of_transactional_unparse, size_t starting_indendation)
{
	UnparseData upd;
	upd.enm = enm;
	upd.topNodeIfTransactionUnparsing = (first_of_transactional_unparse ? tree : nullptr);
	//if the top node needs cycle checks, then need to check all nodes in case there are
	// multiple ways to get to one
	upd.cycleFree = (tree == nullptr || !tree->GetNeedCycleCheck());
	upd.preevaluationNeeded = false;
	upd.emitAttributes = emit_attributes;
	upd.sortKeys = sort_keys;
	Unparse(upd, tree, nullptr, expanded_whitespace, starting_indendation, false);
	return upd.result;
}

EvaluableNode *Parser::GetCodeForPathToSharedNodeFromParentAToParentB(UnparseData &upd,
	EvaluableNode *shared_node, EvaluableNode *a_parent, EvaluableNode *b_parent)
{
	if(shared_node == nullptr || a_parent == nullptr || b_parent == nullptr)
		return nullptr;

	//find all parent nodes of a to find collision with parent node of b, along with depth counts
	EvaluableNode::ReferenceCountType a_parent_nodes;
	size_t a_ancestor_depth = 1;
	while(a_parent != nullptr && a_parent_nodes.emplace(a_parent, a_ancestor_depth++).second == true)
		a_parent = upd.parentNodes[a_parent];

	//restart at a depth of 1 in case something goes wrong
	a_ancestor_depth = 1;
	//keep track of nodes visited to make sure there's no cycle
	EvaluableNode::ReferenceSetType b_nodes_visited;
	//ids to traverse along the path
	std::vector<EvaluableNode *> b_path_nodes;
	//the current node from path b
	EvaluableNode *b = shared_node;
	while(b_nodes_visited.insert(b_parent).second == true) //make sure not visited yet
	{
		//stop if found common parent node
		if(auto a_entry = a_parent_nodes.find(b); a_entry != end(a_parent_nodes))
		{
			a_ancestor_depth = a_entry->second;
			break;
		}

		//could not find a common ancestor, so error out
		if(b == nullptr)
			return nullptr;

		//each kind of child nodes
		if(b_parent->IsAssociativeArray())
		{
			StringInternPool::StringID key_id = StringInternPool::NOT_A_STRING_ID;
			auto &bp_mcn = b_parent->GetMappedChildNodesReference();
			if(!upd.sortKeys)
			{
				//look up which key corresponds to the value
				for(auto &[s_id, s] : bp_mcn)
				{
					if(s == b)
					{
						key_id = s_id;
						break;
					}
				}
			}
			else //sortKeys
			{
				std::vector<StringInternPool::StringID> key_sids;
				key_sids.reserve(bp_mcn.size());
				for(auto &[k_id, _] : bp_mcn)
					key_sids.push_back(k_id);

				std::sort(begin(key_sids), end(key_sids), StringIDNaturalCompareSort);

				for(auto &key_sid : key_sids)
				{
					auto k = bp_mcn.find(key_sid);
					if(k->second == b)
					{
						key_id = k->first;
						break;
					}
				}
			}

			b_path_nodes.insert(begin(b_path_nodes), upd.enm->AllocNode(ENT_STRING, key_id));
		}
		else if(b_parent->IsOrderedArray())
		{
			auto &bp_ocn = b_parent->GetOrderedChildNodesReference();
			const auto &found = std::find(begin(bp_ocn), end(bp_ocn), b);
			auto index = std::distance(begin(bp_ocn), found);
			b_path_nodes.insert(begin(b_path_nodes), upd.enm->AllocNode(static_cast<double>(index)));
		}
		else //didn't work... odd/error condition
		{
			return nullptr;
		}

		b = b_parent;
		b_parent = upd.parentNodes[b];
	}

	//build code to get the reference
	EvaluableNode *target = upd.enm->AllocNode(ENT_TARGET);
	//need to include the get (below) in the depth, so add 1
	target->AppendOrderedChildNode(upd.enm->AllocNode(static_cast<double>(a_ancestor_depth + 1)));

	EvaluableNode *indices = nullptr;
	if(b_path_nodes.size() == 0)
		return target;
	else if(b_path_nodes.size() == 1)
		indices = b_path_nodes[0];
	else
		indices = upd.enm->AllocNode(b_path_nodes, false, true);

	EvaluableNode *get = upd.enm->AllocNode(ENT_GET);
	get->AppendOrderedChildNode(target);
	get->AppendOrderedChildNode(indices);

	return get;
}

void Parser::SkipWhitespaceAndAccumulateAttributes(EvaluableNode *target)
{
	while(pos < code.size())
	{
		//eat any whitespace
		if(size_t space_size = StringManipulation::IsUtf8Whitespace(code, pos); space_size > 0)
		{
			if(StringManipulation::IsUtf8Newline(code, pos) > 0)
			{
				lineNumber++;
				lineStartPos = pos + space_size;
			}

			pos += space_size;
			continue;
		}

		auto cur_char = code[pos];

		//if it's a label, grab the label
		if(cur_char == '#')
		{
			pos++; //skip hash

			//add to labels list
			target->AppendLabel(GetNextIdentifier(true));

			continue;
		}

		//if it's a comment, grab everything until the end of line
		if(cur_char == ';')
		{
			pos++; //skip semicolon

			//add on characters until end of line
			size_t start_pos = pos;
			while(pos < code.size())
			{
				cur_char = code[pos];
				if(cur_char != '\r' && cur_char != '\n')
					pos++;
				else
					break;
			}

			std::string cur_comment;
			//prepend the comment with newlines if there is already a comment on the node
			if(target->GetCommentsStringId() != StringInternPool::NOT_A_STRING_ID)
				cur_comment = "\r\n";
			cur_comment.append(code.substr(start_pos, pos - start_pos));

			target->AppendComments(cur_comment);
			continue;
		}

		//if it's a concurrent marker, set the property
		if(cur_char == '|' && pos + 1 < code.size() && code[pos + 1] == '|')
		{
			pos += 2;	//skip ||
			target->SetConcurrency(true);
			continue;
		}

		if(cur_char == '@')
		{
			pos++;	//skip @
			preevaluationNodes.push_back(target);
			continue;
		}

		//not caught, so exit
		break;
	}

	//if labeling source, prepend as comment
	//add 1 to line and column to make them 1-based instead of 0 based
	if(debugSources)
	{
		std::string new_comment = sourceCommentPrefix;
		new_comment += StringManipulation::NumberToString(GetCurrentLineNumber());
		new_comment += ' ';

		size_t column_number = GetCurrentCharacterNumberInLine();
		new_comment += StringManipulation::NumberToString(column_number);
		new_comment += ' ';
		new_comment += originalSource;
		new_comment += "\r\n";
		if(target->HasComments())
			new_comment += target->GetCommentsString();
		target->SetComments(new_comment);
	}
}

std::string Parser::ParseString()
{
	pos++;

	std::string s;
	while(pos < code.size())
	{
		auto cur_char = code[pos];

		if(cur_char == '"')
			break;

		if(cur_char != '\\')
		{
			s.push_back(cur_char);
			pos++;
		}
		else //escaped character
		{
			pos++;
			if(pos < code.size())
			{
				cur_char = code[pos];
				switch(cur_char)
				{
				case '0':
					s.push_back('\0');
					break;
				case '"':
					s.push_back('"');
					break;
				case 't':
					s.push_back('\t');
					break;
				case 'n':
					s.push_back('\n');
					break;
				case 'r':
					s.push_back('\r');
					break;
				default:
					s.push_back(cur_char);
					break;
				}
			}
			pos++;
		}
	}

	pos++; //skip last double quote
	return s;
}

void Parser::SkipToEndOfIdentifier(bool allow_leading_label_marks)
{
	//eat any label marks
	if(allow_leading_label_marks)
	{
		while(pos < code.size() && code[pos] == '#')
			pos++;
	}

	//eat all characters until one that indicates end of identifier
	while(pos < code.size())
	{
		if(StringManipulation::IsUtf8Whitespace(code, pos))
			break;

		auto cur_char = code[pos];

		if(cur_char == '\\' && pos + 1 < code.size())
		{
			pos += 2;
			continue;
		}

		//check language characters
		if(cur_char == '#'
				|| cur_char == '(' || cur_char == ')'
				|| cur_char == '[' || cur_char == ']'
				|| cur_char == '{' || cur_char == '}'
				|| cur_char == ';')
			break;

		pos++;
	}
}

std::string Parser::GetNextIdentifier(bool allow_leading_label_marks)
{
	if(pos >= code.size())
		return std::string();

	//if quoted string, then go until the next end quote
	if(code[pos] == '"')
		return ParseString();
	else
	{
		size_t start_pos = pos;
		SkipToEndOfIdentifier(allow_leading_label_marks);
		return std::string(code.substr(start_pos, pos - start_pos));
	}
}

EvaluableNode *Parser::GetNextToken(EvaluableNode *parent_node, EvaluableNode *reuse_assoc_token_as_value)
{
	EvaluableNode *new_token = nullptr;
	bool parsing_assoc_key = false;

	if(reuse_assoc_token_as_value == nullptr)
	{
		new_token = evaluableNodeManager->AllocNode(ENT_NULL);
		//if parsing an assoc but haven't been passed a value to reuse, it's a key
		if(parent_node != nullptr && parent_node->IsAssociativeArray())
			parsing_assoc_key = true;
	}
	else
	{
		new_token = reuse_assoc_token_as_value;
	}

	SkipWhitespaceAndAccumulateAttributes(new_token);
	if(pos >= code.size())
	{
		FreeNode(new_token);
		return nullptr;
	}

	auto cur_char = code[pos];

	if(cur_char == '(' || cur_char == '[' || cur_char == '{') //identifier as command
	{
		pos++;
		numOpenParenthesis++;

		//only accumulate attributes for opcodes -- attributes for [ and { must be before the character
		if(cur_char == '(')
			SkipWhitespaceAndAccumulateAttributes(new_token);

		if(pos >= code.size())
		{
			FreeNode(new_token);
			return nullptr;
		}

		if(cur_char == '(')
		{
			std::string token = GetNextIdentifier();
			EvaluableNodeType token_type = GetEvaluableNodeTypeFromString(token);

			if(IsEvaluableNodeTypeValid(token_type) && !IsEvaluableNodeTypeImmediate(token_type))
			{
				new_token->SetType(token_type, evaluableNodeManager, false);
			}
			else
			{
				EmitWarning("Invalid opcode \"" + token + "\"; transforming to apply opcode using the invalid opcode type");

				new_token->SetType(ENT_APPLY, evaluableNodeManager, false);
				new_token->AppendOrderedChildNode(evaluableNodeManager->AllocNode(token));
			}
		}
		else if(cur_char == '[')
		{
			new_token->SetType(ENT_LIST, evaluableNodeManager, false);
		}
		else if(cur_char == '{')
		{
			new_token->SetType(ENT_ASSOC, evaluableNodeManager, false);
		}

		return new_token;
	}
	else if(cur_char == ')' || cur_char == ']' || cur_char == '}')
	{
		EvaluableNodeType parent_node_type = ENT_NULL;
		if(parent_node != nullptr)
			parent_node_type = parent_node->GetType();

		//make sure the closing character and type match
		if(cur_char == ']')
		{
			if(parent_node_type != ENT_LIST)
				EmitWarning("Mismatched ]");
		}
		else if(cur_char == '}')
		{
			if(parent_node_type != ENT_ASSOC)
				EmitWarning("Mismatched }");
		}

		pos++; //skip closing parenthesis
		numOpenParenthesis--;
		FreeNode(new_token);
		return nullptr;
	}
	else if(!parsing_assoc_key
		&& (StringManipulation::IsUtf8ArabicNumerals(cur_char) || cur_char == '-' || cur_char == '.'))
	{
		size_t start_pos = pos;
		SkipToEndOfIdentifier();
		std::string s(code.substr(start_pos, pos - start_pos));

		//check for special values
		double value = 0.0;
		if(s == ".infinity")
			value = std::numeric_limits<double>::infinity();
		else if(s == "-.infinity")
			value = -std::numeric_limits<double>::infinity();
		else
		{
			auto [converted_value, success] = Platform_StringToNumber(s);
			if(success)
				value = converted_value;
		}

		new_token->SetTypeViaNumberValue(value);
		return new_token;
	}
	else if(cur_char == '"')
	{
		new_token->SetType(ENT_STRING, evaluableNodeManager, false);
		new_token->SetStringValue(ParseString());
		return new_token;
	}
	else //identifier
	{
		//store the identifier
		new_token->SetType(ENT_SYMBOL, evaluableNodeManager, false);
		new_token->SetStringValue(GetNextIdentifier());
		return new_token;
	}
}

void Parser::FreeNode(EvaluableNode *node)
{
	evaluableNodeManager->FreeNode(node);
	if(preevaluationNodes.size() > 0 && preevaluationNodes.back() == node)
		preevaluationNodes.pop_back();
}

void Parser::ParseCode()
{
	EvaluableNode *cur_node = nullptr;

	//as long as code left
	while(pos < code.size())
	{
		//if at the top level node and starting to parse a new structure,
		//then all previous ones have completed and can mark this new position as a successful start
		if(topNode != nullptr && cur_node == topNode)
			charOffsetStartOfLastCompletedCode = pos;

		EvaluableNode *n = GetNextToken(cur_node);

		//if end of a list
		if(n == nullptr)
		{
			//nothing here at all
			if(cur_node == nullptr)
				break;

			const auto &parent = parentNodes.find(cur_node);

			//if no parent, then all finished
			if(parent == end(parentNodes) || parent->second == nullptr)
				break;

			//jump up to the parent node
			cur_node = parent->second;
			continue;
		}
		else //got some token
		{
			//if it's the first token, then put it up top
			if(topNode == nullptr)
			{
				topNode = n;
				cur_node = n;
				continue;
			}

			if(cur_node->IsOrderedArray())
			{
				cur_node->AppendOrderedChildNode(n);
			}
			else if(cur_node->IsAssociativeArray())
			{
				//if it's not an immediate value, then need to retrieve closing parenthesis
				if(!IsEvaluableNodeTypeImmediate(n->GetType()))
				{
					SkipWhitespaceAndAccumulateAttributes(n);
					if(pos <= code.size())
					{
						auto cur_char = code[pos];
						if(cur_char == ')')
						{
							pos++;
							numOpenParenthesis--;
						}
						else
						{
							EmitWarning("Missing )");
						}
					}
					else //no more code
					{
						break;
					}
				}

				//n is the id, so need to get the next token
				StringInternPool::StringID index_sid = EvaluableNode::ToStringIDTakingReferenceAndClearing(n);

				//reset the node type but continue to accumulate any attributes
				n->SetType(ENT_NULL, evaluableNodeManager, false);
				n = GetNextToken(cur_node, n);
				cur_node->SetMappedChildNodeWithReferenceHandoff(index_sid, n, true);

				//handle case if uneven number of arguments
				if(n == nullptr)
				{
					//nothing here at all
					if(cur_node == nullptr)
						break;

					const auto &parent = parentNodes.find(cur_node);

					//if no parent, then all finished
					if(parent == end(parentNodes) || parent->second == nullptr)
						break;

					//jump up to the parent node
					cur_node = parent->second;
					continue;
				}
			}

			parentNodes[n] = cur_node;

			//if it's not immediate, then descend into that part of the tree, resetting parent index counter
			if(!IsEvaluableNodeTypeImmediate(n->GetType()))
				cur_node = n;

			//if specifying something unusual, then assume it's just a null
			if(n->GetType() == ENT_NOT_A_BUILT_IN_TYPE)
			{
				n->SetType(ENT_NULL, nullptr, false);
				EmitWarning("Invalid opcode");
			}
		}

		if(transactionalParse && warnings.size() > 0 && cur_node == topNode)
			break;
	}

	int64_t num_allowed_open_parens = 0;
	if(transactionalParse)
	{
		num_allowed_open_parens = 1;

		//if anything went wrong with the last transaction, remove it
		if(warnings.size() > 0 || numOpenParenthesis > 1)
		{
			if(EvaluableNode::IsOrderedArray(topNode))
			{
				auto &top_node_ocn = topNode->GetOrderedChildNodesReference();
				top_node_ocn.pop_back();
			}
			else //nothing came through correctly
			{
				topNode = nullptr;
			}
		}
	}

	if(numOpenParenthesis > num_allowed_open_parens)
		EmitWarning(StringManipulation::NumberToString(
			static_cast<size_t>(numOpenParenthesis - num_allowed_open_parens)) + " missing closing parenthesis");
	else if(numOpenParenthesis < 0)
		EmitWarning(StringManipulation::NumberToString(static_cast<size_t>(-numOpenParenthesis))
			+ " extra closing parenthesis");
}

void Parser::AppendComments(EvaluableNode *n, size_t indentation_depth, bool pretty, std::string &to_append)
{
	const auto comment_lines = n->GetCommentsSeparateLines();

#ifdef DEBUG_PARSER_PRINT_FLAGS
	//prints out extra comments for debugging
	if(n->GetIsIdempotent() || n->GetNeedCycleCheck())
	{
		if(indentation_depth > 0 && pretty)
			AppendNewlineWithIndentation(to_append, indentation_depth, pretty);

		//add comment sign
		to_append.push_back(';');
		if(n->GetIsIdempotent())
			to_append.append("idempotent ");
		if(n->GetNeedCycleCheck())
			to_append.append("need_cycle_check ");

		if(pretty)
			AppendNewlineWithIndentation(to_append, indentation_depth, pretty);
		else //need to end a comment with a newline even if not pretty
			to_append.append("\r\n");
	}
#endif

	if(comment_lines.size() == 0)
		return;

	//if not start of file, make sure there's an extra newline before the comments
	if(indentation_depth > 0 && pretty)
		AppendNewlineWithIndentation(to_append, indentation_depth, pretty);

	for(auto &comment : comment_lines)
	{
		//add comment sign
		to_append.push_back(';');
		to_append.append(comment);

		if(pretty)
			AppendNewlineWithIndentation(to_append, indentation_depth, pretty);
		else //need to end a comment with a newline even if not pretty
			to_append.append("\r\n");
	}
}

//if the string contains a character that needs to be escaped for labels, then will convert
std::string ConvertLabelToQuotedStringIfNecessary(const std::string &s)
{
	if(s.empty())
		return s;

	bool needs_escape = Parser::HasCharactersBeyondIdentifier(s, true);

	if(!needs_escape)
	{
		//if the whole thing starts with #'s, then it's fine
		// but if it has #'s and then something else, then another #, then it needs to be escaped
		size_t last_hash_pos = s.find_last_of('#');
		if(last_hash_pos != std::string::npos)
		{
			//get all #'s at the front
			size_t num_starting_hashes = 0;
			while(s[num_starting_hashes] == '#')
				num_starting_hashes++;

			//if the position after the last starting hash is the same as the last hash, then don't transform the string
			if(num_starting_hashes - 1 != last_hash_pos)
				needs_escape = true;
		}
	}

	if(!needs_escape)
		return s;

	//need to quote and escape the string
	std::string result;
	result.push_back('"');

	if(Parser::NeedsBackslashify(s))
		result.append(Parser::Backslashify(s));
	else
		result.append(s);

	result.push_back('"');
	return result;
}

void Parser::AppendLabels(UnparseData &upd, EvaluableNode *n, size_t indentation_depth, bool pretty)
{
	size_t num_labels = n->GetNumLabels();
	for(size_t i = 0; i < num_labels; i++)
	{
		//add label sign
		upd.result.push_back('#');
		upd.result.append(ConvertLabelToQuotedStringIfNecessary(n->GetLabel(i)));

		//if not the last label, then separate via spaces
		if(i + 1 < num_labels || !pretty)
			upd.result.push_back(' ');
		else //last label and pretty printing
		{
			//if just an immediate or no child nodes, then separate with space
			if(IsEvaluableNodeTypeImmediate(n->GetType()) || n->GetNumChildNodes() == 0)
				upd.result.push_back(' ');
			else //something more elaborate, put newline and reindent
				AppendNewlineWithIndentation(upd.result, indentation_depth, pretty);
		}
	}
}

void Parser::AppendAssocKeyValuePair(UnparseData &upd, StringInternPool::StringID key_sid, EvaluableNode *n, EvaluableNode *parent,
	bool expanded_whitespace, size_t indentation_depth, bool need_initial_space)
{
	if(expanded_whitespace)
	{
		for(size_t i = 0; i < indentation_depth; i++)
			upd.result.push_back(indentationCharacter);
	}
	else if(need_initial_space)
	{
		upd.result.push_back(' ');
	}

	if(key_sid == string_intern_pool.NOT_A_STRING_ID)
	{
		upd.result.append("(null)");
	}
	else
	{
		auto &key_str = string_intern_pool.GetStringFromID(key_sid);

		//surround in quotes only if needed
		if(HasCharactersBeyondIdentifier(key_str))
		{
			upd.result.push_back('"');
			upd.result.append(Backslashify(key_str));
			upd.result.push_back('"');
		}
		else
		{
			upd.result.append(key_str);
		}
	}

	//space between key and value
	upd.result.push_back(' ');

	Unparse(upd, n, parent, expanded_whitespace, indentation_depth + 1, false);
}

void Parser::Unparse(UnparseData &upd, EvaluableNode *tree, EvaluableNode *parent, bool expanded_whitespace, size_t indentation_depth, bool need_initial_indent)
{
	//if need to check for circular references,
	// can skip if nullptr, as the code below this will handle nullptr and apply appropriate spacing
	if(!upd.cycleFree && tree != nullptr)
	{
		//keep track of what was visited
		auto [dest_node_with_parent, inserted] = upd.parentNodes.emplace(tree, parent);

		//if code already referenced, then print path to it
		if(!inserted)
		{
			upd.preevaluationNeeded = true;

			EvaluableNode *code_to_print = GetCodeForPathToSharedNodeFromParentAToParentB(upd,
				tree, parent, dest_node_with_parent->second);
			//unparse the path using a new set of parentNodes as to not pollute the one currently being unparsed
			EvaluableNode::ReferenceAssocType references;
			std::swap(upd.parentNodes, references);
			Unparse(upd, code_to_print, nullptr, expanded_whitespace, indentation_depth, need_initial_indent);
			std::swap(upd.parentNodes, references);	//put the old parentNodes back
			upd.enm->FreeNodeTree(code_to_print);

			return;
		}
	}

	//add indentation
	if(expanded_whitespace && need_initial_indent)
	{
		for(size_t i = 0; i < indentation_depth; i++)
			upd.result.push_back(indentationCharacter);
	}

	if(tree == nullptr)
	{
		upd.result.append(expanded_whitespace ? "(null)\r\n" : "(null)");
		return;
	}

	if(upd.emitAttributes)
	{
		AppendComments(tree, indentation_depth, expanded_whitespace, upd.result);
		AppendLabels(upd, tree, indentation_depth, expanded_whitespace);

		if(tree->GetConcurrency() == true)
			upd.result.append("||");

		//emit an @ to indicate that it needs to be translated into a map or is some other preevaluation
		if(upd.preevaluationNeeded)
		{
			upd.result.push_back('@');
			upd.preevaluationNeeded = false;
		}
	}

	//check if it's an immediate/variable before deciding whether to surround with parenthesis
	EvaluableNodeType tree_type = tree->GetType();
	if(IsEvaluableNodeTypeImmediate(tree_type))
	{
		switch(tree_type)
		{
		case ENT_NUMBER:
			upd.result.append(EvaluableNode::ToStringPreservingOpcodeType(tree));
			break;
		case ENT_STRING:
		{
			auto sid = tree->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID)
			{
				upd.result.append("(null)");
			}
			else //legitimate string
			{
				upd.result.push_back('"');

				auto &s = tree->GetStringValue();
				if(NeedsBackslashify(s))
					upd.result.append(Backslashify(s));
				else
					upd.result.append(s);

				upd.result.push_back('"');
			}
			break;
		}
		case ENT_SYMBOL:
			upd.result.append(tree->GetStringValue());
			break;
		default:
			break;
		}
		if(expanded_whitespace)
			upd.result.append("\r\n");
	}
	else
	{
		//emit opcode
		if(tree_type == ENT_LIST)
		{
			upd.result.push_back('[');
		}
		else if(tree_type == ENT_ASSOC)
		{
			upd.result.push_back('{');
		}
		else
		{
			upd.result.push_back('(');
			upd.result.append(GetStringFromEvaluableNodeType(tree_type));
		}

		//decide whether to expand whitespace of child nodes or write all on the same line
		bool recurse_expanded_whitespace = expanded_whitespace;
		if(expanded_whitespace)
		{
			//if small enough, just inline
			auto &ocn = tree->GetOrderedChildNodes();
			auto &mcn = tree->GetMappedChildNodes();

			//need to double count mapped child nodes because of keys
			size_t num_child_nodes = ocn.size() + 2 * mcn.size();
			if(num_child_nodes == 0)
			{
				recurse_expanded_whitespace = false;
			}
			else if(num_child_nodes <= 6 && num_child_nodes + indentation_depth <= 14)
			{
				//make sure all child nodes are leaf nodes and have no metadata
				bool all_leaf_nodes = true;
				for(auto cn : ocn)
				{
					if(cn != nullptr && (cn->GetNumChildNodes() > 0
						|| cn->GetCommentsStringId() != StringInternPool::NOT_A_STRING_ID || cn->GetNumLabels() > 0))
					{
						all_leaf_nodes = false;
						break;
					}
				}

				for(auto &[_, cn] : mcn)
				{
					//need to count the additional node for the string index
					if(cn != nullptr && (cn->GetNumChildNodes() > 0
						|| cn->GetCommentsStringId() != StringInternPool::NOT_A_STRING_ID || cn->GetNumLabels() > 0))
					{
						all_leaf_nodes = false;
						break;
					}
				}

				if(all_leaf_nodes)
					recurse_expanded_whitespace = false;
			}

			//if expanding out further, add extra whitespace
			if(recurse_expanded_whitespace)
				upd.result.append("\r\n");
		}

		if(tree->IsAssociativeArray())
		{
			auto &tree_mcn = tree->GetMappedChildNodesReference();
			bool initial_space = (tree_type != ENT_LIST && tree_type != ENT_ASSOC);
			if(!upd.sortKeys)
			{
				for(auto &[k_id, k] : tree_mcn)
				{
					AppendAssocKeyValuePair(upd, k_id, k, tree, recurse_expanded_whitespace, indentation_depth + 1, initial_space);
					initial_space = true;
				}
			}
			else //sortKeys
			{
				std::vector<StringInternPool::StringID> key_sids;
				key_sids.reserve(tree_mcn.size());
				for(auto &[k_id, _] : tree_mcn)
					key_sids.push_back(k_id);

				std::sort(begin(key_sids), end(key_sids), StringIDNaturalCompareSort);

				for(auto &key_sid : key_sids)
				{
					auto k = tree_mcn.find(key_sid);
					AppendAssocKeyValuePair(upd, k->first, k->second, tree, recurse_expanded_whitespace, indentation_depth + 1, initial_space);
					initial_space = true;
				}
			}
		}
		else if(tree->IsOrderedArray())
		{
			auto &tree_ocn = tree->GetOrderedChildNodesReference();
			if(recurse_expanded_whitespace)
			{
				for(auto &e : tree_ocn)
					Unparse(upd, e, tree, true, indentation_depth + 1, true);
			}
			else //expanded whitespace
			{
				for(size_t i = 0; i < tree_ocn.size(); i++)
				{
					//if not the first or if it's not a type with a special delimeter, insert a space
					if(i > 0 || (tree_type != ENT_LIST && tree_type != ENT_ASSOC))
						upd.result.push_back(' ');
					Unparse(upd, tree_ocn[i], tree, false, indentation_depth + 1, true);
				}
			}
		}

		if(tree != upd.topNodeIfTransactionUnparsing)
		{
			//add closing parenthesis
			if(expanded_whitespace)
			{
				//indent if appropriate
				if(recurse_expanded_whitespace)
				{
					for(size_t i = 0; i < indentation_depth; i++)
						upd.result.push_back(indentationCharacter);
				}

				if(tree_type == ENT_LIST)
					upd.result.push_back(']');
				else if(tree_type == ENT_ASSOC)
					upd.result.push_back('}');
				else
					upd.result.push_back(')');

				upd.result.push_back('\r');
				upd.result.push_back('\n');
			}
			else
			{
				if(tree_type == ENT_LIST)
					upd.result.push_back(']');
				else if(tree_type == ENT_ASSOC)
					upd.result.push_back('}');
				else
					upd.result.push_back(')');
			}
		}
		else //end of opening transactional; emit a space to ensure things don't get improperly joined
		{
			upd.result.push_back(' ');
		}
	}
}

//given a node, traverses the node via index and returns that child, nullptr if invalid
static EvaluableNode *GetNodeRelativeToIndex(EvaluableNode *node, EvaluableNode *index_node)
{
	if(node == nullptr)
		return nullptr;

	//if it's an assoc, then treat the index as a string
	if(node->IsAssociativeArray())
	{
		StringInternPool::StringID index_sid = EvaluableNode::ToStringIDIfExists(index_node);
		EvaluableNode **found = node->GetMappedChildNode(index_sid);
		if(found != nullptr)
			return *found;
		return nullptr;
	}

	//otherwise treat the index as a number for a list
	size_t index = static_cast<size_t>(EvaluableNode::ToNumber(index_node));
	if(index < node->GetOrderedChildNodes().size())
		return node->GetOrderedChildNodesReference()[index];

	//didn't find anything
	return nullptr;
}

EvaluableNode *Parser::GetNodeFromRelativeCodePath(EvaluableNode *path)
{
	if(path == nullptr)
		return nullptr;

	//traverse based on type
	switch(path->GetType())
	{

	case ENT_GET:
	{
		if(path->GetOrderedChildNodes().size() < 2)
			return nullptr;

		EvaluableNode *result = GetNodeFromRelativeCodePath(path->GetOrderedChildNodes()[0]);
		if(result == nullptr)
			return result;
		EvaluableNode *index_node = path->GetOrderedChildNodes()[1];
		if(index_node == nullptr)
			return nullptr;

		if(index_node->IsOrderedArray())
		{
			//travers the nodes over each index to find the location
			auto &index_ocn = index_node->GetOrderedChildNodesReference();
			for(auto &index_node_element : index_ocn)
			{
				result = GetNodeRelativeToIndex(result, index_node_element);
				if(result == nullptr)
					break;
			}

			return result;
		}
		else //immediate
		{
			return GetNodeRelativeToIndex(result, index_node);
		}

		return nullptr;
	}

	case ENT_TARGET:
	{
		//first parameter is the number of steps to crawl up in the parent tree
		size_t target_steps_up = 1;
		if(path->GetOrderedChildNodes().size() > 0)
		{
			double step_value = EvaluableNode::ToNumber(path->GetOrderedChildNodes()[0]);

			//zero is not allowed here because that means it would attempt to replace itself with itself
			//within the data -- in actual runtime, 0 is allowed for target because other things can point to it,
			//but not during parsing
			if(step_value >= 1)
				target_steps_up = static_cast<size_t>(step_value);
			else
				return nullptr;
		}

		//crawl up parse tree
		EvaluableNode *result = path;
		for(size_t i = 0; i < target_steps_up && result != nullptr; i++)
		{
			auto found = parentNodes.find(result);
			if(found != end(parentNodes))
				result = found->second;
			else
				result = nullptr;
		}

		return result;
	}

	default:
		return nullptr;
	}

	return nullptr;
}

void Parser::PreevaluateNodes()
{
	//only need to update flags if any nodes actually change
	bool any_nodes_changed = false;
	for(auto &n : preevaluationNodes)
	{
		if(n == nullptr)
			continue;

		auto node_type = n->GetType();
		if(node_type != ENT_GET && node_type != ENT_TARGET)
			continue;

		EvaluableNode *target = GetNodeFromRelativeCodePath(n);

		//find the node's parent in order to set it to target
		EvaluableNode *parent = nullptr;
		parent = parentNodes[n];
		if(parent == nullptr)
			continue;

		//copy reference of target to the parent's index of the target
		if(parent->IsAssociativeArray())
		{
			for(auto &[_, cn] : parent->GetMappedChildNodesReference())
			{
				if(cn == n)
				{
					cn = target;
					any_nodes_changed = true;
					break;
				}
			}
		}
		else if(parent->IsOrderedArray())
		{
			for(auto &cn : parent->GetOrderedChildNodesReference())
			{
				if(cn == n)
				{
					cn = target;
					any_nodes_changed = true;
					break;
				}
			}
		}
	}

	if(any_nodes_changed)
	{
		EvaluableNodeManager::UpdateFlagsForNodeTree(topNode);
	}
	else
	{
		if(topNode != nullptr)
			EvaluableNodeManager::UpdateIdempotencyFlagsForNonCyclicNodeTree(topNode);
	}
}
