#pragma once

//project headers:
#include "Entity.h"
#include "EntityWriteListener.h"
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"
#include "EvaluableNodeTreeFunctions.h"
#include "PrintListener.h"
#include "RandomStream.h"

//system headers:
#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

//forward declarations:
class EntityQueryCondition;

//Manages performance constraints and accompanying performance counters
class PerformanceConstraints
{
public:

	//if true, there is a limit to how long can utilize CPU
	constexpr bool ConstrainedExecutionSteps()
	{
		return maxNumExecutionSteps != 0;
	}

	//returns the remaining execution steps
	__forceinline ExecutionCycleCount GetRemainingNumExecutionSteps()
	{
		if(curExecutionStep < maxNumExecutionSteps)
			return maxNumExecutionSteps - curExecutionStep;
		else //already past limit
			return 0;
	}

	//if true, there is a limit on how much memory can utilize
	constexpr bool ConstrainedAllocatedNodes()
	{
		return maxNumAllocatedNodes != 0;
	}

	//returns the remaining execution nodes
	__forceinline size_t GetRemainingNumAllocatedNodes(size_t cur_allocated_nodes)
	{
		cur_allocated_nodes += curNumAllocatedNodesAllocatedToEntities;
		if(cur_allocated_nodes < maxNumAllocatedNodes)
			return maxNumAllocatedNodes - cur_allocated_nodes;
		else //already past limit
			return 0;
	}

	//returns true if new_allocated_nodes would exceed the constraint
	__forceinline bool WouldNewAllocatedNodesExceedConstraint(size_t new_allocated_nodes)
	{
		if(!ConstrainedAllocatedNodes())
			return false;

		new_allocated_nodes += curNumAllocatedNodesAllocatedToEntities;
		return (new_allocated_nodes >= maxNumAllocatedNodes);
	}

	//if true, there is a limit on how deep execution can go in opcodes
	constexpr bool ConstrainedOpcodeExecutionDepth()
	{
		return maxOpcodeExecutionDepth != 0;
	}

	//returns the remaining execution depth
	__forceinline size_t GetRemainingOpcodeExecutionDepth(size_t cur_execution_depth)
	{
		if(cur_execution_depth < maxOpcodeExecutionDepth)
			return maxOpcodeExecutionDepth - cur_execution_depth;
		else //already past limit
			return 0;
	}

	//accrues performance counters into the current object from perf_constraints
	__forceinline void AccruePerformanceCounters(PerformanceConstraints *perf_constraints)
	{
		if(perf_constraints == nullptr)
			return;

		curExecutionStep += perf_constraints->curExecutionStep;
		curNumAllocatedNodesAllocatedToEntities += perf_constraints->curNumAllocatedNodesAllocatedToEntities;
	}

	//current execution step - number of nodes executed
#if defined(MULTITHREAD_SUPPORT)
	std::atomic<ExecutionCycleCount> curExecutionStep;
#else
	ExecutionCycleCount curExecutionStep;
#endif

	//maximum number of execution steps by this Interpreter and anything called from it.  If 0, then unlimited.
	//will terminate execution if the value is reached
	ExecutionCycleCount maxNumExecutionSteps;

	//the maximum opcode execution depth
	size_t maxOpcodeExecutionDepth;

	//number of nodes allocated only to entities
	size_t curNumAllocatedNodesAllocatedToEntities;

	//maximum number of nodes allowed to be allocated by this Interpreter and anything called from it.  If 0, then unlimited.
	//will terminate execution if the value is reached
	size_t maxNumAllocatedNodes;

	//entity from which the constraints are based
	Entity *entityToConstrainFrom;

	//flag set to true if constraints have been exceeded
	bool constraintsExceeded;

	bool constrainMaxContainedEntities;
	bool constrainMaxContainedEntityDepth;

	//constrains the maximum number of contained entities
	size_t maxContainedEntities;

	//constrains how deep entities can be created
	size_t maxContainedEntityDepth;

	//constrains the maximum length of an entity id (primarily to make sure it doesn't cause problems for file systems)
	//If 0, then unlimited
	size_t maxEntityIdLength;
};

class Interpreter
{
public:

	//used with construction stack to store the index and whether previous_result is unique,
	//as well as whether any opcodes have been executed that have side effects, that could have written memory elsewhere,
	//and prevent any part of the construction stack from being unique
	struct ConstructionStackIndexAndPreviousResultUniqueness
	{
		inline ConstructionStackIndexAndPreviousResultUniqueness(EvaluableNodeImmediateValueWithType _index,
			bool _unique, bool execution_side_effects = false)
			: index(_index), unique(_unique), executionSideEffects(execution_side_effects)
		{	}

		EvaluableNodeImmediateValueWithType index;
		bool unique;
		bool executionSideEffects;
	};

	//Creates a new interpreter to run code and to store labels.
	// If no entity is specified via nullptr, then it will run sandboxed
	// if performance_constraints is not nullptr, then it will limit execution appropriately
	Interpreter(EvaluableNodeManager *enm, RandomStream rand_stream,
		std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
		PerformanceConstraints *performance_constraints,
		Entity *t, Interpreter *calling_interpreter);

	~Interpreter()
	{	}

	//Executes the current Entity that this Interpreter is contained by
	// sets up all of the stack and contextual structures, then calls InterpretNode on en
	//if call_stack, opcode_stack, or construction_stack are nullptr, it will start with a new one
	//note that construction_stack and construction_stack_indices should be specified together and should be the same length
	//if immediate_result is true, then the returned value may be immediate
#ifdef MULTITHREAD_SUPPORT
	//if run multithreaded, then for performance reasons, it is optimal to have one of each stack per thread
	// and call_stack_write_mutex is the mutex needed to lock for writing
	EvaluableNodeReference ExecuteNode(EvaluableNode *en,
		EvaluableNode *call_stack = nullptr, EvaluableNode *opcode_stack = nullptr,
		EvaluableNode *construction_stack = nullptr,
		std::vector<ConstructionStackIndexAndPreviousResultUniqueness> *construction_stack_indices = nullptr,
		Concurrency::ReadWriteMutex *call_stack_write_mutex = nullptr,
		bool immediate_result = false);
#else
	EvaluableNodeReference ExecuteNode(EvaluableNode *en,
		EvaluableNode *call_stack = nullptr, EvaluableNode *opcode_stack = nullptr,
		EvaluableNode *construction_stack = nullptr,
		std::vector<ConstructionStackIndexAndPreviousResultUniqueness> *construction_stack_indices = nullptr,
		bool immediate_result = false);
#endif

	//changes debugging state to debugging_enabled
	//cannot be enabled at the same time as profiling
	static void SetDebuggingState(bool debugging_enabled);

	//returns true if in debugging
	static bool GetDebuggingState();

	//changes opcode profiling state to opcode_profiling_enabled
	//cannot be enabled at the same time as other profiling or debugging
	static void SetOpcodeProfilingState(bool opcode_profiling_enabled);

	//changes opcode profiling state to opcode_profiling_enabled
	//cannot be enabled at the same time as other profiling or debugging
	static void SetLabelProfilingState(bool label_profiling_enabled);

	//when debugging, checks any relevant breakpoints and update debugger state if any are triggered
	// if before_opcode is true, then it is checking before it is run, otherwise it'll check after it is completed
	void DebugCheckBreakpointsAndUpdateState(EvaluableNode *en, bool before_opcode);

	//collects garbage on evaluableNodeManager
	__forceinline void CollectGarbage()
	{
		if(evaluableNodeManager->RecommendGarbageCollection())
		{
		#ifdef MULTITHREAD_SUPPORT
			evaluableNodeManager->CollectGarbageWithConcurrentAccess(&memoryModificationLock);
		#else
			evaluableNodeManager->CollectGarbage();
		#endif
		}
	}

	//pushes new_context on the stack; new_context should be a unique associative array,
	// but if not, it will attempt to put an appropriate unique associative array on callStackNodes
	__forceinline void PushNewCallStack(EvaluableNodeReference new_context)
	{
		//make sure unique assoc
		if(EvaluableNode::IsAssociativeArray(new_context))
		{
			if(!new_context.unique)
				new_context.SetReference(evaluableNodeManager->AllocNode(new_context, EvaluableNodeManager::ENMM_REMOVE_ALL));
		}
		else //not assoc, make a new one
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(new_context);
			new_context.SetReference(evaluableNodeManager->AllocNode(ENT_ASSOC));
		}

		//just in case a variable is added which needs cycle checks
		new_context->SetNeedCycleCheck(true);

		callStackNodes->push_back(new_context);
	}

	//pops the top context off the stack
	__forceinline void PopCallStack()
	{
		evaluableNodeManager->FreeNode(callStackNodes->back());
		callStackNodes->pop_back();
	}

	//pushes a new construction context on the stack, which is assumed to not be nullptr
	//the stack is indexed via the constructionStackOffset* constants
	//target_origin is the original node of target useful for keeping track of the reference
	static inline void PushNewConstructionContextToStack(std::vector<EvaluableNode *> &stack_nodes,
		std::vector<ConstructionStackIndexAndPreviousResultUniqueness> &stack_node_indices,
		EvaluableNode *target_origin, EvaluableNode *target,
		EvaluableNodeImmediateValueWithType current_index, EvaluableNode *current_value,
		EvaluableNodeReference previous_result)
	{
		size_t new_size = stack_nodes.size() + constructionStackOffsetStride;
		stack_nodes.resize(new_size, nullptr);

		stack_nodes[new_size + constructionStackOffsetTargetOrigin] = target_origin;
		stack_nodes[new_size + constructionStackOffsetTarget] = target;
		stack_nodes[new_size + constructionStackOffsetCurrentValue] = current_value;
		stack_nodes[new_size + constructionStackOffsetPreviousResult] = previous_result;

		stack_node_indices.emplace_back(current_index, previous_result.unique);
	}

	//pushes a new construction context on the stack
	//the stack is indexed via the constructionStackOffset* constants
	//target_origin is the original node of target useful for keeping track of the reference
	__forceinline void PushNewConstructionContext(EvaluableNode *target_origin, EvaluableNode *target,
		EvaluableNodeImmediateValueWithType current_index, EvaluableNode *current_value,
		EvaluableNodeReference previous_result = EvaluableNodeReference::Null())
	{
		return PushNewConstructionContextToStack(*constructionStackNodes, constructionStackIndicesAndUniqueness,
			target_origin, target, current_index, current_value, previous_result);
	}

	//pops the top construction context off the stack
	//and returns true if that construction stack node had memory write side effects
	inline bool PopConstructionContextAndGetExecutionSideEffectFlag()
	{
		size_t new_size = constructionStackNodes->size();
		if(new_size > constructionStackOffsetStride)
			new_size -= constructionStackOffsetStride;
		else
			new_size = 0;

		constructionStackNodes->resize(new_size);

		if(constructionStackIndicesAndUniqueness.size() > 0)
		{
			bool execution_side_effects = constructionStackIndicesAndUniqueness.back().executionSideEffects;
			constructionStackIndicesAndUniqueness.pop_back();
			return execution_side_effects;
		}

		//something odd happened, shouldn't be here
		return true;
	}

	//returns true if the top of the construction stack has memory write execution side effects
	inline bool DoesConstructionStackHaveExecutionSideEffects()
	{
		if(constructionStackIndicesAndUniqueness.size() > 0)
			return constructionStackIndicesAndUniqueness.front().executionSideEffects;
		return false;
	}

	//updates the construction index at top of the stack to the new value
	//assumes there is at least one construction stack entry
	__forceinline void SetTopCurrentIndexInConstructionStack(double new_index)
	{
		constructionStackIndicesAndUniqueness.back().index = EvaluableNodeImmediateValueWithType(new_index);
	}

	__forceinline void SetTopCurrentIndexInConstructionStack(StringInternPool::StringID new_index)
	{
		constructionStackIndicesAndUniqueness.back().index = EvaluableNodeImmediateValueWithType(new_index);
	}

	//sets the value node for the top reference on the construction stack
	//used for updating the current target value
	//assumes there is at least one construction stack entry
	__forceinline void SetTopCurrentValueInConstructionStack(EvaluableNode *value)
	{
		(*constructionStackNodes)[constructionStackNodes->size() + constructionStackOffsetCurrentValue] = value;
	}

	//sets the previous_result node for the top reference on the construction stack
	//assumes there is at least one construction stack entry
	__forceinline void SetTopPreviousResultInConstructionStack(EvaluableNodeReference previous_result)
	{
		(*constructionStackNodes)[constructionStackNodes->size() + constructionStackOffsetPreviousResult] = previous_result;
		constructionStackIndicesAndUniqueness.back().unique = previous_result.unique;
	}

	//gets the previous_result node for the reference at depth on the construction stack
	//assumes there is at least one construction stack entry and depth is a valid depth
	__forceinline EvaluableNodeReference GetAndClearPreviousResultInConstructionStack(size_t depth)
	{
		size_t uniqueness_offset = constructionStackIndicesAndUniqueness.size() - depth - 1;
		bool previous_result_unique = constructionStackIndicesAndUniqueness[uniqueness_offset].unique;

		//clear previous result
		size_t prev_result_offset = constructionStackNodes->size()
						- (constructionStackOffsetStride * depth) + constructionStackOffsetPreviousResult;
		auto &previous_result_loc = (*constructionStackNodes)[prev_result_offset];
		EvaluableNode *previous_result = nullptr;
		std::swap(previous_result, previous_result_loc);

		return EvaluableNodeReference(previous_result, previous_result_unique);
	}

	//deep copies the previous_result node for the reference at depth on the construction stack
	//assumes there is at least one construction stack entry and depth is a valid depth
	__forceinline EvaluableNodeReference CopyPreviousResultInConstructionStack(size_t depth)
	{
		//clear previous result
		size_t prev_result_offset = constructionStackNodes->size()
						- (constructionStackOffsetStride * depth) + constructionStackOffsetPreviousResult;
		auto &previous_result_loc = (*constructionStackNodes)[prev_result_offset];
		return evaluableNodeManager->DeepAllocCopy(previous_result_loc);
	}

	//clears all uniqueness of previous_results in construction stack in case the construction stack is copied across threads
	inline void RemoveUniquenessFromPreviousResultsInConstructionStack()
	{
		for(auto &entry : constructionStackIndicesAndUniqueness)
			entry.unique = false;
	}

	//should be called by any opcode that has side effects setting memory, such as assignment, accumulation, etc.
	//returns a pair of booleans, where the first value is true if there are any constructions,
	// and the second is true if it set at least one flag (i.e., it was the first time doing so)
	inline std::pair<bool, bool> SetSideEffectsFlagsInConstructionStack()
	{
		bool any_constructions = (constructionStackIndicesAndUniqueness.size() > 0);
		bool any_set = false;
		for(size_t i = constructionStackIndicesAndUniqueness.size(); i > 0; i--)
		{
			size_t index = i - 1;
			//early out if already set with side effects
			if(constructionStackIndicesAndUniqueness[index].executionSideEffects)
				break;

			constructionStackIndicesAndUniqueness[index].executionSideEffects = true;
			any_set = true;
		}

		return std::make_pair(any_constructions, any_set);
	}

	//Makes sure that args is an active associative array is proper for context, meaning initialized assoc and a unique reference.
	// Will allocate a new node appropriately if it is not
	//Then wraps the args on a list which will form the call stack and returns that
	//ensures that args is still a valid EvaluableNodeReference after the call
	static EvaluableNodeReference ConvertArgsToCallStack(EvaluableNodeReference args, EvaluableNodeManager &enm);

	//finds a pointer to the location of the symbol's pointer to value in the top of the context stack and returns a pointer to the location of the symbol's pointer to value,
	// nullptr if it does not exist
	// also sets call_stack_index to the level in the call stack that it was found
	//if include_unique_access is true, then it will cover the top of the stack to callStackUniqueAccessStartingDepth
	//if include_shared_access is true, then it will cover the bottom of the stack from callStackUniqueAccessStartingDepth to 0
	EvaluableNode **GetCallStackSymbolLocation(const StringInternPool::StringID symbol_sid, size_t &call_stack_index
#ifdef MULTITHREAD_SUPPORT
		, bool include_unique_access = true, bool include_shared_access = true
#endif
	);

	//like the other type of GetCallStackSymbolLocation, but returns the EvaluableNode pointer instead of a pointer-to-a-pointer
	__forceinline EvaluableNode *GetCallStackSymbol(const StringInternPool::StringID symbol_sid)
	{
		size_t call_stack_index = 0;
		EvaluableNode **en_ptr = GetCallStackSymbolLocation(symbol_sid, call_stack_index);
		if(en_ptr == nullptr)
			return nullptr;

		return *en_ptr;
	}

	//finds a pointer to the location of the symbol's pointer to value or creates the symbol in the top of the context stack and returns a pointer to the location of the symbol's pointer to value
	// also sets call_stack_index to the level in the call stack that it was found
	EvaluableNode **GetOrCreateCallStackSymbolLocation(const StringInternPool::StringID symbol_sid, size_t &call_stack_index);

	//returns the current call stack index
	__forceinline size_t GetCallStackDepth()
	{
		return callStackNodes->size() - 1;
	}

	//creates a stack state saver for the interpreterNodeStack, which will be restored back to its previous condition when this object is destructed
	__forceinline EvaluableNodeStackStateSaver CreateOpcodeStackStateSaver()
	{
		return EvaluableNodeStackStateSaver(opcodeStackNodes);
	}

	//like CreateOpcodeStackStateSaver, but also pushes another node on the stack
	__forceinline EvaluableNodeStackStateSaver CreateOpcodeStackStateSaver(EvaluableNode *en)
	{
		//count on C++ return value optimization to not call the destructor
		return EvaluableNodeStackStateSaver(opcodeStackNodes, en);
	}

	//keeps the current node on the stack and calls InterpretNodeExecution
	//if immediate_result is true, it will not allocate a node
	EvaluableNodeReference InterpretNode(EvaluableNode *en, bool immediate_result = false);

	//returns the current call stack context, nullptr if none
	EvaluableNode *GetCurrentCallStackContext();

	//returns an EvaluableNodeReference for value, allocating if necessary based on if immediate result is needed
	template<typename T>
	inline EvaluableNodeReference AllocReturn(T value, bool immediate_result)
	{
		return evaluableNodeManager->AllocIfNotImmediate(value, immediate_result);
	}

	//converts enr into a number and frees
	double ConvertNodeIntoNumberValueAndFreeIfPossible(EvaluableNodeReference &enr)
	{
		double value = enr.GetValue().GetValueAsNumber();
		evaluableNodeManager->FreeNodeTreeIfPossible(enr);
		return value;
	}

	//if n is immediate, it just returns it, otherwise calls InterpretNode
	__forceinline EvaluableNodeReference InterpretNodeForImmediateUse(EvaluableNode *n, bool immediate_result = false)
	{
		if(n == nullptr || n->GetIsIdempotent())
			return EvaluableNodeReference(n, false);
		return InterpretNode(n, immediate_result);
	}

	//computes a unary numeric function on the given node
	__forceinline EvaluableNodeReference InterpretNodeUnaryNumericOperation(EvaluableNode *n, bool immediate_result,
		std::function<double(double)> func)
	{
		if(immediate_result)
		{
			double value = InterpretNodeIntoNumberValue(n);
			return EvaluableNodeReference(func(value));
		}

		auto retval = InterpretNodeIntoUniqueNumberValueOrNullEvaluableNode(n);
		double value = retval->GetNumberValue();
		double result = func(value);
		retval->SetTypeViaNumberValue(result);
		return retval;
	}

	//Calls InterpretNode on n, converts to std::string and stores in value to return, then cleans up any resources used
	//returns a pair of bool, whether it was a valid string (and not NaS), and the string
	std::pair<bool, std::string> InterpretNodeIntoStringValue(EvaluableNode *n, bool key_string = false);

	//Calls InterpretNode on n, converts to std::string and stores in value to return, then cleans up any resources used
	// but if n is null, it will return an empty string
	inline std::string InterpretNodeIntoStringValueEmptyNull(EvaluableNode *n, bool key_string = false)
	{
		auto [valid, str] = InterpretNodeIntoStringValue(n, key_string);
		if(!valid)
			return "";
		return str;
	}

	//like InterpretNodeIntoStringValue, but returns the ID only if the string already exists,
	// otherwise it returns NOT_A_STRING_ID
	StringInternPool::StringID InterpretNodeIntoStringIDValueIfExists(EvaluableNode *n, bool key_string = false);

	//like InterpretNodeIntoStringValue, but creates a reference to the string that must be destroyed,
	// regardless of whether the string existed or not (if it did not exist, then it creates one)
	StringInternPool::StringID InterpretNodeIntoStringIDValueWithReference(EvaluableNode *n, bool key_string = false);

	//Calls InterpretNode on n, convers to a string, and makes sure that the node returned is
	// new and unique so that it can be modified
	EvaluableNodeReference InterpretNodeIntoUniqueStringIDValueEvaluableNode(EvaluableNode *n,
		bool immediate_result = false);

	//Calls InterpretNode on n, converts to double and returns, then cleans up any resources used
	double InterpretNodeIntoNumberValue(EvaluableNode *n);

	//Calls InterpretNode on n, convers to a double or null (representing NaN),
	//and makes sure that the node returned is new and unique so that it can be modified
	EvaluableNodeReference InterpretNodeIntoUniqueNumberValueOrNullEvaluableNode(EvaluableNode *n);

	//Calls InterpretNode on n, converts to boolean and returns, then cleans up any resources used
	bool InterpretNodeIntoBoolValue(EvaluableNode *n, bool value_if_null = false);

	//Calls InterpretNode on n, converts n into a destination for an Entity, relative to curEntity.
	// If invalid, returns a nullptr for the EntityWriteReference
	//StringRef is an allocated string reference, and the caller is responsible for freeing it
	std::pair<EntityWriteReference, StringRef> InterpretNodeIntoDestinationEntity(EvaluableNode *n);

	//traverses source based on traversal path list tpl
	// If create_destination_if_necessary is set, then it will expand anything in the source as appropriate
	//Returns the location of the EvaluableNode * of the destination, nullptr if it does not exist
	EvaluableNode **TraverseToDestinationFromTraversalPathList(EvaluableNode **source,
		EvaluableNodeReference &tpl, bool create_destination_if_necessary);

	//calls InterpretNode on tpl, traverses source based on tpl.
	// If create_destination_if_necessary is set, then it will expand anything in the source as appropriate
	//Returns the location of the EvaluableNode * of the destination, nullptr if it does not exist
	__forceinline EvaluableNode **InterpretNodeIntoDestination(EvaluableNode **source,
		EvaluableNode *tpl, bool create_destination_if_necessary)
	{
		EvaluableNodeReference address_list_node = InterpretNodeForImmediateUse(tpl);
		EvaluableNode **destination = TraverseToDestinationFromTraversalPathList(source, address_list_node, create_destination_if_necessary);
		evaluableNodeManager->FreeNodeTreeIfPossible(address_list_node);
		return destination;
	}

	//Interprets node_id_path_to_interpret and then attempts to find the Entity relative to curEntity. Returns nullptr if cannot find
	template<typename EntityReferenceType>
	inline EntityReferenceType InterpretNodeIntoRelativeSourceEntityReference(EvaluableNode *node_id_path_to_interpret)
	{
		if(curEntity == nullptr)
			return EntityReferenceType(nullptr);

		//extra optimization to skip the logic below when the path is null
		if(EvaluableNode::IsNull(node_id_path_to_interpret))
			return EntityReferenceType(curEntity);

		//only need to interpret if not idempotent
		EvaluableNodeReference source_id_node = InterpretNodeForImmediateUse(node_id_path_to_interpret);
		EntityReferenceType source_entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReferenceType>(curEntity, source_id_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(source_id_node);

		return source_entity;
	}

	//like InterpretNodeIntoRelativeSourceEntityReference but with a read reference
	inline EntityReadReference InterpretNodeIntoRelativeSourceEntityReadReference(EvaluableNode *node_id_path_to_interpret)
	{
		return InterpretNodeIntoRelativeSourceEntityReference<EntityReadReference>(node_id_path_to_interpret);
	}

	//like InterpretNodeIntoRelativeSourceEntityReference but with a write reference
	inline EntityWriteReference InterpretNodeIntoRelativeSourceEntityWriteReference(EvaluableNode *node_id_path_to_interpret)
	{
		return InterpretNodeIntoRelativeSourceEntityReference<EntityWriteReference>(node_id_path_to_interpret);
	}

	//like InterpretNodeIntoRelativeSourceEntityReference, but a pair of read references
	inline std::tuple<Entity *, Entity *, Entity::EntityReferenceBufferReference<EntityReadReference>>
		InterpretNodeIntoRelativeSourceEntityReadReferences(EvaluableNode *node_id_path_to_interpret_1, EvaluableNode *node_id_path_to_interpret_2)
	{
		if(curEntity == nullptr)
			return std::make_tuple(nullptr, nullptr,
				Entity::EntityReferenceBufferReference<EntityReadReference>());

		auto node_id_path_1 = InterpretNodeForImmediateUse(node_id_path_to_interpret_1);
		auto node_stack = CreateOpcodeStackStateSaver(node_id_path_1);
		auto node_id_path_2 = InterpretNodeForImmediateUse(node_id_path_to_interpret_2);
		node_stack.PopEvaluableNode();

		auto [entity_1, entity_2, erbr]
			= TraverseToDeeplyContainedEntityReadReferencesViaEvaluableNodeIDPath(curEntity,
				node_id_path_1, node_id_path_2);

		evaluableNodeManager->FreeNodeTreeIfPossible(node_id_path_1);
		evaluableNodeManager->FreeNodeTreeIfPossible(node_id_path_2);

		return std::make_tuple(entity_1, entity_2, std::move(erbr));
	}

protected:

	//traverses down n until it reaches the furthest-most nodes from top_node,
	// then bubbles back up re-evaluating each node via the specified function
	//returns the (potentially) modified tree
	EvaluableNodeReference RewriteByFunction(EvaluableNodeReference function,
		EvaluableNode *tree, FastHashMap<EvaluableNode *, EvaluableNode *> &original_node_to_new_node);

	//populates perf_constraints from params starting at the offset perf_constraint_param_offset,
	// in the order of execution cycles, maximum memory, maximum stack depth
	//returns true if there are any performance constraints, false if not
	//if include_entity_constraints is true, it will include constraints regarding entities
	bool PopulatePerformanceConstraintsFromParams(std::vector<EvaluableNode *> &params,
		size_t perf_constraint_param_offset, PerformanceConstraints &perf_constraints, bool include_entity_constraints = false);

	//if perf_constraints is not null, populates the counters representing the current state of the interpreter
	void PopulatePerformanceCounters(PerformanceConstraints *perf_constraints, Entity *entity_to_constrain_from);

#ifdef MULTITHREAD_SUPPORT

	//class to manage the data for concurrent execution by an interpreter
	class ConcurrencyManager
	{
	public:

		//constructs the concurrency manager.  Assumes parent_interpreter is NOT null
		ConcurrencyManager(Interpreter *parent_interpreter, size_t num_tasks,
			ThreadPool::TaskLock &task_enqueue_lock)
			: taskSet(&Concurrency::threadPool, num_tasks)
		{
			resultsUnique = true;
			resultsNeedCycleCheck = false;
			resultsIdempotent = true;

			parentInterpreter = parent_interpreter;
			numTasks = num_tasks;
			curNumTasksEnqueued = 0;
			taskEnqueueLock = &task_enqueue_lock;

			//create space to store all of these nodes on the stack, but won't copy these over to the other interpreters
			resultsSaver = parent_interpreter->CreateOpcodeStackStateSaver();
			resultsSaverFirstTaskOffset = resultsSaver.GetLocationOfCurrentStackTop() + 1;
			resultsSaverCurrentTaskOffset = resultsSaverFirstTaskOffset;
			resultsSaver.ReserveNodes(num_tasks);

			randomSeeds.reserve(numTasks);
			for(size_t element_index = 0; element_index < numTasks; element_index++)
				randomSeeds.emplace_back(parentInterpreter->randomStream.CreateOtherStreamViaRand());

			//since each thread has a copy of the constructionStackNodes, it's possible that more than one of the threads
			//obtains previous_results, so they must all be marked as not unique
			parentInterpreter->RemoveUniquenessFromPreviousResultsInConstructionStack();
		}

		//Enqueues a concurrent task that needs a construction stack, using the relative interpreter
		// executes node_to_execute with the following parameters matching those of pushing on the construction stack
		// will allocate an appropriate node matching the type of current_index
		//result is set to the result of the task
		template<typename EvaluableNodeRefType>
		void EnqueueTaskWithConstructionStack(EvaluableNode *node_to_execute,
			EvaluableNode *target_origin, EvaluableNode *target,
			EvaluableNodeImmediateValueWithType current_index,
			EvaluableNode *current_value,
			EvaluableNodeRefType &result)
		{
			size_t results_saver_location = resultsSaverCurrentTaskOffset++;
			RandomStream rand_seed = randomSeeds[curNumTasksEnqueued++];

			Concurrency::threadPool.BatchEnqueueTask(
				[this, rand_seed, node_to_execute, target_origin, target, current_index,
				current_value, &result, results_saver_location]
				{
					EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;
					EvaluableNodeManager::ClearThreadLocalAllocationBuffer();

					Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
						parentInterpreter->writeListeners, parentInterpreter->printListener,
						parentInterpreter->performanceConstraints, parentInterpreter->curEntity, parentInterpreter);

					interpreter.memoryModificationLock = Concurrency::ReadLock(enm->memoryModificationMutex);

					//build new construction stack
					EvaluableNode *construction_stack = enm->AllocNode(*parentInterpreter->constructionStackNodes);
					std::vector<ConstructionStackIndexAndPreviousResultUniqueness> csiau(parentInterpreter->constructionStackIndicesAndUniqueness);
					interpreter.PushNewConstructionContextToStack(construction_stack->GetOrderedChildNodes(),
						csiau, target_origin, target, current_index, current_value, EvaluableNodeReference::Null());

					auto result_ref = interpreter.ExecuteNode(node_to_execute,
						enm->AllocNode(*parentInterpreter->callStackNodes),
						enm->AllocNode(begin(*parentInterpreter->opcodeStackNodes),
							begin(*parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset),
						construction_stack,
						&csiau,
						GetCallStackMutex());

					if(interpreter.PopConstructionContextAndGetExecutionSideEffectFlag())
					{
						resultsSideEffect = true;
						resultsUnique = false;
					}

					if(result_ref.unique)
					{
						if(result_ref.GetNeedCycleCheck())
							resultsNeedCycleCheck = true;
					}
					else
					{
						resultsUnique = false;
						resultsNeedCycleCheck = true;
					}

					if(!result_ref.GetIsIdempotent())
						resultsIdempotent = false;

					result = result_ref;
					resultsSaver.SetStackLocation(results_saver_location, result);

					EvaluableNodeManager::ClearThreadLocalAllocationBuffer();
					interpreter.memoryModificationLock.unlock();
					taskSet.MarkTaskCompleted();
				}
			);
		}

		//Enqueues a concurrent task using the relative interpreter, executing node_to_execute
		//if result is specified, it will store the result there, otherwise it will free it
		template<typename EvaluableNodeRefType>
		void EnqueueTask(EvaluableNode *node_to_execute,
			EvaluableNodeRefType *result = nullptr, bool immediate_results = false)
		{
			//save the node to execute, but also save the location
			//so the location can be used later to save the result
			size_t results_saver_location = resultsSaverCurrentTaskOffset++;

			RandomStream rand_seed = randomSeeds[curNumTasksEnqueued++];

			Concurrency::threadPool.BatchEnqueueTask(
				[this, rand_seed, node_to_execute, result, immediate_results, results_saver_location]
				{
					EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;
					EvaluableNodeManager::ClearThreadLocalAllocationBuffer();

					Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
						parentInterpreter->writeListeners, parentInterpreter->printListener,
						parentInterpreter->performanceConstraints, parentInterpreter->curEntity, parentInterpreter);

					interpreter.memoryModificationLock = Concurrency::ReadLock(enm->memoryModificationMutex);

					std::vector<ConstructionStackIndexAndPreviousResultUniqueness> csiau(parentInterpreter->constructionStackIndicesAndUniqueness);
					auto result_ref = interpreter.ExecuteNode(node_to_execute,
						enm->AllocNode(*parentInterpreter->callStackNodes),
						enm->AllocNode(begin(*parentInterpreter->opcodeStackNodes),
							begin(*parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset),
						enm->AllocNode(*parentInterpreter->constructionStackNodes),
						&csiau,
						GetCallStackMutex(), immediate_results);

					if(interpreter.DoesConstructionStackHaveExecutionSideEffects())
						resultsSideEffect = true;

					if(result == nullptr)
					{
						enm->FreeNodeTreeIfPossible(result_ref);
					}
					else //want result
					{
						if(result_ref.unique)
						{
							if(result_ref.GetNeedCycleCheck())
								resultsNeedCycleCheck = true;
						}
						else
						{
							resultsUnique = false;
							resultsNeedCycleCheck = true;
						}

						if(!result_ref.GetIsIdempotent())
							resultsIdempotent = false;

						*result = result_ref;

						//only save the result if it's not immediate
						if(!result_ref.IsImmediateValue())
							resultsSaver.SetStackLocation(results_saver_location, *result);
					}

					EvaluableNodeManager::ClearThreadLocalAllocationBuffer();
					interpreter.memoryModificationLock.unlock();
					taskSet.MarkTaskCompleted();
				}
			);
		}

		//ends concurrency from all interpreters and waits for them to finish
		inline void EndConcurrency()
		{
			//allow other threads to perform garbage collection
			EvaluableNodeManager::ClearThreadLocalAllocationBuffer();
			parentInterpreter->memoryModificationLock.unlock();
			taskSet.WaitForTasks(taskEnqueueLock);
			parentInterpreter->memoryModificationLock.lock();

			//propagate side effects back up
			if(resultsSideEffect)
				parentInterpreter->SetSideEffectsFlagsInConstructionStack();
		}

		//updates the aggregated result reference's properties based on all of the child nodes
		inline void UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(EvaluableNodeReference &new_result)
		{
			if(!resultsUnique)
				new_result.unique = false;

			new_result.SetNeedCycleCheck(resultsNeedCycleCheck);

			if(!resultsIdempotent)
				new_result.SetIsIdempotent(false);
		}

		//returns true if any writes occurred
		inline bool HadSideEffects()
		{
			return resultsSideEffect;
		}

		//returns the relevant write mutex for the call stack
		constexpr Concurrency::ReadWriteMutex *GetCallStackMutex()
		{
			//if there is one currently in use, use it
			if(parentInterpreter->callStackMutex != nullptr)
				return parentInterpreter->callStackMutex;

			//start a new one
			return &callStackMutex;
		}

	protected:
		//random seed for each task, the size of numTasks
		std::vector<RandomStream> randomSeeds;

		//mutex to allow only one thread to write to a call stack symbol at once
		Concurrency::ReadWriteMutex callStackMutex;

		//a barrier to wait for the tasks being run
		ThreadPool::CountableTaskSet taskSet;

		//structure to keep track of the stack to prevent results from being garbage collected
		EvaluableNodeStackStateSaver resultsSaver;

		//interpreter that is running all the concurrent interpreters
		Interpreter *parentInterpreter;

		//if true, indicates all results are unique
		std::atomic_bool resultsUnique;

		//if false, indicates all results are cycle free
		std::atomic_bool resultsNeedCycleCheck;

		//if true, indicates all results are idempotent
		std::atomic_bool resultsIdempotent;

		//if true, indicates there was a side effect
		std::atomic_bool resultsSideEffect;

		//the total number of tasks to be processed
		size_t numTasks;

		//offset for the first task in resultsSaver, up to numTasks
		//uses current location and and counts upward
		size_t resultsSaverFirstTaskOffset;

		//current task offset, which started at resultsSaverFirstTaskOffset
		size_t resultsSaverCurrentTaskOffset;

		//number of tasks enqueued so far
		size_t curNumTasksEnqueued;

		//lock for enqueueing tasks
		ThreadPool::TaskLock *taskEnqueueLock;
	};

	//computes the nodes concurrently and stores the interpreted values into interpreted_nodes
	// looks to parent_node to whether concurrency is enabled
	//if true, immediate_results allows the interpreted_nodes to be set to immediate values
	//returns true if it is able to interpret the nodes concurrently
	bool InterpretEvaluableNodesConcurrently(EvaluableNode *parent_node,
		std::vector<EvaluableNode *> &nodes, std::vector<EvaluableNodeReference> &interpreted_nodes,
		bool immediate_results = false);

	//acquires lock, but does so in a way as to not block other threads that may be waiting on garbage collection
	//if en_to_preserve is not null, then it will create a stack saver for it if garbage collection is invoked
	template<typename LockType>
	inline void LockWithoutBlockingGarbageCollection(
		Concurrency::ReadWriteMutex &mutex, LockType &lock, EvaluableNode *en_to_preserve = nullptr)
	{
		lock = LockType(*callStackMutex, std::defer_lock);
		//if there is lock contention, but one is blocking for garbage collection,
		// keep checking until it can get the lock
		if(en_to_preserve)
		{
			while(!lock.try_lock())
			{
				auto node_stack = CreateOpcodeStackStateSaver(en_to_preserve);
				CollectGarbage();
			}
		}
		else
		{
			while(!lock.try_lock())
				CollectGarbage();
		}
	}

#endif

	//returns false if this or any calling interpreter is currently running on the entity specified or if there is any active concurrency
	// actively editing an entity's EvaluableNode data can cause memory errors if being accessed elsewhere, so a copy must be made
	bool IsEntitySafeForModification(Entity *entity)
	{
		for(Interpreter *cur_interpreter = this; cur_interpreter != nullptr; cur_interpreter = cur_interpreter->callingInterpreter)
		{
			//if accessing the entity or have multiple threads, can't ensure safety
			if(cur_interpreter->curEntity == entity)
				return false;

		#ifdef MULTITHREAD_SUPPORT
			if(cur_interpreter->callStackUniqueAccessStartingDepth > 0)
				return false;
		#endif
		}

		return true;
	}

	//if true, no limit on how much memory can utilize
	constexpr bool ConstrainedAllocatedNodes()
	{
		return (performanceConstraints != nullptr && performanceConstraints->ConstrainedAllocatedNodes());
	}

	//returns true if it can create a new entity given the constraints
	__forceinline bool CanCreateNewEntityFromConstraints(Entity *destination_container, StringInternPool::StringID entity_id,
		size_t total_num_new_entities = 1)
	{
		if(performanceConstraints == nullptr)
			return true;

		if(performanceConstraints->maxEntityIdLength > 0
				&& string_intern_pool.GetStringFromID(entity_id).size() > performanceConstraints->maxEntityIdLength)
			return false;

		//exit early if don't need to lock all contained entities
		if(!performanceConstraints->constrainMaxContainedEntities && !performanceConstraints->constrainMaxContainedEntityDepth)
			return true;

		auto erbr
			= performanceConstraints->entityToConstrainFrom->GetAllDeeplyContainedEntityReferencesGroupedByDepth<
			EntityReadReference>(true, destination_container);

		if(performanceConstraints->constrainMaxContainedEntities)
		{
			if(erbr->size() + total_num_new_entities > performanceConstraints->maxContainedEntities)
				return false;
		}

		if(performanceConstraints->constrainMaxContainedEntityDepth)
		{
			if(1 + erbr.maxEntityPathDepth > performanceConstraints->maxContainedEntityDepth)
				return false;
		}

		return true;
	}

	//returns true if there's a max number of execution steps or nodes and at least one is exhausted
	__forceinline bool AreExecutionResourcesExhausted(bool increment_performance_counters = false)
	{
		if(performanceConstraints == nullptr)
			return false;

		if(performanceConstraints->ConstrainedExecutionSteps())
		{
			if(increment_performance_counters)
				performanceConstraints->curExecutionStep++;

			if(performanceConstraints->curExecutionStep > performanceConstraints->maxNumExecutionSteps)
			{
				performanceConstraints->constraintsExceeded = true;
				return true;
			}
		}

		if(performanceConstraints->ConstrainedAllocatedNodes())
		{
			size_t cur_allocated_nodes = performanceConstraints->curNumAllocatedNodesAllocatedToEntities + evaluableNodeManager->GetNumberOfUsedNodes();
			if(cur_allocated_nodes > performanceConstraints->maxNumAllocatedNodes)
			{
				performanceConstraints->constraintsExceeded = true;
				return true;
			}
		}

		if(performanceConstraints->ConstrainedOpcodeExecutionDepth())
		{
			if(opcodeStackNodes->size() > performanceConstraints->maxOpcodeExecutionDepth)
			{
				performanceConstraints->constraintsExceeded = true;
				return true;
			}
		}

		//return whether they have ever been exceeded
		return performanceConstraints->constraintsExceeded;
	}

	//opcodes
	//returns an EvaluableNode tree from evaluating the tree passed in (or nullptr) and associated properties in an EvaluableNodeReference
	//prior to calling, en must be referenced (via KeepNodeReference, or part of an entity) so it will not be garbage collected
	//further, for performance, en must be guaranteed to be a valid pointer, and not nullptr

	//built-in / system specific
	EvaluableNodeReference InterpretNode_ENT_SYSTEM(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GET_DEFAULTS(EvaluableNode *en, bool immediate_result);

	//parsing
	EvaluableNodeReference InterpretNode_ENT_PARSE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_UNPARSE(EvaluableNode *en, bool immediate_result);

	//core control
	EvaluableNodeReference InterpretNode_ENT_IF(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SEQUENCE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_PARALLEL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_LAMBDA(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CONCLUDE_and_RETURN(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CALL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CALL_SANDBOXED(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_WHILE(EvaluableNode *en, bool immediate_result);

	//definitions
	EvaluableNodeReference InterpretNode_ENT_LET(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_DECLARE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ASSIGN_and_ACCUM(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_RETRIEVE(EvaluableNode *en, bool immediate_result);

	//retrieval
	EvaluableNodeReference InterpretNode_ENT_GET(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_and_REPLACE(EvaluableNode *en, bool immediate_result);

	//stack and node manipulation
	EvaluableNodeReference InterpretNode_ENT_TARGET(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CURRENT_INDEX(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CURRENT_VALUE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_PREVIOUS_RESULT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_OPCODE_STACK(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_STACK(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ARGS(EvaluableNode *en, bool immediate_result);

	//simulation and operations
	EvaluableNodeReference InterpretNode_ENT_RAND(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GET_RAND_SEED(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_RAND_SEED(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SYSTEM_TIME(EvaluableNode *en, bool immediate_result);

	//base math
	EvaluableNodeReference InterpretNode_ENT_ADD(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SUBTRACT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MULTIPLY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_DIVIDE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MODULUS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GET_DIGITS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_DIGITS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_FLOOR(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CEILING(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ROUND(EvaluableNode *en, bool immediate_result);

	//extended math
	EvaluableNodeReference InterpretNode_ENT_EXPONENT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_LOG(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_SIN(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ASIN(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_COS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ACOS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_TAN(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ATAN(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_SINH(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ASINH(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_COSH(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ACOSH(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_TANH(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ATANH(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_ERF(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_TGAMMA(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_LGAMMA(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_SQRT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_POW(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ABS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MAX(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MIN(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_INDEX_MAX(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_INDEX_MIN(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_DOT_PRODUCT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GENERALIZED_DISTANCE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ENTROPY(EvaluableNode *en, bool immediate_result);

	//list manipulation
	EvaluableNodeReference InterpretNode_ENT_FIRST(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_TAIL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_LAST(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_TRUNC(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_APPEND(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SIZE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_RANGE(EvaluableNode *en, bool immediate_result);

	//transformation
	EvaluableNodeReference InterpretNode_ENT_REWRITE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MAP(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_FILTER(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_WEAVE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_REDUCE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_APPLY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_REVERSE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SORT(EvaluableNode *en, bool immediate_result);

	//associative list manipulation
	EvaluableNodeReference InterpretNode_ENT_INDICES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_VALUES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_INDEX(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_VALUE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_REMOVE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_KEEP(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ASSOCIATE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ZIP(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_UNZIP(EvaluableNode *en, bool immediate_result);

	//logic
	EvaluableNodeReference InterpretNode_ENT_AND(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_OR(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_XOR(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_NOT(EvaluableNode *en, bool immediate_result);

	//equivalence
	EvaluableNodeReference InterpretNode_ENT_EQUAL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_NEQUAL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_LESS_and_LEQUAL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GREATER_and_GEQUAL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_TYPE_EQUALS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_TYPE_NEQUALS(EvaluableNode *en, bool immediate_result);

	//built-in constants and variables
	EvaluableNodeReference InterpretNode_ENT_TRUE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_FALSE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_NULL(EvaluableNode *en, bool immediate_result);

	//data types
	EvaluableNodeReference InterpretNode_ENT_LIST(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ASSOC(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_NUMBER(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_STRING(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SYMBOL(EvaluableNode *en, bool immediate_result);

	//node types
	EvaluableNodeReference InterpretNode_ENT_GET_TYPE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GET_TYPE_STRING(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_TYPE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_FORMAT(EvaluableNode *en, bool immediate_result);

	//EvaluableNode management: labels, comments, and concurrency
	EvaluableNodeReference InterpretNode_ENT_GET_LABELS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GET_ALL_LABELS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_LABELS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ZIP_LABELS(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_GET_COMMENTS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_COMMENTS(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_GET_CONCURRENCY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_CONCURRENCY(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_GET_VALUE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_VALUE(EvaluableNode *en, bool immediate_result);

	//string
	EvaluableNodeReference InterpretNode_ENT_EXPLODE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SPLIT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SUBSTR(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CONCAT(EvaluableNode *en, bool immediate_result);

	//encryption
	EvaluableNodeReference InterpretNode_ENT_CRYPTO_SIGN(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CRYPTO_SIGN_VERIFY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ENCRYPT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_DECRYPT(EvaluableNode *en, bool immediate_result);

	//I/O
	EvaluableNodeReference InterpretNode_ENT_PRINT(EvaluableNode *en, bool immediate_result);

	//tree merging
	EvaluableNodeReference InterpretNode_ENT_TOTAL_SIZE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MUTATE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_COMMONALITY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_EDIT_DISTANCE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_INTERSECT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_UNION(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_DIFFERENCE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MIX(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MIX_LABELS(EvaluableNode *en, bool immediate_result);

	//entity merging
	EvaluableNodeReference InterpretNode_ENT_TOTAL_ENTITY_SIZE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_FLATTEN_ENTITY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MUTATE_ENTITY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_COMMONALITY_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_EDIT_DISTANCE_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_INTERSECT_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_UNION_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_DIFFERENCE_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MIX_ENTITIES(EvaluableNode *en, bool immediate_result);

	//entity details
	EvaluableNodeReference InterpretNode_ENT_GET_ENTITY_COMMENTS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_RETRIEVE_ENTITY_ROOT(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ASSIGN_ENTITY_ROOTS_and_ACCUM_ENTITY_ROOTS(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GET_ENTITY_RAND_SEED(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_ENTITY_RAND_SEED(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_GET_ENTITY_ROOT_PERMISSION(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_SET_ENTITY_ROOT_PERMISSION(EvaluableNode *en, bool immediate_result);

	//entity base actions
	EvaluableNodeReference InterpretNode_ENT_CREATE_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CLONE_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_MOVE_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_DESTROY_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_LOAD(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_LOAD_ENTITY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_STORE(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_STORE_ENTITY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_ENTITY(EvaluableNode *en, bool immediate_result);

	//entity query
	EvaluableNodeReference InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_QUERY_and_COMPUTE_opcodes(EvaluableNode *en, bool immediate_result);

	//entity access
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_LABEL(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_DIRECT_ASSIGN_TO_ENTITIES_and_ACCUM_TO_ENTITIES(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_RETRIEVE_FROM_ENTITY_and_DIRECT_RETRIEVE_FROM_ENTITY(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CALL_ENTITY_and_CALL_ENTITY_GET_CHANGES(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_CALL_CONTAINER(EvaluableNode *en, bool immediate_result);

	EvaluableNodeReference InterpretNode_ENT_DEALLOCATED(EvaluableNode *en, bool immediate_result);
	EvaluableNodeReference InterpretNode_ENT_NOT_A_BUILT_IN_TYPE(EvaluableNode *en, bool immediate_result);

	//override hook for debugging
	EvaluableNodeReference InterpretNode_DEBUG(EvaluableNode *en, bool immediate_result);

	//override hook for profiling
	EvaluableNodeReference InterpretNode_PROFILE(EvaluableNode *en, bool immediate_result);

	//ensures that there are no reachable nodes that are deallocated
	void VerifyEvaluableNodeIntegrity();

	//if not nullptr, then contains the respective constraints on performance
	PerformanceConstraints *performanceConstraints;

	//a stack (list) of the current nodes being executed
	std::vector<EvaluableNode *> *opcodeStackNodes;

	EvaluableNodeReference ConstructListWithZero();

	EvaluableNodeReference IndexVectorToList(std::vector<size_t> indices, EvaluableNodeManager* evaluabNodeManager, bool immediate_result)
	{
		EvaluableNodeReference index_list;
		index_list.SetReference(evaluableNodeManager->AllocNode(ENT_LIST));
		std::vector<EvaluableNode*>& index_list_ocn = index_list->GetOrderedChildNodesReference();

		for (size_t index: indices)
		{
			index_list_ocn.push_back(AllocReturn(static_cast<double>(index), immediate_result));
		}

		return index_list;
	}

	template <typename Compare>
	EvaluableNodeReference GetIndexMinMaxFromAssoc(EvaluableNode *assoc, Compare compare, double compare_limit, bool immediate_result)
	{
		EvaluableNode::AssocType  mapped_child_nodes = assoc->GetMappedChildNodesReference();
		double candidate_value = compare_limit;
		bool value_found = false;

		std::vector<StringInternPool::StringID> max_keys;

		for(auto [cur_key, cur_child]: mapped_child_nodes)
		{
			double cur_value = InterpretNodeIntoNumberValue(cur_child);

			if(cur_value == candidate_value)
			{
				max_keys.push_back(cur_key);
			}
			else if(compare(cur_value, candidate_value))
			{
				max_keys.clear();
				candidate_value = cur_value;
				max_keys.push_back(cur_key);
				value_found = true;
			}
		}

		if(value_found)
		{
			EvaluableNodeReference index_list;
			index_list.SetReference(evaluableNodeManager->AllocNode(ENT_LIST));
			auto &index_list_ocn = index_list->GetOrderedChildNodesReference();

			for(StringInternPool::StringID max_key: max_keys)
			{
				index_list_ocn.push_back( Parser::ParseFromKeyString(max_key->string, evaluableNodeManager));
			}

			return index_list;
		}

		return EvaluableNodeReference::Null();
	}

	template <typename Compare>
	EvaluableNodeReference GetIndexMinMaxFromList(EvaluableNode *en, std::vector<EvaluableNode*> &orderedChildNodes, Compare compare, double compare_limit, bool immediate_result)
	{
		if(orderedChildNodes.size() == 0)
			return EvaluableNodeReference::Null();

		bool value_found = false;
		double result_value = compare_limit;

		std::vector<EvaluableNodeReference> interpreted_nodes;

	#ifdef MULTITHREAD_SUPPORT
	
		if(InterpretEvaluableNodesConcurrently(en, orderedChildNodes, interpreted_nodes, true))
		{
			std::vector<size_t> max_indices;
			for(size_t i = 0; i < interpreted_nodes.size(); i++)
			{
				//do the comparison and keep the greater
				double cur_value = ConvertNodeIntoNumberValueAndFreeIfPossible(interpreted_nodes[i]);
				if(cur_value == result_value)
				{
					max_indices.push_back(i);
				}
				else if(compare(cur_value, result_value))
				{
					max_indices.clear();
					value_found = true;
					result_value = cur_value;
					max_indices.push_back(i);
				}
			}

			if(value_found)
				return IndexVectorToList(max_indices, evaluableNodeManager, immediate_result);

			return EvaluableNodeReference::Null();
		}
	#endif

		auto node_stack = CreateOpcodeStackStateSaver();

		for(size_t i = 0; i < orderedChildNodes.size(); i++)
		{
			double cur_value = InterpretNodeIntoNumberValue(orderedChildNodes[i]);

			if(cur_value == result_value)
			{
				max_indices.push_back(i);
			}
			else if(compare(cur_value, result_value))
			{
				max_indices.clear();

				value_found = true;
				result_value = cur_value;
				max_indices.push_back(i);
			}
		}

		if(value_found)
			return IndexVectorToList(max_indices, evaluableNodeManager, immediate_result);

		return EvaluableNodeReference::Null();
	}

public:
	//where to allocate new nodes
	EvaluableNodeManager *evaluableNodeManager;

	//Current entity that is being interpreted upon. If null, then it is assumed to be running in sandboxed mode
	Entity *curEntity;

	//random stream to get random numbers from
	RandomStream randomStream;

protected:

	//the call stack is comprised of the variable contexts
	std::vector<EvaluableNode *> *callStackNodes;

	//the current construction stack, containing an interleaved array of nodes
	std::vector<EvaluableNode *> *constructionStackNodes;

	//current index for each level of constructionStackNodes;
	//note, this should always be the same size as constructionStackNodes
	std::vector<ConstructionStackIndexAndPreviousResultUniqueness> constructionStackIndicesAndUniqueness;

	//references to listeners for writes on an Entity and prints
	std::vector<EntityWriteListener *> *writeListeners;
	PrintListener *printListener;

	//buffer to use as for parsing and querying conditions
	//one per thread to save memory on Interpreter objects
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
		static std::vector<EntityQueryCondition> conditionsBuffer;

	//the interpreter that called this one -- used for debugging
	Interpreter *callingInterpreter;

#ifdef MULTITHREAD_SUPPORT
public:
	//mutex to lock the memory from the EvaluableNodeManager it is using
	Concurrency::ReadLock memoryModificationLock;

protected:

	//the depth of the call stack where multiple threads may modify the same variables
	size_t callStackUniqueAccessStartingDepth;

	//pointer to a mutex for writing to shared variables below callStackUniqueAccessStartingDepth
	Concurrency::ReadWriteMutex *callStackMutex;

#endif

	//opcode function pointers
	// each opcode function takes in an EvaluableNode
	typedef EvaluableNodeReference(Interpreter::*OpcodeFunction) (EvaluableNode *, bool);
	static std::array<OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> _opcodes;

	//opcodes that all point to debugging
	// can be swapped with _opcodes
	static std::array<OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> _debug_opcodes;

	//opcodes that all point to profiling
	// can be swapped with _opcodes
	static std::array<OpcodeFunction, ENT_NOT_A_BUILT_IN_TYPE + 1> _profile_opcodes;

	//set to true if opcode profiling is enabled
	static bool _opcode_profiling_enabled;

	//set to true if label profiling is enabled
	static bool _label_profiling_enabled;

	//number of items in each level of the constructionStack
	static constexpr int64_t constructionStackOffsetStride = 4;

	//index of each item for a given level in the constructionStack relative to the size of the stack minus the level * constructionStackOffsetStride
	//target origin is the original node of target useful for keeping track of the reference
	static constexpr int64_t constructionStackOffsetTargetOrigin = -4;
	static constexpr int64_t constructionStackOffsetTarget = -3;
	static constexpr int64_t constructionStackOffsetCurrentValue = -2;
	static constexpr int64_t constructionStackOffsetPreviousResult = -1;
};
