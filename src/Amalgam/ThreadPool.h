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

	//returns true if there are threads currently idle
	inline bool AreThreadsAvailable()
	{
		std::unique_lock<std::mutex> lock(threadsMutex);
		//need to make sure there's at least one extra thread available to make sure that this batch of tasks can be run
		// in case there are any interdependencies, in order to prevent deadlock
		//need to take into account upcoming tasks, as they may consume threads
		return ((numActiveThreads - numThreadsToTransitionToReserved) + 1 + static_cast<int32_t>(taskQueue.size()) <= maxNumActiveThreads);
	}

	//changes the current thread state from active to waiting
	//the thread must currently be active
	//this is intended to be called before waiting for other threads to complete their tasks
	inline void ChangeCurrentThreadStateFromActiveToWaiting()
	{
		{
			std::unique_lock<std::mutex> lock(threadsMutex);

			size_t task_queue_size = taskQueue.size();
			int32_t num_threads_needed = maxNumActiveThreads;
			//if less than the number of active threads, then small enough to safely cast to the smaller type
			if(task_queue_size < static_cast<size_t>(maxNumActiveThreads))
				num_threads_needed = static_cast<int32_t>(task_queue_size);

			//compute and compare the current threadpool size to that which is needed
			int32_t cur_threadpool_size = static_cast<int32_t>(threads.size());
			int32_t needed_threadpool_size = (numReservedThreads + numThreadsToTransitionToReserved) + num_threads_needed;
			if(cur_threadpool_size < needed_threadpool_size)
			{
				//if there are reserved threads, use them, otherwise create a new thread
				if(numReservedThreads > 0)
				{
					numThreadsToTransitionToReserved--;
				}
				else
				{
					for(; cur_threadpool_size < needed_threadpool_size; cur_threadpool_size++)
						AddNewThread();
				}
			}

			numActiveThreads--;
		}

		//activate another thread to take this one's place
		waitForActivate.notify_one();
	}

	//changes the current thread state from waiting to active
	//the thread must currently be waiting, as called by ChangeCurrentThreadStateFromActiveToWaiting
	//this is intended to be called after other threads, which were being waited on, have completed their tasks
	inline void ChangeCurrentThreadStateFromWaitingToActive()
	{
		{
			std::unique_lock<std::mutex> lock(threadsMutex);
			numActiveThreads++;

			//if there are currently more active threads than allowed,
			//transition another active one to reserved
			if(numActiveThreads > maxNumActiveThreads)
				numThreadsToTransitionToReserved++;
		}

		//get another thread to transition to reserved
		waitForTask.notify_one();
	}

	//enqueues a task into the thread pool
	//it is up to the caller to determine when the task is complete
	inline void EnqueueTask(std::function<void()> &&function)
	{
		{
			std::unique_lock<std::mutex> lock(threadsMutex);
			taskQueue.emplace(std::move(function));
		}
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

	//Contains a lock for the task queue for calling BatchEnqueueTaskWithResult repeatedly while maintaining the lock and layer count
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
	inline BatchTaskEnqueueLockAndLayer BeginEnqueueBatchTask(bool fail_unless_task_queue_availability = true)
	{
		BatchTaskEnqueueLockAndLayer btel(&waitForTask, threadsMutex);

		if(fail_unless_task_queue_availability)
		{
			//need to make sure there's at least one extra thread available to make sure that this batch of tasks can be run
			// in case there are any interdependencies, in order to prevent deadlock
			//need to take into account upcoming tasks, as they may consume threads
			if(!((numActiveThreads - numThreadsToTransitionToReserved) + 1 + static_cast<int32_t>(taskQueue.size()) <= maxNumActiveThreads))
				btel.MarkAsNoThreadsAvailable();
		}

		return btel;
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
		inline CountableTaskSet(size_t num_tasks = 0)
			: numTasks(num_tasks), numTasksCompleted(0)
		{	}

		//increments the number of tasks by num_new_tasks
		inline void AddTask(size_t num_new_tasks = 1)
		{
			numTasks.fetch_add(num_new_tasks);
		}

		//returns when all the tasks have been completed
		inline void WaitForTasks()
		{
			std::unique_lock<std::mutex> lock(mutex);
			cond_var.wait(lock, [this] { return numTasksCompleted >= numTasks; });
		}

		//marks one task as completed
		inline void MarkTaskCompleted()
		{
			size_t prev_tasks_completed = numTasksCompleted.fetch_add(1);
			if(prev_tasks_completed + 1 == numTasks)
			{
				//in theory, this lock may not be necessary, but in practice it is to prevent deadlock
				std::unique_lock<std::mutex> lock(mutex);
				cond_var.notify_all();
			}
		}

	protected:
		std::atomic<size_t> numTasks;
		std::atomic<size_t> numTasksCompleted;
		std::mutex mutex;
		std::condition_variable cond_var;
	};

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

	//tasks for the threadpool to complete
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
