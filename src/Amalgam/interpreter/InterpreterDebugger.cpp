//project headers:
#include "AssetManager.h"
#include "Interpreter.h"
#include "PerformanceProfiler.h"
#include "StringManipulation.h"

//system headers:
#include <tuple>

//makes an array of all the same value
template<typename T, size_t N>
constexpr std::array<T, N> make_array_of_duplicate_values(T value)
{
	std::array<T, N> a{};
	for(auto &x : a)
		x = value;
	return a;
}

//initially all set to point to debug
std::array<Interpreter::OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> Interpreter::_debug_opcodes
= make_array_of_duplicate_values<Interpreter::OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1>(&Interpreter::InterpretNode_DEBUG);

//initially all set to point to profile
std::array<Interpreter::OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> Interpreter::_profile_opcodes
= make_array_of_duplicate_values<Interpreter::OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1>(&Interpreter::InterpretNode_PROFILE);

bool Interpreter::_opcode_profiling_enabled = false;
bool Interpreter::_label_profiling_enabled = false;

//global static data for debugging
struct InterpreterDebugData
{
	//sets interactiveMode and handles any threading issues
	//any modifications to breakpoints triggered should occur before calling this method
	void EnableInteractiveMode()
	{
		interactiveMode = true;
	#ifdef MULTITHREAD_SUPPORT
		interactiveModeThread = std::this_thread::get_id();
	#endif
	}

	//if true, then the user is interacting
	bool interactiveMode = true;

#ifdef MULTITHREAD_SUPPORT
	//when interactiveMode is true, it'll keep running until
	// interactiveModeThread gets its chance to run
	std::thread::id interactiveModeThread = std::thread::id();
#endif

	//labels to break on
	std::vector<std::string> breakLabels;

	//opcodes to break on
	std::vector<EvaluableNodeType> breakOpcodes;

	//strings containing line number followed by filename to break on
	std::vector<std::string> breakLineFile;

	//will run until it reaches this label, then it will clear it
	std::string runUntilLabel = "";

	//will run until it reaches the next occurrence of this opcode, then it will clear it
	EvaluableNodeType runUntilOpcodeType = ENT_NOT_A_BUILT_IN_TYPE;

	//will run until this opcode is reached.  should only be used for opcodes that are preserved in the scope stack
	EvaluableNode *runUntilOpcode = nullptr;

	//will run until the scope stack size is this value
	size_t runUntilScopeStackSize = 0;

#ifdef MULTITHREAD_SUPPORT
	//only one debugger can set this at a time
	Concurrency::SingleMutex debuggingMutex;
#endif

} _interpreter_debug_data;

//if s is longer than max_num_chars, it modifies the string, clamping it
// at the length or newline and adding an ellipsis
void ClampSingleLineStringLength(std::string &s, size_t max_num_chars, std::string ellipsis = "...")
{
	if(max_num_chars < ellipsis.size())
		max_num_chars = ellipsis.size();

	//throw away everything on and after at the first newline
	s = s.substr(0, s.find('\n'));

	if(s.size() > max_num_chars)
	{
		//leave room for ellipsis
		s.resize(max_num_chars - ellipsis.size());
		s += ellipsis;
	}
}

//prints the node and its comment both truncated to max_num_chars or newline
std::pair<std::string, std::string> StringifyNode(EvaluableNode *en, EvaluableNodeManager *enm, size_t max_num_chars = 100)
{
	//if no comments, then can just print
	if(en == nullptr || en->GetCommentsStringId() == string_intern_pool.NOT_A_STRING_ID)
	{
		std::string code_str = Parser::Unparse(en, false, true, true);
		ClampSingleLineStringLength(code_str, max_num_chars);
		return std::make_pair(std::string(), code_str);
	}
	else //has comments, so need to thoughtfully handle showing first line of comments and appropriate amount of code
	{
		//get comment, and make it look like a comment
		std::string comment_str;
		comment_str += en->GetCommentsString();

		//if debug sources enabled, don't clamp the line, make sure it prints out the whole filename
		if(asset_manager.debugSources)
			max_num_chars = std::numeric_limits<size_t>::max();

		ClampSingleLineStringLength(comment_str, max_num_chars);

		//append with code
		EvaluableNode en_without_comment(en);
		en_without_comment.ClearComments();
		std::string code_str = Parser::Unparse(&en_without_comment, false, true, true);
		ClampSingleLineStringLength(code_str, max_num_chars);

		return std::make_pair(comment_str, code_str);
	}
}

//prints the current node for a stack trace
void PrintStackNode(EvaluableNode *en, EvaluableNodeManager *enm, size_t max_num_chars = 100)
{
	auto [comment_str, node_str] = StringifyNode(en, enm);
	if(!asset_manager.debugSources || comment_str.empty())
	{
		std::cout << "  opcode: " << node_str << std::endl;
	}
	else //comment
	{
		std::cout << "  comment:" << comment_str << std::endl;
		std::cout << "  opcode: " << node_str << std::endl;
	}
}

EvaluableNodeReference Interpreter::InterpretNode_DEBUG(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	DebugCheckBreakpointsAndUpdateState(en, true);

	EvaluableNodeType cur_node_type = ENT_NULL;
	if(en != nullptr)
		cur_node_type = en->GetType();

	bool enter_interactive_mode = false;
	if(_interpreter_debug_data.interactiveMode)
	{
	#ifdef MULTITHREAD_SUPPORT
		//if the thread id to look for matches, then clear the thread id
		if(_interpreter_debug_data.interactiveModeThread == std::this_thread::get_id())
			_interpreter_debug_data.interactiveModeThread = std::thread::id();

		//if there's no thread id to look for (e.g., the thread that had the breakpoint
		// broke, so all future threads should stop), then enter_interactive_mode
		if(_interpreter_debug_data.interactiveModeThread == std::thread::id())
	#endif
			enter_interactive_mode = true;
	}

	if(!enter_interactive_mode)
	{
		//get corresponding opcode stored in _debug_opcodes,
		//but don't ever request immediate when debugging
		auto oc = _debug_opcodes[cur_node_type];
		EvaluableNodeReference retval = (this->*oc)(en, false);

		//check for debug after execution
		DebugCheckBreakpointsAndUpdateState(en, false);

		return retval;
	}

#ifdef MULTITHREAD_SUPPORT
	//only one debugger at a time
	Concurrency::SingleLock lock(_interpreter_debug_data.debuggingMutex);

	//if it's no longer in interactiveMode since lock, then go back to normal execution
	if(!_interpreter_debug_data.interactiveMode)
	{
		//don't leave the lock locked while recursing into opcodes
		lock.unlock();

		//get corresponding opcode stored in _debug_opcodes
		//don't request immediate when debugging
		auto oc = _debug_opcodes[cur_node_type];
		EvaluableNodeReference retval = (this->*oc)(en, false);

		//check for debug after execution
		DebugCheckBreakpointsAndUpdateState(en, false);

		return retval;
	}

	//get the current thread
	std::thread::id this_thread_id = std::this_thread::get_id();
#endif

	while(true)
	{
		auto entity_sid = string_intern_pool.NOT_A_STRING_ID;
		if(curEntity != nullptr)
			entity_sid = curEntity->GetIdStringId();

		if(asset_manager.debugMinimal)
		{
			//use carriage return sequence to signify the end of transmission
		#ifdef MULTITHREAD_SUPPORT
			std::cout << "\r\r" << this_thread_id << " >" << std::endl;
		#else
			std::cout << "\r\r>" << std::endl;
		#endif
		}
		else
		{
			if(entity_sid != string_intern_pool.NOT_A_STRING_ID)
				std::cout << "Entity: " << string_intern_pool.GetStringFromID(entity_sid) << std::endl;

		#ifdef MULTITHREAD_SUPPORT
			std::cout << "Thread: " << this_thread_id << std::endl;
		#endif

			auto [comment_str, node_str] = StringifyNode(en, evaluableNodeManager);
			if(comment_str.empty())
			{
				std::cout << "Current opcode: " << node_str << std::endl;
			}
			else //comment
			{
				std::cout << "Current comment:" << comment_str << std::endl;
				std::cout << "Current opcode: " << node_str << std::endl;
			}
			std::cout << "> ";
		}

		std::string input;
		std::getline(std::cin, input);
		std::string command = StringManipulation::RemoveFirstToken(input);

		if(command == "help")
		{
			std::cout << "Debugging commands:" << std::endl;
			std::cout << "help: display this message" << std::endl;
			std::cout << "quit: quit the program and exit" << std::endl;
			std::cout << "s: step to next opcode (step into)" << std::endl;
			std::cout << "n: runs to next opcode (step over)" << std::endl;
			std::cout << "f: finish current opcode (step up)" << std::endl;
			std::cout << "fc: finish call (step out)" << std::endl;
			std::cout << "ul label: runs until it encounters a node with label" << std::endl;
			std::cout << "uo opcode: runs until it encounters a node of type opcode" << std::endl;
			std::cout << "c: continues until next breakpoint" << std::endl;
			std::cout << "finish: finish running the program, leaving debug mode, running at full speed" << std::endl;
			std::cout << "bl label: toggles breakpoint at the label" << std::endl;
			std::cout << "bn line_number file: toggles breakpoint at the line number for file" << std::endl;
			std::cout << "bo opcode: toggles breakpoint on all occurrences of opcode" << std::endl;
			std::cout << "br: lists breakpoints" << std::endl;
			std::cout << "stack: prints out the stack" << std::endl;
			std::cout << "entities: prints out the contained entities" << std::endl;
			std::cout << "entity [name]: prints out the entity specified, current entity if name omitted" << std::endl;
			std::cout << "labels [name]: prints out the labels of the entity specified, current entity if name omitted" << std::endl;
			std::cout << "vars: prints out the variables, grouped by each layer going up the stack" << std::endl;
			std::cout << "p [var]: prints variable var" << std::endl;
			std::cout << "pv [var]: prints only the value of the variable var (no comments or labels)" << std::endl;
			std::cout << "pp [var]: prints only a preview of the value of the variable var (no comments or labels)" << std::endl;
			std::cout << "eval [expression]: evaluates expression" << std::endl;
			std::cout << "validate: validate memory integrity" << std::endl;
		#ifdef MULTITHREAD_SUPPORT
			std::cout << "threads: displays the current thread ids" << std::endl;
		#endif
			continue;
		}
		else if(command == "quit")
		{
			exit(0);
		}
		else if(command == "s")
		{
			if(EvaluableNode::IsNull(en))
				return EvaluableNodeReference::Null();

			//exit interactive loop
			break;
		}
		else if(command == "n")
		{
			_interpreter_debug_data.runUntilOpcode = en;

			//run until breakpoint
			_interpreter_debug_data.interactiveMode = false;

			//exit interactive loop
			break;
		}
		else if(command == "f" || command == "fc" || command == "ul" || command == "uo" || command == "c")
		{
			if(command == "f")
			{
				if(opcodeStackNodes->size() > 0)
					_interpreter_debug_data.runUntilOpcode = opcodeStackNodes->back();
			}
			else if(command == "fc")
			{
				if(scopeStackNodes->size() > 0)
					_interpreter_debug_data.runUntilScopeStackSize = scopeStackNodes->size() - 1;
			}
			else if(command == "ul")
			{
				//go back to prompt if not a string
				if(input.empty())
					continue;

				_interpreter_debug_data.runUntilLabel = input;
			}
			else if(command == "uo")
			{
				_interpreter_debug_data.runUntilOpcodeType = GetEvaluableNodeTypeFromString(input);

				//go back to prompt if not valid type
				if(_interpreter_debug_data.runUntilOpcodeType == ENT_NOT_A_BUILT_IN_TYPE)
					continue;
			}

			//run until breakpoint
			_interpreter_debug_data.interactiveMode = false;

			//exit interactive loop
			break;
		}
		else if(command == "finish")
		{
			SetDebuggingState(false);

			//get regular opcode, not the debug one
			auto oc = _opcodes[cur_node_type];

		#ifdef MULTITHREAD_SUPPORT
			//unlock before executing
			lock.unlock();
		#endif

			//don't request immediate when debugging
			EvaluableNodeReference retval = (this->*oc)(en, false);
			return retval;
		}
		else if(command == "bl")
		{
			if(!input.empty())
			{
				auto found = std::find(begin(_interpreter_debug_data.breakLabels),
					end(_interpreter_debug_data.breakLabels), input);
				if(found == end(_interpreter_debug_data.breakLabels))
				{
					_interpreter_debug_data.breakLabels.push_back(input);
					std::cout << "Added breakpoint for label " << input << std::endl;
				}
				else
				{
					_interpreter_debug_data.breakLabels.erase(found);
					std::cout << "Removed breakpoint for label " << input << std::endl;
				}
			}
		}
		else if(command == "bn")
		{
			auto found = std::find(begin(_interpreter_debug_data.breakLineFile),
				end(_interpreter_debug_data.breakLineFile), input);
			if(found == end(_interpreter_debug_data.breakLineFile))
			{
				_interpreter_debug_data.breakLineFile.push_back(input);
				std::cout << "Added breakpoint for " << input << std::endl;
			}
			else
			{
				_interpreter_debug_data.breakLineFile.erase(found);
				std::cout << "Removed breakpoint for " << input << std::endl;
			}
		}
		else if(command == "bo")
		{
			auto break_opcode = GetEvaluableNodeTypeFromString(input);
			if(break_opcode != ENT_NOT_A_BUILT_IN_TYPE)
			{
				auto found = std::find(begin(_interpreter_debug_data.breakOpcodes),
					end(_interpreter_debug_data.breakOpcodes), break_opcode);
				if(found == end(_interpreter_debug_data.breakOpcodes))
				{
					_interpreter_debug_data.breakOpcodes.push_back(break_opcode);
					std::cout << "Added breakpoint for opcode " << input << std::endl;
				}
				else
				{
					_interpreter_debug_data.breakOpcodes.erase(found);
					std::cout << "Removed breakpoint for opcode " << input << std::endl;
				}
			}
		}
		else if(command == "br")
		{
			std::cout << "Opcodes Breakpoints:" << std::endl;
			for(auto break_opcode : _interpreter_debug_data.breakOpcodes)
				std::cout << "  " << GetStringFromEvaluableNodeType(break_opcode) << std::endl;

			std::cout << "Label Breakpoints:" << std::endl;
			for(auto &break_label : _interpreter_debug_data.breakLabels)
				std::cout << "  " << break_label << std::endl;

			std::cout << "Line Breakpoints:" << std::endl;
			for(auto &break_line : _interpreter_debug_data.breakLineFile)
				std::cout << "  " << break_line << std::endl;
		}
		else if(command == "stack")
		{
			std::cout << "Construction stack:" << std::endl;
			for(EvaluableNode *csn : *constructionStackNodes)
				PrintStackNode(csn, evaluableNodeManager);

			std::cout << "scope stack:" << std::endl;
			for(EvaluableNode *csn : *scopeStackNodes)
				PrintStackNode(csn, evaluableNodeManager);

			std::cout << "Opcode stack:" << std::endl;
			for(EvaluableNode *insn : *opcodeStackNodes)
				PrintStackNode(insn, evaluableNodeManager);
		}
		else if(command == "entities")
		{
			if(curEntity != nullptr && curEntity->HasContainedEntities())
			{
				for(auto &e : curEntity->GetContainedEntities())
					std::cout << "  " << string_intern_pool.GetStringFromID(e->GetIdStringId()) << std::endl;
			}
		}
		else if(command == "entity" || command == "labels")
		{
			if(curEntity == nullptr)
			{
				std::cout << "not in an entity" << std::endl;
				continue;
			}

			Entity *entity = curEntity;

			if(!input.empty())
				entity = curEntity->GetContainedEntity(string_intern_pool.GetIDFromString(input));

			if(entity == nullptr)
			{
				std::cout << "Entity " << input << " not found in current entity" << std::endl;
				continue;
			}

			if(command == "entity")
			{
				std::cout << entity->GetCodeAsString() << std::endl;
			}
			else if(command == "labels")
			{
				entity->IterateFunctionOverLabels([]
				(StringInternPool::StringID label_sid, EvaluableNode *node)
					{
						std::cout << "  " << string_intern_pool.GetStringFromID(label_sid) << std::endl;
					}, nullptr, true, true);
			}
		}
		else if(command == "vars")
		{
			//find symbol by walking up the stack; each layer must be an assoc
			//count down from the top, and use (i - 1) below to make this loop one-based instead of having to wrap around
			for(auto i = scopeStackNodes->size(); i > 0; i--)
			{
				EvaluableNode *cur_context = (*scopeStackNodes)[i - 1];

				//see if this level of the stack contains the symbol
				auto &mcn = cur_context->GetMappedChildNodesReference();
				for(auto &[symbol_id, _] : mcn)
					std::cout << "  " << string_intern_pool.GetStringFromID(symbol_id) << std::endl;
			}
		}
		else if(command == "p" || command == "pv" || command == "pp")
		{
			auto sid = string_intern_pool.GetIDFromString(input);
			if(sid == string_intern_pool.NOT_A_STRING_ID)
			{
				std::cout << "string " << input << " is not currently referenced anywhere." << std::endl;
			}
			else //valid string
			{
				EvaluableNode *node = nullptr;
				bool value_exists = true;

				bool found = false;
				std::tie(node, found) = GetScopeStackSymbol(sid, false);
				if(!found)
				{
					if(curEntity == nullptr)
					{
						std::cout << "Variable " << input << " does not exist on the stack, and there is no current entity." << std::endl;
						value_exists = false;
					}

					bool found_value = false;
					std::tie(node, found_value) = curEntity->GetValueAtLabel(sid, nullptr, true, true);
					if(!found_value)
					{
						std::cout << "Variable " << input << " does not exist on the stack or as a label in the current entity." << std::endl;
						value_exists = false;
					}
				}

				if(value_exists)
				{
					if(command == "p")
						std::cout << Parser::Unparse(node, true, true, true) << std::endl;
					else if(command == "pv")
						std::cout << Parser::Unparse(node, true, false, true) << std::endl;
					else if(command == "pp")
					{
						std::string var_preview = Parser::Unparse(node, true, false, true);
						if(var_preview.size() > 1023)
							var_preview.resize(1023);
						std::cout << var_preview << std::endl;
					}
				}
			}
		}
		else if(command == "eval")
		{
			SetDebuggingState(false);
			auto [node, warnings, char_with_error] = Parser::Parse(input, evaluableNodeManager);
			for(auto &w : warnings)
				std::cerr << w << std::endl;

			EvaluableNodeReference result = InterpretNodeForImmediateUse(node);
			std::cout << Parser::Unparse(result, true, true, true) << std::endl;
			SetDebuggingState(true);
		}
		else if(command == "validate")
		{
			VerifyEvaluableNodeIntegrity();
			std::cout << "validation completed successfully" << std::endl;
		}
	#ifdef MULTITHREAD_SUPPORT
		else if(command == "threads")
		{
			auto thread_ids = Concurrency::threadPool.GetThreadIds();
			for(auto &thread_id : thread_ids)
				std::cout << "  " << thread_id << std::endl;
		}
	#endif
	}

	//finish executing this opcode

#ifdef MULTITHREAD_SUPPORT
	//unlock before executing
	lock.unlock();
#endif

	//get corresponding opcode stored in _debug_opcodes
	//don't request immediate when debugging
	auto oc = _debug_opcodes[en->GetType()];
	EvaluableNodeReference retval = (this->*oc)(en, false);

	//check for debug after execution
	DebugCheckBreakpointsAndUpdateState(en, false);

	return retval;
}

void Interpreter::SetDebuggingState(bool debugging_enabled)
{
	if(debugging_enabled)
	{
		//skip if already debugging
		if(_opcodes[0] == &Interpreter::InterpretNode_DEBUG)
			return;
	}
	else //!debugging_enabled
	{
		//skip if already not debugging
		if(_debug_opcodes[0] == &Interpreter::InterpretNode_DEBUG)
			return;
	}

	//swap debug opcodes for real ones
	for(size_t i = 0; i < _opcodes.size(); i++)
		std::swap(_opcodes[i], _debug_opcodes[i]);
}

bool Interpreter::GetDebuggingState()
{
	return (_opcodes[0] == &Interpreter::InterpretNode_DEBUG);
}

void Interpreter::SetOpcodeProfilingState(bool opcode_profiling_enabled)
{
	if(opcode_profiling_enabled)
	{
		//skip if already debugging or profiling
		if(_opcodes[0] == &Interpreter::InterpretNode_DEBUG
				|| _opcodes[0] == &Interpreter::InterpretNode_PROFILE)
			return;

		_opcode_profiling_enabled = true;
	}
	else //!opcode_profiling_enabled
	{
		//skip if already not debugging
		if(_profile_opcodes[0] == &Interpreter::InterpretNode_PROFILE)
			return;

		_opcode_profiling_enabled = false;
	}

	PerformanceProfiler::SetProfilingState(_opcode_profiling_enabled);

	//swap debug opcodes for real ones
	for(size_t i = 0; i < _opcodes.size(); i++)
		std::swap(_opcodes[i], _profile_opcodes[i]);
}

void Interpreter::SetLabelProfilingState(bool label_profiling_enabled)
{
	_label_profiling_enabled = label_profiling_enabled;
	PerformanceProfiler::SetProfilingState(_label_profiling_enabled);
}

void Interpreter::DebugCheckBreakpointsAndUpdateState(EvaluableNode *en, bool before_opcode)
{
	EvaluableNodeType cur_node_type = ENT_NULL;
	if(en != nullptr)
		cur_node_type = en->GetType();

	//if not interactive, check for events that could trigger interactiveMode
	if(!_interpreter_debug_data.interactiveMode)
	{
		if(_interpreter_debug_data.runUntilOpcodeType == cur_node_type)
		{
			_interpreter_debug_data.runUntilOpcodeType = ENT_NOT_A_BUILT_IN_TYPE;
			_interpreter_debug_data.EnableInteractiveMode();
		}

		//break if finished opcode
		if(_interpreter_debug_data.runUntilOpcode == en)
		{
			_interpreter_debug_data.runUntilOpcode = nullptr;
			_interpreter_debug_data.EnableInteractiveMode();
		}

		if(_interpreter_debug_data.runUntilScopeStackSize == scopeStackNodes->size())
		{
			_interpreter_debug_data.runUntilScopeStackSize = 0;
			_interpreter_debug_data.EnableInteractiveMode();
		}

		for(auto boc : _interpreter_debug_data.breakOpcodes)
		{
			if(cur_node_type == boc)
				_interpreter_debug_data.EnableInteractiveMode();
		}

		//only do line breakpoints before hitting an opcode
		if(asset_manager.debugSources && before_opcode
			&& _interpreter_debug_data.breakLineFile.size() > 0)
		{
			//if it has a source, check against all of the source break points
			auto &comment_str = en->GetCommentsString();
			if(comment_str.rfind(Parser::sourceCommentPrefix, 0) != std::string::npos)
			{
				for(auto &breakpoint_str : _interpreter_debug_data.breakLineFile)
				{
					//start comment after the prefix
					size_t comment_pos = Parser::sourceCommentPrefix.size();
					size_t breakpoint_pos = 0;

					bool breakpoint_match = true;

					//check if line numbers are the same up until the space
					for(; comment_pos < comment_str.size() && breakpoint_pos < breakpoint_str.size(); comment_pos++, breakpoint_pos++)
					{
						//stop if both are done with line number
						if(comment_str[comment_pos] == ' ' && breakpoint_str[breakpoint_pos] == ' ')
						{
							comment_pos++;
							breakpoint_pos++;
							break;
						}

						if(comment_str[comment_pos] != breakpoint_str[breakpoint_pos])
						{
							breakpoint_match = false;
							break;
						}
					}

					//can't proceed if line numbers don't match
					if(comment_pos == comment_str.size() || !breakpoint_match)
						continue;

					//skip over column number in comment
					for(; comment_pos < comment_str.size(); comment_pos++)
					{
						if(comment_str[comment_pos] == ' ')
						{
							comment_pos++;
							break;
						}

						//make sure column only consists of number characters; fail if improper format
						if(comment_str[comment_pos] > '9' || comment_str[comment_pos] < '0')
						{
							breakpoint_match = false;
							break;
						}
					}

					//can't proceed if column numbers isn't valid
					if(comment_pos == comment_str.size() || !breakpoint_match)
						continue;

					//iterate until have reach end of both or found a non-match
					for(; comment_pos < comment_str.size() && breakpoint_pos < breakpoint_str.size(); comment_pos++, breakpoint_pos++)
					{
						//if either line is done, then stop
						bool comment_newline = (comment_str[comment_pos] == '\r' || comment_str[comment_pos] == '\n');
						bool breakpoint_line_newline = (breakpoint_str[breakpoint_pos] == '\r' || breakpoint_str[breakpoint_pos] == '\n');
						if(comment_newline || breakpoint_line_newline)
							break;

						if(comment_str[comment_pos] != breakpoint_str[breakpoint_pos])
						{
							breakpoint_match = false;
							break;
						}
					}

					//make sure both comment and breakpoint line are complete (so that one isn't just a partial match of the other)s
					bool comment_complete = (comment_pos == comment_str.size()
												|| comment_str[comment_pos] == '\r' || comment_str[comment_pos] == '\n');
					bool breakpoint_line_complete = (breakpoint_pos == breakpoint_str.size()
												|| breakpoint_str[breakpoint_pos] == '\r' || breakpoint_str[breakpoint_pos] == '\n');

					if(breakpoint_match && comment_complete && breakpoint_line_complete)
					{
						_interpreter_debug_data.EnableInteractiveMode();
						//don't need to check any more breakpoints
						break;
					}
				}
			}
		}

		//if breaking on a label
		if(!_interpreter_debug_data.runUntilLabel.empty() || _interpreter_debug_data.breakLabels.size() > 0)
		{
			size_t num_labels = 0;
			if(en != nullptr)
				num_labels = en->GetNumLabels();

			if(num_labels > 0)
			{
				//check each label to see if matches
				auto run_until_label_sid = string_intern_pool.GetIDFromString(_interpreter_debug_data.runUntilLabel);

				for(size_t i = 0; i < num_labels; i++)
				{
					auto label_sid = en->GetLabelStringId(i);
					if(label_sid == run_until_label_sid)
					{
						//re-enter interactiveMode and clear runUntilLabel
						_interpreter_debug_data.runUntilLabel = "";
						_interpreter_debug_data.EnableInteractiveMode();
						break;
					}

					//iterate over all break labels
					for(auto &label : _interpreter_debug_data.breakLabels)
					{
						auto break_label_sid = string_intern_pool.GetIDFromString(label);
						if(label_sid == break_label_sid)
						{
							//re-enter interactiveMode
							_interpreter_debug_data.EnableInteractiveMode();
							break;
						}
					}
				}
			}
		}
	}
}

EvaluableNodeReference Interpreter::InterpretNode_PROFILE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	std::string opcode_str = asset_manager.GetEvaluableNodeSourceFromComments(en);
	opcode_str += GetStringFromEvaluableNodeType(en->GetType(), true);
	PerformanceProfiler::StartOperation(opcode_str, evaluableNodeManager->GetNumberOfUsedNodes());

	EvaluableNodeType cur_node_type = ENT_NULL;
	if(en != nullptr)
		cur_node_type = en->GetType();

	//get corresponding opcode stored in _profile_opcodes
	auto oc = _profile_opcodes[cur_node_type];
	EvaluableNodeReference retval = (this->*oc)(en, immediate_result);

	PerformanceProfiler::EndOperation(evaluableNodeManager->GetNumberOfUsedNodes());

	return retval;
}
