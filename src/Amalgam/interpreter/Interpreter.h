#pragma once

//project headers:
#include "Entity.h"
#include "EntityWriteListener.h"
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"
#include "EvaluableNodeTreeFunctions.h"
#include "FastMath.h"
#include "Parser.h"
#include "PerformanceProfiler.h"
#include "PrintListener.h"
#include "RandomStream.h"

//system headers:
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

//forward declarations:
class EntityQueryCondition;

class Interpreter
{
public:

	//used with construction stack to store the index and whether previous_result is unique
	struct ConstructionStackIndexAndPreviousResultUniqueness
	{
		inline ConstructionStackIndexAndPreviousResultUniqueness(EvaluableNodeImmediateValueWithType _index,
			bool _unique)
			: index(_index), unique(_unique)
		{	}

		EvaluableNodeImmediateValueWithType index;
		bool unique;
	};

	//Creates a new interpreter to run code and to store labels.
	// If no entity is specified via nullptr, then it will run sandboxed
	// Uses max_num_steps as the maximum number of operations that can be executed by this and any subordinate operations called. If max_num_steps is 0, then it will execute unlimeted steps
	// Uses max_num_nodes as the maximum number of nodes that can be allocated in memory by this and any subordinate operations called. If max_num_nodes is 0, then it will allow unlimited allocations
	// max_num_sets is also used for any subsequently limited executions
	Interpreter(EvaluableNodeManager *enm,
		ExecutionCycleCount max_num_steps, size_t max_num_nodes, RandomStream rand_stream,
		std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
		Entity *t = nullptr, Interpreter *calling_interpreter = nullptr
	);
	~Interpreter();

	//Executes the current Entity that this Interpreter is contained by
	// sets up all of the stack and contextual structures, then calls InterpretNode on en
	//if call_stack, interpreter_node_stack, or construction_stack are nullptr, it will start with a new one
	//note that construction_stack and construction_stack_indices should be specified together and should be the same length
#ifdef MULTITHREAD_SUPPORT
	//if run multithreaded, then for performance reasons, it is optimal to have one of each stack per thread
	// and call_stack_write_mutex is the mutex needed to lock for writing
	EvaluableNodeReference ExecuteNode(EvaluableNode *en,
		EvaluableNode *call_stack = nullptr, EvaluableNode *interpreter_node_stack = nullptr,
		EvaluableNode *construction_stack = nullptr,
		std::vector<ConstructionStackIndexAndPreviousResultUniqueness> *construction_stack_indices = nullptr,
		Concurrency::SingleMutex *call_stack_write_mutex = nullptr);
#else
	EvaluableNodeReference ExecuteNode(EvaluableNode *en,
		EvaluableNode *call_stack = nullptr, EvaluableNode *interpreter_node_stack = nullptr,
		EvaluableNode *construction_stack = nullptr,
		std::vector<ConstructionStackIndexAndPreviousResultUniqueness> *construction_stack_indices = nullptr);
#endif

	//changes debugging state to debugging_enabled
	//cannot be enabled at the same time as profiling
	static void SetDebuggingState(bool debugging_enabled);

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
	#ifdef MULTITHREAD_SUPPORT
		evaluableNodeManager->CollectGarbage(&memoryModificationLock);
	#else
		evaluableNodeManager->CollectGarbage();
	#endif
	}

	//pushes new_context on the stack; new_context should be a unique associative array,
	// but if not, it will attempt to put an appropriate unique associative array on callStackNodes
	__forceinline void PushNewExecutionContext(EvaluableNodeReference new_context)
	{
		//make sure unique assoc
		if(EvaluableNode::IsAssociativeArray(new_context))
		{
			if(!new_context.unique)
				new_context.reference = evaluableNodeManager->AllocNode(new_context);
		}
		else //not assoc, make a new one
		{
			evaluableNodeManager->FreeNodeTreeIfPossible(new_context);
			new_context.reference = evaluableNodeManager->AllocNode(ENT_ASSOC);
		}

		//just in case a variable is added which needs cycle checks
		new_context->SetNeedCycleCheck(true);

		callStackNodes->push_back(new_context);
	}

	//pops the top execution context off the stack
	__forceinline void PopExecutionContext()
	{
		if(callStackNodes->size() >= 1)
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
		stack_nodes[new_size + constructionStackOffsetPreviousResult] = previous_result.reference;

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
	inline void PopConstructionContext()
	{
		size_t new_size = constructionStackNodes->size();
		if(new_size > constructionStackOffsetStride)
			new_size -= constructionStackOffsetStride;
		else
			new_size = 0;

		constructionStackNodes->resize(new_size);

		if(constructionStackIndicesAndUniqueness.size() > 0)
			constructionStackIndicesAndUniqueness.pop_back();
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
		constructionStackNodes->at(constructionStackNodes->size() + constructionStackOffsetCurrentValue) = value;
	}

	//sets the previous_result node for the top reference on the construction stack
	//assumes there is at least one construction stack entry
	__forceinline void SetTopPreviousResultInConstructionStack(EvaluableNodeReference previous_result)
	{
		constructionStackNodes->at(constructionStackNodes->size() + constructionStackOffsetPreviousResult) = previous_result.reference;
		constructionStackIndicesAndUniqueness.back().unique = previous_result.unique;
	}

	//gets the previous_result node for the top reference on the construction stack
	//assumes there is at least one construction stack entry
	__forceinline EvaluableNodeReference GetTopPreviousResultInConstructionStack()
	{
		return EvaluableNodeReference(constructionStackNodes->at(constructionStackNodes->size() + constructionStackOffsetPreviousResult),
			constructionStackIndicesAndUniqueness.back().unique);
	}

	//Makes sure that args is an active associative array is proper for execution context, meaning initialized assoc and a unique reference.
	// Will allocate a new node appropriately if it is not
	//Then wraps the args on a list which will form the execution context stack and returns that
	//ensures that args is still a valid EvaluableNodeReference after the call
	static EvaluableNodeReference ConvertArgsToCallStack(EvaluableNodeReference &args, EvaluableNodeManager *enm);

	//finds a pointer to the location of the symbol's pointer to value in the top of the context stack and returns a pointer to the location of the symbol's pointer to value,
	// nullptr if it does not exist
	// also sets call_stack_index to the level in the call stack that it was found
	EvaluableNode **GetExecutionContextSymbolLocation(const StringInternPool::StringID symbol_sid, size_t &call_stack_index);

	//like the other type of GetExecutionContextSymbolLocation, but returns the EvaluableNode pointer instead of a pointer-to-a-pointer
	__forceinline EvaluableNode *GetExecutionContextSymbol(const StringInternPool::StringID symbol_sid)
	{
		size_t call_stack_index = 0;
		EvaluableNode **en_ptr = GetExecutionContextSymbolLocation(symbol_sid, call_stack_index);
		if(en_ptr == nullptr)
			return nullptr;

		return *en_ptr;
	}

	//finds a pointer to the location of the symbol's pointer to value or creates the symbol in the top of the context stack and returns a pointer to the location of the symbol's pointer to value
	// also sets call_stack_index to the level in the call stack that it was found
	EvaluableNode **GetOrCreateExecutionContextSymbolLocation(const StringInternPool::StringID symbol_sid, size_t &call_stack_index);

	//creates a stack state saver for the interpreterNodeStack, which will be restored back to its previous condition when this object is destructed
	__forceinline EvaluableNodeStackStateSaver CreateInterpreterNodeStackStateSaver()
	{
		return EvaluableNodeStackStateSaver(interpreterNodeStackNodes);
	}

	//like CreateInterpreterNodeStackStateSaver, but also pushes another node on the stack
	__forceinline EvaluableNodeStackStateSaver CreateInterpreterNodeStackStateSaver(EvaluableNode *en)
	{
		//count on C++ return value optimization to not call the destructor
		return EvaluableNodeStackStateSaver(interpreterNodeStackNodes, en);
	}

	//keeps the current node on the stack and calls InterpretNodeExecution
	EvaluableNodeReference InterpretNode(EvaluableNode *en);

	//returns the number of steps executed since Interpreter was created
	constexpr ExecutionCycleCount GetNumStepsExecuted()
	{	return curExecutionStep;	}

	//returns the number of nodes allocated to all contained entities since Interpreter was created
	constexpr size_t GetNumEntityNodesAllocated()
	{	return curNumExecutionNodesAllocatedToEntities;	}

	//Current entity that is being interpreted upon. If null, then it is assumed to be running in sandboxed mode
	Entity *curEntity;

	//random stream to get random numbers from
	RandomStream randomStream;

	//references to listeners for writes on an Entity and prints
	std::vector<EntityWriteListener *> *writeListeners;
	PrintListener *printListener;

	//where to allocate new nodes
	EvaluableNodeManager *evaluableNodeManager;

	//returns the current execution context, nullptr if none
	EvaluableNode *GetCurrentExecutionContext();

	//if n is immediate, it just returns it, otherwise calls InterpretNode
	__forceinline EvaluableNodeReference InterpretNodeForImmediateUse(EvaluableNode *n)
	{
		if(n == nullptr || n->GetIsIdempotent())
			return EvaluableNodeReference(n, false);
		return InterpretNode(n);
	}

	//Calls InterpretNode on n, converts to std::string and stores in value to return, then cleans up any resources used
	//returns a pair of bool, whether it was a valid string (and not NaS), and the string
	std::pair<bool, std::string> InterpretNodeIntoStringValue(EvaluableNode *n);

	//Calls InterpretNode on n, converts to std::string and stores in value to return, then cleans up any resources used
	// but if n is null, it will return an empty string
	std::string InterpretNodeIntoStringValueEmptyNull(EvaluableNode *n);

	//like InterpretNodeIntoStringValue, but returns the ID only if the string already exists, otherwise it returns NOT_A_STRING_ID
	StringInternPool::StringID InterpretNodeIntoStringIDValueIfExists(EvaluableNode *n);

	//like InterpretNodeIntoStringValue, but creates a reference to the string that must be destroyed, regardless of whether the string existed or not (if it did not exist, then it creates one)
	StringInternPool::StringID InterpretNodeIntoStringIDValueWithReference(EvaluableNode *n);

	//Calls InterpnetNode on n, convers to a string, and makes sure that the node returned is new and unique so that it can be modified
	EvaluableNode *InterpretNodeIntoUniqueStringIDValueEvaluableNode(EvaluableNode *n);

	//Calls InterpretNode on n, converts to double and returns, then cleans up any resources used
	double InterpretNodeIntoNumberValue(EvaluableNode *n);

	//Calls InterpnetNode on n, convers to a double, and makes sure that the node returned is new and unique so that it can be modified
	EvaluableNode *InterpretNodeIntoUniqueNumberValueEvaluableNode(EvaluableNode *n);

	//Calls InterpretNode on n, converts to boolean and returns, then cleans up any resources used
	bool InterpretNodeIntoBoolValue(EvaluableNode *n, bool value_if_null = false);

	//Calls InterpretNode on n, converts n into a destination for an Entity, relative to curEntity.
	// If invalid, destination_entity_parent will be curEntity and new_entity_id will be the empty string
	//new_entity_id is an alocated string, and the caller is responsible for freeing it
	void InterpretNodeIntoDestinationEntity(EvaluableNode *n, Entity *&destination_entity_parent, StringInternRef &new_entity_id);

	//traverses source based on traversal path list tpl
	// If create_destination_if_necessary is set, then it will expand anything in the source as appropriate
	//Returns the location of the EvaluableNode * of the destination, nullptr if it does not exist
	EvaluableNode **TraverseToDestinationFromTraversalPathList(EvaluableNode **source, EvaluableNodeReference &tpl, bool create_destination_if_necessary);

	//calls InterpretNode on tpl, traverses source based on tpl.
	// If create_destination_if_necessary is set, then it will expand anything in the source as appropriate
	//Returns the location of the EvaluableNode * of the destination, nullptr if it does not exist
	__forceinline EvaluableNode **InterpretNodeIntoDestinationFromTraversalPathList(EvaluableNode **source,
		EvaluableNode *tpl, bool create_destination_if_necessary)
	{
		EvaluableNodeReference address_list_node = InterpretNodeForImmediateUse(tpl);
		EvaluableNode **destination = TraverseToDestinationFromTraversalPathList(source, address_list_node, create_destination_if_necessary);
		evaluableNodeManager->FreeNodeTreeIfPossible(address_list_node);
		return destination;
	}

	//Interprets node_id_path_to_interpret and then attempts to find the Entity relative to curEntity. Returns nullptr if cannot find
	template<typename EntityReferenceType>
	EntityReferenceType InterpretNodeIntoRelativeSourceEntityReferenceFromInterpretedEvaluableNodeIDPath(EvaluableNode *node_id_path_to_interpret)
	{
		if(curEntity == nullptr)
			return EntityReferenceType(nullptr);

		if(EvaluableNode::IsEmptyNode(node_id_path_to_interpret))
			return EntityReferenceType(curEntity);

		//only need to interpret if not idempotent
		EvaluableNodeReference source_id_node = InterpretNodeForImmediateUse(node_id_path_to_interpret);
		EntityReferenceType source_entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReferenceType>(curEntity, source_id_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(source_id_node);

		return source_entity;
	}

	//like InterpretNodeIntoRelativeSourceEntityReferenceFromInterpretedEvaluableNodeIDPath but with a read reference
	inline EntityReadReference InterpretNodeIntoRelativeSourceEntityReadReferenceFromInterpretedEvaluableNodeIDPath(EvaluableNode *node_id_path_to_interpret)
	{
		return InterpretNodeIntoRelativeSourceEntityReferenceFromInterpretedEvaluableNodeIDPath<EntityReadReference>(node_id_path_to_interpret);
	}

	//like InterpretNodeIntoRelativeSourceEntityReferenceFromInterpretedEvaluableNodeIDPath but with a write reference
	inline EntityWriteReference InterpretNodeIntoRelativeSourceEntityWriteReferenceFromInterpretedEvaluableNodeIDPath(EvaluableNode *node_id_path_to_interpret)
	{
		return InterpretNodeIntoRelativeSourceEntityReferenceFromInterpretedEvaluableNodeIDPath<EntityWriteReference>(node_id_path_to_interpret);
	}

protected:

	//Traverses down n until it reaches the furthest-most nodes from top_node, then bubbles back up re-evaluating each node via the specified function
	// Returns the (potentially) modified tree of n, modified in-place
	EvaluableNode *RewriteByFunction(EvaluableNodeReference function, EvaluableNode *top_node, EvaluableNode *n, EvaluableNode::ReferenceSetType &references);

#ifdef MULTITHREAD_SUPPORT

	//class to manage the data for concurrent execution by an interpreter
	class ConcurrencyManager
	{
	public:

		//constructs the concurrency manager.  Assumes parent_interpreter is NOT null
		ConcurrencyManager(Interpreter *parent_interpreter, size_t num_elements)
		{
			parentInterpreter = parent_interpreter;
			numElements = num_elements;

			//set up data
			interpreters.reserve(numElements);
			resultFutures.reserve(numElements);

			size_t max_execution_steps_per_element = 0;
			if(parentInterpreter->maxNumExecutionSteps > 0)
				max_execution_steps_per_element = (parentInterpreter->maxNumExecutionSteps - parentInterpreter->GetNumStepsExecuted()) / numElements;

			//set up all the interpreters
			// do this as its own loop to make sure that the vector memory isn't reallocated once the threads have kicked off
			for(size_t element_index = 0; element_index < numElements; element_index++)
			{
				//create interpreter
				interpreters.emplace_back(std::make_unique<Interpreter>(parentInterpreter->evaluableNodeManager, max_execution_steps_per_element, parentInterpreter->maxNumExecutionNodes,
					parentInterpreter->randomStream.CreateOtherStreamViaRand(),
					parentInterpreter->writeListeners, parentInterpreter->printListener, parentInterpreter->curEntity));
			}

			//begins concurrency over all interpreters
			parentInterpreter->memoryModificationLock.unlock();
		}

		//Enqueues a concurrent task resultFutures that needs a construction stack, using the relative interpreter
		// executes node_to_execute with the following parameters matching those of pushing on the construction stack
		// will allocate an approrpiate node matching the type of current_index
		void PushTaskToResultFuturesWithConstructionStack(EvaluableNode *node_to_execute,
			EvaluableNode *target_origin, EvaluableNode *target,
			EvaluableNodeImmediateValueWithType current_index,
			EvaluableNode *current_value, EvaluableNodeReference previous_result = EvaluableNodeReference::Null())
		{
			//get the interpreter corresponding to the resultFutures
			Interpreter *interpreter = interpreters[resultFutures.size()].get();

			resultFutures.emplace_back(
				Concurrency::threadPool.EnqueueBatchTask(
					[this, interpreter, node_to_execute, target_origin, target, current_index, current_value, previous_result]
					{
						EvaluableNodeManager *enm = interpreter->evaluableNodeManager;
						interpreter->memoryModificationLock = Concurrency::ReadLock(enm->memoryModificationMutex);

						//build new construction stack
						EvaluableNode *construction_stack = enm->AllocListNode(parentInterpreter->constructionStackNodes);
						std::vector<ConstructionStackIndexAndPreviousResultUniqueness> csiau(parentInterpreter->constructionStackIndicesAndUniqueness);
						interpreter->PushNewConstructionContextToStack(construction_stack->GetOrderedChildNodes(),
							csiau, target_origin, target, current_index, current_value, previous_result);

						auto result = interpreter->ExecuteNode(node_to_execute,
							enm->AllocListNode(parentInterpreter->callStackNodes),
							enm->AllocListNode(parentInterpreter->interpreterNodeStackNodes),
							construction_stack,
							&csiau,
							GetCallStackWriteMutex());

						enm->KeepNodeReference(result);

						interpreter->memoryModificationLock.unlock();
						return result;
					}
				)
			);
		}

		//ends concurrency from all interpreters and waits for them to finish
		inline void EndConcurrency()
		{
			Concurrency::threadPool.CountCurrentThreadAsPaused();

			//make sure all futures return before moving on
			for(auto &future : resultFutures)
				future.wait();

			if(!parentInterpreter->AllowUnlimitedExecutionSteps())
			{
				for(auto &i : interpreters)
					parentInterpreter->curExecutionStep += i->curExecutionStep;
			}

			Concurrency::threadPool.CountCurrentThreadAsResumed();

			parentInterpreter->memoryModificationLock.lock();
		}

		//returns results from the futures
		// assumes that each result has had KeepNodeReference called upon it, otherwise it'd have not been safe,
		// so it calls FreeNodeReference on each
		inline std::vector<EvaluableNodeReference> GetResultsAndFreeReferences()
		{
			std::vector<EvaluableNodeReference> results;
			results.resize(numElements);

			//fill in results from result_futures and free references
			// note that std::future becomes invalid once get is called
			for(size_t i = 0; i < numElements; i++)
				results[i] = resultFutures[i].get();

			parentInterpreter->evaluableNodeManager->FreeNodeReferences(results);

			return results;
		}

		//returns the relevant write mutex for the call stack
		constexpr Concurrency::SingleMutex *GetCallStackWriteMutex()
		{
			//if there is one currently in use, use it
			if(parentInterpreter->callStackWriteMutex != nullptr)
				return parentInterpreter->callStackWriteMutex;

			//start a new one
			return &callStackWriteMutex;
		}

		//interpreters run concurrently, the size of numElements
		std::vector<std::unique_ptr<Interpreter>> interpreters;

		//where results are placed, the size of numElements
		std::vector<std::future<EvaluableNodeReference>> resultFutures;

		//mutex to allow only one thread to write to a call stack symbol at once
		Concurrency::SingleMutex callStackWriteMutex;

	protected:
		//interpreter that is running all the concurrent interpreters
		Interpreter *parentInterpreter;

		//the number of elements being processed
		size_t numElements;
	};

	//computes the nodes concurrently and stores the interpreted values into interpreted_nodes
	// looks to parent_node to whether concurrency is enabled
	//returns true if it is able to interpret the nodes concurrently
	bool InterpretEvaluableNodesConcurrently(EvaluableNode *parent_node, std::vector<EvaluableNode *> &nodes, std::vector<EvaluableNodeReference> &interpreted_nodes);

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
			if(cur_interpreter->callStackSharedAccessStartingDepth > 0)
				return false;
		#endif
		}

		return true;
	}

	//recalculates curNumExecutionNodes
	__forceinline void UpdateCurNumExecutionNodes()
	{
		curNumExecutionNodes = curNumExecutionNodesAllocatedToEntities + evaluableNodeManager->GetNumberOfUsedNodes();
	}

	//if true, no limit to how long can utilize CPU
	constexpr bool AllowUnlimitedExecutionSteps()
	{	return maxNumExecutionSteps == 0;	}

	constexpr ExecutionCycleCount GetRemainingNumExecutionSteps()
	{
		if(curExecutionStep < maxNumExecutionSteps)
			return maxNumExecutionSteps - curExecutionStep;
		else //already past limit
			return 0;
	}

	//if true, no limit on how much memory can utilize
	constexpr bool AllowUnlimitedExecutionNodes()
	{	return maxNumExecutionNodes == 0;	}

	constexpr size_t GetRemainingNumExecutionNodes()
	{
		if(curNumExecutionNodes < maxNumExecutionNodes)
			return maxNumExecutionNodes - curNumExecutionNodes;
		else //already past limit
			return 0;
	}

	//returns true if there's a max number of execution steps or nodes and at least one is exhausted
	constexpr bool AreExecutionResourcesExhausted()
	{
		if(!AllowUnlimitedExecutionSteps() && curExecutionStep >= maxNumExecutionSteps)
			return true;

		if(!AllowUnlimitedExecutionNodes() && curNumExecutionNodes >= maxNumExecutionNodes)
			return true;

		return false;
	}

	//opcodes
	//returns an EvaluableNode tree from evaluating the tree passed in (or nullptr) and associated properties in an EvaluableNodeReference
	//prior to calling, en must be referenced (via KeepNodeReference, or part of an entity) so it will not be garbage collected
	//further, for performance, en must be guaranteed to be a valid pointer, and not nullptr

	//built-in / system specific
	EvaluableNodeReference InterpretNode_ENT_SYSTEM(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GET_DEFAULTS(EvaluableNode *en);

	//parsing
	EvaluableNodeReference InterpretNode_ENT_PARSE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_UNPARSE(EvaluableNode *en);

	//core control
	EvaluableNodeReference InterpretNode_ENT_IF(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SEQUENCE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_PARALLEL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_LAMBDA(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CONCLUDE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CALL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CALL_SANDBOXED(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_WHILE(EvaluableNode *en);

	//definitions
	EvaluableNodeReference InterpretNode_ENT_LET(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_DECLARE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ASSIGN_and_ACCUM(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_RETRIEVE(EvaluableNode *en);

	//base math
	EvaluableNodeReference InterpretNode_ENT_ADD(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SUBTRACT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MULTIPLY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_DIVIDE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MODULUS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GET_DIGITS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_DIGITS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_FLOOR(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CEILING(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ROUND(EvaluableNode *en);

	//extended math
	EvaluableNodeReference InterpretNode_ENT_EXPONENT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_LOG(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_SIN(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ASIN(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_COS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ACOS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_TAN(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ATAN(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_SINH(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ASINH(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_COSH(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ACOSH(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_TANH(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ATANH(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_ERF(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_TGAMMA(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_LGAMMA(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_SQRT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_POW(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ABS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MAX(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MIN(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_DOT_PRODUCT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GENERALIZED_DISTANCE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ENTROPY(EvaluableNode *en);

	//list manipulation
	EvaluableNodeReference InterpretNode_ENT_FIRST(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_TAIL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_LAST(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_TRUNC(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_APPEND(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SIZE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_RANGE(EvaluableNode *en);

	//transformation
	EvaluableNodeReference InterpretNode_ENT_REWRITE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MAP(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_FILTER(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_WEAVE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_REDUCE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_APPLY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_REVERSE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SORT(EvaluableNode *en);

	//associative list manipulation
	EvaluableNodeReference InterpretNode_ENT_INDICES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_VALUES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_INDEX(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_VALUE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_REMOVE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_KEEP(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ASSOCIATE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ZIP(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_UNZIP(EvaluableNode *en);

	//retrieval
	EvaluableNodeReference InterpretNode_ENT_GET(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_and_REPLACE(EvaluableNode *en);

	//stack and node manipulation
	EvaluableNodeReference InterpretNode_ENT_TARGET(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CURRENT_INDEX(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CURRENT_VALUE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_PREVIOUS_RESULT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_STACK(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ARGS(EvaluableNode *en);

	//logic
	EvaluableNodeReference InterpretNode_ENT_AND(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_OR(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_XOR(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_NOT(EvaluableNode *en);

	//equivalence
	EvaluableNodeReference InterpretNode_ENT_EQUAL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_NEQUAL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_LESS_and_LEQUAL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GREATER_and_GEQUAL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_TYPE_EQUALS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_TYPE_NEQUALS(EvaluableNode *en);

	//simulation and operations
	EvaluableNodeReference InterpretNode_ENT_RAND(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_WEIGHTED_RAND(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GET_RAND_SEED(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_RAND_SEED(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SYSTEM_TIME(EvaluableNode *en);

	//built-in constants and variables
	EvaluableNodeReference InterpretNode_ENT_TRUE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_FALSE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_NULL(EvaluableNode *en);

	//data types
	EvaluableNodeReference InterpretNode_ENT_LIST(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ASSOC(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_NUMBER(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_STRING(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SYMBOL(EvaluableNode *en);

	//node types
	EvaluableNodeReference InterpretNode_ENT_GET_TYPE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GET_TYPE_STRING(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_TYPE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_FORMAT(EvaluableNode *en);

	//EvaluableNode management: labels, comments, and concurrency
	EvaluableNodeReference InterpretNode_ENT_GET_LABELS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GET_ALL_LABELS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_LABELS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ZIP_LABELS(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_GET_COMMENTS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_COMMENTS(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_GET_CONCURRENCY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_CONCURRENCY(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_GET_VALUE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_VALUE(EvaluableNode *en);

	//string
	EvaluableNodeReference InterpretNode_ENT_EXPLODE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SPLIT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SUBSTR(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CONCAT(EvaluableNode *en);

	//encryption
	EvaluableNodeReference InterpretNode_ENT_CRYPTO_SIGN(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CRYPTO_SIGN_VERIFY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ENCRYPT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_DECRYPT(EvaluableNode *en);

	//I/O
	EvaluableNodeReference InterpretNode_ENT_PRINT(EvaluableNode *en);

	//tree merging
	EvaluableNodeReference InterpretNode_ENT_TOTAL_SIZE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MUTATE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_COMMONALITY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_EDIT_DISTANCE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_INTERSECT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_UNION(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_DIFFERENCE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MIX(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MIX_LABELS(EvaluableNode *en);

	//entity merging
	EvaluableNodeReference InterpretNode_ENT_TOTAL_ENTITY_SIZE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_FLATTEN_ENTITY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MUTATE_ENTITY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_COMMONALITY_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_EDIT_DISTANCE_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_INTERSECT_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_UNION_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_DIFFERENCE_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MIX_ENTITIES(EvaluableNode *en);

	//entity details
	EvaluableNodeReference InterpretNode_ENT_GET_ENTITY_COMMENTS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_RETRIEVE_ENTITY_ROOT(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ASSIGN_ENTITY_ROOTS_and_ACCUM_ENTITY_ROOTS(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GET_ENTITY_RAND_SEED(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_ENTITY_RAND_SEED(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_GET_ENTITY_ROOT_PERMISSION(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_SET_ENTITY_ROOT_PERMISSION(EvaluableNode *en);

	//entity base actions
	EvaluableNodeReference InterpretNode_ENT_CREATE_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CLONE_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_MOVE_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_DESTROY_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_LOAD(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_LOAD_ENTITY_and_LOAD_PERSISTENT_ENTITY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_STORE(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_STORE_ENTITY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_ENTITY(EvaluableNode *en);

	//entity query
	EvaluableNodeReference InterpretNode_ENT_CONTAINED_ENTITIES_and_COMPUTE_ON_CONTAINED_ENTITIES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_QUERY_and_COMPUTE_opcodes(EvaluableNode *en);

	//entity access
	EvaluableNodeReference InterpretNode_ENT_CONTAINS_LABEL(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_DIRECT_ASSIGN_TO_ENTITIES_and_ACCUM_TO_ENTITIES(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_RETRIEVE_FROM_ENTITY_and_DIRECT_RETRIEVE_FROM_ENTITY(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CALL_ENTITY_and_CALL_ENTITY_GET_CHANGES(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_CALL_CONTAINER(EvaluableNode *en);

	EvaluableNodeReference InterpretNode_ENT_DEALLOCATED(EvaluableNode *en);
	EvaluableNodeReference InterpretNode_ENT_NOT_A_BUILT_IN_TYPE(EvaluableNode *en);

	//override hook for debugging
	EvaluableNodeReference InterpretNode_DEBUG(EvaluableNode *en);

	//override hook for profiling
	EvaluableNodeReference InterpretNode_PROFILE(EvaluableNode *en);

	//ensures that there are no reachable nodes that are deallocated
	void ValidateEvaluableNodeIntegrity();

	//Current execution step - number of nodes executed
	ExecutionCycleCount curExecutionStep;

	//Maximum number of execution steps by this Interpreter and anything called from it.  If 0, then unlimited.
	//Will terminate execution if the value is reached
	ExecutionCycleCount maxNumExecutionSteps;

	//Current number of nodes created by this interpreter, to be compared to maxNumExecutionNodes
	// should be the sum of curNumExecutionNodesAllocatedToEntities plus any temporary nodes
	size_t curNumExecutionNodes;

	//number of nodes allocated only to entities
	size_t curNumExecutionNodesAllocatedToEntities;

	//Maximum number of nodes allowed to be allocated by this Interpreter and anything called from it.  If 0, then unlimited.
	//Will terminate execution if the value is reached
	size_t maxNumExecutionNodes;

	//The current execution context; the call stack
	std::vector<EvaluableNode *> *callStackNodes;

	//A stack (list) of the current nodes being executed
	std::vector<EvaluableNode *> *interpreterNodeStackNodes;

	//The current construction stack, containing an interleaved array of nodes
	std::vector<EvaluableNode *> *constructionStackNodes;

	//current index for each level of constructionStackNodes;
	//note, this should always be the same size as constructionStackNodes
	std::vector<ConstructionStackIndexAndPreviousResultUniqueness> constructionStackIndicesAndUniqueness;

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
	size_t callStackSharedAccessStartingDepth;

	//pointer to a mutex for writing to shared variables below callStackSharedAccessStartingDepth
	//note that reading does not need to be synchronized because the writes are done with regard to pointers,
	// which are an atomic operation on every major processor in the world, and even Linux core libraries are built on this assumption
	Concurrency::SingleMutex *callStackWriteMutex;

	//buffer to store read locks for deep locking entities
	Concurrency::ReadLocksBuffer entityReadLockBuffer;

	//buffer to store write locks for deep locking entities
	Concurrency::WriteLocksBuffer entityWriteLockBuffer;

#endif

	//opcode function pointers
	// each opcode function takes in an EvaluableNode
	typedef EvaluableNodeReference(Interpreter::*OpcodeFunction) (EvaluableNode *);
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
