#include "Concurrency.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <barrier>
#include <source_location>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

static void Check(bool condition, std::source_location location = std::source_location::current())
{
	if(!condition)
		throw std::runtime_error(std::string("Concurrency check failed at ") + location.file_name() + ":" + std::to_string(location.line()));
}

//Each non-atomic result has one writer, and the continuation must observe every
//write. Reusing the graph also checks that Taskflow resets dependency counters.
static void TestGraph(size_t count, size_t repeats)
{
	std::vector<size_t> results(count, 0);
	std::vector<std::atomic<size_t>> visits(count);
	tf::Taskflow graph;
	size_t iteration = 0;
	size_t continuations = 0;
	auto continuation = graph.emplace([&]
	{
		++continuations;
		for(size_t i = 0; i < count; ++i)
		{
			Check(results[i] == i + 1);
			Check(visits[i] == iteration + 1);
		}
	});
	for(size_t i = 0; i < count; ++i)
		graph.emplace([&, i]
		{
			visits[i].fetch_add(1);
			results[i] = i + 1;
		}).precede(continuation);
	for(; iteration < repeats; ++iteration)
	{
		std::fill(results.begin(), results.end(), 0);
		Concurrency::RunTaskflow(graph);
		Check(continuations == iteration + 1);
		Check(std::accumulate(results.begin(), results.end(), size_t{0}) == count * (count + 1) / 2);
	}
}

static void TestCoarse(size_t count)
{
	auto compute = [](uint64_t value)
	{
		for(size_t i = 0; i < 100000; ++i)
			value = value * 6364136223846793005ULL + 1442695040888963407ULL;
		return value;
	};
	std::vector<uint64_t> results(count);
	tf::Taskflow graph;
	for(size_t i = 0; i < count; ++i)
		graph.emplace([&, i] { results[i] = compute(i); });
	Concurrency::RunTaskflow(graph);
	for(size_t i = 0; i < count; ++i)
		Check(results[i] == compute(i));
}

static size_t Nested(size_t width, size_t depth, bool maintenance = false)
{
	if(depth == 0)
		return 1;
	std::vector<size_t> results(width);
	size_t total = 0;
	if(maintenance)
	{
		tf::Taskflow graph;
		for(size_t i = 0; i < width; ++i)
			graph.emplace([&, i] { results[i] = Nested(width, depth - 1, true); });
		Concurrency::RunMaintenanceTaskflow(graph);
	}
	else
	{
		std::vector<std::function<void()>> children;
		for(size_t i = 0; i < width; ++i)
			children.emplace_back([&, i] { results[i] = Nested(width, depth - 1); });
		Concurrency::RunInterpreterTasks(std::move(children));
	}
	for(auto result : results)
	{
		Check(result != 0);
		total += result;
	}
	return total;
}

//A runtime reference must be rolled back when its child cannot be constructed.
struct ThrowOnCopy
{
	ThrowOnCopy() = default;
	ThrowOnCopy(const ThrowOnCopy &) { throw std::runtime_error("submission exception"); }
	void operator()(tf::Runtime &) const {}
};

static void TestSubmissionException()
{
	tf::Taskflow graph;
	bool caught = false, continued = false;
	auto parent = graph.emplace([&](tf::Runtime &runtime)
	{
		ThrowOnCopy work;
		try { runtime.silent_async(work); }
		catch(const std::runtime_error &e) { caught = std::string_view(e.what()) == "submission exception"; }
	});
	parent.precede(graph.emplace([&] { continued = true; }));
	Concurrency::RunTaskflow(graph);
	Check(caught && continued);
}

static void TestExceptions()
{
	for(bool nested : {false, true})
	{
		tf::Taskflow graph;
		std::atomic<size_t> running{0};
		bool fail = true;
		graph.emplace([&]
		{
			if(!fail)
				return;
			if(nested)
			{
				tf::Taskflow child;
				child.emplace([] { throw std::runtime_error("child exception"); });
				Concurrency::RunMaintenanceTaskflow(child);
			}
			else
				throw std::runtime_error("child exception");
		});
		for(size_t i = 0; i < 100; ++i)
			graph.emplace([&]
			{
				++running;
				std::this_thread::yield();
				--running;
			});
		bool caught = false;
		try { Concurrency::RunTaskflow(graph); }
		catch(const std::runtime_error &e) { caught = std::string_view(e.what()) == "child exception"; }
		Check(caught && running == 0);
		fail = false;
		Concurrency::RunTaskflow(graph);
		Check(running == 0);
	}

	tf::Taskflow graph;
	graph.emplace([] { throw std::runtime_error("maintenance exception"); });
	bool caught = false;
	try { Concurrency::RunMaintenanceTaskflow(graph); }
	catch(const std::runtime_error &) { caught = true; }
	Check(caught);

	graph.clear();
	graph.emplace([]
	{
		tf::Taskflow interpreter;
		interpreter.emplace([] {});
		Concurrency::RunTaskflow(interpreter);
	});
	caught = false;
	try { Concurrency::RunMaintenanceTaskflow(graph); }
	catch(const std::logic_error &) { caught = true; }
	Check(caught);
}

static void TestResize()
{
	Concurrency::SetMaxNumThreads(1);
	Check(Concurrency::GetExecutionThreadCount() == 1);
	tf::Taskflow graph;
	graph.emplace([](tf::Runtime &runtime)
	{
		Concurrency::RunInterpreterRuntime(runtime, []
		{
			Check(Concurrency::GetExecutionThreadCount() == 1);
			Concurrency::SetMaxNumThreads(3);
			Check(Concurrency::GetMaxNumThreads() == 3);
			Check(Nested(4, 3) == 64);
			Check(Concurrency::GetExecutionThreadCount() == 1);
		});
	});
	Concurrency::RunTaskflow(graph);
	Check(Concurrency::GetExecutionThreadCount() == 1);
	graph.clear();
	graph.emplace([] { Check(Concurrency::GetExecutionThreadCount() == 3); });
	Concurrency::RunTaskflow(graph);

	//Swap while a graph is executing. The retired generation is released on its
	//external caller, never on the worker that requested the resize.
	std::atomic<bool> started{false}, release{false};
	std::exception_ptr failure;
	std::thread caller([&]
	{
		try
		{
			tf::Taskflow old;
			old.emplace([&](tf::Runtime &runtime)
			{
				Concurrency::RunInterpreterRuntime(runtime, [&]
				{
					started = true;
					started.notify_one();
					release.wait(false);
					Check(Concurrency::GetExecutionThreadCount() == 3);
					Check(Nested(2, 4) == 16);
				});
			});
			Concurrency::RunTaskflow(old);
		}
		catch(...) { failure = std::current_exception(); }
	});
	started.wait(false);
	Concurrency::SetMaxNumThreads(2);
	graph.clear();
	graph.emplace([] { Check(Concurrency::GetExecutionThreadCount() == 2); });
	try { Concurrency::RunTaskflow(graph); }
	catch(...) { release = true; release.notify_one(); caller.join(); throw; }
	release = true;
	release.notify_one();
	caller.join();
	if(failure) std::rethrow_exception(failure);

	Concurrency::SetMaxNumThreads(0);
	Check(Concurrency::GetMaxNumThreads() >= 1);
	const size_t before = Concurrency::GetMaxNumThreads();
	bool caught = false;
	try { Concurrency::SetMaxNumThreads(std::numeric_limits<size_t>::max()); }
	catch(const std::invalid_argument &) { caught = true; }
	Check(caught && Concurrency::GetMaxNumThreads() == before);
	Concurrency::SetMaxNumThreads(1);
	Check(Nested(2, 3) == 8);
}

//All workers hold locks at once. Each can drain its own descendants without
//helping unrelated writers. Nested root submission remains forbidden.
static void TestNestedLockSafety(size_t threads)
{
	Concurrency::SetMaxNumThreads(threads);
	Check(Concurrency::CanRunInterpreterConcurrently());
	std::barrier ready(static_cast<std::ptrdiff_t>(threads));
	std::vector<std::mutex> locks(threads);
	std::vector<size_t> results(threads);
	std::atomic<size_t> forbidden{0};
	tf::Taskflow graph;
	for(size_t i = 0; i < threads; ++i)
		graph.emplace([&, i](tf::Runtime &runtime)
		{
			Concurrency::RunInterpreterRuntime(runtime, [&]
			{
				std::lock_guard lock(locks[i]);
				ready.arrive_and_wait();
				Check(Concurrency::CanRunInterpreterConcurrently());
				const size_t active = Concurrency::GetActiveInterpreterThreadCount();
				//Keep every worker active through the sample; assert after the barrier
				//so a failure cannot strand another participant.
				ready.arrive_and_wait();
				Check(active == threads);
				tf::Taskflow child;
				child.emplace([&] { ++forbidden; });
				bool rejected = false;
				try { Concurrency::RunTaskflow(child); }
				catch(const std::logic_error &) { rejected = true; }
				Check(rejected);
				results[i] = Nested(4, 5);
			});
		});
	Concurrency::RunTaskflow(graph);
	Check(forbidden == 0);
	for(auto value : results) Check(value == 1024);
	Check(Concurrency::GetActiveInterpreterThreadCount() == 1);
	Check(Concurrency::GetActiveThreadCount() == 1);

	//An unrelated writer queued with a holder cannot be stolen by a nested join.
	graph.clear();
	graph.emplace([&](tf::Runtime &runtime)
	{
		Concurrency::RunInterpreterRuntime(runtime, [&]
		{
			std::lock_guard lock(locks[0]);
			Check(Nested(3, 5) == 243);
		});
	});
	graph.emplace([&] { std::lock_guard lock(locks[0]); });
	Concurrency::RunTaskflow(graph);

	//A lone task gets an allowance of one even in a four-worker generation.
	graph.clear();
	graph.emplace([] { Check(Concurrency::GetActiveInterpreterThreadCount() == 1); });
	Concurrency::RunTaskflow(graph);
}

static void TestMaintenanceIsolation()
{
	Concurrency::SetMaxNumThreads(1);
	tf::Taskflow graph;
	std::mutex mutex;
	bool visible = false;
	graph.emplace([&]
	{
		std::lock_guard lock(mutex);
		tf::Taskflow children;
		children.emplace([&]
		{
			Check(Concurrency::GetActiveThreadCount() == 1);
			visible = true;
		});
		Concurrency::RunMaintenanceTaskflow(children);
		Check(visible);
	});
	graph.emplace([&] { std::lock_guard lock(mutex); });
	Concurrency::RunTaskflow(graph);
	Check(Nested(3, 4, true) == 81);
	std::vector<size_t> values(1000, 7), results(1000);
	IterateOverConcurrentlyIfPossible(values, [&](size_t i, size_t value) { results[i] = value; }, true);
	Check(results == values);
}

int main(int argc, char **argv)
{
	try
	{
		const auto start = std::chrono::steady_clock::now();
		if(argc > 1 && std::string_view(argv[1]) == "--stress")
		{
			Concurrency::SetMaxNumThreads(4);
			TestGraph(500000, 2);
			std::cout << "500000-task stress passed (two executions)\n";
		}
		else
		{
			for(size_t threads : {1, 2, 4})
			{
				Concurrency::SetMaxNumThreads(threads);
				for(size_t count : {size_t{0}, size_t{1}, threads, 2 * threads, 3 * threads, 4 * threads})
				{
					TestGraph(count, 3);
					TestCoarse(count);
				}
				TestGraph(20000, 3);
				for(size_t width : {1, 2, 4})
					for(size_t depth : {1, 3, 5})
					{
						size_t expected = 1;
						for(size_t i = 0; i < depth; ++i) expected *= width;
						Check(Nested(width, depth) == expected);
					}
				TestExceptions();
				TestSubmissionException();
				TestNestedLockSafety(threads);
			}
			TestMaintenanceIsolation();
			TestResize();
			std::cout << "Concurrency probes passed\n";
		}
		std::cout << "Seconds: " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << '\n';
	}
	catch(const std::exception &e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}
}
