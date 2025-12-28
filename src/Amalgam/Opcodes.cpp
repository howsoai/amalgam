//project headers:
#include "Opcodes.h"
#include "StringInternPool.h"

StringInternPool string_intern_pool;

static inline void EmplaceStaticString(EvaluableNodeBuiltInStringId bisid, std::string str)
{
	auto sid = string_intern_pool.CreateStringReference(str);
	string_intern_pool.staticStringsIndexToStringID[bisid] = sid;
	string_intern_pool.staticStringIDToIndex.emplace(sid, bisid);
}

inline void EmplaceNodeTypeString(EvaluableNodeType t, const char *str)
{
	EmplaceStaticString(static_cast<EvaluableNodeBuiltInStringId>(t + NUM_ENBISI_SPECIAL_STRING_IDS), str);
}

void StringInternPool::InitializeStaticStrings()
{
	staticStringsIndexToStringID.resize(ENBISI_FIRST_DYNAMIC_STRING);
	staticStringIDToIndex.reserve(ENBISI_FIRST_DYNAMIC_STRING);

	string_intern_pool.staticStringsIndexToStringID[ENBISI_EMPTY_STRING] = string_intern_pool.emptyStringId;
	string_intern_pool.staticStringIDToIndex.emplace(string_intern_pool.emptyStringId, ENBISI_EMPTY_STRING);

	//opcodes

	//built-in / system specific
	EmplaceNodeTypeString(ENT_SYSTEM, "system");
	EmplaceNodeTypeString(ENT_GET_DEFAULTS, "get_defaults");
	EmplaceNodeTypeString(ENT_RECLAIM_RESOURCES, "reclaim_resources");

	//parsing
	EmplaceNodeTypeString(ENT_PARSE, "parse");
	EmplaceNodeTypeString(ENT_UNPARSE, "unparse");

	//core control
	EmplaceNodeTypeString(ENT_IF, "if");
	EmplaceNodeTypeString(ENT_SEQUENCE, "seq");
	EmplaceNodeTypeString(ENT_LAMBDA, "lambda");
	EmplaceNodeTypeString(ENT_CONCLUDE, "conclude");
	EmplaceNodeTypeString(ENT_RETURN, "return");
	EmplaceNodeTypeString(ENT_CALL, "call");
	EmplaceNodeTypeString(ENT_CALL_SANDBOXED, "call_sandboxed");
	EmplaceNodeTypeString(ENT_WHILE, "while");

	//definitions
	EmplaceNodeTypeString(ENT_LET, "let");
	EmplaceNodeTypeString(ENT_DECLARE, "declare");
	EmplaceNodeTypeString(ENT_ASSIGN, "assign");
	EmplaceNodeTypeString(ENT_ACCUM, "accum");
	EmplaceNodeTypeString(ENT_RETRIEVE, "retrieve");
		
	//retrieval
	EmplaceNodeTypeString(ENT_GET, "get");
	EmplaceNodeTypeString(ENT_SET, "set");
	EmplaceNodeTypeString(ENT_REPLACE, "replace");

	//stack and node manipulation
	EmplaceNodeTypeString(ENT_TARGET, "target");
	EmplaceNodeTypeString(ENT_CURRENT_INDEX, "current_index");
	EmplaceNodeTypeString(ENT_CURRENT_VALUE, "current_value");
	EmplaceNodeTypeString(ENT_PREVIOUS_RESULT, "previous_result");
	EmplaceNodeTypeString(ENT_OPCODE_STACK, "opcode_stack");
	EmplaceNodeTypeString(ENT_STACK, "stack");
	EmplaceNodeTypeString(ENT_ARGS, "args");

	//simulation and operations
	EmplaceNodeTypeString(ENT_RAND, "rand");
	EmplaceNodeTypeString(ENT_GET_RAND_SEED, "get_rand_seed");
	EmplaceNodeTypeString(ENT_SET_RAND_SEED, "set_rand_seed");
	EmplaceNodeTypeString(ENT_SYSTEM_TIME, "system_time");

	//base math
	EmplaceNodeTypeString(ENT_ADD, "+");
	EmplaceNodeTypeString(ENT_SUBTRACT, "-");
	EmplaceNodeTypeString(ENT_MULTIPLY, "*");
	EmplaceNodeTypeString(ENT_DIVIDE, "/");
	EmplaceNodeTypeString(ENT_MODULUS, "mod");
	EmplaceNodeTypeString(ENT_GET_DIGITS, "get_digits");
	EmplaceNodeTypeString(ENT_SET_DIGITS, "set_digits");
	EmplaceNodeTypeString(ENT_FLOOR, "floor");
	EmplaceNodeTypeString(ENT_CEILING, "ceil");
	EmplaceNodeTypeString(ENT_ROUND, "round");

	//extended math
	EmplaceNodeTypeString(ENT_EXPONENT, "exp");
	EmplaceNodeTypeString(ENT_LOG, "log");

	EmplaceNodeTypeString(ENT_SIN, "sin");
	EmplaceNodeTypeString(ENT_ASIN, "asin");
	EmplaceNodeTypeString(ENT_COS, "cos");
	EmplaceNodeTypeString(ENT_ACOS, "acos");
	EmplaceNodeTypeString(ENT_TAN, "tan");
	EmplaceNodeTypeString(ENT_ATAN, "atan");

	EmplaceNodeTypeString(ENT_SINH, "sinh");
	EmplaceNodeTypeString(ENT_ASINH, "asinh");
	EmplaceNodeTypeString(ENT_COSH, "cosh");
	EmplaceNodeTypeString(ENT_ACOSH, "acosh");
	EmplaceNodeTypeString(ENT_TANH, "tanh");
	EmplaceNodeTypeString(ENT_ATANH, "atanh");

	EmplaceNodeTypeString(ENT_ERF, "erf");
	EmplaceNodeTypeString(ENT_TGAMMA, "tgamma");
	EmplaceNodeTypeString(ENT_LGAMMA, "lgamma");

	EmplaceNodeTypeString(ENT_SQRT, "sqrt");
	EmplaceNodeTypeString(ENT_POW, "pow");
	EmplaceNodeTypeString(ENT_ABS, "abs");
	EmplaceNodeTypeString(ENT_MAX, "max");
	EmplaceNodeTypeString(ENT_MIN, "min");
	EmplaceNodeTypeString(ENT_INDEX_MAX, "index_max");
	EmplaceNodeTypeString(ENT_INDEX_MIN, "index_min");
	EmplaceNodeTypeString(ENT_MODE, "mode");
	EmplaceNodeTypeString(ENT_QUANTILE, "quantile");
	EmplaceNodeTypeString(ENT_GENERALIZED_MEAN, "generalized_mean");
	EmplaceNodeTypeString(ENT_GENERALIZED_DISTANCE, "generalized_distance");
	EmplaceNodeTypeString(ENT_DOT_PRODUCT, "dot_product");
	EmplaceNodeTypeString(ENT_NORMALIZE, "normalize");
	EmplaceNodeTypeString(ENT_ENTROPY, "entropy");

	//list manipulation
	EmplaceNodeTypeString(ENT_FIRST, "first");
	EmplaceNodeTypeString(ENT_TAIL, "tail");
	EmplaceNodeTypeString(ENT_LAST, "last");
	EmplaceNodeTypeString(ENT_TRUNC, "trunc");
	EmplaceNodeTypeString(ENT_APPEND, "append");
	EmplaceNodeTypeString(ENT_SIZE, "size");
	EmplaceNodeTypeString(ENT_RANGE, "range");

	//transformation
	EmplaceNodeTypeString(ENT_REWRITE, "rewrite");
	EmplaceNodeTypeString(ENT_MAP, "map");
	EmplaceNodeTypeString(ENT_FILTER, "filter");
	EmplaceNodeTypeString(ENT_WEAVE, "weave");
	EmplaceNodeTypeString(ENT_REDUCE, "reduce");
	EmplaceNodeTypeString(ENT_APPLY, "apply");
	EmplaceNodeTypeString(ENT_REVERSE, "reverse");
	EmplaceNodeTypeString(ENT_SORT, "sort");

	//associative list manipulation
	EmplaceNodeTypeString(ENT_INDICES, "indices");
	EmplaceNodeTypeString(ENT_VALUES, "values");
	EmplaceNodeTypeString(ENT_CONTAINS_INDEX, "contains_index");
	EmplaceNodeTypeString(ENT_CONTAINS_VALUE, "contains_value");
	EmplaceNodeTypeString(ENT_REMOVE, "remove");
	EmplaceNodeTypeString(ENT_KEEP, "keep");
	EmplaceNodeTypeString(ENT_ASSOCIATE, "associate");
	EmplaceNodeTypeString(ENT_ZIP, "zip");
	EmplaceNodeTypeString(ENT_UNZIP, "unzip");

	//logic
	EmplaceNodeTypeString(ENT_AND, "and");
	EmplaceNodeTypeString(ENT_OR, "or");
	EmplaceNodeTypeString(ENT_XOR, "xor");
	EmplaceNodeTypeString(ENT_NOT, "not");

	//equivalence
	EmplaceNodeTypeString(ENT_EQUAL, "=");
	EmplaceNodeTypeString(ENT_NEQUAL, "!=");
	EmplaceNodeTypeString(ENT_LESS, "<");
	EmplaceNodeTypeString(ENT_LEQUAL, "<=");
	EmplaceNodeTypeString(ENT_GREATER, ">");
	EmplaceNodeTypeString(ENT_GEQUAL, ">=");
	EmplaceNodeTypeString(ENT_TYPE_EQUALS, "~");
	EmplaceNodeTypeString(ENT_TYPE_NEQUALS, "!~");

	//built-in constants and variables
	EmplaceNodeTypeString(ENT_NULL, "null");

	//data types
	EmplaceNodeTypeString(ENT_LIST, "list");
	EmplaceNodeTypeString(ENT_UNORDERED_LIST, "unordered_list");
	EmplaceNodeTypeString(ENT_ASSOC, "assoc");

	//immediates - no associated keywords
	EmplaceNodeTypeString(ENT_BOOL, "bool");
	EmplaceNodeTypeString(ENT_NUMBER, "number");
	EmplaceNodeTypeString(ENT_STRING, "string");
	EmplaceNodeTypeString(ENT_SYMBOL, "symbol");

	//node types
	EmplaceNodeTypeString(ENT_GET_TYPE, "get_type");
	EmplaceNodeTypeString(ENT_GET_TYPE_STRING, "get_type_string");
	EmplaceNodeTypeString(ENT_SET_TYPE, "set_type");
	EmplaceNodeTypeString(ENT_FORMAT, "format");

	//labels and comments
	EmplaceNodeTypeString(ENT_GET_LABELS, "get_labels");
	EmplaceNodeTypeString(ENT_GET_ALL_LABELS, "get_all_labels");
	EmplaceNodeTypeString(ENT_SET_LABELS, "set_labels");
	EmplaceNodeTypeString(ENT_ZIP_LABELS, "zip_labels");
	EmplaceNodeTypeString(ENT_GET_COMMENTS, "get_comments");
	EmplaceNodeTypeString(ENT_SET_COMMENTS, "set_comments");
	EmplaceNodeTypeString(ENT_GET_CONCURRENCY, "get_concurrency");
	EmplaceNodeTypeString(ENT_SET_CONCURRENCY, "set_concurrency");
	EmplaceNodeTypeString(ENT_GET_VALUE, "get_value");
	EmplaceNodeTypeString(ENT_SET_VALUE, "set_value");

	//string
	EmplaceNodeTypeString(ENT_EXPLODE, "explode");
	EmplaceNodeTypeString(ENT_SPLIT, "split");
	EmplaceNodeTypeString(ENT_SUBSTR, "substr");
	EmplaceNodeTypeString(ENT_CONCAT, "concat");

	EmplaceNodeTypeString(ENT_CRYPTO_SIGN, "crypto_sign");
	EmplaceNodeTypeString(ENT_CRYPTO_SIGN_VERIFY, "crypto_sign_verify");
	EmplaceNodeTypeString(ENT_ENCRYPT, "encrypt");
	EmplaceNodeTypeString(ENT_DECRYPT, "decrypt");

	//I/O
	EmplaceNodeTypeString(ENT_PRINT, "print");

	//tree merging
	EmplaceNodeTypeString(ENT_TOTAL_SIZE, "total_size");
	EmplaceNodeTypeString(ENT_COMMONALITY, "commonality");
	EmplaceNodeTypeString(ENT_EDIT_DISTANCE, "edit_distance");
	EmplaceNodeTypeString(ENT_MUTATE, "mutate");
	EmplaceNodeTypeString(ENT_INTERSECT, "intersect");
	EmplaceNodeTypeString(ENT_UNION, "union");
	EmplaceNodeTypeString(ENT_DIFFERENCE, "difference");
	EmplaceNodeTypeString(ENT_MIX, "mix");

	//entity merging
	EmplaceNodeTypeString(ENT_TOTAL_ENTITY_SIZE, "total_entity_size");
	EmplaceNodeTypeString(ENT_FLATTEN_ENTITY, "flatten_entity");
	EmplaceNodeTypeString(ENT_COMMONALITY_ENTITIES, "commonality_entities");
	EmplaceNodeTypeString(ENT_EDIT_DISTANCE_ENTITIES, "edit_distance_entities");
	EmplaceNodeTypeString(ENT_MUTATE_ENTITY, "mutate_entity");
	EmplaceNodeTypeString(ENT_INTERSECT_ENTITIES, "intersect_entities");
	EmplaceNodeTypeString(ENT_UNION_ENTITIES, "union_entities");
	EmplaceNodeTypeString(ENT_DIFFERENCE_ENTITIES, "difference_entities");
	EmplaceNodeTypeString(ENT_MIX_ENTITIES, "mix_entities");

	//entity details
	EmplaceNodeTypeString(ENT_GET_ENTITY_COMMENTS, "get_entity_comments");
	EmplaceNodeTypeString(ENT_RETRIEVE_ENTITY_ROOT, "retrieve_entity_root");
	EmplaceNodeTypeString(ENT_ASSIGN_ENTITY_ROOTS, "assign_entity_roots");
	EmplaceNodeTypeString(ENT_ACCUM_ENTITY_ROOTS, "accum_entity_roots");
	EmplaceNodeTypeString(ENT_GET_ENTITY_RAND_SEED, "get_entity_rand_seed");
	EmplaceNodeTypeString(ENT_SET_ENTITY_RAND_SEED, "set_entity_rand_seed");
	EmplaceNodeTypeString(ENT_GET_ENTITY_PERMISSIONS, "get_entity_permissions");
	EmplaceNodeTypeString(ENT_SET_ENTITY_PERMISSIONS, "set_entity_permissions");

	//entity base actions
	EmplaceNodeTypeString(ENT_CREATE_ENTITIES, "create_entities");
	EmplaceNodeTypeString(ENT_CLONE_ENTITIES, "clone_entities");
	EmplaceNodeTypeString(ENT_MOVE_ENTITIES, "move_entities");
	EmplaceNodeTypeString(ENT_DESTROY_ENTITIES, "destroy_entities");
	EmplaceNodeTypeString(ENT_LOAD, "load");
	EmplaceNodeTypeString(ENT_LOAD_ENTITY, "load_entity");
	EmplaceNodeTypeString(ENT_STORE, "store");
	EmplaceNodeTypeString(ENT_STORE_ENTITY, "store_entity");
	EmplaceNodeTypeString(ENT_CONTAINS_ENTITY, "contains_entity");

	//entity query
	EmplaceNodeTypeString(ENT_CONTAINED_ENTITIES, "contained_entities");
	EmplaceNodeTypeString(ENT_COMPUTE_ON_CONTAINED_ENTITIES, "compute_on_contained_entities");
	EmplaceNodeTypeString(ENT_QUERY_SELECT, "query_select");
	EmplaceNodeTypeString(ENT_QUERY_SAMPLE, "query_sample");
	EmplaceNodeTypeString(ENT_QUERY_IN_ENTITY_LIST, "query_in_entity_list");
	EmplaceNodeTypeString(ENT_QUERY_NOT_IN_ENTITY_LIST, "query_not_in_entity_list");
	EmplaceNodeTypeString(ENT_QUERY_EXISTS, "query_exists");
	EmplaceNodeTypeString(ENT_QUERY_NOT_EXISTS, "query_not_exists");
	EmplaceNodeTypeString(ENT_QUERY_EQUALS, "query_equals");
	EmplaceNodeTypeString(ENT_QUERY_NOT_EQUALS, "query_not_equals");
	EmplaceNodeTypeString(ENT_QUERY_BETWEEN, "query_between");
	EmplaceNodeTypeString(ENT_QUERY_NOT_BETWEEN, "query_not_between");
	EmplaceNodeTypeString(ENT_QUERY_AMONG, "query_among");
	EmplaceNodeTypeString(ENT_QUERY_NOT_AMONG, "query_not_among");
	EmplaceNodeTypeString(ENT_QUERY_MAX, "query_max");
	EmplaceNodeTypeString(ENT_QUERY_MIN, "query_min");
	EmplaceNodeTypeString(ENT_QUERY_SUM, "query_sum");
	EmplaceNodeTypeString(ENT_QUERY_MODE, "query_mode");
	EmplaceNodeTypeString(ENT_QUERY_QUANTILE, "query_quantile");
	EmplaceNodeTypeString(ENT_QUERY_GENERALIZED_MEAN, "query_generalized_mean");
	EmplaceNodeTypeString(ENT_QUERY_MIN_DIFFERENCE, "query_min_difference");
	EmplaceNodeTypeString(ENT_QUERY_MAX_DIFFERENCE, "query_max_difference");
	EmplaceNodeTypeString(ENT_QUERY_VALUE_MASSES, "query_value_masses");
	EmplaceNodeTypeString(ENT_QUERY_LESS_OR_EQUAL_TO, "query_less_or_equal_to");
	EmplaceNodeTypeString(ENT_QUERY_GREATER_OR_EQUAL_TO, "query_greater_or_equal_to");
	EmplaceNodeTypeString(ENT_QUERY_WITHIN_GENERALIZED_DISTANCE, "query_within_generalized_distance");
	EmplaceNodeTypeString(ENT_QUERY_NEAREST_GENERALIZED_DISTANCE, "query_nearest_generalized_distance");
	EmplaceNodeTypeString(ENT_QUERY_DISTANCE_CONTRIBUTIONS, "query_distance_contributions");
	EmplaceNodeTypeString(ENT_QUERY_ENTITY_CONVICTIONS, "query_entity_convictions");
	EmplaceNodeTypeString(ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE, "query_entity_group_kl_divergence");
	EmplaceNodeTypeString(ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS, "query_entity_distance_contributions");
	EmplaceNodeTypeString(ENT_QUERY_ENTITY_KL_DIVERGENCES, "query_entity_kl_divergences");
	EmplaceNodeTypeString(ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS, "query_entity_cumulative_nearest_entity_weights");

	//entity access
	EmplaceNodeTypeString(ENT_CONTAINS_LABEL, "contains_label");
	EmplaceNodeTypeString(ENT_ASSIGN_TO_ENTITIES, "assign_to_entities");
	EmplaceNodeTypeString(ENT_DIRECT_ASSIGN_TO_ENTITIES, "direct_assign_to_entities");
	EmplaceNodeTypeString(ENT_ACCUM_TO_ENTITIES, "accum_to_entities");
	EmplaceNodeTypeString(ENT_RETRIEVE_FROM_ENTITY, "retrieve_from_entity");
	EmplaceNodeTypeString(ENT_DIRECT_RETRIEVE_FROM_ENTITY, "direct_retrieve_from_entity");
	EmplaceNodeTypeString(ENT_CALL_ENTITY, "call_entity");
	EmplaceNodeTypeString(ENT_CALL_ENTITY_GET_CHANGES, "call_entity_get_changes");
	EmplaceNodeTypeString(ENT_CALL_CONTAINER, "call_container");

	//end opcodes

	//built-in common values
	EmplaceStaticString(ENBISI_true, ".true");
	EmplaceStaticString(ENBISI_false, ".false");
	EmplaceStaticString(ENBISI_infinity, ".infinity");
	EmplaceStaticString(ENBISI_neg_infinity, "-.infinity");
	EmplaceStaticString(ENBISI_zero, "0");
	EmplaceStaticString(ENBISI_one, "1");
	EmplaceStaticString(ENBISI_one_half, "0.5");
	EmplaceStaticString(ENBISI_two, "2");
	EmplaceStaticString(ENBISI_three, "3");
	EmplaceStaticString(ENBISI_four, "4");
	EmplaceStaticString(ENBISI_five, "5");
	EmplaceStaticString(ENBISI_six, "6");
	EmplaceStaticString(ENBISI_seven, "7");
	EmplaceStaticString(ENBISI_eight, "8");
	EmplaceStaticString(ENBISI_nine, "9");
	EmplaceStaticString(ENBISI_ten, "10");
	EmplaceStaticString(ENBISI_eleven, "11");
	EmplaceStaticString(ENBISI_twelve, "12");
	EmplaceStaticString(ENBISI_neg_one, "-1");
	EmplaceStaticString(ENBISI_neg_two, "-2");
	EmplaceStaticString(ENBISI_zero_number_key, std::string("\0" "0", 2));
	EmplaceStaticString(ENBISI_one_half_number_key, std::string("\0" "0.5", 4));
	EmplaceStaticString(ENBISI_one_number_key, std::string("\0" "1", 2));
	EmplaceStaticString(ENBISI_two_number_key, std::string("\0" "2", 2));
	EmplaceStaticString(ENBISI_three_number_key, std::string("\0" "3", 2));
	EmplaceStaticString(ENBISI_four_number_key, std::string("\0" "4", 2));
	EmplaceStaticString(ENBISI_five_number_key, std::string("\0" "5", 2));
	EmplaceStaticString(ENBISI_six_number_key, std::string("\0" "6", 2));
	EmplaceStaticString(ENBISI_seven_number_key, std::string("\0" "7", 2));
	EmplaceStaticString(ENBISI_eight_number_key, std::string("\0" "8", 2));
	EmplaceStaticString(ENBISI_ten_number_key, std::string("\0" "10", 3));
	EmplaceStaticString(ENBISI_eleven_number_key, std::string("\0" "11", 3));
	EmplaceStaticString(ENBISI_twelve_number_key, std::string("\0" "12", 3));
	EmplaceStaticString(ENBISI_neg_one_number_key, std::string("\0" "-1", 3));
	EmplaceStaticString(ENBISI_neg_two_number_key, std::string("\0" "-2", 3));
	EmplaceStaticString(ENBISI_empty_null, "(null)");
	EmplaceStaticString(ENBISI_empty_list, "(list)");
	EmplaceStaticString(ENBISI_empty_assoc, "(assoc)");
	EmplaceStaticString(ENBISI_null_key, std::string("\0" "(null)", 7));
	EmplaceStaticString(ENBISI_true_key, std::string("\0" ".true", 6));
	EmplaceStaticString(ENBISI_false_key, std::string("\0" ".false", 7));

	//config file parameters
	EmplaceStaticString(ENBISI_rand_seed, "rand_seed");
	EmplaceStaticString(ENBISI_version, "version");

	//file storage options
	EmplaceStaticString(ENBISI_include_rand_seeds, "include_rand_seeds");
	EmplaceStaticString(ENBISI_parallel_create, "parallel_create");
	EmplaceStaticString(ENBISI_escape_resource_name, "escape_resource_name");
	EmplaceStaticString(ENBISI_escape_contained_resource_names, "escape_contained_resource_names");
	EmplaceStaticString(ENBISI_transactional, "transactional");
	EmplaceStaticString(ENBISI_pretty_print, "pretty_print");
	EmplaceStaticString(ENBISI_flatten, "flatten");
	EmplaceStaticString(ENBISI_execute_on_load, "execute_on_load");

	//substr parameters
	EmplaceStaticString(ENBISI_all, "all");
	EmplaceStaticString(ENBISI_submatches, "submatches");

	//dynamically generated function parameters
	EmplaceStaticString(ENBISI__, "_");
	EmplaceStaticString(ENBISI_create_new_entity, "create_new_entity");
	EmplaceStaticString(ENBISI_new_entity, "new_entity");
	EmplaceStaticString(ENBISI_require_version_compatibility, "require_version_compatibility");
	EmplaceStaticString(ENBISI_amlg_version, "amlg_version");
	EmplaceStaticString(ENBISI_version_compatible, "version_compatible");

	//entity access parameters
	EmplaceStaticString(ENBISI_accessing_entity, "accessing_entity");

	//entity permissions
	EmplaceStaticString(ENBISI_std_out_and_std_err, "std_out_and_std_err");
	EmplaceStaticString(ENBISI_std_in, "std_in");
	EmplaceStaticString(ENBISI_load, "load");
	EmplaceStaticString(ENBISI_store, "store");
	EmplaceStaticString(ENBISI_environment, "environment");
	EmplaceStaticString(ENBISI_alter_performance, "alter_performance");
	EmplaceStaticString(ENBISI_system, "system");

	//distance types
	EmplaceStaticString(ENBISI_nominal_bool, "nominal_bool");
	EmplaceStaticString(ENBISI_nominal_number, "nominal_number");
	EmplaceStaticString(ENBISI_nominal_string, "nominal_string");
	EmplaceStaticString(ENBISI_nominal_code, "nominal_code");
	EmplaceStaticString(ENBISI_continuous_number, "continuous_number");
	EmplaceStaticString(ENBISI_continuous_number_cyclic, "continuous_number_cyclic");
	EmplaceStaticString(ENBISI_continuous_string, "continuous_string");
	EmplaceStaticString(ENBISI_continuous_code_no_recursive_matching, "continuous_code_no_recursive_matching");
	EmplaceStaticString(ENBISI_continuous_code, "continuous_code");

	//distance parameter values
	EmplaceStaticString(ENBISI_surprisal, "surprisal");
	EmplaceStaticString(ENBISI_surprisal_to_prob, "surprisal_to_prob");

	//numerical precision types
	EmplaceStaticString(ENBISI_precise, "precise");
	EmplaceStaticString(ENBISI_fast, "fast");
	EmplaceStaticString(ENBISI_recompute_precise, "recompute_precise");

	//format opcode types
	EmplaceStaticString(ENBISI_code, "code");
	EmplaceStaticString(ENBISI_base16, "base16");
	EmplaceStaticString(ENBISI_base64, "base64");
	EmplaceStaticString(ENBISI_int8, "int8");
	EmplaceStaticString(ENBISI_uint8, "uint8");
	EmplaceStaticString(ENBISI_int16, "int16");
	EmplaceStaticString(ENBISI_uint16, "uint16");
	EmplaceStaticString(ENBISI_int32, "int32");
	EmplaceStaticString(ENBISI_uint32, "uint32");
	EmplaceStaticString(ENBISI_int64, "int64");
	EmplaceStaticString(ENBISI_uint64, "uint64");
	EmplaceStaticString(ENBISI_float32, "float32");
	EmplaceStaticString(ENBISI_float64, "float64");
	EmplaceStaticString(ENBISI_gt_int8, ">int8");
	EmplaceStaticString(ENBISI_gt_uint8, ">uint8");
	EmplaceStaticString(ENBISI_gt_int16, ">int16");
	EmplaceStaticString(ENBISI_gt_uint16, ">uint16");
	EmplaceStaticString(ENBISI_gt_int32, ">int32");
	EmplaceStaticString(ENBISI_gt_uint32, ">uint32");
	EmplaceStaticString(ENBISI_gt_int64, ">int64");
	EmplaceStaticString(ENBISI_gt_uint64, ">uint64");
	EmplaceStaticString(ENBISI_gt_float32, ">float32");
	EmplaceStaticString(ENBISI_gt_float64, ">float64");
	EmplaceStaticString(ENBISI_lt_int8, "<int8");
	EmplaceStaticString(ENBISI_lt_uint8, "<uint8");
	EmplaceStaticString(ENBISI_lt_int16, "<int16");
	EmplaceStaticString(ENBISI_lt_uint16, "<uint16");
	EmplaceStaticString(ENBISI_lt_int32, "<int32");
	EmplaceStaticString(ENBISI_lt_uint32, "<uint32");
	EmplaceStaticString(ENBISI_lt_int64, "<int64");
	EmplaceStaticString(ENBISI_lt_uint64, "<uint64");
	EmplaceStaticString(ENBISI_lt_float32, "<float32");
	EmplaceStaticString(ENBISI_lt_float64, "<float64");
	EmplaceStaticString(ENBISI_json, "json");
	EmplaceStaticString(ENBISI_yaml, "yaml");

	//format opcode params
	EmplaceStaticString(ENBISI_sort_keys, "sort_keys");
	EmplaceStaticString(ENBISI_locale, "locale");
	EmplaceStaticString(ENBISI_time_zone, "time_zone");

	//mutate opcode mutation types
	EmplaceStaticString(ENBISI_change_type, "change_type");
	EmplaceStaticString(ENBISI_delete, "delete");
	EmplaceStaticString(ENBISI_insert, "insert");
	EmplaceStaticString(ENBISI_swap_elements, "swap_elements");
	EmplaceStaticString(ENBISI_deep_copy_elements, "deep_copy_elements");
	EmplaceStaticString(ENBISI_delete_elements, "delete_elements");
	EmplaceStaticString(ENBISI_change_label, "change_label");
}
