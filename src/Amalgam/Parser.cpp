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

EvaluableNodeReference Parser::Parse(std::string &code_string, EvaluableNodeManager *enm, std::string *original_source)
{
	Parser pt;
	pt.code = &code_string;
	pt.pos = 0;
	pt.preevaluationNodes.clear();
	pt.evaluableNodeManager = enm;

	pt.originalSource = "";
	if(original_source != nullptr)
	{
		//convert source to minimal absolute path
		std::filesystem::path p = *original_source;
		try
		{
			pt.originalSource = std::filesystem::canonical(p).string();
		}
		catch(std::filesystem::filesystem_error &e)
		{
			//file doesn't exist
			pt.originalSource = e.what();
		}
	}

	EvaluableNode *parse_tree = pt.ParseNextBlock();

	pt.PreevaluateNodes();
	EvaluableNodeManager::UpdateFlagsForNodeTree(parse_tree);

	return EvaluableNodeReference(parse_tree, true);
}

std::string Parser::Unparse(EvaluableNode *tree, EvaluableNodeManager *enm,
	bool expanded_whitespace, bool emit_attributes, bool sort_keys)
{
	UnparseData upd;
	upd.enm = enm;
	//if the top node needs cycle checks, then need to check all nodes in case there are
	// multiple ways to get to one
	upd.cycleFree = (tree == nullptr || !tree->GetNeedCycleCheck());
	upd.preevaluationNeeded = false;
	upd.emitAttributes = emit_attributes;
	upd.sortKeys = sort_keys;
	Unparse(upd, tree, nullptr, expanded_whitespace, 0, false);
	return upd.result;
}

EvaluableNode *Parser::GetCodeForPathFromAToB(UnparseData &upd, EvaluableNode *a, EvaluableNode *b)
{
	if(a == nullptr || b == nullptr)
		return nullptr;

	//climb back from current tree to top level parent; get ancestor (for comparison with b's) and how far back it is
	EvaluableNode *a_ancestor = a;
	EvaluableNode *a_ancestor_parent = upd.parentNodes[a_ancestor];
	int64_t a_ancestor_depth = 0;
	EvaluableNode::ReferenceSetType nodes_visited;
	while(a_ancestor_parent != nullptr
		&& a_ancestor != b					//stop if it's the target
		&& nodes_visited.insert(a_ancestor_parent).second == true) //make sure not visited yet
	{
		//climb back up one level
		a_ancestor_depth++;
		a_ancestor = a_ancestor_parent;
		a_ancestor_parent = upd.parentNodes[a_ancestor];
	}

	//find way from b (as previously defined) back to ancestor
	EvaluableNode *b_ancestor = b;
	EvaluableNode *b_ancestor_parent = upd.parentNodes[b_ancestor];
	nodes_visited.clear();
	std::vector<EvaluableNode *> b_path_nodes;
	while(b_ancestor_parent != nullptr
		&& b_ancestor != a_ancestor					//stop if it's the target
		&& nodes_visited.insert(b_ancestor_parent).second == true) //make sure not visited yet
	{

		EvaluableNode *lookup = upd.enm->AllocNode(ENT_GET);
		lookup->AppendOrderedChildNode(nullptr); //placeholder for the object to use when assembling chain later

		//each kind of child nodes
		if(b_ancestor_parent->IsAssociativeArray())
		{
			StringInternPool::StringID key_id = StringInternPool::NOT_A_STRING_ID;
			auto &bap_mcn = b_ancestor_parent->GetMappedChildNodesReference();
			if(!upd.sortKeys)
			{
				//look up which key corresponds to the value
				for(auto &[s_id, s] : b_ancestor_parent->GetMappedChildNodesReference())
				{
					if(s == b_ancestor)
					{
						key_id = s_id;
						break;
					}
				}
			}
			else //sortKeys
			{
				std::vector<StringInternPool::StringID> key_sids;
				key_sids.reserve(bap_mcn.size());
				for(auto &[k_id, _] : bap_mcn)
					key_sids.push_back(k_id);

				std::sort(begin(key_sids), end(key_sids), StringIDNaturalCompareSort);

				for(auto &key_sid : key_sids)
				{
					auto k = bap_mcn.find(key_sid);
					if(k->second == b_ancestor)
					{
						key_id = k->first;
						break;
					}
				}
			}

			lookup->AppendOrderedChildNode(upd.enm->AllocNode(ENT_STRING, key_id));
		}
		else if(b_ancestor_parent->IsOrderedArray())
		{
			auto &bap_ocn = b_ancestor_parent->GetOrderedChildNodesReference();
			const auto &found = std::find(begin(bap_ocn), end(bap_ocn), b_ancestor);
			auto index = std::distance(begin(bap_ocn), found);
			lookup->AppendOrderedChildNode(upd.enm->AllocNode(static_cast<double>(index)));
		}
		else //didn't work... odd/error condition
		{
			delete lookup;
			return nullptr;
		}

		b_path_nodes.push_back(lookup);
		b_ancestor = b_ancestor_parent;
		b_ancestor_parent = upd.parentNodes[b_ancestor];
	}

	//make sure common ancestor is the same (otherwise return null)
	if(a_ancestor != b_ancestor)
		return nullptr;

	//build code to get the reference
	EvaluableNode *refpath = upd.enm->AllocNode(ENT_TARGET);
	refpath->AppendOrderedChildNode(upd.enm->AllocNode(a_ancestor_depth));

	//combine together
	while(b_path_nodes.size() > 0)
	{
		//pull off the end of b
		EvaluableNode *next = b_path_nodes.back();
		b_path_nodes.pop_back();

		next->GetOrderedChildNodes()[0] = refpath;
		refpath = next;
	}

	return refpath;
}

void Parser::SkipWhitespaceAndAccumulateAttributes(EvaluableNode *target)
{
	while(pos < code->size())
	{
		//eat any whitespace
		if(StringManipulation::IsUtf8Whitespace(*code, pos))
		{
			if(StringManipulation::IsUtf8Newline(*code, pos))
			{
				lineNumber++;
				lineStartPos = pos + 1;
			}

			pos++;
			continue;
		}

		auto cur_char = code->at(pos);

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
			while(pos < code->size())
			{
				cur_char = code->at(pos);
				if(cur_char != '\r' && cur_char != '\n')
					pos++;
				else
					break;
			}

			std::string cur_comment;
			//prepend the comment with newlines if there is already a comment on the node
			if(target->GetCommentsStringId() != StringInternPool::NOT_A_STRING_ID)
				cur_comment = "\r\n";
			cur_comment.append(code->substr(start_pos, pos - start_pos));

			target->AppendComments(cur_comment);
			continue;
		}

		//if it's a concurrent marker, set the property
		if(cur_char == '|' && pos + 1 < code->size() && code->at(pos + 1) == '|')
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
	if(originalSource.size() > 0)
	{
		std::string new_comment = sourceCommentPrefix;
		new_comment += std::to_string(lineNumber);
		new_comment += ' ';

		std::string_view line_to_opcode(&(*code)[lineStartPos], pos - lineStartPos);
		size_t column_number = StringManipulation::GetNumUTF8Characters(line_to_opcode);

		new_comment += std::to_string(column_number);
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
	while(pos < code->size())
	{
		auto cur_char = code->at(pos);

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
			if(pos < code->size())
			{
				cur_char = code->at(pos);
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
		while(pos < code->size() && code->at(pos) == '#')
			pos++;
	}

	//eat all characters until one that indicates end of identifier
	while(pos < code->size())
	{
		auto cur_char = code->at(pos);
		if(!std::isspace(static_cast<unsigned char>(cur_char))
				&& cur_char != ')'
				&& cur_char != '('
				&& cur_char != '#'
				&& cur_char != ';')
			pos++;
		else
			break;
	}
}

std::string Parser::GetNextIdentifier(bool allow_leading_label_marks)
{
	if(pos >= code->size())
		return std::string();

	//if quoted string, then go until the next end quote
	if(code->at(pos) == '"')
		return ParseString();
	else
	{
		size_t start_pos = pos;
		SkipToEndOfIdentifier(allow_leading_label_marks);
		return code->substr(start_pos, pos - start_pos);
	}
}

EvaluableNode *Parser::GetNextToken(EvaluableNode *new_token)
{
	if(new_token == nullptr)
		new_token = evaluableNodeManager->AllocNode(ENT_NULL);

	SkipWhitespaceAndAccumulateAttributes(new_token);
	if(pos >= code->size())
	{
		FreeNode(new_token);
		return nullptr;
	}

	auto cur_char = code->at(pos);

	if(cur_char == '(') //identifier as command
	{
		pos++;
		SkipWhitespaceAndAccumulateAttributes(new_token);
		if(pos >= code->size())
		{
			FreeNode(new_token);
			return nullptr;
		}

		std::string token = GetNextIdentifier();
		//first see if it's a keyword
		new_token->SetType(GetEvaluableNodeTypeFromString(token), evaluableNodeManager, false);
		if(IsEvaluableNodeTypeValid(new_token->GetType()))
			return new_token;

		//unspecified command, store the identifier in the string
		new_token->SetType(ENT_STRING, evaluableNodeManager, false);
		new_token->SetStringValue(token);
		return new_token;
	}
	else if(cur_char == ')')
	{
		pos++; //skip closing parenthesis
		FreeNode(new_token);
		return nullptr;
	}
	else if(std::isdigit(static_cast<unsigned char>(cur_char)) || cur_char == '-' || cur_char == '.')
	{
		size_t start_pos = pos;
		SkipToEndOfIdentifier();
		std::string s = code->substr(start_pos, pos - start_pos);

		//check for special values
		double value = 0.0;
		if(s == ".nas")
		{
			new_token->SetType(ENT_STRING, evaluableNodeManager, false);
			new_token->SetStringID(StringInternPool::NOT_A_STRING_ID);
			return new_token;
		}
		if(s == ".infinity")
			value = std::numeric_limits<double>::infinity();
		else if(s == "-.infinity")
			value = -std::numeric_limits<double>::infinity();
		else if(s == ".nan")
			value = std::numeric_limits<double>::quiet_NaN();
		else
		{
			auto [converted_value, success] = Platform_StringToNumber(s);
			if(success)
				value = converted_value;
		}

		new_token->SetType(ENT_NUMBER, evaluableNodeManager, false);
		new_token->SetNumberValue(value);
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

EvaluableNode *Parser::ParseNextBlock()
{
	EvaluableNode *tree_top = nullptr;
	EvaluableNode *curnode = nullptr;

	//as long as code left
	while(pos < code->size())
	{
		EvaluableNode *n = GetNextToken();

		//if end of a list
		if(n == nullptr)
		{
			//nothing here at all
			if(curnode == nullptr)
				return nullptr;

			const auto &parent = parentNodes.find(curnode);

			//if no parent, then all finished
			if(parent == end(parentNodes) || parent->second == nullptr)
				return tree_top;

			//jump up to the parent node
			curnode = parent->second;
			continue;
		}
		else //got some token
		{
			//if it's the first token, then put it up top
			if(tree_top == nullptr)
			{
				tree_top = n;
				curnode = n;
				continue;
			}

			if(curnode->IsOrderedArray())
			{
				curnode->AppendOrderedChildNode(n);
			}
			else if(curnode->IsAssociativeArray())
			{
				//n is the id, so need to get the next token
				StringInternPool::StringID index_sid = EvaluableNode::ToStringIDTakingReferenceAndClearing(n);

				//reset the node type but continue to accumulate any attributes
				n->SetType(ENT_NULL, evaluableNodeManager, false);
				n = GetNextToken(n);
				curnode->SetMappedChildNodeWithReferenceHandoff(index_sid, n, true);

				//handle case if uneven number of arguments
				if(n == nullptr)
				{
					//nothing here at all
					if(curnode == nullptr)
						return nullptr;

					const auto &parent = parentNodes.find(curnode);

					//if no parent, then all finished
					if(parent == end(parentNodes) || parent->second == nullptr)
						return tree_top;

					//jump up to the parent node
					curnode = parent->second;
					continue;
				}
			}

			parentNodes[n] = curnode;

			//if it's not immediate, then descend into that part of the tree, resetting parent index counter
			if(!IsEvaluableNodeTypeImmediate(n->GetType()))
				curnode = n;

			//if specifying something unusual, then assume it's just a null
			if(n->GetType() == ENT_NOT_A_BUILT_IN_TYPE)
				n->SetType(ENT_NULL, evaluableNodeManager);
		}

	}

	return tree_top;
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
	bool needs_escape = false;

	//check for any characters that need to be escaped
	if(s.find_first_of(" \t\"\n\r") != std::string::npos)
		needs_escape = true;

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
	bool expanded_whitespace, size_t indentation_depth)
{
	if(expanded_whitespace)
	{
		for(size_t i = 0; i < indentation_depth; i++)
			upd.result.push_back(indentationCharacter);
	}
	else
		upd.result.push_back(' ');

	const std::string &key_str = string_intern_pool.GetStringFromID(key_sid);

	//surround in quotes only if needed
	if(key_sid != string_intern_pool.NOT_A_STRING_ID
		&& HasCharactersBeyondIdentifier(key_str))
	{
		upd.result.push_back('"');
		upd.result.append(Backslashify(key_str));
		upd.result.push_back('"');
	}
	else
	{
		upd.result.append(key_str);
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
		auto [_, inserted] = upd.parentNodes.insert(std::make_pair(tree, parent));

		//if code already referenced, then print path to it
		if(!inserted)
		{
			upd.preevaluationNeeded = true;

			EvaluableNode *code_to_print = GetCodeForPathFromAToB(upd, parent, tree);
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

	//if already hit this node, then need to create code to rebuild the circular reference

	//add to check for circular references
	upd.parentNodes[tree] = parent;

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
	if(IsEvaluableNodeTypeImmediate(tree->GetType()))
	{
		switch(tree->GetType())
		{
		case ENT_NUMBER:
			upd.result.append(EvaluableNode::ToString(tree));
			break;
		case ENT_STRING:
		{
			auto sid = tree->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID)
			{
				upd.result.append(".nas");
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
		upd.result.push_back('(');
		upd.result.append(GetStringFromEvaluableNodeType(tree->GetType()));

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
			else if(num_child_nodes <= 6 && num_child_nodes + indentation_depth < 14)
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
			if(!upd.sortKeys)
			{
				for(auto &[k_id, k] : tree_mcn)
					AppendAssocKeyValuePair(upd, k_id, k, tree, recurse_expanded_whitespace, indentation_depth + 1);
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
					AppendAssocKeyValuePair(upd, k->first, k->second, tree, recurse_expanded_whitespace, indentation_depth + 1);
				}
			}
		}
		else if(tree->IsOrderedArray())
		{
			if(recurse_expanded_whitespace)
			{
				for(auto &e : tree->GetOrderedChildNodesReference())
					Unparse(upd, e, tree, true, indentation_depth + 1, true);
			}
			else //expanded whitespace
			{
				for(auto &e : tree->GetOrderedChildNodesReference())
				{
					upd.result.push_back(' ');
					Unparse(upd, e, tree, false, indentation_depth + 1, true);
				}
			}
		}

		//add closing parenthesis
		if(expanded_whitespace)
		{
			//indent if appropriate
			if(recurse_expanded_whitespace)
			{
				for(size_t i = 0; i < indentation_depth; i++)
					upd.result.push_back(indentationCharacter);
			}
			upd.result.append(")\r\n");
		}
		else
		{
			upd.result.append(")");
		}
	}
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

		//if it's an assoc, then treat the index as a string
		if(result->GetMappedChildNodes().size() > 0)
		{
			StringInternPool::StringID index_sid = EvaluableNode::ToStringIDIfExists(index_node);
			EvaluableNode **found = result->GetMappedChildNode(index_sid);
			if(found != nullptr)
				return *found;
			return nullptr;
		}

		//otherwise treat the index as a number for a list
		size_t index = static_cast<size_t>(EvaluableNode::ToNumber(index_node));
		if(result->GetOrderedChildNodes().size() > index)
			return result->GetOrderedChildNodes()[index];
	}

	case ENT_TARGET:
	{
		//first parameter is the number of steps to crawl up in the parent tree
		size_t steps_up = 0;
		if(path->GetOrderedChildNodes().size() > 0)
			steps_up = static_cast<size_t>(EvaluableNode::ToNumber(path->GetOrderedChildNodes()[0]));

		//at least need to go up one step
		steps_up++;

		//crawl up parse tree
		EvaluableNode *result = path;
		while(steps_up > 0 && result != nullptr)
		{
			auto found = parentNodes.find(result);
			if(found != end(parentNodes))
				result = found->second;
			else
				result = nullptr;
		}
	}

	default:
		return nullptr;
	}

	return nullptr;
}

void Parser::PreevaluateNodes()
{
	for(auto &n : preevaluationNodes)
	{
		if(n == nullptr)
			continue;

		auto node_type = n->GetType();
		if(node_type == ENT_GET || node_type == ENT_TARGET)
		{
			EvaluableNode *target = GetNodeFromRelativeCodePath(n);

			//transform the target to a location relative to the target's parent
			EvaluableNode *parent = nullptr;
			if(target == nullptr)
				continue;
			parent = parentNodes[target];
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
						break;
					}
				}
			}

			//mark both the originals' parents and the new parents as both cyclic
			EvaluableNode::SetParentEvaluableNodesCycleChecks(parentNodes[n], parentNodes);
			EvaluableNode::SetParentEvaluableNodesCycleChecks(parent, parentNodes);

			continue;
		}
	}
}
