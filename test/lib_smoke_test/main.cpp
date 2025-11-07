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
	ApiString(char *p) : p(p)
	{}
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
	LoadedEntity(const std::string &handle) : h(handle)
	{}

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
	TestResult(const std::string &test) : test(test), successful(true)
	{}

	operator bool() const
	{
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
	SuiteResult(bool verbose) : verbose(verbose), successful(true)
	{}

	operator bool() const
	{
		return successful;
	}

	void Run(const std::string &test, std::function<void(TestResult &)> f)
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
	char *file = (char *)"test.amlg";
	char file_type[] = "";
	char json_file_params[] = "";
	char write_log[] = "";
	char print_log[] = "";
	auto status = LoadEntity(handle, file, file_type, false, json_file_params, write_log, print_log, nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);

		char label[] = "test";
		ExecuteEntity(handle, label);

		std::string amlg("(size (contained_entities))");
		ApiString result(EvalOnEntity(handle, amlg.data()));
		test_result.Check("EvalOnEntity", result, "24");
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
static std::string amlgSuffix("amlg");
static std::string camlSuffix("caml");
// This string shows up at the start of persisted entities
static std::string declare("(declare\r\n\t{create_new_entity .true");


static void InitializeCounter(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());
		ApiString result(ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data()));
		test_result.Check("ExecuteEntityJsonPtr", result, "0");
	}
}

static void ExecuteEntityJsonWithValue(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());
		std::string json("{\"count\":2}");
		ApiString result(ExecuteEntityJsonPtr(handle.data(), add.data(), json.data()));
		test_result.Check("ExecuteEntityJsonPtr", result, "2");
	}
}

static void ExecuteEntityJsonLogged(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());
		ResultWithLog result = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json(result.json);
		ApiString log(result.log);
		test_result.Check("ExecuteEntityJsonPtrLogged json", json, "1");
		test_result.Check("ExecuteEntityJsonPtrLogged log", log, "(seq (accum_to_entities {!value 1}))");
	}
}

static void ExecuteEntityJsonLoggedUpdating(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		ApiString one(ExecuteEntityJsonPtr(handle.data(), increment.data(), empty.data()));
		test_result.Check("ExecuteEntityJson", one, "1");

		ResultWithLog result = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json(result.json);
		ApiString log(result.log);
		test_result.Check("ExecuteEntityJsonPtrLogged json", json, "2");
		test_result.Check("ExecuteEntityJsonPtrLogged log", log, "(seq (accum_to_entities {!value 1}))");
	}
}

static void ExecuteEntityJsonLoggedRoundTrip(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		// Increment the counter, getting a log.
		ResultWithLog result = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json(result.json);
		ApiString log(result.log);
		test_result.Check("ExecuteEntityJsonPtrLogged json", json, "1");

		// Reset the entity and replay the log.  We should get the same result back from the state.
		ExecuteEntity(handle.data(), initialize.data());
		EvalOnEntity(handle.data(), result.log);
		ApiString gotten(ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data()));
		test_result.Check("ExecuteEntityJsonPtr get_value", gotten, "1");
	}
}

static void ExecuteEntityJsonLoggedTwice(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		// Increment the counter, getting a log.
		ResultWithLog result1 = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json1(result1.json);
		ApiString log1(result1.log);
		test_result.Check("ExecuteEntityJsonPtrLogged json", json1, "1");

		// Again.
		ResultWithLog result2 = ExecuteEntityJsonPtrLogged(handle.data(), increment.data(), empty.data());
		ApiString json2(result2.json);
		ApiString log2(result2.log);
		test_result.Check("ExecuteEntityJsonPtrLogged json", json2, "2");

		// Reset the entity and replay both logs.  We should get the same result back from the state.
		ExecuteEntity(handle.data(), initialize.data());
		EvalOnEntity(handle.data(), result1.log);
		EvalOnEntity(handle.data(), result2.log);
		ApiString gotten(ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data()));
		test_result.Check("ExecuteEntityJsonPtr get_value", gotten, "2");
	}
}

static void ExecuteCounter2(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename2.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		std::string json("{}");
		ApiString result(ExecuteEntityJsonPtr(handle.data(), add.data(), json.data()));
		test_result.Check("ExecuteEntityJsonPtr add", result, "1");

		std::string json2("{\"counter\":\"y\"}");
		ApiString result2(ExecuteEntityJsonPtr(handle.data(), get_value.data(), json2.data()));
		test_result.Check("ExecuteEntityJsonPtr get_value y", result2, "\0(null)");
	}
}

static void ExecuteCounter2Logged(TestResult &test_result)
{
	// This is actually a test for a specific case of accum_entity_roots, via
	// ExecuteEntityJsonPtrLogged(), that needs to preserve labels.
	LoadEntityStatus status = LoadEntity(handle.data(), filename2.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		// Clone the entity, then execute "add" there.
		// Of note this accum_entity_roots, adding a label.
		bool cloned = CloneEntity(handle.data(), handle2.data(), empty.data(), empty.data(), false, empty.data(), empty.data(), empty.data());
		test_result.Require("CloneEntity", cloned);

		ResultWithLog result = ExecuteEntityJsonPtrLogged(handle2.data(), add.data(), empty.data());
		ApiString json1(result.json);
		ApiString log1(result.log);
		test_result.Check("ExecuteEntityJsonPtrLogged add", json1, "1");

		EvalOnEntity(handle.data(), result.log);

		std::string json2 = ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data());
		test_result.Check("ExecuteEntityJsonPtr get_value", json2, "1");
	}
}

static void TestLoadEntityFromMemory(TestResult &test_result)
{
	std::string amlg("(null #get_value \"hello\")");
	LoadEntityStatus status = LoadEntityFromMemory(handle.data(), amlg.data(), amlg.size(), amlgSuffix.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntityFromMemory", status.loaded);
	test_result.Require("LoadEntityFromMemory null entity_path", status.entity_path == nullptr);
	test_result.Require("LoadEntityFromMemory zero entity_path_len", status.entity_path_len == 0);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		std::string result = ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data());
		test_result.Check("ExecuteEntityJsonPtr", result, "\"hello\"");
	}
}

static void LoadSubEntityFromMemory(TestResult &test_result)
{
	LoadEntityStatus status = LoadEntity(handle.data(), filename2.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	test_result.Require("LoadEntity null entity_path", status.entity_path == nullptr);
	test_result.Require("LoadEntity zero entity_path_len", status.entity_path_len == 0);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		std::string amlg("(list #x 17)");
		const char *entityPaths[] = { "test" };
		status = LoadEntityFromMemory(handle.data(), amlg.data(), amlg.size(), amlgSuffix.data(), false, empty.data(), empty.data(), empty.data(), entityPaths, 1);
		test_result.Require("LoadEntityFromMemory", status.loaded);
		if(test_result)
		{
			test_result.Require("LoadEntityFromMemory non-null entity_path", status.entity_path != nullptr);
			test_result.Require("LoadEntityFromMemory one entity_path_len", status.entity_path_len == 1);
			if (test_result) {
				test_result.Check("LoadEntityFromMemory first entity_path", status.entity_path[0], "test");
			}
			std::string input("{\"id\": \"test\"}");
			std::string json = ExecuteEntityJsonPtr(handle.data(), get_value.data(), input.data());
			test_result.Check("ExecuteEntityJsonPtr get_value", json, "17");
		}
	}
}

static void LoadSubSubEntityFromMemory(TestResult &test_result) {
	std::string amlg("(list #x 17)");
	LoadEntityStatus status = LoadEntityFromMemory(handle.data(), amlg.data(), amlg.size(), amlg.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntityFromMemory root", status.loaded);
	if (test_result) {
		const char *entityPaths[] = { "test", "sub" };
		// 1. Loading just {test} produces just {test}
		LoadEntityStatus status1 = LoadEntityFromMemory(handle.data(), amlg.data(), amlg.size(), amlg.data(), false, empty.data(), empty.data(), empty.data(), entityPaths, 1);
		test_result.Require("LoadEntityFromMemory test1", status1.loaded);
		test_result.Require("LoadEntityFromMemory test1 ep", status1.entity_path != nullptr);
		test_result.Require("LoadEntityFromMemory test1 epl", status1.entity_path_len == 1);
		if (test_result) {
			test_result.Check("LoadEntityFromMemory test1 ep value", status1.entity_path[0], "test");
		}
		// 2. Loading just {test} when it already exists produces {test, _12345}
		LoadEntityStatus status2 = LoadEntityFromMemory(handle.data(), amlg.data(), amlg.size(), amlg.data(), false, empty.data(), empty.data(), empty.data(), entityPaths, 1);
		test_result.Require("LoadEntityFromMemory test2", status2.loaded);
		test_result.Require("LoadEntityFromMemory test2 ep", status2.entity_path != nullptr);
		test_result.Require("LoadEntityFromMemory test2 epl", status2.entity_path_len == 2);
		if (test_result) {
			test_result.Check("LoadEntityFromMemory test2 ep value", status2.entity_path[0], "test");
			test_result.Require("LoadEntityFromMemory test2 value2", status2.entity_path[1][0] == '_');
		}
		// 3. Loading {test, sub} produces matching {test, sub}
		LoadEntityStatus status3 = LoadEntityFromMemory(handle.data(), amlg.data(), amlg.size(), amlg.data(), false, empty.data(), empty.data(), empty.data(), entityPaths, 2);
		test_result.Require("LoadEntityFromMemory test3", status3.loaded);
		test_result.Require("LoadEntityFromMemory test3 ep", status3.entity_path != nullptr);
		test_result.Require("LoadEntityFromMemory test3 epl", status3.entity_path_len == 2);
		if (test_result) {
			test_result.Check("LoadEntityFromMemory test3 ep value1", status3.entity_path[0], "test");
			test_result.Check("LoadEntityFromMemory test3 ep value2", status3.entity_path[1], "sub");
		}
	}
}

static void TestStoreEntityToMemory(TestResult &test_result)
{
	// round-trip the trivial entity from TestLoadEntityFromMemory()
	std::string amlg("(null #get_value \"hello\")");
	LoadEntityStatus status = LoadEntityFromMemory(handle.data(), amlg.data(), amlg.size(), amlgSuffix.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntityFromMemory", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		void *data = nullptr;
		size_t len = 0;
		StoreEntityToMemory(handle.data(), &data, &len, amlgSuffix.data(), false, empty.data(), nullptr, 0);
		char *cdata = reinterpret_cast<char *>(data);
		std::string result(cdata, cdata + len);
		// The actual result is very (very?) long and includes some boilerplate
		test_result.Check("StoreEntityToMemory (prolog)", result.substr(0, declare.size()), declare);
		test_result.Require("limit StoreEntityToMemory output to a reasonable size", result.size() < 4096);
	}
}

static void StoreSubEntityToMemory(TestResult &test_result)
{
	// Do the same thing as ExecuteCounter2(), which stores the data in an
	// embedded entity; then retrieve that entity.
	LoadEntityStatus status = LoadEntity(handle.data(), filename2.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		std::string amlg("(retrieve_from_entity \"!id\")");
		std::string idstr = EvalOnEntity(handle.data(), amlg.data());
		test_result.Require("ID string is not empty", idstr.size() >= 2);
		test_result.Require("ID string starts with a quote", idstr.front() == '"');
		test_result.Require("ID string ends with a quote", idstr.back() == '"');
		idstr.pop_back();
		idstr.erase(0, 1);

		ApiString result(ExecuteEntityJsonPtr(handle.data(), add.data(), empty.data()));
		test_result.Check("ExecuteEntityJsonPtr add", result, "1");

		void *data = nullptr;
		size_t len = 0;
		const char *entityPaths[] = { idstr.data() };
		StoreEntityToMemory(handle.data(), &data, &len, amlgSuffix.data(), false, empty.data(), entityPaths, 1);
		char *cdata = reinterpret_cast<char *>(data);
		std::string stored(cdata, cdata + len);
		// This is worth inspecting if you're unsure about it, but at the very center it contains only
		// (lambda [##x 1])
		// Some things we can check for
		test_result.Check("StoreEntityToMemory (prolog)", stored.substr(0, declare.size()), declare);
		test_result.Require("contain the entity contents", stored.find("##x 1") != std::string::npos);
		test_result.Require("does not contain the parent entity contents", stored.find("get_value") == std::string::npos);
	}
}

static void RoundTripCamlToMemory(TestResult &test_result)
{
	// Load the counter, bump it, dump it to an in-memory caml representation, then restore it.
	LoadEntityStatus status = LoadEntity(handle.data(), filename.data(), empty.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
	void *data = nullptr;
	size_t len = 0;
	test_result.Require("LoadEntity", status.loaded);
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);
		ExecuteEntity(handle.data(), initialize.data());

		ApiString incr(ExecuteEntityJsonPtr(handle.data(), increment.data(), empty.data()));
		test_result.Check("ExecuteEntityJsonPtr increment", incr, "1");

		StoreEntityToMemory(handle.data(), &data, &len, camlSuffix.data(), false, empty.data(), nullptr, 0);
		test_result.Require("data pointer written by StoreEntityToMemory", data !=
		 nullptr);
		test_result.Require("content written by StoreEntityToMemory", len > 4);

		// We've stored the data; let loaded_entity go out of scope so the entity can be destroyed
	}
	if(test_result)
	{
		char *cdata = reinterpret_cast<char *>(data);
		std::string magic(cdata, cdata + 4);
		test_result.Check("StoreEntityToMemory (magic number)", magic, "caml");
	}
	if(test_result)
	{
		LoadEntityStatus status = LoadEntityFromMemory(handle.data(), data, len, camlSuffix.data(), false, empty.data(), empty.data(), empty.data(), nullptr, 0);
		test_result.Require("LoadEntityFromMemory", status.loaded);
	}
	if(test_result)
	{
		LoadedEntity loaded_entity(handle);

		ApiString get(ExecuteEntityJsonPtr(handle.data(), get_value.data(), empty.data()));
		test_result.Check("ExecuteEntityJsonPtr get_value", get, "1");
	}
}

int main(int argc, char *argv[])
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
	suite.Run("DumpVersion", DumpVersion);
	suite.Run("LoadAndEval", LoadAndEval);
	suite.Run("InitializeCounter", InitializeCounter);
	suite.Run("ExecuteEntityJsonWithValue", ExecuteEntityJsonWithValue);
	suite.Run("ExecuteEntityJsonLogged", ExecuteEntityJsonLogged);
	suite.Run("ExecuteEntityJsonLoggedUpdating", ExecuteEntityJsonLoggedUpdating);
	suite.Run("ExecuteEntityJsonLoggedRoundTrip", ExecuteEntityJsonLoggedRoundTrip);
	suite.Run("ExecuteEntityJsonLoggedTwice", ExecuteEntityJsonLoggedTwice);
	suite.Run("ExecuteCounter2", ExecuteCounter2);
	suite.Run("ExecuteCounter2Logged", ExecuteCounter2Logged);
	suite.Run("TestLoadEntityFromMemory", TestLoadEntityFromMemory);
	suite.Run("LoadSubEntityFromMemory", LoadSubEntityFromMemory);
	suite.Run("LoadSubSubEntityFromMemory", LoadSubSubEntityFromMemory);
	suite.Run("TestStoreEntityToMemory", TestStoreEntityToMemory);
	suite.Run("StoreSubEntityToMemory", StoreSubEntityToMemory);
	suite.Run("RoundTripCamlToMemory", RoundTripCamlToMemory);

	return suite ? 0 : 1;
}
