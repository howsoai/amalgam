//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "Parser.h"
#include "PerformanceProfiler.h"
#include "PlatformSpecific.h"

template<class... Ts>
constexpr std::array<AmalgamExample, sizeof...(Ts)>
MakeAmalgamUnitTests(Ts... elems)
{
	return { std::forward<Ts>(elems)... };
}

//TODO 25158: implement unit tests
auto _amalgam_unit_tests = MakeAmalgamUnitTests(
	AmalgamExample{ "1", "1" }
);

//runs a test suite against the language
//the return value of this function will be returned for the executable
int32_t RunAmalgamLanguageValidation()
{
	Entity *entity = new Entity();
	entity->SetPermissions(ExecutionPermissions::AllPermissions(), ExecutionPermissions::AllPermissions(), true);

	std::vector<std::pair<std::string, size_t>> failed_test_names_and_numbers;

	//TODO 25158: replace with the top for loop when all are implemented
	//for(size_t opcode_index = 0; opcode_index < NUM_VALID_ENT_OPCODES; opcode_index++)
	for(size_t opcode_index = 0; opcode_index < ENT_LOAD; opcode_index++)
	{
		EvaluableNodeType cur_opcode = static_cast<EvaluableNodeType>(opcode_index);
		std::string cur_opcode_str = GetStringFromEvaluableNodeType(cur_opcode, true);
		std::cout << "Validating opcode " << cur_opcode_str << std::endl;

		size_t num_examples = _opcode_details[opcode_index].examples.size();
		for(size_t test_number = 0; test_number < num_examples; test_number++)
		{
			auto &example = _opcode_details[opcode_index].examples[test_number];
			std::cout << "Test " << (test_number + 1) << " of " << num_examples << ": ";

			if(example.ValidateExample(entity))
				std::cout << "Passed" << std::endl;
			else
				failed_test_names_and_numbers.emplace_back(cur_opcode_str, test_number);
		}
	}

	for(size_t unit_test_num = 0; unit_test_num < _amalgam_unit_tests.size(); unit_test_num++)
	{
		auto &unit_test = _amalgam_unit_tests[unit_test_num];
		std::cout << "Validating unit test " << (unit_test_num + 1) << " of " << _amalgam_unit_tests.size() << ": ";

		if(unit_test.ValidateExample(entity))
			std::cout << "Passed" << std::endl;
		else
			failed_test_names_and_numbers.emplace_back("unit test", unit_test_num);
	}

	delete entity;

	if(failed_test_names_and_numbers.size() == 0)
	{
		std::cout << "----------------" << std::endl;
		std::cout << "All Tests Passed" << std::endl;
		return 0;
	}

	std::cout << "---------------------" << std::endl;
	std::cout << "Not All Tests Passed:" << std::endl;

	for(auto &[test_name, test_number] : failed_test_names_and_numbers)
		std::cerr << "Failed " << test_name << " test number " << (test_number + 1) << std::endl;

	return -1;
}
