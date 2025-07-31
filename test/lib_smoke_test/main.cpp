//
// Test driver for Amalgam shared libraries (dll/so/dylib)
//

//project headers:
#include "Amalgam.h"

//system headers:
#include <functional>
#include <iostream>
#include <string>

// A wrapper around a C string that DeleteString() on exit.
class ApiString
{
	char *p;

public:
	ApiString(char *p) : p(p) {}
	ApiString(const ApiString &a) = delete;
	ApiString(ApiString &&a) : p(a.p)
	{
		a.p = nullptr;
	}

	~ApiString()
	{
		if(p != nullptr)
			DeleteString(p);
	}

	operator std::string() const
	{
		std::string s(p);
		return s;
	}
};

// A wrapper around an entity that DestroyEntity() on exit.
class LoadedEntity
{
	std::string h;

public:
	LoadedEntity(const std::string &handle) : h(handle) {}

	~LoadedEntity()
	{
		DestroyEntity(h.data());
	}

	const std::string &handle() const
	{
		return h;
	}
};

class TestResult
{
	std::string test;
	bool successful;

public:
	TestResult(const std::string &test) : test(test), successful(true) {}

	operator bool() const {
		return successful;
	}

	void Check(const std::string &action, const std::string &actual, const std::string &expected)
	{
		if(actual != expected)
		{
			std::cerr << test << ": " << action << " produced " << actual << " but expected " << expected << std::endl;
			successful = false;
		}
	}

	void Require(const std::string &action, bool actual)
	{
		if(!actual)
		{
			std::cerr << test << ": Failed to " << action << std::endl;
			successful = false;
		}
	}
};

class SuiteResult
{
	bool verbose;
	bool successful;

public:
	SuiteResult(bool verbose) : verbose(verbose), successful(true) {}

	operator bool() const
	{
		return successful;
	}

	void Run(const std::string &test, std::function<void(TestResult&)> f)
	{
		TestResult test_result(test);
		if(verbose)
			std::cout << test << std::endl;
		f(test_result);
		successful = successful && test_result;
	}
};

static void DumpVersion(TestResult &test_result)
{
	ApiString version(GetVersionString());
	std::cout << static_cast<std::string>(version) << std::endl;
}

static void LoadAndEval(TestResult &test_result)
{
	// Load+execute+delete entity:
	char handle[] = "1";
	char* file = (char*)"test.amlg";
	char file_type[] = "";
	char json_file_params[] = "";
	char write_log[] = "";
	char print_log[] = "";
	auto status = LoadEntity(handle, file, file_type, false, json_file_params, write_log, print_log);
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);

		char label[] = "test";
		ExecuteEntity(handle, label);

		std::string amlg("(size (contained_entities))");
		ApiString result(EvalOnEntity(handle, amlg.data()));
		test_result.check("EvalOnEntity", result, "24");
	}
}

static std::string handle("handle");
static std::string handle2("handle2");
static std::string filename("counter.amlg");
static std::string filename2("counter2.amlg");
static std::string empty("");
static std::string initialize("initialize");
static std::string add("add");
static std::string get_value("get_value");
static std::string increment("increment");

static void InitializeCounter(TestResult &test_result)
{
    LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());
		ApiString result(ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data()));
		test_result.check("ExecuteEntityJsonPtr", result, "0");
	}
}

static void ExecuteEntityJsonWithValue(TestResult &test_result)
{
    LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());
		std::string json("{\"count\":2}");
		ApiString result(ExecuteEntityJsonPtr(handle.data(), add.data(), json.data()));
		test_result.check("ExecuteEntityJsonPtr", result, "2");
	}
}

static void ExecuteEntityJsonLogged(TestResult &test_result)
{
    LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());
	    ResultWithLog result = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json(result.json);
		ApiString log(result.log);
		test_result.check("ExecuteEntityJsonPtrLogged json", json, "1");
		test_result.check("ExecuteEntityJsonPtrLogged log", log, "(seq (accum_to_entities {!value 1}))");
	}
}

static void ExecuteEntityJsonLoggedUpdating(TestResult &test_result)
{
    LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		ApiString one(ExecuteEntityJsonPtr(handle.data(), increment.data(), empty.data()));
		test_result.check("ExecuteEntityJson", one, "1");

	    ResultWithLog result = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json(result.json);
		ApiString log(result.log);
		test_result.check("ExecuteEntityJsonPtrLogged json", json, "2");
		test_result.check("ExecuteEntityJsonPtrLogged log", log, "(seq (accum_to_entities {!value 1}))");
	}
}

static void ExecuteEntityJsonLoggedRoundTrip(TestResult &test_result)
{
    LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		// Increment the counter, getting a log.
	    ResultWithLog result = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json(result.json);
		ApiString log(result.log);
		test_result.check("ExecuteEntityJsonPtrLogged json", json, "1");

		// Reset the entity and replay the log.  We should get the same result back from the state.
		ExecuteEntity(handle.data(), initialize.data());
		EvalOnEntity(handle.data(), result.log);
		ApiString gotten(ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data()));
		test_result.check("ExecuteEntityJsonPtr get_value", gotten, "1");
	}
}

static void ExecuteEntityJsonLoggedTwice(TestResult &test_result)
{
    LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		// Increment the counter, getting a log.
	    ResultWithLog result1 = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json1(result1.json);
		ApiString log1(result1.log);
		test_result.check("ExecuteEntityJsonPtrLogged json", json1, "1");

		// Again.
	    ResultWithLog result2 = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json2(result2.json);
		ApiString log2(result2.log);
		test_result.check("ExecuteEntityJsonPtrLogged json", json2, "2");

		// Reset the entity and replay both logs.  We should get the same result back from the state.
		ExecuteEntity(handle.data(), initialize.data());
		EvalOnEntity(handle.data(), result1.log);
		EvalOnEntity(handle.data(), result2.log);
		ApiString gotten(ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data()));
		test_result.check("ExecuteEntityJsonPtr get_value", gotten, "2");
	}
}

static void ExecuteCounter2(TestResult &test_result)
{
    LoadEntityStatus status = LoadEntity(handle.data(), filename2.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		std::string json("{}");
		ApiString result(ExecuteEntityJsonPtr(handle.data(), add.data(), json.data()));
		test_result.check("ExecuteEntityJsonPtr add", result, "1");

		std::string json2("{\"counter\":\"y\"}");
		ApiString result2(ExecuteEntityJsonPtr(handle.data(), get_value.data(), json2.data()));
		test_result.check("ExecuteEntityJsonPtr get_value y", result2, "\0(null)");
	}
}

static void ExecuteCounter2Logged(TestResult &test_result)
{
	// This is actually a test for a specific case of accum_entity_roots, via
	// ExecuteEntityJsonPtrLogged(), that needs to preserve labels.
    LoadEntityStatus status = LoadEntity(handle.data(), filename2.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
	test_result.require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		// Clone the entity, then execute "add" there.
		// Of note this accum_entity_roots, adding a label.
		bool cloned = CloneEntity(handle.data(), handle2.data(), empty.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
		test_result.require("CloneEntity", cloned);

		ResultWithLog result = ExecuteEntityJsonPtrLogged(handle2.data(), add.data(), empty.data());
		ApiString json1(result.json);
		ApiString log1(result.log);
		test_result.check("ExecuteEntityJsonPtrLogged add", json1, "1");

		EvalOnEntity(handle.data(), result.log);

		std::string json2 = ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data());
		test_result.check("ExecuteEntityJsonPtr get_value", json2, "1");
	}
}

int main(int argc, char* argv[])
{
	bool verbose = false;

	for(int i = 1; i < argc; i++)
	{
		if(std::string("--help") == argv[i] || std::string("-h") == argv[i])
		{
			std::cout << "Usage: " << argv[0] << " [-h] [-v]" << std::endl
			<< std::endl
			<< "Options:" << std::endl
			<< "  --help, -h     Print this help message" << std::endl
			<< "  --verbose, -v  Print each test name as it executes" << std::endl;
			return 0;
		}
		else if(std::string("--verbose") == argv[i] || std::string("-v") == argv[i])
		{
			verbose = true;
		}
		else
		{
			std::cerr << argv[0] << ": unrecognized option " << argv[i] << std::endl;
			return 1;
		}
	}

	SuiteResult suite(verbose);
	suite.run("DumpVersion", DumpVersion);
	suite.run("LoadAndEval", LoadAndEval);
	suite.run("InitializeCounter", InitializeCounter);
	suite.run("ExecuteEntityJsonWithValue", ExecuteEntityJsonWithValue);
	suite.run("ExecuteEntityJsonLogged", ExecuteEntityJsonLogged);
	suite.run("ExecuteEntityJsonLoggedUpdating", ExecuteEntityJsonLoggedUpdating);
	suite.run("ExecuteEntityJsonLoggedRoundTrip", ExecuteEntityJsonLoggedRoundTrip);
	suite.run("ExecuteEntityJsonLoggedTwice", ExecuteEntityJsonLoggedTwice);
	suite.run("ExecuteCounter2", ExecuteCounter2);
	suite.run("ExecuteCounter2Logged", ExecuteCounter2Logged);

	return suite ? 0 : 1;
}
