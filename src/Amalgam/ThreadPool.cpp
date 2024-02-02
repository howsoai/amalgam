//project headers:
#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t max_num_active_threads)
{
	shutdownThreads = false;

	maxNumActiveThreads = 1;
	numActiveThreads = 1;
	numReservedThreads = 0;
	numThreadsToTransitionToReserved = 0;

	SetMaxNumActiveThreads(max_num_active_threads);

	mainThreadId = std::this_thread::get_id();
}

void ThreadPool::SetMaxNumActiveThreads(size_t new_max_num_active_threads)
{
	std::unique_lock<std::mutex> lock(threadsMutex);

	//don't need to change anything
	if(new_max_num_active_threads == maxNumActiveThreads || new_max_num_active_threads < 1)
		return;

	//if reducing thread count, clean up all jobs and clear out all threads
	if(new_max_num_active_threads < maxNumActiveThreads)
	{
		lock.unlock();
		ShutdownAllThreads();
		lock.lock();

		threads.clear();

		//no longer shutting down, allow to build up threads
		shutdownThreads = false;

		//reset other stats
		maxNumActiveThreads = 1;
		numActiveThreads = 1;
		numReservedThreads = 0;
	}

	//place an empty idle task for each thread waiting for work
	//but current thread counts as one
	for(size_t i = threads.size(); i < new_max_num_active_threads - 1; i++)
		AddNewThread();

	maxNumActiveThreads = new_max_num_active_threads;

	//notify all just in case a new task was added as the threads were being created
	// but unlock to allow threads to proceed
	lock.unlock();
	waitForTask.notify_all();
}

void ThreadPool::ChangeCurrentThreadStateFromActiveToWaiting()
{
	{
		std::unique_lock<std::mutex> lock(threadsMutex);

		//only add a new thread if no reserved and at capacity
		if(numReservedThreads == 0 && numActiveThreads + 1 == maxNumActiveThreads)
			AddNewThread();
		else
			numThreadsToTransitionToReserved--;

		numActiveThreads--;
	}

	//activate another thread to take this one's place
	waitForActivate.notify_one();
}

void ThreadPool::ChangeCurrentThreadStateFromWaitingToActive()
{
	{
		std::unique_lock<std::mutex> lock(threadsMutex);
		numActiveThreads++;
		numThreadsToTransitionToReserved++;
	}

	//get another thread to transition to reserved
	waitForTask.notify_one();
}

void ThreadPool::AddNewThread()
{
	threads.emplace_back(
		[this]
		{
			//count this thread as active during startup
			//this is important, as the inner loop assumes the default state of the thread is to count itself
			//so the number of threads doesn't change when switching between a completed task and a new one
			numActiveThreads++;

			std::unique_lock<std::mutex> lock(threadsMutex);

			//infinite loop waiting for work
			for(;;)
			{
				if(numThreadsToTransitionToReserved > 0)
				{
					//go into reserved
					numActiveThreads--;
					numThreadsToTransitionToReserved--;
					numReservedThreads++;

					//wait until either shutting down or a thread is requested to come out of reserved
					waitForActivate.wait(lock,
						[this] { return shutdownThreads || numThreadsToTransitionToReserved < 0; });

					//only can make it here if shutting down (otherwise taskQueue has something in it)
					if(shutdownThreads)
						return;

					//coming out of reserved, go active unless no task
					numActiveThreads++;
					numThreadsToTransitionToReserved++;
					numReservedThreads--;
				}
				else //fetching task
				{
					//if no more work, wait until shutdown or more work
					if(taskQueue.empty())
					{
						numActiveThreads--;

						//wait until either shutting down or more work has been added
						waitForTask.wait(lock,
							[this] { return shutdownThreads || !taskQueue.empty() || numThreadsToTransitionToReserved > 0; });

						//only can make it here if shutting down (otherwise taskQueue has something in it)
						if(shutdownThreads)
							return;

						//got a task, resuming the thread
						numActiveThreads++;

						//if transitioning to reserved, don't grab a task
						if(numThreadsToTransitionToReserved > 0)
							continue;
					}

					//take ownership of the task so it can be destructed when complete
					// (won't increment shared_ptr counter)
					std::function<void()> task = std::move(taskQueue.front());
					taskQueue.pop();

					lock.unlock();
					task();
					lock.lock();
				}
			}
		}
	);
}

void ThreadPool::ShutdownAllThreads()
{
	//initiate shutdown
	{
		std::unique_lock<std::mutex> lock(threadsMutex);
		shutdownThreads = true;
	}

	//join all threads
	waitForTask.notify_all();
	waitForActivate.notify_all();
	for(std::thread &worker : threads)
		worker.join();
}
