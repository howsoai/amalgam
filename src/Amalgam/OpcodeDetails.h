#pragma once

//project headers:
#include "Entity.h"
#include "Opcodes.h"

//system headers:
#include <array>
#include <string>
#include <vector>

//contains details, including descriptions, examples, and effects for the corresponding opcode
class OpcodeDetails
{
public:
	struct OpcodeExampleOutputPair
	{
		OpcodeExampleOutputPair(std::string e, std::string o) :
			example(std::move(e)), output(std::move(o))
		{}

		std::string example;
		std::string output;
	};

	enum class OpcodeReturnNewnessType
	{
		NEW, PARTIAL, CONDITIONAL, EXISTING
	};

	//TODO 25157: comment and organize this better
	//TODO 25157: fold any other opcode flag methods into this data structure
	//TODO 25157: create scripts to autogenerate the json file / update docs
	std::string parameters;
	std::string output;
	std::string description;
	std::vector<OpcodeExampleOutputPair> exampleOutputPairs;
	EntityPermissions::Permission permissions = EntityPermissions::Permission::NONE;
	OpcodeReturnNewnessType valueNewness = OpcodeReturnNewnessType::EXISTING;
	bool allowsConcurrency = false;
	bool requiresEntity = false;
	bool newScope = false;
	bool newTargetScope = false;
};

extern const std::array<OpcodeDetails, NUM_ENT_OPCODES> _opcode_details;
