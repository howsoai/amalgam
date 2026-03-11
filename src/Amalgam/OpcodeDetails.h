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
	struct OpcodeExampleOutputPair
	{
		OpcodeExampleOutputPair(std::string e, std::string o) :
			example(std::move(e)), output(std::move(o))
		{}

		std::string example;
		std::string output;
	};

	//different arrangements of ordered parameters
	enum class OrderedChildNodeType
	{
		UNORDERED,
		ORDERED,
		ONE_POSITION_THEN_ORDERED,
		PAIRED,
		ONE_POSITION_THEN_PAIRED,
		POSITION
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

//different characterizations of whether opcodes return new values
enum OpcodeNewValueReturnType
{
	ONVRT_NEW_VALUE,
	ONVRT_PARTIALLY_NEW_VALUE,
	ONVRT_CONDITIONALLY_NEW_VALUE,
	ONVRT_EXISTING_VALUE,
	ONVRT_NULL
};

//returns whether the opcode returns a new value
constexpr OpcodeNewValueReturnType GetOpcodeNewValueReturnType(EvaluableNodeType t)
{
	switch(t)
	{
	case ENT_SYSTEM:	case ENT_HELP:	case ENT_GET_DEFAULTS:	case ENT_RECLAIM_RESOURCES:
	case ENT_PARSE:	case ENT_UNPARSE:
	case ENT_SET:	case ENT_REPLACE:	case ENT_OPCODE_STACK:	case ENT_STACK:
	case ENT_GET_RAND_SEED:
	case ENT_SYSTEM_TIME:
	case ENT_ADD:	case ENT_SUBTRACT:	case ENT_MULTIPLY:	case ENT_DIVIDE:
	case ENT_MODULUS:	case ENT_GET_DIGITS:	case ENT_SET_DIGITS:	case ENT_FLOOR:
	case ENT_CEILING:	case ENT_ROUND:	case ENT_EXPONENT:	case ENT_LOG:	case ENT_SIN:
	case ENT_ASIN:	case ENT_COS:	case ENT_ACOS:	case ENT_TAN:	case ENT_ATAN:
	case ENT_SINH:	case ENT_ASINH:	case ENT_COSH:	case ENT_ACOSH:	case ENT_TANH:
	case ENT_ATANH:	case ENT_ERF:	case ENT_TGAMMA:	case ENT_LGAMMA:	case ENT_SQRT:
	case ENT_POW:	case ENT_ABS:	case ENT_MAX:	case ENT_INDEX_MAX: case ENT_INDEX_MIN: case ENT_MIN: case ENT_DOT_PRODUCT:
	case ENT_NORMALIZE: case ENT_QUANTILE:	case ENT_GENERALIZED_MEAN:	case ENT_GENERALIZED_DISTANCE:	case ENT_ENTROPY:
	case ENT_SIZE:
	case ENT_REWRITE:
	case ENT_INDICES:
	case ENT_CONTAINS_INDEX:
	case ENT_CONTAINS_VALUE:
	case ENT_XOR:	case ENT_NOT:
	case ENT_EQUAL:	case ENT_NEQUAL:	case ENT_LESS:	case ENT_LEQUAL:
	case ENT_GREATER:	case ENT_GEQUAL:	case ENT_TYPE_EQUALS:
	case ENT_TYPE_NEQUALS:	case ENT_NULL:	case ENT_BOOL:
	case ENT_NUMBER:	case ENT_STRING:
	case ENT_GET_TYPE:	case ENT_GET_TYPE_STRING:
	case ENT_FORMAT:
	case ENT_GET_ANNOTATIONS:
	case ENT_GET_COMMENTS:
	case ENT_GET_CONCURRENCY:
	case ENT_GET_VALUE:
	case ENT_EXPLODE:	case ENT_SPLIT:	case ENT_SUBSTR:	case ENT_CONCAT:
	case ENT_CRYPTO_SIGN:	case ENT_CRYPTO_SIGN_VERIFY:	case ENT_ENCRYPT:
	case ENT_DECRYPT:
	case ENT_TOTAL_SIZE:	case ENT_MUTATE:	case ENT_COMMONALITY:
	case ENT_EDIT_DISTANCE:	case ENT_INTERSECT:	case ENT_UNION:	case ENT_DIFFERENCE:
	case ENT_MIX:			case ENT_TOTAL_ENTITY_SIZE:
	case ENT_FLATTEN_ENTITY:	case ENT_MUTATE_ENTITY:	case ENT_COMMONALITY_ENTITIES:
	case ENT_EDIT_DISTANCE_ENTITIES:	case ENT_INTERSECT_ENTITIES:
	case ENT_UNION_ENTITIES:	case ENT_DIFFERENCE_ENTITIES:	case ENT_MIX_ENTITIES:
	case ENT_GET_ENTITY_ANNOTATIONS:	case ENT_GET_ENTITY_COMMENTS:	case ENT_RETRIEVE_ENTITY_ROOT:
	case ENT_ASSIGN_ENTITY_ROOTS:
	case ENT_GET_ENTITY_RAND_SEED:	case ENT_SET_ENTITY_RAND_SEED:
	case ENT_GET_ENTITY_PERMISSIONS:	case ENT_SET_ENTITY_PERMISSIONS:
	case ENT_CREATE_ENTITIES:	case ENT_CLONE_ENTITIES:	case ENT_MOVE_ENTITIES:
	case ENT_DESTROY_ENTITIES:	case ENT_LOAD:	case ENT_LOAD_ENTITY:	case ENT_STORE:
	case ENT_STORE_ENTITY:	case ENT_CONTAINS_ENTITY:
	case ENT_CONTAINS_LABEL:	case ENT_ASSIGN_TO_ENTITIES:
	case ENT_REMOVE_FROM_ENTITIES:	case ENT_ACCUM_TO_ENTITIES:
	case ENT_CALL_CONTAINER:
		return ONVRT_NEW_VALUE;

	case ENT_APPEND:
	case ENT_MAP:	case ENT_FILTER:	case ENT_WEAVE:
	case ENT_REVERSE:	case ENT_SORT:
	case ENT_VALUES:
	case ENT_REMOVE:	case ENT_KEEP:				case ENT_ASSOCIATE:	case ENT_ZIP:	case ENT_UNZIP:
	case ENT_LIST:		case ENT_UNORDERED_LIST:	case ENT_ASSOC:
	case ENT_SET_TYPE:
	case ENT_SET_ANNOTATIONS:
	case ENT_SET_COMMENTS:
	case ENT_SET_CONCURRENCY:
	case ENT_SET_VALUE:
	case ENT_CONTAINED_ENTITIES:	case ENT_COMPUTE_ON_CONTAINED_ENTITIES:
	case ENT_QUERY_SELECT:	case ENT_QUERY_SAMPLE:	case ENT_QUERY_IN_ENTITY_LIST:
	case ENT_QUERY_NOT_IN_ENTITY_LIST:	case ENT_QUERY_EXISTS:	case ENT_QUERY_NOT_EXISTS:
	case ENT_QUERY_EQUALS:	case ENT_QUERY_NOT_EQUALS:	case ENT_QUERY_BETWEEN:
	case ENT_QUERY_NOT_BETWEEN:	case ENT_QUERY_AMONG:	case ENT_QUERY_NOT_AMONG:
	case ENT_QUERY_MAX:	case ENT_QUERY_MIN:	case ENT_QUERY_SUM:	case ENT_QUERY_MODE:
	case ENT_QUERY_QUANTILE:						case ENT_QUERY_GENERALIZED_MEAN:
	case ENT_QUERY_MIN_DIFFERENCE:					case ENT_QUERY_MAX_DIFFERENCE:
	case ENT_QUERY_VALUE_MASSES:					case ENT_QUERY_GREATER_OR_EQUAL_TO:
	case ENT_QUERY_LESS_OR_EQUAL_TO:				case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
	case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:	case ENT_QUERY_DISTANCE_CONTRIBUTIONS:
	case ENT_QUERY_ENTITY_CONVICTIONS:				case ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE:
	case ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS:	case ENT_QUERY_ENTITY_KL_DIVERGENCES:
	case ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS:
		return ONVRT_PARTIALLY_NEW_VALUE;

	case ENT_RAND:
	case ENT_FIRST:	case ENT_TAIL:	case ENT_LAST:	case ENT_TRUNC:
	case ENT_RANGE:
	case ENT_APPLY:
	case ENT_AND:	case ENT_OR:
	case ENT_RETRIEVE_FROM_ENTITY:
	case ENT_CALL_ENTITY:	case ENT_CALL_ENTITY_GET_CHANGES:	case ENT_CALL_ON_ENTITY:
		return ONVRT_CONDITIONALLY_NEW_VALUE;


	case ENT_IF:	case ENT_SEQUENCE:	case ENT_LAMBDA:	case ENT_CONCLUDE:	case ENT_RETURN:
	case ENT_CALL:	case ENT_CALL_SANDBOXED:	case ENT_WHILE:	case ENT_LET:	case ENT_DECLARE:
	case ENT_RETRIEVE:	case ENT_GET:
	case ENT_TARGET:	case ENT_CURRENT_INDEX:	case ENT_CURRENT_VALUE:	case ENT_PREVIOUS_RESULT:
	case ENT_ARGS:
	case ENT_SET_RAND_SEED:
	case ENT_REDUCE:
	case ENT_SYMBOL:
		return ONVRT_EXISTING_VALUE;

	case ENT_ASSIGN:	case ENT_ACCUM:
	case ENT_PRINT:
		return ONVRT_NULL;

	default:
		return ONVRT_EXISTING_VALUE;
	}
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
