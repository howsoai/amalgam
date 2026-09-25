#include "AssetManager.h"
#include "Concurrency.h"
#include "Interpreter.h"
#include "Parser.h"

#include <atomic>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <set>
#include <source_location>
#include <stdexcept>
#include <thread>

static void Check(bool condition, std::source_location location = std::source_location::current())
{
	if(!condition)
		throw std::runtime_error(std::string("Interpreter concurrency check failed at ")
			+ location.file_name() + ":" + std::to_string(location.line()));
}

//The normal print opcode reaches this listener from actual Interpreter children.
//No scheduler or opcode implementation is replaced in this test.
class LeafListener : public PrintListener
{
public:
	explicit LeafListener(bool overlap) : requireOverlap(overlap) {}

	void LogPrint(std::string &value) override
	{
		Check(value == "leaf");
		std::unique_lock lock(mutex);
		++entered;
		++active;
		peak = std::max(peak, active);
		threads.insert(std::this_thread::get_id());
		condition.notify_all();
		bool reached = !requireOverlap || condition.wait_for(lock, std::chrono::seconds(5),
			[&] { return entered >= 2; });
		--active;
		++completed;
		condition.notify_all();
		Check(reached);
	}

	void CheckFinished()
	{
		std::lock_guard lock(mutex);
		Check(entered == 2 && completed == 2 && active == 0);
		if(requireOverlap) Check(peak == 2 && threads.size() == 2);
	}

private:
	bool requireOverlap;
	std::mutex mutex;
	std::condition_variable condition;
	size_t entered = 0, completed = 0, active = 0, peak = 0;
	std::set<std::thread::id> threads;
};

static double Evaluate(std::string_view source, PrintListener *listener = nullptr)
{
	Entity entity;
	auto &enm = entity.evaluableNodeManager;
	asset_manager.SetEntityPermissions(&entity, ExecutionPermissions::AllPermissions(),
		ExecutionPermissions::AllPermissions());
	struct ClearPermissions
	{
		Entity *entity;
		~ClearPermissions()
		{
			asset_manager.SetEntityPermissions(entity, ExecutionPermissions::AllPermissions(), ExecutionPermissions());
		}
	} clear_permissions{&entity};
	auto [code, warnings, offset, complete] = Parser::Parse(source, &enm);
	Check(code != nullptr && warnings.empty());
	enm.SetRootNode(code);
	Interpreter interpreter(&enm, RandomStream("nested-regression"), nullptr, listener, nullptr, &entity, nullptr);
	interpreter.memoryModificationLock = enm.AcquireMemoryModificationReadLock();
	auto result = interpreter.ExecuteNode(code);
	return EvaluableNode::ToNumber(result);
}

//Each recursive call creates a concurrent Interpreter child. At the base case,
//two children of the SAME nested opcode must be inside the listener together.
static constexpr std::string_view recursive_source = R"(
(let {f (lambda (if (= n 0)
                  ||(+ (seq (print "leaf") 20) (seq (print "leaf") 22))
                  ||(+ (call f {n (- n 1)}) 1)))}
  (call f {n 4}))
)";

static void TestOverlap(size_t workers)
{
	Concurrency::SetMaxNumThreads(workers);
	LeafListener listener(workers > 1);
	size_t before = 0, after = 0;
	double result = 0;
	tf::Taskflow graph;
	auto predecessor = graph.emplace([&] { before = 1; });
	auto parent = graph.emplace([&](tf::Runtime &runtime)
	{
		Concurrency::RunInterpreterRuntime(runtime, [&]
		{
			Check(before == 1);
			result = Evaluate(recursive_source, &listener);
			//The synchronous opcode continuation must see both completed leaves.
			listener.CheckFinished();
		});
	});
	auto successor = graph.emplace([&]
	{
		listener.CheckFinished();
		Check(result == 46);
		after = 1;
	});
	predecessor.precede(parent);
	parent.precede(successor);
	Concurrency::RunTaskflow(graph);
	Check(after == 1);
}

static void TestSaturated(size_t workers)
{
	Concurrency::SetMaxNumThreads(workers);
	std::barrier ready(static_cast<std::ptrdiff_t>(workers));
	std::vector<std::mutex> locks(workers);
	std::vector<double> results(workers);
	std::atomic<size_t> writers{0};
	tf::Taskflow graph;
	auto successor = graph.emplace([&]
	{
		Check(writers == workers);
		for(double result : results) Check(result == 512);
	});
	for(size_t i = 0; i < workers; ++i)
		graph.emplace([&, i](tf::Runtime &runtime)
		{
			Concurrency::RunInterpreterRuntime(runtime, [&]
			{
				std::lock_guard lock(locks[i]);
				ready.arrive_and_wait();
				//Queued unrelated work must never run on this lock-holding stack.
				//Runtime anchoring also makes the graph successor wait for writers.
				runtime.silent_async([&, i]
				{
					std::lock_guard writer_lock(locks[i]);
					++writers;
				});
				results[i] = Evaluate(R"(
(apply "+" ||(map (lambda
  (apply "+" ||(map (lambda
    (apply "+" ||(range (lambda 1) 0 7 1))) (range 0 7)))) (range 0 7)))
)");
			});
		}).precede(successor);
	Concurrency::RunTaskflow(graph);
}

class ThrowingListener : public PrintListener
{
public:
	void LogPrint(std::string &) override { throw std::runtime_error("leaf exception"); }
};

static void TestException()
{
	ThrowingListener listener;
	bool caught = false;
	try { Evaluate(recursive_source, &listener); }
	catch(const std::runtime_error &e) { caught = std::string_view(e.what()) == "leaf exception"; }
	Check(caught);
	//The next invocation must not inherit a dead runtime, registration or lock.
	LeafListener recovery(false);
	Check(Evaluate(recursive_source, &recovery) == 46);
	recovery.CheckFinished();
}

int main()
{
	try
	{
		for(size_t workers : {1, 2, 4})
		{
			for(size_t repeat = 0; repeat < 20; ++repeat)
			{
				TestOverlap(workers);
				TestSaturated(workers);
			}
			TestException();
		}
		std::cout << "Recursive Interpreter overlap, graph dependencies, saturation and exceptions passed; "
			"20 repetitions at each of 1/2/4 workers\n";
	}
	catch(const std::exception &e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}
}
