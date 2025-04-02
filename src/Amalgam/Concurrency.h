#pragma once

//if MULTITHREAD_SUPPORT is defined, compiles code with multithreaded support, requires a C++0x17 or newer compiler
//MULTITHREAD_SUPPORT means multithreading will be enabled everywhere, including the appropriate locks
//MULTITHREAD_INTERFACE means that multithreading will be enabled only for the interface,
// which means that multithreaded applications can call this library.  This is a subset of MULTITHREAD_SUPPORT
//MULTITHREAD_ENTITY_CALL_MUTEX will only allow one call per entity as an external library.

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef MULTITHREAD_SUPPORT
	#include "ThreadPool.h"
	#define MULTITHREAD_INTERFACE
	#define MULTITHREAD_ENTITY_CALL_MUTEX
#endif

#ifndef NO_REENTRANCY_LOCKS
	#define MULTITHREAD_INTERFACE

	#ifndef NO_ENTITY_CALL_MUTEX
		#define MULTITHREAD_ENTITY_CALL_MUTEX
	#endif
#endif

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE) || defined(_OPENMP)

//system headers:
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace Concurrency
{
	//standard mutex for singular access
	typedef std::mutex SingleMutex;

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
	//threadPool is the primary thread pool shared for common tasks
	//any tasks that have interdependencies should be enqueued as one batch
	//to make sure that interdependency deadlocks do not occur
	extern ThreadPool threadPool;

	//urgentThreadPool is intended for short urgent tasks, such as building
	//data structures or collecting garbage, where the tasks do not kick off other tasks,
	//and tasks can be comingled freely
	extern ThreadPool urgentThreadPool;
#endif
};
#endif

//iterates over every element in container, passing the element along with the index into func, as long as
//the container's size is bigger than 1 and run_concurrently is true
template<typename ContainerType, typename FunctionType>
inline void IterateOverConcurrentlyIfPossible(ContainerType &container, FunctionType func,
	bool run_concurrently = false, bool urgent = false)
{
	size_t index = 0;
#ifdef MULTITHREAD_SUPPORT
	if(run_concurrently && container.size() > 1)
	{
		auto &thread_pool = (urgent ? Concurrency::urgentThreadPool : Concurrency::threadPool);
		auto enqueue_task_lock = thread_pool.AcquireTaskLock();
		if(thread_pool.AreThreadsAvailable())
		{
			auto task_set = thread_pool.CreateCountableTaskSet(container.size());
			for(auto value : container)
			{
				thread_pool.BatchEnqueueTask(
					[index, value, &func, &task_set]
				{
					func(index, value);
					task_set.MarkTaskCompleted();
				}
				);

				index++;
			}

			task_set.WaitForTasks(&enqueue_task_lock);
			return;
		}
	}
	//not running concurrently
#endif

	for(auto value : container)
	{
		func(index, value);
		index++;
	}
}
