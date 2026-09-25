//Keep allocation injection in a standalone executable, on the submitting thread.
//One blocked worker and one two-slot overflow queue make queue growth deterministic.
#define TF_DEFAULT_UNBOUNDED_TASK_QUEUE_LOG_SIZE 1
#include <taskflow/taskflow.hpp>

#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>

static thread_local long fail_after = -1;
static thread_local size_t allocations = 0;

//Keep replacement allocation boundaries visible under optimization as well.
TF_NO_INLINE void* operator new(std::size_t size)
{
	if(fail_after == 0)
	{
		fail_after = -1;
		throw std::bad_alloc();
	}
	if(fail_after > 0) --fail_after;
	++allocations;
	if(void *p = std::malloc(size ? size : 1)) return p;
	throw std::bad_alloc();
}

TF_NO_INLINE void* operator new[](std::size_t size) { return ::operator new(size); }
TF_NO_INLINE void operator delete(void *p) noexcept { std::free(p); }
TF_NO_INLINE void operator delete[](void *p) noexcept { std::free(p); }
TF_NO_INLINE void operator delete(void *p, std::size_t) noexcept { std::free(p); }
TF_NO_INLINE void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

static void Check(bool condition, const char *message)
{
	if(!condition)
	{
		std::cerr << message << '\n';
		//A broken topology counter must fail promptly, not hang in ~Executor.
		std::_Exit(1);
	}
}

template<int Kind, typename F>
static void Submit(tf::Executor &executor, bool named, F &&work)
{
	std::array<tf::AsyncTask, 0> dependencies;
	if constexpr(Kind == 0)
	{
		if(named) executor.async("probe", std::forward<F>(work));
		else executor.async(std::forward<F>(work));
	}
	else if constexpr(Kind == 1)
	{
		if(named) executor.silent_async("probe", std::forward<F>(work));
		else executor.silent_async(std::forward<F>(work));
	}
	else if constexpr(Kind == 2)
	{
		if(named) executor.dependent_async("probe", std::forward<F>(work), dependencies.begin(), dependencies.end());
		else executor.dependent_async(std::forward<F>(work));
	}
	else
	{
		if(named) executor.silent_dependent_async("probe", std::forward<F>(work), dependencies.begin(), dependencies.end());
		else executor.silent_dependent_async(std::forward<F>(work));
	}
}

template<int Kind, bool Runtime>
static void TestFailures(bool named)
{
	tf::Executor executor(1);
	std::atomic<bool> started{false}, release{false};
	executor.silent_async([&]
	{
		started.store(true);
		started.notify_one();
		release.wait(false);
	});
	started.wait(false);
	std::atomic<size_t> executed{0};
	auto token = std::make_shared<int>(0);
	auto submit = [&]
	{
		//Capture padding by value to exceed STL small-function buffers, including MSVC's.
		//Read it in the callback and verify the closure's stored size at compile time.
		if constexpr(Runtime)
		{
			auto work = [&, token, pad = std::array<char, 64>{}](tf::Runtime &)
			{
				Check(pad == std::array<char, 64>{}, "callback padding corrupted");
				++executed;
			};
			static_assert(sizeof(work) >= 64);
			Submit<Kind>(executor, named, std::move(work));
		}
		else
		{
			auto work = [&, token, pad = std::array<char, 64>{}]
			{
				Check(pad == std::array<char, 64>{}, "callback padding corrupted");
				++executed;
			};
			static_assert(sizeof(work) >= 64);
			Submit<Kind>(executor, named, std::move(work));
		}
	};
	auto fail = [&](long after)
	{
		const size_t before = executor.num_topologies();
		const long references = token.use_count();
		bool caught = false;
		fail_after = after;
		try { submit(); }
		catch(const std::bad_alloc &) { caught = true; }
		fail_after = -1;
		Check(caught, "allocation failure was not injected");
		Check(executor.num_topologies() == before, "failed submission leaked a topology reference");
		Check(token.use_count() == references, "failed submission retained its callback");
	};

	const size_t before = allocations;
	submit();
	const size_t construction_allocations = allocations - before;
	Check(construction_allocations > 0, "callback construction did not allocate");
	//The first allocation belongs to promise/packaged-task or node construction.
	fail(0);
	submit(); //Fill the two-slot queue while its only consumer is blocked.
	//Skip exactly the measured construction allocations: fail the first allocation
	//in queue growth, after animate has returned a fully constructed node.
	fail(static_cast<long>(construction_allocations));
	Check(executor.num_topologies() == 3, "successful submissions were decremented early");
	submit(); //Recovery also proves the failed publication left the queue usable.
	release.store(true);
	release.notify_one();
	executor.wait_for_all();
	Check(executor.num_topologies() == 0 && executed == 3, "completion count or execution count is wrong");
}

static void TestDependentChain()
{
	tf::Executor executor(1);
	std::atomic<bool> release{false};
	int value = 0;
	auto first = executor.silent_dependent_async([&] { release.wait(false); value = 1; });
	auto [second, result] = executor.dependent_async([&] { return ++value; }, first);
	std::array dependencies{second};
	auto third = executor.silent_dependent_async([&] { ++value; }, dependencies.begin(), dependencies.end());
	Check(executor.num_topologies() == 3, "dependent submissions were decremented early");
	release.store(true);
	release.notify_one();
	executor.wait_for_all();
	Check(result.get() == 2 && value == 3 && third.is_done(), "dependent chain did not complete");
	Check(executor.num_topologies() == 0, "dependent completion count is wrong");
}

int main()
{
	for(bool named : {false, true})
	{
		TestFailures<0, false>(named);
		TestFailures<0, true>(named);
		TestFailures<1, false>(named);
		TestFailures<1, true>(named);
		TestFailures<2, false>(named);
		TestFailures<2, true>(named);
		TestFailures<3, false>(named);
		TestFailures<3, true>(named);
	}
	TestDependentChain();
	std::cout << "Taskflow allocation and queue-publication failures passed: 16 overload/callable cases, "
		"32 injected failures, recovery and dependent chain\n";
}
