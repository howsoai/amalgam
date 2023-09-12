#pragma once

//project headers:
#include "StringInternPool.h"

//opcodes / commands / operations in Amalgam
enum EvaluableNodeType : uint8_t
{
	//built-in / system specific
	ENT_SYSTEM,
	ENT_GET_DEFAULTS,

	//parsing
	ENT_PARSE,
	ENT_UNPARSE,

	//core control
	ENT_IF,
	ENT_SEQUENCE,
	ENT_PARALLEL,
	ENT_LAMBDA,
	ENT_CONCLUDE,
	ENT_CALL,
	ENT_CALL_SANDBOXED,
	ENT_WHILE,

	//definitions
	ENT_LET,
	ENT_DECLARE,
	ENT_ASSIGN,
	ENT_ACCUM,

	//retrieval
	ENT_RETRIEVE,
	ENT_GET,
	ENT_SET,
	ENT_REPLACE,

	//stack and node manipulation
	ENT_TARGET,
	ENT_TARGET_INDEX,
	ENT_TARGET_VALUE,
	ENT_STACK,
	ENT_ARGS,

	//simulation and operations
	ENT_RAND,
	ENT_WEIGHTED_RAND,
	ENT_GET_RAND_SEED,
	ENT_SET_RAND_SEED,
	ENT_SYSTEM_TIME,

	//base math
	ENT_ADD,
	ENT_SUBTRACT,
	ENT_MULTIPLY,
	ENT_DIVIDE,
	ENT_MODULUS,
	ENT_GET_DIGITS,
	ENT_SET_DIGITS,
	ENT_FLOOR,
	ENT_CEILING,
	ENT_ROUND,

	//extended math
	ENT_EXPONENT,
	ENT_LOG,

	ENT_SIN,
	ENT_ASIN,
	ENT_COS,
	ENT_ACOS,
	ENT_TAN,
	ENT_ATAN,

	ENT_SINH,
	ENT_ASINH,
	ENT_COSH,
	ENT_ACOSH,
	ENT_TANH,
	ENT_ATANH,

	ENT_ERF,
	ENT_TGAMMA,
	ENT_LGAMMA,

	ENT_SQRT,
	ENT_POW,
	ENT_ABS,
	ENT_MAX,
	ENT_MIN,
	ENT_DOT_PRODUCT,
	ENT_GENERALIZED_DISTANCE,
	ENT_ENTROPY,

	//list manipulation
	ENT_FIRST,
	ENT_TAIL,
	ENT_LAST,
	ENT_TRUNC,
	ENT_APPEND,
	ENT_SIZE,
	ENT_RANGE,

	//transformation
	ENT_REWRITE,
	ENT_MAP,
	ENT_FILTER,
	ENT_WEAVE,
	ENT_REDUCE,
	ENT_APPLY,
	ENT_REVERSE,
	ENT_SORT,

	//associative list manipulation
	ENT_INDICES,
	ENT_VALUES,
	ENT_CONTAINS_INDEX,
	ENT_CONTAINS_VALUE,
	ENT_REMOVE,
	ENT_KEEP,
	ENT_ASSOCIATE,
	ENT_ZIP,
	ENT_UNZIP,

	//logic
	ENT_AND,
	ENT_OR,
	ENT_XOR,
	ENT_NOT,

	//equivalence
	ENT_EQUAL,
	ENT_NEQUAL,
	ENT_LESS,
	ENT_LEQUAL,
	ENT_GREATER,
	ENT_GEQUAL,
	ENT_TYPE_EQUALS,
	ENT_TYPE_NEQUALS,

	//built-in constants and variables
	ENT_TRUE,
	ENT_FALSE,
	ENT_NULL,

	//data types
	ENT_LIST,
	ENT_ASSOC,
	ENT_NUMBER,
	ENT_STRING,
	ENT_SYMBOL,

	//node types
	ENT_GET_TYPE,
	ENT_GET_TYPE_STRING,
	ENT_SET_TYPE,
	ENT_FORMAT,

	//EvaluableNode management: labels, comments, and concurrency
	ENT_GET_LABELS,
	ENT_GET_ALL_LABELS,
	ENT_SET_LABELS,
	ENT_ZIP_LABELS,

	ENT_GET_COMMENTS,
	ENT_SET_COMMENTS,

	ENT_GET_CONCURRENCY,
	ENT_SET_CONCURRENCY,

	ENT_GET_VALUE,
	ENT_SET_VALUE,

	//string
	ENT_EXPLODE,
	ENT_SPLIT,
	ENT_SUBSTR,
	ENT_CONCAT,

	//encryption
	ENT_CRYPTO_SIGN,
	ENT_CRYPTO_SIGN_VERIFY,
	ENT_ENCRYPT,
	ENT_DECRYPT,

	//I/O
	ENT_PRINT,

	//tree merging
	ENT_TOTAL_SIZE,
	ENT_MUTATE,
	ENT_COMMONALITY,
	ENT_EDIT_DISTANCE,
	ENT_INTERSECT,
	ENT_UNION,
	ENT_DIFFERENCE,
	ENT_MIX,
	ENT_MIX_LABELS,

	//entity merging
	ENT_TOTAL_ENTITY_SIZE,
	ENT_FLATTEN_ENTITY,
	ENT_MUTATE_ENTITY,
	ENT_COMMONALITY_ENTITIES,
	ENT_EDIT_DISTANCE_ENTITIES,
	ENT_INTERSECT_ENTITIES,
	ENT_UNION_ENTITIES,
	ENT_DIFFERENCE_ENTITIES,
	ENT_MIX_ENTITIES,

	//entity details
	ENT_GET_ENTITY_COMMENTS,
	ENT_RETRIEVE_ENTITY_ROOT,
	ENT_ASSIGN_ENTITY_ROOTS,
	ENT_ACCUM_ENTITY_ROOTS,
	ENT_GET_ENTITY_RAND_SEED,
	ENT_SET_ENTITY_RAND_SEED,
	ENT_GET_ENTITY_ROOT_PERMISSION,
	ENT_SET_ENTITY_ROOT_PERMISSION,

	//entity base actions
	ENT_CREATE_ENTITIES,
	ENT_CLONE_ENTITIES,
	ENT_MOVE_ENTITIES,
	ENT_DESTROY_ENTITIES,
	ENT_LOAD,
	ENT_LOAD_ENTITY,
	ENT_LOAD_PERSISTENT_ENTITY,
	ENT_STORE,
	ENT_STORE_ENTITY,
	ENT_CONTAINS_ENTITY,

	//entity query
	ENT_CONTAINED_ENTITIES,
	ENT_COMPUTE_ON_CONTAINED_ENTITIES,
	ENT_QUERY_SELECT,
	ENT_QUERY_SAMPLE,
	ENT_QUERY_WEIGHTED_SAMPLE,
	ENT_QUERY_IN_ENTITY_LIST,
	ENT_QUERY_NOT_IN_ENTITY_LIST,
	ENT_QUERY_COUNT,
	ENT_QUERY_EXISTS,
	ENT_QUERY_NOT_EXISTS,
	ENT_QUERY_EQUALS,
	ENT_QUERY_NOT_EQUALS,
	ENT_QUERY_BETWEEN,
	ENT_QUERY_NOT_BETWEEN,
	ENT_QUERY_AMONG,
	ENT_QUERY_NOT_AMONG,
	ENT_QUERY_MAX,
	ENT_QUERY_MIN,
	ENT_QUERY_SUM,
	ENT_QUERY_MODE,
	ENT_QUERY_QUANTILE,
	ENT_QUERY_GENERALIZED_MEAN,
	ENT_QUERY_MIN_DIFFERENCE,
	ENT_QUERY_MAX_DIFFERENCE,
	ENT_QUERY_VALUE_MASSES,
	ENT_QUERY_GREATER_OR_EQUAL_TO,
	ENT_QUERY_LESS_OR_EQUAL_TO,
	ENT_QUERY_WITHIN_GENERALIZED_DISTANCE,
	ENT_QUERY_NEAREST_GENERALIZED_DISTANCE,

	//aggregate analysis entity query
	ENT_COMPUTE_ENTITY_CONVICTIONS,
	ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE,
	ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS,
	ENT_COMPUTE_ENTITY_KL_DIVERGENCES,

	//entity access
	ENT_CONTAINS_LABEL,
	ENT_ASSIGN_TO_ENTITIES,
	ENT_DIRECT_ASSIGN_TO_ENTITIES,
	ENT_ACCUM_TO_ENTITIES,
	ENT_RETRIEVE_FROM_ENTITY,
	ENT_DIRECT_RETRIEVE_FROM_ENTITY,
	ENT_CALL_ENTITY,
	ENT_CALL_ENTITY_GET_CHANGES,
	ENT_CALL_CONTAINER,

	//not in active memory
	//freed and no longer in use
	ENT_DEALLOCATED,
	//allocated, but not in use yet
	ENT_UNINITIALIZED,

	//something went wrong - maximum value
	ENT_NOT_A_BUILT_IN_TYPE,
};

//total number of opcodes
constexpr size_t NUM_ENT_OPCODES = ENT_NOT_A_BUILT_IN_TYPE;
//total number of valid opcodes
constexpr size_t NUM_VALID_ENT_OPCODES = ENT_DEALLOCATED;


//Different arrangements of ordered parameters
enum OrderedChildNodeType
{
	OCNT_UNORDERED,
	OCNT_ORDERED,
	OCNT_ONE_POSITION_THEN_ORDERED,
	OCNT_PAIRED,
	OCNT_ONE_POSITION_THEN_PAIRED,
	OCNT_POSITION
};

//Returns the type of structure that the ordered child nodes have for a given type
constexpr OrderedChildNodeType GetInstructionOrderedChildNodeType(EvaluableNodeType t)
{
	switch(t)
	{
	case ENT_PARALLEL:
	case ENT_ADD:
	case ENT_MULTIPLY:
	case ENT_MAX:					case ENT_MIN:
	case ENT_AND:					case ENT_OR:				case ENT_XOR:
	case ENT_EQUAL:					case ENT_NEQUAL:
	case ENT_NULL:
	case ENT_DESTROY_ENTITIES:
		return OCNT_UNORDERED;

	case ENT_SYSTEM:
	case ENT_GET_DEFAULTS:
	case ENT_SEQUENCE:
	case ENT_APPEND:				case ENT_FILTER:			case ENT_SORT:
	case ENT_ZIP:					case ENT_UNZIP:
	case ENT_LESS:					case ENT_LEQUAL:
	case ENT_GREATER:				case ENT_GEQUAL:			case ENT_TYPE_EQUALS:		case ENT_TYPE_NEQUALS:
	case ENT_TRUE:					case ENT_FALSE:
	case ENT_LIST:
	case ENT_CONCAT:
	case ENT_PRINT:
	case ENT_ASSIGN_ENTITY_ROOTS:	case ENT_ACCUM_ENTITY_ROOTS:
	case ENT_SET_ENTITY_RAND_SEED:
	case ENT_CREATE_ENTITIES:
	case ENT_CONTAINED_ENTITIES:	case ENT_COMPUTE_ON_CONTAINED_ENTITIES:
	case ENT_QUERY_SELECT:			case ENT_QUERY_SAMPLE:									case ENT_QUERY_WEIGHTED_SAMPLE:
	case ENT_QUERY_IN_ENTITY_LIST:	case ENT_QUERY_NOT_IN_ENTITY_LIST:						case ENT_QUERY_COUNT:
	case ENT_QUERY_EXISTS:			case ENT_QUERY_NOT_EXISTS:
	case ENT_QUERY_EQUALS:			case ENT_QUERY_NOT_EQUALS:
	case ENT_QUERY_BETWEEN:			case ENT_QUERY_NOT_BETWEEN:
	case ENT_QUERY_AMONG:			case ENT_QUERY_NOT_AMONG:
	case ENT_QUERY_MAX:				case ENT_QUERY_MIN:
	case ENT_QUERY_SUM:				case ENT_QUERY_MODE:
	case ENT_QUERY_QUANTILE:		case ENT_QUERY_GENERALIZED_MEAN:
	case ENT_QUERY_MIN_DIFFERENCE:	case ENT_QUERY_MAX_DIFFERENCE:
	case ENT_QUERY_VALUE_MASSES:
	case ENT_QUERY_GREATER_OR_EQUAL_TO:				case ENT_QUERY_LESS_OR_EQUAL_TO:
	case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:		case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
	case ENT_COMPUTE_ENTITY_CONVICTIONS:			case ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE:
	case ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS:	case ENT_COMPUTE_ENTITY_KL_DIVERGENCES:
	case ENT_CONTAINS_LABEL:		case ENT_ASSIGN_TO_ENTITIES:							case ENT_DIRECT_ASSIGN_TO_ENTITIES:
	case ENT_ACCUM_TO_ENTITIES:		case ENT_RETRIEVE_FROM_ENTITY:							case ENT_DIRECT_RETRIEVE_FROM_ENTITY:
	case ENT_CALL_ENTITY:			case ENT_CALL_ENTITY_GET_CHANGES:						case ENT_CALL_CONTAINER:
		return OCNT_ORDERED;

	case ENT_WHILE:					case ENT_LET:				case ENT_DECLARE:			case ENT_SUBTRACT:
	case ENT_DIVIDE:				case ENT_MODULUS:
		return OCNT_ONE_POSITION_THEN_ORDERED;

	case ENT_ASSOC:		case ENT_ASSOCIATE:
		return OCNT_PAIRED;

	case ENT_ASSIGN:				case ENT_ACCUM:
	case ENT_SET:					case ENT_REPLACE:
		return OCNT_ONE_POSITION_THEN_PAIRED;

	case ENT_PARSE:						case ENT_UNPARSE:			case ENT_IF:				case ENT_LAMBDA:
	case ENT_CONCLUDE:
	case ENT_CALL:						case ENT_CALL_SANDBOXED:
	case ENT_RETRIEVE:
	case ENT_GET:
	case ENT_TARGET:					case ENT_TARGET_INDEX:		case ENT_TARGET_VALUE:
	case ENT_STACK:						case ENT_ARGS:
	case ENT_RAND:						case ENT_WEIGHTED_RAND:		case ENT_GET_RAND_SEED:		case ENT_SET_RAND_SEED:
	case ENT_SYSTEM_TIME:
	case ENT_GET_DIGITS:				case ENT_SET_DIGITS:
	case ENT_FLOOR:						case ENT_CEILING:			case ENT_ROUND:
	case ENT_SIN:						case ENT_ASIN:				case ENT_COS:				case ENT_ACOS:
	case ENT_EXPONENT:					case ENT_LOG:				case ENT_TAN:
	case ENT_ATAN:
	case ENT_SINH:						case ENT_ASINH:				case ENT_COSH:				case ENT_ACOSH:
	case ENT_TANH:						case ENT_ATANH:
	case ENT_ERF:						case ENT_TGAMMA:			case ENT_LGAMMA:
	case ENT_SQRT:						case ENT_POW:				case ENT_ABS:
	case ENT_DOT_PRODUCT:				case ENT_GENERALIZED_DISTANCE:	case ENT_ENTROPY:
	case ENT_FIRST:						case ENT_TAIL:				case ENT_LAST:				case ENT_TRUNC:
	case ENT_SIZE:						case ENT_RANGE:
	case ENT_REWRITE:					case ENT_MAP:				case ENT_WEAVE:
	case ENT_REDUCE:					case ENT_APPLY:				case ENT_REVERSE:
	case ENT_INDICES:
	case ENT_VALUES:					case ENT_CONTAINS_INDEX:	case ENT_CONTAINS_VALUE:
	case ENT_REMOVE:					case ENT_KEEP:
	case ENT_NOT:
	case ENT_NUMBER:					case ENT_STRING:
	case ENT_SYMBOL:
	case ENT_GET_TYPE:					case ENT_GET_TYPE_STRING:	case ENT_SET_TYPE:			case ENT_FORMAT:
	case ENT_GET_LABELS:				case ENT_GET_ALL_LABELS:	case ENT_SET_LABELS:		case ENT_ZIP_LABELS:
	case ENT_GET_COMMENTS:				case ENT_SET_COMMENTS:
	case ENT_GET_CONCURRENCY:			case ENT_SET_CONCURRENCY:
	case ENT_GET_VALUE:					case ENT_SET_VALUE:
	case ENT_EXPLODE:					case ENT_SPLIT:				case ENT_SUBSTR:
	case ENT_CRYPTO_SIGN:				case ENT_CRYPTO_SIGN_VERIFY:
	case ENT_ENCRYPT:					case ENT_DECRYPT:
	case ENT_TOTAL_SIZE:				case ENT_COMMONALITY:		case ENT_EDIT_DISTANCE:		case ENT_MUTATE:
	case ENT_INTERSECT:					case ENT_UNION:				case ENT_DIFFERENCE:
	case ENT_MIX:						case ENT_MIX_LABELS:
	case ENT_TOTAL_ENTITY_SIZE:			case ENT_FLATTEN_ENTITY:	case ENT_MUTATE_ENTITY:
	case ENT_COMMONALITY_ENTITIES:
	case ENT_INTERSECT_ENTITIES:		case ENT_UNION_ENTITIES:	case ENT_DIFFERENCE_ENTITIES:
	case ENT_MIX_ENTITIES:
	case ENT_GET_ENTITY_COMMENTS:
	case ENT_RETRIEVE_ENTITY_ROOT:
	case ENT_GET_ENTITY_RAND_SEED:
	case ENT_GET_ENTITY_ROOT_PERMISSION:								case ENT_SET_ENTITY_ROOT_PERMISSION:
	case ENT_CLONE_ENTITIES:			case ENT_MOVE_ENTITIES:
	case ENT_LOAD:						case ENT_LOAD_ENTITY:			case ENT_LOAD_PERSISTENT_ENTITY:
	case ENT_STORE_ENTITY:				case ENT_STORE:
	case ENT_CONTAINS_ENTITY:
		return OCNT_POSITION;

	default:
		return OCNT_POSITION;
	}
}

//Returns true if the instruction uses an associative array as parameters. If false, then a regular kind of list
constexpr bool DoesInstructionUseAssocParameters(EvaluableNodeType t)
{
	return GetInstructionOrderedChildNodeType(t) == OCNT_PAIRED;
}

//Returns true if the type is an immediate value
constexpr bool IsEvaluableNodeTypeImmediate(EvaluableNodeType t)
{
	return (t == ENT_NUMBER || t == ENT_STRING || t == ENT_SYMBOL);
}

//Returns true if the type uses string data
constexpr bool DoesEvaluableNodeTypeUseStringData(EvaluableNodeType t)
{
	return (t == ENT_STRING || t == ENT_SYMBOL);
}

//Returns true if the type uses number data
constexpr bool DoesEvaluableNodeTypeUseNumberData(EvaluableNodeType t)
{
	return (t == ENT_NUMBER);
}

//Returns true if the type uses association data
constexpr bool DoesEvaluableNodeTypeUseAssocData(EvaluableNodeType t)
{
	return (t == ENT_ASSOC);
}

//Returns true if the type uses ordered data (doesn't use any other type)
constexpr bool DoesEvaluableNodeTypeUseOrderedData(EvaluableNodeType t)
{
	return (!IsEvaluableNodeTypeImmediate(t) && !DoesEvaluableNodeTypeUseAssocData(t));
}

//returns true if the type is a query
constexpr bool IsEvaluableNodeTypeQuery(EvaluableNodeType type)
{
	return (type == ENT_QUERY_SELECT || type == ENT_QUERY_IN_ENTITY_LIST || type == ENT_QUERY_NOT_IN_ENTITY_LIST || type == ENT_QUERY_COUNT
		|| type == ENT_QUERY_SAMPLE || type == ENT_QUERY_WEIGHTED_SAMPLE || type == ENT_QUERY_EXISTS || type == ENT_QUERY_NOT_EXISTS
		|| type == ENT_QUERY_EQUALS || type == ENT_QUERY_NOT_EQUALS
		|| type == ENT_QUERY_BETWEEN || type == ENT_QUERY_NOT_BETWEEN || type == ENT_QUERY_AMONG || type == ENT_QUERY_NOT_AMONG
		|| type == ENT_QUERY_MAX || type == ENT_QUERY_MIN || type == ENT_QUERY_SUM || type == ENT_QUERY_MODE
		|| type == ENT_QUERY_QUANTILE || type == ENT_QUERY_GENERALIZED_MEAN
		|| type == ENT_QUERY_MIN_DIFFERENCE || type == ENT_QUERY_MAX_DIFFERENCE || type == ENT_QUERY_VALUE_MASSES
		|| type == ENT_QUERY_LESS_OR_EQUAL_TO || type == ENT_QUERY_GREATER_OR_EQUAL_TO
		|| type == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE || type == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
		|| type == ENT_COMPUTE_ENTITY_CONVICTIONS || type == ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE
		|| type == ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS || type == ENT_COMPUTE_ENTITY_KL_DIVERGENCES
		);
}

//returns true if the type could potentially be idempotent
constexpr bool IsEvaluableNodeTypePotentiallyIdempotent(EvaluableNodeType type)
{
	return (type == ENT_NUMBER || type == ENT_STRING
		|| type == ENT_TRUE || type == ENT_FALSE
		|| type == ENT_NULL || type == ENT_LIST || type == ENT_ASSOC
		|| IsEvaluableNodeTypeQuery(type));
}

constexpr bool IsEvaluableNodeTypeValid(EvaluableNodeType t)
{
	return (t < NUM_VALID_ENT_OPCODES);
}

//covers ENBISI_NOT_A_STRING and ENBISI_EMPTY_STRING
constexpr size_t NUM_ENBISI_SPECIAL_STRING_IDS = 2;

//ids of built-in strings
enum EvaluableNodeBuiltInStringId
{
	ENBISI_NOT_A_STRING = 0,
	ENBISI_EMPTY_STRING = 1,

	//leave space for ENT_ opcodes, start at the end

	//built-in common values
	ENBISI_nas = NUM_VALID_ENT_OPCODES + NUM_ENBISI_SPECIAL_STRING_IDS,
	ENBISI_nan,
	ENBISI_infinity,
	ENBISI_neg_infinity,
	ENBISI_zero,
	ENBISI_one,
	ENBISI_neg_one,
	ENBISI_empty_null,
	ENBISI_empty_list,
	ENBISI_empty_assoc,
	ENBISI_empty_true,
	ENBISI_empty_false,

	//config file parameters
	ENBISI_rand_seed,

	//substr parameters
	ENBISI_all,
	ENBISI_submatches,

	//dynamically generated function parameters
	ENBISI__,
	ENBISI_new_entity,

	//entity access parameters
	ENBISI_accessing_entity,

	//distance types
	ENBISI_nominal,
	ENBISI_continuous,
	ENBISI_cyclic,
	//ENBISI_string, //string is already covered
	ENBISI_code,

	//distance parameter values
	ENBISI_surprisal_to_prob,

	//numerical precision types
	ENBISI_precise,
	ENBISI_fast,
	ENBISI_recompute_precise,

	//format opcode types
	ENBISI_Base16,
	ENBISI_Base64,
	ENBISI_int8,
	ENBISI_uint8,
	ENBISI_int16,
	ENBISI_uint16,
	ENBISI_int32,
	ENBISI_uint32,
	ENBISI_int64,
	ENBISI_uint64,
	ENBISI_float,
	ENBISI_double,
	ENBISI_INT8,
	ENBISI_UINT8,
	ENBISI_INT16,
	ENBISI_UINT16,
	ENBISI_INT32,
	ENBISI_UINT32,
	ENBISI_INT64,
	ENBISI_UINT64,
	ENBISI_FLOAT,
	ENBISI_DOUBLE,
	ENBISI_json,
	ENBISI_yaml,

	//format opcode params
	ENBISI_sort_keys,
	ENBISI_locale,
	ENBISI_timezone,

	//mutate opcode mutation types
	ENBISI_change_type,
	ENBISI_delete,
	ENBISI_insert,
	ENBISI_swap_elements,
	ENBISI_deep_copy_elements,
	ENBISI_delete_elements,
	ENBISI_change_label,
	
	//enumeration of the first string that isn't static
	ENBISI_FIRST_DYNAMIC_STRING
};


//returns the string id representing EvaluableNodeType t
constexpr StringInternPool::StringID GetStringIdFromNodeTypeFromString(EvaluableNodeType t)
{
	if(t >= NUM_VALID_ENT_OPCODES)
		return ENT_NOT_A_BUILT_IN_TYPE;
	return static_cast<StringInternPool::StringID>(t + NUM_ENBISI_SPECIAL_STRING_IDS);
}

//like GetEvaluableNodeTypeFromString but uses a string id
constexpr EvaluableNodeType GetEvaluableNodeTypeFromStringId(StringInternPool::StringID sid)
{
	if(sid <= ENBISI_EMPTY_STRING)
		return ENT_NOT_A_BUILT_IN_TYPE;

	size_t type_index = sid - NUM_ENBISI_SPECIAL_STRING_IDS;
	if(type_index >= NUM_VALID_ENT_OPCODES)
		return ENT_NOT_A_BUILT_IN_TYPE;

	return static_cast<EvaluableNodeType>(type_index);
}

//returns a string of the enumerated type specified
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

	return string_intern_pool.GetStringFromID(GetStringIdFromNodeTypeFromString(t));
}

//returns the enumerated type for the string
// if get_non_keywords is true, then it will return types that are not necessarily keywords, like number
inline EvaluableNodeType GetEvaluableNodeTypeFromString(const std::string &s, bool get_non_keywords = false)
{
	auto sid = string_intern_pool.GetIDFromString(s);
	if(sid == string_intern_pool.NOT_A_STRING_ID)
		return ENT_NOT_A_BUILT_IN_TYPE;

	return GetEvaluableNodeTypeFromStringId(sid);
}
