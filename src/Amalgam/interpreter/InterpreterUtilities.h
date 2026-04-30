#pragma once

typedef int64_t ExecutionCycleCount;
typedef int32_t ExecutionCycleCountCompactDelta;

//Manages performance constraints and accompanying performance counters
class InterpreterConstraints
{
public:
	enum class ViolationType
	{
		NoViolation,
		NodeAllocation,
		ExecutionStep,
		ExecutionDepth,
		ContainedEntitiesNumber,
		ContainedEntitiesDepth
	};

	//Adds the string specified by warning to the list of warnings. Takes warning as an rvalue reference.
	void AddWarning(std::string &&warning)
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::WriteLock warningLock(warningMutex);
	#endif
		warnings[warning]++;
	}

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

	//accrues performance counters into the current object from interpreter_constraints
	__forceinline void AccruePerformanceCounters(InterpreterConstraints *interpreter_constraints)
	{
		if(interpreter_constraints == nullptr)
			return;

		curExecutionStep += interpreter_constraints->curExecutionStep;
		curNumAllocatedNodesAllocatedToEntities += interpreter_constraints->curNumAllocatedNodesAllocatedToEntities;
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

	//if true, it will prevent any write or modification operations from happening to any entity
	bool readOnlyEntities;

	//if true, collect warnings, and return them with any constraint violations
	bool collectWarnings;

	ViolationType constraintViolation;

	//maps warnings to the count of their occurrence 
	FastHashMap<std::string, size_t> warnings;

private:
#ifdef MULTITHREAD_SUPPORT
	Concurrency::ReadWriteMutex warningMutex;
#endif

};

//Uses an EvaluableNode as a stack which may already have elements in it
// upon destruction it restores the stack back to the state it was when constructed
class EvaluableNodeStackStateSaver
{
public:
	inline EvaluableNodeStackStateSaver()
		: stack(nullptr), originalStackSize(0)
	{}

	__forceinline EvaluableNodeStackStateSaver(std::vector<EvaluableNode *> *_stack)
	{
		stack = _stack;
		originalStackSize = stack->size();
	}

	//constructor that adds one first element
	__forceinline EvaluableNodeStackStateSaver(std::vector<EvaluableNode *> *_stack, EvaluableNode *initial_element)
	{
		stack = _stack;
		originalStackSize = stack->size();

	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(initial_element == nullptr || initial_element->IsNodeValid());
	#endif

		stack->push_back(initial_element);
	}

	__forceinline ~EvaluableNodeStackStateSaver()
	{
		stack->resize(originalStackSize);
	}

	//ensures that the stack is allocated to hold up to num_new_nodes
	__forceinline void ReserveNodes(size_t num_new_nodes)
	{
		stack->resize(stack->size() + num_new_nodes);
	}

	__forceinline void PushEvaluableNode(EvaluableNode *n)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(n == nullptr || n->IsNodeValid());
	#endif
		stack->push_back(n);
	}

	__forceinline void PopEvaluableNode()
	{
		stack->pop_back();
	}

	//returns the offset to the first element of this state saver
	__forceinline size_t GetIndexOfFirstElement()
	{
		return originalStackSize;
	}

	//returns the offset to the last element of this state saver
	__forceinline size_t GetIndexOfLastElement()
	{
		return stack->size();
	}

	//returns the corresponding element
	__forceinline EvaluableNode *GetStackElement(size_t location)
	{
		return (*stack)[location];
	}

	//replaces the position of the stack with new_value
	__forceinline void SetStackElement(size_t location, EvaluableNode *new_value)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(new_value == nullptr || new_value->IsNodeValid());
	#endif
		(*stack)[location] = new_value;
	}

	std::vector<EvaluableNode *> *stack;
	size_t originalStackSize;
};
