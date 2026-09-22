#pragma once

//system headers:
#include <array>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <new>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
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
		//initiate shutdown
		{
			std::unique_lock<std::mutex> lock(threadsMutex);
			shutdownThreads = true;
		}

		//have threads shut themselves down
		waitForTask.notify_all();
		waitForActivate.notify_all();

		//wait for all to shut down
		for(std::thread &worker : threads)
			worker.join();
	}

	//changes the maximum number of active threads
	//if max_num_active_threads is 0, it will attempt to ascertain and
	//use the number of cores specified by hardware
	//only the main thread can reduce the number of threads;
	// if called by any thread other than the main thread, it will not do anything
	void SetMaxNumActiveThreads(int32_t new_max_num_active_threads);

	//returns the current maximum number of threads that are available
	inline int32_t GetMaxNumActiveThreads()
	{
		return maxNumActiveThreads;
	}

	//returns the number of threads that are performing tasks
	inline int32_t GetNumActiveThreads()
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
		//new scope for the lock
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
						AddNewThread();
				}
			}

			numActiveThreads--;
		}

		//awaken another thread
		waitForTask.notify_one();
	}

	//changes the current thread state from waiting to active
	//the thread must currently be waiting, as called by ChangeCurrentThreadStateFromActiveToWaiting
	//this is intended to be called after other threads, which were being waited on, have completed their tasks
	inline void ChangeCurrentThreadStateFromWaitingToActive()
	{
		bool notify = false;
		//new scope for lock
		{
			std::unique_lock<std::mutex> lock(threadsMutex);
			numActiveThreads++;

			//if there are currently more active threads than allowed,
			//transition another active one to reserved
			if(numActiveThreads > maxNumActiveThreads)
			{
				numThreadsToTransitionToReserved++;
				notify = true;
			}
		}

		if(notify)
			waitForTask.notify_one();
	}

	//enqueues a task into the thread pool
	//it is up to the caller to determine when the task is complete
	template<typename Func>
	inline void EnqueueTask(Func &&function)
	{
		//new scope for the lock
		{
			std::unique_lock<std::mutex> lock(threadsMutex);
			taskQueue.push(Task::Create(std::forward<Func>(function)));
		}
		waitForTask.notify_one();
	}

	//acquire a lock to begin enqueueing tasks or querying thread availability
	inline TaskLock AcquireTaskLock()
	{
		return std::unique_lock<std::mutex>(threadsMutex);
	}

	//returns true if there is at least one spare thread available
	bool AreThreadsAvailable()
	{
		//don't spin up new threads if shutting down, since that could cause a deadlock
		if(shutdownThreads) [[unlikely]]
			return false;

		//need to make sure there's at least one extra thread available to make sure that this batch of tasks can be run
		// in case there are any interdependencies, in order to prevent deadlock
		//need to take into account upcoming tasks, as they may consume threads
		auto num_threads_requested = (numActiveThreads - numThreadsToTransitionToReserved)
			+ static_cast<int32_t>(taskQueue.size());
		return (num_threads_requested < maxNumActiveThreads);
	}

	//enqueues a task into the thread pool
	//it is up to the caller to determine when the task is complete
	//AcquireTaskLock must be called to protect the calls to this method
	template <typename Func>
	inline void BatchEnqueueTask(Func &&function)
	{
		taskQueue.push(Task::Create(std::forward<Func>(function)));
	}

	//implements a counter for a set of tasks
	//when the number of tasks has been completed, it WaitForTasks will return
	class CountableTaskSet
	{
	public:
		inline CountableTaskSet(ThreadPool *thread_pool, size_t num_tasks = 0)
			: numTasks(num_tasks), numTasksCompleted(0), threadPool(thread_pool)
		{}

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
			//call the notify_all under a lock to prevent other references to condVar
			//in other threads from attempting to call it on a deallocated object
			std::unique_lock<std::mutex> lock(mutex);
			if(++numTasksCompleted == numTasks)
				condVar.notify_all();
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

	//mutex for the thread pool
	std::mutex threadsMutex;

	//the thread pool
	std::vector<std::thread> threads;

	//condition to notify threads when to start work
	std::condition_variable waitForTask;

	//condition to notify threads when to move from reserved to active
	std::condition_variable waitForActivate;

	//constant memory sized holder for a task that is big enough to
	//cover most small tasks via small buffer optimization, but will allocate on the heap
	//if needed.  note that for performance, it assumes it will be given a valid task
	//before being executed
	struct Task
	{
		using ExecuteFunc = void (*)(void *p);
		using MoveFunc = void (*)(void *dst, const void *src);
		using DestroyFunc = void (*)(void *p);

		inline Task() : execute(nullptr), destroy(nullptr), move(nullptr)
		{}

		//prevent accidental copying to avoid double-destruction
		Task(const Task &) = delete;

		Task &operator=(const Task &) = delete;

		inline Task(Task &&other) noexcept
		{
			if(other.move != nullptr)
				(*other.move)(buffer, other.buffer);
			else
				std::memcpy(buffer, other.buffer, INLINE_SIZE);

			execute = other.execute;
			destroy = other.destroy;
			move = other.move;

			other.destroy = nullptr;
			other.move = nullptr;
		}

		inline Task &operator=(Task &&other) noexcept
		{
			if(this != &other)
			{
				if(destroy != nullptr)
					(*destroy)(buffer);

				if(other.move != nullptr)
					(*other.move)(buffer, other.buffer);
				else
					std::memcpy(buffer, other.buffer, INLINE_SIZE);

				execute = other.execute;
				destroy = other.destroy;
				move = other.move;

				other.destroy = nullptr;
				other.move = nullptr;
			}
			return *this;
		}

		inline ~Task()
		{
			if(destroy != nullptr)
				(*destroy)(buffer);
		}

		//assumes execute is not nullptr; otherwise it wouldn't be a task
		inline void operator()()
		{
			(*execute)(buffer);
		}

		//creates the task from a lambda function
		template<typename F>
		inline static Task Create(F &&f)
		{
			Task t;
			using FuncType = std::decay_t<F>;

			if constexpr(sizeof(FuncType) <= INLINE_SIZE && alignof(FuncType) <= alignof(decltype(Task::buffer)))
			{
				new (t.buffer) FuncType(std::forward<F>(f));

				t.execute = [](void *p) {
					auto *func = std::launder(reinterpret_cast<FuncType *>(p));
					(*func)();
				};

				if constexpr(!std::is_trivially_destructible_v<FuncType>)
				{
					t.destroy = [](void *p) {
						auto *func = std::launder(reinterpret_cast<FuncType *>(p));
						func->~FuncType();
					};
				}
				else
				{
					t.destroy = nullptr;
				}

				if constexpr(!std::is_trivially_copyable_v<FuncType>)
				{
					t.move = [](void *dst, const void *src) {
						auto *source_obj = std::launder(reinterpret_cast<const FuncType *>(src));
						new (dst) FuncType(std::move(*source_obj));
						source_obj->~FuncType();
					};
				}
				else
				{
					t.move = nullptr;
				}
			}
			else
			{
				FuncType *heapFunc = new FuncType(std::forward<F>(f));
				std::memcpy(t.buffer, &heapFunc, sizeof(FuncType *));

				t.execute = [](void *p) {
					auto **ptr_to_func = std::launder(reinterpret_cast<FuncType **>(p));
					(**ptr_to_func)();
				};

				//heap storage always requires a destroy call
				t.destroy = [](void *p) {
					auto **ptr_to_func = std::launder(reinterpret_cast<FuncType **>(p));
					delete *ptr_to_func;
				};

				t.move = nullptr;
			}
			return t;
		}

		static constexpr size_t INLINE_SIZE = 128 - 3 * sizeof(void *);

		ExecuteFunc execute;
		DestroyFunc destroy;
		MoveFunc move;

		alignas(std::max_align_t) uint8_t buffer[INLINE_SIZE];
	};

	//tasks for the thread pool to complete
	std::queue<Task> taskQueue;

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

	//mutex to change the maximum number of threads
	std::mutex setMaxNumActiveThreadsMutex;
};
