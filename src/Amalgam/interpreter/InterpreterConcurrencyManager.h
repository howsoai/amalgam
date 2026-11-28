#pragma once
//project headers:
#include "Interpreter.h"

#ifdef MULTITHREAD_SUPPORT

namespace InterpreterConcurrencyManagerTraits
{
	//base traits - can be left empty or used for defaults
	struct Default
	{
		static constexpr size_t BufferSize = 80;
		using ModeTag = void;
	};

	struct NoStack : Default
	{
		//must match the parameter size of the tasks.emplace_back lambda in 
		static constexpr size_t BufferSize = 6 * sizeof(size_t);
		struct SimpleMode
		{};
		using ModeTag = SimpleMode;
	};

	struct StackWithoutResults : Default
	{
		//must match the parameter size of the tasks.emplace_back lambda in 
		static constexpr size_t BufferSize = 5 * sizeof(size_t);
		struct StackWithoutResultsMode
		{};
		using ModeTag = StackWithoutResultsMode;
	};

	struct StackWithResults : Default
	{
		//must match the parameter size of the tasks.emplace_back lambda in 
		static constexpr size_t BufferSize = 7 * sizeof(size_t);
		struct StackWithResultMode
		{};
		using ModeTag = StackWithResultMode;
	};
}

//empty base class to be able to compare pointers
class InterpreterConcurrencyManagerBase
{};

//class to manage the data for concurrent execution by an interpreter
template<typename Traits = InterpreterConcurrencyManagerTraits::Default>
class InterpreterConcurrencyManager : public InterpreterConcurrencyManagerBase
{
public:

	//constructs the concurrency manager.  Assumes parent_interpreter is NOT null
	InterpreterConcurrencyManager(Interpreter *parent_interpreter, size_t num_tasks,
		EvaluableNode *target_origin_node, EvaluableNodeReference *target_node)
	{
		targetOriginNode = target_origin_node;
		targetNode = target_node;
		resultsUnique = true;
		resultsUniqueUnreferencedTopNode = true;
		resultsNeedCycleCheck = false;
		resultsIdempotent = true;
		resultsSideEffect = false;

		parentInterpreter = parent_interpreter;
		numTasks = num_tasks;
		curNumTasksAdded = 0;
		tasks.reserve(num_tasks);

		//create space to store all of these nodes on the stack, but won't copy these over to the other interpreters
		resultsSaver = parent_interpreter->CreateOpcodeStackStateSaver();
		resultsSaverFirstTaskOffset = resultsSaver.GetIndexOfFirstElement();
		resultsSaver.ReserveNodes(num_tasks);

		//since each thread has a copy of the constructionStack, it's possible that more than one of the threads
		//obtains previous_results, so they must all be marked as not unique
		parentInterpreter->RemoveUniquenessFromPreviousResultsInConstructionStack();

		//need to create a mutex for all interpreters that will be called
		previousScopeStackMutex = std::exchange(parentInterpreter->scopeStackMutex, &scopeStackMutex);
	}

	//reuses the reusable lock if possible, otherwise initializes the lock
	inline void InitializeReusableLock(Interpreter &interpreter)
	{
		if(Interpreter::reusableReadLock.owns_lock())
		{
			if(this == Interpreter::reusableReadLockConcurrencyManager)
			{
				interpreter.memoryModificationLock = std::move(Interpreter::reusableReadLock);
				Interpreter::reusableReadLockConcurrencyManager = nullptr;
				return;
			}

			//this should not happen
			assert(false);
		}

		//can't reuse the lock
		interpreter.memoryModificationLock = Concurrency::ReadLock(
			parentInterpreter->evaluableNodeManager->GetMemoryModificationMutex());
		Interpreter::reusableReadLockConcurrencyManager = nullptr;
	}

	//lets the next task for this thread reuse the lock without having to unlock
	inline void ReleaseReusableLock(Interpreter &interpreter)
	{
		Interpreter::reusableReadLock = std::move(interpreter.memoryModificationLock);
		Interpreter::reusableReadLockConcurrencyManager = this;
	}

	//Adds a child task to the runtime group that needs a construction stack, using the relative interpreter
	// executes node_to_execute with the following parameters matching those of pushing on the construction stack
	// will allocate an appropriate node matching the type of current_index
	//result is set to the result of the task
	template<typename EvaluableNodeRefType>
		requires std::is_same_v<typename Traits::ModeTag,
			typename InterpreterConcurrencyManagerTraits::StackWithResults::ModeTag>
	void AddTaskWithConstructionStackWithResult(EvaluableNode *node_to_execute,
		EvaluableNodeImmediateValueWithType current_index,
		EvaluableNode *current_value,
		EvaluableNodeRefType &result)
	{
		size_t task_index = curNumTasksAdded++;
		auto rand_seed = parentInterpreter->randomStream.CreateOtherStreamViaRand();

		//must keep InterpreterConcurrencyManagerTraits::StackWithResults up to date with the
		// number of params of the following lambda
		tasks.emplace_back(
			[this, task_index, rand_seed, node_to_execute, current_index, current_value, &result]
		{
			Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
				parentInterpreter->writeListeners, parentInterpreter->printListener,
				parentInterpreter->interpreterConstraints, parentInterpreter->curEntity, parentInterpreter);

			InitializeReusableLock(interpreter);

			//build new construction stack
			std::vector<ConstructionStackEntry> construction_stack(parentInterpreter->constructionStack);
			construction_stack.emplace_back(targetOriginNode, targetNode, current_index, current_value, EvaluableNodeReference::Null());

			std::vector<EvaluableNode *> opcode_stack(begin(parentInterpreter->opcodeStackNodes),
				begin(parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset);

			auto result_ref = interpreter.ExecuteNode(node_to_execute,
				nullptr, &opcode_stack, &construction_stack, EvaluableNodeRequestedValueTypes::Type::EVALUABLE_NODE, false);

			if(interpreter.PopConstructionContextAndGetExecutionSideEffectFlag())
			{
				resultsSideEffect = true;
				resultsUnique = false;
				resultsUniqueUnreferencedTopNode = false;
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
			resultsSaver.SetStackElement(resultsSaverFirstTaskOffset + task_index, result);

			ReleaseReusableLock(interpreter);
		}
		);
	}

	//like the previous definition of AddTaskWithConstructionStackWithResult,
	//but without keeping results or building a target
	template<typename EvaluableNodeRefType>
		requires std::is_same_v<typename Traits::ModeTag,
			typename InterpreterConcurrencyManagerTraits::StackWithoutResults::ModeTag>
	void AddTaskWithConstructionStackWithoutResult(EvaluableNode *node_to_execute,
		EvaluableNodeImmediateValueWithType current_index,
		EvaluableNode *current_value)
	{
		curNumTasksAdded++;
		auto rand_seed = parentInterpreter->randomStream.CreateOtherStreamViaRand();

		//must keep InterpreterConcurrencyManagerTraits::StackWithoutResults up to date with the
		// number of params of the following lambda
		tasks.emplace_back(
			[this, rand_seed, node_to_execute, current_index, current_value]
		{
			Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
				parentInterpreter->writeListeners, parentInterpreter->printListener,
				parentInterpreter->interpreterConstraints, parentInterpreter->curEntity, parentInterpreter);

			InitializeReusableLock(interpreter);

			//build new construction stack
			std::vector<ConstructionStackEntry> construction_stack(parentInterpreter->constructionStack);
			construction_stack.emplace_back(nullptr, &Interpreter::_null_reference, current_index, current_value, EvaluableNodeReference::Null());

			std::vector<EvaluableNode *> opcode_stack(begin(parentInterpreter->opcodeStackNodes),
				begin(parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset);

			auto result = interpreter.ExecuteNode(node_to_execute, nullptr, &opcode_stack, &construction_stack,
				EvaluableNodeRequestedValueTypes::Type::NULL_VALUE, false);

			interpreter.PopConstructionContextAndGetExecutionSideEffectFlag();
			parentInterpreter->evaluableNodeManager->FreeNodeTreeIfPossible(result);

			ReleaseReusableLock(interpreter);
		}
		);
	}

	//Adds a child task to the runtime group using the relative interpreter, executing node_to_execute
	//if result is specified, it will store the result there, otherwise it will free it
	template<typename EvaluableNodeRefType>
		requires std::is_same_v<typename Traits::ModeTag,
			typename InterpreterConcurrencyManagerTraits::NoStack::ModeTag>
	void AddTask(EvaluableNode *node_to_execute,
		EvaluableNodeRefType *result = nullptr, EvaluableNodeRequestedValueTypes immediate_results = false)
	{
		size_t task_index = curNumTasksAdded++;
		auto rand_seed = parentInterpreter->randomStream.CreateOtherStreamViaRand();

		//must keep InterpreterConcurrencyManagerTraits::NoStack up to date with the
		// number of params of the following lambda
		tasks.emplace_back(
			[this, task_index, rand_seed, node_to_execute, result, immediate_results]
		{
			Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
				parentInterpreter->writeListeners, parentInterpreter->printListener,
				parentInterpreter->interpreterConstraints, parentInterpreter->curEntity, parentInterpreter);

			InitializeReusableLock(interpreter);

			std::vector<EvaluableNode *> opcode_stack(begin(parentInterpreter->opcodeStackNodes),
				begin(parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset);
			std::vector<ConstructionStackEntry> construction_stack(parentInterpreter->constructionStack);

			auto result_ref = interpreter.ExecuteNode(node_to_execute, nullptr, &opcode_stack,
				&construction_stack, immediate_results, false);

			if(interpreter.DoesConstructionStackHaveExecutionSideEffects())
				resultsSideEffect = true;

			if(result == nullptr)
			{
				parentInterpreter->evaluableNodeManager->FreeNodeTreeIfPossible(result_ref);
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
					resultsSaver.SetStackElement(resultsSaverFirstTaskOffset + task_index, *result);
			}

			ReleaseReusableLock(interpreter);
		}
		);
	}

	//ends concurrency from all interpreters and waits for them to finish
	inline void EndConcurrency()
	{
		//allow other threads to perform garbage collection
		parentInterpreter->memoryModificationLock.unlock();
		Concurrency::RunInterpreterTasks(std::move(tasks),
			[this]() {
				if(this == Interpreter::reusableReadLockConcurrencyManager)
				{
					if(Interpreter::reusableReadLock.owns_lock())
						Interpreter::reusableReadLock.unlock();
					Interpreter::reusableReadLockConcurrencyManager = nullptr;
				}
			});

		parentInterpreter->memoryModificationLock.lock();

		//release scope stack mutex
		parentInterpreter->scopeStackMutex = previousScopeStackMutex;

		//propagate side effects back up
		if(resultsSideEffect)
			parentInterpreter->SetSideEffectsFlags();
	}

	//updates the aggregated result reference's properties based on all of the child nodes
	inline void UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(EvaluableNodeReference &new_result)
	{
		if(!resultsUnique)
			new_result.unique = false;

		if(!resultsUniqueUnreferencedTopNode)
			new_result.uniqueUnreferencedTopNode = false;

		new_result.SetNeedCycleCheck(resultsNeedCycleCheck);

		if(!resultsIdempotent)
			new_result.SetIsIdempotent(false);
	}

	//returns true if any writes occurred
	inline bool HadSideEffects()
	{
		return resultsSideEffect;
	}

protected:
	//The parent and children borrow this mutex until the synchronous join.
	Concurrency::SingleMutex scopeStackMutex;
	Concurrency::SingleMutex *previousScopeStackMutex;

	//concurrent tasks to be completed
	std::vector<Concurrency::FixedSizeTask<>> tasks;

	EvaluableNode *targetOriginNode;

	EvaluableNodeReference *targetNode;

	//structure to keep track of the stack to prevent results from being garbage collected
	EvaluableNodeStackStateSaver resultsSaver;

	//interpreter that is running all the concurrent interpreters
	Interpreter *parentInterpreter;

	//if true, indicates all results are unique
	std::atomic_bool resultsUnique;

	//if true, indicates the result top node is unique
	std::atomic_bool resultsUniqueUnreferencedTopNode;

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

	//number of tasks added so far
	size_t curNumTasksAdded;
};
#endif
