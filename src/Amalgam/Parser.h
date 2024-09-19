#pragma once

//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"

//system headers:
#include <algorithm>
#include <string>
#include <vector>

class Parser
{
public:
	Parser();

	//returns true if the string needs to be backslashified
	inline static bool NeedsBackslashify(const std::string &s)
	{
		for(auto c : s)
		{
			switch(c)
			{
			case '\0':
			case '\\':
			case '"':
			case '\t':
			case '\n':
			case '\r':
				return true;

			default:
				break;
			}
		}

		return false;
	}

	//returns true if the string needs to be backslashified, has spaces, or has special characters
	inline static bool HasCharactersBeyondIdentifier(const std::string &s, bool label = false)
	{
		if(s.empty())
			return false;

		if(s[0] == '.' || s[0] == '-')
			return true;

		bool in_label_initial_hashes = label;
		for(size_t i = 0; i < s.size(); i++)
		{
			//can ignore any #'s up front
			if(in_label_initial_hashes)
			{
				if(s[i] == '#')
					continue;
				in_label_initial_hashes = false;
			}

			if(StringManipulation::IsUtf8Whitespace(s, i))
				return true;

			switch(s[i])
			{
			case '\0':
			case '\\':
			case '"':
			case '(':
			case ')':
			case '[':
			case ']':
			case '{':
			case '}':
			case '#':
			case '@':
			case ';':
				return true;

			default:
				break;
			}
		}

		return false;
	}

	//Returns a properly backslashified string
	static std::string Backslashify(const std::string &s);

	//appends a newline to s and indents the newline the required amount
	static inline void AppendNewlineWithIndentation(std::string &s, size_t indentation_depth, bool pretty)
	{
		if(pretty)
		{
			s.append("\r\n");
			for(size_t i = 0; i < indentation_depth; i++)
				s.push_back(indentationCharacter);
		}
		else
			s.push_back(' ');
	}

	//Parses the code string and returns a tree of EvaluableNodeReference that represents the code,
	// as well as the offset of any error, or larger than the length of code_string if no errors
	//if transactional_parse is true, then it will ignore any incomplete or erroneous opcodes except the outermost one
	//if original_source is a valid string, it will emit any warnings to stderr
	//if debug_sources is true, it will prepend each node with a comment indicating original source
	static std::tuple<EvaluableNodeReference, std::vector<std::string>, size_t>
		Parse(std::string &code_string, EvaluableNodeManager *enm,
		bool transactional_parse = false, std::string *original_source = nullptr, bool debug_sources = false);

	//Returns a string that represents the tree
	// if expanded_whitespace, will emit additional whitespace to make it easier to read
	// if emit_attributes, then it will emit comments, labels, concurrency, preevaluations, etc.; if emit_attributes is false, then it will only emit values
	// if sort_keys, then it will perform a sort on all unordered nodes
	static std::string Unparse(EvaluableNode *tree, EvaluableNodeManager *enm,
		bool expanded_whitespace = true, bool emit_attributes = true, bool sort_keys = false);

	//prefix used in the comments when attributing sources to EvaluableNodes
	inline static const std::string sourceCommentPrefix = "src: ";

protected:

	//data passed down through recursion with UnparseData
	class UnparseData
	{
	public:
		//result string
		std::string result;

		//parentNodes contains each reference as the key and the parent as the value
		EvaluableNode::ReferenceAssocType parentNodes;

		EvaluableNodeManager *enm;

		//if true, then the tree is cycle free and don't need to keep track of potential circular references
		bool cycleFree;

		//if true, then should be marked for preevaluation
		bool preevaluationNeeded;

		//if true, then emit comments, labels, concurrency, preevaluations, etc.
		bool emitAttributes;

		//if true, then it will perform a sort on all unordered nodes
		bool sortKeys;
	};

	//Returns code that will get from location a to b.
	static EvaluableNode *GetCodeForPathToSharedNodeFromParentAToParentB(UnparseData &upd,
		EvaluableNode *shared_node, EvaluableNode *a_parent, EvaluableNode *b_parent);

	//Skips whitespace and accumulates any attributes (e.g., labels, comments) on to target
	void SkipWhitespaceAndAccumulateAttributes(EvaluableNode *target);

	//Parses until the end of the quoted string, updating the position and returns the string with interpreted characters
	std::string ParseString();

	//Skips non-whitespace, non-parenthesis, and non-label markers, non-comment begin, etc.
	// if allow_leading_label_marks is true, then it will not end on label marks (#) at the beginning of the string
	void SkipToEndOfIdentifier(bool allow_leading_label_marks = false);

	//Advances position and returns the current identifier
	// if allow_leading_label_marks is true, then it will not end on label marks (#) at the beginning of the string
	std::string GetNextIdentifier(bool allow_leading_label_marks = false);

	//Returns a EvaluableNode containing the next token, null if none left in current context
	// parent_node is primarily to check for errors or warnings
	//if new_token is not nullptr, it will put the token in the EvaluableNode provided, otherwise will return a new one
	EvaluableNode *GetNextToken(EvaluableNode *parent_node, EvaluableNode *new_token = nullptr);

	//deallocates the current node in case there is an early exit or error
	void FreeNode(EvaluableNode *node);

	//Parses the next block of code into topNode
	void ParseCode();

	//Prints out all comments for the respective node
	static void AppendComments(EvaluableNode *n, size_t indentation_depth, bool pretty, std::string &to_append);

	//Prints out all labels for the respective node. If omit_label is not null, it will not print any label that matches it
	static void AppendLabels(UnparseData &upd, EvaluableNode *n, size_t indentation_depth, bool pretty);

	//Prints out key and its associated node n
	static void AppendAssocKeyValuePair(UnparseData &upd,
		StringInternPool::StringID key_sid, EvaluableNode *n, EvaluableNode *parent,
		bool expanded_whitespace, size_t indentation_depth, bool need_initial_space);

	size_t GetCurrentLineNumber()
	{
		return lineNumber + 1;
	}

	size_t GetCurrentCharacterNumberInLine()
	{
		std::string_view line_to_opcode(&(*code)[lineStartPos], pos - lineStartPos);
		size_t char_number = StringManipulation::GetNumUTF8Characters(line_to_opcode);
		return char_number + 1;
	}

	//appends the warning string on to warnings
	inline void EmitWarning(std::string warning)
	{
		std::string combined = "Warning: " + warning
			+ " at line " + StringManipulation::NumberToString(GetCurrentLineNumber())
			+ ", column " + StringManipulation::NumberToString(GetCurrentCharacterNumberInLine());

		if(!originalSource.empty())
			combined += " of " + originalSource;

		warnings.emplace_back(combined);
	}

	//Appends to the string s that represents the code tree
	//if expanded_whitespace, then it will add whitespace as appropriate to make it pretty
	// each line is additionally indented by the number of spaces specified
	// if need_initial_indent is true, then it will perform an indentation before generating the first code,
	// otherwise, will assume the indentation is already where it should be
	static void Unparse(UnparseData &upd, EvaluableNode *tree, EvaluableNode *parent, bool expanded_whitespace, size_t indentation_depth, bool need_initial_indent);
	
	//given a path starting at path's parent, parses the path and returns the target location
	EvaluableNode *GetNodeFromRelativeCodePath(EvaluableNode *path);

	//resolves any nodes that require preevaluation (such as assocs or circular references)
	void PreevaluateNodes();

	//Pointer to code currently being parsed
	std::string *code;

	//Position of the code currently being parsed
	size_t pos;

	//Current line number
	size_t lineNumber;

	//Position at the start of the current line
	size_t lineStartPos;

	//number of currently open parenthesis
	int64_t numOpenParenthesis;

	//Original source (e.g., file if applicable)
	std::string originalSource;

	//if true, will prepend debug sources to node comments
	bool debugSources;

	//the top node of everything being parsed
	EvaluableNode *topNode;

	//contains a list of nodes that need to be preevaluated on parsing
	std::vector<EvaluableNode *> preevaluationNodes;

	//any warnings from parsing
	std::vector<std::string> warnings;

	//parentNodes contains each reference as the key and the parent as the value
	EvaluableNode::ReferenceAssocType parentNodes;

	EvaluableNodeManager *evaluableNodeManager;

	//if true, then it will ignore any incomplete or erroneous opcodes except the outermost one
	bool transactionalParse;

	//offset of the last code that was properly completed
	size_t charOffsetStartOfLastCompletedCode;

	//character used for indentation
	static const char indentationCharacter = '\t';
};
