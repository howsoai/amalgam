//project headers:
#include "Opcodes.h"
#include "StringInternPool.h"

void StringInternPool::InitializeStaticStrings()
{
	numStaticStrings = ENBISI_FIRST_DYNAMIC_STRING;
	stringToID.reserve(numStaticStrings);
	idToStringAndRefCount.resize(numStaticStrings);

	EmplaceStaticString(ENBISI_NOT_A_STRING, ".nas");
	EmplaceStaticString(ENBISI_EMPTY_STRING, "");


	//opcodes

	//built-in / system specific
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SYSTEM), "system");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_DEFAULTS), "get_defaults");

	//parsing
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_PARSE), "parse");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_UNPARSE), "unparse");

	//core control
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_IF), "if");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SEQUENCE), "seq");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_PARALLEL), "parallel");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LAMBDA), "lambda");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CONCLUDE), "conclude");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CALL), "call");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CALL_SANDBOXED), "call_sandboxed");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_WHILE), "while");

	//definitions
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LET), "let");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DECLARE), "declare");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ASSIGN), "assign");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ACCUM), "accum");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_RETRIEVE), "retrieve");
		
	//retrieval
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET), "get");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET), "set");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_REPLACE), "replace");

	//stack and node manipulation
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TARGET), "target");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CURRENT_INDEX), "current_index");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CURRENT_VALUE), "current_value");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_PREVIOUS_RESULT), "previous_result");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_STACK), "stack");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ARGS), "args");

	//simulation and operations
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_RAND), "rand");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_WEIGHTED_RAND), "weighted_rand");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_RAND_SEED), "get_rand_seed");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_RAND_SEED), "set_rand_seed");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SYSTEM_TIME), "system_time");

	//base math
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ADD), "+");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SUBTRACT), "-");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MULTIPLY), "*");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DIVIDE), "/");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MODULUS), "mod");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_DIGITS), "get_digits");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_DIGITS), "set_digits");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_FLOOR), "floor");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CEILING), "ceil");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ROUND), "round");

	//extended math
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_EXPONENT), "exp");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LOG), "log");

	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SIN), "sin");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ASIN), "asin");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COS), "cos");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ACOS), "acos");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TAN), "tan");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ATAN), "atan");

	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SINH), "sinh");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ASINH), "asinh");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COSH), "cosh");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ACOSH), "acosh");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TANH), "tanh");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ATANH), "atanh");

	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ERF), "erf");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TGAMMA), "tgamma");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LGAMMA), "lgamma");

	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SQRT), "sqrt");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_POW), "pow");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ABS), "abs");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MAX), "max");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MIN), "min");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GENERALIZED_DISTANCE), "generalized_distance");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DOT_PRODUCT), "dot_product");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ENTROPY), "entropy");

	//list manipulation
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_FIRST), "first");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TAIL), "tail");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LAST), "last");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TRUNC), "trunc");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_APPEND), "append");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SIZE), "size");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_RANGE), "range");

	//transformation
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_REWRITE), "rewrite");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MAP), "map");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_FILTER), "filter");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_WEAVE), "weave");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_REDUCE), "reduce");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_APPLY), "apply");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_REVERSE), "reverse");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SORT), "sort");

	//associative list manipulation
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_INDICES), "indices");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_VALUES), "values");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CONTAINS_INDEX), "contains_index");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CONTAINS_VALUE), "contains_value");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_REMOVE), "remove");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_KEEP), "keep");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ASSOCIATE), "associate");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ZIP), "zip");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_UNZIP), "unzip");

	//logic
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_AND), "and");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_OR), "or");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_XOR), "xor");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_NOT), "not");

	//equivalence
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_EQUAL), "=");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_NEQUAL), "!=");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LESS), "<");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LEQUAL), "<=");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GREATER), ">");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GEQUAL), ">=");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TYPE_EQUALS), "~");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TYPE_NEQUALS), "!~");

	//built-in constants and variables
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TRUE), "true");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_FALSE), "false");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_NULL), "null");

	//data types
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LIST), "list");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ASSOC), "assoc");

	//immediates - no associated keywords
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_NUMBER), "number");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_STRING), "string");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SYMBOL), "symbol");

	//node types
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_TYPE), "get_type");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_TYPE_STRING), "get_type_string");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_TYPE), "set_type");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_FORMAT), "format");

	//labels and comments
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_LABELS), "get_labels");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_ALL_LABELS), "get_all_labels");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_LABELS), "set_labels");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ZIP_LABELS), "zip_labels");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_COMMENTS), "get_comments");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_COMMENTS), "set_comments");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_CONCURRENCY), "get_concurrency");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_CONCURRENCY), "set_concurrency");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_VALUE), "get_value");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_VALUE), "set_value");

	//string
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_EXPLODE), "explode");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SPLIT), "split");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SUBSTR), "substr");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CONCAT), "concat");

	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CRYPTO_SIGN), "crypto_sign");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CRYPTO_SIGN_VERIFY), "crypto_sign_verify");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ENCRYPT), "encrypt");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DECRYPT), "decrypt");

	//I/O
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_PRINT), "print");

	//tree merging
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TOTAL_SIZE), "total_size");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COMMONALITY), "commonality");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_EDIT_DISTANCE), "edit_distance");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MUTATE), "mutate");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_INTERSECT), "intersect");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_UNION), "union");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DIFFERENCE), "difference");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MIX), "mix");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MIX_LABELS), "mix_labels");

	//entity merging
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_TOTAL_ENTITY_SIZE), "total_entity_size");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_FLATTEN_ENTITY), "flatten_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COMMONALITY_ENTITIES), "commonality_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_EDIT_DISTANCE_ENTITIES), "edit_distance_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MUTATE_ENTITY), "mutate_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_INTERSECT_ENTITIES), "intersect_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_UNION_ENTITIES), "union_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DIFFERENCE_ENTITIES), "difference_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MIX_ENTITIES), "mix_entities");

	//entity details
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_ENTITY_COMMENTS), "get_entity_comments");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_RETRIEVE_ENTITY_ROOT), "retrieve_entity_root");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ASSIGN_ENTITY_ROOTS), "assign_entity_roots");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ACCUM_ENTITY_ROOTS), "accum_entity_roots");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_ENTITY_RAND_SEED), "get_entity_rand_seed");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_ENTITY_RAND_SEED), "set_entity_rand_seed");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_GET_ENTITY_ROOT_PERMISSION), "get_entity_root_permission");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_SET_ENTITY_ROOT_PERMISSION), "set_entity_root_permission");

	//entity base actions
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CREATE_ENTITIES), "create_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CLONE_ENTITIES), "clone_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_MOVE_ENTITIES), "move_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DESTROY_ENTITIES), "destroy_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LOAD), "load");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LOAD_ENTITY), "load_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_LOAD_PERSISTENT_ENTITY), "load_persistent_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_STORE), "store");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_STORE_ENTITY), "store_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CONTAINS_ENTITY), "contains_entity");

	//entity query
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CONTAINED_ENTITIES), "contained_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COMPUTE_ON_CONTAINED_ENTITIES), "compute_on_contained_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_COUNT), "query_count");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_SELECT), "query_select");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_SAMPLE), "query_sample");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_WEIGHTED_SAMPLE), "query_weighted_sample");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_IN_ENTITY_LIST), "query_in_entity_list");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_NOT_IN_ENTITY_LIST), "query_not_in_entity_list");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_EXISTS), "query_exists");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_NOT_EXISTS), "query_not_exists");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_EQUALS), "query_equals");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_NOT_EQUALS), "query_not_equals");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_BETWEEN), "query_between");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_NOT_BETWEEN), "query_not_between");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_AMONG), "query_among");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_NOT_AMONG), "query_not_among");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_MAX), "query_max");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_MIN), "query_min");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_SUM), "query_sum");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_MODE), "query_mode");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_QUANTILE), "query_quantile");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_GENERALIZED_MEAN), "query_generalized_mean");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_MIN_DIFFERENCE), "query_min_difference");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_MAX_DIFFERENCE), "query_max_difference");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_VALUE_MASSES), "query_value_masses");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_LESS_OR_EQUAL_TO), "query_less_or_equal_to");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_GREATER_OR_EQUAL_TO), "query_greater_or_equal_to");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_WITHIN_GENERALIZED_DISTANCE), "query_within_generalized_distance");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_QUERY_NEAREST_GENERALIZED_DISTANCE), "query_nearest_generalized_distance");

	//compute queries
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COMPUTE_ENTITY_CONVICTIONS), "compute_entity_convictions");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE), "compute_entity_group_kl_divergence");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS), "compute_entity_distance_contributions");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_COMPUTE_ENTITY_KL_DIVERGENCES), "compute_entity_kl_divergences");

	//entity access
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CONTAINS_LABEL), "contains_label");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ASSIGN_TO_ENTITIES), "assign_to_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DIRECT_ASSIGN_TO_ENTITIES), "direct_assign_to_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_ACCUM_TO_ENTITIES), "accum_to_entities");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_RETRIEVE_FROM_ENTITY), "retrieve_from_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_DIRECT_RETRIEVE_FROM_ENTITY), "direct_retrieve_from_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CALL_ENTITY), "call_entity");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CALL_ENTITY_GET_CHANGES), "call_entity_get_changes");
	EmplaceStaticString(GetStringIdFromNodeTypeFromString(ENT_CALL_CONTAINER), "call_container");

	//end opcodes

	//built-in common values
	EmplaceStaticString(ENBISI_nan, ".nan");
	EmplaceStaticString(ENBISI_infinity, ".infinity");
	EmplaceStaticString(ENBISI_neg_infinity, "-.infinity");
	EmplaceStaticString(ENBISI_zero, "0");
	EmplaceStaticString(ENBISI_one, "1");
	EmplaceStaticString(ENBISI_neg_one, "-1");
	EmplaceStaticString(ENBISI_empty_null, "(null)");
	EmplaceStaticString(ENBISI_empty_list, "(list)");
	EmplaceStaticString(ENBISI_empty_assoc, "(assoc)");
	EmplaceStaticString(ENBISI_empty_true, "(true)");
	EmplaceStaticString(ENBISI_empty_false, "(false)");

	//config file parameters
	EmplaceStaticString(ENBISI_rand_seed, "rand_seed");
	EmplaceStaticString(ENBISI_version, "version");

	//substr parameters
	EmplaceStaticString(ENBISI_all, "all");
	EmplaceStaticString(ENBISI_submatches, "submatches");

	//dynamically generated function parameters
	EmplaceStaticString(ENBISI__, "_");
	EmplaceStaticString(ENBISI_create_new_entity, "create_new_entity");
	EmplaceStaticString(ENBISI_new_entity, "new_entity");

	//entity access parameters
	EmplaceStaticString(ENBISI_accessing_entity, "accessing_entity");

	//distance types
	EmplaceStaticString(ENBISI_nominal_numeric, "nominal_numeric");
	EmplaceStaticString(ENBISI_nominal_string, "nominal_string");
	EmplaceStaticString(ENBISI_nominal_code, "nominal_code");
	EmplaceStaticString(ENBISI_continuous_numeric, "continuous_numeric");
	EmplaceStaticString(ENBISI_continuous_numeric_cyclic, "continuous_numeric_cyclic");
	EmplaceStaticString(ENBISI_continuous_string, "continuous_string");
	EmplaceStaticString(ENBISI_continuous_code, "continuous_code");

	//distance parameter values
	EmplaceStaticString(ENBISI_surprisal_to_prob, "surprisal_to_prob");

	//numerical precision types
	EmplaceStaticString(ENBISI_precise, "precise");
	EmplaceStaticString(ENBISI_fast, "fast");
	EmplaceStaticString(ENBISI_recompute_precise, "recompute_precise");

	//format opcode types
	EmplaceStaticString(ENBISI_code, "code");
	EmplaceStaticString(ENBISI_Base16, "Base16");
	EmplaceStaticString(ENBISI_Base64, "Base64");
	EmplaceStaticString(ENBISI_int8, "int8");
	EmplaceStaticString(ENBISI_uint8, "uint8");
	EmplaceStaticString(ENBISI_int16, "int16");
	EmplaceStaticString(ENBISI_uint16, "uint16");
	EmplaceStaticString(ENBISI_int32, "int32");
	EmplaceStaticString(ENBISI_uint32, "uint32");
	EmplaceStaticString(ENBISI_int64, "int64");
	EmplaceStaticString(ENBISI_uint64, "uint64");
	EmplaceStaticString(ENBISI_float, "float");
	EmplaceStaticString(ENBISI_double, "double");
	EmplaceStaticString(ENBISI_INT8, "INT8");
	EmplaceStaticString(ENBISI_UINT8, "UINT8");
	EmplaceStaticString(ENBISI_INT16, "INT16");
	EmplaceStaticString(ENBISI_UINT16, "UINT16");
	EmplaceStaticString(ENBISI_INT32, "INT32");
	EmplaceStaticString(ENBISI_UINT32, "UINT32");
	EmplaceStaticString(ENBISI_INT64, "INT64");
	EmplaceStaticString(ENBISI_UINT64, "UINT64");
	EmplaceStaticString(ENBISI_FLOAT, "FLOAT");
	EmplaceStaticString(ENBISI_DOUBLE, "DOUBLE");
	EmplaceStaticString(ENBISI_json, "json");
	EmplaceStaticString(ENBISI_yaml, "yaml");

	//formapt opcode params
	EmplaceStaticString(ENBISI_sort_keys, "sort_keys");
	EmplaceStaticString(ENBISI_locale, "locale");
	EmplaceStaticString(ENBISI_timezone, "timezone");

	//mutate opcode mutation types
	EmplaceStaticString(ENBISI_change_type, "change_type");
	EmplaceStaticString(ENBISI_delete, "delete");
	EmplaceStaticString(ENBISI_insert, "insert");
	EmplaceStaticString(ENBISI_swap_elements, "swap_elements");
	EmplaceStaticString(ENBISI_deep_copy_elements, "deep_copy_elements");
	EmplaceStaticString(ENBISI_delete_elements, "delete_elements");
	EmplaceStaticString(ENBISI_change_label, "change_label");
}
