#pragma once

//project headers:
#include "FastMath.h"
#include "StringInternPool.h"

//system headers:

//opcodes / commands / operations in Amalgam
enum EvaluableNodeType : uint8_t
{
	//System and Runtime
	ENT_HELP,
	ENT_PRINT,
	ENT_SYSTEM_TIME,
	ENT_SYSTEM,
	ENT_RECLAIM_RESOURCES,

	//Primitive Types
	ENT_NULL,
	ENT_BOOL,
	ENT_NUMBER,
	ENT_STRING,
	ENT_LIST,
	ENT_UNORDERED_LIST,
	ENT_ASSOC,

	//Variable Definition & Modification
	ENT_SYMBOL,
	ENT_LET,
	ENT_DECLARE,
	ENT_ASSIGN,
	ENT_ACCUM,
	ENT_RETRIEVE,
	ENT_TARGET,
	ENT_STACK,
	ENT_ARGS,
	ENT_GET_TYPE,
	ENT_GET_TYPE_STRING,
	ENT_SET_TYPE,
	ENT_FORMAT,
	//TODO 25241: consolidate from here down
	//Control Flow
	ENT_IF,
	ENT_SEQUENCE,
	ENT_LAMBDA,
	ENT_CALL,
	ENT_CALL_SANDBOXED,
	ENT_WHILE,
	ENT_CONCLUDE,
	ENT_RETURN,
	ENT_APPLY,
	ENT_OPCODE_STACK,

	//Logic & Comparison
	ENT_AND,
	ENT_OR,
	ENT_XOR,
	ENT_NOT,
	ENT_EQUAL,
	ENT_NEQUAL,
	ENT_LESS,
	ENT_LEQUAL,
	ENT_GREATER,
	ENT_GEQUAL,
	ENT_TYPE_EQUALS,
	ENT_TYPE_NEQUALS,

	//Basic Math
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
	ENT_ABS,
	ENT_MAX,
	ENT_MIN,
	ENT_INDEX_MAX,
	ENT_INDEX_MIN,

	//Advanced Math
	ENT_EXPONENT,
	ENT_LOG,
	ENT_ERF,
	ENT_TGAMMA,
	ENT_LGAMMA,
	ENT_SQRT,
	ENT_POW,
	ENT_DOT_PRODUCT,
	ENT_NORMALIZE,
	ENT_MODE,
	ENT_QUANTILE,
	ENT_GENERALIZED_MEAN,
	ENT_GENERALIZED_DISTANCE,
	ENT_ENTROPY,

	//Trigonometry
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

	//String Operations
	ENT_EXPLODE,
	ENT_SPLIT,
	ENT_SUBSTR,
	ENT_CONCAT,
	ENT_PARSE,
	ENT_UNPARSE,

	//Container Manipulation
	ENT_FIRST,
	ENT_TAIL,
	ENT_LAST,
	ENT_TRUNC,
	ENT_APPEND,
	ENT_SIZE,
	ENT_GET,
	ENT_SET,
	ENT_REPLACE,
	ENT_REVERSE,
	ENT_SORT,
	ENT_INDICES,
	ENT_VALUES,
	ENT_CONTAINS_INDEX,
	ENT_CONTAINS_VALUE,
	ENT_REMOVE,
	ENT_KEEP,

	//Iteration and Container Transformation
	ENT_RANGE,
	ENT_REWRITE,
	ENT_MAP,
	ENT_FILTER,
	ENT_WEAVE,
	ENT_REDUCE,
	ENT_ASSOCIATE,
	ENT_ZIP,
	ENT_UNZIP,
	ENT_CURRENT_INDEX,
	ENT_CURRENT_VALUE,
	ENT_PREVIOUS_RESULT,

	//Entity Lifecycle and Storage
	ENT_CREATE_ENTITIES,
	ENT_CLONE_ENTITIES,
	ENT_MOVE_ENTITIES,
	ENT_DESTROY_ENTITIES,
	ENT_LOAD,
	ENT_LOAD_ENTITY,
	ENT_STORE,
	ENT_STORE_ENTITY,
	ENT_CONTAINS_ENTITY,
	ENT_FLATTEN_ENTITY,

	//Entity Access and Manipulation
	ENT_CONTAINS_LABEL,
	ENT_ASSIGN_TO_ENTITIES,
	ENT_REMOVE_FROM_ENTITIES,
	ENT_ACCUM_TO_ENTITIES,
	ENT_RETRIEVE_FROM_ENTITY,
	ENT_CALL_ENTITY,
	ENT_CALL_ENTITY_GET_CHANGES,
	ENT_CALL_ON_ENTITY,
	ENT_CALL_CONTAINER,

	//Entity Query Engine
	ENT_CONTAINED_ENTITIES,
	ENT_COMPUTE_ON_CONTAINED_ENTITIES,
	ENT_QUERY_SELECT,
	ENT_QUERY_SAMPLE,
	ENT_QUERY_IN_ENTITY_LIST,
	ENT_QUERY_NOT_IN_ENTITY_LIST,
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
	ENT_QUERY_DISTANCE_CONTRIBUTIONS,
	ENT_QUERY_ENTITY_CONVICTIONS,
	ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE,
	ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS,
	ENT_QUERY_ENTITY_KL_DIVERGENCES,
	ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS,

	//Metadata
	ENT_GET_ANNOTATIONS,
	ENT_SET_ANNOTATIONS,
	ENT_GET_COMMENTS,
	ENT_SET_COMMENTS,
	ENT_GET_CONCURRENCY,
	ENT_SET_CONCURRENCY,
	ENT_GET_VALUE,
	ENT_SET_VALUE,
	ENT_GET_ENTITY_ANNOTATIONS,
	ENT_GET_ENTITY_COMMENTS,
	ENT_RETRIEVE_ENTITY_ROOT,
	ENT_ASSIGN_ENTITY_ROOTS,
	ENT_GET_ENTITY_PERMISSIONS,
	ENT_SET_ENTITY_PERMISSIONS,

	//Code Comparison and Evolution
	ENT_TOTAL_SIZE,
	ENT_MUTATE,
	ENT_GET_MUTATION_DEFAULTS,
	ENT_COMMONALITY,
	ENT_EDIT_DISTANCE,
	ENT_INTERSECT,
	ENT_UNION,
	ENT_DIFFERENCE,
	ENT_MIX,

	//Entity Comparison and Evolution
	ENT_TOTAL_ENTITY_SIZE,
	ENT_MUTATE_ENTITY,
	ENT_COMMONALITY_ENTITIES,
	ENT_EDIT_DISTANCE_ENTITIES,
	ENT_INTERSECT_ENTITIES,
	ENT_UNION_ENTITIES,
	ENT_DIFFERENCE_ENTITIES,
	ENT_MIX_ENTITIES,

	//Random
	ENT_RAND,
	ENT_GET_RAND_SEED,
	ENT_SET_RAND_SEED,
	ENT_GET_ENTITY_RAND_SEED,
	ENT_SET_ENTITY_RAND_SEED,

	//Cryptography
	ENT_CRYPTO_SIGN,
	ENT_CRYPTO_SIGN_VERIFY,
	ENT_ENCRYPT,
	ENT_DECRYPT,

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

__forceinline constexpr bool IsEvaluableNodeTypeValid(EvaluableNodeType t)
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
	ENBISI_true = NUM_VALID_ENT_OPCODES + NUM_ENBISI_SPECIAL_STRING_IDS,
	ENBISI_false,
	ENBISI_infinity,
	ENBISI_neg_infinity,
	ENBISI_zero,
	ENBISI_one_half,
	ENBISI_one,
	ENBISI_two,
	ENBISI_three,
	ENBISI_four,
	ENBISI_five,
	ENBISI_six,
	ENBISI_seven,
	ENBISI_eight,
	ENBISI_nine,
	ENBISI_ten,
	ENBISI_eleven,
	ENBISI_twelve,
	ENBISI_neg_one,
	ENBISI_neg_two,
	ENBISI_zero_number_key,
	ENBISI_one_half_number_key,
	ENBISI_one_number_key,
	ENBISI_two_number_key,
	ENBISI_three_number_key,
	ENBISI_four_number_key,
	ENBISI_five_number_key,
	ENBISI_six_number_key,
	ENBISI_seven_number_key,
	ENBISI_eight_number_key,
	ENBISI_nine_number_key,
	ENBISI_ten_number_key,
	ENBISI_eleven_number_key,
	ENBISI_twelve_number_key,
	ENBISI_neg_one_number_key,
	ENBISI_neg_two_number_key,
	ENBISI_empty_null,
	ENBISI_empty_list,
	ENBISI_empty_assoc,
	ENBISI_null_key,
	ENBISI_true_key,
	ENBISI_false_key,

	//config file parameters
	ENBISI_rand_seed,
	ENBISI_version,

	//help options
	ENBISI_overview,
	ENBISI_syntax,
	ENBISI_distance,
	ENBISI_opcodes,

	//file storage options
	ENBISI_include_rand_seeds,
	ENBISI_escape_resource_name,
	ENBISI_escape_contained_resource_names,
	ENBISI_transactional,
	ENBISI_pretty_print,
	//ENBISI_sort_keys -- covered in format below
	ENBISI_flatten,
	ENBISI_parallel_create,
	ENBISI_execute_on_load,

	//substr parameters
	ENBISI_all,
	ENBISI_submatches,

	//dynamically generated function parameters
	ENBISI__,
	ENBISI_create_new_entity,
	ENBISI_new_entity,
	ENBISI_require_version_compatibility,
	ENBISI_amlg_version,
	ENBISI_version_compatible,

	//entity access parameters
	ENBISI_accessing_entity,

	//entity permissions
	ENBISI_std_out_and_std_err,
	ENBISI_std_in,
	ENBISI_load,
	ENBISI_store,
	ENBISI_environment,
	ENBISI_alter_performance,
	ENBISI_system,

	//distance types
	ENBISI_nominal_bool,
	ENBISI_nominal_number,
	ENBISI_nominal_string,
	ENBISI_nominal_code,
	ENBISI_continuous_number,
	ENBISI_continuous_number_cyclic,
	ENBISI_continuous_string,
	ENBISI_continuous_code,

	//distance parameter values
	ENBISI_surprisal,
	ENBISI_surprisal_to_prob,

	//numeric precision types
	ENBISI_precise,
	ENBISI_fast,
	ENBISI_recompute_precise,

	//format opcode types
	ENBISI_code,
	ENBISI_base16,
	ENBISI_base64,
	ENBISI_int8,
	ENBISI_uint8,
	ENBISI_int16,
	ENBISI_uint16,
	ENBISI_int32,
	ENBISI_uint32,
	ENBISI_int64,
	ENBISI_uint64,
	ENBISI_float32,
	ENBISI_float64,
	ENBISI_gt_int8,
	ENBISI_gt_uint8,
	ENBISI_gt_int16,
	ENBISI_gt_uint16,
	ENBISI_gt_int32,
	ENBISI_gt_uint32,
	ENBISI_gt_int64,
	ENBISI_gt_uint64,
	ENBISI_gt_float32,
	ENBISI_gt_float64,
	ENBISI_lt_int8,
	ENBISI_lt_uint8,
	ENBISI_lt_int16,
	ENBISI_lt_uint16,
	ENBISI_lt_int32,
	ENBISI_lt_uint32,
	ENBISI_lt_int64,
	ENBISI_lt_uint64,
	ENBISI_lt_float32,
	ENBISI_lt_float64,
	ENBISI_json,
	ENBISI_yaml,

	//format opcode params
	ENBISI_sort_keys,
	ENBISI_locale,
	ENBISI_time_zone,

	//mutate opcode mutation types
	ENBISI_change_type,
	ENBISI_delete,
	ENBISI_insert,
	ENBISI_swap_elements,
	ENBISI_deep_copy_elements,
	ENBISI_delete_elements,
	
	//mix parameters
	ENBISI_string_edit_distance,
	ENBISI_types_must_match,
	ENBISI_nominal_numbers,
	ENBISI_nominal_strings,
	ENBISI_similar_mix_chance,
	ENBISI_unnamed_entity_mix_chance,
	ENBISI_recursive_matching,

	//enumeration of the first string that isn't static
	ENBISI_FIRST_DYNAMIC_STRING
};
