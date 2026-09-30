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
#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
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

	inline size_t DefaultThreadCount()
	{
		size_t count = std::thread::hardware_concurrency();
	#if !defined(MULTITHREAD_SUPPORT) && defined(_OPENMP)
		count = (count + 1) / 2;
	#endif
		return std::max<size_t>(1, count);
	}

	inline std::atomic<size_t> max_thread_count{ DefaultThreadCount() };

	inline size_t GetMaxNumThreads()
	{
		return max_thread_count.load();
	}

	//sets the maximum number of threads to use
	// if zero is specified, then it uses a heuristic default based on the system
	inline void SetMaxNumThreads(size_t max_num_threads)
	{
		if(max_num_threads == 0)
			max_num_threads = DefaultThreadCount();

		//reject values that previously narrowed to negative thread counts
		if(max_num_threads > static_cast<size_t>(std::numeric_limits<int>::max()))
			return;

		max_thread_count.store(max_num_threads);
	#ifdef _OPENMP
		omp_set_num_threads(static_cast<int>(max_num_threads));
	#endif
		//a new generation is created at the next external graph submission
		//currently executing graphs and all their descendants keep the old generation until they finish
	}

#ifdef MULTITHREAD_SUPPORT
	//a set of potentially concurrent tasks at a given level of the execution graph
	using TaskSet = tf::Taskflow;

	//constant memory sized holder for a task that is big enough to
	//cover most small tasks via small buffer optimization, but will allocate on the heap
	//if needed.  note that for performance, it assumes it will be given a valid task
	//before being executed
	template<size_t bufferSize = 96>
	struct FixedSizeTask
	{
	public:
		FixedSizeTask() = default;
		FixedSizeTask(const FixedSizeTask &) = delete;
		FixedSizeTask &operator=(const FixedSizeTask &) = delete;

		template<typename F> requires (!std::is_same_v<std::decay_t<F>, FixedSizeTask<bufferSize>>)
		explicit FixedSizeTask(F &&f)
		{
			using FuncType = std::decay_t<F>;
			static_assert(sizeof(FuncType) <= bufferSize, "Task capture exceeds inline storage");
			static constexpr Operations new_operations = {
				[](void *p) { (*std::launder(reinterpret_cast<FuncType *>(p)))(); },
				[](void *dst, void *src)
				{
					auto *source = std::launder(reinterpret_cast<FuncType *>(src));
					new (dst) FuncType(std::move(*source));
					source->~FuncType();
				},
				[](void *p) { std::launder(reinterpret_cast<FuncType *>(p))->~FuncType(); }
			};
			new (buffer) FuncType(std::forward<F>(f));
			operations = &new_operations;
		}

		FixedSizeTask(FixedSizeTask<bufferSize> &&other) noexcept
			: operations(std::exchange(other.operations, nullptr))
		{
			if(operations)
				operations->move(buffer, other.buffer);
		}

		FixedSizeTask &operator=(FixedSizeTask<bufferSize> &&other) noexcept
		{
			if(this != &other)
			{
				Reset();
				operations = std::exchange(other.operations, nullptr);
				if(operations)
					operations->move(buffer, other.buffer);
			}
			return *this;
		}

		~FixedSizeTask()
		{
			Reset();
		}

		void Reset() noexcept
		{
			if(operations)
				std::exchange(operations, nullptr)->destroy(buffer);
		}

		//only populated tasks may be invoked
		void operator()()
		{
			operations->execute(buffer);
		}

	private:
		alignas(std::max_align_t) std::byte buffer[bufferSize];

		struct Operations
		{
			void (*execute)(void *);
			void (*move)(void *, void *);
			void (*destroy)(void *);
		};
		const Operations *operations = nullptr;
	};

	struct ConcurrentExecutor;
	inline thread_local ConcurrentExecutor *concurrent_executor = nullptr;
	inline thread_local bool is_system_worker = false;
	inline thread_local tf::Runtime *interpreter_runtime = nullptr;

	//internal activity trackers for thread counting
	inline std::atomic<size_t> active_interpreter_tasks{ 0 };
	inline std::atomic<size_t> active_system_tasks{ 0 };
	inline thread_local size_t active_task_depth{ 0 };

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

	//a worker which borrows its executor
	class WorkerContext : public tf::WorkerInterface
	{
	public:
		inline WorkerContext(ConcurrentExecutor *exec, bool is_system)
			: executor(exec), isSystem(is_system)
		{}

		inline void scheduler_prologue(tf::Worker &) override
		{
			concurrent_executor = executor;
			is_system_worker = isSystem;
		}

		inline void scheduler_epilogue(tf::Worker &, std::exception_ptr) override
		{
			concurrent_executor = nullptr;
		}

	private:

		ConcurrentExecutor *executor;
		bool isSystem;
	};

	//manager of execution based on Taskflow
	struct ConcurrentExecutor
	{
		explicit ConcurrentExecutor(size_t count)
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

	struct ExecutionState
	{
		std::mutex mutex;
		std::shared_ptr<ConcurrentExecutor> current;
	};

	inline ExecutionState &GetExecutionState()
	{
		static ExecutionState state;
		return state;
	}

	//returns an executor
	inline std::shared_ptr<ConcurrentExecutor> AcquireConcurrentExecutor()
	{
		auto &state = GetExecutionState();
		std::shared_ptr<ConcurrentExecutor> retired;
		std::shared_ptr<ConcurrentExecutor> result;
		{
			std::lock_guard lock(state.mutex);
			size_t count = max_thread_count.load();
			if(!state.current || state.current->numThreads != count)
				retired = std::exchange(state.current, std::make_shared<ConcurrentExecutor>(count));
			result = state.current;
		}
		
		//let any destruction of retired occur outside the lock
		return result;
	}

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
			//first element is accounted for by default value of next
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
			tasks[i].Reset();
			if(remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
				remaining.notify_all();
		}

		std::vector<FuncType> tasks;
		//default to 1 since the dispatching thread will take the first element
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

			auto exec = AcquireConcurrentExecutor();
			exec->interpreter.run(task_set).get();
			return;
		}

		const size_t num_workers = std::min(tasks.size(), concurrent_executor->numThreads) - 1;
		if(num_workers == 0)
		{
			InterpreterTaskGroup group(std::move(tasks));
			group.Join();
			return;
		}

		auto group = std::make_shared<InterpreterTaskGroup<FuncType>>(std::move(tasks));
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
		if(concurrent_executor)
		{
			//a waiting task is not active
			//nested system tasks count their own work
			PauseActivity pause;

			if(is_system_worker)
				concurrent_executor->system.corun(task_set);
			else
				//one-way dependency; system tasks don't wait on Interpreter tasks
				concurrent_executor->system.run(task_set).get();
		}
		else
		{
			auto exec = AcquireConcurrentExecutor();
			exec->system.run(task_set).get();
		}
	}

	inline size_t GetActiveInterpreterThreadCount()
	{
		return std::max<size_t>(1, active_interpreter_tasks.load() + (concurrent_executor ? 0 : 1));
	}

	inline size_t GetActiveThreadCount()
	{
		return std::max<size_t>(1, active_interpreter_tasks.load() + active_system_tasks.load()
			+ (concurrent_executor ? 0 : 1));
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
			//determine how big FixedSizeTask needs to be
			using ValueType = typename ContainerType::value_type;
			using TaskLambda = decltype([
				index = std::declval<size_t>(),
				value = std::declval<ValueType>(),
				&func = std::declval<FunctionType &>()
			] {});

			std::vector<Concurrency::FixedSizeTask<sizeof(TaskLambda)>> tasks;
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
