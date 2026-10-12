#pragma once
//project headers:
#include "Interpreter.h"

#ifdef MULTITHREAD_SUPPORT

namespace InterpreterConcurrencyManagerTraits
{
	//base traits - can be left empty or used for defaults
	struct Default
	{
		static constexpr size_t BufferSize = 96;
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
		static constexpr size_t BufferSize = 9 * sizeof(size_t);
		struct StackWithResultMode
		{};
		using ModeTag = StackWithResultMode;
	};
}

//class to manage the data for concurrent execution by an interpreter
template<typename Traits = InterpreterConcurrencyManagerTraits::Default>
class InterpreterConcurrencyManager
{
public:

	//constructs the concurrency manager.  Assumes parent_interpreter is NOT null
	InterpreterConcurrencyManager(Interpreter *parent_interpreter, size_t num_tasks)
	{
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
		resultsSaverCurrentTaskOffset = resultsSaverFirstTaskOffset;
		resultsSaver.ReserveNodes(num_tasks);

		randomSeeds.reserve(numTasks);
		for(size_t element_index = 0; element_index < numTasks; element_index++)
			randomSeeds.emplace_back(parentInterpreter->randomStream.CreateOtherStreamViaRand());

		//since each thread has a copy of the constructionStack, it's possible that more than one of the threads
		//obtains previous_results, so they must all be marked as not unique
		parentInterpreter->RemoveUniquenessFromPreviousResultsInConstructionStack();

		//need to create a mutex for all interpreters that will be called
		previousScopeStackMutex = std::exchange(parentInterpreter->scopeStackMutex, &scopeStackMutex);
	}

	//Adds a child task to the runtime group that needs a construction stack, using the relative interpreter
	// executes node_to_execute with the following parameters matching those of pushing on the construction stack
	// will allocate an appropriate node matching the type of current_index
	//result is set to the result of the task
	template<typename EvaluableNodeRefType>
		requires std::is_same_v<typename Traits::ModeTag,
			typename InterpreterConcurrencyManagerTraits::StackWithResults::ModeTag>
	void AddTaskWithConstructionStackWithResult(EvaluableNode *node_to_execute,
		EvaluableNode *target_origin, EvaluableNodeReference *target,
		EvaluableNodeImmediateValueWithType current_index,
		EvaluableNode *current_value,
		EvaluableNodeRefType &result)
	{
		size_t results_saver_location = resultsSaverCurrentTaskOffset++;
		size_t task_index = curNumTasksAdded++;

		//must keep InterpreterConcurrencyManagerTraits::StackWithResults up to date with the
		// number of params of the following lambda
		tasks.emplace_back(
			[this, task_index, node_to_execute, target_origin, target, current_index,
			current_value, &result, results_saver_location]
		{
			EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;

			Interpreter interpreter(parentInterpreter->evaluableNodeManager, randomSeeds[task_index],
				parentInterpreter->writeListeners, parentInterpreter->printListener,
				parentInterpreter->interpreterConstraints, parentInterpreter->curEntity, parentInterpreter);

			interpreter.memoryModificationLock = Concurrency::ReadLock(enm->GetMemoryModificationMutex());

			//build new construction stack
			std::vector<ConstructionStackEntry> construction_stack(parentInterpreter->constructionStack);
			construction_stack.emplace_back(target_origin, target, current_index, current_value, EvaluableNodeReference::Null());

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
			resultsSaver.SetStackElement(results_saver_location, result);

			interpreter.memoryModificationLock.unlock();
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
		size_t task_index = curNumTasksAdded++;

		//must keep InterpreterConcurrencyManagerTraits::StackWithoutResults up to date with the
		// number of params of the following lambda
		tasks.emplace_back(
			[this, task_index, node_to_execute, current_index, current_value]
		{
			EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;

			Interpreter interpreter(parentInterpreter->evaluableNodeManager, randomSeeds[task_index],
				parentInterpreter->writeListeners, parentInterpreter->printListener,
				parentInterpreter->interpreterConstraints, parentInterpreter->curEntity, parentInterpreter);

			interpreter.memoryModificationLock = Concurrency::ReadLock(enm->GetMemoryModificationMutex());

			//build new construction stack
			std::vector<ConstructionStackEntry> construction_stack(parentInterpreter->constructionStack);
			construction_stack.emplace_back(nullptr, &Interpreter::_null_reference, current_index, current_value, EvaluableNodeReference::Null());

			std::vector<EvaluableNode *> opcode_stack(begin(parentInterpreter->opcodeStackNodes),
				begin(parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset);

			auto result = interpreter.ExecuteNode(node_to_execute, nullptr, &opcode_stack, &construction_stack,
				EvaluableNodeRequestedValueTypes::Type::NULL_VALUE, false);

			interpreter.PopConstructionContextAndGetExecutionSideEffectFlag();
			enm->FreeNodeTreeIfPossible(result);

			interpreter.memoryModificationLock.unlock();
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
		//save the node to execute, but also save the location
		//so the location can be used later to save the result
		size_t results_saver_location = resultsSaverCurrentTaskOffset++;

		size_t task_index = curNumTasksAdded++;

		//must keep InterpreterConcurrencyManagerTraits::NoStack up to date with the
		// number of params of the following lambda
		tasks.emplace_back(
			[this, task_index, node_to_execute, result, immediate_results, results_saver_location]
		{
			EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;

			Interpreter interpreter(parentInterpreter->evaluableNodeManager, randomSeeds[task_index],
				parentInterpreter->writeListeners, parentInterpreter->printListener,
				parentInterpreter->interpreterConstraints, parentInterpreter->curEntity, parentInterpreter);

			interpreter.memoryModificationLock = Concurrency::ReadLock(enm->GetMemoryModificationMutex());

			std::vector<EvaluableNode *> opcode_stack(begin(parentInterpreter->opcodeStackNodes),
				begin(parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset);
			std::vector<ConstructionStackEntry> construction_stack(parentInterpreter->constructionStack);

			auto result_ref = interpreter.ExecuteNode(node_to_execute, nullptr, &opcode_stack,
				&construction_stack, immediate_results, false);

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
					resultsSaver.SetStackElement(results_saver_location, *result);
			}

			interpreter.memoryModificationLock.unlock();
		}
		);
	}

	//ends concurrency from all interpreters and waits for them to finish
	inline void EndConcurrency()
	{
		//allow other threads to perform garbage collection
		parentInterpreter->memoryModificationLock.unlock();
		Concurrency::RunInterpreterTasks(std::move(tasks));
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

	//random seed for each task, the size of numTasks
	std::vector<RandomStream> randomSeeds;

	//concurrent tasks to be completed
	std::vector<Concurrency::FixedSizeTask<>> tasks;

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
