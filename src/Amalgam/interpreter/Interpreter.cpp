//project headers:
#include "Interpreter.h"

#include "AssetManager.h"
#include "EntityQueries.h"
#include "EntityQueryBuilder.h"
#include "EvaluableNodeTreeFunctions.h"
#include "PerformanceProfiler.h"
#include "StringInternPool.h"

//system headers:
#include <utility>

std::array<Interpreter::OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> Interpreter::_opcodes = {
	
	//built-in / system specific
	&Interpreter::InterpretNode_ENT_SYSTEM,															// ENT_SYSTEM
	&Interpreter::InterpretNode_ENT_GET_DEFAULTS,													// ENT_GET_DEFAULTS
	&Interpreter::InterpretNode_ENT_RECLAIM_RESOURCES,												// ENT_RECLAIM_RESOURCES

	//parsing
	&Interpreter::InterpretNode_ENT_PARSE,															// ENT_PARSE
	&Interpreter::InterpretNode_ENT_UNPARSE,														// ENT_UNPARSE
	
	//core control
	&Interpreter::InterpretNode_ENT_IF,																// ENT_IF
	&Interpreter::InterpretNode_ENT_SEQUENCE,														// ENT_SEQUENCE
	&Interpreter::InterpretNode_ENT_LAMBDA,															// ENT_LAMBDA
	&Interpreter::InterpretNode_ENT_CONCLUDE_and_RETURN,											// ENT_CONCLUDE
	&Interpreter::InterpretNode_ENT_CONCLUDE_and_RETURN,											// ENT_RETURN
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
	&Interpreter::InterpretNode_ENT_OPCODE_STACK,													// ENT_OPCODE_STACK
	&Interpreter::InterpretNode_ENT_STACK,															// ENT_STACK
	&Interpreter::InterpretNode_ENT_ARGS,															// ENT_ARGS

	//simulation and operations
	&Interpreter::InterpretNode_ENT_RAND,															// ENT_RAND
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
	&Interpreter::InterpretNode_ENT_INDEX_MAX,														// ENT_INDEX_MAX
	&Interpreter::InterpretNode_ENT_INDEX_MIN,														// ENT_INDEX_MIN
	&Interpreter::InterpretNode_ENT_DOT_PRODUCT,													// ENT_DOT_PRODUCT
	&Interpreter::InterpretNode_ENT_NORMALIZE,														// ENT_NORMALIZE
	&Interpreter::InterpretNode_ENT_MODE,															// ENT_MODE,
	&Interpreter::InterpretNode_ENT_QUANTILE,														// ENT_QUANTILE,
	&Interpreter::InterpretNode_ENT_GENERALIZED_MEAN,												// ENT_GENERALIZED_MEAN,
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
	&Interpreter::InterpretNode_ENT_NULL,															// ENT_NULL

	//data types
	&Interpreter::InterpretNode_ENT_LIST_and_UNORDERED_LIST,										// ENT_LIST
	&Interpreter::InterpretNode_ENT_LIST_and_UNORDERED_LIST,										// ENT_UNORDERED_LIST
	&Interpreter::InterpretNode_ENT_ASSOC,															// ENT_ASSOC
	&Interpreter::InterpretNode_ENT_BOOL,															// ENT_BOOL
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
	&Interpreter::InterpretNode_ENT_GET_ENTITY_PERMISSIONS,											// ENT_GET_ENTITY_PERMISSIONS
	&Interpreter::InterpretNode_ENT_SET_ENTITY_PERMISSIONS,											// ENT_SET_ENTITY_PERMISSIONS

	//entity base actions
	&Interpreter::InterpretNode_ENT_CREATE_ENTITIES,												// ENT_CREATE_ENTITIES
	&Interpreter::InterpretNode_ENT_CLONE_ENTITIES,													// ENT_CLONE_ENTITIES
	&Interpreter::InterpretNode_ENT_MOVE_ENTITIES,													// ENT_MOVE_ENTITIES
	&Interpreter::InterpretNode_ENT_DESTROY_ENTITIES,												// ENT_DESTROY_ENTITIES
	&Interpreter::InterpretNode_ENT_LOAD,															// ENT_LOAD
	&Interpreter::InterpretNode_ENT_LOAD_ENTITY,													// ENT_LOAD_ENTITY
	&Interpreter::InterpretNode_ENT_STORE,															// ENT_STORE
	&Interpreter::InterpretNode_ENT_STORE_ENTITY,													// ENT_STORE_ENTITY
	&Interpreter::InterpretNode_ENT_CONTAINS_ENTITY,												// ENT_CONTAINS_ENTITY

	//entity query
	&Interpreter::InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES,			// ENT_CONTAINED_ENTITIES
	&Interpreter::InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES,			// ENT_COMPUTE_ON_CONTAINED_ENTITIES
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_SELECT
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_SAMPLE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_IN_ENTITY_LIST
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_NOT_IN_ENTITY_LIST
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_EXISTS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_NOT_EXISTS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_EQUALS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_NOT_EQUALS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_BETWEEN
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_NOT_BETWEEN
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_AMONG
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_NOT_AMONG
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_MAX
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_MIN
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_SUM
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_MODE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_QUANTILE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_GENERALIZED_MEAN
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_MIN_DIFFERENCE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_MAX_DIFFERENCE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_VALUE_MASSES
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_GREATER_OR_EQUAL_TO
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_LESS_OR_EQUAL_TO
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_DISTANCE_CONTRIBUTIONS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_ENTITY_CONVICTIONS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_ENTITY_KL_DIVERGENCES
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
	&Interpreter::InterpretNode_ENT_QUERY_opcodes,													// ENT_QUERY_ENTITY_CLUSTERS

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


Interpreter::Interpreter(EvaluableNodeManager *enm, RandomStream rand_stream,
	std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
	InterpreterConstraints *interpreter_constraints, Entity *t, Interpreter *calling_interpreter)
{
	interpreterConstraints = interpreter_constraints;

	randomStream = rand_stream;
	curEntity = t;
	callingInterpreter = calling_interpreter;
	writeListeners = write_listeners;
	printListener = print_listener;

	scopeStackNodes = nullptr;
	opcodeStackNodes = nullptr;
	constructionStackNodes = nullptr;

	evaluableNodeManager = enm;
#ifdef MULTITHREAD_SUPPORT
	bottomOfScopeStack = true;
#endif
}

EvaluableNodeReference Interpreter::ExecuteNode(EvaluableNode *en,
	EvaluableNode *scope_stack, EvaluableNode *opcode_stack, EvaluableNode *construction_stack,
	bool manage_stack_references,
	std::vector<ConstructionStackIndexAndPreviousResultUniqueness> *construction_stack_indices,
	bool immediate_result
#ifdef MULTITHREAD_SUPPORT
	, bool new_scope_stack
#endif
	)
{
	//use specified or create new scopeStack
	if(scope_stack == nullptr)
	{
		//create list of associative lists, and populate it with the top of the stack
		scope_stack = evaluableNodeManager->AllocNode(ENT_LIST);
		scope_stack->SetNeedCycleCheck(true);

		EvaluableNode *new_context_entry = evaluableNodeManager->AllocNode(ENT_ASSOC);
		new_context_entry->SetNeedCycleCheck(true);
		scope_stack->AppendOrderedChildNode(new_context_entry);
	}

	if(opcode_stack == nullptr)
		opcode_stack = evaluableNodeManager->AllocNode(ENT_LIST);
	opcode_stack->SetNeedCycleCheck(true);
	
	if(construction_stack == nullptr)
		construction_stack = evaluableNodeManager->AllocNode(ENT_LIST);
	construction_stack->SetNeedCycleCheck(true);

	scopeStackNodes = &scope_stack->GetOrderedChildNodes();
	opcodeStackNodes = &opcode_stack->GetOrderedChildNodes();
	constructionStackNodes = &construction_stack->GetOrderedChildNodes();

#ifdef MULTITHREAD_SUPPORT
	bottomOfScopeStack = new_scope_stack;
#endif

	if(construction_stack_indices != nullptr)
		constructionStackIndicesAndUniqueness = *construction_stack_indices;
	
	if(manage_stack_references)
		evaluableNodeManager->KeepNodeReferences(scope_stack, opcode_stack, construction_stack);

	auto retval = InterpretNode(en, immediate_result);

	if(manage_stack_references)
		evaluableNodeManager->FreeNodeReferences(scope_stack, opcode_stack, construction_stack);

	return retval;
}

EvaluableNode *Interpreter::GetScopeStackGivenDepth(size_t depth)
{
	size_t ss_size = scopeStackNodes->size();
	if(ss_size > depth)
		return (*scopeStackNodes)[ss_size - (depth + 1)];

#ifdef MULTITHREAD_SUPPORT
	//need to search further down the stack if appropriate
	if(!bottomOfScopeStack && callingInterpreter != nullptr)
		return callingInterpreter->GetScopeStackGivenDepth(depth - ss_size);
#endif

	return nullptr;
}

EvaluableNode *Interpreter::MakeCopyOfScopeStack()
{
	EvaluableNode stack_top_holder(ENT_LIST);
	stack_top_holder.SetOrderedChildNodes(*scopeStackNodes);
	EvaluableNodeReference copied_stack = evaluableNodeManager->DeepAllocCopy(&stack_top_holder);

#ifdef MULTITHREAD_SUPPORT
	//copy the rest of the stack if there is more
	if(!bottomOfScopeStack)
	{
		auto &stack_nodes_ocn = copied_stack->GetOrderedChildNodesReference();
		for(Interpreter *interp = callingInterpreter; interp != nullptr; interp = interp->callingInterpreter)
		{
			stack_nodes_ocn.insert(begin(stack_nodes_ocn), scopeStackNodes->size(), nullptr);
			for(size_t i = 0; i < scopeStackNodes->size(); i++)
				stack_nodes_ocn[i] = evaluableNodeManager->DeepAllocCopy((*scopeStackNodes)[i]);

			if(interp->bottomOfScopeStack)
				break;
		}
	}
#endif

	return copied_stack;
}

EvaluableNodeReference Interpreter::ConvertArgsToScopeStack(EvaluableNodeReference &args, EvaluableNodeManager &enm)
{
	//ensure have arguments
	if(args == nullptr)
	{
		args.SetReference(enm.AllocNode(ENT_ASSOC), true);
	}
	else if(!args->IsAssociativeArray())
	{
		args.SetReference(enm.AllocNode(ENT_ASSOC), true);
	}
	else if(!args.unique)
	{
		args.SetReference(enm.AllocNode(args, EvaluableNodeManager::ENMM_REMOVE_ALL));
		args.uniqueUnreferencedTopNode = true;
	}
	
	EvaluableNode *scope_stack = enm.AllocNode(ENT_LIST);
	scope_stack->AppendOrderedChildNode(args);

	scope_stack->SetNeedCycleCheck(true);
	args->SetNeedCycleCheck(true);

	return EvaluableNodeReference(scope_stack, args.unique, true);
}

void Interpreter::SetSideEffectFlagsAndAccumulatePerformanceCounters(EvaluableNode *node)
{
	auto [any_constructions, initial_side_effect] = SetSideEffectsFlags();
	if(_opcode_profiling_enabled && any_constructions)
	{
		std::string variable_location = asset_manager.GetEvaluableNodeSourceFromComments(node);
		PerformanceProfiler::AccumulateTotalSideEffectMemoryWrites(variable_location);
		if(initial_side_effect)
			PerformanceProfiler::AccumulateInitialSideEffectMemoryWrites(variable_location);
	}
}

EvaluableNodeReference Interpreter::InterpretNode(EvaluableNode *en, bool immediate_result)
{
	if(EvaluableNode::IsNull(en))
		return EvaluableNodeReference::Null();

	//reference this node before we collect garbage
	//CreateOpcodeStackStateSaver is a bit expensive for this frequently called function
	//especially because only one node is kept
	opcodeStackNodes->push_back(en);

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	CollectGarbage();

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	if(AreExecutionResourcesExhausted(true))
	{
		opcodeStackNodes->pop_back();
		return EvaluableNodeReference::Null();
	}

	//get corresponding opcode
	EvaluableNodeType ent = en->GetType();
	auto oc = _opcodes[ent];

	EvaluableNodeReference retval = (this->*oc)(en, immediate_result);

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	//finished with opcode
	opcodeStackNodes->pop_back();

	return retval;
}

EvaluableNode *Interpreter::GetCurrentScopeStackContext()
{
	//this should not happen, but just in case
	if(scopeStackNodes->size() < 1)
		return nullptr;

	return scopeStackNodes->back();
}

std::pair<bool, std::string> Interpreter::InterpretNodeIntoStringValue(EvaluableNode *n, bool key_string)
{
	if(EvaluableNode::IsNull(n))
		return std::make_pair(false, "");

	//shortcut if the node has what is being asked
	if(n->GetType() == ENT_STRING)
		return std::make_pair(true, n->GetStringValue());

	auto result = InterpretNodeForImmediateUse(n, true);
	auto &result_value = result.GetValue();

	auto [valid, str] = result_value.GetValueAsString(key_string);
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return std::make_pair(valid, str);
}

StringInternPool::StringID Interpreter::InterpretNodeIntoStringIDValueIfExists(EvaluableNode *n, bool key_string)
{
	//shortcut if the node has what is being asked
	if(n != nullptr && n->GetType() == ENT_STRING)
		return n->GetStringID();

	auto result = InterpretNodeForImmediateUse(n, true);
	auto &result_value = result.GetValue();

	auto sid = result_value.GetValueAsStringIDIfExists(key_string);
	//ID already exists outside of this, so not expecting to keep this reference
	evaluableNodeManager->FreeNodeTreeIfPossible(result);
	return sid;
}

StringInternPool::StringID Interpreter::InterpretNodeIntoStringIDValueWithReference(EvaluableNode *n, bool key_string)
{
	//shortcut if the node has what is being asked
	if(n != nullptr && n->GetType() == ENT_STRING)
		return string_intern_pool.CreateStringReference(n->GetStringID());

	auto result = InterpretNodeForImmediateUse(n, true);

	if(result.IsImmediateValue())
	{
		auto &result_value = result.GetValue();

		//reuse the reference if it has one
		if(result_value.nodeType == ENIVT_STRING_ID)
			return result_value.nodeValue.stringID;

		//create new reference
		return result_value.GetValueAsStringIDWithReference(key_string);
	}
	else //not immediate
	{
		//if have a unique string, then just grab the string's reference instead of creating a new one
		if(result.unique)
		{
			StringInternPool::StringID result_sid = string_intern_pool.NOT_A_STRING_ID;
			if(result != nullptr && result->GetType() == ENT_STRING)
				result_sid = result->GetAndClearStringIDWithReference();
			else
				result_sid = EvaluableNode::ToStringIDWithReference(result, key_string);

			evaluableNodeManager->FreeNodeTree(result);
			return result_sid;
		}
		else //not unique, so can't free
		{
			return EvaluableNode::ToStringIDWithReference(result, key_string);
		}
	}
}

EvaluableNodeReference Interpreter::InterpretNodeIntoUniqueStringIDValueEvaluableNode(
	EvaluableNode *n, bool immediate_result)
{
	//if can skip InterpretNode, then just allocate the string
	if(n == nullptr || n->GetIsIdempotent()
		|| n->GetType() == ENT_STRING || n->GetType() == ENT_BOOL || n->GetType() == ENT_NUMBER)
	{
		auto sid = EvaluableNode::ToStringIDWithReference(n);

		if(immediate_result)
			return EvaluableNodeReference(sid, true);
		else
			return EvaluableNodeReference(evaluableNodeManager->AllocNodeWithReferenceHandoff(ENT_STRING,
				sid), true);
	}

	auto result = InterpretNode(n);

	if(result == nullptr || !result.unique)
		return EvaluableNodeReference(evaluableNodeManager->AllocNodeWithReferenceHandoff(ENT_STRING,
												EvaluableNode::ToStringIDWithReference(result)), true);

	result->ClearMetadata();

	auto type = result->GetType();
	if(type != ENT_STRING && type != ENT_NULL)
		result->SetType(ENT_STRING, evaluableNodeManager, true);

	return result;
}

double Interpreter::InterpretNodeIntoNumberValue(EvaluableNode *n)
{
	if(n != nullptr && n->GetType() == ENT_NUMBER)
		return n->GetNumberValueReference();

	auto result = InterpretNodeForImmediateUse(n, true);
	auto &result_value = result.GetValue();

	double value = result_value.GetValueAsNumber();
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return value;
}

EvaluableNodeReference Interpreter::InterpretNodeIntoUniqueNumberValueOrNullEvaluableNode(EvaluableNode *n)
{
	if(n == nullptr || n->GetIsIdempotent())
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(EvaluableNode::ToNumber(n)), true);

	auto result = InterpretNode(n);

	if(result == nullptr || !result.unique)
		return EvaluableNodeReference(evaluableNodeManager->AllocNode(EvaluableNode::ToNumber(result)), true);
	
	result->ClearMetadata();

	auto type = result->GetType();
	if(type != ENT_NUMBER && type != ENT_NULL)
		result->SetType(ENT_NUMBER, evaluableNodeManager, true);

	return result;
}

bool Interpreter::InterpretNodeIntoBoolValue(EvaluableNode *n, bool value_if_null)
{
	if(n != nullptr && n->GetType() == ENT_BOOL)
		return n->GetBoolValueReference();

	auto result = InterpretNodeForImmediateUse(n, true);
	auto &result_value = result.GetValue();

	bool value = result_value.GetValueAsBoolean(value_if_null);
	evaluableNodeManager->FreeNodeTreeIfPossible(result);

	return value;
}

std::pair<EntityWriteReference, StringRef> Interpreter::InterpretNodeIntoDestinationEntity(EvaluableNode *n)
{
	EvaluableNodeReference destination_entity_id_path = InterpretNodeForImmediateUse(n);

	StringRef new_entity_id;
	auto [entity, entity_container] = TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityWriteReference>(
			curEntity, destination_entity_id_path, &new_entity_id);

	evaluableNodeManager->FreeNodeTreeIfPossible(destination_entity_id_path);

	//if it already exists, then place inside it
	if(entity != nullptr)
		return std::make_pair(std::move(entity), StringRef());
	else //return the container
		return std::make_pair(std::move(entity_container), new_entity_id);
}

EvaluableNode **Interpreter::TraverseToDestinationFromTraversalPathList(EvaluableNode **source, EvaluableNodeReference &tpl, bool create_destination_if_necessary)
{
	EvaluableNode **address_list;
	//default list length to 1
	size_t address_list_length = 1;

	//if it's an actual address list, then use it
	if(!EvaluableNode::IsNull(tpl) && DoesEvaluableNodeTypeUseOrderedData(tpl->GetType()))
	{
		auto &ocn = tpl->GetOrderedChildNodesReference();
		address_list = ocn.data();
		address_list_length = ocn.size();
	}
	else //it's only a single value; use default list length of 1
	{
		address_list = &tpl.GetReference();
	}

	size_t max_num_nodes = 0;
	if(ConstrainedAllocatedNodes())
		max_num_nodes = interpreterConstraints->GetRemainingNumAllocatedNodes(evaluableNodeManager->GetNumberOfUsedNodes());

	EvaluableNode **destination = GetRelativeEvaluableNodeFromTraversalPathList(source, address_list, address_list_length, create_destination_if_necessary ? evaluableNodeManager : nullptr, max_num_nodes);

	return destination;
}

EvaluableNodeReference Interpreter::RewriteByFunction(EvaluableNodeReference function,
	EvaluableNode *tree, FastHashMap<EvaluableNode *, EvaluableNode *> &original_node_to_new_node)
{
	EvaluableNodeReference cur_node(tree, false);

	if(tree != nullptr)
	{
		//attempt to insert; if found, return previous value
		auto [existing_record, inserted] = original_node_to_new_node.emplace(tree, static_cast<EvaluableNode *>(nullptr));
		if(!inserted)
			return EvaluableNodeReference(existing_record->second, false);

		cur_node = EvaluableNodeReference(evaluableNodeManager->AllocNode(tree), true);
		existing_record->second = cur_node;

		if(cur_node->IsAssociativeArray())
		{
			PushNewConstructionContext(nullptr, cur_node, EvaluableNodeImmediateValueWithType(StringInternPool::NOT_A_STRING_ID), nullptr);

			for(auto &[e_id, e] : cur_node->GetMappedChildNodesReference())
			{
				SetTopCurrentIndexInConstructionStack(e_id);
				SetTopCurrentValueInConstructionStack(e);
				auto new_e = RewriteByFunction(function, e, original_node_to_new_node);

				cur_node.UpdatePropertiesBasedOnAttachedNode(new_e);
				e = new_e;
			}
			if(PopConstructionContextAndGetExecutionSideEffectFlag())
				cur_node->SetNeedCycleCheck(true);
		}
		else if(cur_node->IsOrderedArray())
		{
			auto &ocn = cur_node->GetOrderedChildNodesReference();
			if(ocn.size() > 0)
			{
				PushNewConstructionContext(nullptr, cur_node, EvaluableNodeImmediateValueWithType(0.0), nullptr);

				for(size_t i = 0; i < ocn.size(); i++)
				{
					SetTopCurrentIndexInConstructionStack(static_cast<double>(i));
					SetTopCurrentValueInConstructionStack(ocn[i]);
					auto new_e = RewriteByFunction(function, ocn[i], original_node_to_new_node);
					cur_node.UpdatePropertiesBasedOnAttachedNode(new_e);
					ocn[i] = new_e;
				}

				if(PopConstructionContextAndGetExecutionSideEffectFlag())
					cur_node->SetNeedCycleCheck(true);
			}
		}
	}

	SetTopCurrentValueInConstructionStack(cur_node);
	return InterpretNode(function);
}

bool Interpreter::PopulateInterpreterConstraintsFromParams(std::vector<EvaluableNode *> &params,
	size_t perf_constraint_param_offset, InterpreterConstraints &interpreter_constraints, bool include_entity_constraints)
{
	//start with constraints if there are already interpreter constraints
	bool any_constraints = (interpreterConstraints != nullptr);

	interpreter_constraints.constraintViolation = InterpreterConstraints::ViolationType::NoViolation;

	//for each of the three parameters below, values of zero indicate no limit

	//populate maxNumExecutionSteps
	interpreter_constraints.curExecutionStep = 0;
	interpreter_constraints.maxNumExecutionSteps = 0;
	size_t execution_steps_offset = perf_constraint_param_offset + 0;
	if(params.size() > execution_steps_offset)
	{
		double value = InterpretNodeIntoNumberValue(params[execution_steps_offset]);
		//nan will fail, so don't need a separate nan check
		if(value >= 1.0)
		{
			interpreter_constraints.maxNumExecutionSteps = static_cast<ExecutionCycleCount>(value);
			any_constraints = true;
		}
	}

	//populate maxNumAllocatedNodes
	interpreter_constraints.curNumAllocatedNodesAllocatedToEntities = 0;
	interpreter_constraints.maxNumAllocatedNodes = 0;
	size_t max_num_allocated_nodes_offset = perf_constraint_param_offset + 1;
	if(params.size() > max_num_allocated_nodes_offset)
	{
		double value = InterpretNodeIntoNumberValue(params[max_num_allocated_nodes_offset]);
		//nan will fail, so don't need a separate nan check
		if(value >= 1.0)
		{
			interpreter_constraints.maxNumAllocatedNodes = static_cast<ExecutionCycleCount>(value);
			any_constraints = true;
		}
	}
	//populate maxOpcodeExecutionDepth
	interpreter_constraints.maxOpcodeExecutionDepth = 0;
	size_t max_opcode_execution_depth_offset = perf_constraint_param_offset + 2;
	if(params.size() > max_opcode_execution_depth_offset)
	{
		double value = InterpretNodeIntoNumberValue(params[max_opcode_execution_depth_offset]);
		//nan will fail, so don't need a separate nan check
		if(value >= 1.0)
		{
			interpreter_constraints.maxOpcodeExecutionDepth = static_cast<ExecutionCycleCount>(value);
			any_constraints = true;
		}
	}

	interpreter_constraints.entityToConstrainFrom = nullptr;
	interpreter_constraints.constrainMaxContainedEntities = false;
	interpreter_constraints.maxContainedEntities = 0;
	interpreter_constraints.constrainMaxContainedEntityDepth = false;
	interpreter_constraints.maxContainedEntityDepth = 0;
	interpreter_constraints.maxEntityIdLength = 0;

	size_t warning_override_offset = perf_constraint_param_offset + 3;

	if(include_entity_constraints)
	{
		warning_override_offset += 3;

		//populate maxContainedEntities
		size_t max_contained_entities_offset = perf_constraint_param_offset + 3;
		if(params.size() > max_contained_entities_offset)
		{
			double value = InterpretNodeIntoNumberValue(params[max_contained_entities_offset]);
			//nan will fail, so don't need a separate nan check
			if(value >= 0.0)
			{
				interpreter_constraints.constrainMaxContainedEntities = true;
				interpreter_constraints.maxContainedEntities = static_cast<ExecutionCycleCount>(value);
				any_constraints = true;
			}
		}

		//populate maxContainedEntityDepth
		size_t max_contained_entity_depth_offset = perf_constraint_param_offset + 4;
		if(params.size() > max_contained_entity_depth_offset)
		{
			double value = InterpretNodeIntoNumberValue(params[max_contained_entity_depth_offset]);
			//nan will fail, so don't need a separate nan check
			if(value >= 0.0)
			{
				interpreter_constraints.constrainMaxContainedEntityDepth = true;
				interpreter_constraints.maxContainedEntityDepth = static_cast<ExecutionCycleCount>(value);
				any_constraints = true;
			}
		}

		//populate maxEntityIdLength
		size_t max_entity_id_length_offset = perf_constraint_param_offset + 5;
		if(params.size() > max_entity_id_length_offset)
		{
			double value = InterpretNodeIntoNumberValue(params[max_entity_id_length_offset]);
			//nan will fail, so don't need a separate nan check
			if(value >= 1.0)
			{
				interpreter_constraints.maxEntityIdLength = static_cast<ExecutionCycleCount>(value);
				any_constraints = true;
			}
		}
	}


	//check if caller specifed override of the default warning collections behavior
	if(params.size() > warning_override_offset)
	{
		interpreter_constraints.collectWarnings = InterpretNodeIntoBoolValue(params[warning_override_offset], any_constraints);
		any_constraints |= interpreter_constraints.collectWarnings;
	}
	else
	{
		interpreter_constraints.collectWarnings = any_constraints;
	}
	
	return any_constraints;
}

void Interpreter::PopulatePerformanceCounters(InterpreterConstraints *interpreter_constraints, Entity *entity_to_constrain_from)
{
	if(interpreter_constraints == nullptr)
		return;

	interpreter_constraints->constraintsExceeded = false;

	//handle execution steps
	if(interpreterConstraints != nullptr && interpreterConstraints->ConstrainedExecutionSteps())
	{
		ExecutionCycleCount remaining_steps = interpreterConstraints->GetRemainingNumExecutionSteps();
		if(remaining_steps > 0)
		{
			if(interpreter_constraints->ConstrainedExecutionSteps())
				interpreter_constraints->maxNumExecutionSteps = std::min(
					interpreter_constraints->maxNumExecutionSteps, remaining_steps);
			else
				interpreter_constraints->maxNumExecutionSteps = remaining_steps;
		}
		else //out of resources, ensure nothing will run (can't use 0 for maxNumExecutionSteps)
		{
			interpreter_constraints->maxNumExecutionSteps = 1;
			interpreter_constraints->curExecutionStep = 1;
			interpreter_constraints->constraintsExceeded = true;
			interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ExecutionStep;
		}
	}

	//handle allocated nodes
	if(interpreterConstraints != nullptr && interpreterConstraints->ConstrainedAllocatedNodes())
	{
		size_t remaining_allocs = interpreterConstraints->GetRemainingNumAllocatedNodes(
			evaluableNodeManager->GetNumberOfUsedNodes());
		if(remaining_allocs > 0)
		{
			if(interpreter_constraints->ConstrainedAllocatedNodes())
				interpreter_constraints->maxNumAllocatedNodes = std::min(
					interpreter_constraints->maxNumAllocatedNodes, remaining_allocs);
			else
				interpreter_constraints->maxNumAllocatedNodes = remaining_allocs;
		}
		else //out of resources, ensure nothing will run (can't use 0 for maxNumAllocatedNodes)
		{
			interpreter_constraints->maxNumAllocatedNodes = 1;
			interpreter_constraints->constraintsExceeded = true;
			interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::NodeAllocation;
		}
	}

	if(interpreter_constraints->ConstrainedAllocatedNodes())
	{
	#ifdef MULTITHREAD_SUPPORT
		//if multiple threads, the other threads could be eating into this
		interpreter_constraints->maxNumAllocatedNodes *= Concurrency::threadPool.GetNumActiveThreads();
	#endif

		//offset the max appropriately
		interpreter_constraints->maxNumAllocatedNodes += evaluableNodeManager->GetNumberOfUsedNodes();
	}

	//handle opcode execution depth
	if(interpreterConstraints != nullptr && interpreterConstraints->ConstrainedOpcodeExecutionDepth())
	{
		size_t remaining_depth = interpreterConstraints->GetRemainingOpcodeExecutionDepth(
			opcodeStackNodes->size());
		if(remaining_depth > 0)
		{
			if(interpreter_constraints->ConstrainedOpcodeExecutionDepth())
				interpreter_constraints->maxOpcodeExecutionDepth = std::min(
					interpreter_constraints->maxOpcodeExecutionDepth, remaining_depth);
			else
				interpreter_constraints->maxOpcodeExecutionDepth = remaining_depth;
		}
		else //out of resources, ensure nothing will run (can't use 0 for maxOpcodeExecutionDepth)
		{
			interpreter_constraints->maxOpcodeExecutionDepth = 1;
			interpreter_constraints->constraintsExceeded = true;
			interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ExecutionDepth;
		}
	}

	if(entity_to_constrain_from == nullptr)
		return;

	interpreter_constraints->entityToConstrainFrom = entity_to_constrain_from;

	if(interpreterConstraints != nullptr && interpreterConstraints->constrainMaxContainedEntities
		&& interpreterConstraints->entityToConstrainFrom != nullptr)
	{
		interpreter_constraints->constrainMaxContainedEntities = true;

		//if calling a contained entity, figure out how many this one can create
		size_t max_entities = interpreterConstraints->maxContainedEntities;
		if(interpreterConstraints->entityToConstrainFrom->DoesDeepContainEntity(interpreter_constraints->entityToConstrainFrom))
		{
			auto erbr = interpreterConstraints->entityToConstrainFrom->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
			size_t container_total_entities = erbr->size();
			erbr.Clear();
			erbr = interpreter_constraints->entityToConstrainFrom->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
			size_t contained_total_entities = erbr->size();
			erbr.Clear();

			if(container_total_entities >= interpreterConstraints->maxContainedEntities)
			{
				max_entities = 0;
				interpreter_constraints->constraintsExceeded = true;
				interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ContainedEntitiesDepth;
			}
			else
			{
				max_entities = interpreterConstraints->maxContainedEntities - (container_total_entities - contained_total_entities);
			}
		}

		interpreter_constraints->maxContainedEntities = std::min(interpreter_constraints->maxContainedEntities, max_entities);
	}

	if(interpreterConstraints != nullptr && interpreterConstraints->constrainMaxContainedEntityDepth
		&& interpreterConstraints->entityToConstrainFrom != nullptr)
	{
		interpreter_constraints->constrainMaxContainedEntityDepth = true;

		size_t max_depth = interpreterConstraints->maxContainedEntityDepth;
		size_t cur_depth = 0;
		if(interpreterConstraints->entityToConstrainFrom->DoesDeepContainEntity(interpreter_constraints->entityToConstrainFrom))
		{
			for(Entity *cur_entity = interpreter_constraints->entityToConstrainFrom;
					cur_entity != interpreterConstraints->entityToConstrainFrom;
					cur_entity = cur_entity->GetContainer())
				cur_depth++;
		}

		if(cur_depth >= max_depth)
		{
			interpreter_constraints->maxContainedEntityDepth = 0;
			interpreter_constraints->constraintsExceeded = true;
			interpreter_constraints->constraintViolation = InterpreterConstraints::ViolationType::ContainedEntitiesDepth;
		}
		else
		{
			interpreter_constraints->maxContainedEntityDepth = std::min(interpreter_constraints->maxContainedEntityDepth,
				max_depth - cur_depth);
		}
	}

	if(interpreterConstraints != nullptr && interpreterConstraints->maxEntityIdLength > 0)
	{
		if(interpreter_constraints->maxEntityIdLength > 0)
			interpreter_constraints->maxEntityIdLength = std::min(interpreter_constraints->maxEntityIdLength,
				interpreterConstraints->maxEntityIdLength);
		else
			interpreter_constraints->maxNumAllocatedNodes = interpreterConstraints->maxEntityIdLength;
	}
}


#ifdef MULTITHREAD_SUPPORT

bool Interpreter::InterpretEvaluableNodesConcurrently(EvaluableNode *parent_node,
	std::vector<EvaluableNode *> &nodes, std::vector<EvaluableNodeReference> &interpreted_nodes,
	bool immediate_results)
{
	if(!parent_node->GetConcurrency())
		return false;

	size_t num_tasks = nodes.size();
	if(num_tasks < 2)
		return false;

	auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
	if(!Concurrency::threadPool.AreThreadsAvailable())
		return false;

	ConcurrencyManager concurrency_manager(this, num_tasks, enqueue_task_lock);

	interpreted_nodes.resize(num_tasks);

	//kick off interpreters
	for(size_t i = 0; i < num_tasks; i++)
		concurrency_manager.EnqueueTask<EvaluableNodeReference>(nodes[i], &interpreted_nodes[i], immediate_results);

	concurrency_manager.EndConcurrency();
	return true;
}

Interpreter *Interpreter::LockScopeStackTop(Concurrency::SingleLock &lock, EvaluableNode *en_to_preserve,
	Interpreter *executing_interpreter)
{
	if(scopeStackNodes->size() == 0 && callingInterpreter != nullptr)
		return callingInterpreter->LockScopeStackTop(lock, en_to_preserve,
			executing_interpreter == nullptr ? this : executing_interpreter);

	if(scopeStackMutex.get() != nullptr)
	{
		if(executing_interpreter != nullptr)
			executing_interpreter->LockMutexWithoutBlockingGarbageCollection(lock, *scopeStackMutex, en_to_preserve);
		else
			LockMutexWithoutBlockingGarbageCollection(lock, *scopeStackMutex, en_to_preserve);
	}

	return this;
}

#endif
