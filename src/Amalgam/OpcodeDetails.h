#pragma once

//project headers:
#include "BitmaskEnum.h"
#include "FastMath.h"
#include "Opcodes.h"
#include "StringInternPool.h"
#include "UninitializedArray.h"

//system headers:
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

//forward declarations
class EvaluableNode;
class EvaluableNodeManager;
class ExecutionPermissions
{
public:
	using PermissionType = uint8_t;
	enum class Permission : PermissionType
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
		: allPermissions(static_cast<PermissionType>(initial_permissions))
	{}

	inline bool HasPermission(Permission permission) const
	{
		return (allPermissions & static_cast<PermissionType>(permission)) != 0;
	}

	inline void SetPermission(Permission permission, bool enable = true)
	{
		if(enable)
			allPermissions |= static_cast<PermissionType>(permission);
		else
			allPermissions &= ~static_cast<PermissionType>(permission);
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
	PermissionType allPermissions = static_cast<PermissionType>(Permission::NONE);
};

class Entity;

//combination of example and its expected output
class AmalgamExample
{
public:
	AmalgamExample(std::string_view e, std::string_view o,
		std::string_view r = std::string_view(), std::string_view c = std::string_view())
		: example(e), output(o),
		regexMatch(r), cleanup(c)
	{}

	//runs the example on entity, returns true on success, false on failure
	//any errors will be written to stderr
	bool ValidateExample(Entity *entity);

	//the code example
	std::string_view example;
	//example output
	std::string_view output;

	//if regexMatch is anything other than the empty string,
	//it will verify the example's output based on the regexMatch
	//if regexMatch is empty string, then it will compare example's output
	//to output except for amount of white space
	std::string_view regexMatch;

	//if there is code in cleanup, it will run that after example has been run
	//and verified
	std::string_view cleanup;
};

//helper that builds a vector of AmalgamExample from a list of example and output pairs supplied by the generator
inline std::vector<AmalgamExample> MakeAmalgamExamples(
		std::initializer_list<AmalgamExample> list)
{
	std::vector<AmalgamExample> out;
	out.reserve(list.size());

	for(const auto &e : list)
		out.emplace_back(e.example, e.output, e.regexMatch, e.cleanup);
	return out;
}

//contains details, including descriptions, examples, and effects for the corresponding opcode
class OpcodeDetails
{
public:
	//arrangements of ordered parameters
	enum class OrderedChildNodeType
	{
		NONE,
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

	using OpcodeDataTypeType = uint16_t;
	enum class OpcodeDataType : OpcodeDataTypeType
	{
		NULL_TYPE = 1 << 0,
		BOOL = 1 << 1,
		NUMBER = 1 << 2,
		BARE_STRING = 1 << 3,
		STRING = 1 << 4,
		LIST = 1 << 5,
		UNORDERED_LIST = 1 << 6,
		ASSOC = 1 << 7,
		WALK_PATH = 1 << 8,
		ENTITY_ID = 1 << 9,
		QUERY = 1 << 10,
		ANY_BASIC = NULL_TYPE | BOOL | NUMBER | STRING | LIST | ASSOC
	};

	//attribute ordering here is generally ordered by operational use to improve caching,
	//with descriptive strings at the end

	//true if the opcode may return itself if all child nodes are also idempotent
	bool potentiallyIdempotent = false;

	//true if the opcode may retrieve data from outside and require execution
	bool retrievesData = false;

	//true if the opcode may affect data outside itself
	bool hasSideEffects = false;

	//true if the opcode may cause a change in the current entity
	bool mayCauseNodeUpdateInCurrentEntity = false;

	//true if the opcode allows concurrent execution
	bool allowsConcurrency = false;

	//true if the opcode requires an entity to operate
	bool requiresEntity = false;

	//true if the opcode creates a new variable scope
	bool newScope = false;

	//true if the opcode creates a new target scope
	bool newTargetScope = false;

	//true if the opcode is a query run by the query engine
	bool isQuery = false;

	//if the opcode has ordered child nodes, how they're ordered
	OrderedChildNodeType orderedChildNodeType = OrderedChildNodeType::POSITION;

	//what kind of special permissions the opcode needs to run
	ExecutionPermissions::Permission permissions = ExecutionPermissions::Permission::NONE;

	//whether the opcode returns a newly allocated value
	OpcodeReturnNewnessType valueNewness = OpcodeReturnNewnessType::EXISTING;

	std::string_view parameters;
	OpcodeDataType returns;
	std::string_view description;
	std::vector<AmalgamExample> examples;
	double frequencyPer10000Opcodes = 1.0;
	std::string_view opcodeGroup;
};

template<> struct IsBitmaskEnum<OpcodeDetails::OpcodeDataType> : std::true_type
{
};

//details for every opcode, indexed by EvaluableNodeType
//stored as an UninitializedArray to prevent initialization order from clobbering
//the data being assigned
extern UninitializedArray<OpcodeDetails, NUM_ENT_OPCODES> _opcode_details;

//forward declaration
template<typename OpcodeFunction>
void SetInterpreterOpcodeFunction(EvaluableNodeType type, OpcodeFunction func);

//no-storage class to initialize storage for opcodes such that all relevant
//code and data for an opcode can be kept in the same location
class OpcodeInitializer
{
public:

	template<typename OpcodeFunction, typename OpcodeDetailsBuilder>
	OpcodeInitializer(EvaluableNodeType type, OpcodeFunction func, OpcodeDetailsBuilder details_builder)
	{
		SetInterpreterOpcodeFunction(type, func);
		//construct from a move due to the more complex data structures with heap data, e.g., std::vector
		new (&_opcode_details[static_cast<size_t>(type)]) OpcodeDetails(std::move(details_builder()));
	}
};

//returns the type of structure that the ordered child nodes have for a given t
__forceinline OpcodeDetails::OrderedChildNodeType GetOpcodeOrderedChildNodeType(EvaluableNodeType t)
{
	return _opcode_details[t].orderedChildNodeType;
}

//returns true if the opcode may retrieve data
__forceinline bool DoesOpcodeRetrieveData(EvaluableNodeType t)
{
	return _opcode_details[t].retrievesData;
}

//returns true if the opcode modifies things outside of its return
__forceinline bool DoesOpcodeHaveSideEffects(EvaluableNodeType t)
{
	return _opcode_details[t].hasSideEffects;
}

//returns true if the opcode modifies things outside of its return
__forceinline bool MayOpcodeCauseNodeUpdateInCurrentEntity(EvaluableNodeType t)
{
	return _opcode_details[t].mayCauseNodeUpdateInCurrentEntity;
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
	return (t == ENT_NULL || t == ENT_BOOL || t == ENT_NUMBER || t == ENT_STRING || t == ENT_SYMBOL);
}

//returns true if t uses null (no) data
__forceinline constexpr bool DoesEvaluableNodeTypeUseNullData(EvaluableNodeType t)
{
	return (t == ENT_NULL);
}

//returns true if t uses boolean data
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

//returns true if t is a query
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

	EvaluableNodeBuiltInStringId builtin_sid = static_cast<EvaluableNodeBuiltInStringId>(found->second);
	if(builtin_sid >= ENBISI_FIRST_DYNAMIC_STRING)
		return ENBISI_NOT_A_STRING;

	return builtin_sid;
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
		AmlgAssert(false);
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
