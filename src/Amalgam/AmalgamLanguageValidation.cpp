//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "Parser.h"
#include "PerformanceProfiler.h"
#include "PlatformSpecific.h"

//runs a test suite against the language
//the return value of this function will be returned for the executable
int32_t RunAmalgamLanguageValidation()
{
	Entity *entity = new Entity();
	bool any_failures = false;

	for(size_t opcode_index = 0; opcode_index < NUM_VALID_ENT_OPCODES; opcode_index++)
	{
		EvaluableNodeType cur_opcode = static_cast<EvaluableNodeType>(opcode_index);
		std::string cur_opcode_str = GetStringFromEvaluableNodeType(cur_opcode, true);
		std::cout << "Validating opcode " << cur_opcode_str << std::endl;

		size_t num_examples = _opcode_details[opcode_index].exampleOutputPairs.size();
		for(size_t test_number = 0; test_number < num_examples; test_number++)
		{
			auto &example_output_pair = _opcode_details[opcode_index].exampleOutputPairs[test_number];
			std::cout << "Test " << (test_number + 1) << " of " << num_examples << ": ";

			//TODO 25158: implement test

			entity->ReclaimResources(false, true, false);
			if(entity->GetLabelIndex().size() != 0)
			{
				std::cerr << "Failed: Labels remain in entity after test" << std::endl;
				any_failures = true;
			}

			std::cout << "Passed" << std::endl;
		}
	}

	delete entity;

	return (any_failures ? -1 : 0);
}
