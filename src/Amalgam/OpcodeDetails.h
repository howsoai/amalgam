#pragma once

//project headers:
#include "FastMath.h"
#include "Opcodes.h"

//system headers:
#include <array>
#include <string>
#include <vector>

//forward declarations
class EvaluableNode;
class EvaluableNodeManager;
class ExecutionPermissions
{
public:
	using StorageType = uint8_t;
	enum class Permission : StorageType
	{
		NONE = 0,
		STD_OUT_AND_STD_ERR = 1 << 0,
		STD_IN = 1 << 1,
		LOAD = 1 << 2,
		STORE = 1 << 3,
		ENVIRONMENT = 1 << 4,
		ALTER_PERFORMANCE = 1 << 5,
		SYSTEM = 1 << 6,
		ALL = STD_OUT_AND_STD_ERR | STD_IN | LOAD | STORE | ENVIRONMENT | ALTER_PERFORMANCE | SYSTEM
	};

	ExecutionPermissions() = default;

	explicit inline ExecutionPermissions(Permission initial_permissions)
		: allPermissions(static_cast<StorageType>(initial_permissions))
	{}

	inline bool HasPermission(Permission permission) const
	{
		return (allPermissions & static_cast<StorageType>(permission)) != 0;
	}

	inline void SetPermission(Permission permission, bool enable = true)
	{
		if(enable)
			allPermissions |= static_cast<StorageType>(permission);
		else
			allPermissions &= ~static_cast<StorageType>(permission);
	}

	static ExecutionPermissions AllPermissions()
	{
		return ExecutionPermissions(Permission::ALL);
	}

	//builds a new assoc from enm and returns it populated with
	//the permissions
	EvaluableNode *GetPermissionsAsEvaluableNode(EvaluableNodeManager *enm);

	//returns a pair of [permissions_to_set, permission_values] corresponding to en
	//if en is an assoc, it will use key-value pairs to obtain the permissions and their values
	//otherwise it will set all permissions based on whether en is true
	static std::pair<ExecutionPermissions, ExecutionPermissions> EvaluableNodeToPermissions(EvaluableNode *en);

	//method to get the type into a basic permissions type easily
	Permission permissions() const noexcept
	{
		return static_cast<Permission>(allPermissions);
	}

	//permissions as a bit field for use with bitwise operations
	StorageType allPermissions = static_cast<StorageType>(Permission::NONE);
};

//contains details, including descriptions, examples, and effects for the corresponding opcode
class OpcodeDetails
{
public:

	//combination of example and its expected output
	struct OpcodeExampleOutputPair
	{
		OpcodeExampleOutputPair(std::string e, std::string o) :
			example(std::move(e)), output(std::move(o))
		{}

		std::string example;
		std::string output;
	};

	//arrangements of ordered parameters
	enum class OrderedChildNodeType
	{
		UNORDERED,
		ORDERED,
		ONE_POSITION_THEN_ORDERED,
		PAIRED,
		ONE_POSITION_THEN_PAIRED,
		POSITION
	};

	//whether an opcode returns a newly allocated value
	enum class OpcodeReturnNewnessType
	{
		NEW, PARTIAL, CONDITIONAL, EXISTING, NULL_VALUE
	};

	std::string parameters;
	std::string output;
	std::string description;
	std::vector<OpcodeExampleOutputPair> exampleOutputPairs;
	OrderedChildNodeType orderedChildNodeType = OrderedChildNodeType::POSITION;
	ExecutionPermissions::Permission permissions = ExecutionPermissions::Permission::NONE;
	OpcodeReturnNewnessType valueNewness = OpcodeReturnNewnessType::EXISTING;
	bool potentiallyIdempotent = false;
	bool hasSideEffects = false;
	bool allowsConcurrency = false;
	bool requiresEntity = false;
	bool newScope = false;
	bool newTargetScope = false;
	bool isQuery = false;
};

extern const std::array<OpcodeDetails, NUM_ENT_OPCODES> _opcode_details;

//returns the type of structure that the ordered child nodes have for a given t
__forceinline OpcodeDetails::OrderedChildNodeType GetOpcodeOrderedChildNodeType(EvaluableNodeType t)
{
	return _opcode_details[t].orderedChildNodeType;
}

//returns true if the opcode modifies things outside of its return
__forceinline bool DoesOpcodeHaveSideEffects(EvaluableNodeType t)
{
	return _opcode_details[t].hasSideEffects;
}

//returns whether the opcode returns a new value
__forceinline OpcodeDetails::OpcodeReturnNewnessType GetOpcodeNewValueReturnType(EvaluableNodeType t)
{
	return _opcode_details[t].valueNewness;
}

//returns true if the opcode uses an associative array as parameters. If false, then a regular kind of list
__forceinline bool DoesOpcodeUseAssocParameters(EvaluableNodeType t)
{
	return GetOpcodeOrderedChildNodeType(t) == OpcodeDetails::OrderedChildNodeType::PAIRED;
}

//returns true if t is an immediate value
__forceinline constexpr bool IsEvaluableNodeTypeImmediate(EvaluableNodeType t)
{
	return (t == ENT_BOOL || t == ENT_NUMBER || t == ENT_STRING || t == ENT_SYMBOL);
}

//returns true if t uses string data
__forceinline constexpr bool DoesEvaluableNodeTypeUseBoolData(EvaluableNodeType t)
{
	return (t == ENT_BOOL);
}

//returns true if t uses number data
__forceinline constexpr bool DoesEvaluableNodeTypeUseNumberData(EvaluableNodeType t)
{
	return (t == ENT_NUMBER);
}

//returns true if t uses string data
__forceinline bool DoesEvaluableNodeTypeUseStringData(EvaluableNodeType t)
{
	return (t == ENT_STRING || t == ENT_SYMBOL);
}

//returns true if t uses association data
__forceinline constexpr bool DoesEvaluableNodeTypeUseAssocData(EvaluableNodeType t)
{
	return (t == ENT_ASSOC);
}

//returns true if t uses ordered data (doesn't use any other t)
constexpr bool DoesEvaluableNodeTypeUseOrderedData(EvaluableNodeType t)
{
	return (IsEvaluableNodeTypeValid(t) && !IsEvaluableNodeTypeImmediate(t) && !DoesEvaluableNodeTypeUseAssocData(t));
}

//returns true if it is a query
__forceinline bool IsEvaluableNodeTypeQuery(EvaluableNodeType t)
{
	return _opcode_details[t].isQuery;
}

//returns true if t could potentially be idempotent
__forceinline bool IsEvaluableNodeTypePotentiallyIdempotent(EvaluableNodeType t)
{
	return _opcode_details[t].potentiallyIdempotent;
}

//returns the string id representing EvaluableNodeBuiltInStringId t
inline StringInternPool::StringID GetStringIdFromBuiltInStringId(EvaluableNodeBuiltInStringId t)
{
	if(t >= ENBISI_FIRST_DYNAMIC_STRING)
		return string_intern_pool.staticStringsIndexToStringID[ENBISI_NOT_A_STRING];
	return string_intern_pool.staticStringsIndexToStringID[t];
}

//returns the EvaluableNodeType for a given string, ENT_NOT_A_BUILT_IN_TYPE if it isn't one
inline EvaluableNodeBuiltInStringId GetBuiltInStringIdFromStringId(StringInternPool::StringID sid)
{
	auto found = string_intern_pool.staticStringIDToIndex.find(sid);
	if(found == end(string_intern_pool.staticStringIDToIndex))
		return ENBISI_NOT_A_STRING;

	EvaluableNodeBuiltInStringId bisid = static_cast<EvaluableNodeBuiltInStringId>(found->second);
	if(bisid >= ENBISI_FIRST_DYNAMIC_STRING)
		return ENBISI_NOT_A_STRING;

	return bisid;
}

//returns the string id representing EvaluableNodeType t
inline StringInternPool::StringID GetStringIdFromNodeType(EvaluableNodeType t)
{
	if(t >= NUM_VALID_ENT_OPCODES)
		return string_intern_pool.staticStringsIndexToStringID[ENT_NOT_A_BUILT_IN_TYPE];
	return string_intern_pool.staticStringsIndexToStringID[t + NUM_ENBISI_SPECIAL_STRING_IDS];
}

//returns the EvaluableNodeType for a given string, ENT_NOT_A_BUILT_IN_TYPE if it isn't one
inline EvaluableNodeType GetEvaluableNodeTypeFromStringId(StringInternPool::StringID sid)
{
	auto found = string_intern_pool.staticStringIDToIndex.find(sid);
	if(found == end(string_intern_pool.staticStringIDToIndex))
		return ENT_NOT_A_BUILT_IN_TYPE;

	size_t type_index = found->second - NUM_ENBISI_SPECIAL_STRING_IDS;
	if(type_index >= NUM_VALID_ENT_OPCODES)
		return ENT_NOT_A_BUILT_IN_TYPE;

	return static_cast<EvaluableNodeType>(type_index);
}

//returns a string of the type specified
// if get_non_keywords is true, then it will return types that are not necessarily keywords, like number
inline std::string GetStringFromEvaluableNodeType(EvaluableNodeType t, bool get_non_keywords = false)
{
	if(!get_non_keywords && IsEvaluableNodeTypeImmediate(t))
		return "";

	if(t >= NUM_VALID_ENT_OPCODES)
	{
		assert(false);
		return "";
	}

	return string_intern_pool.GetStringFromID(GetStringIdFromNodeType(t));
}

//returns the enumerated type for the string
inline EvaluableNodeType GetEvaluableNodeTypeFromString(const std::string &s)
{
	auto sid = string_intern_pool.GetIDFromString(s);
	if(sid == string_intern_pool.NOT_A_STRING_ID || sid == string_intern_pool.emptyStringId)
		return ENT_NOT_A_BUILT_IN_TYPE;

	return GetEvaluableNodeTypeFromStringId(sid);
}
