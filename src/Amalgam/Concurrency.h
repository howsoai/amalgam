#pragma once

//if MULTITHREAD_SUPPORT is defined, compiles code with multithreaded support, requires a C++0x17 or newer compiler
//MULTITHREAD_SUPPORT means multithreading will be enabled everywhere, including the appropriate locks
//MULTITHREAD_INTERFACE means that multithreading will be enabled only for the interface,
// which means that multithreaded applications can call this library.  This is a subset of MULTITHREAD_SUPPORT
//MULTITHREAD_ENTITY_CALL_MUTEX will only allow one call per entity as an external library.

#ifdef _OPENMP
#include <omp.h>
#endif

#include <thread>

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
#include <mutex>
#include <shared_mutex>
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

	//vector of standard read locks
	typedef std::vector<ReadLock> ReadLocksBuffer;

	//vector of standard write locks
	typedef std::vector<WriteLock> WriteLocksBuffer;

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
	extern ThreadPool threadPool;
#endif
};
#endif
