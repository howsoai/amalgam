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
	return !worker_generation && GetMaxNumThreads() > 1;
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
	//No helping on a stack that can retain entity/query/scope locks. Interpreter
	//opcodes use their ordinary serial implementation within a worker instead.
	if(worker_generation)
		throw std::logic_error("Workers cannot submit Interpreter graphs; use the serial path");
	auto generation = AcquireGeneration();
	generation->interpreter.run(graph).get();
}

void Concurrency::RunMaintenanceTaskflow(tf::Taskflow &graph)
{
	if(worker_generation)
	{
		//A waiting task is not active. Nested maintenance tasks count their own work.
		struct PauseActivity
		{
			std::atomic<size_t> &count;
			size_t depth = active_task_depth;
			PauseActivity() : count(maintenance_worker ? active_maintenance_tasks : active_interpreter_tasks)
			{
				if(depth) --count;
				active_task_depth = 0;
			}
			~PauseActivity()
			{
				active_task_depth = depth;
				if(depth) ++count;
			}
		} pause;
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
