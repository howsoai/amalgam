//project headers:
#include "Concurrency.h"

#if defined(MULTITHREAD_SUPPORT) || defined(_OPENMP)
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

size_t DefaultThreadCount()
{
	size_t count = std::thread::hardware_concurrency();
#if !defined(MULTITHREAD_SUPPORT) && defined(_OPENMP)
	count = (count + 1) / 2;
#endif
	return std::max<size_t>(1, count);
}

std::atomic<size_t> max_thread_count{ DefaultThreadCount() };

#ifdef MULTITHREAD_SUPPORT
std::atomic<size_t> Concurrency::active_interpreter_tasks{ 0 };
std::atomic<size_t> Concurrency::active_system_tasks{ 0 };
thread_local size_t Concurrency::active_task_depth = 0;

thread_local Concurrency::ExecutionGeneration *Concurrency::worker_generation = nullptr;
thread_local bool Concurrency::is_system_worker = false;
thread_local tf::Runtime *Concurrency::interpreter_runtime = nullptr;

struct ExecutionState
{
	std::mutex mutex;
	std::shared_ptr<Concurrency::ExecutionGeneration> current;
};

ExecutionState &GetExecutionState()
{
	static ExecutionState state;
	return state;
}

std::shared_ptr<Concurrency::ExecutionGeneration> Concurrency::AcquireGeneration()
{
	auto &state = GetExecutionState();
	std::lock_guard lock(state.mutex);
	size_t count = max_thread_count.load();

	if(!state.current || state.current->numThreads != count)
		state.current = std::make_shared<ExecutionGeneration>(count);

	return state.current;
}
#endif

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

#endif
