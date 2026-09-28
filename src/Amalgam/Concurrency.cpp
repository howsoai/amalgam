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

	void set_up(size_t) override
	{}

	void on_entry(tf::WorkerView, tf::TaskView) override
	{
		if(active_task_depth++ == 0)
			count++;
	}

	void on_exit(tf::WorkerView, tf::TaskView) override
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
	PauseActivity()
		: count(is_system_worker ? active_system_tasks : active_interpreter_tasks),
		depth(active_task_depth)
	{
		if(depth)
			count--;
		active_task_depth = 0;
	}

	~PauseActivity()
	{
		active_task_depth = depth;
		if(depth)
			count++;
	}

private:
	std::atomic<size_t> &count;
	size_t depth;
};

//The synchronous opcode stack cannot be preempted. Give it access only to its
//own children, while Taskflow runtime runners can claim those same children.
//A claimed child is never queued waiting for capacity: its claimant executes it.
class InterpreterTaskGroup
{
public:
	explicit InterpreterTaskGroup(std::vector<std::function<void()>> tasks)
		: tasks(std::move(tasks)), failures(this->tasks.size()), remaining(this->tasks.size()) {}

	void Cancel()
	{
		cancelled.store(true, std::memory_order_relaxed);
	}

	void Drain()
	{
		for(size_t i = next.fetch_add(1); i < tasks.size(); i = next.fetch_add(1))
			Execute(i);
	}

	void Join()
	{
		//Keep the first child on the submitting stack. A worker that takes a
		//short sibling can return to Taskflow and pick up inner runtime work.
		if(!tasks.empty())
			Execute(0);

		Drain();

		{
			PauseActivity pause;

			for(size_t count = remaining.load(std::memory_order_acquire);
					count != 0; count = remaining.load(std::memory_order_acquire))
				remaining.wait(count, std::memory_order_acquire);
		}

		for(auto &failure : failures)
		{
			if(failure)
				std::rethrow_exception(failure);
		}
	}

private:
	void Execute(size_t i)
	{
		try
		{
			if(!cancelled.load(std::memory_order_relaxed)) tasks[i]();
		}
		catch(...)
		{
			failures[i] = std::current_exception();
			Cancel();
		}

		//No queued runner may retain references to an Interpreter stack after
		//Join returns, including the captures in completed callbacks.
		tasks[i] = nullptr;
		if(remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
			remaining.notify_all();
	}

	std::vector<std::function<void()>> tasks;
	std::vector<std::exception_ptr> failures;
	std::atomic<size_t> next{1};
	std::atomic<size_t> remaining;
	std::atomic<bool> cancelled{false};
};

//A worker borrows its generation; only external synchronous callers own it.
//Consequently the last owner can never destroy an executor on its own worker.
class WorkerContext : public tf::WorkerInterface
{
public:
	WorkerContext(ExecutionGeneration *generation, bool is_system)
		: generation(generation), isSystem(is_system)
	{}

	void scheduler_prologue(tf::Worker &) override
	{
		worker_generation = generation;
		is_system_worker = isSystem;
	}

	void scheduler_epilogue(tf::Worker &, std::exception_ptr) override
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
		throw std::invalid_argument("Thread count exceeds supported range");

	max_thread_count.store(max_num_threads);
#ifdef _OPENMP
	omp_set_num_threads(static_cast<int>(max_num_threads));
#endif
	//A new generation is created at the next external graph submission. In-flight
	//graphs and all their descendants keep the old generation until they finish.
}

#ifdef MULTITHREAD_SUPPORT
size_t Concurrency::GetExecutionThreadCount()
{
	//A serial caller is not executing in a graph, even if workers are configured.
	return worker_generation ? worker_generation->numThreads : 1;
}

bool Concurrency::CanRunInterpreterConcurrently()
{
	return !worker_generation || interpreter_runtime != nullptr;
}

void RunInterpreterRuntime(tf::Runtime &runtime, const std::function<void()> &entry)
{
	if(!worker_generation || is_system_worker
			|| &runtime.executor() != &worker_generation->interpreter
			|| runtime.executor().this_worker() != &runtime.worker())
		throw std::logic_error("Interpreter entry requires its owning Interpreter runtime worker");

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
	if(worker_generation && !interpreter_runtime)
		throw std::logic_error("Interpreter children require an Interpreter runtime");

	if(!interpreter_runtime)
	{
		TaskSet task_set;
		task_set.emplace([&](tf::Runtime &runtime)
		{
			RunInterpreterRuntime(runtime, [&] { RunInterpreterTasks(std::move(tasks)); });
		});

		RunTaskSet(task_set);
		return;
	}
	const size_t runners = tasks.empty() ? 0 : std::min(tasks.size() - 1, GetExecutionThreadCount() - 1);
	auto group = std::make_shared<InterpreterTaskGroup>(std::move(tasks));
	//Runtime's implicit anchor keeps every runner (including late empty runners)
	//in the root DAG. Successors and external shutdown wait for their retirement.
	//Only the owning worker accesses this runtime, even when draining inline.
	try
	{
		for(size_t i = 0; i < runners; ++i)
			interpreter_runtime->silent_async([group](tf::Runtime &runtime)
			{
				RunInterpreterRuntime(runtime, [&] { group->Drain(); });
			});
	}
	catch(...)
	{
		//already published children must finish before their captures unwind
		auto failure = std::current_exception();
		group->Cancel();
		try {
			group->Join();
		} catch(...) {}

		std::rethrow_exception(failure);
	}
	group->Join();
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

void Concurrency::RunTaskSet(TaskSet &task_set)
{
	//nested Interpreter work belongs to the current runtime
	if(worker_generation)
		throw std::logic_error("Workers cannot submit root graphs; use Interpreter runtime children");

	auto generation = AcquireGeneration();
	generation->interpreter.run(task_set).get();
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
