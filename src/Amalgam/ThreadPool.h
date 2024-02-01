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

//Creates a flexible thread pool for generic tasks aimed at making sure a specified
// number of CPU cores worth of compute can be active at any one time.  Because threads
// are sometimes in idle states waiting on other threads to complete, the total number
// of threads in the thread pool may exceed the number of allowed active threads
//
//threads have four states:
// available -- the thread is ready and waiting for a task
// active -- the thread is currently executing a task
// waiting -- the thread is idle, waiting for other threads to finish tasks
//             this allows another thread to be created or move from reserve to available
// reserved -- the thread is idle, but cannot accept a task because the number of active
//             plus the number of available threads is equal to maxNumActiveThreads
class ThreadPool
{
public:
	ThreadPool(size_t max_num_active_threads = 0);

	//destroys all the threads and waits to join them
	~ThreadPool()
	{
		ShutdownAllThreads();
	}

	//changes the maximum number of active threads
	inline void SetMaxNumActiveThreads(size_t max_num_active_threads);

	//returns the current maximum number of threads that are available
	constexpr size_t GetMaxNumActiveThreads()
	{
		return maxNumActiveThreads;
	}

	//returns the number of threads that are performing tasks
	inline size_t GetNumActiveThreads()
	{
		return numActiveThreads;
	}

	//returns a vector of the thread ids for the thread pool
	inline std::vector<std::thread::id> GetThreadIds()
	{
		std::vector<std::thread::id> thread_ids;

		std::unique_lock<std::mutex> lock(threadsMutex);
		thread_ids.reserve(threads.size() + 1);
		thread_ids.push_back(mainThreadId);
		for(std::thread &worker : threads)
			thread_ids.push_back(worker.get_id());
		return thread_ids;
	}

	//returns true if there are threads currently idle
	inline bool AreThreadsAvailable()
	{
		std::unique_lock<std::mutex> lock(threadsMutex);
		//need to make sure there's at least one extra thread available to make sure that this batch of tasks can be run
		// in case there are any interdependencies, in order to prevent deadlock
		//need to take into account upcoming tasks, as they may consume threads
		return (numActiveThreads + 1 + taskQueue.size() <= maxNumActiveThreads);
	}

	//changes the current thread state from active to waiting
	//the thread must currently be active
	//this is intended to be called before waiting for other threads to complete their tasks
	void ChangeCurrentThreadStateFromActiveToWaiting();

	//changes the current thread state from waiting to active
	//the thread must currently be waiting, as called by ChangeCurrentThreadStateFromActiveToWaiting
	//this is intended to be called after other threads, which were being waited on, have completed their tasks
	void ChangeCurrentThreadStateFromWaitingToActive();

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
			std::unique_lock<std::mutex> lock(threadsMutex);
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
		BatchTaskEnqueueLockAndLayer btel(&waitForTask, threadsMutex);

		if(fail_unless_task_queue_availability)
		{
			//need to make sure there's at least one extra thread available to make sure that this batch of tasks can be run
			// in case there are any interdependencies, in order to prevent deadlock
			//need to take into account upcoming tasks, as they may consume threads
			if(!(numActiveThreads + 1 + taskQueue.size() <= maxNumActiveThreads))
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

protected:
	//adds a new thread to threads
	// threadsMutex must be locked prior to calling
	void AddNewThread();

	//waits for all threads to complete, then shuts them down
	void ShutdownAllThreads();

	//the thread pool
	std::mutex threadsMutex;
	std::vector<std::thread> threads;

	//condition to notify threads when to start work
	std::condition_variable waitForTask;

	//condition to notify threads when to move from reserved to active
	std::condition_variable waitForActivate;

	//if true, then all threads should end work so they can be joined
	bool shutdownThreads;

	//tasks for the threadpool to complete
	std::queue<std::function<void()>> taskQueue;

	//the number of threads that can be active at any time
	//the total number of threads is
	//numActiveThreads + numReservedThreads + number of idle threads
	size_t maxNumActiveThreads;

	//number of threads running
	//atomic so that it can be read dynamically without a lock
	std::atomic<size_t> numActiveThreads;

	//number of threads that are currently in reserve
	//that can be activated to replace an existing thread that is blocked
	size_t numReservedThreads;

	//number of threads that need to be switched to reserve state
	//if positive, as threads become available they can decrement the value
	//transition to reserved.  if negative, then reserved threads can increment
	//the value to become available
	int64_t numThreadsToTransitionToReserved;

	//id of the main thread
	std::thread::id mainThreadId;
};
