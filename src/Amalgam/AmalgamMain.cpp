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
int32_t RunAmalgamTrace(std::istream *in_stream, std::ostream *out_stream, std::string &rand_seed);

//usage:
// Note: spaces in raw string are correct, do not replace with tabs
static std::string GetUsage()
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

    --permissions [permissions]
                     Sets the permission for the file being run.  By default all permissions are granted.
                     Permissions is a string that can consist of +xyz... or -xyz..., where x, y, and z are
                     letters that correspond to each permission.  If it starts with a +, then it assumes
                     no permissions and adds those, if it starts with a - it assumes all permissions are set
                     and removes those listed.  The letters for each permission are:
                         o: std_out_and_std_err
                         i: std_in
                         l: load
                         s: store
                         e: environment
                         a: alter_performance
                         x: system (e[x]ecute)
                     For example, -xe would yield all permissions but remove environment and system permissions,
                     whereas +io would only allow console input and output

    --debug          When specified, begins in debugging mode

    --debug-minimal  When specified, begins in debugging mode with minimal output while stepping

    --debug-sources  When specified, prepends all node comments with the source of the node when applicable

    --warn-on-undefined
                     When specified, amalgam will emit warnings for undefined variables

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

//parses the permissions string and returns the permissions parsed
static EntityPermissions ParsePermissionsCommandLineParam(std::string_view permissions_str)
{
	if(permissions_str.empty())
		return EntityPermissions::AllPermissions();

	//start with no permissions, but if removing permissions, then start with all
	EntityPermissions permissions;
	bool add_permissions = true;
	size_t permission_letters_start = 0;
	if(permissions_str[0] == '+')
	{
		permission_letters_start++;
	}
	else if(permissions_str[0] == '-')
	{
		permissions = EntityPermissions::AllPermissions();
		add_permissions = false;
		permission_letters_start++;
	}

	// Iterate over the permission characters in the input string
	for(size_t i = permission_letters_start; i < permissions_str.size(); i++)
	{
		switch(permissions_str[i])
		{
		case 'o': permissions.individualPermissions.stdOutAndStdErr		= add_permissions;	break;
		case 'i': permissions.individualPermissions.stdIn				= add_permissions;	break;
		case 'l': permissions.individualPermissions.load				= add_permissions;	break;
		case 's': permissions.individualPermissions.store				= add_permissions;	break;
		case 'e': permissions.individualPermissions.environment			= add_permissions;	break;
		case 'a': permissions.individualPermissions.alterPerformance	= add_permissions;	break;
		case 'x': permissions.individualPermissions.system				= add_permissions;	break;
		default:  std::cerr << "Invalid permission character: '" << permissions_str[i] << "'" << std::endl;
		}
	}

	return permissions;
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
//TODO 24709: remove this

	using Map = ConcurrentFastHashMap<std::string, std::unique_ptr<int>>;
	Map m;
	assert(m.empty());
	assert(m.size() == 0);

	// ---------- 2. insert & size ----------
	{
		auto p1 = m.insert({ "one", std::make_unique<int>(1) });
		assert(p1.second);                     // inserted
	}
	assert(m.size() == 1);
	assert(!m.empty());

	// inserting the same key again must not insert
	{
		auto p2 = m.insert({ "one", std::make_unique<int>(42) });
		assert(!p2.second);                    // not inserted
	}
	assert(m.size() == 1);                 // size unchanged

	// ---------- 3. emplace ----------
	{
		auto p3 = m.emplace("two", std::make_unique<int>(2));
		assert(p3.second);
	}
	assert(m.size() == 2);

	// ---------- 4. find ----------
	{
		auto it = m.find("one");
		assert(it != m.end());                 // found
		assert(*(it->second) == 1);            // value correct
	}

	{
		auto cit = static_cast<const Map &>(m).find("two");
		assert(cit != m.end());
		assert(*(cit->second) == 2);
	}

	{
		// ---------- 5. operator[] ----------
		m["three"] = std::make_unique<int>(3);
		assert(m.size() == 3);
		assert(*(m["three"]) == 3);
	}

	// ---------- 6. at ----------
	try
	{
		int val = *m.at("three");
		assert(val == 3);
	}
	catch(...)
	{
		assert(false);
	}

	bool threw = false;
	try
	{
		m.at("nonexistent");
	}
	catch(std::out_of_range &)
	{
		threw = true;
	}
	assert(threw);                         // at must throw on missing key

	// ---------- 7. iteration ----------
	{
		std::size_t count = 0;
		for(auto &kv : m)
		{
			++count;
			// each iteration holds the shard lock – we can safely read
			assert(!kv.first.empty());
			assert(kv.second != nullptr);
		}
		assert(count == m.size());
	}

	// ---------- 8. erase ----------
	{
		std::size_t erased = m.erase("two");
		assert(erased == 1);
		assert(m.size() == 2);
		assert(m.find("two") == m.end());
	}

	{
		// erase via iterator
		auto itErase = m.find("one");
		assert(itErase != m.end());
		auto nextIt = m.erase(itErase);
		// nextIt may be end() or point to the next element; both are fine
		assert(nextIt == m.end() || nextIt->first != "one");
		assert(m.size() == 1);
	}

	// ---------- 9. clear ----------
	m.clear();
	assert(m.empty());
	assert(m.size() == 0);
	assert(m.begin() == m.end());

	// ---------- 10. concurrent access sanity ----------
	{
		Map concurrentMap;
		std::thread t1([&] {
			for(int i = 0; i < 1000; ++i)
				concurrentMap.emplace("t1_" + std::to_string(i),
									 std::make_unique<int>(i));
		});
		std::thread t2([&] {
			for(int i = 0; i < 1000; ++i)
				concurrentMap.emplace("t2_" + std::to_string(i),
									 std::make_unique<int>(i));
		});
		t1.join(); t2.join();

		assert(concurrentMap.size() == 2000);
		// spot‑check a few keys
		assert(*(concurrentMap.find("t1_42")->second) == 42);
		assert(*(concurrentMap.find("t2_999")->second) == 999);
	}

	//run options
	bool debug_state = false;
	bool debug_minimal = false;
	bool debug_sources = false;
	bool warn_on_undefined = false;
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
	EntityPermissions entity_permissions = EntityPermissions::AllPermissions();

	std::string rand_seed;
	if(Platform_IsDebuggerPresent())
		rand_seed = "01234567890123456789012345";

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
			rand_seed = args[++i];
		else if(args[i] == "-t" && i + 1 < args.size())
			write_log_filename = args[++i];
		else if(args[i] == "--p-opcodes")
			profile_opcodes = true;
		else if(args[i] == "--p-labels")
			profile_labels = true;
		else if(args[i] == "--p-count" && i + 1 < args.size())
			profile_count = std::max<size_t>(std::atoi(args[++i].data()), 0);
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
		else if(args[i] == "--warn-on-undefined")
			warn_on_undefined = true;
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
		else if(args[i] == "--permissions" && i + 1 < args.size())
		{
			entity_permissions = ParsePermissionsCommandLineParam(args[++i]);
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

	asset_manager.warnOnUndefined = warn_on_undefined;

	if(debug_minimal)
		asset_manager.debugMinimal = true;

	if(profile_opcodes)
		Interpreter::SetOpcodeProfilingState(true);

	if(profile_labels)
		Interpreter::SetLabelProfilingState(true);

	if(rand_seed.empty())
	{
		rand_seed.resize(RandomStream::randStateStringifiedSizeInBytes);
		Platform_GenerateSecureRandomData(rand_seed.data(), RandomStream::randStateStringifiedSizeInBytes);
	}

	if(run_trace)
	{
		return RunAmalgamTrace(&std::cin, &std::cout, rand_seed);
	}
	else if(run_tracefile)
	{
		std::ifstream trace_stream(tracefile);
		int return_val = RunAmalgamTrace(&trace_stream, &std::cout, rand_seed);

		if(profile_opcodes || profile_labels)
			PerformanceProfiler::PrintProfilingInformation(profile_out_file, profile_count);

		return return_val;
	}
	else
	{
		//run the standard amlg command line interface
		EntityExternalInterface::LoadEntityStatus status;
		AssetManager::AssetParametersRef asset_params
			= std::make_shared<AssetManager::AssetParameters>(amlg_file_to_run, "", true);

		Entity *entity = asset_manager.LoadEntityFromResource(asset_params, false, rand_seed, nullptr, status);

		if(!status.loaded)
			return 1;

		entity->SetPermissions(EntityPermissions::AllPermissions(), entity_permissions, true);

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
			std::unique_ptr<std::ostream> log_file = std::make_unique<std::ofstream>(write_log_filename, std::ios::binary);
			EntityWriteListener *write_log = new EntityWriteListener(entity, std::move(log_file), false, false, false);
			write_listeners.push_back(write_log);
		}

		//transform args into args variable
		EvaluableNode *scope_stack = entity->evaluableNodeManager.AllocNode(ENT_LIST);
		EvaluableNode *args_node = entity->evaluableNodeManager.AllocNode(ENT_ASSOC);
		scope_stack->AppendOrderedChildNode(args_node);

		//top-level stack variable holding argv
		args_node->SetMappedChildNode("argv", CreateListOfStringsFromIteratorAndFunction(passthrough_params,
			&entity->evaluableNodeManager, [](auto s) { return s; }));

		//set need cycle check because things may be assigned
		scope_stack->SetNeedCycleCheck(true);
		args_node->SetNeedCycleCheck(true);

		//top-level stack variable holding path to interpreter
		EvaluableNode *interpreter_node = entity->evaluableNodeManager.AllocNode(ENT_STRING);
		interpreter_node->SetStringValue(interpreter_path);
		args_node->SetMappedChildNode("interpreter", interpreter_node);

		//execute the entity
		entity->Execute(StringInternPool::NOT_A_STRING_ID, scope_stack, false, nullptr,
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
					std::cerr << Parser::Unparse(used_node);
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
		#ifndef AMALGAM_FAST_MEMORY_INTEGRITY
			//AMALGAM_FAST_MEMORY_INTEGRITY already calls VerifyEvaluableNodeIntegrity on entity destruction
			entity->VerifyEvaluableNodeIntegrityAndAllContainedEntities();
		#endif

			delete entity;

			auto num_strings_used = string_intern_pool.GetNumDynamicStringsInUse();
			//there should always at least be the empty string
			if(num_strings_used > 0)
			{
				std::cerr << "ERROR: Num strings still in use: " << num_strings_used << std::endl;
				std::vector<std::pair<std::string, size_t>> in_use = string_intern_pool.GetDynamicStringsInUse();
				for(auto &[s, count] : in_use)
					std::cerr << '"' << s << "\":" << count << std::endl;

				return_val = -1;
			}

			std::cout << "Memory reclamation complete." << std::endl;
		}

		return return_val;
	}

	return 0;
}
