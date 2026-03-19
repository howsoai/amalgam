//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "Parser.h"
#include "PerformanceProfiler.h"
#include "PlatformSpecific.h"

//system headers:
#include <regex>

//returns a copy of s where each consecutive whitespace block is replaced
//by a single space and any leading and trailing spaces are removed
static std::string NormalizeWhitespace(const std::string &s)
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
inline static bool EqualIgnoringWhitespace(const std::string &a, const std::string &b)
{
	return NormalizeWhitespace(a) == NormalizeWhitespace(b);
}

//runs a test suite against the language
//the return value of this function will be returned for the executable
int32_t RunAmalgamLanguageValidation()
{
	Entity *entity = new Entity();
	entity->SetPermissions(ExecutionPermissions::AllPermissions(), ExecutionPermissions::AllPermissions(), true);

	std::vector<std::pair<std::string, size_t>> failed_test_names_and_numbers;
	bool any_failures = false;

	//TODO 25158: replace with the top for loop when all are implemented
	//for(size_t opcode_index = 0; opcode_index < NUM_VALID_ENT_OPCODES; opcode_index++)
	for(size_t opcode_index = 0; opcode_index < ENT_SIZE; opcode_index++)
	{
		EvaluableNodeType cur_opcode = static_cast<EvaluableNodeType>(opcode_index);
		std::string cur_opcode_str = GetStringFromEvaluableNodeType(cur_opcode, true);
		std::cout << "Validating opcode " << cur_opcode_str << std::endl;

		size_t num_examples = _opcode_details[opcode_index].examples.size();
		for(size_t test_number = 0; test_number < num_examples; test_number++)
		{
			bool test_succeeded = true;
			auto &example = _opcode_details[opcode_index].examples[test_number];
			std::cout << "Test " << (test_number + 1) << " of " << num_examples << ": ";

			entity->SetRandomState("12345", true);

			auto [code, warnings, char_with_error, code_complete]
				= Parser::Parse(example.example, &entity->evaluableNodeManager);

			auto result = entity->ExecuteOnEntity(code, nullptr);
			std::string result_str = Parser::Unparse(result, true, true, true);

			if(example.regexMatch.empty())
			{
				if(!EqualIgnoringWhitespace(result_str, example.output))
				{
					std::cerr << "Failed, ran code:" << std::endl;
					std::cerr << example.example << std::endl;
					std::cerr << "Expected result:" << std::endl;
					std::cerr << example.output << std::endl;
					std::cerr << "Observed result:" << std::endl;
					std::cerr << result_str << std::endl;
					test_succeeded = false;
				}
			}
			else //match with regular expression
			{
				std::regex pattern(example.regexMatch, std::regex::ECMAScript);
				if(std::regex_match(result_str, pattern))
				{
					std::cerr << "Failed, ran code:" << std::endl;
					std::cerr << example.example << std::endl;
					std::cerr << "Expected to match:" << std::endl;
					std::cerr << example.regexMatch << std::endl;
					std::cerr << "Observed:" << std::endl;
					std::cerr << result_str << std::endl;
					test_succeeded = false;
				}
			}

			//if the test needs to be cleaned up, do so
			if(!example.cleanup.empty())
			{
				std::cout << "Cleaning up test." << std::endl;
				auto [cleanup_code, cleanup_warnings, cleanup_char_with_error, cleanup_code_complete]
					= Parser::Parse(example.example, &entity->evaluableNodeManager);

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
				std::cerr << "Failed: Labels remain in entity after test" << std::endl;
				test_succeeded = false;
			}

			if(test_succeeded)
				std::cout << "Passed" << std::endl;
			else
				failed_test_names_and_numbers.emplace_back(cur_opcode_str, test_number);
		}
	}

	//TODO 25158: implement tests beyond opcode tests, should genericize loop above

	delete entity;

	if(failed_test_names_and_numbers.size() == 0)
	{
		std::cout << "All Tests Passed" << std::endl;
		return 0;
	}

	std::cout << "Not All Tests Passed:" << std::endl;

	for(auto &[test_name, test_number] : failed_test_names_and_numbers)
		std::cerr << "Failed " << test_name << " test number " << (test_number + 1) << std::endl;

	return -1;
}
