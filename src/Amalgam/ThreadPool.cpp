//project headers:
#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t max_num_threads)
{
	shutdownThreads = false;

	maxNumActiveThreads = 1;
	numActiveThreads = 1;
	numReserveThreads = 0;

	ChangeThreadPoolSize(max_num_threads);


	mainThreadId = std::this_thread::get_id();
}

void ThreadPool::ChangeThreadPoolSize(size_t new_max_num_threads)
{
	std::unique_lock<std::mutex> threads_lock(threadsMutex);

	//don't need to change anything
	if(new_max_num_threads == maxNumActiveThreads)
		return;

	//if reducing thread count, clean up all jobs and clear out all threads
	if(new_max_num_threads < threads.size())
	{
		ShutdownAllThreads();
		threads.clear();

		//no longer shutting down, allow to build up threads
		shutdownThreads = false;

		//reset other stats
		maxNumActiveThreads = 1;
		numActiveThreads = 1;
		numReserveThreads = 0;
	}

	size_t num_new_threads = new_max_num_threads - threads.size();

	//place an empty idle task for each thread waiting for work
	for(size_t i = 0; i < num_new_threads; i++)
		AddNewThread();

	//notify all just in case a new task was added as the threads were being created
	// but unlock to allow threads to proceed
	threads_lock.unlock();
	waitForTask.notify_all();
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

			//infinite loop waiting for work
			for(;;)
			{
				//container for the task
				std::function<void()> task;

				//fetch from queue
				{
					std::unique_lock<std::mutex> lock(threadsMutex);

					//if no more work, wait until shutdown or more work
					if(taskQueue.empty())
					{
						numActiveThreads--;

						//wait until either shutting down or more work has been added
						waitForTask.wait(lock,
							[this] { return shutdownThreads || !taskQueue.empty(); });

						//only can make it here if shutting down (otherwise taskQueue has something in it)
						if(shutdownThreads)
							return;

						//got a task, resuming the thread
						numActiveThreads++;
					}

					//take ownership of the task so it can be destructed when complete
					// (won't increment shared_ptr counter)
					task = std::move(taskQueue.front());
					taskQueue.pop();
				}

				task();
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
	for(std::thread &worker : threads)
		worker.join();
}
