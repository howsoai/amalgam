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
	struct FixedSizeTask
	{
		using ExecuteFunc = void (*)(void *p);
		using MoveFunc = void (*)(void *dst, void *src);
		using DestroyFunc = void (*)(void *p);

		inline FixedSizeTask() : execute(nullptr), destroy(nullptr), move(nullptr)
		{}

		//prevent accidental copying to avoid double-destruction
		FixedSizeTask(const FixedSizeTask &) = delete;

		FixedSizeTask &operator=(const FixedSizeTask &) = delete;

		inline FixedSizeTask(FixedSizeTask &&other) noexcept
		{
			if(other.move != nullptr)
				(*other.move)(buffer, other.buffer);
			else
				std::memcpy(buffer, other.buffer, INLINE_SIZE);

			execute = other.execute;
			destroy = other.destroy;
			move = other.move;

			other.destroy = nullptr;
			other.move = nullptr;
		}

		inline FixedSizeTask &operator=(FixedSizeTask &&other) noexcept
		{
			if(this != &other)
			{
				if(destroy != nullptr)
					(*destroy)(buffer);

				if(other.move != nullptr)
					(*other.move)(buffer, other.buffer);
				else
					std::memcpy(buffer, other.buffer, INLINE_SIZE);

				execute = other.execute;
				destroy = other.destroy;
				move = other.move;

				other.destroy = nullptr;
				other.move = nullptr;
			}
			return *this;
		}

		inline ~FixedSizeTask()
		{
			if(destroy != nullptr)
				(*destroy)(buffer);
		}

		//assumes execute is not nullptr; otherwise it wouldn't be a task
		inline void operator()()
		{
			(*execute)(buffer);
		}

		//creates the task from a lambda function
		template<typename F>
		inline static FixedSizeTask Create(F &&f)
		{
			FixedSizeTask t;
			using FuncType = std::decay_t<F>;

			if constexpr(sizeof(FuncType) <= INLINE_SIZE
				&& alignof(FuncType) <= alignof(std::max_align_t)
				&& std::is_nothrow_move_constructible_v<FuncType>)
			{
				new (t.buffer) FuncType(std::forward<F>(f));

				t.execute = [](void *p) {
					auto *func = std::launder(reinterpret_cast<FuncType *>(p));
					(*func)();
					};

				if constexpr(!std::is_trivially_destructible_v<FuncType>)
				{
					t.destroy = [](void *p) {
						auto *func = std::launder(reinterpret_cast<FuncType *>(p));
						func->~FuncType();
						};
				}
				else
				{
					t.destroy = nullptr;
				}

				if constexpr(!std::is_trivially_copyable_v<FuncType>)
				{
					t.move = [](void *dst, void *src) {
						auto *source_obj = std::launder(reinterpret_cast<FuncType *>(src));
						new (dst) FuncType(std::move(*source_obj));
						source_obj->~FuncType();
						};
				}
				else
				{
					t.move = nullptr;
				}
			}
			else
			{
				FuncType *heapFunc = new FuncType(std::forward<F>(f));
				std::memcpy(t.buffer, &heapFunc, sizeof(FuncType *));

				t.execute = [](void *p) {
					auto **ptr_to_func = std::launder(reinterpret_cast<FuncType **>(p));
					(**ptr_to_func)();
					};

				//heap storage always requires a destroy call
				t.destroy = [](void *p) {
					auto **ptr_to_func = std::launder(reinterpret_cast<FuncType **>(p));
					delete *ptr_to_func;
					};

				t.move = nullptr;
			}
			return t;
		}

		static constexpr size_t INLINE_SIZE = 128 - 3 * sizeof(void *);

		ExecuteFunc execute;
		DestroyFunc destroy;
		MoveFunc move;

		alignas(std::max_align_t) uint8_t buffer[INLINE_SIZE];
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
		std::lock_guard lock(state.mutex);
		size_t count = max_thread_count.load();

		if(!state.current || state.current->numThreads != count)
			state.current = std::make_shared<ExecutionGeneration>(count);

		return state.current;
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
