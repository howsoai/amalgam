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
	typedef std::unique_lock<std::mutex> TaskLock;

	ThreadPool(int32_t max_num_active_threads = 0);

	//destroys all the threads and waits to join them
	~ThreadPool()
	{
		ShutdownAllThreads();
	}

	//changes the maximum number of active threads
	//if max_num_active_threads is 0, it will attempt to ascertain and
	//use the number of cores specified by hardware
	void SetMaxNumActiveThreads(int32_t max_num_active_threads);

	//returns the current maximum number of threads that are available
	constexpr int32_t GetMaxNumActiveThreads()
	{
		return maxNumActiveThreads;
	}

	//returns the number of threads that are performing tasks
	constexpr int32_t GetNumActiveThreads()
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

	//changes the current thread state from active to waiting
	//the thread must currently be active
	//this is intended to be called before waiting for other threads to complete their tasks
	inline void ChangeCurrentThreadStateFromActiveToWaiting()
	{
		std::unique_lock<std::mutex> lock(threadsMutex);

		size_t task_queue_size = taskQueue.size();
		int32_t num_threads_needed = maxNumActiveThreads;
		//if less than the number of active threads, then small enough to safely cast to the smaller type
		if(task_queue_size < static_cast<size_t>(maxNumActiveThreads))
			num_threads_needed = static_cast<int32_t>(task_queue_size);

		//compute and compare the current thread pool size to that which is needed
		int32_t cur_thread_pool_size = static_cast<int32_t>(threads.size());
		int32_t needed_thread_pool_size = (numReservedThreads + numThreadsToTransitionToReserved) + num_threads_needed;
		if(cur_thread_pool_size < needed_thread_pool_size)
		{
			//if there are reserved threads, use them, otherwise create a new thread
			if(numReservedThreads > 0)
			{
				numThreadsToTransitionToReserved--;
			}
			else
			{
				for(; cur_thread_pool_size < needed_thread_pool_size; cur_thread_pool_size++)
				{
					AddNewThread();
					waitForTask.notify_one();
				}
			}
		}

		numActiveThreads--;
	}

	//changes the current thread state from waiting to active
	//the thread must currently be waiting, as called by ChangeCurrentThreadStateFromActiveToWaiting
	//this is intended to be called after other threads, which were being waited on, have completed their tasks
	inline void ChangeCurrentThreadStateFromWaitingToActive()
	{
		std::unique_lock<std::mutex> lock(threadsMutex);
		numActiveThreads++;

		//if there are currently more active threads than allowed,
		//transition another active one to reserved
		if(numActiveThreads > maxNumActiveThreads)
		{
			numThreadsToTransitionToReserved++;
			waitForTask.notify_one();
		}
	}

	//enqueues a task into the thread pool
	//it is up to the caller to determine when the task is complete
	inline void EnqueueTask(std::function<void()> &&function)
	{
		std::unique_lock<std::mutex> lock(threadsMutex);
		taskQueue.emplace(std::move(function));
		lock.unlock();

		waitForTask.notify_one();
	}

	//enqueues a task into the thread pool comprised of a function and arguments, automatically inferring the function type
	template<class FunctionType, class ...ArgsType>
	inline std::future<typename std::invoke_result<FunctionType, ArgsType ...>::type> EnqueueTaskWithResult(FunctionType &&function, ArgsType &&...args)
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

		EnqueueTask(
			[task]()
			{
				(*task)();
			}
		);

		return result;
	}

	//acquire a lock to begin enqueueing tasks or querying thread availability
	inline TaskLock AcquireTaskLock()
	{
		return std::unique_lock<std::mutex>(threadsMutex);
	}

	//returns true if there is at least one spare thread available
	bool AreThreadsAvailable()
	{
		//need to make sure there's at least one extra thread available to make sure that this batch of tasks can be run
		// in case there are any interdependencies, in order to prevent deadlock
		//need to take into account upcoming tasks, as they may consume threads
		auto num_threads_requested = (numActiveThreads - numThreadsToTransitionToReserved)
			+ static_cast<int32_t>(taskQueue.size());
		return (num_threads_requested < maxNumActiveThreads);
	}

	//enqueues a task into the thread pool
	//it is up to the caller to determine when the task is complete
	inline void BatchEnqueueTask(std::function<void()> &&function)
	{
		taskQueue.emplace(std::move(function));
	}

	//enqueues a task into the thread pool comprised of a function and arguments, automatically inferring the function type
	template<class FunctionType, class ...ArgsType>
	inline std::future<typename std::invoke_result<FunctionType, ArgsType ...>::type> BatchEnqueueTaskWithResult(FunctionType &&function, ArgsType &&...args)
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

		BatchEnqueueTask(
			[task]()
			{
				(*task)();
			}
		);

		return result;
	}

	//implements a counter for a set of tasks
	//when the number of tasks has been completed, it WaitForTasks will return
	class CountableTaskSet
	{
	public:
		inline CountableTaskSet(ThreadPool *thread_pool, size_t num_tasks = 0)
			: numTasks(num_tasks), numTasksCompleted(0), threadPool(thread_pool)
		{	}

		//increments the number of tasks by num_new_tasks
		inline void AddTask(size_t num_new_tasks = 1)
		{
			std::unique_lock<std::mutex> lock(mutex);
			numTasks += num_new_tasks;
		}

		//returns when all the tasks have been completed
		//if task_enqueue_lock is not nullptr, it will unlock it and begin execution
		inline void WaitForTasks(TaskLock *task_enqueue_lock = nullptr)
		{
			if(task_enqueue_lock != nullptr)
			{
				task_enqueue_lock->unlock();
				threadPool->waitForTask.notify_all();
			}

			threadPool->ChangeCurrentThreadStateFromActiveToWaiting();
			
			{
				std::unique_lock<std::mutex> task_lock(mutex);
				condVar.wait(task_lock, [this] { return numTasksCompleted >= numTasks; });
			}

			threadPool->ChangeCurrentThreadStateFromWaitingToActive();
		}

		//marks one task as completed
		inline void MarkTaskCompleted()
		{
			std::unique_lock<std::mutex> lock(mutex);
			if(++numTasksCompleted == numTasks)
			{
				lock.unlock();
				condVar.notify_all();
			}
		}

		//marks one task as completed, but can be called from the thread setting up the tasks
		inline void MarkTaskCompletedBeforeWaitForTasks()
		{
			std::unique_lock<std::mutex> lock(mutex);
			numTasksCompleted++;
		}

	protected:
		//the counters are not atomic as the condVar needs a mutex around any change of value anyway
		size_t numTasks;
		size_t numTasksCompleted;
		std::mutex mutex;
		std::condition_variable condVar;
		ThreadPool *threadPool;
	};

	//creates a CountableTaskSet for this ThreadPool
	inline CountableTaskSet CreateCountableTaskSet(size_t num_tasks = 0)
	{
		return CountableTaskSet(this, num_tasks);
	}

protected:
	//adds a new thread to threads
	// threadsMutex must be locked prior to calling
	void AddNewThread();

	//waits for all threads to complete, then shuts them down
	void ShutdownAllThreads();

	//mutex for the thread pool
	std::mutex threadsMutex;

	//the thread pool
	std::vector<std::thread> threads;

	//condition to notify threads when to start work
	std::condition_variable waitForTask;

	//condition to notify threads when to move from reserved to active
	std::condition_variable waitForActivate;

	//tasks for the thread pool to complete
	std::queue<std::function<void()>> taskQueue;

	//the number of threads that can be active at any time
	//the total number of threads is
	//numActiveThreads + numReservedThreads + number of idle threads
	int32_t maxNumActiveThreads;

	//number of threads running
	int32_t numActiveThreads;

	//number of threads that are currently in reserve
	//that can be activated to replace an existing thread that is blocked
	int32_t numReservedThreads;

	//number of threads that need to be switched to reserve state
	//if positive, as threads become available they can decrement the value
	//transition to reserved.  if negative, then reserved threads can increment
	//the value to become available
	int32_t numThreadsToTransitionToReserved;

	//if true, then all threads should end work so they can be joined
	bool shutdownThreads;

	//id of the main thread
	std::thread::id mainThreadId;
};
