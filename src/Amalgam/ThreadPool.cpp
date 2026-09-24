//project headers:
#include "ThreadPool.h"

//system headers:

ThreadPool::ThreadPool(int32_t max_num_active_threads)
{
	shutdownThreads = false;

	maxNumActiveThreads = 1;
	numActiveThreads = 1;
	numReservedThreads = 0;
	numThreadsToTransitionToReserved = 0;

	mainThreadId = std::this_thread::get_id();

	SetMaxNumActiveThreads(max_num_active_threads);
}

void ThreadPool::SetMaxNumActiveThreads(int32_t new_max_num_active_threads)
{
	//only allow one call of this method at a time, because
	//shutting down threads frees the thread lock
	std::lock_guard single_call(setMaxNumActiveThreadsMutex);

	std::unique_lock<std::mutex> lock(threadsMutex);

	//if zero is specified, attempt to get hardware concurrency
	if(new_max_num_active_threads == 0)
		new_max_num_active_threads = std::thread::hardware_concurrency();

	//don't need to change anything
	if(new_max_num_active_threads == maxNumActiveThreads || new_max_num_active_threads < 1)
		return;

	//if reducing thread count, clean up all jobs and clear out all threads
	if(new_max_num_active_threads < maxNumActiveThreads)
	{
		//can't reduce number of threads if this isn't the main thread
		if(mainThreadId != std::this_thread::get_id())
			return;

		shutdownThreads = true;
		lock.unlock();

		//have threads shut themselves down
		waitForTask.notify_all();
		waitForActivate.notify_all();

		//wait for all to shut down
		for(std::thread &worker : threads)
			worker.join();

		lock.lock();

		threads.clear();

		//no longer shutting down, allow to build up threads
		shutdownThreads = false;

		//reset other stats
		maxNumActiveThreads = 1;
		numActiveThreads = 1;
		numReservedThreads = 0;
		numThreadsToTransitionToReserved = 0;
	}

	//place an empty idle task for each thread waiting for work
	//but current thread counts as one
	for(int32_t i = static_cast<int32_t>(threads.size()); i < new_max_num_active_threads - 1; i++)
		AddNewThread();

	maxNumActiveThreads = new_max_num_active_threads;

	//notify all just in case a new task was added as the threads were being created
	lock.unlock();
	waitForTask.notify_all();
}

void ThreadPool::AddNewThread()
{
	// Count starting workers before releasing threadsMutex.
	++numActiveThreads;
	try
	{
		threads.emplace_back(
			[this]
			{
				//infinite loop waiting for work; lock is unlocked for going around the loop
				for(;;)
				{
					if(numThreadsToTransitionToReserved > 0 && !shutdownThreads.load(std::memory_order_acquire))
					{
						std::unique_lock<std::mutex> lock(threadsMutex);
						//double check transition under lock
						if(numThreadsToTransitionToReserved <= 0 || shutdownThreads.load(std::memory_order_acquire))
							continue;

						//go into reserved
						numActiveThreads--;
						numThreadsToTransitionToReserved--;
						numReservedThreads++;

						//wait until either shutting down or a thread is requested to come out of reserved
						waitForActivate.wait(lock,
							[this] { return numThreadsToTransitionToReserved < 0 || shutdownThreads.load(std::memory_order_acquire); });

						//On shutdown reserved workers also drain already accepted work.
						numActiveThreads++;
						if(!shutdownThreads.load(std::memory_order_acquire))
							numThreadsToTransitionToReserved++;
						numReservedThreads--;
					}
					else //fetching task
					{
						//take ownership of the task so it can be destructed when complete
						if(auto task = taskQueue.pop())
						{
							(*task)();
						}
						else //no more work, wait until shutdown or more work
						{
							std::unique_lock<std::mutex> lock(threadsMutex);

							//double check if empty under lock
							if(!taskQueue.empty())
								continue;

							taskQueue.reclaim();
							numActiveThreads--;

							//wait until either shutting down or more work has been added
							waitForTask.wait(lock, [this] {
								return !taskQueue.empty() || numThreadsToTransitionToReserved > 0 ||
									shutdownThreads.load(std::memory_order_acquire);
							});

							//Exit only after accepted work has been dispatched.
							if(shutdownThreads.load(std::memory_order_acquire) && taskQueue.empty()) [[unlikely]]
								return;

							//got a task, resuming the thread
							numActiveThreads++;

							//if transitioning to reserved, don't grab a task
							if(numThreadsToTransitionToReserved > 0)
								continue;
						}
					}
				}
			}
		);
	}
	catch(...)
	{
		--numActiveThreads;
		throw;
	}
}
