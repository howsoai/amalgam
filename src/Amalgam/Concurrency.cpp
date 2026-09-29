//project headers:
#include "Concurrency.h"

#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace
{
size_t DefaultThreadCount()
{
	size_t count = std::thread::hardware_concurrency();
#if !defined(MULTITHREAD_SUPPORT) && defined(_OPENMP)
	count = (count + 1) / 2;
#endif
	return std::max<size_t>(1, count);
}

std::atomic<size_t> max_thread_count{DefaultThreadCount()};

#ifdef MULTITHREAD_SUPPORT
std::atomic<size_t> active_interpreter_tasks{ 0 };
std::atomic<size_t> active_system_tasks{ 0 };
thread_local size_t active_task_depth = 0;

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

struct ExecutionGeneration;
thread_local ExecutionGeneration *worker_generation = nullptr;
thread_local bool is_system_worker = false;
thread_local tf::Runtime *interpreter_runtime = nullptr;

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

//TODO: replace std::function<void()> with task from main branch

//A worker borrows its generation; only external synchronous callers own it.
//Consequently the last owner can never destroy an executor on its own worker.
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

ExecutionState &GetExecutionState()
{
	static ExecutionState state;
	return state;
}

std::shared_ptr<ExecutionGeneration> AcquireGeneration()
{
	auto &state = GetExecutionState();
	std::shared_ptr<ExecutionGeneration> retired;
	std::shared_ptr<ExecutionGeneration> result;
	{
		std::lock_guard lock(state.mutex);
		size_t count = max_thread_count.load();
		if(!state.current || state.current->numThreads != count)
		{
			//construction failure leaves the previous generation usable
			auto replacement = std::make_shared<ExecutionGeneration>(count);
			retired = std::exchange(state.current, std::move(replacement));
		}
		result = state.current;
	}

	//join retired workers outside the state mutex
	return result;
}
#endif
}

size_t Concurrency::GetMaxNumThreads()
{
	return max_thread_count.load();
}

void Concurrency::SetMaxNumThreads(size_t max_num_threads)
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

template<typename FuncType>
static void RunInterpreterRuntime(tf::Runtime &runtime, const FuncType &entry)
{
	struct RuntimeScope
	{
		~RuntimeScope()
		{
			interpreter_runtime = previous;
		}

		tf::Runtime *previous;
	} scope{std::exchange(interpreter_runtime, &runtime)};

	entry();
}

void Concurrency::RunInterpreterTasks(std::vector<std::function<void()>> tasks)
{
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

	if(tasks.empty())
		return;

	size_t total_tasks = tasks.size();
	size_t available_worker_count = (worker_generation ? worker_generation->numThreads : 1);

	//initialize next to 1 and don't count self in peer workers, since this thread will start executing the first task
	std::atomic<size_t> next{ 1 };
	std::atomic<size_t> remaining{ total_tasks };
	const size_t num_peer_workers = std::min(total_tasks, available_worker_count) - 1;

	auto execute_task = [&](size_t i) {
		tasks[i]();
		tasks[i] = nullptr;
		if(remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
			remaining.notify_all();
	};

	auto drain = [&]() {
		for(size_t i = next.fetch_add(1); i < total_tasks; i = next.fetch_add(1))
			execute_task(i);
	};

	for(size_t i = 0; i < num_peer_workers; i++)
		interpreter_runtime->silent_async([&]() { RunInterpreterRuntime(*interpreter_runtime, [&] { drain(); }); });

	//execute the first task here
	execute_task(0);
	drain();

	//wait for the remaining tasks
	{
		PauseActivity pause;
		size_t count = remaining.load(std::memory_order_acquire);
		while(count != 0)
		{
			remaining.wait(count, std::memory_order_acquire);
			count = remaining.load(std::memory_order_acquire);
		}
	}
}

size_t Concurrency::GetActiveInterpreterThreadCount()
{
	return std::max<size_t>(1, active_interpreter_tasks.load() + (worker_generation ? 0 : 1));
}

size_t Concurrency::GetActiveThreadCount()
{
	return std::max<size_t>(1, active_interpreter_tasks.load() + active_system_tasks.load()
		+ (worker_generation ? 0 : 1));
}

void Concurrency::RunSystemTasks(TaskSet &task_set)
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
#endif
#endif
