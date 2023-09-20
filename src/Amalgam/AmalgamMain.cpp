//project headers: 
#include "Amalgam.h"
#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "Concurrency.h"
#include "Entity.h"
#include "EntityExternalInterface.h"
#include "EntityQueries.h"
#include "EntityQueryManager.h"
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

int RunAmalgamTrace(std::istream *in_stream, std::ostream *out_stream, std::string &random_seed);

void PrintProfilingInformationIfApplicable()
{
	if(performance_profiler.IsProfilingEnabled())
	{
		size_t max_num_perf_counters_to_display = 20;
		std::cout << "Operations that took the longest total time (s): " << std::endl;
		auto longest_total_time = performance_profiler.GetNumCallsByTotalTime();
		for(size_t i = 0; i < max_num_perf_counters_to_display && i < longest_total_time.size(); i++)
			std::cout << longest_total_time[i].first << ": " << longest_total_time[i].second << std::endl;
		std::cout << std::endl;

		std::cout << "Operations called the most number of times: " << std::endl;
		auto most_calls = performance_profiler.GetNumCallsByType();
		for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_calls.size(); i++)
			std::cout << most_calls[i].first << ": " << most_calls[i].second << std::endl;
		std::cout << std::endl;

		std::cout << "Operations that took the longest average time (s): " << std::endl;
		auto longest_ave_time = performance_profiler.GetNumCallsByAveTime();
		for(size_t i = 0; i < max_num_perf_counters_to_display && i < longest_ave_time.size(); i++)
			std::cout << longest_ave_time[i].first << ": " << longest_ave_time[i].second << std::endl;
		std::cout << std::endl;

		std::cout << "Operations that increased the memory usage the most in total (nodes): " << std::endl;
		auto most_total_memory = performance_profiler.GetNumCallsByTotalMemoryIncrease();
		for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_total_memory.size(); i++)
			std::cout << most_total_memory[i].first << ": " << most_total_memory[i].second << std::endl;
		std::cout << std::endl;

		std::cout << "Operations that increased the memory usage the most on average (nodes): " << std::endl;
		auto most_ave_memory = performance_profiler.GetNumCallsByAveMemoryIncrease();
		for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_ave_memory.size(); i++)
			std::cout << most_ave_memory[i].first << ": " << most_ave_memory[i].second << std::endl;
		std::cout << std::endl;

		std::cout << "Operations that decreased the memory usage the most in total (nodes): " << std::endl;
		for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_total_memory.size(); i++)
		{
			//only write out those that had a net decrease
			double mem_delta = most_total_memory[most_total_memory.size() - 1 - i].second;
			if(mem_delta >= 0)
				break;
			std::cout << most_total_memory[i].first << ": " << mem_delta << std::endl;
		}
		std::cout << std::endl;

		std::cout << "Operations that decreased the memory usage the most on average (nodes): " << std::endl;
		for(size_t i = 0; i < max_num_perf_counters_to_display && i < most_ave_memory.size(); i++)
		{
			//only write out those that had a net decrease
			double mem_delta = most_ave_memory[most_total_memory.size() - 1 - i].second;
			if(mem_delta >= 0)
				break;
			std::cout << most_total_memory[i].first << ": " << mem_delta << std::endl;
		}
		std::cout << std::endl;

		std::cout << "Total number of operations: " << performance_profiler.GetTotalNumCalls() << std::endl;

		auto [total_mem_increase, positive_mem_increase] = performance_profiler.GetTotalAndPositiveMemoryIncreases();
		std::cout << "Net number of nodes allocated: " << total_mem_increase << std::endl;
		std::cout << "Total node increases: " << positive_mem_increase << std::endl;
	}
}

PLATFORM_MAIN_CONSOLE
{
	PLATFORM_ARGS_CONSOLE;

	if(args.size() == 1)
	{
		std::cout
			<< "Concurrency type: " << GetConcurrencyTypeString() << std::endl
			<< "Must specify an input file.  Flags:" << std::endl
			<< "-l [filename]: specify a debug log file." << std::endl
		#if defined(INTERPRETER_PROFILE_OPCODES) || defined(INTERPRETER_PROFILE_LABELS_CALLED)
			<< "-p: display engine performance counters upon completion" << std::endl
		#endif
			<< "-s [random number seed]: specify a particular random number seed -- can be any alphanumeric string." << std::endl
			<< "-t [filename]: specify a code-based transaction log file." << std::endl
		#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
			<< "--numthreads [number]: maximum number of threads to use (if unspecified or set to zero, may use unlimited)." << std::endl
		#endif
			<< "--debug: when specified, begins in debugging mode." << std::endl
			<< "--debug-minimal: when specified, begins in debugging mode with minimal output while stepping." << std::endl
			<< "--debug-sources: when specified, prepends all node comments with the source of the node when applicable." << std::endl
			<< "--nosbfds: disables the sbfds acceleration, which is generally preferred in the heuristics." << std::endl
			<< "--trace: uses commands via stdio to act as if it were being called as a library." << std::endl
			<< "--tracefile [file]: like trace, but pulls the data from the file specified." << std::endl
			<< "--version: prints the current version." << std::endl;
		return 0;
	}

	//run options
	bool debug_state = false;
	bool debug_minimal = false;
	bool debug_sources = false;
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
		if(args[i] == "-l" && i + 1 < args.size())
		{
			print_log_filename = args[++i];
		}
	#if defined(INTERPRETER_PROFILE_OPCODES) || defined(INTERPRETER_PROFILE_LABELS_CALLED)
		else if(args[i] == "-p")
			performance_profiler.EnableProfiling();
	#endif
		else if(args[i] == "-q")
			print_to_stdio = false;
		else if(args[i] == "-s" && i + 1 < args.size())
			random_seed = args[++i];
		else if(args[i] == "-t" && i + 1 < args.size())
			write_log_filename = args[++i];
	#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
		else if(args[i] == "--numthreads")
			num_threads = static_cast<size_t>(std::max(std::atoi(args[++i].data()), 0));
	#endif
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
		else if(args[i] == "--version")
			std::cout << "Amalgam Version: " << AMALGAM_VERSION_STRING << std::endl;
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

	if(run_trace)
	{
		return RunAmalgamTrace(&std::cin, &std::cout, random_seed);
	}
	else if(run_tracefile)
	{
		std::istream *trace_stream = new std::ifstream(tracefile);
		int ret = RunAmalgamTrace(trace_stream, &std::cout, random_seed);
		delete trace_stream;

		PrintProfilingInformationIfApplicable();
		return ret;
	}
	else
	{
		//run the standard amlg command line interface
		std::string file_type = "";
		Entity *entity = asset_manager.LoadEntityFromResourcePath(amlg_file_to_run, file_type, false, true, false, true, random_seed);
		if(entity == nullptr)
			return 0;

		asset_manager.SetRootPermission(entity, true);

		ExecutionCycleCount num_steps_executed = 0;
		size_t num_nodes_allocated = 0;
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
		entity->Execute(0, num_steps_executed, 0, num_nodes_allocated, &write_listeners, print_listener, call_stack);

		//clean up the nodes created here
		entity->evaluableNodeManager.FreeNodeTree(call_stack);

		//detect memory leaks for debugging
		// the entity should have one reference left, which is the entity's code itself
		if(entity->evaluableNodeManager.GetNumberOfNodesReferenced() > 1)
		{
			auto &temp_used_nodes = entity->evaluableNodeManager.GetNodesReferenced();
			std::cerr << "Error: memory leak." << std::endl;

			if(Platform_IsDebuggerPresent())
			{
				std::cerr << "The following temporary nodes are still in use : " << std::endl;
				for(auto &[used_node, _] : temp_used_nodes)
				{
					std::cerr << "Item:" << std::endl;
					std::cerr << Parser::Unparse(used_node, &entity->evaluableNodeManager);
				}
			}
		}

		PrintProfilingInformationIfApplicable();

		if(Platform_IsDebuggerPresent())
		{
			auto nodes_used = entity->evaluableNodeManager.GetNumberOfUsedNodes();
			auto nodes_free = entity->evaluableNodeManager.GetNumberOfUnusedNodes();
			std::cout << "Root Entity nodes in use: " << nodes_used << "/" << (nodes_used + nodes_free) << std::endl;
		}

		for(auto &ewl : write_listeners)
			delete ewl;
		if(print_listener != nullptr)
			delete print_listener;

		if(Platform_IsDebuggerPresent())
		{
			delete entity;

			auto num_strings_used = string_intern_pool.GetNumDynamicStringsInUse();
			//there should always at least be the empty string
			if(num_strings_used > 0)
			{
				std::cerr << "ERROR: Num strings still in use: " << num_strings_used << std::endl;
				std::vector<std::string> in_use = string_intern_pool.GetNonStaticStringsInUse();
				for(auto &s : in_use)
					std::cerr << '"' << s << '"' << std::endl;
			}

			std::cout << "Memory reclaimation complete." << std::endl;
		}

		return 0;
	}
}
