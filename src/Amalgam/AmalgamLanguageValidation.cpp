//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "Parser.h"
#include "PerformanceProfiler.h"
#include "PlatformSpecific.h"

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

	bool any_failures = false;

	for(size_t opcode_index = 0; opcode_index < NUM_VALID_ENT_OPCODES; opcode_index++)
	{
		EvaluableNodeType cur_opcode = static_cast<EvaluableNodeType>(opcode_index);
		std::string cur_opcode_str = GetStringFromEvaluableNodeType(cur_opcode, true);
		std::cout << "Validating opcode " << cur_opcode_str << std::endl;

		size_t num_examples = _opcode_details[opcode_index].exampleOutputPairs.size();
		for(size_t test_number = 0; test_number < num_examples; test_number++)
		{
			bool test_succeeded = true;
			auto &example_output_pair = _opcode_details[opcode_index].exampleOutputPairs[test_number];
			std::cout << "Test " << (test_number + 1) << " of " << num_examples << ": ";

			entity->SetRandomState("12345", true);

			auto [code, warnings, char_with_error, code_complete]
				= Parser::Parse(example_output_pair.example, &entity->evaluableNodeManager);

			auto result = entity->ExecuteOnEntity(code, nullptr);
			std::string result_str = Parser::Unparse(result, true, true, true);

			//TODO 25158: put this line back in once fix the bug
			//if(!EqualIgnoringWhitespace(result_str, example_output_pair.output))
			if(result_str != example_output_pair.output)
			{
				std::cerr << "Failed, expected:" << std::endl;
				std::cerr << example_output_pair.output << std::endl;
				std::cerr << "Observed:" << std::endl;
				std::cerr << result_str << std::endl;
				test_succeeded = false;
			}

			//TODO 25158: implement test

			entity->ReclaimResources(false, true, false);
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
				any_failures = true;
		}
	}

	delete entity;

	return (any_failures ? -1 : 0);
}
