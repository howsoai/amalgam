#pragma once

#include <array>
#include <string>
#include <vector>

#include "Entity.h"   // EntityPermissions enum
#include "Opcodes.h"   // EvaluableNodeType enum (ENT_…) 

struct ExampleOutputPair {
    std::string example;
    std::string output;
    ExampleOutputPair(std::string e, std::string o) :
        example(std::move(e)), output(std::move(o)) {}
};

enum class NewValue { New, Partial, Conditional, Existing };

class OpcodeDetails {
public:
    std::string opcode;
    std::string parameters;
    std::string output;
    bool concurrency = false;
    std::string description;
    std::vector<ExampleOutputPair> exampleOutputPairs;
    EntityPermissions::Permission permissions = EntityPermissions::Permission::NONE;
    bool requiresEntity = false;
    bool newScope = false;
    bool newTargetScope = false;
    NewValue newValue = NewValue::Existing;
};

#define VERIFY_ENUM_INDEX(enum_val, idx) \
    static_assert(static_cast<std::size_t>(enum_val) == idx, "Enum value does not match array index")

extern const std::array<OpcodeDetails, NUM_ENT_OPCODES> kOpcodeDetails;