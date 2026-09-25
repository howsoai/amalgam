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
	//Zero selects a system default (at least one). Values beyond INT_MAX throw.
	//MT changes apply at the next external graph submission; descendants keep their generation.
	void SetMaxNumThreads(size_t max_num_threads);

#ifdef MULTITHREAD_SUPPORT
	//Maintenance workers cannot enter the Interpreter domain. Interpreter workers
	//may fork recursively, including with one configured worker.
	bool CanRunInterpreterConcurrently();

	//Bind an Interpreter entry to a Taskflow runtime. Graph tasks that invoke the
	//Interpreter use this entry; descendants inherit it through RunInterpreterTasks.
	void RunInterpreterRuntime(tf::Runtime &runtime, const std::function<void()> &entry);

	//Fork/join independent children in submission order. A waiting caller helps
	//only this group, never an unrelated task from an executor queue.
	void RunInterpreterTasks(std::vector<std::function<void()>> tasks);

	//Run a complete root graph and publish results. Worker submissions throw.
	void RunTaskflow(tf::Taskflow &graph);

	//For GC/cache/query graphs whose caller retains locks. Tasks in this domain
	//must not execute Interpreter code or depend on the calling thread's locks.
	void RunMaintenanceTaskflow(tf::Taskflow &graph);

	//Worker count of the current execution generation (including during resize).
	size_t GetExecutionThreadCount();

	//Sample active tasks (excluding synchronous joins), with a serial caller counted.
	size_t GetActiveInterpreterThreadCount();
	size_t GetActiveThreadCount();
#endif
};
#endif

//iterates over every element in container, passing the element along with the index into func, as long as
//the container's size is bigger than 1 and run_concurrently is true
template<typename ContainerType, typename FunctionType>
inline void IterateOverConcurrentlyIfPossible(ContainerType &container, FunctionType func,
	bool run_concurrently = false)
{
	size_t index = 0;
#ifdef MULTITHREAD_SUPPORT
	if(run_concurrently && container.size() > 1 && Concurrency::GetMaxNumThreads() > 1)
	{
		tf::Taskflow graph;
		for(auto value : container)
		{
			graph.emplace([index, value, &func] { func(index, value); });
			index++;
		}
		Concurrency::RunMaintenanceTaskflow(graph);
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
