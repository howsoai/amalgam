#pragma once

//if MULTITHREAD_SUPPORT is defined, compiles code with multithreaded support and reentrancy
//otherwise everything is considered not reentrant

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef MULTITHREAD_SUPPORT
	#include <taskflow/taskflow.hpp>
#endif

//system headers:
#include <cstddef>
#include <cstdint>

#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)

//system headers:
#include <atomic>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace Concurrency
{
	//standard mutex for singular access
	typedef std::mutex SingleMutex;

	//standard lock for singular access and performance
	typedef std::lock_guard<SingleMutex> Lock;

	//standard lock for singular access
	typedef std::unique_lock<SingleMutex> SingleLock;

	//standard read-write mutex
	typedef std::shared_mutex ReadWriteMutex;

	//standard read lock on a read-write shared mutex
	typedef std::shared_lock<ReadWriteMutex> ReadLock;

	//standard write lock on a read-write shared mutex
	typedef std::unique_lock<ReadWriteMutex> WriteLock;

	//Object to perform scope-based unlocking of a vector of locks of LockType for an existing buffer
	template<typename LockBufferType>
	class MultipleLockBufferObject
	{
	public:
		inline MultipleLockBufferObject(LockBufferType &_buffer)
		{
			buffer = &_buffer;
		}

		inline ~MultipleLockBufferObject()
		{
			buffer->clear();
		}

		LockBufferType *buffer;
	};

	size_t GetMaxNumThreads();

	//sets the maximum number of threads to use
	// if zero is specified, then it uses a heuristic default based on the system
	void SetMaxNumThreads(size_t max_num_threads);

#ifdef MULTITHREAD_SUPPORT
	//a set of potentially concurrent tasks at a given level of the execution graph
	using TaskSet = tf::Taskflow;

	struct ExecutionGeneration;
	extern thread_local ExecutionGeneration *worker_generation;
	extern thread_local bool is_system_worker;
	extern thread_local tf::Runtime *interpreter_runtime;

	//internal activity trackers for thread counting
	extern std::atomic<size_t> active_interpreter_tasks;
	extern std::atomic<size_t> active_system_tasks;
	extern thread_local size_t active_task_depth;

	//internal state management
	class TaskActivity : public tf::ObserverInterface
	{
	public:
		explicit TaskActivity(bool is_system)
			: count(is_system ? active_system_tasks : active_interpreter_tasks)
		{}

		inline void set_up(size_t) override
		{}

		inline void on_entry(tf::WorkerView, tf::TaskView) override
		{
			if(active_task_depth++ == 0)
				count++;
		}

		inline void on_exit(tf::WorkerView, tf::TaskView) override
		{
			if(--active_task_depth == 0)
				count--;
		}

	private:
		std::atomic<size_t> &count;
	};

	//a worker which borrows its generation
	class WorkerContext : public tf::WorkerInterface
	{
	public:
		inline WorkerContext(ExecutionGeneration *generation, bool is_system)
			: generation(generation), isSystem(is_system)
		{}

		inline void scheduler_prologue(tf::Worker &) override
		{
			worker_generation = generation;
			is_system_worker = isSystem;
		}

		inline void scheduler_epilogue(tf::Worker &, std::exception_ptr) override
		{
			worker_generation = nullptr;
		}

	private:

		ExecutionGeneration *generation;
		bool isSystem;
	};

	struct ExecutionGeneration
	{
		explicit ExecutionGeneration(size_t count)
			: numThreads(count),
			interpreter(count, std::make_shared<WorkerContext>(this, false)),
			system(count, std::make_shared<WorkerContext>(this, true))
		{
			interpreter.make_observer<TaskActivity>(false);
			system.make_observer<TaskActivity>(true);
		}

		size_t numThreads;
		tf::Executor interpreter;
		tf::Executor system;
	};

	std::shared_ptr<ExecutionGeneration> AcquireGeneration();

	//internal mechanism to pause activity
	class PauseActivity
	{
	public:
		inline PauseActivity()
			: count(is_system_worker ? active_system_tasks : active_interpreter_tasks),
			depth(active_task_depth)
		{
			if(depth)
				count--;
			active_task_depth = 0;
		}

		inline ~PauseActivity()
		{
			active_task_depth = depth;
			if(depth)
				count++;
		}

	private:
		std::atomic<size_t> &count;
		size_t depth;
	};

	//group of tasks for a given interpreter
	template<typename FuncType>
	class InterpreterTaskGroup
	{
	public:
		explicit InterpreterTaskGroup(std::vector<FuncType> &&tasks)
			: tasks(std::move(tasks)), remaining(this->tasks.size())
		{}

		inline void Drain()
		{
			for(size_t i = next.fetch_add(1); i < tasks.size(); i = next.fetch_add(1))
				Execute(i);
		}

		inline void Join()
		{
			//this thread will take the first task
			if(!tasks.empty())
				Execute(0);

			Drain();

			PauseActivity pause;

			for(size_t count = remaining.load(std::memory_order_acquire);
					count != 0; count = remaining.load(std::memory_order_acquire))
				remaining.wait(count, std::memory_order_acquire);
		}

	private:

		inline void Execute(size_t i)
		{
			tasks[i]();

			//mark task as done and if done, notify all that it's all done
			tasks[i] = nullptr;
			if(remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
				remaining.notify_all();
		}

		std::vector<FuncType> tasks;
		std::atomic<size_t> next{ 1 };
		std::atomic<size_t> remaining;
	};

	template<typename FuncType>
	inline void RunInterpreterRuntime(tf::Runtime &runtime, const FuncType &entry)
	{
		auto previous = std::exchange(interpreter_runtime, &runtime);
		auto cleanup = [&] { interpreter_runtime = previous; };

		entry();
		cleanup();
	}

	//runs the tasks for the current interpreter
	template<typename FuncType>
	inline void RunInterpreterTasks(std::vector<FuncType> tasks)
	{
		if(tasks.empty())
			return;

		if(!interpreter_runtime)
		{
			TaskSet task_set;
			task_set.emplace([&](tf::Runtime &runtime)
			{
				RunInterpreterRuntime(runtime, [&] { RunInterpreterTasks(std::move(tasks)); });
			});

			auto generation = AcquireGeneration();
			generation->interpreter.run(task_set).get();
			return;
		}

		size_t total_tasks = tasks.size();
		size_t available_worker_count = (worker_generation ? worker_generation->numThreads : 1);
		const size_t num_workers = std::min(total_tasks, available_worker_count) - 1;

		auto group = std::make_shared<InterpreterTaskGroup<std::function<void()>>>(std::move(tasks));
		for(size_t i = 0; i < num_workers; i++)
		{
			interpreter_runtime->silent_async([group](tf::Runtime &runtime)
			{
				RunInterpreterRuntime(runtime, [&] { group->Drain(); });
			});
		}

		group->Join();
	}

	//for garbage collection, cache, query tasks whose caller retains locks and may not execute interpreter code
	inline void RunSystemTasks(TaskSet &task_set)
	{
		if(worker_generation)
		{
			//a waiting task is not active
			//nested system tasks count their own work
			PauseActivity pause;

			if(is_system_worker)
				worker_generation->system.corun(task_set);
			else
				//one-way dependency; system tasks don't wait on Interpreter tasks
				worker_generation->system.run(task_set).get();
		}
		else
		{
			auto generation = AcquireGeneration();
			generation->system.run(task_set).get();
		}
	}

	inline size_t GetActiveInterpreterThreadCount()
	{
		return std::max<size_t>(1, active_interpreter_tasks.load() + (worker_generation ? 0 : 1));
	}

	inline size_t GetActiveThreadCount()
	{
		return std::max<size_t>(1, active_interpreter_tasks.load() + active_system_tasks.load()
			+ (worker_generation ? 0 : 1));
	}

#endif
};
#endif

//iterates over every element in container, passing the element along with the index into func, as long as
//the container's size is bigger than 1 and run_concurrently is true
template<typename ContainerType, typename FunctionType>
inline void IterateOverConcurrentlyIfPossible(ContainerType &container, FunctionType func,
	bool run_concurrently = false, bool is_system = true)
{
	size_t index = 0;
#ifdef MULTITHREAD_SUPPORT
	if(run_concurrently && container.size() > 1 && Concurrency::GetMaxNumThreads() > 1)
	{
		if(is_system)
		{
			Concurrency::TaskSet task_set;
			for(auto value : container)
			{
				task_set.emplace([index, value, &func] { func(index, value); });
				index++;
			}
			Concurrency::RunSystemTasks(task_set);
		}
		else
		{
			std::vector<std::function<void()>> tasks;
			tasks.reserve(container.size());
			for(auto value : container)
			{
				tasks.emplace_back([index, value, &func] { func(index, value); });
				index++;
			}
			Concurrency::RunInterpreterTasks(std::move(tasks));
		}
		return;
	}
	//not running concurrently
#endif

	for(auto value : container)
	{
		func(index, value);
		index++;
	}
}
