#include "Concurrency.h"

#include <array>
#include <barrier>
#include <cstdio>
#include <cstdlib>
#include <source_location>

//GCC diagnoses the malloc/free implementation of replacement new/delete after inlining.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

//Thread-local instrumentation excludes allocations by other Taskflow workers.
//The measured callbacks themselves neither allocate nor throw.
namespace
{
thread_local bool measure = false;
thread_local size_t allocations = 0, bytes = 0;
thread_local int fail_after = -1;

void Check(bool condition, std::source_location where = std::source_location::current())
{
	if(!condition)
	{
		std::fprintf(stderr, "Failed at %s:%u\n", where.file_name(), where.line());
		std::abort();
	}
}
}

void *operator new(size_t size)
{
	if(fail_after >= 0 && fail_after-- == 0)
		throw std::bad_alloc();
	if(measure)
	{
		++allocations;
		bytes += size;
	}
	if(void *p = std::malloc(size ? size : 1))
		return p;
	throw std::bad_alloc();
}
void *operator new[](size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }

using Task = Concurrency::FixedSizeTask;
static_assert(!std::is_copy_constructible_v<Task>);
static_assert(std::is_nothrow_move_constructible_v<Task>);
static_assert(std::is_nothrow_move_assignable_v<Task>);

//Move-only ownership token, including nontrivial moves and destruction, without
//allocating a resource just to test ownership of that resource.
struct Token
{
	std::atomic<size_t> *live;
	explicit Token(std::atomic<size_t> &count) : live(&count) { ++*live; }
	Token(Token &&other) noexcept : live(std::exchange(other.live, nullptr)) {}
	~Token() { if(live) --*live; }
};

static void TestStorage()
{
	std::vector<Task> tasks;
	tasks.reserve(4096);
	std::atomic<size_t> live{0};
	size_t calls = 0;
	allocations = bytes = 0;
	measure = true;
	for(size_t i = 0; i < 4096; ++i)
		tasks.emplace_back([token = Token(live), &calls] { ++calls; });
	Check(live == tasks.size());
	Task moved(std::move(tasks[0]));
	tasks[0] = std::move(moved);
	for(auto &task : tasks)
	{
		task();
		task.Reset();
		task.Reset();
	}
	tasks.clear();
	measure = false;
	Check(live == 0 && calls == 4096 && allocations == 0);
	//Exercise the exact capacity, and a throwing initial copy (moves are noexcept).
	struct FullCapture
	{
		std::array<std::byte, Task::INLINE_SIZE> data{};
		void operator()() {}
	};
	Task full{FullCapture{}};
	full();
	struct ThrowingCopy
	{
		ThrowingCopy() = default;
		ThrowingCopy(const ThrowingCopy &) { throw 42; }
		ThrowingCopy(ThrowingCopy &&) noexcept = default;
		void operator()() {}
	} source;
	bool caught = false;
	try { tasks.emplace_back(source); }
	catch(int value) { caught = value == 42; }
	Check(caught && tasks.empty());

	std::printf("inline storage: %zu bytes/task, %zu allocations for 4096 tasks after reserve\n",
		sizeof(Task), allocations);
}

static size_t Nested(size_t depth)
{
	if(depth == 0) return 1;
	std::array<size_t, 3> results{};
	std::vector<Task> tasks;
	tasks.reserve(results.size());
	for(size_t i = 0; i < results.size(); ++i)
		tasks.emplace_back([&, i] { results[i] = Nested(depth - 1); });
	Concurrency::RunInterpreterTasks(std::move(tasks));
	return results[0] + results[1] + results[2];
}

//Short outer siblings must release their workers to run the innermost siblings
//while the submitting worker remains inside the synchronous Interpreter stack.
static void NestedOverlap(size_t depth, std::barrier<> &leaves,
	std::array<std::thread::id, 2> &threads)
{
	std::vector<Task> tasks;
	tasks.reserve(2);
	for(size_t i = 0; i < 2; ++i)
		tasks.emplace_back([&, i]
		{
			if(depth == 0)
			{
				threads[i] = std::this_thread::get_id();
				leaves.arrive_and_wait();
			}
			else if(i == 0)
				NestedOverlap(depth - 1, leaves, threads);
		});
	Concurrency::RunInterpreterTasks(std::move(tasks));
}

static void TestRuntime(size_t workers)
{
	Concurrency::SetMaxNumThreads(workers);
	auto generation = Concurrency::AcquireGeneration();
	std::barrier start(static_cast<std::ptrdiff_t>(workers));
	std::atomic<size_t> finished{0};
	tf::Taskflow graph;
	auto successor = graph.emplace([&] { Check(finished == workers); });
	for(size_t i = 0; i < workers; ++i)
		graph.emplace([&](tf::Runtime &runtime)
		{
			Concurrency::RunInterpreterRuntime(runtime, [&]
			{
				start.arrive_and_wait();
				Check(Nested(5) == 243);
				++finished;
			});
		}).precede(successor);
	for(size_t repeat = 0; repeat < 20; ++repeat)
	{
		finished = 0;
		generation->interpreter.run(graph).get();
	}

	if(workers > 1)
	{
		graph.clear();
		std::barrier leaves(2);
		std::array<std::thread::id, 2> threads;
		graph.emplace([&](tf::Runtime &runtime)
		{
			Concurrency::RunInterpreterRuntime(runtime, [&] { NestedOverlap(4, leaves, threads); });
		});
		generation->interpreter.run(graph).get();
		Check(threads[0] != threads[1]);
	}

	//Queue actual unrelated writers while all workers hold their locks.
	std::vector<std::mutex> locks(workers);
	graph.clear();
	for(size_t i = 0; i < workers; ++i)
		graph.emplace([&, i](tf::Runtime &runtime)
		{
			Concurrency::RunInterpreterRuntime(runtime, [&]
			{
				std::lock_guard lock(locks[i]);
				start.arrive_and_wait();
				runtime.silent_async([&, i] { std::lock_guard writer(locks[i]); });
				Check(Nested(4) == 81);
			});
		});
	generation->interpreter.run(graph).get();

	//Submission-order exception selection, closure destruction, TLS restoration,
	//and successful reuse of the same runtime after catching the child failure.
	graph.clear();
	graph.emplace([&](tf::Runtime &runtime)
	{
		Concurrency::RunInterpreterRuntime(runtime, [&]
		{
			std::atomic<size_t> live{0};
			std::vector<Task> tasks;
			tasks.reserve(2);
			std::barrier failures(static_cast<std::ptrdiff_t>(std::min<size_t>(workers, 2)));
			for(int i = 0; i < 2; ++i)
				tasks.emplace_back([&, i, token = Token(live)] { failures.arrive_and_wait(); throw i; });
			bool caught = false;
			try { Concurrency::RunInterpreterTasks(std::move(tasks)); }
			catch(int i) { caught = (i == 0); }
			Check(caught && live == 0 && Concurrency::interpreter_runtime == &runtime);
			Check(Nested(3) == 27);
		});
		Check(Concurrency::interpreter_runtime == nullptr);
		try { Concurrency::RunInterpreterRuntime(runtime, [] { throw 42; }); }
		catch(int value) { Check(value == 42); }
		Check(Concurrency::interpreter_runtime == nullptr);
	});
	generation->interpreter.run(graph).get();

	//Fault injection covers group construction and each runtime publication.
	for(int failure = 0; failure < 12; ++failure)
	{
		graph.clear();
		graph.emplace([&](tf::Runtime &runtime)
		{
			Concurrency::RunInterpreterRuntime(runtime, [&]
			{
				std::atomic<size_t> live{0};
				std::vector<Task> tasks;
				tasks.reserve(8);
				for(size_t i = 0; i < 8; ++i)
					tasks.emplace_back([token = Token(live)] {});
				fail_after = failure;
				try { Concurrency::RunInterpreterTasks(std::move(tasks)); }
				catch(const std::bad_alloc &) {}
				fail_after = -1;
				Check(live == 0);
				Check(Nested(2) == 9);
			});
		});
		generation->interpreter.run(graph).get();
	}
	std::printf("runtime: %zu workers, nested overlap/saturation/late runners/exceptions/submission failures passed\n", workers);
}

static void TestDomains()
{
	Check(Nested(3) == 27); //External entry creates and joins one runtime root.
	auto generation = Concurrency::AcquireGeneration();
	std::atomic<size_t> rejected{0};
	tf::Taskflow graph;
	graph.emplace([&]
	{
		std::vector<Task> tasks;
		tasks.reserve(1);
		tasks.emplace_back([] {});
		try { Concurrency::RunInterpreterTasks(std::move(tasks)); }
		catch(const std::logic_error &) { ++rejected; }
	});
	generation->interpreter.run(graph).get();
	Concurrency::RunSystemTasks(graph);
	Check(rejected == 2);
}

static void TestAllocations(size_t workers)
{
	Concurrency::SetMaxNumThreads(workers);
	auto generation = Concurrency::AcquireGeneration();
	//Measure the only non-Taskflow shared allocation separately, with its vector
	//already reserved. Do not infer its size from implementation-specific layout.
	std::vector<Task> tasks;
	tasks.reserve(1);
	tasks.emplace_back([] {});
	allocations = bytes = 0;
	measure = true;
	auto group = std::make_shared<Concurrency::InterpreterTaskGroup>(std::move(tasks));
	measure = false;
#if defined(__linux__) && defined(__GLIBCXX__)
	Check(allocations == 1);
#else
	//Some library allocator paths bypass this probe's replacement global new.
	Check(allocations <= 1);
#endif
	std::printf("shared group including control block: %zu allocation, %zu bytes\n", allocations, bytes);
	group->Join();

	for(size_t count : {size_t{1}, size_t{4}, size_t{4096}})
	{
		tf::Taskflow graph;
		graph.emplace([&](tf::Runtime &runtime)
		{
			Concurrency::RunInterpreterRuntime(runtime, [&]
			{
				std::vector<Task> children;
				children.reserve(count);
				allocations = bytes = 0;
				measure = true;
				for(size_t i = 0; i < count; ++i)
					children.emplace_back([] {});
				Check(allocations == 0);
				Concurrency::RunInterpreterTasks(std::move(children));
				measure = false;
				if(workers == 1 || count == 1) Check(allocations == 0);
				std::printf("%zu workers, %zu tasks: %zu allocations, %zu bytes (includes Taskflow submission)\n",
					workers, count, allocations, bytes);
			});
		});
		generation->interpreter.run(graph).get();
	}
}

int main()
{
	TestStorage();
	for(size_t workers : {1, 2, 4})
	{
		TestRuntime(workers);
		TestAllocations(workers);
		TestDomains();
	}
}
