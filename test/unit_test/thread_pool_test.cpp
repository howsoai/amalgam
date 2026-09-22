#include "ThreadPool.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

static void Require(bool condition, const char *message)
{
	if(!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		// Do not unwind through a pool whose worker may be blocked on a test gate.
		// CTest also bounds the whole process, including resize and destruction.
		std::exit(EXIT_FAILURE);
	}
}

// Observe real worker transitions under their mutex; never manufacture pool state.
class ObservedThreadPool : public ThreadPool
{
public:
	using ThreadPool::ThreadPool;

	void WaitForState(std::size_t workers, std::int32_t active, std::int32_t reserved)
	{
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		for(;;)
		{
			{
				auto lock = AcquireTaskLock();
				if(threads.size() == workers && numActiveThreads == active
					&& numReservedThreads == reserved && numThreadsToTransitionToReserved == 0
					&& taskQueue.empty())
					return;
			}
			Require(std::chrono::steady_clock::now() < deadline, "pool did not reach expected state");
			std::this_thread::yield();
		}
	}
};

// With a one-active-thread budget, main waiting must create or reactivate a worker.
// Keep that worker inside its task until main resumes, forcing it into reserve.
static std::thread::id RunTaskAndReserve(ObservedThreadPool &pool, bool reactivation)
{
	std::promise<std::thread::id> started;
	auto started_future = started.get_future();
	std::promise<void> release;
	auto release_future = release.get_future();
	{
		auto lock = pool.AcquireTaskLock();
		pool.BatchEnqueueTask([&]
		{
			started.set_value(std::this_thread::get_id());
			release_future.wait();
		});
	}
	// Batch enqueue deliberately sends no notification: this transition must wake
	// the right waiter. A reserved worker cannot consume a waitForTask notification.
	pool.ChangeCurrentThreadStateFromActiveToWaiting();
	Require(started_future.wait_for(5s) == std::future_status::ready,
		reactivation ? "reserved worker was not reactivated" : "new worker did not start");
	const auto worker_id = started_future.get();
	pool.ChangeCurrentThreadStateFromWaitingToActive();
	release.set_value();
	// Taking the mutex after numReservedThreads is incremented guarantees the
	// worker has entered waitForActivate (the wait atomically releases that mutex).
	pool.WaitForState(1, 1, 1);
	return worker_id;
}

static void TestReserveReactivation()
{
	ObservedThreadPool pool(1);
	const auto worker_id = RunTaskAndReserve(pool, false);
	for(int i = 0; i < 8; ++i)
		Require(RunTaskAndReserve(pool, true) == worker_id, "reserved worker was replaced");
}

static void RunAvailableTask(ObservedThreadPool &pool)
{
	std::promise<int> completed;
	auto result = completed.get_future();
	pool.EnqueueTask([completed = std::move(completed)]() mutable { completed.set_value(42); });
	Require(result.wait_for(5s) == std::future_status::ready, "available worker did not run task");
	Require(result.get() == 42, "incorrect task result");
}

static void TestResize()
{
	ObservedThreadPool pool(1);
	for(int i = 0; i < 4; ++i)
	{
		RunTaskAndReserve(pool, false);
		pool.SetMaxNumActiveThreads(3);
		Require(pool.GetMaxNumActiveThreads() == 3, "pool did not grow");
		RunAvailableTask(pool);
		pool.WaitForState(2, 1, 1);
		// Shrinking must join both the reserved and the available worker.
		// WaitForState also ensures no reserve transition is pending before shrink;
		// resetting an in-flight transition counter is a separate lifecycle concern.
		pool.SetMaxNumActiveThreads(1);
		Require(pool.GetMaxNumActiveThreads() == 1, "pool did not shrink");
		pool.WaitForState(0, 1, 0);
	}
	// Verify reuse after the final shutdown/rebuild too.
	RunTaskAndReserve(pool, false);
	RunTaskAndReserve(pool, true);
}

static void TestShutdown()
{
	{
		ObservedThreadPool pool(2);
		RunAvailableTask(pool);
		pool.WaitForState(1, 1, 0);
	}
	{
		ObservedThreadPool pool(1);
		RunTaskAndReserve(pool, false);
	}
	{
		ObservedThreadPool pool(1);
		RunTaskAndReserve(pool, false);
		pool.SetMaxNumActiveThreads(3);
		RunAvailableTask(pool);
		pool.WaitForState(2, 1, 1);
	}
}

int main(int argc, char **argv)
{
	Require(argc == 2, "expected a test case name");
	const std::string test_case = argv[1];
	if(test_case == "reserve_reactivation")
		TestReserveReactivation();
	else if(test_case == "resize")
		TestResize();
	else if(test_case == "shutdown")
		TestShutdown();
	else
		Require(false, "unknown test case");
	std::cout << "PASS: " << test_case << std::endl;
	return EXIT_SUCCESS;
}
