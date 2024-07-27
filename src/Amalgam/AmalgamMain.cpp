//project headers:
#include "Amalgam.h"
#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "Concurrency.h"
#include "Entity.h"
#include "EntityExternalInterface.h"
#include "EntityQueries.h"
#include "EntityWriteListener.h"
#include "EvaluableNode.h"
#include "EvaluableNodeTreeFunctions.h"
#include "Interpreter.h"
#include "Parser.h"
#include "PerformanceProfiler.h"
#include "PlatformSpecific.h"

//system headers:
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

//function prototypes:
int32_t RunAmalgamTrace(std::istream *in_stream, std::ostream *out_stream, std::string &random_seed);

//usage:
// Note: spaces in raw string are correct, do not replace with tabs
std::string GetUsage()
{
	std::stringstream usage;
	usage
		<< "Amalgam Interpreter (" << AMALGAM_VERSION_STRING << ") - " << GetConcurrencyTypeString() << std::endl
		<<
R"(
Usage: amalgam [options] [file]

Options:
    -h, --help       Show help

    -v, --version    Show version

    -q, --quiet      Silence all stdio

    -l [file]        Specify a log file

    -s [seed]        Specify a particular random number seed. Can be any alphanumeric string

    -t [file]        Specify a code-based transaction log file

    --p-opcodes      Display engine profiling information for opcodes upon completion (one profiling
                     type allowed at a time); when used with --debug-sources, reports line numbers

    --p-labels       Display engine profiling information for labels upon completion (one profiling
                     type allowed at a time)

    --p-count [number]
                     When used with --p-opcodes or --p-labels, specifies the count of the top profile
                     information elements to display; the default is 20 for command line, all when
                     --p-file is specified

    --p-file [file]  When used with --p-opcodes or --p-labels, writes the profile information to a file

    --debug          When specified, begins in debugging mode

    --debug-minimal  When specified, begins in debugging mode with minimal output while stepping

    --debug-sources  When specified, prepends all node comments with the source of the node when applicable

    --nosbfds        Disables the sbfds acceleration, which is generally preferred in the heuristics

    --trace          Uses commands via stdio to act as if it were being called as a library

    --tracefile [file]
                     Like trace, but pulls the data from the file specified
)";

	//additional compiler defined options
	usage
#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
		<< std::endl
		<< "    --numthreads [number]" << std::endl
		<< "                     Maximum number of threads to use (if unspecified or set to zero, may use unlimited)" << std::endl
#endif
		<< std::endl;

	return usage.str();
}

//main
PLATFORM_MAIN_CONSOLE
{
	PLATFORM_ARGS_CONSOLE;

	if(args.size() == 1)
	{
		std::cout << GetUsage() << std::endl;
		return 0;
	}

	//run options
	bool debug_state = false;
	bool debug_minimal = false;
	bool debug_sources = false;
	bool profile_opcodes = false;
	bool profile_labels = false;
	size_t profile_count = 0;
	std::string profile_out_file;
	bool run_trace = false;
	bool run_tracefile = false;
	std::string tracefile;
	std::string amlg_file_to_run;
	bool print_to_stdio = true;
	std::string write_log_filename;
	std::string print_log_filename;
#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
	size_t num_threads = 0;
#endif
	bool debug_internal_memory = Platform_IsDebuggerPresent();

	typedef std::chrono::steady_clock clk;
	auto t = std::chrono::duration_cast<std::chrono::milliseconds>(clk::now().time_since_epoch()).count();
	std::string random_seed = std::to_string(t);
	if(Platform_IsDebuggerPresent())
		random_seed = "01234567890123456789012345";

	//parameters to be passed into the code being run
	std::string interpreter_path{args[0]};
	std::vector<std::string> passthrough_params;
	passthrough_params.emplace_back(""); //add placeholder for script name

	for(size_t i = 1; i < args.size(); i++)
	{
		if(args[i] == "-h" || args[i] == "--help")
		{
			std::cout << GetUsage();
			return 0;
		}
		else if(args[i] == "-v" || args[i] == "--version")
		{
			std::cout << AMALGAM_VERSION_STRING << std::endl;
			return 0;
		}
		else if(args[i] == "-q" || args[i] == "--quiet")
			print_to_stdio = false;
		else if(args[i] == "-l" && i + 1 < args.size())
			print_log_filename = args[++i];
		else if(args[i] == "-s" && i + 1 < args.size())
			random_seed = args[++i];
		else if(args[i] == "-t" && i + 1 < args.size())
			write_log_filename = args[++i];
		else if(args[i] == "--p-opcodes")
			profile_opcodes = true;
		else if(args[i] == "--p-labels")
			profile_labels = true;
		else if(args[i] == "--p-count" && i + 1 < args.size())
			profile_count = static_cast<size_t>(std::max(std::atoi(args[++i].data()), 0));
		else if(args[i] == "--p-file" && i + 1 < args.size())
			profile_out_file = args[++i];
		else if(args[i] == "--debug")
			debug_state = true;
		else if(args[i] == "--debug-minimal")
		{
			debug_state = true;
			debug_minimal = true;
		}
		else if(args[i] == "--debug-sources")
			debug_sources = true;
		else if(args[i] == "--nosbfds")
			_enable_SBF_datastore = false;
		else if(args[i] == "--trace")
			run_trace = true;
		else if(args[i] == "--tracefile" && i + 1 < args.size())
		{
			run_tracefile = true;
			tracefile = args[++i];
		}
	#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
		else if(args[i] == "--numthreads")
			num_threads = static_cast<size_t>(std::max(std::atoi(args[++i].data()), 0));
	#endif
		else if(args[i] == "--debug-internal-memory")
		{
			//parameter for internal debugging only -- intentionally not listed in documentation
			debug_internal_memory = true;
		}
		else if(amlg_file_to_run == "")
		{
			//if relative path, prepend current working dir to make absolute path
			//path is not converted to canonical path to preserve user's input
			std::filesystem::path file(args[i]);
			if(file.is_relative())
				file = std::filesystem::current_path() / file;

			amlg_file_to_run = file.string();
			passthrough_params[0] = amlg_file_to_run;
		}
		else //add on to passthrough params
			passthrough_params.emplace_back(args[i]);
	}

#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
	Concurrency::SetMaxNumThreads(num_threads);
#endif

	if(debug_state)
		Interpreter::SetDebuggingState(true);

	if(debug_sources)
		asset_manager.debugSources = true;

	if(debug_minimal)
		asset_manager.debugMinimal = true;

	if(profile_opcodes)
		Interpreter::SetOpcodeProfilingState(true);

	if(profile_labels)
		Interpreter::SetLabelProfilingState(true);

	if(run_trace)
	{
		return RunAmalgamTrace(&std::cin, &std::cout, random_seed);
	}
	else if(run_tracefile)
	{
		std::ifstream trace_stream(tracefile);
		int ret = RunAmalgamTrace(&trace_stream, &std::cout, random_seed);

		if(profile_opcodes || profile_labels)
			PerformanceProfiler::PrintProfilingInformation(profile_out_file, profile_count);

		return ret;
	}
	else
	{
		//run the standard amlg command line interface
		EntityExternalInterface::LoadEntityStatus status;
		std::string file_type = "";
		Entity *entity = asset_manager.LoadEntityFromResourcePath(amlg_file_to_run, file_type,
			false, true, false, true, random_seed, nullptr, status);

		if(!status.loaded)
			return 1;

		asset_manager.SetRootPermission(entity, true);

		PrintListener *print_listener = nullptr;
		std::vector<EntityWriteListener *> write_listeners;

		if(Platform_IsDebuggerPresent())
		{
			print_listener = new PrintListener("out.txt", print_to_stdio);
		}
		else if(print_log_filename != "" || print_to_stdio)
		{
			print_listener = new PrintListener(print_log_filename, print_to_stdio);
		}

		if(write_log_filename != "")
		{
			EntityWriteListener *write_log = new EntityWriteListener(entity, false, write_log_filename);
			write_listeners.push_back(write_log);
		}

		//transform args into args variable
		EvaluableNode *call_stack = entity->evaluableNodeManager.AllocNode(ENT_LIST);
		EvaluableNode *args_node = entity->evaluableNodeManager.AllocNode(ENT_ASSOC);
		call_stack->AppendOrderedChildNode(args_node);

		//top-level stack variable holding argv
		args_node->SetMappedChildNode("argv", CreateListOfStringsFromIteratorAndFunction(passthrough_params,
			&entity->evaluableNodeManager, [](auto s) { return s; }));

		//top-level stack variable holding path to interpreter
		EvaluableNode *interpreter_node = entity->evaluableNodeManager.AllocNode(ENT_STRING);
		interpreter_node->SetStringValue(interpreter_path);
		args_node->SetMappedChildNode("interpreter", interpreter_node);

		//execute the entity
		entity->Execute(StringInternPool::NOT_A_STRING_ID, call_stack, false, nullptr,
			&write_listeners, print_listener);

		int return_val = 0;

		//detect memory leaks for debugging
		// the entity should have one reference left, which is the entity's code itself
		if(entity->evaluableNodeManager.GetNumberOfNodesReferenced() > 1)
		{
			auto &nr = entity->evaluableNodeManager.GetNodesReferenced();
			std::cerr << "Error: memory leak." << std::endl;

			if(debug_internal_memory)
			{
				std::cerr << "The following temporary nodes are still in use : " << std::endl;
				for(auto &[used_node, _] : nr.nodesReferenced)
				{
					std::cerr << "Item:" << std::endl;
					std::cerr << Parser::Unparse(used_node, &entity->evaluableNodeManager);
				}
			}

			return_val = -1;
		}

		if(profile_opcodes || profile_labels)
			PerformanceProfiler::PrintProfilingInformation(profile_out_file, profile_count);

		if(debug_internal_memory)
		{
			auto nodes_used = entity->evaluableNodeManager.GetNumberOfUsedNodes();
			auto nodes_free = entity->evaluableNodeManager.GetNumberOfUnusedNodes();
			std::cout << "Root Entity nodes in use: " << nodes_used << "/" << (nodes_used + nodes_free) << std::endl;
		}

		for(auto &ewl : write_listeners)
			delete ewl;
		if(print_listener != nullptr)
			delete print_listener;

		if(debug_internal_memory)
		{
			delete entity;

			auto num_strings_used = string_intern_pool.GetNumDynamicStringsInUse();
			//there should always at least be the empty string
			if(num_strings_used > 0)
			{
				std::cerr << "ERROR: Num strings still in use: " << num_strings_used << std::endl;
				std::vector<std::pair<std::string, int64_t>> in_use = string_intern_pool.GetNonStaticStringsInUse();
				for(auto &[s, count] : in_use)
					std::cerr << '"' << s << "\":" << count << std::endl;

				return_val = -1;
			}

			std::cout << "Memory reclaimation complete." << std::endl;
		}

		return return_val;
	}

	return 0;
}
