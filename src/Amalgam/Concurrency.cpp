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

std::atomic<size_t> configured_threads{DefaultThreadCount()};

#ifdef MULTITHREAD_SUPPORT
std::atomic<size_t> active_interpreter_tasks{0}, active_maintenance_tasks{0};
thread_local size_t active_task_depth = 0;

class TaskActivity : public tf::ObserverInterface
{
public:
	explicit TaskActivity(bool maintenance) : count(maintenance ? active_maintenance_tasks : active_interpreter_tasks) {}
	void set_up(size_t) override {}
	void on_entry(tf::WorkerView, tf::TaskView) override
	{
		if(active_task_depth++ == 0) ++count;
	}
	void on_exit(tf::WorkerView, tf::TaskView) override
	{
		if(--active_task_depth == 0) --count;
	}
private:
	std::atomic<size_t> &count;
};

struct ExecutionGeneration;
thread_local ExecutionGeneration *worker_generation = nullptr;
thread_local bool maintenance_worker = false;
thread_local tf::Runtime *interpreter_runtime = nullptr;

class PauseActivity
{
public:
	PauseActivity() : count(maintenance_worker ? active_maintenance_tasks : active_interpreter_tasks),
		depth(active_task_depth)
	{
		if(depth) --count;
		active_task_depth = 0;
	}
	~PauseActivity()
	{
		active_task_depth = depth;
		if(depth) ++count;
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

	void Cancel() { cancelled.store(true, std::memory_order_relaxed); }

	void Drain()
	{
		for(size_t i = next.fetch_add(1); i < tasks.size(); i = next.fetch_add(1))
			Execute(i);
	}

	void Join()
	{
		//Keep the first child on the submitting stack. A worker that takes a
		//short sibling can return to Taskflow and pick up inner runtime work.
		if(!tasks.empty()) Execute(0);
		Drain();
		{
			PauseActivity pause;
			for(size_t count = remaining.load(std::memory_order_acquire); count != 0;
				count = remaining.load(std::memory_order_acquire))
				remaining.wait(count, std::memory_order_acquire);
		}
		for(auto &failure : failures)
			if(failure) std::rethrow_exception(failure);
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
	WorkerContext(ExecutionGeneration *generation, bool maintenance)
		: generation(generation), maintenance(maintenance) {}

	void scheduler_prologue(tf::Worker &) override
	{
		worker_generation = generation;
		maintenance_worker = maintenance;
	}

	void scheduler_epilogue(tf::Worker &, std::exception_ptr) override
	{
		worker_generation = nullptr;
	}

private:
	ExecutionGeneration *generation;
	bool maintenance;
};

struct ExecutionGeneration
{
	explicit ExecutionGeneration(size_t count)
		: numThreads(count),
		  interpreter(count, std::make_shared<WorkerContext>(this, false)),
		  maintenance(count, std::make_shared<WorkerContext>(this, true))
	{
		interpreter.make_observer<TaskActivity>(false);
		maintenance.make_observer<TaskActivity>(true);
	}

	size_t numThreads;
	tf::Executor interpreter;
	tf::Executor maintenance;
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
		size_t count = configured_threads.load();
		if(!state.current || state.current->numThreads != count)
		{
			//Construction failure leaves the previous generation usable.
			auto replacement = std::make_shared<ExecutionGeneration>(count);
			retired = std::exchange(state.current, std::move(replacement));
		}
		result = state.current;
	}
	//Join retired workers outside the state mutex.
	return result;
}
#endif
}

size_t Concurrency::GetMaxNumThreads()
{
	return configured_threads.load();
}

void Concurrency::SetMaxNumThreads(size_t max_num_threads)
{
	if(max_num_threads == 0)
		max_num_threads = DefaultThreadCount();
	//Reject values that previously narrowed to negative thread counts.
	if(max_num_threads > static_cast<size_t>(std::numeric_limits<int>::max()))
		throw std::invalid_argument("Thread count exceeds supported range");

	configured_threads.store(max_num_threads);
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

void Concurrency::RunInterpreterRuntime(tf::Runtime &runtime, const std::function<void()> &entry)
{
	if(!worker_generation || maintenance_worker
		|| &runtime.executor() != &worker_generation->interpreter
		|| runtime.executor().this_worker() != &runtime.worker())
		throw std::logic_error("Interpreter entry requires its owning Interpreter runtime worker");
	struct RuntimeScope
	{
		tf::Runtime *previous;
		~RuntimeScope() { interpreter_runtime = previous; }
	} scope{std::exchange(interpreter_runtime, &runtime)};
	entry();
}

void Concurrency::RunInterpreterTasks(std::vector<std::function<void()>> tasks)
{
	if(worker_generation && !interpreter_runtime)
		throw std::logic_error("Interpreter children require an Interpreter runtime");
	if(!interpreter_runtime)
	{
		tf::Taskflow graph;
		graph.emplace([&](tf::Runtime &runtime)
		{
			RunInterpreterRuntime(runtime, [&] { RunInterpreterTasks(std::move(tasks)); });
		});
		RunTaskflow(graph);
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
		//Already published children must finish before their captures unwind.
		auto failure = std::current_exception();
		group->Cancel();
		try { group->Join(); } catch(...) {}
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
	return std::max<size_t>(1, active_interpreter_tasks.load() + active_maintenance_tasks.load()
		+ (worker_generation ? 0 : 1));
}

void Concurrency::RunTaskflow(tf::Taskflow &graph)
{
	//Nested Interpreter work belongs to the current runtime, not a new topology.
	if(worker_generation)
		throw std::logic_error("Workers cannot submit root graphs; use Interpreter runtime children");
	auto generation = AcquireGeneration();
	generation->interpreter.run(graph).get();
}

void Concurrency::RunMaintenanceTaskflow(tf::Taskflow &graph)
{
	if(worker_generation)
	{
		//A waiting task is not active. Nested maintenance tasks count their own work.
		PauseActivity pause;
		if(maintenance_worker)
			worker_generation->maintenance.corun(graph);
		else
			//One-way dependency: maintenance tasks never wait on Interpreter tasks.
			worker_generation->maintenance.run(graph).get();
	}
	else
	{
		auto generation = AcquireGeneration();
		generation->maintenance.run(graph).get();
	}
}
#endif
#endif
