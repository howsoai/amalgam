//project headers:
#include "Interpreter.h"

#include "AmalgamVersion.h"
#include "AssetManager.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EntityQueryBuilder.h"
#include "EvaluableNodeTreeDifference.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"
#include "PerformanceProfiler.h"
#include "StringInternPool.h"

//system headers:
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <utility>

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
	std::vector<EntityQueryCondition> Interpreter::conditionsBuffer;

std::array<Interpreter::OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> Interpreter::_opcodes = {
	
	//built-in / system specific
	&Interpreter::InterpretNode_ENT_SYSTEM,															// ENT_SYSTEM
	&Interpreter::InterpretNode_ENT_GET_DEFAULTS,													// ENT_GET_DEFAULTS

	//parsing
	&Interpreter::InterpretNode_ENT_PARSE,															// ENT_PARSE
	&Interpreter::InterpretNode_ENT_UNPARSE,														// ENT_UNPARSE
	
	//core control
	&Interpreter::InterpretNode_ENT_IF,																// ENT_IF
	&Interpreter::InterpretNode_ENT_SEQUENCE,														// ENT_SEQUENCE
	&Interpreter::InterpretNode_ENT_PARALLEL,														// ENT_PARALLEL
	&Interpreter::InterpretNode_ENT_LAMBDA,															// ENT_LAMBDA
	&Interpreter::InterpretNode_ENT_CONCLUDE,														// ENT_CONCLUDE
	&Interpreter::InterpretNode_ENT_CALL,															// ENT_CALL
	&Interpreter::InterpretNode_ENT_CALL_SANDBOXED,													// ENT_CALL_SANDBOXED
	&Interpreter::InterpretNode_ENT_WHILE,															// ENT_WHILE

	//definitions
	&Interpreter::InterpretNode_ENT_LET,															// ENT_LET
	&Interpreter::InterpretNode_ENT_DECLARE,														// ENT_DECLARE
	&Interpreter::InterpretNode_ENT_ASSIGN_and_ACCUM,												// ENT_ASSIGN
	&Interpreter::InterpretNode_ENT_ASSIGN_and_ACCUM,												// ENT_ACCUM

	//retrieval
	&Interpreter::InterpretNode_ENT_RETRIEVE,														// ENT_RETRIEVE
	&Interpreter::InterpretNode_ENT_GET,															// ENT_GET
	&Interpreter::InterpretNode_ENT_SET_and_REPLACE,												// ENT_SET
	&Interpreter::InterpretNode_ENT_SET_and_REPLACE,												// ENT_REPLACE
	
	//stack and node manipulation
	&Interpreter::InterpretNode_ENT_TARGET,															// ENT_TARGET
	&Interpreter::InterpretNode_ENT_CURRENT_INDEX,													// ENT_CURRENT_INDEX
	&Interpreter::InterpretNode_ENT_CURRENT_VALUE,													// ENT_CURRENT_VALUE
	&Interpreter::InterpretNode_ENT_PREVIOUS_RESULT,												// ENT_PREVIOUS_RESULT
	&Interpreter::InterpretNode_ENT_STACK,															// ENT_STACK
	&Interpreter::InterpretNode_ENT_ARGS,															// ENT_ARGS

	//simulation and operations
	&Interpreter::InterpretNode_ENT_RAND,															// ENT_RAND
	&Interpreter::InterpretNode_ENT_WEIGHTED_RAND,													// ENT_WEIGHTED_RAND
	&Interpreter::InterpretNode_ENT_GET_RAND_SEED,													// ENT_GET_RAND_SEED
	&Interpreter::InterpretNode_ENT_SET_RAND_SEED,													// ENT_SET_RAND_SEED
	&Interpreter::InterpretNode_ENT_SYSTEM_TIME,													// ENT_SYSTEM_TIME

	//base math
	&Interpreter::InterpretNode_ENT_ADD,															// ENT_ADD
	&Interpreter::InterpretNode_ENT_SUBTRACT,														// ENT_SUBTRACT
	&Interpreter::InterpretNode_ENT_MULTIPLY,														// ENT_MULTIPLY
	&Interpreter::InterpretNode_ENT_DIVIDE,															// ENT_DIVIDE
	&Interpreter::InterpretNode_ENT_MODULUS,														// ENT_MODULUS
	&Interpreter::InterpretNode_ENT_GET_DIGITS,														// ENT_GET_DIGITS
	&Interpreter::InterpretNode_ENT_SET_DIGITS,														// ENT_SET_DIGITS
	&Interpreter::InterpretNode_ENT_FLOOR,															// ENT_FLOOR
	&Interpreter::InterpretNode_ENT_CEILING,														// ENT_CEILING
	&Interpreter::InterpretNode_ENT_ROUND,															// ENT_ROUND

	//extended math
	&Interpreter::InterpretNode_ENT_EXPONENT,														// ENT_EXPONENT
	&Interpreter::InterpretNode_ENT_LOG,															// ENT_LOG
	&Interpreter::InterpretNode_ENT_SIN,															// ENT_SIN
	&Interpreter::InterpretNode_ENT_ASIN,															// ENT_ASIN
	&Interpreter::InterpretNode_ENT_COS,															// ENT_COS
	&Interpreter::InterpretNode_ENT_ACOS,															// ENT_ACOS
	&Interpreter::InterpretNode_ENT_TAN,															// ENT_TAN
	&Interpreter::InterpretNode_ENT_ATAN,															// ENT_ATAN
	&Interpreter::InterpretNode_ENT_SINH,															// ENT_SINH
	&Interpreter::InterpretNode_ENT_ASINH,															// ENT_ASINH
	&Interpreter::InterpretNode_ENT_COSH,															// ENT_COSH
	&Interpreter::InterpretNode_ENT_ACOSH,															// ENT_ACOSH
	&Interpreter::InterpretNode_ENT_TANH,															// ENT_TANH
	&Interpreter::InterpretNode_ENT_ATANH,															// ENT_ATANH
	&Interpreter::InterpretNode_ENT_ERF,															// ENT_ERF
	&Interpreter::InterpretNode_ENT_TGAMMA,															// ENT_TGAMMA
	&Interpreter::InterpretNode_ENT_LGAMMA,															// ENT_LGAMMA
	&Interpreter::InterpretNode_ENT_SQRT,															// ENT_SQRT
	&Interpreter::InterpretNode_ENT_POW,															// ENT_POW
	&Interpreter::InterpretNode_ENT_ABS,															// ENT_ABS
	&Interpreter::InterpretNode_ENT_MAX,															// ENT_MAX
	&Interpreter::InterpretNode_ENT_MIN,															// ENT_MIN
	&Interpreter::InterpretNode_ENT_DOT_PRODUCT,													// ENT_DOT_PRODUCT
	&Interpreter::InterpretNode_ENT_GENERALIZED_DISTANCE,											// ENT_GENERALIZED_DISTANCE
	&Interpreter::InterpretNode_ENT_ENTROPY,														// ENT_ENTROPY

	//list manipulation
	&Interpreter::InterpretNode_ENT_FIRST,															// ENT_FIRST
	&Interpreter::InterpretNode_ENT_TAIL,															// ENT_TAIL
	&Interpreter::InterpretNode_ENT_LAST,															// ENT_LAST
	&Interpreter::InterpretNode_ENT_TRUNC,															// ENT_TRUNC
	&Interpreter::InterpretNode_ENT_APPEND,															// ENT_APPEND
	&Interpreter::InterpretNode_ENT_SIZE,															// ENT_SIZE
	&Interpreter::InterpretNode_ENT_RANGE,															// ENT_RANGE

	//transformation
	&Interpreter::InterpretNode_ENT_REWRITE,														// ENT_REWRITE
	&Interpreter::InterpretNode_ENT_MAP,															// ENT_MAP
	&Interpreter::InterpretNode_ENT_FILTER,															// ENT_FILTER
	&Interpreter::InterpretNode_ENT_WEAVE,															// ENT_WEAVE
	&Interpreter::InterpretNode_ENT_REDUCE,															// ENT_REDUCE
	&Interpreter::InterpretNode_ENT_APPLY,															// ENT_APPLY
	&Interpreter::InterpretNode_ENT_REVERSE,														// ENT_REVERSE
	&Interpreter::InterpretNode_ENT_SORT,															// ENT_SORT

	//associative list manipulation
	&Interpreter::InterpretNode_ENT_INDICES,														// ENT_INDICES
	&Interpreter::InterpretNode_ENT_VALUES,															// ENT_VALUES
	&Interpreter::InterpretNode_ENT_CONTAINS_INDEX,													// ENT_CONTAINS_INDEX
	&Interpreter::InterpretNode_ENT_CONTAINS_VALUE,													// ENT_CONTAINS_VALUE
	&Interpreter::InterpretNode_ENT_REMOVE,															// ENT_REMOVE
	&Interpreter::InterpretNode_ENT_KEEP,															// ENT_KEEP
	&Interpreter::InterpretNode_ENT_ASSOCIATE,														// ENT_ASSOCIATE
	&Interpreter::InterpretNode_ENT_ZIP,															// ENT_ZIP
	&Interpreter::InterpretNode_ENT_UNZIP,															// ENT_UNZIP

	//logic
	&Interpreter::InterpretNode_ENT_AND,															// ENT_AND
	&Interpreter::InterpretNode_ENT_OR,																// ENT_OR
	&Interpreter::InterpretNode_ENT_XOR,															// ENT_XOR
	&Interpreter::InterpretNode_ENT_NOT,															// ENT_NOT

	//equivalence
	&Interpreter::InterpretNode_ENT_EQUAL,															// ENT_EQUAL
	&Interpreter::InterpretNode_ENT_NEQUAL,															// ENT_NEQUAL
	&Interpreter::InterpretNode_ENT_LESS_and_LEQUAL,												// ENT_LESS
	&Interpreter::InterpretNode_ENT_LESS_and_LEQUAL,												// ENT_LEQUAL
	&Interpreter::InterpretNode_ENT_GREATER_and_GEQUAL,												// ENT_GREATER
	&Interpreter::InterpretNode_ENT_GREATER_and_GEQUAL,												// ENT_GEQUAL
	&Interpreter::InterpretNode_ENT_TYPE_EQUALS,													// ENT_TYPE_EQUALS
	&Interpreter::InterpretNode_ENT_TYPE_NEQUALS,													// ENT_TYPE_NEQUALS

	//built-in constants and variables
	&Interpreter::InterpretNode_ENT_TRUE,															// ENT_TRUE
	&Interpreter::InterpretNode_ENT_FALSE,															// ENT_FALSE
	&Interpreter::InterpretNode_ENT_NULL,															// ENT_NULL

	//data types
	&Interpreter::InterpretNode_ENT_LIST,															// ENT_LIST
	&Interpreter::InterpretNode_ENT_ASSOC,															// ENT_ASSOC
	&Interpreter::InterpretNode_ENT_NUMBER,															// ENT_NUMBER
	&Interpreter::InterpretNode_ENT_STRING,															// ENT_STRING
	&Interpreter::InterpretNode_ENT_SYMBOL,															// ENT_SYMBOL

	//node types
	&Interpreter::InterpretNode_ENT_GET_TYPE,														// ENT_GET_TYPE
	&Interpreter::InterpretNode_ENT_GET_TYPE_STRING,												// ENT_GET_TYPE_STRING
	&Interpreter::InterpretNode_ENT_SET_TYPE,														// ENT_SET_TYPE
	&Interpreter::InterpretNode_ENT_FORMAT,															// ENT_FORMAT

	//EvaluableNode management: labels, comments, and concurrency
	&Interpreter::InterpretNode_ENT_GET_LABELS,														// ENT_GET_LABELS
	&Interpreter::InterpretNode_ENT_GET_ALL_LABELS,													// ENT_GET_ALL_LABELS
	&Interpreter::InterpretNode_ENT_SET_LABELS,														// ENT_SET_LABELS
	&Interpreter::InterpretNode_ENT_ZIP_LABELS,														// ENT_ZIP_LABELS
	&Interpreter::InterpretNode_ENT_GET_COMMENTS,													// ENT_GET_COMMENTS
	&Interpreter::InterpretNode_ENT_SET_COMMENTS,													// ENT_SET_COMMENTS
	&Interpreter::InterpretNode_ENT_GET_CONCURRENCY,												// ENT_GET_CONCURRENCY
	&Interpreter::InterpretNode_ENT_SET_CONCURRENCY,												// ENT_SET_CONCURRENCY
	&Interpreter::InterpretNode_ENT_GET_VALUE,														// ENT_GET_VALUE
	&Interpreter::InterpretNode_ENT_SET_VALUE,														// ENT_SET_VALUE

	//string
	&Interpreter::InterpretNode_ENT_EXPLODE,														// ENT_EXPLODE
	&Interpreter::InterpretNode_ENT_SPLIT,															// ENT_SPLIT
	&Interpreter::InterpretNode_ENT_SUBSTR,															// ENT_SUBSTR
	&Interpreter::InterpretNode_ENT_CONCAT,															// ENT_CONCAT

	//encryption
	&Interpreter::InterpretNode_ENT_CRYPTO_SIGN,													// ENT_CRYPTO_SIGN
	&Interpreter::InterpretNode_ENT_CRYPTO_SIGN_VERIFY,												// ENT_CRYPTO_SIGN_VERIFY
	&Interpreter::InterpretNode_ENT_ENCRYPT,														// ENT_ENCRYPT
	&Interpreter::InterpretNode_ENT_DECRYPT,														// ENT_DECRYPT

	//I/O
	&Interpreter::InterpretNode_ENT_PRINT,															// ENT_PRINT

	//tree merging
	&Interpreter::InterpretNode_ENT_TOTAL_SIZE,														// ENT_TOTAL_SIZE
	&Interpreter::InterpretNode_ENT_MUTATE,															// ENT_MUTATE
	&Interpreter::InterpretNode_ENT_COMMONALITY,													// ENT_COMMONALITY
	&Interpreter::InterpretNode_ENT_EDIT_DISTANCE,													// ENT_EDIT_DISTANCE
	&Interpreter::InterpretNode_ENT_INTERSECT,														// ENT_INTERSECT
	&Interpreter::InterpretNode_ENT_UNION,															// ENT_UNION
	&Interpreter::InterpretNode_ENT_DIFFERENCE,														// ENT_DIFFERENCE
	&Interpreter::InterpretNode_ENT_MIX,															// ENT_MIX
	&Interpreter::InterpretNode_ENT_MIX_LABELS,														// ENT_MIX_LABELS

	//entity merging
	&Interpreter::InterpretNode_ENT_TOTAL_ENTITY_SIZE,												// ENT_TOTAL_ENTITY_SIZE
	&Interpreter::InterpretNode_ENT_FLATTEN_ENTITY,													// ENT_FLATTEN_ENTITY
	&Interpreter::InterpretNode_ENT_MUTATE_ENTITY,													// ENT_MUTATE_ENTITY
	&Interpreter::InterpretNode_ENT_COMMONALITY_ENTITIES,											// ENT_COMMONALITY_ENTITIES
	&Interpreter::InterpretNode_ENT_EDIT_DISTANCE_ENTITIES,											// ENT_EDIT_DISTANCE_ENTITIES
	&Interpreter::InterpretNode_ENT_INTERSECT_ENTITIES,												// ENT_INTERSECT_ENTITIES
	&Interpreter::InterpretNode_ENT_UNION_ENTITIES,													// ENT_UNION_ENTITIES
	&Interpreter::InterpretNode_ENT_DIFFERENCE_ENTITIES,											// ENT_DIFFERENCE_ENTITIES
	&Interpreter::InterpretNode_ENT_MIX_ENTITIES,													// ENT_MIX_ENTITIES

	//entity details
	&Interpreter::InterpretNode_ENT_GET_ENTITY_COMMENTS,											// ENT_GET_ENTITY_COMMENTS
	&Interpreter::InterpretNode_ENT_RETRIEVE_ENTITY_ROOT,											// ENT_RETRIEVE_ENTITY_ROOT
	&Interpreter::InterpretNode_ENT_ASSIGN_ENTITY_ROOTS_and_ACCUM_ENTITY_ROOTS,						// ENT_ASSIGN_ENTITY_ROOTS
	&Interpreter::InterpretNode_ENT_ASSIGN_ENTITY_ROOTS_and_ACCUM_ENTITY_ROOTS,						// ENT_ACCUM_ENTITY_ROOTS
	&Interpreter::InterpretNode_ENT_GET_ENTITY_RAND_SEED,											// ENT_GET_ENTITY_RAND_SEED
	&Interpreter::InterpretNode_ENT_SET_ENTITY_RAND_SEED,											// ENT_SET_ENTITY_RAND_SEED
	&Interpreter::InterpretNode_ENT_GET_ENTITY_ROOT_PERMISSION,										// ENT_GET_ENTITY_ROOT_PERMISSION
	&Interpreter::InterpretNode_ENT_SET_ENTITY_ROOT_PERMISSION,										// ENT_SET_ENTITY_ROOT_PERMISSION

	//entity base actions
	&Interpreter::InterpretNode_ENT_CREATE_ENTITIES,												// ENT_CREATE_ENTITIES
	&Interpreter::InterpretNode_ENT_CLONE_ENTITIES,													// ENT_CLONE_ENTITIES
	&Interpreter::InterpretNode_ENT_MOVE_ENTITIES,													// ENT_MOVE_ENTITIES
	&Interpreter::InterpretNode_ENT_DESTROY_ENTITIES,												// ENT_DESTROY_ENTITIES
	&Interpreter::InterpretNode_ENT_LOAD,															// ENT_LOAD
	&Interpreter::InterpretNode_ENT_LOAD_ENTITY_and_LOAD_PERSISTENT_ENTITY,							// ENT_LOAD_ENTITY
	&Interpreter::InterpretNode_ENT_LOAD_ENTITY_and_LOAD_PERSISTENT_ENTITY,							// ENT_LOAD_PERSIST
	&Interpreter::InterpretNode_ENT_STORE,															// ENT_STORE
	&Interpreter::InterpretNode_ENT_STORE_ENTITY,													// ENT_STORE_ENTITY
	&Interpreter::InterpretNode_ENT_CONTAINS_ENTITY,												// ENT_CONTAINS_ENTITY

	//entity query
	&Interpreter::InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES,			// ENT_CONTAINED_ENTITIES
	&Interpreter::InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES,			// ENT_COMPUTE_ON_CONTAINED_ENTITIES
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_SELECT
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_SAMPLE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_WEIGHTED_SAMPLE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_IN_ENTITY_LIST
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_NOT_IN_ENTITY_LIST
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_COUNT
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_EXISTS
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_NOT_EXISTS
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_EQUALS
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_NOT_EQUALS
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_BETWEEN
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_NOT_BETWEEN
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_AMONG
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_NOT_AMONG
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_MAX
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_MIN
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_SUM
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_MODE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_QUANTILE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_GENERALIZED_MEAN
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_MIN_DIFFERENCE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_MAX_DIFFERENCE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_VALUE_MASSES
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_GREATER_OR_EQUAL_TO
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_LESS_OR_EQUAL_TO
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_QUERY_NEAREST_GENERALIZED_DISTANCE

	//aggregate analysis query Functions
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_COMPUTE_ENTITY_CONVICTIONS
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS
	&Interpreter::InterpretNode_ENT_QUERY_and_COMPUTE_opcodes,										// ENT_COMPUTE_ENTITY_KL_DIVERGENCES

	//entity access
	&Interpreter::InterpretNode_ENT_CONTAINS_LABEL,													// ENT_CONTAINS_LABEL
	&Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_DIRECT_ASSIGN_TO_ENTITIES_and_ACCUM_TO_ENTITIES,	// ENT_ASSIGN_TO_ENTITIES
	&Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_DIRECT_ASSIGN_TO_ENTITIES_and_ACCUM_TO_ENTITIES,	// ENT_DIRECT_ASSIGN_TO_ENTITIES
	&Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_DIRECT_ASSIGN_TO_ENTITIES_and_ACCUM_TO_ENTITIES,	// ENT_ACCUM_TO_ENTITIES
	&Interpreter::InterpretNode_ENT_RETRIEVE_FROM_ENTITY_and_DIRECT_RETRIEVE_FROM_ENTITY,			// ENT_RETRIEVE_FROM_ENTITY
	&Interpreter::InterpretNode_ENT_RETRIEVE_FROM_ENTITY_and_DIRECT_RETRIEVE_FROM_ENTITY,			// ENT_DIRECT_RETRIEVE_FROM_ENTITY
	&Interpreter::InterpretNode_ENT_CALL_ENTITY_and_CALL_ENTITY_GET_CHANGES,						// ENT_CALL_ENTITY
	&Interpreter::InterpretNode_ENT_CALL_ENTITY_and_CALL_ENTITY_GET_CHANGES,						// ENT_CALL_ENTITY_GET_CHANGES
	&Interpreter::InterpretNode_ENT_CALL_CONTAINER,													// ENT_CALL_CONTAINER

	//not in active memory
	&Interpreter::InterpretNode_ENT_DEALLOCATED,													// ENT_DEALLOCATED
	&Interpreter::InterpretNode_ENT_DEALLOCATED,													// ENT_UNINITIALIZED

	//something went wrong - maximum value
	&Interpreter::InterpretNode_ENT_NOT_A_BUILT_IN_TYPE,											// ENT_NOT_A_BUILT_IN_TYPE
};


Interpreter::Interpreter(EvaluableNodeManager *enm,
	ExecutionCycleCount max_num_steps, size_t max_num_nodes, RandomStream rand_stream,
	std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
	Entity *t, Interpreter *calling_interpreter)
{
	curExecutionStep = 0;
	maxNumExecutionSteps = max_num_steps;

	//account for what is already in use
	curNumExecutionNodes = enm->GetNumberOfUsedNodes();
	curNumExecutionNodesAllocatedToEntities = 0;
	if(max_num_nodes == 0)
		maxNumExecutionNodes = 0;
	else
		maxNumExecutionNodes = max_num_nodes + enm->GetNumberOfUsedNodes();

	randomStream = rand_stream;
	curEntity = t;
	callingInterpreter = calling_interpreter;
	writeListeners = write_listeners;
	printListener = print_listener;

	callStackNodes = nullptr;
	interpreterNodeStackNodes = nullptr;
	constructionStackNodes = nullptr;

	evaluableNodeManager = enm;
}

#ifdef MULTITHREAD_SUPPORT
EvaluableNodeReference Interpreter::ExecuteNode(EvaluableNode *en,
	EvaluableNode *call_stack, EvaluableNode *interpreter_node_stack,
	EvaluableNode *construction_stack, std::vector<ConstructionStackIndexAndPreviousResultUniqueness> *construction_stack_indices,
	Concurrency::SingleMutex *call_stack_write_mutex)
#else
EvaluableNodeReference Interpreter::ExecuteNode(EvaluableNode *en,
	EvaluableNode *call_stack, EvaluableNode *interpreter_node_stack,
	EvaluableNode *construction_stack, std::vector<ConstructionStackIndexAndPreviousResultUniqueness> *construction_stack_indices)
#endif
{

#ifdef MULTITHREAD_SUPPORT
	if(call_stack == nullptr)
		callStackSharedAccessStartingDepth = 0;
	else
		callStackSharedAccessStartingDepth = call_stack->GetOrderedChildNodes().size();

	callStackWriteMutex = call_stack_write_mutex;
#endif

	//use specified or create new callStack
	if(call_stack == nullptr)
	{
		//create list of associative lists, and populate it with the top of the stack
		call_stack = evaluableNodeManager->AllocNode(ENT_LIST);

		EvaluableNode *new_context_entry = evaluableNodeManager->AllocNode(ENT_ASSOC);
		call_stack->AppendOrderedChildNode(new_context_entry);
	}

	if(interpreter_node_stack == nullptr)
		interpreter_node_stack = evaluableNodeManager->AllocNode(ENT_LIST);

	if(construction_stack == nullptr)
		construction_stack = evaluableNodeManager->AllocNode(ENT_LIST);

	callStackNodes = &call_stack->GetOrderedChildNodes();
	interpreterNodeStackNodes = &interpreter_node_stack->GetOrderedChildNodes();
	constructionStackNodes = &construction_stack->GetOrderedChildNodes();

	if(construction_stack_indices != nullptr)
		constructionStackIndicesAndUniqueness = *construction_stack_indices;

	//protect all of the stacks with needing cycle free checks
	// in case a node is added to one which isn't cycle free
	call_stack->SetNeedCycleCheck(true);
	for(auto &cn : call_stack->GetOrderedChildNodesReference())
		cn->SetNeedCycleCheck(true);
	interpreter_node_stack->SetNeedCycleCheck(true);
	construction_stack->SetNeedCycleCheck(true);

	//keep these references as long as the interpreter is around
	std::array<EvaluableNode *, 3> nodes_to_keep = { call_stack, interpreter_node_stack, construction_stack };
	evaluableNodeManager->KeepNodeReferences(nodes_to_keep);
	auto retval = InterpretNode(en);
	evaluableNodeManager->FreeNodeReferences(nodes_to_keep);

	//remove these nodes
	evaluableNodeManager->FreeNode(interpreter_node_stack);
	evaluableNodeManager->FreeNode(construction_stack);

	return retval;
}

EvaluableNodeReference Interpreter::ConvertArgsToCallStack(EvaluableNodeReference &args, EvaluableNodeManager *enm)
{
	if(enm == nullptr)
		return EvaluableNodeReference::Null();

	//ensure have arguments
	if(args == nullptr)
	{
		args.SetReference(enm->AllocNode(ENT_ASSOC), true);
	}
	else if(!args->IsAssociativeArray())
	{
		enm->FreeNodeTreeIfPossible(args);
		args.SetReference(enm->AllocNode(ENT_ASSOC), true);
	}
	else if(!args.unique)
	{
		args.SetReference(enm->AllocNode(args));
	}
	
	EvaluableNode *call_stack = enm->AllocNode(ENT_LIST);
	call_stack->AppendOrderedChildNode(args);

	return EvaluableNodeReference(call_stack, args.unique);
}

EvaluableNode **Interpreter::GetExecutionContextSymbolLocation(const StringInternPool::StringID symbol_sid, size_t &call_stack_index)
{
	//find symbol by walking up the stack; each layer must be an assoc
	for(call_stack_index = callStackNodes->size(); call_stack_index > 0; call_stack_index--)
	{
		EvaluableNode *cur_context = (*callStackNodes)[call_stack_index - 1];

		//see if this level of the stack contains the symbol
		auto &mcn = cur_context->GetMappedChildNodesReference();
		auto found = mcn.find(symbol_sid);
		if(found != end(mcn))
		{
			//subtract one here to match the subtraction above
			call_stack_index--;

			return &found->second;
		}
	}

	//didn't find it anywhere, so default it to the current top of the stack
	call_stack_index = callStackNodes->size() - 1;
	return nullptr;
}

EvaluableNode **Interpreter::GetOrCreateExecutionContextSymbolLocation(const StringInternPool::StringID symbol_sid, size_t &call_stack_index)
{
	//find appropriate context for symbol by walking up the stack
	for(call_stack_index = callStackNodes->size(); call_stack_index > 0; call_stack_index--)
	{
		EvaluableNode *cur_context = (*callStackNodes)[call_stack_index - 1];

		//see if this level of the stack contains the symbol
		auto &mcn = cur_context->GetMappedChildNodesReference();
		auto found = mcn.find(symbol_sid);
		if(found != end(mcn))
		{
			//subtract one here to match the subtraction above
			call_stack_index--;

			return &found->second;
		}
	}

	//didn't find it anywhere, so default it to the current top of the stack and create it
	call_stack_index = callStackNodes->size() - 1;
	EvaluableNode *context_to_use = (*callStackNodes)[call_stack_index];
	return context_to_use->GetOrCreateMappedChildNode(symbol_sid);
}

EvaluableNodeReference Interpreter::InterpretNode(EvaluableNode *en, bool immediate_result)
{
	if(EvaluableNode::IsNull(en))
		return EvaluableNodeReference::Null();

	//make sure don't run for longer than allowed
	if(!AllowUnlimitedExecutionSteps())
	{
		curExecutionStep++;
		if(curExecutionStep >= maxNumExecutionSteps)
			return EvaluableNodeReference::Null();
	}

	evaluableNodeManager->executionCyclesSinceLastGarbageCollection++;

	//reference this node before we collect garbage
	//CreateInterpreterNodeStackStateSaver is a bit expensive for this frequently called function
	//especially because only one node is kept
	interpreterNodeStackNodes->push_back(en);

	//for deep debugging only
	//ValidateEvaluableNodeIntegrity();

	CollectGarbage();

	//for deep debugging only
	//ValidateEvaluableNodeIntegrity();

	//make sure don't eat more memory than allowed
	if(!AllowUnlimitedExecutionNodes())
	{
		UpdateCurNumExecutionNodes();
		if(curNumExecutionNodes >= maxNumExecutionNodes)
			return EvaluableNodeReference::Null();
	}

	//get corresponding opcode
	EvaluableNodeType ent = en->GetType();
	auto oc = _opcodes[ent];

	EvaluableNodeReference retval = (this->*oc)(en, immediate_result);

	//for deep debugging only
	//ValidateEvaluableNodeIntegrity();

	//finished with opcode
	interpreterNodeStackNodes->pop_back();

	return retval;
}

EvaluableNode *Interpreter::GetCurrentExecutionContext()
{
	//this should not happen, but just in case
	if(callStackNodes->size() < 1)
		return nullptr;

	return callStackNodes->back();
}

std::pair<bool, std::string> Interpreter::InterpretNodeIntoStringValue(EvaluableNode *n)
{
	if(EvaluableNode::IsEmptyNode(n))
		return std::make_pair(false, "");

	//shortcut if the node has what is being asked
	if(n->GetType() == ENT_STRING)
		return std::make_pair(true, n->GetStringValue());

	auto result = InterpretNodeForImmediateUse(n);
	std::string result_string = EvaluableNode::ToString(result);
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return std::make_pair(true, result_string);
}

std::string Interpreter::InterpretNodeIntoStringValueEmptyNull(EvaluableNode *n)
{
	if(EvaluableNode::IsEmptyNode(n))
		return "";

	//shortcut if the node has what is being asked
	if(n->GetType() == ENT_STRING)
		return n->GetStringValue();

	auto result = InterpretNodeForImmediateUse(n);

	if(EvaluableNode::IsEmptyNode(result))
		return "";

	std::string result_string = EvaluableNode::ToString(result);
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return result_string;
}

StringInternPool::StringID Interpreter::InterpretNodeIntoStringIDValueIfExists(EvaluableNode *n)
{
	//shortcut if the node has what is being asked
	if(n != nullptr && n->GetType() == ENT_STRING)
		return n->GetStringID();

	auto result = InterpretNodeForImmediateUse(n);
	StringInternPool::StringID result_sid = EvaluableNode::ToStringIDIfExists(result);
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return result_sid;
}

StringInternPool::StringID Interpreter::InterpretNodeIntoStringIDValueWithReference(EvaluableNode *n)
{
	//shortcut if the node has what is being asked
	if(n != nullptr && n->GetType() == ENT_STRING)
		return string_intern_pool.CreateStringReference(n->GetStringID());

	auto result = InterpretNodeForImmediateUse(n);

	StringInternPool::StringID result_sid = string_intern_pool.NOT_A_STRING_ID;
	//if have a unique string, then just grab the string's reference instead of creating a new one
	if(result.unique)
	{
		if(result != nullptr && result->IsStringValue())
			result_sid = result->GetAndClearStringIDWithReference();
		else
			result_sid = EvaluableNode::ToStringIDWithReference(result);

		evaluableNodeManager->FreeNodeTree(result);
	}
	else //not unique, so can't free
	{
		result_sid = EvaluableNode::ToStringIDWithReference(result);
	}

	return result_sid;
}

EvaluableNode *Interpreter::InterpretNodeIntoUniqueStringIDValueEvaluableNode(EvaluableNode *n)
{
	//if can skip InterpretNode, then just allocate the string
	if(n == nullptr || n->GetIsIdempotent()
			|| n->GetType() == ENT_STRING || n->GetType() == ENT_NUMBER)
		return evaluableNodeManager->AllocNodeWithReferenceHandoff(ENT_STRING,
												EvaluableNode::ToStringIDWithReference(n));

	auto result = InterpretNode(n);

	if(result == nullptr || !result.unique)
		return evaluableNodeManager->AllocNodeWithReferenceHandoff(ENT_STRING,
												EvaluableNode::ToStringIDWithReference(result));

	result->ClearMetadata();

	if(result->GetType() != ENT_STRING)
		result->SetType(ENT_STRING, evaluableNodeManager);

	return result;
}

double Interpreter::InterpretNodeIntoNumberValue(EvaluableNode *n)
{
	if(n == nullptr)
		return std::numeric_limits<double>::quiet_NaN();

	auto type = n->GetType();

	//shortcut if the node has what is being asked
	if(type == ENT_NUMBER)
		return n->GetNumberValueReference();

	auto result = InterpretNodeForImmediateUse(n, true);
	auto &result_value = result.GetValue();

	double value = result_value.GetValueAsNumber();
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return value;
}

EvaluableNode *Interpreter::InterpretNodeIntoUniqueNumberValueEvaluableNode(EvaluableNode *n)
{
	if(n == nullptr || n->GetIsIdempotent())
		return evaluableNodeManager->AllocNode(EvaluableNode::ToNumber(n));

	auto result = InterpretNode(n);

	if(result == nullptr || !result.unique)
		return evaluableNodeManager->AllocNode(EvaluableNode::ToNumber(result));
	
	result->ClearMetadata();

	if(result->GetType() != ENT_NUMBER)
		result->SetType(ENT_NUMBER, evaluableNodeManager);

	return result;
}

bool Interpreter::InterpretNodeIntoBoolValue(EvaluableNode *n, bool value_if_null)
{
	//shortcut if the node has what is being asked
	if(n == nullptr)
		return value_if_null;

	auto result = InterpretNodeForImmediateUse(n);
	bool result_value = value_if_null;
	if(!EvaluableNode::IsNull(result))
		result_value = EvaluableNode::IsTrue(result);

	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return result_value;
}

void Interpreter::InterpretNodeIntoDestinationEntity(EvaluableNode *n, Entity *&destination_entity_parent, StringInternRef &new_entity_id)
{
	//TODO 10975: update this to lock
	EvaluableNodeReference new_entity_id_node = InterpretNodeForImmediateUse(n);
	TraverseEntityToNewDestinationViaEvaluableNodeIDPath(curEntity, new_entity_id_node, destination_entity_parent, new_entity_id);
	evaluableNodeManager->FreeNodeTreeIfPossible(new_entity_id_node);
}

EvaluableNode **Interpreter::TraverseToDestinationFromTraversalPathList(EvaluableNode **source, EvaluableNodeReference &tpl, bool create_destination_if_necessary)
{
	EvaluableNode **address_list;
	//default list length to 1
	size_t address_list_length = 1;

	//if it's an actual address list, then use it
	if(tpl != nullptr && DoesEvaluableNodeTypeUseOrderedData(tpl->GetType()))
	{
		auto &ocn = tpl->GetOrderedChildNodes();
		address_list = ocn.data();
		address_list_length = ocn.size();
	}
	else //it's only a single value; use default list length of 1
	{
		address_list = &tpl.GetReference();
	}

	size_t max_num_nodes = 0;
	if(!AllowUnlimitedExecutionNodes())
		max_num_nodes = (maxNumExecutionNodes - curNumExecutionNodes);

	EvaluableNode **destination = GetRelativeEvaluableNodeFromTraversalPathList(source, address_list, address_list_length, create_destination_if_necessary ? evaluableNodeManager : nullptr, max_num_nodes);

	return destination;
}

EvaluableNode *Interpreter::RewriteByFunction(EvaluableNodeReference function, EvaluableNode *top_node, EvaluableNode *n, EvaluableNode::ReferenceSetType &references)
{
	if(function == nullptr || n == nullptr)
		return nullptr;
	
	//try to record in references, but if already processed and exists, then return the existing value
	if(references.insert(n).second == false)
		return n;

	if(n->IsAssociativeArray())
	{
		PushNewConstructionContext(top_node, nullptr, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), n);

		for(auto &[e_id, e] : n->GetMappedChildNodesReference())
		{
			SetTopCurrentIndexInConstructionStack(e_id);
			SetTopCurrentValueInConstructionStack(e);
			e = RewriteByFunction(function, top_node, e, references);
		}

		PopConstructionContext();
	}
	else
	{
		auto &ocn = n->GetOrderedChildNodes();
		if(ocn.size() > 0)
		{
			PushNewConstructionContext(top_node, nullptr, EvaluableNodeImmediateValueWithType(0.0), n);

			//rewrite child nodes before rewriting this one
			for(size_t i = 0; i < ocn.size(); i++)
			{
				SetTopCurrentIndexInConstructionStack(static_cast<double>(i));
				SetTopCurrentValueInConstructionStack(ocn[i]);
				ocn[i] = RewriteByFunction(function, top_node, ocn[i], references);
			}

			PopConstructionContext();
		}
	}

	EvaluableNodeReference result = InterpretNode(function);
	//reuse the existing node since it has already been deepcopied
	n->CopyValueFrom(result);

	return result;
}

#ifdef MULTITHREAD_SUPPORT

bool Interpreter::InterpretEvaluableNodesConcurrently(EvaluableNode *parent_node, std::vector<EvaluableNode *> &nodes, std::vector<EvaluableNodeReference> &interpreted_nodes)
{
	if(!parent_node->GetConcurrency())
		return false;
	
	size_t num_tasks = nodes.size();
	if(num_tasks < 2)
		return false;

	auto enqueue_task_lock = Concurrency::threadPool.BeginEnqueueBatchTask();
	if(!enqueue_task_lock.AreThreadsAvailable())
		return false;

	ConcurrencyManager concurrency_manager(this, num_tasks);

	//kick off interpreters
	for(size_t task_index = 0; task_index < num_tasks; task_index++)
	{
		auto &interpreter = *concurrency_manager.interpreters[task_index];
		EvaluableNode *node_to_execute = nodes[task_index];

		concurrency_manager.resultFutures.emplace_back(
			Concurrency::threadPool.EnqueueBatchTask(
				[this, &interpreter, node_to_execute, &concurrency_manager]
				{
					interpreter.memoryModificationLock = Concurrency::ReadLock(interpreter.evaluableNodeManager->memoryModificationMutex);
					auto result = interpreter.ExecuteNode(node_to_execute,
						evaluableNodeManager->AllocListNode(callStackNodes),
						evaluableNodeManager->AllocListNode(interpreterNodeStackNodes),
						evaluableNodeManager->AllocListNode(constructionStackNodes),
						&constructionStackIndicesAndUniqueness,
						concurrency_manager.GetCallStackWriteMutex());

					evaluableNodeManager->KeepNodeReference(result);
					interpreter.memoryModificationLock.unlock();
					return result;
				}
			)
		);
	}

	enqueue_task_lock.Unlock();

	concurrency_manager.EndConcurrency();
	interpreted_nodes = concurrency_manager.GetResultsAndFreeReferences();
	return true;
}

#endif
