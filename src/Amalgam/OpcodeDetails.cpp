//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"
#include "OpcodeDetails.h"

//system headers:
#include <regex>

EvaluableNode *ExecutionPermissions::GetPermissionsAsEvaluableNode(EvaluableNodeManager *enm)
{
	EvaluableNode *permissions_en = enm->AllocNode(ENT_ASSOC);
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err),
		enm->AllocNode(HasPermission(Permission::STD_OUT_AND_STD_ERR)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_in),
		enm->AllocNode(HasPermission(Permission::STD_IN)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_load),
		enm->AllocNode(HasPermission(Permission::LOAD)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_store),
		enm->AllocNode(HasPermission(Permission::STORE)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_environment),
		enm->AllocNode(HasPermission(Permission::ENVIRONMENT)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_alter_performance),
		enm->AllocNode(HasPermission(Permission::ALTER_PERFORMANCE)));
	permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_system),
		enm->AllocNode(HasPermission(Permission::SYSTEM)));

	return permissions_en;
}

std::pair<ExecutionPermissions, ExecutionPermissions> ExecutionPermissions::EvaluableNodeToPermissions(EvaluableNode *en)
{
	ExecutionPermissions permissions_to_set;
	ExecutionPermissions permission_values;

	if(EvaluableNode::IsAssociativeArray(en))
	{
		for(auto [permission_type, allow_en] : en->GetMappedChildNodesReference())
		{
			bool allow = EvaluableNode::ToBool(allow_en);

			if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err))
			{
				permissions_to_set.SetPermission(Permission::STD_OUT_AND_STD_ERR, true);
				permission_values.SetPermission(Permission::STD_OUT_AND_STD_ERR, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_in))
			{
				permissions_to_set.SetPermission(Permission::STD_IN, true);
				permission_values.SetPermission(Permission::STD_IN, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_load))
			{
				permissions_to_set.SetPermission(Permission::LOAD, true);
				permission_values.SetPermission(Permission::LOAD, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_store))
			{
				permissions_to_set.SetPermission(Permission::STORE, true);
				permission_values.SetPermission(Permission::STORE, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_environment))
			{
				permissions_to_set.SetPermission(Permission::ENVIRONMENT, true);
				permission_values.SetPermission(Permission::ENVIRONMENT, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_alter_performance))
			{
				permissions_to_set.SetPermission(Permission::ALTER_PERFORMANCE, true);
				permission_values.SetPermission(Permission::ALTER_PERFORMANCE, allow);
			}
			else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_system))
			{
				permissions_to_set.SetPermission(Permission::SYSTEM, true);
				permission_values.SetPermission(Permission::SYSTEM, allow);
			}
		}
	}
	else if(EvaluableNode::ToBool(en))
	{
		permissions_to_set = AllPermissions();
		permission_values = AllPermissions();
	}

	return std::make_pair(permissions_to_set, permission_values);
}


//returns a copy of s where each consecutive whitespace block is replaced
//by a single space and any leading and trailing spaces are removed
static std::string NormalizeWhitespace(std::string_view s)
{
	std::string out;
	out.reserve(s.size());

	bool in_whitespace = false;
	for(char ch : s)
	{
		if(std::isspace(static_cast<unsigned char>(ch)))
		{
			//if first whitespace, change to space
			if(!in_whitespace)
			{
				out.push_back(' ');
				in_whitespace = true;
			}
		}
		else //not first whitespace so skip
		{
			out.push_back(ch);
			in_whitespace = false;
		}
	}

	//trim spaces
	if(!out.empty() && out.front() == ' ')
		out.erase(out.begin());
	if(!out.empty() && out.back() == ' ')
		out.pop_back();

	return out;
}

//returns true if a and b are equal ignoring differences in the
//type of whitespace (spaces, tabs, newlines, etc.)
inline static bool EqualIgnoringWhitespace(std::string_view a, std::string_view b)
{
	return NormalizeWhitespace(a) == NormalizeWhitespace(b);
}

bool AmalgamExample::ValidateExample(Entity *entity)
{
	bool test_succeeded = true;
	std::cout << "Initializing... ";

	entity->SetRandomState("12345", true);

	auto [code, warnings, char_with_error, code_complete]
		= Parser::Parse(example, &entity->evaluableNodeManager);

	if(warnings.size() > 0)
	{
		std::cerr << "Improper code: " << std::endl;
		for(auto &w : warnings)
			std::cerr << w << std::endl;

		return false;
	}

	std::cout << "Executing... ";
	auto result = entity->ExecuteOnEntity(code, nullptr);
	std::string result_str = Parser::Unparse(result, true, true, true);

	if(regexMatch.empty())
	{
		if(!EqualIgnoringWhitespace(result_str, output))
		{
			std::cerr << "Failed, ran code:" << std::endl;
			std::cerr << example << std::endl;
			std::cerr << "Expected result:" << std::endl;
			std::cerr << output << std::endl;
			std::cerr << "Observed result:" << std::endl;
			std::cerr << result_str << std::endl;
			test_succeeded = false;
		}
	}
	else //match with regular expression
	{
		//use the begin/end constructor since std::string_view isn't universally supported
		std::regex pattern(begin(regexMatch), end(regexMatch), std::regex::ECMAScript);
		if(std::regex_match(result_str, pattern))
		{
			std::cerr << "Failed, ran code:" << std::endl;
			std::cerr << example << std::endl;
			std::cerr << "Expected to match:" << std::endl;
			std::cerr << regexMatch << std::endl;
			std::cerr << "Observed:" << std::endl;
			std::cerr << result_str << std::endl;
			test_succeeded = false;
		}
	}

	std::cout << "Reclaiming Resources... ";
	if(!cleanup.empty())
	{
		auto [cleanup_code, cleanup_warnings, cleanup_char_with_error, cleanup_code_complete]
			= Parser::Parse(cleanup, &entity->evaluableNodeManager);

		entity->ExecuteOnEntity(cleanup_code, nullptr);
	}

	entity->ReclaimResources(false, true, false);

	auto query_caches = entity->GetQueryCaches();
	if(query_caches != nullptr)
		query_caches->sbfds.VerifyAllEntitiesForAllColumns();

	if(entity->GetLabelIndex().size() != 0)
	{
		std::cerr << "Failed: Labels remain in entity after test" << std::endl;
		test_succeeded = false;
	}

	if(entity->GetContainedEntities().size() > 0)
	{
		std::cerr << "Failed: One or more contained entities remain after test" << std::endl;
		test_succeeded = false;
	}

	return test_succeeded;
}

static std::array<OpcodeDetails, NUM_ENT_OPCODES> BuildAmalgamExamplesArrayForOpcodes()
{
	std::array<OpcodeDetails, NUM_ENT_OPCODES> arr{};

	arr[static_cast<std::size_t>(ENT_SYSTEM)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string command [* optional1] ... [* optionalN])";
		d.returns = R"(any)";
		d.description = R"(Executes system command specified by `command`.  The available system commands are as follows:
 - exit:                Exits the application.
 - readline:            Reads a line of input from the terminal and returns the string.
 - printline:           Prints a line of string output of the second argument directly to the terminal and returns null.
 - cwd:                 If no additional parameter is specified, returns the current working directory. If an additional parameter is specified, it attempts to change the current working directory to that parameter, returning true on success and false on failure.
 - system:              Executes the the second argument as a system command (i.e., a string that would normally be run on the command line). Returns `(null)` if the command was not found. If found, it returns a list, where the first value is the exit code and the second value is a string containing everything printed to stdout.
 - os:                  Returns a string describing the operating system.
 - sleep:               Sleeps for the amount of seconds specified by the second argument.
 - version:             Returns a string representing the current Amalgam version.
 - est_mem_reserved:    Returns data involving the estimated memory reserved.
 - est_mem_used:        Returns data involving the estimated memory used (excluding memory management overhead, caching, etc.).
 - mem_diagnostics:     Returns data involving memory diagnostics.
 - rand:                Returns the number of bytes specified by the additional parameter of secure random data intended for cryptographic use.
 - sign_key_pair:       Returns a list of two values, first a public key and second a secret key, for use with cryptographic signatures using the Ed25519 algorithm, generated via securely generated random numbers.
 - encrypt_key_pair:    Returns a list of two values, first a public key and second a secret key, for use with cryptographic encryption using the XSalsa20 and Curve25519 algorithms, generated via securely generated random numbers.
 - debugging_info:      Returns a list of two values. The first is true if a debugger is present, false if it is not. The second is true if debugging sources is enabled, which means that source code location information is prepended to opcodes comments for any opcodes loaded from a file.
 - get_max_num_threads: Returns the current maximum number of threads.
 - set_max_num_threads: Attempts to set the current maximum number of threads, where 0 means to use the number of processor cores reported by the operating system. Returns the maximum number of threads after it has been set.
 - built_in_data:       Returns built-in data compiled along with the version information.)";
		d.examples = MakeAmalgamExamples({
			{R"((system "debugging_info"))", R"([.false .false])"}
			});
		d.permissions = ExecutionPermissions::Permission::ALL;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_HELP)] = []() {
		OpcodeDetails d;
		d.parameters = R"([string topic])";
		d.returns = R"(any)";
		d.description = R"(If no parameter is specified it returns a string of the topics that can be used.  For given a `topic`, returns a string or relevant data that describes the given topic.)";
		d.examples = MakeAmalgamExamples({
			{R"((help "+"))", R"&({
	allows_concurrency .true
	description "Sums all numbers."
	examples [
			{example "(+ 1 2 3 4)" output "10"}
		]
	new_scope .false
	new_target_scope .false
	parameters "[number x1] [number x2] ... [number xN]"
	permissions "none"
	requires_entity .false
	returns "number"
	value_newness "new"
})&"}
			});
		d.permissions = ExecutionPermissions::Permission::ALL;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_DEFAULTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string value_type)";
		d.returns = R"(any)";
		d.description = R"(Retrieves the default values of `value_type`, either "mutation_opcodes" or "mutation_types")";
		d.examples = MakeAmalgamExamples({
			{R"((get_defaults "mutation_types"))", R"({
	change_type 0.29
	deep_copy_elements 0.07
	delete 0.1
	delete_elements 0.05
	insert 0.25
	swap_elements 0.24
})"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();
	
	arr[static_cast<std::size_t>(ENT_RECLAIM_RESOURCES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity] [bool apply_to_all_contained_entities] [bool|list clear_query_caches] [bool collect_garbage] [bool force_free_memory])";
		d.returns = R"(any)";
		d.description = R"(Frees resources of the specified types on `entity`, which is the current entity if null.  Will include all contained entities if `apply_to_all_contained_entities` is true, which defaults to false, though the opcode will be unable to complete if there are concurrent threads running on any of the contained entities.  The parameter `clear_query_caches` will remove the query caches, which will make it faster to add, remove, or edit contained entities, but the cache will be rebuilt once a query is called.  If `clear_query_caches` is a boolean, then it will either clear all the caches or none.  If `clear_query_caches` is a list of strings, then it will only clear caches for the labels corresponding to the strings in the list.  The parameter `collect_garbage` will perform garbage collection on the entity, and if `force_free_memory` is true, it will reallocate memory buffers to their current size, after garbage collection if both are specified.)";
		d.examples = MakeAmalgamExamples({
			{R"((reclaim_resources (null) .true ["x"] .true .true ))", R"((null))"},
			{R"((reclaim_resources (null) .true .true .true .true ))", R"((null))"}
			});
		d.permissions = ExecutionPermissions::Permission::ALTER_PERFORMANCE;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_PARSE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string str [bool transactional] [bool return_warnings])";
		d.returns = R"(any)";
		d.description = R"(String `str` is parsed into code, and the result is returned.  If `transactional` is false, the default, it will attempt to parse the whole string and will return the closest code possible if there are any parse issues.  If `transactional` is true, it will parse the string transactionally, meaning that any node that has a parse error or is incomplete will be omitted along with all child nodes except for the top node.  If any performance constraints are given or `return_warnings` is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is an assoc mapping all warnings to their number of occurrences, and perf_constraint violation is a string denoting the constraint exceeded (or (null) if none)), unless `return_warnings` is false, in which case just the value will be returned.)";
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
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_UNPARSE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(code c [bool pretty_print] [bool sort_keys] [bool include_attributes])";
		d.returns = R"(string)";
		d.description = R"(Code is unparsed and the representative string is returned. If `pretty_print` is true, the output will be in pretty-print format, otherwise by default it will be inlined.  If `sort_keys` is true, the default, then it will print assoc structures and anything that could come in different orders in a natural sorted order by key, otherwise it will default to whatever order it is stored in memory.  If `include_attributes` is true, it will print out attributes like comments, but by default it will not.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unparse (parse "(print \"hello\")")))&", R"&("(print \"hello\")")&"},
			{R"&((parse (unparse (list (sqrt -1) (null) .infinity -.infinity))))&", R"&([(null) (null) .infinity -.infinity])&"},
			{R"&((unparse (associate "a" 1 "b" 2 "c" (list "alpha" "beta" "gamma"))))&", R"&("{a 1 b 2 c [\"alpha\" \"beta\" \"gamma\"]}")&"},
			{R"&((unparse (associate "a" 1 "b" 2 "c" (list "alpha" "beta" "gamma")) .true))&", R"&("{\r\n\ta 1\r\n\tb 2\r\n\tc [\"alpha\" \"beta\" \"gamma\"]\r\n}\r\n")&"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_IF)] = []() {
		OpcodeDetails d;
		d.parameters = R"([bool condition1] [code then1] [bool condition2] [code then2] ... [bool conditionN] [code thenN] [code else])";
		d.returns = R"(any)";
		d.description = R"(If `condition1` is true, then it will evaluate to the then1 argument.  Otherwise `condition2` will be checked, repeating for every pair.  If there is an odd number of parameters, the last is the final 'else', and will be evaluated as that if all conditions are false.  If there is an even number of parameters and none are true, then evaluates to null.)";
		d.examples = MakeAmalgamExamples({
			{R"&((if 1 "if 1"))&", R"("if 1")"},
			{R"&((if 0 "not this one" "if 2"))&", R"("if 2")"},
			{R"&((if
	(null) 1
	0 2
	0 3
	4
 ))&", R"(4)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SEQUENCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([code c1] [code c2] ... [code cN])";
		d.returns = R"(any)";
		d.description = R"(Runs each code block sequentially.  Evaluates to the result of the last code block run, unless it encounters a conclude or return in an earlier step, in which case it will halt processing and evaluate to the value returned by conclude or propagate the return.  Note that the last step will not consume a concluded value (see conclude opcode).)";
		d.examples = MakeAmalgamExamples({
			{R"((seq 1 2 3))", R"(3)"},
			{R"((seq
	(declare {a 1})
	(accum "a" 1)
	a
))", R"(2)"},
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LAMBDA)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* function [bool evaluate_and_wrap])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the code specified without evaluating it.  Useful for referencing functions or handling data without evaluating it.  The parameter `evaluate_and_wrap` defaults to false, but if it is true, it will evaluate the function, but then return the result wrapped in a lambda opcode.)";
		d.examples = MakeAmalgamExamples({
			{R"((lambda (+ 1 2)))", R"((+ 1 2))"},
			{R"((seq
	(declare {foo (lambda (+ y 1))})
	(call foo {y 1})
))", R"(2)"},
			{R"((lambda (+ 1 2) .true ))", R"((lambda 3))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CONCLUDE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* conclusion)";
		d.returns = R"(any)";
		d.description = R"(Evaluates to `conclusion` wrapped in a `conclude` opcode.  If a step in a `seq`, `let`, `declare`, or `while` evaluates to a `conclude` (excluding variable declarations for `let` and `declare`, the last step in `set`, `let`, and `declare`, or the condition of `while`), then it will conclude the execution and evaluate to the value `conclusion`.  Note that conclude opcodes may be nested to break out of outer opcodes.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	"seq1"
	(conclude "success")
	"seq2"
))&", R"("success")"},
			{R"&((while
	(< 1 100)
	"while1"
	(conclude "success")
	"while2"
))&", R"("success")"},
			{R"&((let
	{a 1}
	"let1"
	(conclude "success")
	"let2"
))&", R"("success")"},
			{R"&((declare
	{abcd 1}
	"declare1"
	(conclude "success")
	"declare2"
))&", R"("success")"},
			{R"&((seq
	1
	(declare
		{}
		(while
			1
			(if .true (conclude))
		)
		4
	)
	2
))&", R"(2)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_RETURN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* return_value)";
		d.returns = R"(any)";
		d.description = R"(Evaluates to `return_value` wrapped in a `return` opcode.  If a step in a `seq`, `let`, `declare`, or `while` evaluates to a return (excluding variable declarations for `let` and `declare`, the last step in `set`, `let`, and `declare`, or the condition of `while`), then it will conclude the execution and evaluate to the `return` opcode with its `return_value`.  This means it will continue to conclude each level up the stack until it reaches any kind of call opcode, including `call`, `call_sandboxed`, `call_entity`, `call_entity_get_changes`, or `call_container`, at which point it will evaluate to `return_value`.  Note that return opcodes may be nested to break out of multiple calls.)";
		d.examples = MakeAmalgamExamples({
			{R"&((call
	(seq
		1
		2
		(seq
			(return 3)
			4
		)
		5
	)
))&", R"(3)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CALL)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* function [assoc arguments])";
		d.returns = R"(any)";
		d.description = R"(Evaluates `function` after pushing the `arguments` assoc onto the scope stack.)";
		d.examples = MakeAmalgamExamples({
			{R"&((let
	{
		foo (lambda
				(declare
					{x 6}
					(+ x 2)
				)
			)
	}
	(call
		foo
		{x 3}
	)
))&", R"(5)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.hasSideEffects = true;
		d.newScope = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CALL_SANDBOXED)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* function assoc arguments [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [bool return_warnings])";
		d.returns = R"(any)";
		d.description = R"(Evaluates the code specified by function, isolating it from everything except for arguments, which is used as a single layer of the scope stack.  This is useful when evaluating code passed by other entities that may or may not be trusted.  Opcodes run from within call_sandboxed that require any form of permissions will not perform any action and will evaluate to null.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed. If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted, up to the limits of the current calling context. If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If `max_node_allocations` is 0 or infinite and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if `max_node_allocations` is specified while call_sandboxed is being called in a multithreaded environment, if the collective memory from all the related threads exceeds the average memory specified by call_sandboxed, that may trigger a memory limit for the call_sandboxed.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called. If `return_warnings` is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or (null) if none)).  If `return_warnings` is false, just the value will be returned.)";
		d.examples = MakeAmalgamExamples({
			{R"&((call_sandboxed
	(lambda
		(+
			(+ y 4)
			4
		)
	)
	{y 3}
	(null)
	(null)
	50
))&", R"([11 {} (null)])"},
			{R"&((call_sandboxed
	(lambda
		(+
			(+ y 4)
			4
		)
	)
	{y 3}
	(null)
	(null)
	1
))&", R"([(null) {} "Execution depth exceeded"])"},
			{R"&((call_sandboxed
	(lambda
		(call_sandboxed
			(lambda
				(+
					(+ y 4)
					4
				)
			)
			{y 3}
			(null)
			(null)
			50
		)
	)
	{y 3}
	(null)
	(null)
	3
))&", R"([
	[(null) {} "Execution depth exceeded"]
	{}
	(null)
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.newScope = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_WHILE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(bool condition [code code1] [code code2] ... [code codeN])";
		d.returns = R"(any)";
		d.description = R"(Each time the `condition` evaluates to true, it runs each of code sequentially, looping. Evaluates to the last `codeN` or null if the `condition` was initially false or if it encounters a `conclude` or `return`, it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  For each iteration of the loop, it pushes a new target scope onto the target stack, with `(current_index)` being the iteration count, and `(previous_result)` being the last evaluated `codeN` of the previous loop.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(assign
		{i 1}
	)
	(while
		(< i 10)
		(accum
			{i 1}
		)
	)
	i
))&", R"(10)"},
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LET)] = []() {
		OpcodeDetails d;
		d.parameters = R"(assoc variables [code code1] [code code2] ... [code codeN])";
		d.returns = R"(any)";
		d.description = R"(Pushes the key-value pairs of `variables` onto the scope stack so that they become the new variables, then runs each code block sequentially, evaluating to the last code block run, unless it encounters a `conclude` or `return`, in which case it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  Note that the last step will not consume a concluded value.)";
		d.examples = MakeAmalgamExamples({
			{R"&((let
	{x 4 y 6}
	(+ x y)
))&", R"(10)"},
			{R"&((let
	{x 4 y 6}
	(declare
		{x 5 z 1}
		(+ x y z)
	)
))&", R"(11)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
		d.newScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_DECLARE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(assoc variables [code code1] [code code2] ... [code codeN])";
		d.returns = R"(any)";
		d.description = R"(For each key-value pair of `variables`, if not already in the current context in the scope stack, it will define them.  Then it runs each code block sequentially, evaluating to the last code block run, unless it encounters a `conclude` or `return`, in which case it will halt processing and evaluate to the value returned by `conclude` or propagate the `return`.  Note that the last step will not consume a concluded value.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		{x 7}
		(accum "x" 1)
	)
	(declare
		{x 4}
	)
	x
))&", R"(8)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ASSIGN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(assoc|string variables [number index1|string index1|list walk_path1|* new_value1] [* new_value1] [number index2|string index2|list walk_path2] [* new_value2] ...)";
		d.returns = R"(null)";
		d.description = R"(If `variables` is an assoc, then for each key-value pair it assigns the value to the variable represented by the key found by tracing upward on the stack.  If a variable is not found, it will create a variable on the top of the stack with that name.  If `variables` is a string and there are two parameters, it will assign the second parameter to the variable represented by the first.  If `variables` is a string and there are three or more parameters, then it will find the variable by tracing up the stack and then use each pair of walk_path and new_value to assign new_value to that part of the variable's structure.)";
		d.examples = MakeAmalgamExamples({
			{R"&((let
	{x 0}
	(assign {x 10} )
	x
))&", R"(10)"},
			{R"&((seq
	(assign "x" 20)
	x
))&", R"(20)"},
			{R"&((seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(assign
		"x"
		[1]
		"not 1"
	)
	x
))&", R"([
	0
	"not 1"
	2
	{a 1 b 2 c 3}
])"},
			{R"&((seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(assign
		"x"
		[3 "c"]
		["c attribute"]
		[3 "a"]
		["a attribute"]
	)
	x
))&", R"([
	0
	1
	2
	{
		a ["a attribute"]
		b 2
		c ["c attribute"]
	}
])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ACCUM)] = []() {
		OpcodeDetails d;
		d.parameters = R"(assoc|string variables [number index1|string index1|list walk_path1] [* accum_value1] [number index2|string index2|list walk_path2] [* accum_value2] ...)";
		d.returns = R"(null)";
		d.description = R"(If `variables` is an assoc, then for each key-value pair of data, it assigns the value of the pair accumulated with the current value of the variable represented by the key on the stack, and stores the result in the variable.  It searches for the variable name tracing up the stack to find the variable. If the variable is not found, it will create a variable on the top of the stack.  Accumulation is performed differently based on the type.  For numeric values it adds, for strings it concatenates, for lists and assocs it appends.  If `variables` is a string and there are two parameters, then it will accum the second parameter to the variable represented by the first.  If `variables` is a string and there are three or more parameters, then it will find the variable by tracing up the stack and then use each pair of the corresponding walk path and accum value to that part of the variable's structure.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(assign
		{x 10}
	)
	(accum
		{x 1}
	)
	x
))&", R"(11)"},
			{R"&((declare
	{
		accum_assoc (associate "a" 1 "b" 2)
		accum_list [1 2 3]
		accum_string "abc"
	}
	(accum
		{accum_string "def"}
	)
	(accum
		{
			accum_list [4 5 6]
		}
	)
	(accum
		{
			accum_list (associate "7" 8)
		}
	)
	(accum
		{
			accum_assoc (associate "c" 3 "d" 4)
		}
	)
	(accum
		{
			accum_assoc ["e" 5]
		}
	)
	[accum_string accum_list accum_assoc]
))&", R"([
	"abcdef"
	[
		1
		2
		3
		4
		5
		6
		"7"
		8
	]
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
])"},
			{R"&((seq
		(assign "x" 1)
		(accum "x" [] 4)
		x
))&", R"(5)"},
			{R"&((seq
	(assign
		"x"
		[
			0
			1
			2
			(associate "a" 1 "b" 2 "c" 3)
		]
	)
	(accum
		"x"
		[1]
		1
	)
	x
))&", R"([
	0
	2
	2
	{a 1 b 2 c 3}
])"},
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_RETRIEVE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([string|list|assoc variables])";
		d.returns = R"(any)";
		d.description = R"(If `variables` is a string, then it gets the value on the stack specified by the string.  If `variables` is a list, it returns a list of the values on the stack specified by each element of the list interpreted as a string.  If `variables` is an assoc, it returns an assoc with the indices of the assoc which was passed in with the values being the appropriate values on the stack for each index.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(assign
		{a 1}
	)
	(retrieve "a")
))&", R"(1)"},
			{R"&((seq
	(assign
		{a 1 b 2}
	)
	[
		(retrieve "a")
		(retrieve
			["a" "b"]
		)
		(retrieve
			(zip
				["a" "b"]
			)
		)
	]
))&", R"([
	1
	[@(target .true 0) 2]
	{a @(target .true 0) b @(target .true [1 1])}
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* data [number|index|list walk_path_1] [number|string|list walk_path_2] ...)";
		d.returns = R"(any)";
		d.description = R"(Evaluates to `data` as traversed by the set of values specified by `walk_path_1', which can be any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values.  If multiple walk paths are specified, then `get` returns a list, where each element in the list is the respective element retrieved by the respective walk path.  If the walk path continues past the data structure, it will return a null.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get
	[4 9.2 "this"]
))&", R"([4 9.2 "this"])"},
			{R"&((get
	[4 9.2 "this"]
	1
))&", R"(9.2)"},
			{R"&((get
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"c"
))&", R"(3)"},
			{R"&((get
	[
		0
		1
		2
		3
		[
			0
			1
			2
			(associate "a" 1)
		]
	]
	[4 3 "a"]
))&", R"(1)"},
			{R"&((get
	[4 9.2 "this"]
	1
	2
))&", R"([9.2 "this"])"},
			{R"&((seq
	(declare
		{
			var {
					A (associate "B" 2)
					B 2
				}
		}
	)
	[
		(get
			var
			["A" "B"]
		)
		(get
			var
			["A" "C"]
		)
		(get
			var
			["B" "C"]
		)
	]
))&", R"([2 (null) (null)])"},
			{R"&((get
	{(null) 3}
	(null)
))&", R"(3)"},
			{R"&((let
	{
		complex_assoc {
				4 "number"
				[4] "list"
				{4 4} "assoc"
				"4" "string"
			}
	}
	[
		(get complex_assoc 4)
		(get complex_assoc "4")
		(get
			complex_assoc
			[
				[4]
			]
		)
		(get
			complex_assoc
			{4 4}
		)
		(sort (indices complex_assoc))
	]
))&", R"([
	"number"
	"string"
	"list"
	"assoc"
	[
		4
		[4]
		{4 4}
		"4"
	]
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* data [number|string|list walk_path1] [* new_value1] [number|string|list walk_path2] [* new_value2] ... [number|string|list walk_pathN] [* new_valueN])";
		d.returns = R"(any)";
		d.description = R"(Performs a deep copy on `data` (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values as a walk path of indices. `new_value1` to `new_valueN` represent a value that will be used to replace  whatever is in the location the preceding location parameter specifies.  If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc); however, it will not change the type of immediate values to an assoc or list. Note that `(target)` will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.)";
		d.examples = MakeAmalgamExamples({
			{R"&((set
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"e"
	5
))&", R"({
	4 "d"
	a 1
	b 2
	c 3
	e 5
})"},
			{R"&((set
	[0 1 2 3 4]
	2
	10
))&", R"([0 1 10 3 4])"},
			{R"&((set
	(associate "a" 1 "b" 2)
	"a"
	3
))&", R"({a 3 b 2})"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_REPLACE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* data [number|string|list walk_path1] [* function1] [number|string|list walk_path2] [* function2] ... [number|string|list walk_pathN] [* functionN])";
		d.returns = R"(any)";
		d.description = R"(Performs a deep copy on `data` (a copy of all data structures referenced by it and its references), then looks at the remaining parameters as pairs.  For each pair, the first is any of: a number, representing an index, with negative numbers representing backward traversal from the end of the list; a string, representing the index; or a list, representing a way to walk into the structure as the aforementioned values. `function1` to `functionN` represent a function that will be used to replace in place of whatever is in the location of the corresponding walk_path, and will be passed the current node in (current_value).  The function can optionally be just be an immediate value or any code that can be evaluated.  If a particular location does not exist, it will be created assuming the most generic type that will support the index (as a null, list, or assoc). Note that the `(target)` will evaluate to the new copy of data, which is the base of the newly constructed data; this is useful for creating circular references.)";
		d.examples = MakeAmalgamExamples({
			{R"&((replace
	[
		(associate "a" 13)
	]
))&", R"([
	{a 13}
])"},
			{R"&((replace
	[
		(associate "a" 1)
	]
	[2]
	1
	[0]
	[4 5 6]
))&", R"([
	[4 5 6]
	(null)
	1
])"},
			{R"&((replace
	[
		(associate "a" 1)
	]
	2
	1
	0
	[4 5 6]
))&", R"([
	[4 5 6]
	(null)
	1
])"},
			{R"&((replace
	[
		(associate "a" 1)
	]
	[0]
	(lambda
		(set (current_value) "b" 2)
	)
))&", R"([
	{a 1 b 2}
])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TARGET)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number|bool stack_distance] [number|string|list walk_path])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the node being created, referenced by the parameters by target.  Useful for serializing graph data structures or looking up data during iteration.  If `stack_distance` is a number, it climbs back up the target stack that many levels.  If `stack_distance` is a boolean, then `.true` indicates the top of the stack and `.false` indicates the bottom.  If `walk_path` is specified, it will walk from the node at `stack_distance` to the corresponding target.  If building an object, specifying `stack_distance` to true is often useful for accessing or traversing the top-level elements.)";
		d.examples = MakeAmalgamExamples({
			{R"&([
	1
	2
	3
	(target 0 1)
	4
])&", R"([1 2 3 @(target .true 1) 4])"},
			{R"&([
	0
	1
	2
	3
	(+
		(target 0 1)
	)
	4
])&", R"([0 1 2 3 1 4])"},
			{R"&([
	0
	1
	2
	3
	[
		0
		1
		2
		3
		(+
			(target 1 1)
		)
		4
	]
])&", R"([
	0
	1
	2
	3
	[0 1 2 3 1 4]
])"},
			{R"&({
	a 0
	b 1
	c 2
	d 3
	e [
			0
			1
			2
			3
			(+
				(target 1 "a")
			)
			4
		]
})&", R"({
	a 0
	b 1
	c 2
	d 3
	e [0 1 2 3 0 4]
})"},
			{R"&((call_sandboxed {
	a 0
	b 1
	c 2
	d 3
	e [
			[
				0
				1
				2
				3
				(+
					(target .true "a")
				)
				4
			]
		]
}))&", R"({
	a 0
	b 1
	c 2
	d 3
	e [
			[0 1 2 3 0 4]
		]
})"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CURRENT_INDEX)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number stack_distance])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the index of the current node being iterated on within the current target.  If `stack_distance` is specified, it climbs back up the target stack that many levels.)";
		d.examples = MakeAmalgamExamples({
			{R"&([0 1 2 3 (current_index) 5])&", R"([0 1 2 3 4 5])"},
			{R"&([
	0
	1
	[
		0
		1
		2
		3
		(current_index 1)
		4
	]
])&", R"([
	0
	1
	[0 1 2 3 2 4]
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CURRENT_VALUE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number stack_distance])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the current node being iterated on within the current target.  If `stack_distance` is specified, it climbs back up the target stack that many levels.)";
		d.examples = MakeAmalgamExamples({
			{R"&((map
	(lambda
		(* 2 (current_value))
	)
	(range 0 4)
))&", R"([0 2 4 6 8])"},
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_PREVIOUS_RESULT)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number stack_distance] [bool copy])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the resulting node of the previous iteration for applicable opcodes. If `stack_distance` is specified, it climbs back up the target stack that many levels.  If `copy` is true, which is false by default, then a copy of the resulting node of the previous iteration is returned, otherwise the result of the previous iteration is returned directly and consumed.)";
		d.examples = MakeAmalgamExamples({
			{R"&((while
	(< (current_index) 3)
	(append (previous_result) (current_index))
))&", R"([(null) 0 1 2])"},
			{R"&((while
	(< (current_index) 3)
	(if
		(= (current_index) 0)
		3
		(append
			(previous_result 0 .true)
			(previous_result 0)
			(previous_result 0)
		)
	)
))&", R"([
	3
	3
	(null)
	3
	3
	(null)
	(null)
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_OPCODE_STACK)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number stack_distance] [bool no_child_nodes])";
		d.returns = R"(list of any)";
		d.description = R"(Evaluates to the list of opcodes that make up the call stack or a single opcode within the call stack.  If `stack_distance` is specified, then a copy of the node at that specified depth is returned, otherwise the list of all opcodes in opcode stack are returned. Negative values for `stack_distance` specify the depth from the top of the stack and positive values specify the depth from the bottom.  If `no_child_nodes` is true, then only the root node(s) are returned, otherwise the returned node(s) are deep-copied.)";
		d.examples = MakeAmalgamExamples({
			{R"&((size (opcode_stack)))&", R"(2)"},
			{R"&((seq
	(seq
		(opcode_stack 2)
	)
))&", R"((seq
	(seq
		(opcode_stack 2)
	)
))"},
			{R"&((seq
	(seq
		(opcode_stack -1 .true)
	)
))&", R"((seq))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_STACK)] = []() {
		OpcodeDetails d;
		d.parameters = R"( )";
		d.returns = R"(list of assoc)";
		d.description = R"(Evaluates to the current execution context, also known as the scope stack, containing all of the variables for each layer of the stack.)";
		d.examples = MakeAmalgamExamples({
			{R"&((stack))&", R"([{}])"},
			{R"&((call
	(lambda
		(let
			{a 1}
			(stack)
		)
	)
	{x 1}
))&", R"([
	{}
	{x 1}
	{a 1}
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ARGS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number stack_distance])";
		d.returns = R"(assoc)";
		d.description = R"(Evaluates to the top context of the stack, the current execution context, or scope stack, known as the arguments.  If `stack_distance` is specified, then it evaluates to the context that many layers up the stack.)";
		d.examples = MakeAmalgamExamples({
			{R"&((call
	(lambda
		(let
			(associate "bbb" 3)
			[
				(args)
				(args 1)
			]
		)
	)
	{x 1}
))&", R"([
	{bbb 3}
	{x 1}
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_RAND)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|number range] [number number_to_generate] [bool unique])";
		d.returns = R"(any)";
		d.description = R"(Generates random values based on its parameters.  The random values are drawn from a random stream specific to each execution flow for each entity.  With no range, evaluates to a random number between 0.0 and 1.0.  If range is a list, it will uniformly randomly choose and evaluate to one element of the list.  If range is a number, it will evaluate to a value greater than or equal to zero and less than the number specified.  If range is an assoc, then it will randomly evaluate to one of the keys using the values as the weights for the probabilities.  If  number_to_generate is specified, it will generate a list of multiple values (even if number_to_generate is 1).  If unique is true (it defaults to false), then it will only return unique values, the same as selecting from the list or assoc without replacement.  Note that if unique only applies to list and assoc ranges.  If unique is true and there are not enough values in a list or assoc, it will only generate the number of elements in range.)";
		d.examples = MakeAmalgamExamples({
			{R"&((rand))&", R"(0.4153759082605256)"},
			{R"&((rand 50))&", R"(20.768795413026282)"},
			{R"&((rand
	[1 2 4 5 7]
))&", R"(1)"},
			{R"&((rand
	(range 0 10)
))&", R"(4)"},
			{R"&((rand
	(range 0 10)
	0
))&", R"([])"},
			{R"&((rand
	(range 0 10)
	1
))&", R"([4])"},
			{R"&((rand
	(range 0 10)
	10
	.true
))&", R"([
	4
	0
	5
	9
	10
	1
	2
	7
	6
	8
])"},
			{R"&((rand 50 4))&", R"([20.768795413026282 23.51742714184096 6.034392211178502 29.777315548569128])"},
			{R"&((rand
	(associate "a" 0.25 "b" 0.75)
))&", R"("b")"},
			{R"&((rand
	(associate "a" 0.25 "b" 0.75)
	16
))&", "", R"&(\[\s*
    "(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
	"(?:a|b)"\s *
\])&"
},
			{R"&((rand
	(associate
		"a"
		0.25
		"b"
		0.75
		"c"
		.infinity
		"d"
		.infinity
	)
	4
))&", R"(["c" "c" "c" "d"])",
R"&(\[\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
    "(?:c|d)"\s*
\])&"
},
			{R"&(;should come out somewhere near the correct proportion
(zip
	(lambda
		(+
			(current_value 1)
			(current_value)
		)
	)
	(rand
		(associate "a" 0.25 "b" 0.5 "c" 0.25)
		100
	)
	1
))&", R"({a 30 b 50 c 20})",
			R"&(\{\s*
    a\s+(\d+)\s+
    b\s+(\d+)\s+
    c\s+(\d+)\s*
\})&"
},
			{R"&(;these should be weighted toward smaller numbers
(rand
	(zip
		(range 1 10)
		(map
			(lambda
				(/
					(/ 1 (current_value))
					2
				)
			)
			(range 1 10)
		)
	)
	3
	.true
))&", R"([2 6 1])",
			R"&(\[\s*
    (\d+)\s*
    (\d+)\s*
    (\d+)\s*
\])&"
}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_RAND_SEED)] = []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(string)";
		d.description = R"(Evaluates to a string representing the current state of the random number generator.  Note that the string will be a string of bytes that may not be valid as UTF-8.)";
		d.examples = MakeAmalgamExamples({
			{R"&((format (get_rand_seed) "string" "base64"))&", R"("X6f8e5JTT5kuHHGZUu7r6/8=")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_RAND_SEED)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string seed)";
		d.returns = R"(string)";
		d.description = R"(Initializes the random number stream for the given `seed` without affecting any entity.  If the seed is already a string in the proper format output by `get_entity_rand_seed` or `get_rand_seed`, then it will set the random generator to that current state, picking up where the previous state left off.  If it is anything else, it uses the value as a random seed to start the generator.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		{cur_seed (get_rand_seed)}
	)
	(declare
		{
			first_pair [(rand) (rand)]
		}
	)
	(set_rand_seed cur_seed)
	(declare
		{
			second_pair [(rand) (rand)]
		}
	)
	(append first_pair second_pair)
))&", R"([0.4153759082605256 0.47034854283681926 0.4153759082605256 0.47034854283681926])"},
			{R"&((seq
	(set_rand_seed "12345")
	(rand)
))&", R"(0.5507987428849511)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SYSTEM_TIME)] = []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the current system time since epoch in seconds (including fractions of seconds).)";
		d.examples = MakeAmalgamExamples({
			{R"&((system_time))&", R"(1773855306.4474)",
			R"&(^\s*
    (                                   # start of the number
        (?:\d+\.\d*|\.\d+|\d+)          # integer part with optional fraction
        (?:[eE][+-]?\d+)?               # optional exponent
    )
    \s*$)&"
}
			});
		d.permissions = ExecutionPermissions::Permission::ENVIRONMENT;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ADD)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number x1] [number x2] ... [number xN])";
		d.returns = R"(number)";
		d.allowsConcurrency = true;
		d.description = R"(Sums all numbers.)";
		d.examples = MakeAmalgamExamples({
			{R"((+ 1 2 3 4))", R"(10)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SUBTRACT)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number x1] [number x2] ... [number xN])";
		d.returns = R"(number)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to `x1` - `x2` - ... - `xN`.  If only one parameter is passed, then it is treated as negative)";
		d.examples = MakeAmalgamExamples({
			{R"((- 1 2 3 4))", R"(-8)"},
			{R"((- 3))", R"(-3)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MULTIPLY)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number x1] [number x2] ... [number xN])";
		d.returns = R"(number)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to the product of all numbers.)";
		d.examples = MakeAmalgamExamples({
			{R"((* 1 2 3 4))", R"(24)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_DIVIDE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number x1] [number x2] ... [number xN])";
		d.returns = R"(number)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to `x1` / `x2` / ... / `xN`.)";
		d.examples = MakeAmalgamExamples({
			{R"((/ 1.0 2 3 4))", R"(0.041666666666666664)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MODULUS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number x1] [number x2] ... [number xN])";
		d.returns = R"(number)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates the modulus of `x1` % `x2` % ... % `xN`.)";
		d.examples = MakeAmalgamExamples({
			{R"((mod 1 2 3 4))", R"(1)"},
			{R"((mod 5 3))", R"(2)"},
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_DIGITS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number value [number base] [number start_digit] [number end_digit] [bool relative_to_zero])";
		d.returns = R"(list of number)";
		d.description = R"(Evaluates to a list of the number of each digit of `value` for the given `base`.  If `base` is omitted, 10 is the default.  The parameters `start_digit` and `end_digit` can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of `start_digit` and `end_digit` are with respect to relative_to_zero, which defaults to true.  If relative_to_zero is true, then the digits are indexed from their distance to zero, such as "5 4 3 2 1 0 . -1 -2".  If relative_to_zero is false, then the digits are indexed from their most significant digit, such as "0 1 2 3 4 5 . 6  7".  The default values of `start_digit` and `end_digit` are the most and least significant digits respectively.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get_digits 1234567.8 10))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	0
	0
	0
])"},
			{R"&((get_digits 1234567.89 10))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	8
	9
	9
])"},
			{R"&((get_digits 1234.5678 10 -1 -.infinity))&", R"([
	5
	6
	7
	8
	0
	0
	0
	0
	0
	0
	0
])"},
			{R"&((get_digits 7 2 .infinity 0))&", R"([1 1 1])"},
			{R"&((get_digits 16 2 .infinity 0))&", R"([1 0 0 0 0])"},
			{R"&((get_digits 24 4 .infinity 0))&", R"([1 2 0])"},
			{R"&((get_digits 40 3 .infinity 0))&", R"([1 1 1 1])"},
			{R"&((get_digits 16 2 .infinity 0))&", R"([1 0 0 0 0])"},
			{R"&((get_digits 16 8 .infinity 0))&", R"([2 0])"},
			{R"&((get_digits 3 2 5 0))&", R"([0 0 0 0 1 1])"},
			{R"&((get_digits 1.5 1.5 .infinity 0))&", R"([1 0])"},
			{R"&((get_digits 3.75 1.5 .infinity -7))&", R"([
	1
	0
	0
	0
	0
	0
	1
	0
	0
	0
	1
])"},
			{R"&((get_digits 1234567.8 10 0 4 .false))&", R"([1 2 3 4 5])"},
			{R"&((get_digits 1234567.8 10 4 8 .false))&", R"([5 6 7 8 0])"},
			{R"&((get_digits 1.2345678e+100 10 0 4 .false))&", R"([1 2 3 4 5])"},
			{R"&((get_digits 1.2345678e+100 10 4 8 .false))&", R"([5 6 7 8 0])"},
			{R"&(;should print empty list for these
(get_digits 0 2.714 1 3 .false))&", R"([])"},
			{R"&((get_digits 0 2.714 1 3 .true))&", R"([])"},
			{R"&((get_digits 0 10 0 10 .false))&", R"([])"},
			{R"&(;4 followed by zeros
(get_digits 0.4 10 0 10 .false))&", R"([
	4
	0
	0
	0
	0
	0
	0
	0
	0
	0
	0
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_DIGITS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number value [number base] [list|number|null digits] [number start_digit] [number end_digit] [bool relative_to_zero])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to `value` having each of the values in the list of `digits` replace each of the relative digits in `value` for the given base.  If a digit is null in `digits`, then that digit is not set.  If `base` is omitted, 10 is the default.  The parameters `start_digit` and `end_digit` can be used to get a specific set of digits, but can also be infinite or null to catch all the digits on one side of the number.  The interpretation of `start_digit` and `end_digit` are with respect to `relative_to_zero`, which defaults to true.  If `relative_to_zero` is true, then the digits are indexed from their distance to zero, such as "5 4 3 2 1 0 . -1 -2".  If `relative_to_zer`o is false, then the digits are indexed from their most significant digit, such as "0 1 2 3 4 5 . 6  7".  The default values of `start_digit` and `end_digit` are the most and least significant digits respectively.)";
		d.examples = MakeAmalgamExamples({
			{R"&((set_digits
	1234567.8
	10
	[5 5 5]
))&", R"(5554567.8)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5]
	-1
	-.infinity
))&", R"(1234567.555)"},
			{R"&((set_digits
	7
	2
	[1 0 0]
	.infinity
	0
))&", R"(4)"},
			{R"&((set_digits
	1.5
	1.5
	[1]
	.infinity
	0
))&", R"(1.5)"},
			{R"&((set_digits
	1.5
	1.5
	[2]
	.infinity
	0
))&", R"(3)"},
			{R"&((set_digits
	1.5
	1.5
	[1 0]
	1
	0
))&", R"(1.5)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5]
	10
))&", R"(55501234567.8)"},
			{R"&((set_digits
	1.5
	1.5
	[1 0 0]
	2
	0
))&", R"(2.25)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5 5 5]
	0
	4
	.false
))&", R"(5555567.8)"},
			{R"&((set_digits
	1234567.8
	10
	[5 5 5 5 5]
	4
	8
	.false
))&", R"(1234555.55)"},
			{R"&((set_digits
	1.2345678e+100
	10
	[5 5 5 5 5]
	0
	4
	.false
))&", R"(5.555567800000001e+100)"},
			{R"&((set_digits
	1.2345678e+100
	10
	[5 5 5 5 5]
	4
	8
	.false
))&", R"(1.2345555499999999e+100)"},
			{R"&((set_digits
	1.2345678e+100
	10
	[5 (null) 5 (null) 5]
	4
	8
	.false
))&", R"(1.23456585e+100)"},
			{R"&(;these should all print (list 1 0 1)
(get_digits
	(set_digits
		1234567.8
		10
		[1 0 1 0]
		2
		5
		.false
	)
	10
	2
	5
	.false
))&", R"([1 0 1 0])"},
			{R"&((get_digits
	(set_digits
		1234567.8
		2
		[1 0 1 0]
		2
		5
		.false
	)
	2
	2
	5
	.false
))&", R"([1 0 1 0])"},
			{R"&((get_digits
	(set_digits
		1234567.8
		3.1
		[1 0 1 0]
		2
		5
		.false
	)
	3.1
	2
	5
	.false
))&", R"([1 0 1 0])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_FLOOR)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(int)";
		d.description = R"(Evaluates to the mathematical floor of x.)";
		d.examples = MakeAmalgamExamples({
			{R"((floor 1.5))", R"(1)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CEILING)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(int)";
		d.description = R"(Evaluates to the mathematical ceiling of x.)";
		d.examples = MakeAmalgamExamples({
			{R"((ceil 1.5))", R"(2)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ROUND)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x [number significant_digits] [number significant_digits_after_decimal])";
		d.returns = R"(int)";
		d.description = R"(Rounds the value `x` and evaluates to the new value.  If only one parameter is specified, it rounds to the nearest integer.  If `significant_digits` is specified, then it rounds to the specified number of significant digits.  If `significant_digits_after_decimal` is specified, then it ensures that `x` will be rounded at least to the number of decimal points past the integer as specified, and takes priority over `significant_digits`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((round 12.7))&", R"(13)"},
			{R"&((round 12.7 1))&", R"(10)"},
			{R"&((round 123.45678 5))&", R"(123.46)"},
			{R"&((round 123.45678 2))&", R"(120)"},
			{R"&((round 123.45678 2 2))&", R"(120)"},
			{R"&((round 123.45678 6 2))&", R"(123.46)"},
			{R"&((round 123.45678 4 0))&", R"(123)"},
			{R"&((round 123.45678 0 0))&", R"(0)"},
			{R"&((round 1.2345678 2 4))&", R"(1.2)"},
			{R"&((round 1.2345678 0 4))&", R"(0)"},
			{R"&((round 0.012345678 2 4))&", R"(0.012)"},
			{R"&((round 0.012345678 4 2))&", R"(0.01)"},
			{R"&((round 0.012345678 0 0))&", R"(0)"},
			{R"&((round 0.012345678 100 100))&", R"(0.012345678)"},
			{R"&((round 0.6 2))&", R"(0.6)"},
			{R"&((round 0.6 32 2))&", R"(0.6)"},
			{R"&((round
	(/ 1 3)
	32
	1
))&", R"(0.3)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_EXPONENT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(number)";
		d.description = R"(e^x)";
		d.examples = MakeAmalgamExamples({
			{R"((exp 0.5))", R"(1.6487212707001282)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LOG)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x [number base])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the logarithm of `x`.  If `base` is specified, uses that base, otherwise defaults to natural log.)";
		d.examples = MakeAmalgamExamples({
			{R"((log 0.5))", R"(-0.6931471805599453)"},
			{R"((log 0.5 2))", R"(-1)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SIN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number theta)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the sine of `theta`.)";
		d.examples = MakeAmalgamExamples({
			{R"((sin 0.5))", R"(0.479425538604203)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ASIN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number length)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the arc sine (inverse sine) of `length`.)";
		d.examples = MakeAmalgamExamples({
			{R"((sin 0.5))", R"(0.479425538604203)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_COS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number theta)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the cosine of `theta`.)";
		d.examples = MakeAmalgamExamples({
			{R"((cos 0.5))", R"(0.8775825618903728)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ACOS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number length)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the arc cosine (inverse cosine) of `length`.)";
		d.examples = MakeAmalgamExamples({
			{R"((acos 0.5))", R"(1.0471975511965979)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TAN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number theta)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the tangent of `theta`.)";
		d.examples = MakeAmalgamExamples({
			{R"((tan 0.5))", R"(0.5463024898437905)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ATAN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number num [number divisor])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the arc tangent (inverse tangent) of `num`.  If two numbers are provided, then it evaluates to the arc tangent of `num` / `divisor`.)";
		d.examples = MakeAmalgamExamples({
			{R"((atan 0.5))", R"(0.4636476090008061)"}, {R"((atan 0.5 0.5))", R"(0.7853981633974483)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SINH)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number z)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the hyperbolic sine of `z`.)";
		d.examples = MakeAmalgamExamples({
			{R"((sinh 0.5))", R"(0.5210953054937474)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ASINH)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the hyperbolic arc sine of `x`.)";
		d.examples = MakeAmalgamExamples({
			{R"((asinh 0.5))", R"(0.48121182505960347)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_COSH)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number z)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the hyperbolic cosine of `z`.)";
		d.examples = MakeAmalgamExamples({
			{R"((cosh 0.5))", R"(1.1276259652063807)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ACOSH)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the hyperbolic arc cosine of `x`.)";
		d.examples = MakeAmalgamExamples({
			{R"((acosh 1.5))", R"(0.9624236501192069)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TANH)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number z)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the hyperbolic tangent on `z`.)";
		d.examples = MakeAmalgamExamples({
			{R"((tanh 0.5))", R"(0.46211715726000974)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ATANH)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the hyperbolic arc tangent on `x`.)";
		d.examples = MakeAmalgamExamples({
			{R"((atanh 0.5))", R"(0.5493061443340549)", R"(0.54930614433405\d+)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ERF)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number errno)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the error function on `errno`.)";
		d.examples = MakeAmalgamExamples({
			{R"((erf 0.5))", R"(0.5204998778130465)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TGAMMA)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number z)";
		d.returns = R"(number)";
		d.description = R"(Evaluates the true (complete) gamma function on `z`.)";
		d.examples = MakeAmalgamExamples({
			{R"((tgamma 0.5))", R"(1.772453850905516)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LGAMMA)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number z)";
		d.returns = R"(number)";
		d.description = R"(Evaluates the log-gamma function function on `z`.)";
		d.examples = MakeAmalgamExamples({
			{R"((lgamma 0.5))", R"(0.5723649429247001)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SQRT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(number)";
		d.description = R"(Returns the square root of `x`.)";
		d.examples = MakeAmalgamExamples({
			{R"((sqrt 0.5))", R"(0.7071067811865476)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_POW)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number base number exponent)";
		d.returns = R"(number)";
		d.description = R"(Returns `base` raised to the `exponent` power.)";
		d.examples = MakeAmalgamExamples({
			{R"((pow 0.5 2))", R"(0.25)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ABS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number x)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to absolute value of `x`)";
		d.examples = MakeAmalgamExamples({
			{R"((abs -0.5))", R"(0.5)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MAX)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number x1] [number x2] ... [number xN])";
		d.returns = R"(number)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to the maximum of all of parameters.)";
		d.examples = MakeAmalgamExamples({
			{R"&((max 0.5 1 7 9 -5))&", R"(9)"},
			{R"&((max (null) 4 8))&", R"(8)"},
			{R"&((max (null)))&", R"((null))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MIN)] = []() {
		OpcodeDetails d;
		d.parameters = R"([number x1] [number x2] ... [number xN])";
		d.returns = R"(number)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to the minimum of all of the numbers.)";
		d.examples = MakeAmalgamExamples({
			{R"&((min 0.5 1 7 9 -5))&", R"(-5)"},
			{R"&((min (null) 4 8))&", R"(4)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_INDEX_MAX)] = []() {
		OpcodeDetails d;
		d.parameters = R"([[number x1] [number x2] [number x3] ... [number xN]] | assoc|list values)";
		d.returns = R"([any])";
		d.allowsConcurrency = true;
		d.description = R"(If given multiple arguments, returns a list of the indices of the arguments with the maximum value.  If given a single argument that is an assoc, it returns the a list of keys associated with the maximum values; the list will be a single value unless there are ties.  If given a single argument that is a list, it returns a list of list indices with the maximum value.)";
		d.examples = MakeAmalgamExamples({
			{R"&((index_max 0.5 -12 3 5 7))&", R"([4])"},
			{R"&((index_max
	[1 1 3 2 1 3]
))&", R"([2 5])"},
			{R"&((index_max (null) 34 -66))&", R"([1])"},
			{R"&((index_max (null) (null) (null)))&", R"((null))"},
			{R"&((index_max
	{1 2 3 5 tomato 4444}
))&", R"(["tomato"])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_INDEX_MIN)] = []() {
		OpcodeDetails d;
		d.parameters = R"([[number x1] [number x2] [number x3] ... [number xN]] | assoc values | list values)";
		d.returns = R"([any])";
		d.allowsConcurrency = true;
		d.description = R"(If given multiple arguments, returns a list of the indices of the arguments with the minimum value.  If given a single argument that is an assoc, it returns the a list of keys associated with the minimum values; the list will be a single value unless there are ties.  If given a single argument that is a list, it returns a list of list indices with the minimum value.)";
		d.examples = MakeAmalgamExamples({
			{R"&((index_min 0.5 -12 3 5 7))&", R"([1])"},
			{R"&((index_min
	[1 1 3 2 1 3]
))&", R"([0 1 4])"},
			{R"&((index_min (null) 34 -66))&", R"([2])"},
			{R"&((index_min
	{1 2 3 5 tomato 4444}
))&", R"([1])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_DOT_PRODUCT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc x1 list|assoc x2)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the sum of all corresponding element-wise products of `x1` and `x2`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((dot_product
	[0.5 0.25 0.25]
	[4 8 8]
))&", R"(6)"},
			{R"&((dot_product
	(associate "a" 0.5 "b" 0.25 "c" 0.25)
	(associate "a" 4 "b" 8 "c" 8)
))&", R"(6)"},
			{R"&((dot_product
	(associate 0 0.5 1 0.25 2 0.25)
	[4 8 8]
))&", R"(6)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_NORMALIZE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc values [number p])";
		d.returns = R"(list|assoc)";
		d.description = R"(Evaluates to a container of the values with the elements normalized, where `p` represents the order of the Lebesgue space to normalize the vector (e.g., 1 is Manhattan or surprisal space, 2 is Euclidean) and defaults to 1.)";
		d.examples = MakeAmalgamExamples({
			{R"&((normalize
	[0.5 0.5 0.5 0.5]
))&", R"([0.25 0.25 0.25 0.25])"},
			{R"&((normalize
	[0.5 0.5 0.5 .infinity]
))&", R"([0 0 0 1])"},
			{R"&((normalize
	{
		a 1
		b 1
		c 1
		d 1
	}
	2
))&", R"({
	a 0.5
	b 0.5
	c 0.5
	d 0.5
})"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MODE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc values [list|assoc weights])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to mode of the `values`.  If `values` is an assoc, it will return the key.  If `weights` is specified and both `values` and `weights` are lists, then the corresponding elements will be weighted by `weights`.  If weights is specified and is an assoc, then each value will be looked up in the `weights`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((mode
	[1 1 2 3 4 5]
))&", R"(1)"},
			{R"&((mode
	[
		1
		1
		2
		3
		4
		5
		5
		5
	]
))&", R"(5)"},
			{R"&((mode
	[
		1
		1
		[]
		[]
		[]
		{}
		{}
	]
))&", R"([])"},
			{R"&((mode
	[
		1
		1
		2
		3
		4
		5
		(null)
	]
))&", R"(1)"},
			{R"&((mode
	[1 1 2 3 4 5]
))&", R"(1)"},
			{R"&((mode
	[1 1 2 3 4 5]
	[0.5 0.1 0.1 0.1 0.1]
))&", R"(1)"},
			{R"&((mode
	{
		a 1
		b 1
		c 3
		d 4
		e 5
	}
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
))&", R"(1)"},
			{R"&((mode
	[1 1 2 3 4 5]
	{0 0.75 4 0.125}
))&", R"(1)"},
			{R"&((mode
	{
		0 1
		1 1
		2 2
		3 3
		4 4
		5 5
	}
	[0.75 0 0 0 0.125]
))&", R"(1)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUANTILE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc values number quantile [list|assoc weights])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the quantile of the `values` specified by `quantile` ranging from 0 to 1.  If `weights` is specified and both `values` and `weights` are lists, then the corresponding elements will be weighted by `weights`.  If `weights` is specified and is an assoc, then each value will be looked up in the `weights`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((quantile
	[1 2 3 4 5]
	0.5
))&", R"(3)"},
			{R"&((quantile
	[1 2 3 4 5 (null)]
	0.5
))&", R"(3)"},
			{R"&((quantile
	[1 2 3 4 5]
	0.5
))&", R"(3)"},
			{R"&((quantile
	[1 2 3 4 5]
	0.5
	[0.5 0.1 0.1 0.1 0.1]
))&", R"(1.6666666666666667)"},
			{R"&((quantile
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
	0.5
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
))&", R"(1.6666666666666667)"},
			{R"&((quantile
	[1 2 3 4 5]
	0.5
	{0 0.75 4 0.125}
))&", R"(1.5714285714285716)"},
			{R"&((quantile
	{
		0 1
		1 2
		2 3
		3 4
		4 5
		5 (null)
	}
	0.5
	[0.75 0 0 0 0.125]
))&", R"(1.1666666666666667)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GENERALIZED_MEAN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc values [number p] [list|assoc weights] [number center] [bool calculate_moment] [bool absolute_value])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the generalized mean of the `values`.  If `p` is specified (which defaults to 1), it is the parameter that can control the type of mean from minimum (negative infinity) to harmonic mean (-1) to geometric mean (0) to arithmetic mean (1) to maximum (infinity).  If `weights` are specified, it uses those when calculating the corresponding values for the generalized mean.  If `center` is specified, calculations will use that as central point, and the default center is is 0.0.  If `calculate_moment` is true, which defaults to false, then the results will not be raised to 1/`p` at the end.  If `absolute_value` is true, which defaults to false, the differences will take the absolute value.  Various parameterizations of generalized_mean can be used to compute moments about the mean, especially setting the calculate_moment parameter to true and using the mean as the center.)";
		d.examples = MakeAmalgamExamples({
			{R"&((generalized_mean
	[1 2 3 4 5]
))&", R"(3)"},
			{R"&((generalized_mean
	[1 2 3 4 5 (null)]
))&", R"(3)"},
			{R"&((generalized_mean
	[1 2 3 4 5]
	2
))&", R"(3.3166247903554)"},
			{R"&((generalized_mean
	[1 2 3 4 5]
	1
	[0.5 0.1 0.1 0.1 0.1]
))&", R"(2.111111111111111)", R"(2.1111111111111\d\d)"},
			{R"&((generalized_mean
	{
		a 1
		b 2
		c 3
		d 4
		e 5
	}
	1
	{
		a 0.5
		b 0.1
		c 0.1
		d 0.1
		e 0.1
	}
))&", R"(2.111111111111111)", R"(2.1111111111111\d\d)"},
			{R"&((generalized_mean
	[1 2 3 4 5]
	1
	{0 0.75 4 0.125}
))&", R"(1.5714285714285714)"},
			{R"&((generalized_mean
	{
		0 1
		1 2
		2 3
		3 4
		4 5
		5 (null)
	}
	1
	[0.75 0 0 0 0.125]
))&", R"(1.5714285714285714)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GENERALIZED_DISTANCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc|* vector1 [list|assoc|* vector2] [number p_value] [list|assoc|assoc of assoc|number weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc|number deviations] [list value_names] [list|string weights_selection_features] [bool surprisal_space])";
		d.returns = R"(number)";
		d.description = R"(Computes the generalized norm between `vector1` and `vector2` (or an equivalent zero vector if unspecified) using the numerical distance or edit distance as appropriate.  The parameter `value_names`, if specified as a list of the names of the values, will transform via unzipping any assoc into a list for the respective parameter in the order of the `value_names`, or if a number will use the number repeatedly for every element.  If any vector value is null or any of the differences between `vector1` and `vector2` evaluate to null, then it will compute a corresponding maximum distance value based on the properties of the feature.  If `surprisal_space` is true, which defaults to false, it will perform all computations in surprisal space.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.)";
		d.examples = MakeAmalgamExamples({
			{R"&((generalized_distance
	(map
		10000
		(range 0 200)
	)
	(null)
	0.01
))&", R"(2.0874003024080013e+234)"},
			{R"&((generalized_distance
	[1 2 3]
	[0 2 3]
	0.01
))&", R"(1)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	2
))&", R"(5)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	-.infinity
))&", R"(3)"},
			{R"&((generalized_distance
	[1 2 3]
	[0 2 3]
	0.01
	[0.3333 0.3333 0.3333]
))&", R"(1.9210176984148622e-48)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	2
	[1 1]
))&", R"(5)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	2
	[0.5 0.5]
))&", R"(3.5355339059327378)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	1
	[0.5 0.5]
))&", R"(3.5)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	0.5
	[0.5 0.5]
))&", R"(3.482050807568877)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	0.1
	[0.5 0.5]
))&", R"(3.467687001077147)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	0.01
	[0.5 0.5]
))&", R"(3.4644599990846436)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	0.001
	[0.5 0.5]
))&", R"(3.4641374518767565)"},
			{R"&((generalized_distance
	[3 4]
	(null)
	0
	[0.5 0.5]
))&", R"(3.4641016151377544)"},
			{R"&((generalized_distance
	[(null) 4]
	(null)
	2
	[1 1]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[(null) 4]
	(null)
	0
	[1 1]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[(null) 4]
	(null)
	2
	[0.5 0.5]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[(null) 4]
	(null)
	0
	[0.5 0.5]
))&", R"(.infinity)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 4]
	1
	(null)
	["nominal_number"]
	[1]
))&", R"(2)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	(null)
	["nominal_number"]
	[1]
))&", R"(8)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	(null)
	["nominal_number"]
	[1]
))&", R"(8)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 4]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1]
))&", R"(0.6666)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1]
))&", R"(2.6664)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1]
))&", R"(2.6664)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number" "continuous_number_cyclic" "continuous_number_cyclic"]
	[1 360 12]
))&", R"(1.9997999999999998)"},
			{R"&((generalized_distance
	[1 2 3]
	[10 2 10]
	1
	[0.3333 0.3333 0.3333]
	["nominal_number"]
	[1.1]
	[0.25 180 -12]
))&", R"(92.57407500000001)"},
			{R"&((generalized_distance
	[4 4 (null)]
	[2 (null) (null)]
	2
	[1 0 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
	[0.1 0.1 0.1]
))&", R"(2.227195548101088)"},
			{R"&((generalized_distance
	[4 4 (null)]
	[2 (null) (null)]
	2
	[1 0 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
))&", R"(2.23606797749979)"},
			{R"&((generalized_distance
	[4 4 (null) 4]
	[2 (null) (null) 2]
	2
	[1 0 1 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
	[0.1 0.1 0.1 0.1]
))&", R"(2.9933927271513525)"},
			{R"&((generalized_distance
	[4 4 (null) 4]
	[2 (null) (null) 2]
	2
	[1 0 1 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
))&", R"(3)"},
			{R"&((generalized_distance
	[4 4 4 4 4]
	[2 (null) 2 2 2]
	1
	[1 0 1 1 1]
))&", R"((null))"},
			{R"&((generalized_distance
	[4 4 4]
	[2 2 2]
	1
	{x 1 y 1 z 1}
	{x "nominal_number" y "continuous_number" z "continuous_number"}
	{z 5}
	(null)
	(null)
	(null)
	["x" "y" "z"]
))&", R"(6)"},
			{R"&((generalized_distance
	[4 4 (null)]
	[2 2 (null)]
	1
	[1 1 1]
	["continuous_number" "nominal_number" "nominal_number"]
	[(null) 5 5]
))&", R"(4)"},
			{R"&((generalized_distance
	[4 4 4 4]
	[2 2 2 (null)]
	0
	[1 1 1 1]
	["continuous_number" "nominal_number" "nominal_number" "continuous_number"]
	[(null) 5 5 (null)]
	[
		[0 2]
		(null)
		(null)
		[0 2]
	]
))&", R"(4)"},
			{R"&((generalized_distance
	[4 "s" "s" 4]
	[2 "s" 2 (null)]
	1
	[1 1 1 1]
	["continuous_number" "nominal_string" "nominal_string" "continuous_number"]
	[(null) 5 5 (null)]
	[
		[0 1]
		(null)
		(null)
		[0 1]
	]
))&", R"(4)"},
			{R"&((generalized_distance
	[
		[1 2 3 4 5]
		"s"
	]
	[
		[1 2 3]
		"s"
	]
	1
	[1 1]
	["continuous_code" "nominal_string"]
	[0 5]
))&", R"(2)"},
			{R"&((generalized_distance
	[
		[1.5 2 3 4 5]
		"s"
	]
	[
		[1 2 3]
		"s"
	]
	1
	[1 1]
	["continuous_code" "nominal_string"]
	[0 5]
))&", R"(3.3255881193876142)"},
			{R"&((generalized_distance
	[1 1]
	[1 1]
	1
	[1 1]
	["continuous_number" "continuous_number"]
	(null)
	[0.5 0.5]
	(null)
	(null)
	.true
))&", R"(0)"},
			{R"&((generalized_distance
	[1 1]
	[1 1]
	1
	[1 1]
	["nominal_number" "nominal_number"]
	(null)
	[0.5 0.5]
	(null)
	(null)
	.true
))&", R"(0)"},
			{R"&((generalized_distance
	[1 1]
	[2 2]
	1
	[1 1]
	["continuous_number" "continuous_number"]
	(null)
	[0.5 0.5]
	(null)
	(null)
	.true
))&", R"(1.6766764161830636)"},
			{R"&((generalized_distance
	[1 1]
	[2 2]
	1
	[1 1]
	["nominal_number" "nominal_number"]
	[2 2]
	[0.25 0.25]
	(null)
	(null)
	.true
))&", R"(2.197224577336219)"},
			{R"&((generalized_distance
	["b"]
	
	;vector 1
	["c"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b {a 0.501736111 b 0.501736111 c 0.996527778}
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
))&", R"(4.966335099422683)"},
			{R"&((generalized_distance
	["b"]
	
	;vector 1
	["a"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b {a 0.501736111 b 0.501736111 c 0.996527778}
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
))&", R"(0)"},
			{R"&((generalized_distance
	["b"]
	
	;vector 1
	["q"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		{
			a {a 0.00744879 b 0.996275605 c 0.996275605}
			b [
					{a 0.501736111 b 0.501736111 c 0.996527778}
					0.8
				]
			c {a 0.996539792 b 0.996539792 c 0.006920415}
		}
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
))&", R"(0.9128124677208268)"},
			{R"&((generalized_distance
	["q"]
	
	;vector 1
	["u"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[2 2]
	
	;attributes
	[0.2]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
))&", R"(1.3862943611198906)"},
			{R"&((generalized_distance
	["q"]
	
	;vector 1
	["u"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		[
			{
				a {a 0.00744879 b 0.996275605 c 0.996275605}
				b [
						{a 0.501736111 b 0.501736111 c 0.996527778}
						0.8
					]
				c {a 0.996539792 b 0.996539792 c 0.006920415}
			}
			0.2
		]
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
))&", R"(1.3862943611198906)"},
			{R"&((generalized_distance
	["q"]
	
	;vector 1
	["u"]
	
	;vector 2
	1
	
	;p
	[1 1]
	
	;weights
	["nominal_string"]
	
	;types
	[4]
	
	;attributes
	[
		[
			[
				{
					a {a 0.00744879 b 0.996275605 c 0.996275605}
					b [
							{a 0.501736111 b 0.501736111 c 0.996527778}
							0.8
						]
					c {a 0.996539792 b 0.996539792 c 0.006920415}
				}
				0.2
			]
			0.2
		]
	]
	
	;deviations
	(null)
	
	;names
	(null)
	
	;weights_selection_feature
	.true
))&", R"(1.3862943611198906)"},
			{R"&((generalized_distance
	{
		A1 1
		A2 1
		A3 1
		B 1
	}
	
	;vector 1
	{
		A1 2
		A2 2
		A3 2
		B 2
	}
	
	;vector 2
	1
	
	;p
	{
		A1 {
				A1 0
				A2 0.372145984
				A3 0.370497589
				B 0.082723928
				sum 0.174632499
			}
		A2 {
				A1 0.371518433
				A2 0
				A3 0.370520996
				B 0.082668725
				sum 0.175291846
			}
		A3 {
				A1 0.370319458
				A2 0.370968492
				A3 0
				B 0.085480882
				sum 0.173231167
			}
		B {
				A1 0.061363751
				A2 0.049512288
				A3 0.05628626
				B 0
				sum 0.832837701
			}
		sum {
				A1 0.114003407
				A2 0.106173002
				A3 0.100958636
				B 0.678864956
				sum 0
			}
	}
	
	;weights
	["continuous_number"]
	
	;types
	(null)
	
	;attributes
	0.5
	
	;deviations
	["A2" "A3" "B"]
	
	;names
	"sum"
	
	;weights_selection_feature
	.true
))&", R"(0.8383382080915319)"},
			{R"&((generalized_distance
	[
		[1.5 2 3 4 5 "s12"]
	]
	[
		[1 2 3 "s21"]
	]
	1
	[1]
	["continuous_code"]
	[{}]
))&", R"(5.325588119387614)"},
			{R"&((generalized_distance
	[
		[1.5 2 3 4 5 "s12"]
	]
	[
		[1 2 3 "s21"]
	]
	1
	[1]
	["continuous_code"]
	[
		{nominal_strings .false types_must_match .false}
	]
))&", R"(3.697640774259515)"},
			{R"&((generalized_distance
	{
		A1 1
		A2 1
		A3 1
		B 1
	}
	
	;vector 1
	{
		A1 2
		A2 2
		A3 2
		B 2
	}
	
	;vector 2
	1
	
	;p
	{
		A1 {
				A1 0
				A2 0.372145984
				A3 0.370497589
				B 0.082723928
				sum 0.174632499
			}
		A2 {
				A1 0.371518433
				A2 0
				A3 0.370520996
				B 0.082668725
				sum 0.175291846
			}
		A3 {
				A1 0.370319458
				A2 0.370968492
				A3 0
				B 0.085480882
				sum 0.173231167
			}
		B {
				A1 0.061363751
				A2 0.049512288
				A3 0.05628626
				B 0
				sum 0.832837701
			}
		sum {
				A1 0.114003407
				A2 0.106173002
				A3 0.100958636
				B 0.678864956
				sum 0
			}
	}
	
	;weights
	["continuous_number"]
	
	;types
	(null)
	
	;attributes
	0.5
	
	;deviations
	["A2" "A3"]
	
	;names
	["sum" "A1" "B"]
	
	;weights_selection_feature
	.true
))&", R"(0.8383382080915318)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ENTROPY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc|number p [list|assoc|number q] [number p_exponent] [number q_exponent])";
		d.returns = R"(number)";
		d.description = R"(Computes a form of entropy on the specified vectors `p` and `q` using nats (natural log, not bits) in the form of -sum p_i ln (p_i^p_exponent * q_i^q_exponent).  For both `p` and `q`, if `p` or `q` is a list of numbers, then it will treat each entry as being the probability of that element.  If it is an associative array, then elements with matching keys will be matched.  If `p` or `q` is a number then it will use that value in place of each element.  If `p` or `q` is null or not specified, it will be calculated as the reciprocal of the size of the other element (p_i would be 1/|q| or q_i would be 1/|p|).  If either `p_exponent` or `q_exponent` is 0, then that exponent will be ignored.  Shannon entropy can be computed by ignoring the q parameters by specifying it as null, setting `p_exponent` to 1 and `q_exponent` to 0. KL-divergence can be computed by providing both `p` and `q` and setting `p_exponent` to -1 and `q_exponent` to 1.  Cross-entropy can be computed by setting `p_exponent` to 0 and `q_exponent` to 1.)";
		d.examples = MakeAmalgamExamples({
			{R"&((entropy
	[0.5 0.5]
))&", R"(0.6931471805599453)"},
			{R"&((entropy
	[0.5 0.5]
	[0.25 0.75]
	-1
	1
))&", R"(0.14384103622589045)"},
			{R"&((entropy
	[0.5 0.5]
	[0.25 0.75]
))&", R"(0.14384103622589045)"},
			{R"&((entropy
	0.5
	[0.25 0.75]
	-1
	1
))&", R"(0.14384103622589045)"},
			{R"&((entropy
	0.5
	[0.25 0.75]
	0
	1
))&", R"(1.6739764335716716)"},
			{R"&((entropy
	{A 0.5 B 0.5}
	{A 0.75 B 0.25}
))&", R"(0.14384103622589045)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_FIRST)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|number|string data])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the first element of `data`.  If `data` is a list, it will be the first element.  If `data` is an assoc, it will evaluate to the first element by assoc storage, but order does not matter.  If `data` is a string, it will be the first character.  If `data` is a number, it will evaluate to 1 if nonzero, 0 if zero.)";
		d.examples = MakeAmalgamExamples({
			{R"&((first
	[4 9.2 "this"]
))&", R"(4)"},
			{R"&((first
	(associate "a" 1 "b" 2)
))&", R"(2)", R"(1|2)"},
			{R"&((first 3))&", R"(1)"},
			{R"&((first 0))&", R"(0)"},
			{R"&((first "abc"))&", R"("a")"},
			{R"&((first ""))&", R"((null))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TAIL)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|number|string data] [number retain_count])";
		d.returns = R"(list)";
		d.description = R"(Evaluates to everything but the first element.  If `data` is a list, it will be a list of all but the first element.  If `data` is an assoc, it will evaluate to the assoc without the first element by assoc storage order, but order does not matter.  If `data` is a string, it will be all but the first character.  If `data` is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero.  If a `retain_count` is specified, it will be the number of elements to retain.  A positive number means from the end, a negative number means from the beginning.  The default value is -1 (all but the first element).)";
		d.examples = MakeAmalgamExamples({
			{R"&((tail
	[4 9.2 "this"]
))&", R"([9.2 "this"])"},
			{R"&((tail
	[1 2 3 4 5 6]
))&", R"([2 3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	2
))&", R"([5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	-2
))&", R"([3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	-6
))&", R"([])"},
			{R"&((tail
	[1 2 3 4 5 6]
	6
))&", R"([1 2 3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	10
))&", R"([1 2 3 4 5 6])"},
			{R"&((tail
	[1 2 3 4 5 6]
	-10
))&", R"([])"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
))&", R"({
	a 1
	b 2
	c 3
	d 4
	f 6
})",
			//just check that some subset worked
			R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	2
))&", R"({b 2 c 3})",
			//just check that some subset worked
			R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-2
))&", R"({
	a 1
	b 2
	c 3
	d 4
})",
			//just check that some subset worked
			R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&"},
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	10
))&", R"({
	a 1
	b 2
	c 3
	d 4
	e 5
	f 6
})",
			//just check that some subset worked
			R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((tail
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-10
))&", R"({})"},
			{R"&((tail 3))&", R"(2)"},
			{R"&((tail 0))&", R"(0)"},
			{R"&((tail "abcdef"))&", R"("bcdef")"},
			{R"&((tail "abcdef" 2))&", R"("ef")"},
			{R"&((tail "abcdef" -2))&", R"("cdef")"},
			{R"&((tail "abcdef" 6))&", R"("abcdef")"},
			{R"&((tail "abcdef" -6))&", R"("")"},
			{R"&((tail "abcdef" 10))&", R"("abcdef")"},
			{R"&((tail "abcdef" -10))&", R"("")"},
			{R"&((tail ""))&", R"((null))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LAST)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|number|string data])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the last element of `data`.  If `data` is a list, it will be the last element.  If `data` is an assoc, it will evaluate to the first element by assoc storage, because order does not matter.  If `data` is a string, it will be the last character.  If `data` is a number, it will evaluate to 1 if nonzero, 0 if zero.)";
		d.examples = MakeAmalgamExamples({
			{R"&((last
	[4 9.2 "this"]
))&", R"("this")"},
			{R"&((last
	(associate "a" 1 "b" 2)
))&", R"(2)", R"(1|2)"},
			{R"&((last 3))&", R"(1)"},
			{R"&((last 0))&", R"(0)"},
			{R"&((last "abc"))&", R"("c")"},
			{R"&((last ""))&", R"((null))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TRUNC)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|number|string data] [number retain_count])";
		d.returns = R"(list)";
		d.description = R"(Truncates, evaluates to everything in `data` but the last element. If `data` is a list, it will be a list of all but the last element.  If `data` is an assoc, it will evaluate to the assoc without the first element by assoc storage order, because order does not matter.  If `data` is a string, it will be all but the last character.  If `data` is a number, it will evaluate to the value minus 1 if nonzero, 0 if zero. If `truncate_count` is specified, it will be the number of elements to retain.  A positive number means from the beginning, a negative number means from the end.  The default value is -1, indicating all but the last.)";
		d.examples = MakeAmalgamExamples({
			{R"&((trunc
	[4 9.2 "end"]
))&", R"([4 9.2])"},
			{R"&((trunc
	[1 2 3 4 5 6]
))&", R"([1 2 3 4 5])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	2
))&", R"([1 2])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	-2
))&", R"([1 2 3 4])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	-6
))&", R"([])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	6
))&", R"([1 2 3 4 5 6])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	10
))&", R"([1 2 3 4 5 6])"},
			{R"&((trunc
	[1 2 3 4 5 6]
	-10
))&", R"([])"},
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
))&", R"({
	a 1
	c 3
	d 4
	e 5
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	2
))&", R"({e 5 f 6})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-2
))&", R"({
	c 3
	d 4
	e 5
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	10
))&", R"({
	a 1
	b 2
	c 3
	d 4
	e 5
	f 6
})",
//just check that some subset worked
R"&(^\s*\{\s*
(?:a\s+1\s*)?
(?:b\s+2\s*)?
(?:c\s+3\s*)?
(?:d\s+4\s*)?
(?:e\s+5\s*)?
(?:f\s+6\s*)?
\}$)&" },
			{R"&((trunc
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
		"e"
		5
		"f"
		6
	)
	-10
))&", R"({})"},
			{R"&((trunc 3))&", R"(2)"},
			{R"&((trunc 0))&", R"(0)"},
			{R"&((trunc "abcdef"))&", R"("abcde")"},
			{R"&((trunc "abcdef" 2))&", R"("ab")"},
			{R"&((trunc "abcdef" -2))&", R"("abcd")"},
			{R"&((trunc "abcdef" 6))&", R"("abcdef")"},
			{R"&((trunc "abcdef" -6))&", R"("")"},
			{R"&((trunc "abcdef" 10))&", R"("abcdef")"},
			{R"&((trunc "abcdef" -10))&", R"("")"},
			{R"&((trunc ""))&", R"((null))"},

			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_APPEND)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|* collection1] [list|assoc|* collection2] ... [list|assoc|* collectionN])";
		d.returns = R"(list|assoc)";
		d.description = R"(Evaluates to a new list or assoc which merges all lists, `collection1` through `collectionN`, based on parameter order. If any assoc is passed in, then returns an assoc (lists will be automatically converted to an assoc with the indices as keys and the list elements as values). If a non-list and non-assoc is specified, then it just adds that one element to the list)";
		d.examples = MakeAmalgamExamples({
			{R"&((append
	[1 2 3]
	[4 5 6]
	[7 8 9]
))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	9
])"},
			{R"&((append
	[1 2 3]
	(associate "a" 4 "b" 5 "c" 6)
	[7 8 9]
	(associate "d" 10 "e" 11)
))&", R"({
	0 1
	1 2
	2 3
	3 7
	4 8
	5 9
	a 4
	b 5
	c 6
	d 10
	e 11
})"},
			{R"&((append
	[4 9.2 "this"]
	"end"
))&", R"([4 9.2 "this" "end"])"},
			{R"&((append
	(associate 0 4 1 9.2 2 "this")
	"end"
))&", R"({
	0 4
	1 9.2
	2 "this"
	3 "end"
})"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SIZE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc|string collection] collection)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the size of the `collection` in number of elements.  If `collection` is a string, returns the length in UTF-8 characters.)";
		d.examples = MakeAmalgamExamples({
			{R"&((size
	[4 9.2 "this"]
))&", R"(3)"},
			{R"&((size
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
))&", R"(4)"},
			{R"&((size "hello"))&", R"(5)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_RANGE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* function] number low_endpoint number high_endpoint [number step_size])";
		d.returns = R"(list)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to a list with the range from `low_endpoint` to `high_endpoint`.  The default `step_size` is 1.  Evaluates to an empty list if the range is not valid.  If four arguments are specified, then `function` will be evaluated for each value in the range.)";
		d.examples = MakeAmalgamExamples({
			{R"&((range 0 10))&", R"([
	0
	1
	2
	3
	4
	5
	6
	7
	8
	9
	10
])"},
			{R"&((range 10 0))&", R"([
	10
	9
	8
	7
	6
	5
	4
	3
	2
	1
	0
])"},
			{R"&((range 0 5 0))&", R"([])"},
			{R"&((range 0 5 1))&", R"([0 1 2 3 4 5])"},
			{R"&((range 12 0 5 1))&", R"([12 12 12 12 12 12])"},
			{R"&((range
	(lambda
		(+ (current_index) 1)
	)
	0
	5
	1
))&", R"([1 2 3 4 5 6])"},
			{R"&(||(range
	(lambda
		(+ (current_index) 1)
	)
	0
	5
	1
))&", R"([1 2 3 4 5 6])"}
			});
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_REWRITE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* function * target)";
		d.returns = R"(any)";
		d.description = R"(Rewrites `target` by applying the `function` in a bottom-up manner.  For each node in the `target` structure, it pushes a new target scope onto the target stack, with `(current_value)` being the current node and `(current_index)` being to the index to the current node relative to the node passed into rewrite accessed via target, and evaluates `function`.  Returns the resulting structure, after have been rewritten by function.  Note that there is a small performance overhead if `target` is a graph structure rather than a tree structure.)";
		d.examples = MakeAmalgamExamples({
			{R"&((rewrite
	(lambda
		(if
			(~ (current_value) 0)
			(+ (current_value) 1)
			(current_value)
		)
	)
	[
		(associate "a" 13)
	]
))&", R"([
	{a 14}
])"},
			{R"&(;rewrite all integer additions into multiplies and then fold constants
(rewrite
	(lambda
		
		;find any nodes with a + and where its list is filled to its size with integers
		(if
			(and
				(=
					(get_type (current_value))
					(lambda (+))
				)
				(=
					(size (current_value))
					(size
						(filter
							(lambda
								(~ (current_value) 0)
							)
							(current_value)
						)
					)
				)
			)
			(reduce
				(lambda
					(* (previous_result) (current_value))
				)
				(current_value)
			)
			(current_value)
		)
	)
	
	;original code with additions to be rewritten
	(lambda
		[
			(associate
				"a"
				(+
					3
					(+ 13 4 2)
				)
			)
		]
	)
))&", R"([
	(associate "a" 312)
])"},
			{R"&(;rewrite numbers as sums of position in the list and the number (all 8s)
(rewrite
	(lambda
		
		;find any nodes with a + and where its list is filled to its size with integers
		(if
			(=
				(get_type_string (current_value))
				"number"
			)
			(+
				(current_value)
				(get_value (current_index))
			)
			(current_value)
		)
	)
	
	;original code with additions to be rewritten
	(lambda
		[
			8
			7
			6
			5
			4
			3
			2
			1
			0
		]
	)
))&", R"([
	8
	8
	8
	8
	8
	8
	8
	8
	8
])"},
			{R"&((rewrite
	(lambda
		(if
			(and
				(=
					(get_type (current_value))
					(lambda (+))
				)
				(=
					(size (current_value))
					(size
						(filter
							(lambda
								(~ (current_value) 0)
							)
							(current_value)
						)
					)
				)
			)
			(reduce
				(lambda
					(+ (previous_result) (current_value))
				)
				(current_value)
			)
			(current_value)
		)
	)
	(lambda
		(+
			(+ 13 4)
			a
		)
	)
))&", R"((+ 17 a))"}
			});
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MAP)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* function [list|assoc collection1] [list|assoc collection2] ... [list|assoc collectionN])";
		d.returns = R"(list)";
		d.allowsConcurrency = true;
		d.description = R"(For each element in the collection, pushes a new target scope onto the stack, so that `(current_value)` accesses the element or elements in the list and `(current_index)` accesses the list or assoc index, with `(target)` representing the outer set of lists or assocs, and evaluates the function.  Returns the list of results, mapping the list via the specified `function`.  If multiple lists or assocs are specified, then it pulls from each list or assoc simultaneously (null if overrun or index does not exist) and `(current_value)` contains an array of the values in parameter order.  Note that concurrency is only available when more than one one collection is specified.)";
		d.examples = MakeAmalgamExamples({
						{R"&((map
	(lambda
		(* (current_value) 2)
	)
	[1 2 3 4]
))&", R"([2 4 6 8])"},
			{R"&((map
	(lambda
		(+ (current_value) (current_index))
	)
	[
		10
		1
		20
		2
		30
		3
		40
		4
	]
))&", R"([
	10
	2
	22
	5
	34
	8
	46
	11
])"},
			{R"&((map
	(lambda
		(+ (current_value) (current_index))
	)
	(associate
		10
		1
		20
		2
		30
		3
		40
		4
	)
))&", R"({
	10 11
	20 22
	30 33
	40 44
})"},
			{R"&((map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
		)
	)
	[1 2 3 4 5 6]
	[2 2 2 2 2 2]
))&", R"([3 4 5 6 7 8])"},
			{R"&((map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
		)
	)
	[1 2 3 4 5]
	[2 2 2 2 2 2]
))&", R"([3 4 5 6 7 (null)])"},
			{R"&((map
	(lambda
		(+
			(get (current_value) 0)
			(get (current_value) 1)
			(get (current_value) 2)
		)
	)
	(associate 0 0 1 1 "a" 3)
	(associate 0 1 "a" 4)
	[2 2 2 2]
))&", R"({
	0 3
	1 (null)
	2 (null)
	3 (null)
	a (null)
})"}
			});
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_FILTER)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* function] list|assoc collection)";
		d.returns = R"(list|assoc)";
		d.allowsConcurrency = true;
		d.description = R"(For each element in the `collection`, pushes a new target scope onto the stack, so that `(current_value)` accesses the element in the list and `(current_index)` accesses the list or assoc index, with `(target)` representing the original list or assoc, and evaluates the function.  If `function` evaluates to true, then the element is put in a new list or assoc (matching the input type) that is returned.  If function is omitted, then it will remove any elements in the collection that are null.)";
		d.examples = MakeAmalgamExamples({
			{R"&((filter
	(lambda
		(> (current_value) 2)
	)
	[1 2 3 4]
))&", R"([3 4])"},
			{R"&((filter
	(lambda
		(< (current_index) 3)
	)
	[
		10
		1
		20
		2
		30
		3
		40
		4
	]
))&", R"([10 1 20])"},
			{R"&((filter
	(lambda
		(< (current_index) 20)
	)
	(associate
		10
		1
		20
		2
		30
		3
		40
		4
	)
))&", R"({10 1})"},
			{R"&((filter
	[
		10
		1
		20
		(null)
		30
		(null)
		(null)
		40
		4
	]
))&", R"([10 1 20 30 40 4])"},
			{R"&((filter
	[
		10
		1
		20
		(null)
		30
		""
		40
		4
	]
))&", R"([
	10
	1
	20
	30
	""
	40
	4
])"},
			{R"&((filter
	{
		a 10
		b 1
		c 20
		d ""
		e 30
		f 3
		g (null)
		h 4
	}
))&", R"({
	a 10
	b 1
	c 20
	d ""
	e 30
	f 3
	h 4
})"},
			{R"&((filter
	{
		a 10
		b 1
		c 20
		d ""
		e 30
		f 3
		g (null)
		h 4
	}
))&", R"({
	a 10
	b 1
	c 20
	d ""
	e 30
	f 3
	h 4
})"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_WEAVE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* function] list|immediate values1 [list|immediate values2] [list|immediate values3]...)";
		d.returns = R"(list)";
		d.description = R"(Interleaves the values lists optionally by applying a function.  If only `values1` is passed in, then it evaluates to `values1`. If `values1` and `values2` are passed in, or, if more values are passed in but function is null, it interleaves the lists and extends the result to the length of the longest list, filling in the remainder with null.  If any of the value parameters are immediate, then it will repeat that immediate value when weaving.  If the `function` is specified and not null, it pushes a new target scope onto the stack, so that `(current_value)` accesses a list of elements to be woven together from the list, and `(current_index)` accesses the list or assoc index, with `(target)` representing the resulting list or assoc.  The `function` should evaluate to a list, and weave will evaluate to a concatenated list of all of the lists that the function evaluated to.)";
		d.examples = MakeAmalgamExamples({
			{R"&((weave
	[1 2 3]
))&", R"([1 2 3])"},
			{R"&((weave
	[1 3 5]
	[2 4 6]
))&", R"([1 2 3 4 5 6])"},
			{R"&((weave
	(null)
	[2 4 6]
	(null)
))&", R"([2 (null) 4 (null) 6 (null)])"},
			{R"&((weave
	"a"
	[2 4 6]
))&", R"(["a" 2 @(target .true 0) 4 @(target .true 0) 6])"},
			{R"&((weave
	(null)
	[1 4 7]
	[2 5 8]
	[3 6 9]
))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	9
])"},
			{R"&((weave
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	9
	10
	11
	12
])"},
			{R"&((weave
	(lambda (current_value))
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	9
	10
	11
	12
])"},
			{R"&((weave
	(lambda
		(map
			(lambda
				(* 2 (current_value))
			)
			(current_value)
		)
	)
	[1 3 5 7 9 11]
	[2 4 6 8 10 12]
))&", R"([
	2
	4
	6
	8
	10
	12
	14
	16
	18
	20
	22
	24
])"},
			{R"&((weave
	(lambda
		[
			(apply
				"min"
				(current_value 1)
			)
		]
	)
	[1 3 4 5 5 6]
	[2 2 3 4 6 7]
))&", R"([1 2 3 4 5 6])"},
			{R"&((weave
	(lambda
		(if
			(<=
				(get (current_value) 0)
				4
			)
			[
				(apply
					"min"
					(current_value 1)
				)
			]
			(current_value)
		)
	)
	[1 3 4 5 5 6]
	[2 2 3 4 6 7]
))&", R"([
	1
	2
	3
	5
	4
	5
	6
	6
	7
])"},
			{R"&((weave
	(lambda
		(if
			(>=
				(first (current_value))
				3
			)
			[
				(first
					(current_value 1)
				)
			]
			[]
		)
	)
	[1 2 3 4 5]
	(null)
))&", R"([3 4 5])"}
			});
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_REDUCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* function list|assoc collection)";
		d.returns = R"(any)";
		d.description = R"(For each element in the `collection` after the first one, it evaluates `function` with a new scope on the stack where `(current_value)` accesses each of the elements from the `collection`, `(current_index)` accesses the list or assoc index and `(previous_result)` accesses the previously reduced result.  If the `collection` is empty, null is returned.  If the `collection` is of size one, the single element is returned.)";
		d.examples = MakeAmalgamExamples({
			{R"&((reduce
	(lambda
		(* (current_value) (previous_result))
	)
	[1 2 3 4]
))&", R"(24)"},
			{R"&((reduce
	(lambda
		(* (current_value) (previous_result))
	)
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		"d"
		4
	)
))&", R"(24)"}
			});
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_APPLY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* to_apply [list|assoc collection])";
		d.returns = R"(any)";
		d.description = R"(Creates a new list of the values of the elements of the `collection`, applies the type specified by `to_apply`, which is either the type corresponding to a string or the type of `to_apply`, and then evaluates it.  If `to_apply` has any parameters, i.e., it is a node with one or more elements, these are prepended to the `collection` as the first parameters.  When no extra parameters are passed, it is a more efficient equivalent to `(call (set_type type collection))`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((apply
	(lambda (+))
	[1 2 3 4]
))&", R"(10)"},
			{R"&((apply
	(lambda
		(+ 5)
	)
	[1 2 3 4]
))&", R"(15)"},
			{R"&((apply
	"+"
	[1 2 3 4]
))&", R"(10)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_REVERSE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list collection)";
		d.returns = R"(list)";
		d.description = R"(Returns a new list containing the `collection` with its elements in reversed order.)";
		d.examples = MakeAmalgamExamples({
			{R"&((reverse
	[1 2 3 4 5]
))&", R"([5 4 3 2 1])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SORT)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* function] list|assoc collection [number k])";
		d.returns = R"(list)";
		d.description = "Returns a new list containing the elements from `collection` sorted in increasing order, regardless of whether `collection` is an assoc or list.  If `function` is null or true it sorts ascending, if false it sorts descending, and if any other value it pushes a pair of new scope onto the stack with `(current_value)` and `(current_value 1)` accessing a pair of elements from the list, and evaluates `function`.  The function should return a number, positive if `(current_value)` is greater, negative if `(current_value 1)` is greater, or 0 if equal.  If `k` is specified in addition to `function` and not null, then it will only return the `k` smallest values sorted in order, or, if `k` is negative, it will return the highest `k` values using the absolute value of `k`.";
		d.examples = MakeAmalgamExamples({
			{R"&((sort
	[4 9 3 5 1]
))&", R"([1 3 4 5 9])"},
			{R"&((sort
	{
		a 4
		b 9
		c 3
		d 5
		e 1
	}
))&", R"([1 3 4 5 9])"},
			{R"&((sort
	[
		"n"
		"b"
		"hello"
		"soy"
		4
		1
		3.2
		[1 2 3]
	]
))&", R"([
	1
	3.2
	4
	[1 2 3]
	"b"
	"hello"
	"n"
	"soy"
])"},
			{R"&((sort
	[
		1
		"1x"
		"10"
		20
		"z2"
		"z10"
		"z100"
	]
))&", R"([
	1
	20
	"1x"
	"10"
	"z2"
	"z10"
	"z100"
])"},
			{R"&((sort
	[
		1
		"001x"
		"010"
		20
		"z002"
		"z010"
		"z100"
	]
))&", R"([
	1
	20
	"001x"
	"010"
	"z002"
	"z010"
	"z100"
])"},
			{R"&((sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
))&", R"([1 3 4 5 9])"},
			{R"&((sort
	(lambda
		(- (rand) (rand))
	)
	(range 0 10)
))&", R"([
	8
	10
	6
	9
	7
	5
	1
	0
	2
	4
	3
])"},
			{R"&((sort
	[
		"2020-06-08 lunes 11.33.36"
		"2020-06-08 lunes 11.32.47"
		"2020-06-08 lunes 11.32.49"
		"2020-06-08 lunes 11.32.37"
		"2020-06-08 lunes 11.33.48"
		"2020-06-08 lunes 11.33.40"
		"2020-06-08 lunes 11.33.45"
		"2020-06-08 lunes 11.33.42"
		"2020-06-08 lunes 11.33.47"
		"2020-06-08 lunes 11.33.43"
		"2020-06-08 lunes 11.33.38"
		"2020-06-08 lunes 11.33.39"
		"2020-06-08 lunes 11.32.36"
		"2020-06-08 lunes 11.32.38"
		"2020-06-08 lunes 11.33.37"
		"2020-06-08 lunes 11.32.58"
		"2020-06-08 lunes 11.33.44"
		"2020-06-08 lunes 11.32.48"
		"2020-06-08 lunes 11.32.46"
		"2020-06-08 lunes 11.32.57"
		"2020-06-08 lunes 11.33.41"
		"2020-06-08 lunes 11.32.39"
		"2020-06-08 lunes 11.32.59"
		"2020-06-08 lunes 11.32.56"
		"2020-06-08 lunes 11.33.46"
	]
))&", R"([
	"2020-06-08 lunes 11.32.36"
	"2020-06-08 lunes 11.32.37"
	"2020-06-08 lunes 11.32.38"
	"2020-06-08 lunes 11.32.39"
	"2020-06-08 lunes 11.32.46"
	"2020-06-08 lunes 11.32.47"
	"2020-06-08 lunes 11.32.48"
	"2020-06-08 lunes 11.32.49"
	"2020-06-08 lunes 11.32.56"
	"2020-06-08 lunes 11.32.57"
	"2020-06-08 lunes 11.32.58"
	"2020-06-08 lunes 11.32.59"
	"2020-06-08 lunes 11.33.36"
	"2020-06-08 lunes 11.33.37"
	"2020-06-08 lunes 11.33.38"
	"2020-06-08 lunes 11.33.39"
	"2020-06-08 lunes 11.33.40"
	"2020-06-08 lunes 11.33.41"
	"2020-06-08 lunes 11.33.42"
	"2020-06-08 lunes 11.33.43"
	"2020-06-08 lunes 11.33.44"
	"2020-06-08 lunes 11.33.45"
	"2020-06-08 lunes 11.33.46"
	"2020-06-08 lunes 11.33.47"
	"2020-06-08 lunes 11.33.48"
])"},
			{R"&((sort
	(null)
	[4 9 3 5 1]
	2
))&", R"([1 3])"},
			{R"&((sort
	(null)
	[4 9 3 5 1]
	-2
))&", R"([5 9])"},
			{R"&((sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
	2
))&", R"([1 3])"},
			{R"&((sort
	(lambda
		(-
			(current_value)
			(current_value 1)
		)
	)
	[4 9 3 5 1]
	-2
))&", R"([9 5])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_INDICES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc collection)";
		d.returns = R"(list of string|number)";
		d.description = R"(Evaluates to the list of strings or numbers that comprise the indices for the list or associative parameter `collection`.  It is guaranteed that the opcodes indices and values will evaluate and return elements in the same order when given the same node.)";
		d.examples = MakeAmalgamExamples({
			{R"&((sort
	(indices
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
	)
))&", R"([4 "a" "b" "c"])"},
			{R"&((indices
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
))&", R"([
	0
	1
	2
	3
	4
	5
	6
	7
])"},
			{R"&((indices
	(range 0 3)
))&", R"([0 1 2 3])"},
			{R"&((sort
	(indices
		(zip
			(range 0 3)
		)
	)
))&", R"([0 1 2 3])"},
			{R"&((sort
	(indices
		(zip
			[0 1 2 3]
		)
	)
))&", R"([0 1 2 3])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_VALUES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc collection [bool only_unique_values])";
		d.returns = R"(list of any)";
		d.description = R"(Evaluates to the list of entities that comprise the values for the list or associative list `collection`.  If `only_unique_values` is true (defaults to false), then it will filter out any duplicate values and only return those that are unique, preserving their order of first appearance.  If `only_unique_values` is not true, then it is guaranteed that the opcodes indices and values will evaluate and return elements in the same order when given the same node.)";
		d.examples = MakeAmalgamExamples({
			{R"&((sort
	(values
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
	)
))&", R"([1 2 3 "d"])"},
			{R"&((values
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
))&", R"([
	"a"
	1
	"b"
	2
	"c"
	3
	4
	"d"
])"},
			{R"&((values
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
		1
		2
		3
		4
		"a"
		"b"
		"c"
	]
	.true
))&", R"([
	"a"
	1
	"b"
	2
	"c"
	3
	4
	"d"
])"},
			{R"&((sort
	(values
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
			"e"
			1
		)
		.true
	)
))&", R"([1 2 3 "d"])"},
			{R"&((values
	(append
		(range 1 20)
		(range 1 20)
	)
	.true
))&", R"([
	1
	2
	3
	4
	5
	6
	7
	8
	9
	10
	11
	12
	13
	14
	15
	16
	17
	18
	19
	20
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CONTAINS_INDEX)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc collection string|number|list index)";
		d.returns = R"(bool)";
		d.description = R"(Evaluates to true if the index is in the `collection`.  If index is a string, it will attempt to look at `collection` as an assoc, if number, it will look at `collection` as a list.  If index is a list, it will traverse a via the elements in the list as a walk path, with each element .)";
		d.examples = MakeAmalgamExamples({
			{R"&((contains_index
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"c"
))&", R"(.true)"},
			{R"&((contains_index
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	"m"
))&", R"(.false)"},
			{R"&((contains_index
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	2
))&", R"(.true)"},
			{R"&((contains_index
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	100
))&", R"(.false)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CONTAINS_VALUE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc|string collection_or_string string|number value)";
		d.returns = R"(bool)";
		d.description = R"(Evaluates to true if the `value` is contained in `collection_or_string`.  If `collection_or_string` is a string, then it uses `value` as a regular expression and evaluates to true if the regular expression matches.)";
		d.examples = MakeAmalgamExamples({
			{R"&((contains_value
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	1
))&", R"(.true)"},
			{R"&((contains_value
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	44
))&", R"(.false)"},
			{R"&((contains_value
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	"d"
))&", R"(.true)"},
			{R"&((contains_value
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	100
))&", R"(.false)"},
			{R"&((contains_value "hello world" ".*world"))&", R"(.true)"},
			{R"&((contains_value "abcdefg" "a.*g"))&", R"(.true)"},
			{R"&((contains_value "3.141" "[0-9]+\\.[0-9]+"))&", R"(.true)"},
			{R"&((contains_value "3.141" "\\d+\\.\\d+"))&", R"(.true)"},
			{R"&((contains_value "3.a141" "\\d+\\.\\d+"))&", R"(.false)"},
			{R"&((contains_value "abc\r\n123" "(.|\r)*\n.*"))&", R"(.true)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_REMOVE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc collection number|string|list index)";
		d.returns = R"(list|assoc)";
		d.description = R"(Removes the index-value pair with `index` being the index in assoc or index of `collection`, returning a new list or assoc with `index` removed.  If `index` is a list of numbers or strings, then it will remove each of the requested indices.  Negative numbered indices will count back from the end of a list.)";
		d.examples = MakeAmalgamExamples({
			{R"&((sort
	(remove
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
		4
	)
))&", R"([1 2 3])"},
			{R"&((remove
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	4
))&", R"([
	"a"
	1
	"b"
	2
	3
	4
	"d"
])"},
			{R"&((sort
	(remove
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
		[4 "a"]
	)
))&", R"([2 3])"},
			{R"&((remove
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	[4 "a"]
))&", R"([
	"a"
	1
	"b"
	2
	3
	4
	"d"
])"},
			{R"&((remove
	[0 1 2 3 4 5]
	[0 2]
))&", R"([1 3 4 5])"},
			{R"&((remove
	[0 1 2 3 4 5]
	-1
))&", R"([0 1 2 3 4])"},
			{R"&((remove
	[0 1 2 3 4 5]
	[0 -1]
))&", R"([1 2 3 4])"},
			{R"&((remove
	[0 1 2 3 4 5]
	[
		5
		0
		1
		2
		3
		4
		5
		6
	]
))&", R"([])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_KEEP)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|assoc collection number|string|list index)";
		d.returns = R"(list|assoc)";
		d.description = R"(Keeps only the index-value pair with index being the index in `collection`, returning a new list or assoc with only that index.  If `index` is a list of numbers or strings, then it will only keep those requested indices.  Negative numbered indices will count back from the end of a list.)";
		d.examples = MakeAmalgamExamples({
			{R"&((keep
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
	4
))&", R"({4 "d"})"},
			{R"&((keep
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	4
))&", R"(["c"])"},
			{R"&((sort
	(keep
		(associate
			"a"
			1
			"b"
			2
			"c"
			3
			4
			"d"
		)
		[4 "a"]
	)
))&", R"([1 "d"])"},
			{R"&((keep
	[
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	]
	[4 "a"]
))&", R"(["c"])"},
			{R"&((keep
	[0 1 2 3 4 5]
	[0 2]
))&", R"([0 2])"},
			{R"&((keep
	[0 1 2 3 4 5]
	-1
))&", R"([5])"},
			{R"&((keep
	[0 1 2 3 4 5]
	[0 -1]
))&", R"([0 5])"},
			{R"&((keep
	[0 1 2 3 4 5]
	[
		5
		0
		1
		2
		3
		4
		5
		6
	]
))&", R"([0 1 2 3 4 5])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ASSOCIATE)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* index1] [* value1] [* index2] [* value2] ... [* indexN] [* valueN])";
		d.returns = R"(assoc)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to the assoc, where each pair of parameters (e.g., `index1` and `value1`) comprises a index/value pair.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the assoc, the current index, and the current value.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unparse
	(associate
		"a"
		1
		"b"
		2
		"c"
		3
		4
		"d"
	)
))&", R"("{4 \"d\" a 1 b 2 c 3}")"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ZIP)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* function] list indices [* values])";
		d.returns = R"(assoc)";
		d.description = R"(Evaluates to a new assoc where `indices` are the keys and `values` are the values, with corresponding positions in the list matched.  If the `values` is omitted and only one parameter is specified, then it will use nulls for each of the values.  If `values` is not a list, then all of the values in the assoc returned are set to the same value.  When two parameters are specified, it is the `indices` and `values`.  When three values are specified, it is the `function`, indices, and values.  The parameter `values` defaults to null and `function` defaults to `(lambda (current_value))`.  When there is a collision of indices, `function` is called with a of new target scope pushed onto the stack, so that `(current_value)` accesses a list of elements from the list, `(current_index)` accesses the list or assoc index if it is not already reduced, and `(target)` represents the original list or assoc.  When evaluating `function`, existing indices will be overwritten.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unparse
	(zip
		["a" "b" "c" "d"]
		[1 2 3 4]
	)
))&", R"("{a 1 b 2 c 3 d 4}")"},
			{R"&((unparse
	(zip
		["a" "b" "c" "d"]
	)
))&", R"("{a (null) b (null) c (null) d (null)}")"},
			{R"&((unparse
	(zip
		["a" "b" "c" "d"]
		3
	)
))&", R"("{a 3 b (target .true \"a\") c (target .true \"a\") d (target .true \"a\")}")"},
			{R"&((unparse
	(zip
		(lambda (current_value))
		["a" "b" "c" "d" "a"]
		[1 2 3 4 4]
	)
))&", R"("{a 4 b 2 c 3 d 4}")"},
			{R"&((unparse
	(zip
		(lambda
			(+
				(current_value 1)
				(current_value)
			)
		)
		["a" "b" "c" "d" "a"]
		[1 2 3 4 4]
	)
))&", R"("{a 5 b 2 c 3 d 4}")"},
			{R"&((unparse
	(zip
		(lambda
			(+
				(current_value 1)
				(current_value)
			)
		)
		["a" "b" "c" "d" "a"]
		1
	)
))&", R"("{a 2 b 1 c (target .true \"b\") d (target .true \"b\")}")"}
			});
		d.newTargetScope = true;
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_UNZIP)] = []() {
		OpcodeDetails d;
		d.parameters = R"([list|assoc collection] list indices)";
		d.returns = R"(list)";
		d.description = R"(Evaluates to a new list, using `indices` to look up each value from the `collection` in the same order as each index is specified in `indices`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unzip
	[1 2 3]
	[0 -1 1]
))&", R"([1 3 2])"},
			{R"&((unzip
	(associate "a" 1 "b" 2 "c" 3)
	["a" "b"]
))&", R"([1 2])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_AND)] = []() {
		OpcodeDetails d;
		d.parameters = R"([bool condition1] [bool condition2] ... [bool conditionN])";
		d.returns = R"(any)";
		d.allowsConcurrency = true;
		d.description = R"(If all condition expressions are true, evaluates to `conditionN`.  Otherwise evaluates to false.)";
		d.examples = MakeAmalgamExamples({
			{R"&((and 1 4.8 "true" .true))&", R"(.true)"},
			{R"&((and 1 0 "true" .true))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_OR)] = []() {
		OpcodeDetails d;
		d.parameters = R"([bool condition1] [bool condition2] ... [bool conditionN])";
		d.returns = R"(any)";
		d.allowsConcurrency = true;
		d.description = R"(If all condition expressions are false, evaluates to false.  Otherwise evaluates to the first condition that is true.)";
		d.examples = MakeAmalgamExamples({
			{R"&((or .true .false))&", R"(.true)"},
			{R"&((or .false .false .false))&", R"(.false)"},
			{R"&((or 1 0 "true"))&", R"(1)"},
			{R"&((or 1 4.8 "true"))&", R"(1)"},
			{R"&((or 0 0 ""))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_XOR)] = []() {
		OpcodeDetails d;
		d.parameters = R"([bool condition1] [bool condition2] ... [bool conditionN])";
		d.returns = R"(any)";
		d.allowsConcurrency = true;
		d.description = R"(If an even number of condition expressions are true, evaluates to false.  Otherwise evaluates to true.)";
		d.examples = MakeAmalgamExamples({
			{R"&((xor .true .true))&", R"(.false)"},
			{R"&((xor .true .false))&", R"(.true)"},
			{R"&((xor 1 4.8 "true"))&", R"(.true)"},
			{R"&((xor 1 0 "true"))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_NOT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(bool condition)";
		d.returns = R"(bool)";
		d.description = R"(Evaluates to false if `condition` is true, true if false.)";
		d.examples = MakeAmalgamExamples({
			{R"&((not .true))&", R"(.false)"},
			{R"&((not .false))&", R"(.true)"},
			{R"&((not 1))&", R"(.false)"},
			{R"&((not ""))&", R"(.true)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_EQUAL)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to true if the value of all nodes are equal, false otherwise. Values of null are considered equal, and any complex data structures will be traversed evaluated for deep equality.)";
		d.examples = MakeAmalgamExamples({
			{R"&((= 4 4 5))&", R"(.false)"},
			{R"&((= 4 4 4))&", R"(.true)"},
			{R"&((=
	(sqrt -1)
	(null)
))&", R"(.true)"},
			{R"&((= (null) (null)))&", R"(.true)"},
			{R"&((= .infinity .infinity))&", R"(.true)"},
			{R"&((= .infinity -.infinity))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_NEQUAL)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to true if no two values are equal, false otherwise.  Values of null are considered equal, and any complex data structures will be traversed evaluated for deep equality.)";
		d.examples = MakeAmalgamExamples({
			{R"&((!= 4 4))&", R"(.false)"},
			{R"&((!= 4 5))&", R"(.true)"},
			{R"&((!= 4 4 5))&", R"(.false)"},
			{R"&((!= 4 4 4))&", R"(.false)"},
			{R"&((!= 4 4 "hello" 4))&", R"(.false)"},
			{R"&((!= 4 4 4 1 3 "hello"))&", R"(.false)"},
			{R"&((!=
	1
	2
	3
	4
	5
	6
	"hello"
))&", R"(.true)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LESS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to true if all values are in strict increasing order, false otherwise.)";
		d.examples = MakeAmalgamExamples({
			{R"&((< 4 5))&", R"(.true)"},
			{R"&((< 4 4))&", R"(.false)"},
			{R"&((< 4 5 6))&", R"(.true)"},
			{R"&((< 4 5 6 5))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LEQUAL)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to true if all values are in nondecreasing order, false otherwise.)";
		d.examples = MakeAmalgamExamples({
			{R"&((<= 4 5))&", R"(.true)"},
			{R"&((<= 4 4))&", R"(.true)"},
			{R"&((<= 4 5 6))&", R"(.true)"},
			{R"&((<= 4 5 6 5))&", R"(.false)"},
			{R"&((<= (null) 2))&", R"(.false)"},
			{R"&((<= 2 (null)))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GREATER)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to true if all values are in strict decreasing order, false otherwise.)";
		d.examples = MakeAmalgamExamples({
			{R"&((> 6 5))&", R"(.true)"},
			{R"&((> 4 4))&", R"(.false)"},
			{R"&((> 6 5 4))&", R"(.true)"},
			{R"&((> 6 5 4 5))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GEQUAL)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to true if all values are in nonincreasing order, false otherwise.)";
		d.examples = MakeAmalgamExamples({
			{R"&((>= 6 5))&", R"(.true)"},
			{R"&((>= 4 4))&", R"(.true)"},
			{R"&((>= 6 5 4))&", R"(.true)"},
			{R"&((>= 6 5 4 5))&", R"(.false)"},
			{R"&((>= (null) 2))&", R"(.false)"},
			{R"&((>= 2 (null)))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TYPE_EQUALS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to true if all values are of the same data type, false otherwise.)";
		d.examples = MakeAmalgamExamples({
			{R"&((~ 1 4 5))&", R"(.true)"},
			{R"&((~ 1 4 "a"))&", R"(.false)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TYPE_NEQUALS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(bool)";
		d.description = R"(Evaluates to true if no two values are of the same data types, false otherwise.)";
		d.examples = MakeAmalgamExamples({
			{R"&((!~
	"true"
	"false"
	[3 2]
))&", R"(.false)"},
			{R"&((!~
	"true"
	1
	[3 2]
))&", R"(.true)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_NULL)] = []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(null)";
		d.description = R"(Evaluates to the immediate null value, regardless of any parameters.)";
		d.examples = MakeAmalgamExamples({
			{R"&((null))&", R"((null))"},
			{R"&((lambda
	(null
		(+ 3 5)
		7
	)
))&", R"((null
	(+ 3 5)
	7
))"},
			{R"&((lambda
	
	#annotation
	(null)
))&", R"(#annotation
(null))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LIST)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(list)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to a list with the parameters as elements.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the list itself, the current index, and the current value.  If `[]`'s are used instead of parenthesis, the keyword `list` may be omitted.  `[]` are considered identical to `(list)`.)";
		d.examples = MakeAmalgamExamples({
			{R"&(["a" 1 "b"])&", R"(["a" 1 "b"])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_UNORDERED_LIST)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(unordered_list)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to the list specified by parameters as elements.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the unordered list itself, the current index, and the current value.  It operates like a list, except any operations that would normally consider a list's order.  For example, union, intersect, and mix, will consider the values unordered.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unordered_list 4 4 5))&", R"((unordered_list 4 4 5))"},
			{R"&((unordered_list
	(unordered_list 4 4 5)
	(unordered_list 4 5 6)
))&", R"((unordered_list
	(unordered_list 4 4 5)
	(unordered_list 4 5 6)
))"},
			{R"&((=
	(unordered_list 4 4 5)
	(unordered_list 4 4 5)
))&", R"(.true)"},
			{R"&((=
	(unordered_list 4 4 5)
	(unordered_list 4 4 6)
))&", R"(.false)"},
			{R"&((=
	(unordered_list 4 4 5)
	(unordered_list 4 4 5)
))&", R"(.true)"},
			{R"&((=
	(set_type
		(range 0 100)
		"unordered_list"
	)
	(set_type
		(reverse
			(range 0 100)
		)
		"unordered_list"
	)
))&", R"(.true)"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ASSOC)] = []() {
		OpcodeDetails d;
		d.parameters = R"([bstring index1] [* value1] [bstring index1] [* value2] ...)";
		d.returns = R"(assoc)";
		d.allowsConcurrency = true;
		d.description = R"(Evaluates to an associative list, where each pair of parameters (e.g., `index1` and `value1`) comprises a index-value pair.  Pushes a new target scope such that `(target)`, `(current_index)`, and `(current_value)` access the assoc, the current index, and the current value.  If any of the bareword strings (bstrings) do not have reserved characters or whitespace, then quotes are optional; if whitespace or reserved characters are present, then quotes are required.  If `{}`'s are used instead of parenthesis, the keyword assoc may be omitted.  `{}` are considered identical to `(assoc)`)";
		d.examples = MakeAmalgamExamples({
			{R"&((unparse
	{b 2 c 3}
))&", R"("{b 2 c 3}")"},
			{R"&((unparse
	{(null) 0 (+ 1 2) 3}
))&", R"("{(null) 0 (+ 1 2) 3}")"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
		d.newTargetScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_BOOL)] = []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(bool)";
		d.description = R"(A 64-bit floating point value)";
		d.examples = MakeAmalgamExamples({
			{R"&(.true)&", R"(.true)"},
			{R"&(.false)&", R"(.false)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_NUMBER)] = []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(number)";
		d.description = R"(A 64-bit floating point value)";
		d.examples = MakeAmalgamExamples({
			{R"&(1)&", R"(1)"},
			{R"&(1.5)&", R"(1.5)"},
			{R"&(6.02214076e+23)&", R"(6.02214076e+23)"},
			{R"&(.infinity)&", R"(.infinity)"},
			{R"&((-
	(* 3 .infinity)
))&", R"(-.infinity)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_STRING)] = []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(string)";
		d.description = R"(A string.  Many opcodes assume UTF-8 formatted strings, but many, such as `format`, can work with any bytes.  Any non double-quote character is considered valid.)";
		d.examples = MakeAmalgamExamples({
			{R"&("hello")&", R"("hello")"},
			{R"&("\tHello\n\"Hello\"")&", R"("\tHello\n\"Hello\"")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SYMBOL)] = []() {
		OpcodeDetails d;
		d.parameters = R"()";
		d.returns = R"(*)";
		d.description = R"(A string representing an internal symbol, a variable.)";
		d.examples = MakeAmalgamExamples({
			{R"&((let
	{foo 1}
	foo
))&", R"(1)"},
			{R"&(not_defined)&", R"((null))"},
			{R"&((lambda foo))&", R"(foo)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_TYPE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node)";
		d.returns = R"(any)";
		d.description = R"(Returns a node of the type corresponding to the node.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get_type
	(lambda
		(+ 3 4)
	)
))&", R"((+))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_TYPE_STRING)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node)";
		d.returns = R"(string)";
		d.description = R"(Returns a string that represents the type corresponding to the node.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get_type_string
	(lambda
		(+ 3 4)
	)
))&", R"("+")"},
			{R"&((get_type_string "hello"))&", R"("string")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_TYPE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node [string|* type])";
		d.returns = R"(any)";
		d.description = R"(Creates a copy of `node`, setting the type of the node of to `type`.  If `type` is a string, it will look that up as the type, or if `type` is a node that is not a string, it will set the type to match the top node of `type`.  It will convert opcode parameters as necessary.)";
		d.examples = MakeAmalgamExamples({
			{R"&((set_type
	(lambda
		(+ 3 4)
	)
	"-"
))&", R"((- 3 4))"},
			{R"&((sort
	(set_type
		(associate "a" 4 "b" 3)
		"list"
	)
))&", R"([3 4 "a" "b"])"},
			{R"&((sort
	(set_type
		(associate "a" 4 "b" 3)
		[]
	)
))&", R"([3 4 "a" "b"])"},
			{R"&((unparse
	(set_type
		["a" 4 "b" 3]
		"assoc"
	)
))&", R"("{a 4 b 3}")"},
			{R"&((call
	(set_type
		[1 0.5 "3.2" 4]
		"+"
	)
))&", R"(8.7)"},
			{R"&((set_type
	[
		(set_annotations
			(lambda
				(+ 3 4)
			)
			"react"
		)
	]
	"unordered_list"
))&", R"((unordered_list
	
	#react
	(+ 3 4)
))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_FORMAT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* data string from_format string to_format [assoc from_params] [assoc to_params])";
		d.returns = R"(any)";
		d.description = R"(Converts data from `from_format` into `to_format`.  Supported language types are "number", "string", and "code", where code represents everything beyond number and string.  Beyond the supported language types, additional formats that are stored in a binary string.  The additional formats are "base16", "base64", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float32", "float64", ">int8", ">uint8", ">int16", ">uint16", ">int32", ">uint32", ">int64", ">uint64", ">float32", ">float64", "<int8", "<uint8", "<int16", "<uint16", "<int32", "<uint32", "<int64", "<uint64", "<float32", "<float64", "json", "yaml", "date", and "time" (though date and time are special cases).  Binary types starting with a "<" represent little endian, binary types starting with a ">" represent big endian, and binary types without either will be the endianness of the machine.  Binary types will be handled as strings.  The "date" type requires additional information.  Following "date" or "time" is a colon, followed by a standard strftime date or time format string.  If `from_params` or `to_params` are specified, then it will apply the appropriate from or to as appropriate.  If the format is either "string", "json", or "yaml", then the key "sort_keys" can be used to specify a boolean value, if true, then it will sort the keys, otherwise the default behavior is to emit the keys based on memory layout.  If the format is date or time, then the to or from params can be an assoc with "locale" as an optional key.  If date then "time_zone" is also allowed.  The locale is provided, then it will leverage operating system support to apply appropriate formatting, such as en_US.  Note that UTF-8 is assumed and automatically added to the locale.  If no locale is specified, then the default will be used.  If converting to or from dates, if "time_zone" is specified, it will use the standard time_zone name, if unspecified or empty string, it will assume the current time zone.)";
		d.examples = MakeAmalgamExamples({
			{R"&((map
	(lambda
		(format (current_value) "int8" "number")
	)
	(explode "abcdefg" 1)
))&", R"([
	97
	98
	99
	100
	101
	102
	103
])"},
			{R"&((format 65 "number" "int8"))&", R"("A")"},
			{R"&((format
	(format -100 "number" "float64")
	"float64"
	"number"
))&", R"(-100)"},
			{R"&((format
	(format -100 "number" "float32")
	"float32"
	"number"
))&", R"(-100)"},
			{R"&((format
	(format 100 "number" "uint32")
	"uint32"
	"number"
))&", R"(100)"},
			{R"&((format
	(format 123456789 "number" ">uint32")
	"<uint32"
	"number"
))&", R"(365779719)"},
			{R"&((format
	(format 123456789 "number" ">uint32")
	">uint32"
	"number"
))&", R"(123456789)"},
			{R"&((format
	(format 14294967296 "number" "uint64")
	"uint64"
	"number"
))&", R"(14294967296)"},
			{R"&((format "A" "int8" "number"))&", R"(65)"},
			{R"&((format -100 "float32" "number"))&", R"(6.409830999309918e-10)"},
			{R"&((format 65 "uint8" "string"))&", R"("54")"},
			{R"&((format 254 "uint8" "base16"))&", R"("32")"},
			{R"&((format "AAA" "string" "base16"))&", R"("414141")"},
			{R"&((format "414141" "base16" "string"))&", R"("AAA")"},
			{R"&((format "Many hands make light work." "string" "base64"))&", R"("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu")"},
			{R"&((format "Many hands make light work.." "string" "base64"))&", R"("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLg==")"},
			{R"&((format "Many hands make light work..." "string" "base64"))&", R"("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLi4=")"},
			{R"&((format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu" "base64" "string"))&", R"("Many hands make light work.")"},
			{R"&((format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLg==" "base64" "string"))&", R"("Many hands make light work..")"},
			{R"&((format "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsuLi4=" "base64" "string"))&", R"("Many hands make light work...")"},
			{R"&((format "[{\"a\" : 3, \"b\" : 4}, {\"c\" : \"c\"}]" "json" "code"))&", R"([
	{a 3 b 4}
	{c "c"}
])"},
			{R"&((format
	[
		{a 3 b 4}
		{c "c" d (null)}
	]
	"code"
	"json"
	(null)
	{sort_keys .true}
))&", R"("[{\"a\":3,\"b\":4},{\"c\":\"c\",\"d\":null}]")"},
			{R"&((format
	{
		a 1
		b 2
		c 3
		d 4
		e ["a" "b" (null) .infinity]
	}
	"code"
	"yaml"
	(null)
	{sort_keys .true}
))&", R"("a: 1\nb: 2\nc: 3\nd: 4\ne:\n  - a\n  - b\n  - \n  - .inf\n")"},
			{R"&((format "a: 1" "yaml" "code"))&", R"({a 1})"},
			{R"&((format 1591503779.1 "number" "date:%Y-%m-%d-%H.%M.%S"))&", R"("2020-06-07-00.22.59.1000000")"},
			{R"&((format 1591503779 "number" "date:%F %T"))&", R"("2020-06-07 00:22:59")"},
			{R"&((format "Feb 2014" "date:%b %Y" "number"))&", R"(1391230800)"},
			{R"&((format "2014-Feb" "date:%Y-%h" "number"))&", R"(1391230800)"},
			{R"&((format "02/2014" "date:%m/%Y" "number"))&", R"(1391230800)"},
			{R"&((format 1591505665002 "number" "date:%F %T"))&", R"("-6053-05-28 00:24:29")"},
			{R"&((format 1591330905 "number" "date:%F %T"))&", R"("2020-06-05 00:21:45")"},
			{R"&((format 1591330905 "number" "date:%c %Z"))&", R"("06/05/20 00:21:45 EDT")"},
			{R"&((format 1591330905 "number" "date:%S"))&", R"("45")"},
			{R"&((format 1591330905 "number" "date:%Oe"))&", R"(" 5")"},
			{R"&((format 1591330905 "number" "date:%s"))&", R"(" s")"},
			{R"&((format 1591330905 "number" "date:%s%"))&", R"(" s")"},
			{R"&((format 1591330905 "number" "date:%a%b%c%d%e%f"))&", R"("FriJun06/05/20 00:21:4505 5 f")"},
			{R"&((format
	"abcd"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "es_ES"}
))&", R"("jueves, ene. 01, 1970")"},
			{R"&((format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "etete123"}
))&", R"("Sunday, Jun 07, 2020")"},
			{R"&((format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "notalocale"}
	{locale "es_ES"}
))&", R"("domingo, jun. 07, 2020")"},
			{R"&((format "2020-06-07" "date:%Y-%m-%d" "number"))&", R"(1591502400)"},
			{R"&((format "2020-06-07" "date:%Y-%m-%d" "date:%b %d, %Y"))&", R"("Jun 07, 2020")"},
			{R"&((format
	"2020-06-07"
	"date:%Y-%m-%d"
	"date:%A, %b %d, %Y"
	{locale "en_US"}
	{locale "es_ES"}
))&", R"("domingo, jun. 07, 2020")"},
			{R"&((format "1970-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number"))&", R"(664428)"},
			{R"&((format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number"))&", R"(-314954772)"},
			{R"&((format
	(format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
	"number"
	"date:%Y-%m-%d %H.%M.%S"
))&", R"("1960-01-08 11.33.48")"},
			{R"&((format
	(+
		0.01
		(format "1960-01-08 11.33.48" "date:%Y-%m-%d %H.%M.%S" "number")
	)
	"number"
	"date:%Y-%m-%d %H.%M.%S"
))&", R"("1960-01-08 11.33.48.0100000")"},
			{R"&((format "13:22:44" "time:%H:%M:%S" "number"))&", R"(48164)"},
			{R"&((format
	"13:22:44"
	"time:%H:%M:%S"
	"number"
	{locale "en_US"}
))&", R"(48164)"},
			{R"&((format "10:22:44" "time:%H:%M:%S" "number"))&", R"(37364)"},
			{R"&((format "10:22:44am" "time:%I:%M:%S%p" "number"))&", R"(37364)"},
			{R"&((format "10:22:44.33am" "time:%I:%M:%S%p" "number"))&", R"(37364.33)"},
			{R"&((format "10:22:44" "time:%I:%M:%S" "number"))&", R"(0)"},
			{R"&((format "10:22:44" "time:%qqq:%qqq:%qqq" "number"))&", R"(0)"},
			{R"&((format
	"13:22:44"
	"time:%H:%M:%S"
	"number"
	{locale "notalocale"}
))&", R"(48164)"},
			{R"&((format 48164 "number" "time:%H:%M:%S"))&", R"("13:22:44")"},
			{R"&((format
	48164
	"number"
	"time:%I:%M:%S%p"
	(null)
	{locale "es_ES"}
))&", R"("01:22:44PM")"},
			{R"&((format 37364.33 "number" "time:%I:%M:%S%p"))&", R"("10:22:44.3300000AM")"},
			{R"&((format 0 "number" "time:%I:%M:%S%p"))&", R"("12:00:00AM")"},
			{R"&((format (null) "number" "time:%I:%M:%S%p"))&", R"("12:00:00AM")"},
			{R"&((format .infinity "number" "time:%I:%M:%S%p"))&", R"("12:00:00AM")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_ANNOTATIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node)";
		d.returns = R"(string)";
		d.description = R"(Returns a string comprising all of the annotation lines for `node`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get_annotations
	(lambda
		
		#annotation line 1
		#annotation line 2
		.true
	)
))&", R"("annotation line 1\r\nannotation line 2")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_ANNOTATIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node [string new_annotation])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to a new copy of `node` with the annotation specified by `new_annotation`, where each newline is a separate line of annotation.  If `new_annotation` is null or missing, it will clear annotations for `node`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unparse
	(set_annotations
		(lambda
			
			#labelC
			.true
		)
		["labelD" "labelE"]
	)
	.true
	.true
	.true
))&", R"("#[\"labelD\" \"labelE\"]\r\n.true\r\n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_COMMENTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node)";
		d.returns = R"(string)";
		d.description = R"(Returns a strings comprising all of the comment lines for `node`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get_comments
	(lambda
		
		;comment line 1
		;comment line 2
		.true
	)
))&", R"("comment line 1\r\ncomment line 2")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_COMMENTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node [string new_comment])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to a new copy of `node` with the comment specified by `new_comment`, where each newline is a separate line of comment.  If `new_comment` is null or missing, it will clear comments for `node`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unparse
	(set_annotations
		(lambda
			
			#labelC
			.true
		)
		["labelD" "labelE"]
	)
	.true
	.true
	.true
))&", R"("#[\"labelD\" \"labelE\"]\r\n.true\r\n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_CONCURRENCY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node)";
		d.returns = R"(bool)";
		d.description = R"(Returns true if `node` has a preference to be processed in a manner where its operations are run concurrentl, false if it is not.  Note that concurrency is potentially subject to race conditions or inconsistent results if tasks write to the same locations without synchronization.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get_concurrency
	(lambda
		(print "hello")
	)
))&", R"(.false)"},
			{R"&((get_concurrency
	(lambda
		||(print "hello")
	)
))&", R"(.true)"},
			{R"&((get_concurrency
	(set_concurrency
		(lambda
			(print "hello")
		)
		.true
	)
))&", R"(.true)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_CONCURRENCY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node bool concurrent)";
		d.returns = R"(any)";
		d.description = R"(Evaluates to a new copy of `node` with the preference for concurrency set by `concurrent`.  Note that concurrency is potentially subject to race conditions or inconsistent results if tasks write to the same locations without synchronization.)";
		d.examples = MakeAmalgamExamples({
			{R"&((unparse
	(set_concurrency
		(lambda
			(print "hello")
		)
		.true
	)
	.true
	.true
	.true
))&", R"("||(print \"hello\")\r\n")"},
			{R"&((unparse
	(set_concurrency
		(lambda
			
			;complex test
			
			#some annotation
			{a "hello" b 4}
		)
		.true
	)
	.true
	.true
	.true
))&", R"(";complex test\r\n#some annotation\r\n||{a \"hello\" b 4}\r\n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_VALUE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node)";
		d.returns = R"(any)";
		d.description = R"(Evaluates to a new copy of `node` without annotations, comments, or concurrency.)";
		d.examples = MakeAmalgamExamples({
			{R"&((get_value
	
	;first comment
	(lambda
		
		;second comment
		
		#annotation part 1
		#annotation part 2
		.true
	)
))&", R"(.true)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_VALUE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* target * val)";
		d.returns = R"(any)";
		d.description = R"(Evaluates to a new copy of `node` with the value set to `val`, keeping existing annotations, comments, and concurrency).)";
		d.examples = MakeAmalgamExamples({
			{R"&((set_value
	
	;first comment
	(lambda
		
		;second comment
		.true
	)
	3
))&", R"(;second comment
3)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_EXPLODE)] = []() {
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
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SPLIT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string str [string split_string] [number max_split_count] [number stride])";
		d.returns = R"(list of string)";
		d.description = R"(Splits `str` into a list of strings based on `split_string`, which is handled as a regular expression.  Any data matching `split_string` will not be included in any of the resulting strings.  If `max_split_count` is provided and greater than zero, it will only split up to that many times.  If `stride` is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If `stride` is specified and a value other than zero, then it does not use `split_string` as a regular expression but rather a string, and it breaks the result into chunks of that many bytes.  For example, a `stride` of 1 would break it into bytes, whereas a `stride` of 4 would break it into 32-bit chunks.)";
		d.examples = MakeAmalgamExamples({
			{R"&((split "hello world"))&", R"(["hello world"])"},
			{R"&((split "hello world" " "))&", R"(["hello" "world"])"},
			{R"&((split "hello\r\nworld\r\n!" "\r\n"))&", R"(["hello" "world" "!"])"},
			{R"&((split "hello world !" "\\s" 1))&", R"(["hello" "world !"])"},
			{R"&((split "hello to the world" "to" (null) 2))&", R"(["hello " " the world"])"},
			{R"&((split "abcdefgij"))&", R"(["abcdefgij"])"},
			{R"&((split "abc de fghij" " "))&", R"(["abc" "de" "fghij"])"},
			{R"&((split "abc\r\nde\r\nfghij" "\r\n"))&", R"(["abc" "de" "fghij"])"},
			{R"&((split "abc de fghij" " " 1))&", R"(["abc" "de fghij"])"},
			{R"&((split "abc de fghij" " de " (null) 4))&", R"(["abc de fghij"])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SUBSTR)] = []() {
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
			{R"&((substr "hello world" -4 -1 (null) 1))&", R"("orl")"},
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
			{R"&((substr "hello world" "(e|o)" (null) "[$&]"))&", R"("h[e]ll[o] w[o]rld")"},
			{R"&((substr "hello world" "(e|o)" 2 "[$&]"))&", R"("h[e]ll[o] world")"},
			{R"&((substr "abcdefgijk"))&", R"("abcdefgijk")"},
			{R"&((substr "abcdefgijk" 1))&", R"("bcdefgijk")"},
			{R"&((substr "abcdefgijk" 1 8))&", R"("bcdefgi")"},
			{R"&((substr "abcdefgijk" 1 100))&", R"("bcdefgijk")"},
			{R"&((substr "abcdefgijk" 1 -1))&", R"("bcdefgij")"},
			{R"&((substr "abcdefgijk" -4 -1))&", R"("gij")"},
			{R"&((substr "abcdefgijk" -4 -1 (null) 1))&", R"("gij")"},
			{R"&((substr "abcdefgijk" 1 3 "x"))&", R"("axdefgijk")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CONCAT)] = []() {
		OpcodeDetails d;
		d.parameters = R"([string str1] [string str2] ... [string strN])";
		d.returns = R"(string)";
		d.description = R"(Concatenates all strings and evaluates to the single resulting string.)";
		d.examples = MakeAmalgamExamples({
			{R"&((concat "hello" " " "world"))&", R"("hello world")"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CRYPTO_SIGN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string message string secret_key)";
		d.returns = R"(string)";
		d.description = R"(Signs `message` given `secret_key` and returns the signature using the Ed25519 algorithm.  Note that `message` is not included in the `signature`.  The `system` opcode using the command "sign_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_sign_key" "secret_sign_key"]
			(system "sign_key_pair")
		)
	)
	(declare
		{message "hello"}
	)
	(declare
		{
			signature (crypto_sign message secret_sign_key)
		}
	)
	(concat
		"valid signature: "
		(crypto_sign_verify message public_sign_key signature)
		"\n"
	)
))&", R"("valid signature: .true\n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CRYPTO_SIGN_VERIFY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string message string public_key string signature)";
		d.returns = R"(bool)";
		d.description = R"(Verifies that `message` was signed with the signature via the public key using the Ed25519 algorithm and returns true if the signature is valid, false otherwise.  Note that `message` is not included in the `signature`.  The `system` opcode using the command "sign_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_sign_key" "secret_sign_key"]
			(system "sign_key_pair")
		)
	)
	(declare
		{message "hello"}
	)
	(declare
		{
			signature (crypto_sign message secret_sign_key)
		}
	)
	(concat
		"valid signature: "
		(crypto_sign_verify message public_sign_key signature)
		"\n"
	)
))&", R"("valid signature: .true\n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ENCRYPT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string plaintext_message string key1 [string nonce] [string key2])";
		d.returns = R"(string)";
		d.description = R"(If `key2` is not provided, then it uses the XSalsa20 algorithm to perform shared secret key encryption on the `message`, returning the encrypted value.  If `key2` is provided, then the Curve25519 algorithm will additionally be used, and `key1` will represent the receiver's public key and `key2` will represent the sender's secret key.  The `nonce` is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The `system` opcode using the command "encrypt_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_encrypt_key" "secret_encrypt_key"]
			(system "encrypt_key_pair")
		)
	)
	(declare
		{
			encrypted (encrypt message secret_encrypt_key "1234")
		}
	)
	(concat
		"decrypted: "
		(decrypt encrypted secret_encrypt_key "1234")
		"\n"
	)
))&", R"("decrypted: \n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_DECRYPT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string cyphertext_message string key1 [string nonce] [string key2])";
		d.returns = R"(string)";
		d.description = R"(If `key2` is not provided, then it uses the XSalsa20 algorithm to perform shared secret key decryption on the `message`, returning the encrypted value.  If `key2` is provided, then the Curve25519 algorithm will additionally be used, and `key1` will represent the sender's public key and `key2` will represent the receiver's secret key.  The `nonce` is a string of bytes up to 24 bytes long, that will be used to randomize the encryption, and will need to be provided to the decryption in order to work.  Nonces are not technically required, but strongly recommended to prevent replay attacks.  The `system` opcode using the command "encrypt_key_pair" can be used to create a public/secret key pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(declare
		(zip
			["public_encrypt_key" "secret_encrypt_key"]
			(system "encrypt_key_pair")
		)
	)
	(declare
		{
			encrypted (encrypt message secret_encrypt_key "1234")
		}
	)
	(concat
		"decrypted: "
		(decrypt encrypted secret_encrypt_key "1234")
		"\n"
	)
))&", R"("decrypted: \n")"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_PRINT)] = []() {
		OpcodeDetails d;
		d.parameters = R"([* node1] [* node2] ... [* nodeN])";
		d.returns = R"(null)";
		d.description = R"(Prints each of the parameters in order in a manner interpretable as if they were code, except strings are printed without quotes.  Output is pretty-printed.)";
		d.examples = MakeAmalgamExamples({
			{R"&((print "hello world\n"))&", R"((null))"},
			{R"&((print
	1
	2
	[3 4]
	"5"
	"\n"
))&", R"((null))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.permissions = ExecutionPermissions::Permission::STD_OUT_AND_STD_ERR;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NULL_VALUE;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TOTAL_SIZE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the total count of all of the nodes referenced directly or indirectly by `node`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((total_size
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		[5 6]
	]
))&", R"(10)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MUTATE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node [number mutation_rate] [assoc mutation_weights] [assoc operation_type] [preserve_type_depth])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to a mutated version of `node`.  The `mutation_rate` can range from 0.0 to 1.0 and defaulting to 0.00001, and indicates the probability that any node will experience a mutation.  The parameter `mutation_weights` is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The parameter `operation_type` is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings "change_type", "delete", "insert", "swap_elements", "deep_copy_elements", and "delete_elements".  If `preserve_type_depth` is specified, it will retain the types of node down to and including whatever depth is specified, and defaults to 0 indicating that none of the structure needs to be preserved.)";
		d.examples = MakeAmalgamExamples({
			{R"&((mutate
	(lambda
		[
			1
			2
			3
			4
			5
			6
			7
			8
			9
			10
			11
			12
			13
			14
			(associate "a" 1 "b" 2)
		]
	)
	0.4
))&", R"([
	1
	(and)
	3
	{}
	5
	6
	(tail)
	(get)
	(acos)
	(floor)
	(let)
	12
	zbiqZH
	14
	(associate (null))
])",
			//accept anything since mutation can do anything
			".*"},
			{R"&((mutate
	(lambda
		[
			1
			2
			3
			4
			(associate "alpha" 5 "beta" 6)
			(associate
				"nest"
				(associate
					"count"
					[7 8 9]
				)
				"end"
				[10 11 12]
			)
		]
	)
	0.2
	(associate "+" 0.5 "-" 0.3 "*" 0.2)
	(associate "change_type" 0.08 "delete" 0.02 "insert" 0.9)
))&", R"([
	1
	(-)
	3
	(-)
	(associate "alpha" 5 (+) 6)
	(associate
		"nest"
		(associate
			"count"
			[(*) 8 9]
		)
		"end"
		[(*) 11 12]
	)
])",
			//accept anything since mutation can do anything
			".*"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_COMMONALITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node1 * node2 [assoc params])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the total count of all of the nodes referenced within `node1` and `node2` that are equivalent.  The assoc `params` can contain the keys "string_edit_distance", "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "use_string_edit_distance" is true (default is false), it will assume `node1` and `node2` as string literals and compute via string edit distance.  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
		d.examples = MakeAmalgamExamples({
			{R"&((commonality
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"(3)"},
			{R"&((commonality
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"(15)"},
			{R"&((commonality .infinity 3))&", R"(0.125)"},
			{R"&((commonality
	(null)
	3
	{types_must_match .false}
))&", R"(0.125)"},
			{R"&((commonality .infinity .infinity))&", R"(1)"},
			{R"&((commonality .infinity -.infinity))&", R"(0.125)"},
			{R"&((commonality "hello" "hello"))&", R"(1)"},
			{R"&((commonality
	"hello"
	"hello"
	{string_edit_distance .true}
))&", R"(5)"},
			{R"&((commonality
	"hello"
	"el"
	{nominal_strings .false}
))&", R"(0.49099467997549845)"},
			{R"&((commonality
	"hello"
	"el"
	{string_edit_distance .true}
))&", R"(2)"},
			{R"&((commonality
	"el"
	"hello"
	{string_edit_distance .true}
))&", R"(2)"},
			{R"&((commonality
	(lambda
		{a 1 b 2 c 3}
	)
	(lambda
		(if
			x
			{a 1 b 2 c 3}
			.false
		)
	)
))&", R"(4)"},
			{R"&((commonality
	[1 2 3]
	[
		[1 2 3]
	]
))&", R"(4)"},
			{R"&((commonality
	[1 2 3]
	(lambda
		(null 1 2 3)
	)
	{types_must_match .false}
))&", R"(3.125)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_EDIT_DISTANCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node1 * node2 [assoc params])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the number of nodes that are different between `node1` and `node2`. The assoc `params` can contain the keys "string_edit_distance", "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "use_string_edit_distance" is true (default is false), it will assume `node1` and `node2` as string literals and compute via string edit distance.  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
		d.examples = MakeAmalgamExamples({
			{R"&((edit_distance
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"(3)"},
			{R"&((edit_distance
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"(2)"},
			{R"&((edit_distance "hello" "hello"))&", R"(0)"},
			{R"&((edit_distance
	"hello"
	"hello"
	{string_edit_distance .true}
))&", R"(0)"},
			{R"&((edit_distance
	"hello"
	"el"
	{nominal_strings .false}
))&", R"(1.018010640049003)"},
			{R"&((edit_distance
	"hello"
	"el"
	{string_edit_distance .true}
))&", R"(3)"},
			{R"&((edit_distance
	"el"
	"hello"
	{string_edit_distance .true}
))&", R"(3)"},
			{R"&((edit_distance
	[1 2 3]
	(lambda
		(unordered_list
			[1 2 3]
		)
	)
))&", R"(1)"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_INTERSECT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node1 * node2 [assoc params])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to whatever is common between `node1` and `node2` exclusive.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
		d.examples = MakeAmalgamExamples({
			{R"&((intersect
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "a" 3 "b" 4)
	]
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "c" 3 "b" 4)
	]
))&", R"([
	1
	(- 4 2)
	{b 4}
])"},
			{R"&((intersect
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"((seq 2 1))"},
			{R"&((intersect
	(lambda
		(unordered_list (get_entity_comments) 1 2)
	)
	(lambda
		(unordered_list (get_entity_comments) 1 2 4)
	)
))&", R"((unordered_list (get_entity_comments) 1 2))"},
			{R"&((intersect
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"([
	1
	2
	3
	{b 4}
	(if
		true
		1
		(unordered_list (get_entity_comments) 1)
	)
	[5 6]
])"},
			{R"&((intersect
	(lambda
		[
			1
			(associate "a" 3 "b" 4)
		]
	)
	(lambda
		[
			1
			(associate "c" 3 "b" 4)
		]
	)
))&", R"([
	1
	(associate (null) 3 "b" 4)
])"},
			{R"&((intersect
	(lambda
		(replace 4 2 6 1 7)
	)
	(lambda
		(replace 4 1 7 2 6)
	)
))&", R"((replace 4 2 6 1 7))"},
			{R"&((unparse
	(intersect
		(lambda
			[
				
				;comment 1
				;comment 2
				;comment 3
				1
				3
				5
				7
				9
				11
				13
			]
		)
		(lambda
			[
				
				;comment 2
				;comment 3
				;comment 4
				1
				4
				6
				8
				10
				12
				14
			]
		)
	)
	.true
	.true
	.true
))&", R"("[\r\n\t\r\n\t;comment 2\r\n\t;comment 3\r\n\t1\r\n]\r\n")"},
			{R"&((intersect
	[1 2 3]
	[
		[1 2 3]
	]
))&", R"([1 2 3])"},
			{R"&((intersect
	[1 2 3]
	[
		[1 2 3]
	]
	{recursive_matching .false}
))&", R"([])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_UNION)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node1 * node2 [assoc params])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to whatever is inclusive when merging `node1` and `node2`.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
		d.examples = MakeAmalgamExamples({
			{R"&((union
	(lambda
		(seq 2 (get_entity_comments) 1)
	)
	(lambda
		(seq 2 1 4 (get_entity_comments))
	)
))&", R"((seq 2 (get_entity_comments) 1 4 (get_entity_comments)))"},
			{R"&((union
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "a" 3 "b" 4)
	]
	[
		1
		(lambda
			(- 4 2)
		)
		(associate "c" 3 "b" 4)
	]
))&", R"([
	1
	(- 4 2)
	{a 3 b 4 c 3}
])"},
			{R"&((union
	(lambda
		(unordered_list (get_entity_comments) 1 2)
	)
	(lambda
		(unordered_list (get_entity_comments) 1 2 4)
	)
))&", R"((unordered_list (get_entity_comments) 1 2 4))"},
			{R"&((union
	[
		1
		2
		3
		(associate "a" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
	[
		1
		2
		3
		(associate "c" 3 "b" 4)
		(lambda
			(if
				true
				1
				(unordered_list (get_entity_comments) 1)
			)
		)
		[5 6]
	]
))&", R"([
	1
	2
	3
	{a 3 b 4 c 3}
	(if
		true
		1
		(unordered_list (get_entity_comments) 1)
	)
	[5 6]
])"},
			{R"&((union
	(lambda
		[
			1
			(associate "a" 3 "b" 4)
		]
	)
	(lambda
		[
			1
			(associate "c" 3 "b" 4)
		]
	)
))&", R"([
	1
	(associate (null) 3 "b" 4)
])"},
			{R"&((union
	[3 2]
	[3 4]
))&", R"([3 4 2])"},
			{R"&((union
	[2 3]
	[3 2 4]
))&", R"([3 2 4 3])"},
			{R"&((unparse
	(union
		(lambda
			[
				
				;comment 1
				;comment 2
				;comment 3
				1
				2
				3
				5
				7
				9
				11
				13
			]
		)
		(lambda
			[
				
				;comment 2
				;comment 3
				;comment 4
				1
				
				;comment x
				2
				4
				6
				8
				10
				12
				14
			]
		)
	)
	.true
	.true
	.true
))&", R"("[\r\n\t\r\n\t;comment 1\r\n\t;comment 2\r\n\t;comment 3\r\n\t;comment 4\r\n\t1\r\n\t\r\n\t;comment x\r\n\t2\r\n\t4\r\n\t3\r\n\t6\r\n\t5\r\n\t8\r\n\t7\r\n\t10\r\n\t9\r\n\t12\r\n\t11\r\n\t14\r\n\t13\r\n]\r\n")"},
			{R"&((union
	[1 2 3]
	[
		[1 2 3]
	]
))&", R"([
	[1 2 3]
])"},
			{R"&((union
	[
		[1 2 3]
	]
	[1 2 3]
))&", R"([
	[1 2 3]
])"},
			{R"&((union
	[1 2 3]
	(lambda
		[
			[1 2 3]
		]
	)
))&", R"([
	[1 2 3]
])"},
			{R"&((union
	[1 2 3]
	(lambda
		[
			[1 2 3]
		]
	)
	{recursive_matching .false}
))&", R"([
	1
	2
	3
	[1 2 3]
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_DIFFERENCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node1 * node2)";
		d.returns = R"(any)";
		d.description = R"(Finds the difference between `node1` and `node2`, and generates code that, if evaluated passing `node1` as its parameter "_", would turn it into `node2`.  Useful for finding a small difference of what needs to be changed to apply it to new (and possibly slightly different) data or code.)";
		d.examples = MakeAmalgamExamples({
			{R"&((difference
	(lambda
		{
			a 1
			b 2
			c 4
			d 7
			e 10
			f 12
			g 13
		}
	)
	(lambda
		[
			a
			2
			c
			4
			d
			6
			q
			8
			e
			10
			f
			12
			g
			14
		]
	)
))&", R"((declare
	{_ (null)}
	(replace
		_
		[]
		(lambda
			[
				a
				2
				c
				4
				d
				6
				q
				8
				e
				10
				f
				12
				g
				14
			]
		)
	)
))"},
			{R"&((difference
	{
		a 1
		b 2
		c 4
		d 7
		e 10
		f 12
		g 13
	}
	{
		a 2
		c 4
		d 6
		e 10
		f 12
		g 14
		q 8
	}
))&", R"((declare
	{_ (null)}
	(replace
		_
		[]
		(lambda
			{
				a 2
				c (get
						(current_value 1)
						"c"
					)
				d 6
				e (get
						(current_value 1)
						"e"
					)
				f (get
						(current_value 1)
						"f"
					)
				g 14
				q 8
			}
		)
	)
))"},
			{R"&((difference
	(lambda
		[
			1
			2
			4
			7
			10
			12
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
))&", R"((declare
	{_ (null)}
	(replace
		_
		[]
		(lambda
			[
				(get
					(current_value 1)
					1
				)
				(get
					(current_value 1)
					2
				)
				6
				8
				(get
					(current_value 1)
					4
				)
				(get
					(current_value 1)
					5
				)
				14
			]
		)
	)
))"},
			{R"&((unparse
	(difference
		(lambda
			{
				a 1
				b 2
				c 4
				d 7
				e 10
				f 12
				g 13
			}
		)
		(lambda
			{
				a 2
				c 4
				d 6
				e 10
				f 12
				g 14
				q 8
			}
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{\r\n\t\t\t\ta 2\r\n\t\t\t\tc (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"c\"\r\n\t\t\t\t\t)\r\n\t\t\t\td 6\r\n\t\t\t\te (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"e\"\r\n\t\t\t\t\t)\r\n\t\t\t\tf (get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t\"f\"\r\n\t\t\t\t\t)\r\n\t\t\t\tg 14\r\n\t\t\t\tq 8\r\n\t\t\t}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(lambda
			(associate
				a
				1
				g
				[1 2]
			)
		)
		(lambda
			(associate
				a
				2
				g
				[1 4]
			)
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[3]\r\n\t\t(lambda\r\n\t\t\t[\r\n\t\t\t\t(get\r\n\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t0\r\n\t\t\t\t)\r\n\t\t\t\t4\r\n\t\t\t]\r\n\t\t)\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t(set_type\r\n\t\t\t\t[\r\n\t\t\t\t\ta\r\n\t\t\t\t\t2\r\n\t\t\t\t\tg\r\n\t\t\t\t\t(get\r\n\t\t\t\t\t\t(current_value 1)\r\n\t\t\t\t\t\t3\r\n\t\t\t\t\t)\r\n\t\t\t\t]\r\n\t\t\t\t\"associate\"\r\n\t\t\t)\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(zip
			[1 2 3 4 5]
		)
		(append
			(zip
				[2 6 5]
			)
			{a 1}
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{\r\n\t\t\t\t2 (null)\r\n\t\t\t\t5 (null)\r\n\t\t\t\t6 (null)\r\n\t\t\t\ta 1\r\n\t\t\t}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(zip
			[1 2 3 4 5]
		)
		(zip
			[2 6 5]
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{2 (null) 5 (null) 6 (null)}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((unparse
	(difference
		(zip
			[1 2 5]
		)
		(zip
			[2 6 5]
		)
	)
	.true
	.true
	.true
))&", R"("(declare\r\n\t{_ (null)}\r\n\t(replace\r\n\t\t_\r\n\t\t[]\r\n\t\t(lambda\r\n\t\t\t{2 (null) 5 (null) 6 (null)}\r\n\t\t)\r\n\t)\r\n)\r\n")"},
			{R"&((let
	{
		x (lambda
				[
					6
					[1 2]
				]
			)
		y (lambda
				[
					7
					[1 4]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
))&", R"([
	7
	[1 4]
])"},
			{R"&((let
	{
		x (lambda
				[
					(+ 0 1)
					[1 2]
				]
			)
		y (lambda
				[
					(+ 7 8)
					[1 4]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
))&", R"([
	(+ 7 8)
	[1 4]
])"},
			{R"&((let
	{
		x (lambda
				[
					6
					[
						["a" "b"]
						1
						2
					]
				]
			)
		y (lambda
				[
					7
					[
						["a" "x"]
						1
						4
					]
				]
			)
	}
	(call
		(difference x y)
		{_ x}
	)
))&", R"([
	7
	[
		["a" "x"]
		1
		4
	]
])"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MIX)] = []() {
		OpcodeDetails d;
		d.parameters = R"(* node1 * node2 [number keep_chance_node1] [number keep_chance_node2] [assoc params])";
		d.returns = R"(any)";
		d.description = R"(Performs a union operation on `node1` and `node2`, but randomly ignores nodes from one or the other if the nodes are not equal.  If only `keep_chance_node1` is specified, `keep_chance_node2` defaults to 1 - `keep_chance_node1`. `keep_chance_node1` specifies the probability that a node from `node1` will be kept, and `keep_chance_node2` the probability that a node from `node2` will be kept.  `keep_chance_node1` + `keep_chance_node2` should be between 1 and 2, as there are two objects being merged, otherwise the values will be normalized.  `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", "recursive_matching", and "similar_mix_chance".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of `node1` to `node2`.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  "similar_mix_chance" is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number and string values based on `keep_chance_node1` and `keep_chance_node2`, and defaults to 0.0.  If "similar_mix_chance" is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.)";
		d.examples = MakeAmalgamExamples({
			{R"&((mix
	(lambda
		[
			1
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	0
))&", R"([1 3 4 9 11 14])",
			//accept anything since mutation can do anything
			".*"},
			{R"&((mix
	(lambda
		[
	
			;comment 1
			;comment 2
			;comment 3
			1
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			
			;comment 2
			;comment 3
			;comment 4
			1
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance 0}
))&", R"([
	
	;comment 1
	;comment 2
	;comment 3
	;comment 4
	1
	4
	3
	5
	9
	11
	14
])",
			//accept anything since mutation can do anything
			".*"},
			{R"&((mix
	(lambda
		[
			1
			2
			(associate "a" 3 "b" 4)
			(lambda
				(if
					true
					1
					(unordered_list (get_entity_comments) 1)
				)
			)
			[5 6]
		]
	)
	(lambda
		[
			1
			5
			3
			(associate "a" 3 "b" 4)
			(lambda
				(if
					false
					1
					(unordered_list
						(get_entity_comments)
						(lambda
							[2 9]
						)
					)
				)
			)
		]
	)
	0.8
	0.8
	{similar_mix_chance 0.5}
))&", R"([
	1
	5
	3
	(associate "a" 3 "b" 4)
	(lambda
		(if
			true
			1
			(unordered_list
				(get_entity_comments)
				(lambda
					[2 9]
				)
			)
		)
	)
	[5]
])",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	(lambda
		[
			1
			2
			(associate "a" 3 "b" 4)
			(lambda
				(if
					true
					1
					(unordered_list (get_entity_comments) 1)
				)
			)
			[5 6]
		]
	)
	(lambda
		[
			1
			5
			3
			{a 3 b 4}
			(lambda
				(if
					false
					1
					(seq
						(get_entity_comments)
						(lambda
							[2 9]
						)
					)
				)
			)
		]
	)
	0.8
	0.8
	{similar_mix_chance 1}
))&", R"([
	1
	2.5
	{a 3 b 4}
	(associate "a" 3 "b" 4)
	(lambda
		(if
			true
			1
			(seq
				(get_entity_comments)
				(lambda
					[2 9]
				)
			)
		)
	)
	[5]
])",
			//accept anything since mutation can do anything
			".*"},
			{R"&((mix
	(lambda
		[
			.true
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance 1}
))&", R"([
	.true
	3
	5
	7.5
	9.5
	11.5
	13.5
])",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	(lambda
		[
			.true
			3
			5
			7
			9
			11
			13
		]
	)
	(lambda
		[
			2
			4
			6
			8
			10
			12
			14
		]
	)
	0.5
	0.5
	{similar_mix_chance -1}
))&", R"([3 5 2 4 12 11])",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance -1}
))&", R"(4)",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance -0.8}
))&", R"(4)",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance 0.5}
))&", R"(1)",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	1
	4
	0.5
	0.5
	{similar_mix_chance 1}
))&", R"(2.5)",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
))&", R"("abcdexyz")",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
))&", R"("abcdexyz")",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	"abcdexyz"
	"abcomxyz"
	0.5
	0.5
	{nominal_strings .false similar_mix_chance 0.5}
))&", R"("abcdexyz")",
			//accept anything since mutation can do anything
			".*" },
			{R"&((mix
	{
		a [0 1]
		b [1 2]
		c [2 3]
	}
	{
		a [0 1]
		b [1 2]
		w [2 3]
		x [3 4]
		y [4 5]
		z [5 6]
	}
	0.5
	0.5
	{recursive_matching .false}
))&", R"({
	a [0 1]
	b [1 2]
	w [2]
	z [5]
})",
			//accept anything since mutation can do anything
			".*" }
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_TOTAL_ENTITY_SIZE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity)";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the total count of all of the nodes of `entity` and all of its contained entities.  Each entity itself counts as multiple nodes, corresponding to flattening an entity via the `flatten_entity` opcode.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity1"
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "EntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "EntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{E 3 F 4}
	)
	(create_entities
		["Entity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(total_entity_size "Entity1")
))&", R"(67)", "", R"((destroy_entities "Entity1"))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_FLATTEN_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity [bool include_rand_seeds] [bool parallel_create] [bool include_version])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to code that, if called, would completely reproduce the `entity`, as well as all contained entities.  If `include_rand_seeds` is true, by default, it will include all entities' random seeds.  If `parallel_create` is true, then the creates will be performed with parallel markers as appropriate for each group of contained entities.  If `include_version` is true, it will include a comment on the top node that is the current version of the Amalgam interpreter, which can be used for validating interoperability when loading code.  The code returned accepts two parameters, `create_new_entity`, which defaults to true, and `new_entity`, which defaults to null.  If `create_new_entity` is true, then it will create a new entity using the id path specified by `new_entity`, where null will create an unnamed entity.  If `create_new_entity` is false, then it will overwrite the current entity's code and create all contained entities.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"FlattenEntity"
		(lambda {})
	)
	(call_entity "FlattenEntity" "a")
	(create_entities
		["FlattenEntity" "DeepRand"]
		(lambda
			{a (rand)}
		)
	)
	(declare
		{
			flattened_code (flatten_entity "FlattenEntity" .true .true)
		}
	)
	(declare
		{first_rand (null) second_rand (null)}
	)
	(assign
		{
			first_rand (call_entity
					["FlattenEntity" "DeepRand"]
					"a"
				)
		}
	)
	(let
		{
			new_entity (call flattened_code)
		}
		(assign
			{
				second_rand (call_entity
						[new_entity "DeepRand"]
						"a"
					)
			}
		)
	)
	[first_rand second_rand]
))&", R"([0.611779739433564 0.611779739433564])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MUTATE_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path source_entity [number mutation_rate] [id_path dest_entity] [assoc mutation_weights] [assoc operation_type] [preserve_type_depth])";
		d.returns = R"(id_path)";
		d.description = R"(Creates a mutated version of the entity specified by `source_entity` like mutate. Returns the id path of a new entity created contained by the entity that ran it.  The value specified by `mutation_rate`, from 0.0 to 1.0 and defaulting to 0.00001, indicates the probability that any node will experience a mutation.  Uses `dest_entity` as the optional destination.  The parameter `mutation_weights` is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The `operation_type` is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings "change_type", "delete", "insert", "swap_elements", "deep_copy_elements", and "delete_elements".  If `preserve_type_depth` is specified, it will retain the types of node down to and including whatever depth is specified, and defaults to 1 indicating that the top level of the entities will have a preserved type, namely an assoc.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"MutateEntity"
		(lambda
			{
				a 1
				b 2
				c 3
				d 4
				e 5
				f 6
				g 7
				h 8
				i 9
				j 10
				k 11
				l 12
				m 13
				n 14
				o (associate "a" 1 "b" 2)
			}
		)
	)
	(mutate_entity "MutateEntity" 0.4 "MutatedEntity1")
	(mutate_entity "MutateEntity" 0.5 "MutatedEntity2")
	(mutate_entity
		"MutateEntity"
		0.5
		"MutatedEntity3"
		(associate "+" 0.5 "-" 0.3 "*" 0.2)
		(associate "change_type" 0.08 "delete" 0.02 "insert" 0.9)
	)
	[
		(retrieve_entity_root "MutatedEntity1")
		(retrieve_entity_root "MutatedEntity2")
		(retrieve_entity_root "MutatedEntity3")
	]
))&", R"([
	{
		a 1
		b 2
		c 3
		d (set_type)
		e (if)
		f (>=)
		g (<=)
		h 8
		i 9
		j 10
		k 11
		l 12
		m 13
		n -20.325081516830192
		o "b"
	}
	{
		a 1
		b (map)
		c (min)
		d 4
		e 5
		f (apply)
		g 7
		h (get_type_string)
		i (round)
		j 10
		k (lambda)
		l 12
		m (declare)
		n 14
		o (map)
	}
	{
		a (*)
		b (*)
		c 3
		d 4
		e (+)
		f (*)
		g (-)
		h 8
		i 9
		j 10
		k 11
		l 12
		m (+)
		n 14
		o (associate (-) 1 (*) (+))
	}
])", ".*", R"((destroy_entities "MutateEntity" "MutatedEntity1" "MutatedEntity2" "MutatedEntity3" ))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_COMMONALITY_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity1 id_path entity2 [assoc params])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the total count of all of the nodes referenced within `entity1` and `entity2` that are equivalent, including all contained entities.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"MergeEntity1"
		{a 3 b 4 c "c1"}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["MergeEntity1"]
		{E 3 F 4}
	)
	(create_entities
		["MergeEntity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(create_entities
		"MergeEntity2"
		{b 4 c "c2"}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild1"]
		{x 3 y 4 z 5}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild2"]
		{
			p 3
			q 4
			u 5
			v 6
			w 7
		}
	)
	(create_entities
		["MergeEntity2"]
		{
			E 3
			F 4
			G 5
			H 6
		}
	)
	(create_entities
		["MergeEntity2"]
		{e 3 f 4}
	)
	[
		(commonality_entities "MergeEntity1" "MergeEntity2")
		(commonality_entities
			"MergeEntity1"
			"MergeEntity2"
			{nominal_strings .false types_must_match .false}
		)
	]
))&", R"([64 64.74178574543642])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" )"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_EDIT_DISTANCE_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity1 id_path entity2 [assoc params])";
		d.returns = R"(number)";
		d.description = R"(Evaluates to the edit distance of all of the nodes referenced within `entity1` and `entity2` that are equivalent, including all contained entities.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		[
			(edit_distance_entities "MergeEntity1" "MergeEntity2")
			(edit_distance_entities
				"MergeEntity1"
				"MergeEntity2"
				{nominal_strings .false types_must_match .false}
			)
		]
	)
))&", R"([11 9.516428509127167])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" )"},

			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_INTERSECT_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity1 id_path entity2 [assoc params] [id_path entity3])";
		d.returns = R"(id_path)";
		d.description = R"(Creates an entity of whatever is common between the entities `entity1` and `entity2` exclusive.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  Uses `entity3` as the optional destination via an internal call create_contained_entity.  Any contained entities will be intersected either based on matching name or maximal similarity for nameless entities.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		(intersect_entities "MergeEntity1" "MergeEntity2" (null) "IntersectedEntity")
		[
			(retrieve_entity_root "IntersectedEntity")
			(sort
				(contained_entities "IntersectedEntity")
			)
		]
	)
))&", R"([
	{b 4 c (null)}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" "IntersectedEntity")"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_UNION_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity1 id_path entity2 [assoc params] [id_path entity3])";
		d.returns = R"(id_path)";
		d.description = R"(Creates an entity of whatever is inclusive when merging the entities `entity1` and `entity2`.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  Uses `entity3` as the optional destination via an internal call to create_contained_entity.  Any contained entities will be unioned either based on matching name or maximal similarity for nameless entities.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		(union_entities "MergeEntity1" "MergeEntity2" (null) "UnionedEntity")
		[
			(retrieve_entity_root "UnionedEntity")
			(sort
				(contained_entities "UnionedEntity")
			)
		]
	)
))&", R"([
	{a 3 b 4 c (null)}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" "UnionedEntity")"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_DIFFERENCE_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity1 id_path entity2)";
		d.returns = R"(any)";
		d.description = R"(Finds the difference between the entities specified by `entity1` and `entity2` and generates code that, if evaluated passing the entity id_path as its parameter "_", would create a new entity into the id path specified by its parameter "new_entity" (null if unspecified), which would contain the applied difference between the two entities and returns the newly created entity id path.  Useful for finding a small difference of what needs to be changed to apply it to new (and possibly slightly different) entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"DiffEntity1"
		(lambda
			{a 3 b 4}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1"]
		(lambda
			{x 3 y 4 z 6}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1" "DiffEntityChild2"]
		(lambda
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1" "DiffEntityChild2" "DiffEntityChild3"]
		(lambda
			{
				a 5
				e 3
				o 6
				p 4
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity1" "OnlyIn1"]
		(lambda
			{m 4}
		)
	)
	(create_entities
		["DiffEntity1"]
		(lambda
			{e 3 f 4}
		)
	)
	(create_entities
		["DiffEntity1"]
		(lambda
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
	)
	(create_entities
		"DiffEntity2"
		(lambda
			{b 4 c 3}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1"]
		(lambda
			{x 3 y 4 z 5}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1" "DiffEntityChild2"]
		(lambda
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1" "DiffEntityChild2" "DiffEntityChild3"]
		(lambda
			{
				a 5
				e 3
				o 6
				p 4
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity2" "OnlyIn2"]
		(lambda
			{o 6}
		)
	)
	(create_entities
		["DiffEntity2"]
		(lambda
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
	)
	(create_entities
		["DiffEntity2"]
		(lambda
			{e 3 f 4}
		)
	)
	
	;applying the difference to DiffEntity1 results in an entity identical to DiffEntity2
	(let
		{
			new_entity (call
					(difference_entities "DiffEntity1" "DiffEntity2")
					{_ "DiffEntity1"}
				)
		}
		(difference_entities "DiffEntity2" new_entity)
	)
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))", "", R"((apply "destroy_entities" (contained_entities)))"}
		});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MIX_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity1 id_path entity2 [number keep_chance_entity1] [number keep_chance_entity2] [assoc params] [id_path entity3])";
		d.returns = R"(id_path)";
		d.description = R"(Performs a union operation on the entities represented by `entity1` and `entity2`, but randomly ignores nodes from one or the other tree if not equal.  If only `keep_chance_entity1` is specified, `keep_chance_entity2` defaults to 1 - `keep_chance_entity1`.  `keep_chance_entity1` specifies the probability that a node from the entity represented by `entity1` will be kept, and `keep_chance_entity2` the probability that a node from the entity represented by `entity2` will be kept.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  `similar_mix_chance` is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number and string values based on `keep_chance_node1` and `keep_chance_node2`, and defaults to 0.0.  If `similar_mix_chance` is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.  `unnamed_entity_mix_chance` represents the probability that an unnamed entity pair will be mixed versus preserved as independent chunks, where 0.2 would yield 20% of the entities mixed. Returns the id path of a new entity created contained by the entity that ran it.  Uses `entity3` as the optional destination entity.   Any contained entities will be mixed either based on matching name or maximal similarity for nameless entities.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"MergeEntity1"
		{a 3 b 4 c "c1"}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["MergeEntity1"]
		{E 3 F 4}
	)
	(create_entities
		["MergeEntity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(create_entities
		"MergeEntity2"
		{b 4 c "c2"}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild1"]
		{x 3 y 4 z 5}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild2"]
		{
			p 3
			q 4
			u 5
			v 6
			w 7
		}
	)
	(create_entities
		["MergeEntity2"]
		{
			E 3
			F 4
			G 5
			H 6
		}
	)
	(create_entities
		["MergeEntity2"]
		{e 3 f 4}
	)
	(mix_entities
		"MergeEntity1"
		"MergeEntity2"
		0.5
		0.5
		{similar_mix_chance 0.5 unnamed_entity_mix_chance 0.2}
		"MixedEntities"
	)
	[
		(retrieve_entity_root "MixedEntities")
		(sort
			(contained_entities "MixedEntities")
		)
	]
))&", R"([
	{b 4 c "c1"}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
])", ".*", R"((apply "destroy_entities" (contained_entities)))"},
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_ENTITY_ANNOTATIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity] [string label] [bool deep_annotations])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the corresponding annotations for `entity`.  If `entity` is null then it will use the current entity.  If `label` is null or empty string, it will retrieve annotations for the entity root, otherwise if it is a valid `label` it will attempt to retrieve the annotations for that label, null if the label doesn't exist.  If `deep_annotations` is specified and the label is a declare, then it will return a list of two elements.  The first element of this list is an assoc with the keys being the parameters and the values being lists of the descriptions followed by the default value.  The second element of this list is the annotation of the assoc itself, which is intended to be used to describe what is returned.  If label is empty string or null and deep_annotations is true, then it will return an assoc of label to annotation for each label in the entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"descriptive_entity"
		(lambda
			
			;this is a fully described entity
			
			#entity annotations
			{
				!privatevar 
					;some private variable
					
					#privatevar annotation
					2
				^containervar 
					;a variable accessible to contained entities
					
					#containervar annotation
					3
				foo 
					;the function foo
					
					#foo annotation
					(declare
						
						;a number representing the sum
						
						#return annotation
						{
							x 
								;the value of x
								;the default value of x
								
								#x annotation
								#x value annotation
								1
							y 
								;the value of y
								
								#y value annotation
								2
						}
						(+ x y)
					)
				get_api 
					;returns the api details
					
					#get_api annotation
					(seq
						{
							description (get_entity_comments)
							labels (map
									(lambda
										{
											description (current_value 1)
											parameters (get_entity_comments
													(null)
													(current_index 1)
													.true
												)
										}
									)
									(get_entity_comments (null) (null) .true)
								)
						}
					)
				publicvar 
					;some public variable
					
					#publicvar annotation
					1
			}
		)
	)
	[
		(get_entity_annotations "descriptive_entity")
		(get_entity_annotations "descriptive_entity" (null) .true)
		(get_entity_annotations "descriptive_entity" "foo" .true)
	]
))&", R"([
	"entity annotations"
	{
		^containervar "containervar annotation"
		foo "foo annotation"
		get_api "get_api annotation"
		publicvar "publicvar annotation"
	}
	[
		{
			x ["x annotation\r\nx value annotation" 1]
			y ["y value annotation" 2]
		}
		"return annotation"
	]
])", "", R"((destroy_entities "descriptive_entity"))"},
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_ENTITY_COMMENTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity] [string label] [bool deep_comments])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the corresponding comments for `entity`.  If `entity` is null then it will use the current entity.  If `label` is null or empty string, it will retrieve comments for the entity root, otherwise if it is a valid `label` it will attempt to retrieve the comments for that label, null if the label doesn't exist.  If `deep_comments` is specified and the label is a declare, then it will return a list of two elements.  The first element of this list is an assoc with the keys being the parameters and the values being lists of the descriptions followed by the default value.  The second element of this list is the comment of the assoc itself, which is intended to be used to describe what is returned.  If label is empty string or null and deep_comments is true, then it will return an assoc of label to comment for each label in the entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"descriptive_entity"
		(lambda
			
			;this is a fully described entity
			
			#entity annotations
			{
				!privatevar 
					;some private variable
					
					#privatevar annotation
					2
				^containervar 
					;a variable accessible to contained entities
					
					#containervar annotation
					3
				foo 
					;the function foo
					
					#foo annotation
					(declare
						
						;a number representing the sum
						
						#return annotation
						{
							x 
								;the value of x
								;the default value of x
								
								#x annotation
								#x value annotation
								1
							y 
								;the value of y
								
								#y value annotation
								2
						}
						(+ x y)
					)
				get_api 
					;returns the api details
					
					#get_api annotation
					(seq
						{
							description (get_entity_comments)
							labels (map
									(lambda
										{
											description (current_value 1)
											parameters (get_entity_comments
													(null)
													(current_index 1)
													.true
												)
										}
									)
									(get_entity_comments (null) (null) .true)
								)
						}
					)
				publicvar 
					;some public variable
					
					#publicvar annotation
					1
			}
		)
	)
	[
		(get_entity_comments "descriptive_entity")
		(get_entity_comments "descriptive_entity" (null) .true)
		(get_entity_comments "descriptive_entity" "foo" .true)
	]
))&", R"([
	"this is a fully described entity"
	{
		^containervar "a variable accessible to contained entities"
		foo "the function foo"
		get_api "returns the api details"
		publicvar "some public variable"
	}
	[
		{
			x ["the value of x\r\nthe default value of x" 1]
			y ["the value of y" 2]
		}
		"a number representing the sum"
	]
])", "", R"((destroy_entities "descriptive_entity"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_RETRIEVE_ENTITY_ROOT)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity])";
		d.returns = R"(any)";
		d.description = R"(Evaluates to the code contained by `entity`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{1 2 three 3}
		)
	)
	(retrieve_entity_root "Entity")
))&", R"({1 2 three 3})", "", R"((destroy_entities "Entity"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ASSIGN_ENTITY_ROOTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity1] * root1 [id_path entity2] [* root2] [...])";
		d.returns = R"(bool)";
		d.description = R"(Sets the code of the `entity1 to `root1`, as well as all subsequent entity-code pairs of parameters.  If `entity1` is not specified or null, then uses the current entity.  On assigning the code to the new entity, any root that is not of a type assoc will be put into an assoc under the null key.  If all assignments were successful, then returns true, otherwise returns false.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{1 2 three 3}
		)
	)
	(assign_entity_roots
		"Entity"
		{a 4 b 5 c 6}
	)
	(retrieve_entity_root "Entity")
))&", R"({a 4 b 5 c 6})", "", R"((destroy_entities "Entity"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_ENTITY_RAND_SEED)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity])";
		d.returns = R"(string)";
		d.description = R"(Evaluates to a string representing the current state of the random number generator for `entity` used for seeding the random streams of any calls to the entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Rand"
		(lambda
			{a (rand)}
		)
	)
	(call_entity "Rand" "a")
	(format
		(get_entity_rand_seed "Rand")
		"string"
		"base64"
	)
))&", R"("nHKVcHddHVaqvcDt3AYbD/8=")", "", R"((destroy_entities "Rand"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_ENTITY_RAND_SEED)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity] * node [bool deep])";
		d.returns = R"(string)";
		d.description = R"(Sets the random number seed and state for the random number generator of `entity`, or the current entity if null or not specified, to the state specified by `node`.  If `node` is already a string in the proper format output by `(get_entity_rand_seed)`, then it will set the random generator to that current state, picking up where the previous state left off.  If `node` is anything else, it uses the value as a random seed to start the generator.  Note that this will not affect the state of the current random number stream, only future random streams created by `entity` for new calls.  The parameter `deep` defaults to false, but if it is true, all contained entities are recursively set with random seeds based on the specified random seed and a hash of their relative id path to the entity being set.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Rand"
		(lambda
			{a (rand)}
		)
	)
	(create_entities
		["Rand" "DeepRand"]
		(lambda
			{a (rand)}
		)
	)
	(declare
		{
			seed (get_entity_rand_seed "Rand")
		}
	)
	(declare
		{
			first_rand_numbers [
					(call_entity "Rand" "a")
					(call_entity
						["Rand" "DeepRand"]
						"a"
					)
				]
		}
	)
	(set_entity_rand_seed "Rand" seed .true)
	(declare
		{
			second_rand_numbers [
					(call_entity "Rand" "a")
					(call_entity
						["Rand" "DeepRand"]
						"a"
					)
				]
		}
	)
	[first_rand_numbers second_rand_numbers]
))&", R"([
	[0.9512993766655248 0.3733350484591008]
	[0.9512993766655248 0.3733350484591008]
])", "", R"((destroy_entities "Rand"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::EXISTING;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_GET_ENTITY_PERMISSIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity])";
		d.returns = R"(assoc)";
		d.description = R"(Returns an assoc of the permissions of `entity`, the current entity if `entity` is not specified or null, where each key is the permission and each value is either true or false.  Permission keys consist of: "std_out_and_std_err", which allows output; "std_in", which allows input; "load", which allows reading files; "store", which allows writing files; "environment", which allows reading information about the environment; "alter_performance", which allows adjusting performance characteristics; and "system", which allows running system commands.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		(lambda
			(print (system_time))
		)
	)
	(get_entity_permissions "Entity")
))&", R"({
	alter_performance .false
	environment .false
	load .false
	std_in .false
	std_out_and_std_err .false
	store .false
	system .false
})", "", R"((destroy_entities "Entity"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_SET_ENTITY_PERMISSIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity bool|assoc permissions [bool deep])";
		d.returns = R"(id_path)";
		d.description = R"(Sets the permissions on the `entity`.  If permissions is true, then it grants all permissions, if it is false, then it removes all.  If permissions is an assoc, it alters the permissions of the assoc keys to the boolean values of the assoc's values.  Permission keys consist of: "std_out_and_std_err", which allows output; "std_in", which allows input; "load", which allows reading files; "store", which allows writing files; "environment", which allows reading information about the environment; "alter_performance", which allows adjusting performance characteristics; and "system", which allows running system commands.  The parameter `deep` defaults to false, but if it is true, all contained entities have their permissions updated.  Returns the id path of `entity`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		(lambda
			(print (system_time))
		)
	)
	(set_entity_permissions "Entity" .true)
	(get_entity_permissions "Entity")
))&", R"({
	alter_performance .true
	environment .true
	load .true
	std_in .true
	std_out_and_std_err .true
	store .true
	system .true
})", "", R"((destroy_entities "Entity"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CREATE_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity1] * node1 [id_path entity2] [* node2] [...])";
		d.returns = R"(list of id_path)";
		d.description = R"(Creates a new entity for id path `entity1` with code specified by `node1`, repeating this for all entity-node pairs, returning a list of the id paths for each of the entities created.  If the execution does not have permission to create the entities, it will evaluate to null.  If the `entity` is omitted, then it will create an unnamed new entity in the calling entity.  If `entity1` specifies an existing entity, then it will create the new entity within that existing entity.  If the last id path in the string is not an existing entity, then it will attempt to create that entity (returning null if it cannot).  If the node is of any other type than assoc, it will create an assoc as the top node and place the node under the null key.  Unlike the rest of the entity creation commands, create_entities specifies the optional id path first to make it easy to read entity definitions.  If more than 2 parameters are specified, create_entities will iterate through all of the pairs of parameters, treating them like the first two as it creates new entities.)";
		d.examples = MakeAmalgamExamples({
			{R"&((create_entities
	"Entity"
	(lambda
		{
			a (+ 3 4)
		}
	)
))&", R"(["Entity"])", "", R"((destroy_entities "Entity"))"},
			{R"&((seq
	(create_entities
		"EntityWithContainedEntities"
		{a 3 b 4}
	)
	(create_entities
		["EntityWithContainedEntities" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["EntityWithContainedEntities" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["EntityWithContainedEntities"]
		{m 3 n 4}
	)
	(contained_entities "EntityWithContainedEntities")
))&", R"(["NamedEntity1" "NamedEntity2" "_hIcoPxJ8LiS"])", "", R"((destroy_entities "EntityWithContainedEntities"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CLONE_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path source_entity1 [id_path destination_entity1] [id_path source_entity2] [id_path destination_entity2] [...])";
		d.returns = R"(list of id_path)";
		d.description = R"(Creates a clone of `source_entity1`.  If `destination_entity1` is not specified, then it clones the entity into an unnamed entity in the current entity.  If `destination_entity1` is specified, then it clones it into the location specified by `destination_entity1`; if `destination_entity1` is an existing entity, then it will create it as a contained entity within `destination_entity1`, if not, it will attempt to create it with the given id path of `destination_entity1`.  Evaluates to the id path of the new entity.  Can only be performed by an entity that contains both `source_entity1` and the specified path of `destination_entity1`. If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity1"
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{m 3 n 4}
	)
	(clone_entities "Entity1" "Entity2")
	(contained_entities "Entity2")
))&", R"(["NamedEntity1" "NamedEntity2" "_539JylCpbqn"])", "", R"((destroy_entities "Entity1" "Entity2"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_MOVE_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path source_entity1 [id_path destination_entity1] [id_path source_entity2] [id_path destination_entity2] [...])";
		d.returns = R"(list of id_path)";
		d.description = R"(Moves the entity from location specified by `source_entity1` to destination `destination_entity1`.  If `destination_entity1` exists, it will move `source_entity1` using `source_entity1`'s current id path into `destination_entity1`.  If `destination_entity1` does not exist, then it will move `source_entity1` and rename it to the end of the id path specified by `destination_entity1`. Can only be performed by a containing entity relative to both ids.  If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity1"
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{m 3 n 4}
	)
	(move_entities "Entity1" "Entity2")
	(contained_entities "Entity2")
))&", R"(["NamedEntity1" "NamedEntity2" "_539JylCpbqn"])", "", R"((destroy_entities "Entity2"))"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_DESTROY_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity1] [id_path entity2] [...])";
		d.returns = R"(bool)";
		d.description = R"(Destroys the entities specified by the ids `entity1`, `entity2`, etc. Can only be performed by containing entity.  Returns true if all entities were successfully destroyed, false if not.  Generally entities can be destroyed unless they do not exist or if there is code currently being run in it.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 3 b 4}
	)
	(destroy_entities "Entity")
	(contained_entities)
))&", R"([])"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LOAD)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string resource_path [string resource_type] [assoc params])";
		d.returns = R"(any)";
		d.description = R"(Loads the data specified by `resource_path`, parses it into the appropriate code and data, and returns it. If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(store
		"file.amlg"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.amlg")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.amlg") (system "system" "rm file.amlg")))"},
			{R"&((seq
	(store
		"file.json"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.json")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.json") (system "system" "rm file.json")))"},
			{R"&((seq
	(store
		"file.yaml"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.yaml")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.yaml") (system "system" "rm file.yaml")))"},
			{R"&((seq
	(store "file.txt" "This is text.")
	(load "test.txt")
))&", R"((null))"},
			{R"&((seq
	(store
		"file.caml"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.caml")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.caml") (system "system" "rm file.caml")))"},
			{R"&((seq
	(declare
		{
			csv_data [
					[6.4 2.8 5.6 2.2 "virginica"]
					[4.9 2.5 4.5 1.7 "virg\"inica"]
					[]
					["" "" "" (null)]
					[4.9 3.1 1.5 0.1 "set\nosa" 3]
					[4.4 3.2 1.3 0.2 "setosa"]
				]
		}
	)
	(store "file.csv" csv_data)
	(load "file.csv")
))&", R"([
	[6.4 2.8 5.6 2.2 "virginica"]
	[4.9 2.5 4.5 1.7 "virg\"inica"]
	[(null)]
	[(null) (null) (null) (null)]
	[4.9 3.1 1.5 0.1 "set\nosa" 3]
	[4.4 3.2 1.3 0.2 "setosa"]
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.csv") (system "system" "rm file.csv")))"}
			});
		d.permissions = ExecutionPermissions::Permission::LOAD;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_LOAD_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string resource_path [id_path entity] [string resource_type] [bool persistent] [assoc params])";
		d.returns = R"(id_path)";
		d.description = R"(Loads the data specified by `resource_path` and parse it into the appropriate code and data, and stores it in `entity`.  It follows the same id path creation rules as `(create_entities)`, except that if no id path is specified, it may default to a name based on the resource if available.  If `persistent` is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2}
	)
	(create_entities
		["Entity" "Contained1"]
		{c 3}
	)
	(create_entities
		["Entity" "Contained1" "Contained2_1"]
		{d 4}
	)
	(create_entities
		["Entity" "Contained1" "Contained2_2"]
		{e 5}
	)
	(store_entity "entity.amlg" "Entity")
	(destroy_entities "Entity")
	(load_entity "entity.amlg" "Entity")
	(flatten_entity "Entity" .false)
))&", R"((declare
	{create_new_entity .true new_entity (null) require_version_compatibility .false}
	(let
		{
			_ (lambda
					{a 1 b 2}
				)
		}
		(if
			create_new_entity
			(assign
				"new_entity"
				(first
					(create_entities new_entity _)
				)
			)
			(assign_entity_roots new_entity _)
		)
	)
	(create_entities
		(append new_entity "Contained1")
		(lambda
			{c 3}
		)
	)
	(create_entities
		(append
			new_entity
			["Contained1" "Contained2_1"]
		)
		(lambda
			{d 4}
		)
	)
	(create_entities
		(append
			new_entity
			["Contained1" "Contained2_2"]
		)
		(lambda
			{e 5}
		)
	)
	new_entity
))", "", R"((seq (destroy_entities "Entity") (if (= (system "os") "Windows") (seq (system "system" "del /q entity*") (system "system" "rmdir /s /q entity")) (system "system" "rm -rf entity*"))))"},
			{R"&((seq
	(create_entities
		"Entity"
		[1 2 3 4]
	)
	(create_entities
		["Entity" "Contained1"]
		[5 6 7]
	)
	(create_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 8 nine 9}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_3"]
		[12 13]
	)
	(store_entity
		"entity.caml"
		"Entity"
		(null)
		.true
		{flatten .true transactional .true}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_2"]
		[10 11]
	)
	(destroy_entities
		["Entity" "Contained1" "Contained1_3"]
	)
	(assign_to_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 88}
	)
	(load_entity
		"entity.caml"
		"EntityCopy"
		(null)
		.false
		{execute_on_load .true require_version_compatibility .true transactional .true}
	)
	(declare
		{
			diff (difference_entities "EntityCopy" "Entity")
		}
	)
	(destroy_entities "EntityCopy" "Entity")
	diff
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))"}
			});
		d.permissions = ExecutionPermissions::Permission::LOAD;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_STORE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string resource_path * node [string resource_type] [assoc params])";
		d.returns = R"(bool)";
		d.description = R"(Stores `node` into `resource_path`.  Returns true if successful, false if not.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(store
		"file.amlg"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.amlg")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.amlg") (system "system" "rm file.amlg")))"},
			{R"&((seq
	(store
		"file.json"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.json")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.json") (system "system" "rm file.json")))"},
			{R"&((seq
	(store
		"file.yaml"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.yaml")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.yaml") (system "system" "rm file.yaml")))"},
			{R"&((seq
	(store "file.txt" "This is text.")
	(load "test.txt")
))&", R"((null))"},
			{R"&((seq
	(store
		"file.caml"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.caml")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.caml") (system "system" "rm file.caml")))"},
			{R"&((seq
	(declare
		{
			csv_data [
					[6.4 2.8 5.6 2.2 "virginica"]
					[4.9 2.5 4.5 1.7 "virg\"inica"]
					[]
					["" "" "" (null)]
					[4.9 3.1 1.5 0.1 "set\nosa" 3]
					[4.4 3.2 1.3 0.2 "setosa"]
				]
		}
	)
	(store "file.csv" csv_data)
	(load "file.csv")
))&", R"([
	[6.4 2.8 5.6 2.2 "virginica"]
	[4.9 2.5 4.5 1.7 "virg\"inica"]
	[(null)]
	[(null) (null) (null) (null)]
	[4.9 3.1 1.5 0.1 "set\nosa" 3]
	[4.4 3.2 1.3 0.2 "setosa"]
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.csv") (system "system" "rm file.csv")))"}
			});
		d.permissions = ExecutionPermissions::Permission::STORE;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_STORE_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string resource_path id_path entity [string resource_type] [bool persistent] [assoc params])";
		d.returns = R"(bool)";
		d.description = R"(Stores `entity` into `resource_path`.  Returns true if successful, false if not.  If `persistent` is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2}
	)
	(create_entities
		["Entity" "Contained1"]
		{c 3}
	)
	(create_entities
		["Entity" "Contained1" "Contained2_1"]
		{d 4}
	)
	(create_entities
		["Entity" "Contained1" "Contained2_2"]
		{e 5}
	)
	(store_entity "entity.amlg" "Entity")
	(destroy_entities "Entity")
	(load_entity "entity.amlg" "Entity")
	(flatten_entity "Entity" .false)
))&", R"((declare
	{create_new_entity .true new_entity (null) require_version_compatibility .false}
	(let
		{
			_ (lambda
					{a 1 b 2}
				)
		}
		(if
			create_new_entity
			(assign
				"new_entity"
				(first
					(create_entities new_entity _)
				)
			)
			(assign_entity_roots new_entity _)
		)
	)
	(create_entities
		(append new_entity "Contained1")
		(lambda
			{c 3}
		)
	)
	(create_entities
		(append
			new_entity
			["Contained1" "Contained2_1"]
		)
		(lambda
			{d 4}
		)
	)
	(create_entities
		(append
			new_entity
			["Contained1" "Contained2_2"]
		)
		(lambda
			{e 5}
		)
	)
	new_entity
))", "", R"((seq (destroy_entities "Entity") (if (= (system "os") "Windows") (seq (system "system" "del /q entity*") (system "system" "rmdir /s /q entity")) (system "system" "rm -rf entity*"))))"},
			{R"&((seq
	(create_entities
		"Entity"
		[1 2 3 4]
	)
	(create_entities
		["Entity" "Contained1"]
		[5 6 7]
	)
	(create_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 8 nine 9}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_3"]
		[12 13]
	)
	(store_entity
		"entity.caml"
		"Entity"
		(null)
		.true
		{flatten .true transactional .true}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_2"]
		[10 11]
	)
	(destroy_entities
		["Entity" "Contained1" "Contained1_3"]
	)
	(assign_to_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 88}
	)
	(load_entity
		"entity.caml"
		"EntityCopy"
		(null)
		.false
		{execute_on_load .true require_version_compatibility .true transactional .true}
	)
	(declare
		{
			diff (difference_entities "EntityCopy" "Entity")
		}
	)
	(destroy_entities "EntityCopy" "Entity")
	diff
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))"}
			});
		d.permissions = ExecutionPermissions::Permission::STORE;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CONTAINS_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity)";
		d.returns = R"(bool)";
		d.description = R"(Returns true if `entity` exists, false if not.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2}
	)
	(create_entities
		["Entity" "Contained1"]
		{c 3}
	)
	[
		(contains_entity "Entity")
		(contains_entity
			["Entity" "NotAnEntity"]
		)
	]
))&", R"([.true .false])", "", R"((destroy_entities "Entity")"}
			});
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CONTAINED_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path containing_entity | query|list condition1] [query|list condition2] ...[ query|list conditionN])";
		d.returns = R"(list of string)";
		d.description = R"(Returns a list of strings of ids of entities contained in `containing_entity` or the current entity if containing_entity is omitted or null.  The parameters of `condition1` through `conditionN` are query conditions, and they may be any of the query opcodes (beginning with `query_`) or may be a list of query opcodes, where each condition will be executed in order as a conjunction.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity1"
		{a 1 b 2}
		"Entity2"
		{c 3}
	)
	(create_entities
		["Entity2" "A"]
		{d 4}
		["Entity2"]
		{e 5}
	)
	[
		(contained_entities)
		(contained_entities "Entity2")
		(contained_entities
			(query_exists "a")
		)
	]
))&", R"([
	["Entity1" "Entity2"]
	["A" "_3SaCTguSSie"]
	["Entity1"]
])", "", R"((destroy_entities "Entity1" "Entity2"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_COMPUTE_ON_CONTAINED_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path containing_entity | query|list condition1] [query|list condition2] ...[ query|list conditionN])";
		d.returns = R"(any)";
		d.description = R"(Performs queries like `(contained_entities)` on `containing_entity` or the current entity if containing_entity is omitted or null, but returns a value or set of values appropriate for the last query in conditions.  The parameters of `condition1` through `conditionN` are query conditions, and they may be any of the query opcodes (beginning with `query_`) or may be a list of query opcodes, where each condition will be executed in order as a conjunction.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity1"
		{a 1 b 2}
		"Entity2"
		{a 3}
	)
	[
		(compute_on_contained_entities
			(query_exists "a")
		)
		(compute_on_contained_entities
			(query_exists "a")
			(query_sum "a")
		)
	]
))&", R"([
	{
		Entity1 {a 1}
		Entity2 {a 3}
	}
	4
])", "", R"((destroy_entities "Entity1" "Entity2"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_SELECT)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number num_to_select [number start_offset] [number random_seed])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects `num_to_select` entities sorted by entity id.  If `start_offset` is specified, then it will return `num_to_select` entities starting that far in, and subsequent calls can be used to get all entities in batches.  If `random_seed` is specified, then it will select `num_to_select` entities randomly from the list based on the random seed.  If `random_seed` is specified and `start_offset` is null, then it will not guarantee a position in the order for subsequent calls that specify `start_offset`, and will execute more quickly.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5 q 5}
	)
	[
		(contained_entities
			(query_select 3)
		)
		(contained_entities
			(query_select 3 1)
		)
		(contained_entities
			(query_select 100 2)
		)
		(contained_entities
			(query_select 2 0 1)
		)
		(contained_entities
			(query_select 2 2 1)
		)
		(contained_entities
			(query_select 2 4 1)
		)
		(contained_entities
			(query_select 4 (null) (rand))
		)
		(contained_entities
			(query_select 4 (null) (rand))
		)
		(contained_entities
			(query_not_exists "q")
			(query_select 2 3)
		)
	]
))&", R"([
	["E1" "E2" "E3"]
	["E2" "E3" "E4"]
	["E3" "E4" "E5"]
	["E2" "E3"]
	["E4" "E5"]
	["E1"]
	["E1" "E2" "E3" "E4"]
	["E1" "E2" "E3" "E4"]
	["E4"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_SAMPLE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number num_to_select [string weight_label_name] [number random_seed])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects a random sample of `num_to_select` entities sorted by entity id, sampled with replacement.  If `weight_label_name` is specified and not null, it will use `weight_label_name` as the feature containing the weights for the sampling, which will be normalized prior to sampling.  Non-numbers and negative infinite values for weights will be ignored, and if there are any infinite values, those will be selected from uniformly.  If `random_seed` is specified, then it will select `num_to_select` entities randomly from the list based on the random seed.  If `random_seed` is not specified then the subsequent calls will return the same sample of entities.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 weight 0.4}
		"E2"
		{a 2 weight 0.5}
		"E3"
		{a 3 weight 0.01}
		"E4"
		{a 4 weight 0.01}
		"E5"
		{a 5 q 5 weight 3.5}
	)
	[
		(contained_entities (query_sample))
		(contained_entities
			(query_sample 2)
		)
		(contained_entities
			(query_sample 1 (null) (rand))
		)
		(contained_entities
			(query_sample 1 (null) (null))
		)
		(contained_entities
			(query_sample 1 "weight")
		)
		(contained_entities
			(query_sample 1 "weight")
		)
		(contained_entities
			(query_sample 5 "weight" (rand))
		)
		(contained_entities
			(query_sample 5 "weight" (null))
		)
		(contained_entities
			(query_not_in_entity_list
				["E1" "E2" "E5"]
			)
			(query_sample 5 "weight" (rand))
		)
		(contained_entities
			(query_sample 10 "weight" (rand))
			(query_not_in_entity_list
				["E5"]
			)
		)
	]
))&", R"([
	["E1"]
	["E2" "E3"]
	["E4"]
	["E3"]
	["E5"]
	["E5"]
	["E2" "E5" "E2" "E5" "E5"]
	["E5" "E2" "E2" "E5" "E1"]
	["E3" "E4" "E3" "E3" "E4"]
	["E1" "E2"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_IN_ENTITY_LIST)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list entity_ids)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects only the entities in `entity_ids`.  It can be used to filter results before doing subsequent queries, especially to reduce computation required for complex queries.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
	)
	(contained_entities
		(query_in_entity_list
			["E1" "E2"]
		)
	)
))&", R"(["E1" "E2"])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_NOT_IN_ENTITY_LIST)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list entity_ids)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, filters out the entities in `entity_ids`.  It can be used to filter results before doing subsequent queries, especially to reduce computation required for complex queries.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
	)
	(contained_entities
		(query_not_in_entity_list
			["E1" "E2"]
		)
	)
))&", R"(["E3" "E4" "E5"])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_EXISTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities which have the label `label_name`.  If called last with compute_on_contained_entities, then it returns an assoc of entity ids, where each value is an assoc of corresponding label names and values.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{!e 3 a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
	)
	[
		(contained_entities
			(query_exists "a")
		)
		
		;can't find private labels
		(contained_entities
			(query_exists "!e")
		)
		(contained_entities
			(query_equals "a" 5)
			(query_exists "q")
		)
	]
))&", R"([
	["E1" "E2" "E3" "E4" "E5" "E5q"]
	[]
	["E5q"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_NOT_EXISTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities which do not have the the label `label_name`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
		"Eq"
		{q 0}
		"Er"
		{r 0}
	)
	[
		(contained_entities
			(query_not_exists "q")
		)
		(contained_entities
			(query_not_exists "q")
			(query_exists "a")
		)
	]
))&", R"([
	["E1" "E2" "E3" "E4" "E5" "Er"]
	["E1" "E2" "E3" "E4" "E5"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_EQUALS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name * value)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities for which the value at label `label_name` is equal to `value`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
		"Eq"
		{q 0}
		"Er"
		{r 0}
	)
	[
		(contained_entities
			(query_equals "a" 5)
		)
		(contained_entities
			(query_exists "q")
			(query_equals "a" 5)
		)
	]
))&", R"([
	["E5" "E5q"]
	["E5q"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_NOT_EQUALS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name * value)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities for which the value at label `label_name` is not equal to `value`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q 3}
		"Eq"
		{q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_not_equals "a" 5)
		)
		(contained_entities
			(query_not_equals "q" "q")
			(query_equals "q" 3)
		)
	]
))&", R"([
	["E1" "E2" "E3" "E4"]
	["E5q"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_BETWEEN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name * lower_bound * upper_bound)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities for which the value at label `label_name` is at least `lower_bound` and at most `upper_bound`, inclusive for both values.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_between "a" 2 4)
		)
		(contained_entities
			(query_between "a" 3 100)
			(query_between "q" "m" "z")
		)
	]
))&", R"([
	["E2" "E3" "E4"]
	["E6"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_NOT_BETWEEN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name * lower_bound * upper_bound)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities for which the value at label `label_name` is less than `lower_bound` or greater than `upper_bound`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_not_between "a" 2 4)
		)
		(contained_entities
			(query_exists "a")
			(query_not_between "q" "m" "z")
		)
	]
))&", R"([
	["E1" "E5" "E5q" "E6"]
	["E5q"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_AMONG)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name list values)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities for which the value at label `label_name` is one of the values specified in `values`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_among
				"a"
				[1 5]
			)
		)
		(contained_entities
			(query_exists "a")
			(query_among
				"q"
				["a"]
			)
		)
	]
))&", R"([
	["E1" "E5" "E5q"]
	["E5q"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_NOT_AMONG)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name list values)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities for which the value at label `label_name` is not one of the values specified in `values`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_not_among
				"a"
				[1 5]
			)
		)
		(contained_entities
			(query_exists "a")
			(query_not_among
				"q"
				["a"]
			)
		)
	]
))&", R"([
	["E2" "E3" "E4" "E6"]
	["E6"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_MAX)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [number num_entities] [bool numeric])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects a number of entities with the highest values for the label `label_name`.  If `num_entities` is specified, it will return that many entities, otherwise will return 1.  If `numeric` is true, its default value, then it only considers numeric values; if false, will consider all types.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_max "a")
		)
		(contained_entities
			(query_max "a" 2)
		)
		(compute_on_contained_entities
			(query_exists "a")
			(query_max "q" 1 .false)
			(query_exists "q")
		)
	]
))&", R"([
	["E6"]
	["E5" "E6"]
	{
		E6 {q "q"}
	}
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_MIN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [number entities_returned] [bool numeric])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects a number of entities with the lowest values for the label `label_name`.  If `num_entities` is specified, it will return that many entities, otherwise will return 1.  If `numeric` is true, its default value, then it only considers numeric values; if false, will consider all types.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5q"
		{a 5 q "a"}
		"E6"
		{a 6 q "q"}
		"Er"
		{r "r"}
	)
	[
		(contained_entities
			(query_min "a")
		)
		(contained_entities
			(query_min "a" 2)
		)
		(compute_on_contained_entities
			(query_exists "a")
			(query_min "q" 1 .false)
			(query_exists "q")
		)
	]
))&", R"([
	["E1"]
	["E1" "E2"]
	{
		E5q {q "a"}
	}
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_SUM)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [string weight_label_name])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, returns the sum of all entities over the value at `label_name`.  If `weight_label_name` is specified, it will find the weighted sum, which is the same as a dot product.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_sum "a")
		)
		(compute_on_contained_entities
			(query_sum "a" "weight")
		)
	]
))&", R"([15 35])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_MODE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [string weight_label_name])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, finds the statistical mode of `label_name` across all entities using numerical values.  If `weight_label_name` is specified, it will find the weighted mode.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
		"E5_2"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_mode "a")
		)
		(compute_on_contained_entities
			(query_mode "a" "weight")
		)
	]
))&", R"([5 1])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_QUANTILE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [number q] [string weight_label_name])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, finds the statistical quantile of `label_name` for numerical data, using `q` as the parameter to the quantile, the default being 0.5 which is the median.  If `weight_label_name` is specified, it will find the weighted quantile, otherwise the weight of every entity is 1.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_quantile "a" 0.5)
		)
		(compute_on_contained_entities
			(query_quantile "a" 0.5 "weight")
		)
		(compute_on_contained_entities
			(query_quantile "a" 0.25)
		)
		(compute_on_contained_entities
			(query_quantile "a" 0.25 "weight")
		)
	]
))&", R"([3 2.142857142857143 2 1.2777777777777777])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_GENERALIZED_MEAN)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [number p] [string weight_label_name] [number center] [bool calculate_moment] [bool absolute_value])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, computes the generalized mean over the label `label_name` for numerical data.  If `p` is specified (which defaults to 1), it is the parameter that can control the type of mean from minimum (negative infinity), to harmonic mean (-1), to geometric mean (0), to arithmetic mean (1), to maximum (infinity).  If `weight_label_name` is specified, it will normalize the weights and compute a weighted mean.  If `center` is specified, calculations will use that value as the central point, and the default is 0.0.  If `calculate_moment` is true, the results will not be raised to 1 / `p`.  If `absolute_value` is true, the differences will take the absolute value.  Various parameterizations of `(generalized_mean)` can be used to compute moments about the mean, especially by setting the `calculate_moment` parameter to true and using the mean as the center.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
	)
	(declare
		{
			mean (compute_on_contained_entities
					(query_generalized_mean "a" 1)
				)
		}
	)
	[
		mean
		(compute_on_contained_entities
			(query_generalized_mean "weight" 0)
		)
		(compute_on_contained_entities
			(query_generalized_mean "weight" -1)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 2)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 1 "weight")
		)
		(compute_on_contained_entities
			(query_generalized_mean "weight" 0 "weight")
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 1 (null) mean .true .true)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 2 (null) mean .true)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 3 (null) mean .false)
		)
		(compute_on_contained_entities
			(query_generalized_mean "a" 4 (null) mean .true)
		)
	]
))&", R"([
	3
	2.6051710846973517
	2.18978102189781
	3.3166247903554
	2.3333333333333335
	86400000.00000006
	1.2
	2
	0
	6.8
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_MIN_DIFFERENCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [number cyclic_range] [bool include_zero_difference])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, finds the smallest difference between any two values for the label `label_name`. If `cyclic_range` is null, the default value, then it will assume the values are not cyclic.  If `cyclic_range` is a number, then it will assume the range is from 0 to `cyclic_range`.  If `include_zero_difference` is true then it will return 0 if the smallest gap between any two numbers is 0.  If `include_zero_difference` is false, its default value, it will return the smallest nonzero value.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E0.1"
		{a 0.1}
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5.5"
		{a 5.5}
		"E5.5_2"
		{a 5.5}
	)
	[
		(compute_on_contained_entities
			(query_min_difference "a")
		)
		(compute_on_contained_entities
			(query_min_difference "a" (null) .true)
		)
		(compute_on_contained_entities
			(query_min_difference "a" 5.5 .false)
		)
	]
))&", R"([0.5 0 0.1])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_MAX_DIFFERENCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [number cyclic_range])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, finds the largest difference between any two values for the label `label_name`. If `cyclic_range` is null, the default value, then it will assume the values are not cyclic.  If `cyclic_range` is a number, then it will assume the range is from 0 to `cyclic_range`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E0.1"
		{a 0.1}
		"E1"
		{a 1}
		"E2"
		{a 2}
		"E3"
		{a 3}
		"E4"
		{a 4}
		"E5"
		{a 5}
		"E5.5"
		{a 5.5}
		"E5.5_2"
		{a 5.5}
	)
	[
		(compute_on_contained_entities
			(query_max_difference "a")
		)
		(compute_on_contained_entities
			(query_max_difference "a" 7.5)
		)
	]
))&", R"([1 2.1])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_VALUE_MASSES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name [string weight_label_name])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, computes the counts for each value of the label `label_name` and returns an assoc with the keys being the label values and the values being the counts or weights of the values.  If `weight_label_name` is specified, then it will accumulate that weight for each value, otherwise it will use a weight of 1 for each yielding a count.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 weight 5}
		"E2"
		{a 2 weight 4}
		"E3"
		{a 3 weight 3}
		"E4"
		{a 4 weight 2}
		"E5"
		{a 5 weight 1}
		"E5_2"
		{a 5 weight 1}
	)
	[
		(compute_on_contained_entities
			(query_value_masses "a")
		)
		(compute_on_contained_entities
			(query_value_masses "a" "weight")
		)
	]
))&", R"([
	{
		1 1
		2 1
		3 1
		4 1
		5 2
	}
	{
		1 5
		2 4
		3 3
		4 2
		5 2
	}
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_LESS_OR_EQUAL_TO)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name * max_value)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities with a value in label `label_name` less than or equal to `max_value`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 b "a"}
		"E2"
		{a 2 b "b"}
		"E3"
		{a 3 b "c"}
		"E4"
		{a 4 b "d"}
		"E5"
		{a 5 b "e"}
	)
	[
		(contained_entities
			(query_less_or_equal_to "a" 3)
		)
		(contained_entities
			(query_less_or_equal_to "b" "c")
		)
	]
))&", R"([
	["E1" "E2" "E3"]
	["E1" "E2" "E3"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_GREATER_OR_EQUAL_TO)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string label_name * min_value)";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects entities with a value in label `label_name` greater than or equal to `min_value`.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"E1"
		{a 1 b "a"}
		"E2"
		{a 2 b "b"}
		"E3"
		{a 3 b "c"}
		"E4"
		{a 4 b "d"}
		"E5"
		{a 5 b "e"}
	)
	[
		(contained_entities
			(query_greater_or_equal_to "a" 3)
		)
		(contained_entities
			(query_greater_or_equal_to "b" "c")
		)
	]
))&", R"([
	["E3" "E4" "E5"]
	["E3" "E4" "E5"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_WITHIN_GENERALIZED_DISTANCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(number max_distance list feature_labels list|string axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects the entities with labels that are at least as close as `max_distance` to the given point.  The parameter `axis_values_or_entity_id` specifies the corresponding values for the point to test from, or if `axis_values_or_entity_id` is a string the entity to collect the labels from.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
{R"&((seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	(compute_on_contained_entities
		(query_within_generalized_distance
			1.5
			["x" "y"]
			[1 2]
			2
			(null)
			(null)
			(null)
			(null)
			(null)
			1
			(null)
			"random seed 1234"
		)
	)
))&", R"({vert2 1 vert3 1.4142135623730951 vert5 1.4142135623730951})", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_NEAREST_GENERALIZED_DISTANCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|number selection_bandwidth list feature_labels list|string axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list])";
		d.returns = R"(query)";
		d.description = R"(When used as a query argument, selects the closest entities to the given point.  The parameter `axis_values_or_entity_id` specifies the corresponding values for the point to test from, or if `axis_values_or_entity_id` is a string the entity to collect the labels from.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			(query_nearest_generalized_distance
				3
				["x" "y"]
				[1 2]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_nearest_generalized_distance
				[0.2 1]
				["x" "y"]
				[1 2]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				1
				(null)
				"random seed 1234"
			)
		)
	]
))&", R"([
	{vert2 1 vert3 1.4142135623730951 vert5 1.4142135623730951}
	{
		vert2 1
		vert3 1.4142135623730951
		vert4 1.5811388300841898
		vert5 1.4142135623730951
	}
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_DISTANCE_CONTRIBUTIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|number selection_bandwidth list feature_labels list axis_values_or_entity_id [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list])";
		d.returns = R"(query)";
		d.allowsConcurrency = true;
		d.description = R"(When used as a query argument, computes the distance or surprisal contribution for every entity.  The parameter `axis_values_or_entity_id` specifies the corresponding values for the point to test from, or if `axis_values_or_entity_id` is a string the entity to collect the labels from.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	(compute_on_contained_entities
		(query_distance_contributions
			2
			["x" "y"]
			[
				[1 2]
			]
			2
			(null)
			(null)
			(null)
			(null)
			(null)
			-1
			(null)
			"random seed 1234"
		)
	)
))&", R"([1.17157287525381])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_ENTITY_CONVICTIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal] [* output_sorted_list])";
		d.returns = R"(query)";
		d.allowsConcurrency = true;
		d.description = R"(When used as a query argument, computes the case conviction for every case given in `entity_ids_to_compute` with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			(query_entity_convictions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_entity_convictions
				2
				["x" "y"]
				["vert0" "vert1" "vert2"]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_convictions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_convictions
				2
				["x" "y"]
				(contained_entities
					(query_exists "object")
				)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
	]
))&", R"([
	{
		vert0 20.64507068846579
		vert1 10.936029135274522
		vert2 0.7220080663101216
		vert3 20.64507068846579
		vert4 0.6024424202611007
		vert5 0.361435163373361
	}
	{vert0 10.493921488434916 vert1 5.558800590831786 vert2 0.3669978212333347}
	{
		vert0 20.64507068846579
		vert1 10.936029135274522
		vert2 0.7220080663101216
		vert3 20.64507068846579
		vert4 0.6024424202611007
		vert5 0.361435163373361
	}
	{
		vert0 20.64507068846579
		vert1 10.936029135274522
		vert2 0.7220080663101216
		vert3 20.64507068846579
		vert4 0.6024424202611007
		vert5 0.361435163373361
	}
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal])";
		d.returns = R"(query)";
		d.allowsConcurrency = true;
		d.description = R"(When used as a query argument, computes the case kl divergence for every case given in `entity_ids_to_compute` as a group with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	(compute_on_contained_entities
		(query_exists "object")
		(query_entity_group_kl_divergence
			2
			["x" "y"]
			(contained_entities
				(query_equals "object" 2)
			)
			2
			(null)
			(null)
			(null)
			(null)
			(null)
			-1
			(null)
			"random seed 1234"
		)
	)
))&", R"(0.01228960638554566)", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list])";
		d.returns = R"(query)";
		d.allowsConcurrency = true;
		d.description = R"(When used as a query argument, computes the case conviction for every case given in `entity_ids_to_compute` with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			(query_entity_distance_contributions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_entity_distance_contributions
				2
				["x" "y"]
				["vert0" "vert1" "vert2"]
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_distance_contributions
				2
				["x" "y"]
				(null)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
		(compute_on_contained_entities
			(query_exists "object")
			(query_entity_distance_contributions
				2
				["x" "y"]
				(contained_entities
					(query_exists "object")
				)
				2
				(null)
				(null)
				(null)
				(null)
				(null)
				-1
				(null)
				"random seed 1234"
			)
		)
	]
))&", R"([
	{
		vert0 0.8284271247461902
		vert1 0.8284271247461902
		vert2 0.8284271247461902
		vert3 0.8284271247461902
		vert4 0.7071067811865476
		vert5 1.17157287525381
	}
	{vert0 0.8284271247461902 vert1 0.8284271247461902 vert2 0.8284271247461902}
	{
		vert0 0.8284271247461902
		vert1 0.8284271247461902
		vert2 0.8284271247461902
		vert3 0.8284271247461902
		vert4 0.7071067811865476
		vert5 1.17157287525381
	}
	{
		vert0 0.8284271247461902
		vert1 0.8284271247461902
		vert2 0.8284271247461902
		vert3 0.8284271247461902
		vert4 0.7071067811865476
		vert5 1.17157287525381
	}
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_ENTITY_KL_DIVERGENCES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [bool conviction_of_removal] [* output_sorted_list])";
		d.returns = R"(query)";
		d.allowsConcurrency = true;
		d.description = R"(When used as a query argument, computes the case conviction for every case given in `entity_ids_to_compute` with respect to *all* cases in the contained entities set input during a query.  If `entity_ids_to_compute` is null or an empty list, case conviction is computed for all cases.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"vert0"
		{object 1 x 0 y 0}
	)
	(create_entities
		"vert1"
		{object 1 x 1 y 0}
	)
	(create_entities
		"vert2"
		{object 1 x 1 y 1}
	)
	(create_entities
		"vert3"
		{object 1 x 0 y 1}
	)
	(create_entities
		"vert4"
		{object 2 x 0.5 y 0.5}
	)
	(create_entities
		"vert5"
		{object 2 x 2 y 1}
	)
	[
		(compute_on_contained_entities
			[
				(query_exists "object")
				(query_entity_kl_divergences
					2
					["x" "y"]
					(null)
					2
					(null)
					(null)
					(null)
					(null)
					(null)
					-1
					(null)
					"random seed 1234"
				)
			]
		)
		(compute_on_contained_entities
			[
				(query_exists "object")
				(query_entity_kl_divergences
					2
					["x" "y"]
					(contained_entities
						(query_exists "object")
					)
					2
					(null)
					(null)
					(null)
					(null)
					(null)
					-1
					(null)
					"random seed 1234"
				)
			]
		)
	]
))&", R"([
	{
		vert0 0.00018681393615961172
		vert1 0.0003526679446349979
		vert2 0.005341750456218763
		vert3 0.00018681393615961172
		vert4 0.006401917906003654
		vert5 0.010670757326457676
	}
	{
		vert0 0.00018681393615961172
		vert1 0.0003526679446349979
		vert2 0.005341750456218763
		vert3 0.00018681393615961172
		vert4 0.006401917906003654
		vert5 0.010670757326457676
	}
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS)] = []() {
		OpcodeDetails d;
		d.parameters = R"(list|number selection_bandwidth list feature_labels list entity_ids_to_compute [number p_value] [list|assoc|assoc of assoc weights] [list|assoc distance_types] [list|assoc attributes] [list|assoc deviations] [list|string weights_selection_features] [string|number distance_transform] [string entity_weight_label_name] [number random_seed] [string radius_label] [string numerical_precision] [* output_sorted_list])";
		d.returns = R"(query)";
		d.allowsConcurrency = true;
		d.description = R"(When used as a query argument, computes the nearest neighbors to every entity given by `entity_ids_to_compute`, normalizes their influence weights, and accumulates the entity's total influence weights relative to every other case.  It returns a list of all cases whose cumulative neighbor values are greater than zero.  See Distance and Surprisal Calculations for details on the other parameters and how distance is computed.  If `output_sorted_list` is not specified or is false, then it will return an assoc of entity string id as the key with the distance as the value; if `output_sorted_list` is true, then it will return a list of lists, where the first list is the entity ids and the second list contains the corresponding distances, where both lists are in sorted order starting with the closest or most important (based on whether `distance_weight_exponent` is positive or negative respectively). If `output_sorted_list` is a string, then it will additionally return a list where the values correspond to the values of the labels for each respective entity.  If `output_sorted_list` is a list of strings, then it will additionally return a list of values for each of the label values for each respective entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"entity1"
		{alpha 3 b 0.17 c 1}
	)
	(create_entities
		"entity2"
		{alpha 4 b 0.12 c 0}
	)
	(create_entities
		"entity3"
		{
			alpha 5
			b 0.1
			c 0
			x 16
		}
	)
	(create_entities
		"entity4"
		{
			alpha 1
			b 0.14
			c 1
			x 8
		}
	)
	(create_entities
		"entity5"
		{
			alpha 9
			b 0.11
			c 1
			x 32
		}
	)
	(compute_on_contained_entities
		[
			(query_entity_cumulative_nearest_entity_weights
				2
				["alpha" "b" "c"]
				(null)
				0.5
				(null)
				[0 0 1]
			)
		]
	)
))&", R"({
	entity1 2.3019715434701133
	entity2 1.5822400592793713
	entity3 0.7781961968246989
	entity4 0.33759220042581645
})", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
		d.isQuery = true;
		d.potentiallyIdempotent = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CONTAINS_LABEL)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity] string label_name)";
		d.returns = R"(bool)";
		d.description = R"(Evaluates to true if the label represented by `label_name` exists for `entity`.  If `entity` is omitted or null, then it uses the current entity.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2 c 3}
	)
	[
		(contains_label "Entity" "a")
		(contains_label "Entity" "z")
	]
))&", R"([.true .false])", "", R"((destroy_entities "Entity"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ASSIGN_TO_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity1] assoc label_value_pairs1 [id_path entity2] [assoc label_value_pairs2] [...])";
		d.returns = R"(bool)";
		d.description = R"(For each index-value pair of `label_value_pairs`, assigns the value to the label on the contained entity represented by the respective `entity`, itself if `entity` is not specified or is null.  If the label is not found, it will create it.  Returns true if all assignments were successful, false if not.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2 c 3}
	)
	(assign_to_entities
		"Entity"
		{a 2 b 3 c 4}
		"Entity"
		{three 12}
	)
	(retrieve_entity_root "Entity")
))&", R"({
	a 2
	b 3
	c 4
	three 12
})", "", R"((destroy_entities "Entity"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_ACCUM_TO_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity1] assoc label_value_pairs1 [id_path entity2] [assoc label_value_pairs2] [...])";
		d.returns = R"(bool)";
		d.description = R"(For each index-value pair of `label_value_pairs`, it accumulates the value to the label on the contained entity represented by the respective `entity`, itself if `entity` is not specified or is null.  If the label is not found, it will create it.  Returns true if all assignments were successful, false if not.  Accumulation is performed differently based on the type: for numeric values it adds, for strings, it concatenates, for lists it appends, and for assocs it appends based on the pair.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2 c 3}
	)
	(accum_to_entities
		"Entity"
		{a 2 b 3 c 4}
		"Entity"
		{doesnt_exist 12}
	)
	(retrieve_entity_root "Entity")
))&", R"({a 3 b 5 c 7})", "", R"((destroy_entities "Entity"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_REMOVE_FROM_ENTITIES)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity1] string|list label_names1 [id_path entity2] [list string|label_names2] [...])";
		d.returns = R"(bool)";
		d.description = R"(Removes all labels in `label_names1` from `entity1` and so on for each respective entity and label list.  Returns true if all removes were successful, false otherwise.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{
			a 1
			b 2
			c 3
			d 4
		}
	)
	(remove_from_entities
		"Entity"
		"a"
		"Entity"
		["b" "c"]
	)
	(retrieve_entity_root "Entity")
))&", R"({d 4})", "", R"((destroy_entities "Entity"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_RETRIEVE_FROM_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"([id_path entity] [string|list|assoc label_names])";
		d.returns = R"(any)";
		d.description = R"(Retrieves one or more labels from `entity`, using its own entity if `entity` is omitted or null.  If `label_names` is a string, it returns the value at the corresponding label.  If `label_names` is a list, it returns a list of the values of the labels of the corresponding labels.  If `label_names` is an assoc, it an assoc with label names as keys and the label values as the values.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 12 b 13}
	)
	[
		(retrieve_from_entity "Entity" "a")
		(retrieve_from_entity
			"Entity"
			["a" "b"]
		)
		(retrieve_from_entity
			"Entity"
			(zip
				["a" "b"]
				null
			)
		)
	]
))&", R"([
	12
	[12 13]
	{a 12 b 13}
])", "", R"((destroy_entities "Entity"))"}
			});
		d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
		d.requiresEntity = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CALL_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity [string label_name] [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [bool return_warnings])";
		d.returns = R"(any)";
		d.description = R"(Calls the contained `entity` and returns the result of the call.  If `label_name` is specified, then it will call the label specified by string, otherwise it will call the null label.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{
				!private_method "should not access"
				copy_entity (while
						.true
						(clone_entities (null) (null))
					)
				hello (declare
						{message ""}
						(concat "hello " message)
					)
				load (while .true)
			}
		)
	)
	[
		(call_entity
			"Entity"
			"hello"
			{message "world"}
		)
		(call_entity "Entity" "!private_method")
		(call_entity "Entity" "load" (null) 100)
		(call_entity
			"Entity"
			"copy_entity"
			(null)
			1000
			1000
			100
			10
			3
			20
		)
	]
))&", R"([
	"hello world"
	(null)
	[(null) {} "Execution step limit exceeded"]
	[(null) {} "Execution step limit exceeded"]
])", "", R"((destroy_entities "Entity"))"}
			});
		d.requiresEntity = true;
		d.newScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CALL_ENTITY_GET_CHANGES)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity [string label_name] [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [bool return_warnings])";
		d.returns = R"(list of any1 any2)";
		d.description = R"(Calls the contained `entity` and returns the result of the call.  However, it also returns a list of opcodes that hold an executable log of all of the changes that have elapsed to the entity and its contained entities.  The log may be evaluated to apply or re-apply the changes to any entity passed in to the executable log as the parameter "_".  If `label_name` is specified, then it will call the label specified by string, otherwise it will call the null label.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{
				a_assign (seq
						(create_entities
							"Contained"
							{a 4 b 6}
						)
						(assign_to_entities
							"Contained"
							{a 6 b 10}
						)
						(set_entity_rand_seed "Contained" "bbbb")
						(accum_to_entities
							"Contained"
							{b 12}
						)
						(destroy_entities "Contained")
					)
			}
		)
	)
	(set_entity_permissions "Entity" .true)
	(call_entity_get_changes "Entity" "a_assign")
))&", R"([
	.true
	(seq
		(create_entities
			["Entity" "Contained"]
			(lambda
				{a 4 b 6}
			)
		)
		(assign_to_entities
			["Entity" "Contained"]
			{a 6 b 10}
		)
		(set_entity_rand_seed
			["Entity" "Contained"]
			"bbbb"
			.false
		)
		(accum_to_entities
			["Entity" "Contained"]
			{b 12}
		)
		(destroy_entities
			["Entity" "Contained"]
		)
	)
])", "", R"((destroy_entities "Entity"))"}
			});
		d.requiresEntity = true;
		d.newScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CALL_ON_ENTITY)] = []() {
		OpcodeDetails d;
		d.parameters = R"(id_path entity * code [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [number max_contained_entities] [number max_contained_entity_depth] [number max_entity_id_length] [bool return_warnings])";
		d.returns = R"(any)";
		d.description = R"(Calls `code` to be run on the contained `entity` and returns the result of the call.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2}
	)
	(set_entity_permissions "Entity" .true)
	(call_on_entity
		"Entity"
		(lambda
			[a b c]
		)
		{c 3}
	)
))&", R"([1 2 3])", "", R"((destroy_entities "Entity"))"}
			});
		d.requiresEntity = true;
		d.newScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	arr[static_cast<std::size_t>(ENT_CALL_CONTAINER)] = []() {
		OpcodeDetails d;
		d.parameters = R"(string parent_label_name [assoc arguments] [number operation_limit] [number max_node_allocations] [number max_opcode_execution_depth] [bool return_warnings])";
		d.returns = R"(any)";
		d.description = R"(Attempts to call the container associated with `label_name` that must begin with a caret; the caret indicates that the label is allowed to be accessed by contained entities.  It will evaluate to the return value of the call.  If `arguments` is specified, then it will pass those as the arguments on the scope stack.  If `operation_limit` is specified, it represents the number of operations that are allowed to be performed.  If `operation_limit` is 0 or infinite, then an infinite of operations will be allotted to the entity, but only if its containing entity (the current entity) has infinite operations.  If `max_node_allocations` is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory.   If `max_node_allocations` is 0 or infinite, then there is no limit to the number of nodes to be allotted to the entity as long as the machine has sufficient memory, but only if the containing entity (the current entity) has unlimited memory access.  If `max_opcode_execution_depth` is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise `max_opcode_execution_depth` limits how deep nested opcodes will be called.  The parameters `max_contained_entities`, `max_contained_entity_depth`, and `max_entity_id_length` constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true, the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.)";
		d.examples = MakeAmalgamExamples({
			{R"&((seq
	(create_entities
		"OuterEntity"
		(lambda
			{
				^available_method 3
				compute_value (call_entity
						"InnerEntity"
						"inner_call"
						{x x}
					)
			}
		)
	)
	(create_entities
		["OuterEntity" "InnerEntity"]
		(lambda
			{
				inner_call (+
						x
						(call_container "^available_method")
					)
			}
		)
	)
	[
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			30
			30
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			1
			1
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			1
			1
			1
			1
			1
			.false
		)
	]
))&", R"([
	8
	[
		[8 {} (null)]
		{}
		(null)
	]
	[(null) {} "Execution step limit exceeded"]
	[(null) {} "Execution step limit exceeded"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
			});
		d.requiresEntity = true;
		d.newScope = true;
		d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
		d.hasSideEffects = true;
		return d;
	}();

	return arr;
}

std::array<OpcodeDetails, NUM_ENT_OPCODES> _opcode_details = BuildAmalgamExamplesArrayForOpcodes();
