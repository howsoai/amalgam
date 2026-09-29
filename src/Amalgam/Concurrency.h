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

	//a set of potentially concurrent tasks at a given level of the execution graph
	using TaskSet = tf::Taskflow;

	size_t GetMaxNumThreads();

	//sets the maximum number of threads to use
	// if zero is specified, then it uses a heuristic default based on the system
	void SetMaxNumThreads(size_t max_num_threads);

#ifdef MULTITHREAD_SUPPORT
	//Fork/join independent children in submission order. A waiting caller helps
	//only this group, never an unrelated task from an executor queue.
	void RunInterpreterTasks(std::vector<std::function<void()>> tasks);

	//for garbage collection, cache, query tasks whose caller retains locks and may not execute interpreter code
	void RunSystemTasks(TaskSet &task_set);

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
			std::vector<std::function<void()>> tasks;
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
