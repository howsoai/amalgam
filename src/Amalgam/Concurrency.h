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

	//Move-only, inline-only task storage. Every production closure is checked at
	//its emplace_back site; larger or throwing-move captures must be redesigned.
	class FixedSizeTask
	{
	public:
		static constexpr size_t INLINE_SIZE = 96;

		FixedSizeTask() = default;
		FixedSizeTask(const FixedSizeTask &) = delete;
		FixedSizeTask &operator=(const FixedSizeTask &) = delete;

		template<typename F> requires (!std::is_same_v<std::decay_t<F>, FixedSizeTask>)
		explicit FixedSizeTask(F &&f)
		{
			using FuncType = std::decay_t<F>;
			static_assert(sizeof(FuncType) <= INLINE_SIZE, "Task capture exceeds inline storage");
			static_assert(alignof(FuncType) <= alignof(std::max_align_t), "Task capture is over-aligned");
			static_assert(std::is_nothrow_move_constructible_v<FuncType>, "Task capture must move without throwing");
			static_assert(std::is_nothrow_destructible_v<FuncType>);
			static_assert(std::is_invocable_r_v<void, FuncType &>);
			static constexpr Operations operations = {
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
			ops = &operations;
		}

		FixedSizeTask(FixedSizeTask &&other) noexcept
			: ops(std::exchange(other.ops, nullptr))
		{
			if(ops)
				ops->move(buffer, other.buffer);
		}

		FixedSizeTask &operator=(FixedSizeTask &&other) noexcept
		{
			if(this != &other)
			{
				Reset();
				ops = std::exchange(other.ops, nullptr);
				if(ops)
					ops->move(buffer, other.buffer);
			}
			return *this;
		}

		~FixedSizeTask() { Reset(); }

		void Reset() noexcept
		{
			if(ops)
				std::exchange(ops, nullptr)->destroy(buffer);
		}

		//Only populated tasks may be invoked.
		void operator()() { ops->execute(buffer); }

	private:
		struct Operations
		{
			void (*execute)(void *);
			void (*move)(void *, void *);
			void (*destroy)(void *);
		};
		alignas(std::max_align_t) std::byte buffer[INLINE_SIZE];
		const Operations *ops = nullptr;
	};

	struct ExecutionGeneration;
	inline thread_local ExecutionGeneration *worker_generation = nullptr;
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

	struct ExecutionState
	{
		std::mutex mutex;
		std::shared_ptr<ExecutionGeneration> current;
	};

	inline ExecutionState &GetExecutionState()
	{
		static ExecutionState state;
		return state;
	}

	inline std::shared_ptr<ExecutionGeneration> AcquireGeneration()
	{
		auto &state = GetExecutionState();
		std::shared_ptr<ExecutionGeneration> retired, result;
		{
			std::lock_guard lock(state.mutex);
			size_t count = max_thread_count.load();
			if(!state.current || state.current->numThreads != count)
				retired = std::exchange(state.current, std::make_shared<ExecutionGeneration>(count));
			result = state.current;
		}
		//Retired executors join outside the configuration mutex.
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

	//Owns the original vector storage until even late runtime runners retire.
	//A stack group is safe only when no runners are submitted: waiting for queued
	//runners at a nested join would deadlock saturated workers.
	class InterpreterTaskGroup
	{
	public:
		explicit InterpreterTaskGroup(std::vector<FixedSizeTask> &&tasks)
			: tasks(std::move(tasks)), remaining(this->tasks.size())
		{}

		void Cancel() { cancelled.store(true, std::memory_order_relaxed); }

		void Drain()
		{
			for(size_t i = next.fetch_add(1); i < tasks.size(); i = next.fetch_add(1))
				Execute(i);
		}

		void Join()
		{
			Execute(0);
			Drain();
			{
				PauseActivity pause;
				for(size_t count = remaining.load(std::memory_order_acquire);
					count != 0; count = remaining.load(std::memory_order_acquire))
					remaining.wait(count, std::memory_order_acquire);
			}
			if(failure)
				std::rethrow_exception(failure);
		}

	private:
		void Execute(size_t i)
		{
			try
			{
				if(!cancelled.load(std::memory_order_relaxed))
					tasks[i]();
			}
			catch(...)
			{
				//Only failing tasks contend here. Keep submission order without
				//a per-task exception vector or any allocation on the success path.
				std::lock_guard lock(failureMutex);
				if(i < failureIndex)
				{
					failureIndex = i;
					failure = std::current_exception();
				}
				Cancel();
			}
			//End every capture's lifetime before publishing child completion.
			//Late runners only inspect the unchanged vector size and claim index.
			tasks[i].Reset();
			if(remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
				remaining.notify_all();
		}

		std::vector<FixedSizeTask> tasks;
		std::atomic<size_t> next{ 1 };
		std::atomic<size_t> remaining;
		std::atomic<bool> cancelled{ false };
		std::mutex failureMutex;
		size_t failureIndex = std::numeric_limits<size_t>::max();
		std::exception_ptr failure;
	};

	//Bind nested forks to the currently executing runtime, including on unwind.
	template<typename FuncType>
	inline void RunInterpreterRuntime(tf::Runtime &runtime, const FuncType &entry)
	{
		if(!worker_generation || is_system_worker
			|| &runtime.executor() != &worker_generation->interpreter
			|| runtime.executor().this_worker() != &runtime.worker())
			throw std::logic_error("Interpreter entry requires its owning runtime worker");
		struct RuntimeScope
		{
			tf::Runtime *previous;
			~RuntimeScope() { interpreter_runtime = previous; }
		} scope{ std::exchange(interpreter_runtime, &runtime) };
		entry();
	}

	inline void RunInterpreterTasks(std::vector<FixedSizeTask> tasks)
	{
		if(tasks.empty())
			return;
		if(worker_generation && !interpreter_runtime)
			throw std::logic_error("Interpreter children require an Interpreter runtime");

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

		const size_t num_workers = std::min(tasks.size(), worker_generation->numThreads) - 1;
		if(num_workers == 0)
		{
			InterpreterTaskGroup group(std::move(tasks));
			group.Join();
			return;
		}

		//One shared allocation per parallel group; no allocation per user task.
		//Taskflow's implicit anchor keeps late runners and descendants in the DAG.
		auto group = std::make_shared<InterpreterTaskGroup>(std::move(tasks));
		try
		{
			for(size_t i = 0; i < num_workers; i++)
				interpreter_runtime->silent_async([group](tf::Runtime &runtime)
				{
					RunInterpreterRuntime(runtime, [&] { group->Drain(); });
				});
		}
		catch(...)
		{
			//Submission failure must not unwind captures still used by children.
			auto failure = std::current_exception();
			group->Cancel();
			try { group->Join(); } catch(...) {}
			std::rethrow_exception(failure);
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
			std::vector<Concurrency::FixedSizeTask> tasks;
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
