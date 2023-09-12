#pragma once

//system headers:
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

//Creates a flexible thread pool for generic tasks
class ThreadPool
{
public:
	ThreadPool(size_t max_num_threads = 0);

	//will change the number of threads in the pool to the number specified
	void ChangeThreadPoolSize(size_t new_max_num_threads);

	//returns the number of threads that are performing tasks
	inline size_t GetNumActiveThreads()
	{
		return numActiveThreads;
	}

	//returns a vector of the thread ids for the thread pool
	inline std::vector<std::thread::id> GetThreadIds()
	{
		std::vector<std::thread::id> thread_ids;
		thread_ids.reserve(threads.size() + 1);
		thread_ids.push_back(mainThreadId);
		for(std::thread &worker : threads)
			thread_ids.push_back(worker.get_id());
		return thread_ids;
	}

	//destroys all the threads and waits to join them
	~ThreadPool();

	//enqueues a task into the thread pool comprised of a function and arguments, automatically inferring the function type
	template<class FunctionType, class ...ArgsType>
	std::future<typename std::invoke_result<FunctionType, ArgsType ...>::type> EnqueueSingleTask(FunctionType &&function, ArgsType &&...args)
	{
		using return_type = typename std::invoke_result<FunctionType, ArgsType ...>::type;

		//create a shared pointer of the task, as we don't know which could happen first, either
		// this function will return and the thread will free the memory, or the thread could return really fast
		// and this function will need to clean up the memory, but both need a valid reference
		auto task = std::make_shared< std::packaged_task<return_type()> >(
										std::bind(std::forward<FunctionType>(function), std::forward<ArgsType>(args) ...)
									);

		//hold the future to return
		std::future<return_type> result = task->get_future();

		//put the task on the queue
		{
			std::unique_lock<std::mutex> lock(taskQueueMutex);
			taskQueue.emplace(
					[task]()
					{
						(*task)();
					}
				);
		}
		waitForTask.notify_one();

		return result;
	}

	//Contains a lock for the task queue for calling EnqueueBatchTask repeatedly while maintaining the lock and layer count
	struct BatchTaskEnqueueLockAndLayer
	{
		inline BatchTaskEnqueueLockAndLayer(std::condition_variable *wait_for_task, std::mutex &task_queue_mutex)
		{
			waitForTask = wait_for_task;
			lock = std::unique_lock<std::mutex>(task_queue_mutex);
		}

		//move constructor to allow a function to build and return the lock
		inline BatchTaskEnqueueLockAndLayer(BatchTaskEnqueueLockAndLayer &&other)
			: waitForTask(std::move(other.waitForTask)), lock(std::move(other.lock))
		{
			//mark the lock as invalid so the other's destructor doesn't try to unlock an invalid lock
			other.waitForTask = nullptr;
		}

		//move assignment
		inline BatchTaskEnqueueLockAndLayer &operator =(BatchTaskEnqueueLockAndLayer &&other)
		{
			std::swap(waitForTask, other.waitForTask);
			std::swap(lock, other.lock);
			return *this;
		}

		//unlocks the lock if locked and invalidates the knowledge of any threads being available
		inline void Unlock()
		{
			//if waitForTask is not nullptr, then the lock must be locked
			// otherwise just let the lock destructor clean up
			if(waitForTask != nullptr)
			{
				lock.unlock();
				waitForTask->notify_all();
				waitForTask = nullptr;
			}
		}

		inline ~BatchTaskEnqueueLockAndLayer()
		{
			Unlock();
		}

		//returns true if there's available threads as denoted by a proper way to notify the threads
		constexpr bool AreThreadsAvailable()
		{
			return (waitForTask != nullptr);
		}

		//marks as there aren't threads available
		constexpr void MarkAsNoThreadsAvailable()
		{
			waitForTask = nullptr;
		}

		//used to notify threads when enqueueing is done
		// this is marked as nullptr if there aren't available threads
		std::condition_variable *waitForTask;

		//lock for enqueueing tasks
		std::unique_lock<std::mutex> lock;
	};

	//attempts to begin a batch of tasks of num_tasks
	//if fail_unless_task_queue_availability is true and there are backlogged tasks,
	// then it will not begin the task batch and return false; this is useful for preventing deadlock
	// when attempting to enqueue tasks which are subtasks of other tasks
	BatchTaskEnqueueLockAndLayer BeginEnqueueBatchTask(bool fail_unless_task_queue_availability = true)
	{
		BatchTaskEnqueueLockAndLayer btel(&waitForTask, taskQueueMutex);

		if(fail_unless_task_queue_availability)
		{
			//need to make sure there's at least one extra thread available to make sure that this batch of tasks can be run
			// in case there are any interdependencies, in order to prevent deadlock
			if(taskQueue.size() + numActiveThreads >= threads.size())
				btel.MarkAsNoThreadsAvailable();			
		}

		return btel;
	}

	//enqueues a task into the thread pool comprised of a function and arguments, automatically inferring the function type
	template<class FunctionType, class ...ArgsType>
	std::future<typename std::invoke_result<FunctionType, ArgsType ...>::type> EnqueueBatchTask(FunctionType &&function, ArgsType &&...args)
	{
		using return_type = typename std::invoke_result<FunctionType, ArgsType ...>::type;

		//create a shared pointer of the task, as we don't know which could happen first, either
		// this function will return and the thread will free the memory, or the thread could return really fast
		// and this function will need to clean up the memory, but both need a valid reference
		auto task = std::make_shared< std::packaged_task<return_type()> >(
										std::bind(std::forward<FunctionType>(function), std::forward<ArgsType>(args) ...)
									);

		//hold the future to return
		std::future<return_type> result = task->get_future();

		//put the task on the queue
		taskQueue.emplace(
			[task]()
			{
				(*task)();
			}
		);

		return result;
	}

	//if a thread will be sitting waiting for other threads to complete, it can mark itself as inactive
	// but it should call ResumeCurrentThread once ready again
	inline void CountCurrentThreadAsPaused()
	{
		numActiveThreads--;
	}

	//if a thread will be sitting waiting for other threads to complete, it can mark itself as inactive via PauseCurrentThread
	// and should call ResumeCurrentThread once ready again
	inline void CountCurrentThreadAsResumed()
	{
		numActiveThreads++;
	}

private:
	//waits for all threads to complete, then shuts them down
	void ShutdownAllThreads();

	//the thread pool
	std::mutex threadsMutex;
	std::vector<std::thread> threads;

	//lock to notify threads when to start work
	std::condition_variable waitForTask;

	//if true, then all threads should end work so they can be joined
	bool shutdownThreads;

	//tasks for the threadpool to complete
	std::mutex taskQueueMutex;
	std::queue<std::function<void()>> taskQueue;

	//number of threads running
	std::atomic<size_t> numActiveThreads;

	//id of the main thread
	std::thread::id mainThreadId;
};
