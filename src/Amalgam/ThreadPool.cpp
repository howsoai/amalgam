//project headers:
#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t max_num_threads)
{
	shutdownThreads = false;
	ChangeThreadPoolSize(max_num_threads);

	//there must be one active thread
	numActiveThreads = 1;
	
	mainThreadId = std::this_thread::get_id();
}

void ThreadPool::ChangeThreadPoolSize(size_t new_max_num_threads)
{
	std::unique_lock<std::mutex> lock(threadsMutex);

	//don't need to change anything
	if(new_max_num_threads == threads.size())
		return;

	//if reducing thread count, clean up all jobs and clear out all threads
	if(new_max_num_threads < threads.size())
	{
		ShutdownAllThreads();
		threads.clear();

		//no longer shutting down, allow to build up threads
		shutdownThreads = false;
	}

	size_t num_new_threads = new_max_num_threads - threads.size();

	//place an empty idle task for each thread waiting for work
	for(size_t i = 0; i < num_new_threads; i++)
	{
		threads.emplace_back(
			[this]
		{
			//infinite loop waiting for work
			for(;;)
			{
				//container for the task
				std::function<void()> task;

				//fetch from queue
				{
					std::unique_lock<std::mutex> lock(taskQueueMutex);

					//if no more work, wait until shutdown or more work
					if(taskQueue.empty())
					{
						//wait until either shutting down or more work has been added
						waitForTask.wait(lock,
							[this] { return shutdownThreads || !taskQueue.empty(); });

						//only can make it here if shutting down (otherwise taskQueue has something in it)
						if(shutdownThreads)
							return;
					}					

					//take ownership of the task so it can be destructed when complete
					// (won't increment shared_ptr counter)
					task = std::move(taskQueue.front());
					taskQueue.pop();

					//count the thread as active before releasing the lock
					numActiveThreads++;
				}

				task();
				numActiveThreads--;
			}
		}
		);
	}

	//notify all just in case a new task was added as the threads were being created
	// but unlock to allow threads to proceed
	lock.unlock();
	waitForTask.notify_all();
}

ThreadPool::~ThreadPool()
{
	ShutdownAllThreads();
}

void ThreadPool::ShutdownAllThreads()
{
	//initiate shutdown
	{
		std::unique_lock<std::mutex> lock(taskQueueMutex);
		shutdownThreads = true;
	}

	//join all threads
	waitForTask.notify_all();
	for(std::thread &worker : threads)
		worker.join();
}
