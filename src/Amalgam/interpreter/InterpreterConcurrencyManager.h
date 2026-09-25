#pragma once
//project headers:
#include "Interpreter.h"

#ifdef MULTITHREAD_SUPPORT
//class to manage the data for concurrent execution by an interpreter
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
		constructionEffects.resize(num_tasks);

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
		parentInterpreter->scopeStackMutex = std::make_unique<Concurrency::SingleMutex>();
	}

	~InterpreterConcurrencyManager()
	{
		//An abandoned group has never run; do not start side effects during unwinding.
		if(!completed)
			parentInterpreter->scopeStackMutex.reset();
	}

	//Adds a child task to the runtime group that needs a construction stack, using the relative interpreter
	// executes node_to_execute with the following parameters matching those of pushing on the construction stack
	// will allocate an appropriate node matching the type of current_index
	//result is set to the result of the task
	template<typename EvaluableNodeRefType>
	void AddTaskWithConstructionStack(EvaluableNode *node_to_execute,
		EvaluableNode *target_origin, EvaluableNodeReference *target,
		EvaluableNodeImmediateValueWithType current_index,
		EvaluableNode *current_value,
		EvaluableNodeRefType &result)
	{
		size_t results_saver_location = resultsSaverCurrentTaskOffset++;
		size_t task_index = curNumTasksAdded++;
		RandomStream rand_seed = randomSeeds[task_index];
		constructionEffects[task_index].target = target;

		tasks.emplace_back(
			[this, rand_seed, node_to_execute, target_origin, target, current_index,
			current_value, &result, results_saver_location, task_index]
		{
			EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;

			Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
				parentInterpreter->writeListeners, parentInterpreter->printListener,
				parentInterpreter->interpreterConstraints, parentInterpreter->curEntity, parentInterpreter);

			interpreter.memoryModificationLock = Concurrency::ReadLock(enm->GetMemoryModificationMutex());

			//build new construction stack
			std::vector<ConstructionStackEntry> construction_stack(parentInterpreter->constructionStack);
			construction_stack.emplace_back(target_origin, target, current_index, current_value, EvaluableNodeReference::Null());

			std::vector<EvaluableNode *> opcode_stack(begin(parentInterpreter->opcodeStackNodes),
				begin(parentInterpreter->opcodeStackNodes) + resultsSaverFirstTaskOffset);

			auto result_ref = interpreter.ExecuteNode(node_to_execute,
				nullptr, &opcode_stack, &construction_stack, EvaluableNodeRequestedValueTypes::Type::NONE, false);

			//This entry points at the parent's shared construction target. Only
			//collect local effects here; the parent finalizes target flags after join.
			AmlgAssert(!interpreter.constructionStack.empty());
			bool side_effects = interpreter.constructionStack.back().executionSideEffects;
			constructionEffects[task_index].sideEffects = side_effects;
			interpreter.constructionStack.pop_back();
			if(side_effects)
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

	//like the previous definition of AddTaskWithConstructionStack,
	//but without keeping results or building a target
	template<typename EvaluableNodeRefType>
	void AddTaskWithConstructionStack(EvaluableNode *node_to_execute,
		EvaluableNodeImmediateValueWithType current_index,
		EvaluableNode *current_value)
	{
		RandomStream rand_seed = randomSeeds[curNumTasksAdded++];

		tasks.emplace_back(
			[this, rand_seed, node_to_execute, current_index, current_value]
		{
			EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;

			Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
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
	void AddTask(EvaluableNode *node_to_execute,
		EvaluableNodeRefType *result = nullptr, EvaluableNodeRequestedValueTypes immediate_results = false)
	{
		//save the node to execute, but also save the location
		//so the location can be used later to save the result
		size_t results_saver_location = resultsSaverCurrentTaskOffset++;

		RandomStream rand_seed = randomSeeds[curNumTasksAdded++];

		tasks.emplace_back(
			[this, rand_seed, node_to_execute, result, immediate_results, results_saver_location]
		{
			EvaluableNodeManager *enm = parentInterpreter->evaluableNodeManager;

			Interpreter interpreter(parentInterpreter->evaluableNodeManager, rand_seed,
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
		if(completed)
			return;
		completed = true;

		//The group join is the child-before-parent dependency. Release the parent's
		//read lock before waiting for children, including children that need GC.
		parentInterpreter->memoryModificationLock.unlock();
		std::exception_ptr failure;
		try
		{
			Concurrency::RunInterpreterTasks(std::move(tasks));
		}
		catch(...)
		{
			failure = std::current_exception();
		}
		parentInterpreter->memoryModificationLock.lock();

		//Each child wrote only its own effect record. The join publishes those
		//records and ends every borrow of the parent's construction targets.
		for(auto &effect : constructionEffects)
			if(effect.target != nullptr)
				Interpreter::FinalizeConstructionTarget(*effect.target, effect.sideEffects);

		//release scope stack mutex
		parentInterpreter->scopeStackMutex.reset();

		//propagate side effects back up
		if(resultsSideEffect)
			parentInterpreter->SetSideEffectsFlags();
		if(failure)
			std::rethrow_exception(failure);
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
	struct ConstructionEffect
	{
		EvaluableNodeReference *target = nullptr;
		bool sideEffects = false;
	};
	//Stable slots: children never resize this vector or write a sibling's slot.
	std::vector<ConstructionEffect> constructionEffects;

	//random seed for each task, the size of numTasks
	std::vector<RandomStream> randomSeeds;

	//Children join the current runtime; execution starts only at EndConcurrency.
	std::vector<std::function<void()>> tasks;
	bool completed = false;

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
