#include "AssetManager.h"
#include "Concurrency.h"
#include "Interpreter.h"
#include "InterpreterConcurrencyManager.h"
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

//With one worker the second print runs after the first child's completion.
//The old child pop therefore deterministically changed these flags too early.
class ConstructionTargetListener : public PrintListener
{
public:
	explicit ConstructionTargetListener(EvaluableNodeReference &target) : target(target) {}
	void LogPrint(std::string &) override
	{
		Check(target.uniqueUnreferencedTopNode);
		Check(!target->GetNeedCycleCheck());
		++calls;
	}
	EvaluableNodeReference &target;
	std::atomic<size_t> calls{0};
};

static void TestConstructionTarget()
{
	Entity entity;
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
	auto &enm = entity.evaluableNodeManager;
	auto [code, warnings, offset, complete] = Parser::Parse("(seq (print \"target\") 42)", &enm);
	Check(code != nullptr && warnings.empty());
	enm.SetRootNode(code);
	EvaluableNodeReference target(enm.AllocNode(ENT_LIST), true);
	target->SetNeedCycleCheck(false);
	target->SetIsFreeableTopNode(false);
	ConstructionTargetListener listener(target);
	Interpreter parent(&enm, RandomStream("target-regression"), nullptr, &listener, nullptr, &entity, nullptr);
	parent.memoryModificationLock = enm.AcquireMemoryModificationReadLock();
	auto roots = parent.CreateOpcodeStackStateSaver(code);
	roots.PushEvaluableNode(target);
	enm.AddActiveInterpreter(&parent);
	std::vector<EvaluableNode *> results(32);
	{
		InterpreterConcurrencyManager group(&parent, results.size());
		for(size_t i = 0; i < results.size(); ++i)
			group.AddTaskWithConstructionStack(code, nullptr, &target,
				EvaluableNodeImmediateValueWithType(static_cast<double>(i)), nullptr, results[i]);
		group.EndConcurrency();
		Check(listener.calls == results.size());
		Check(!target.uniqueUnreferencedTopNode && target->GetNeedCycleCheck());
		group.UpdateResultEvaluableNodePropertiesBasedOnNewChildNodes(target);
		Check(!target.uniqueUnreferencedTopNode);
		for(auto result : results) Check(EvaluableNode::ToNumber(result) == 42);
	}
	enm.RemoveActiveInterpreter(&parent);
}

class RootedInterpreter : public Interpreter
{
public:
	using Interpreter::Interpreter;
	void AddRoots(EvaluableNode *root, EvaluableNode *assoc, EvaluableNodeReference &target)
	{
		scopeStack.push_back(root);
		opcodeStackNodes.push_back(assoc);
		constructionStack.emplace_back(root, &target,
			EvaluableNodeImmediateValueWithType(0.0), assoc, EvaluableNodeReference(root, false));
	}
};

//Every root overlaps a large cyclic graph, including extended ordered/assoc
//storage whose layout reads share the byte modified by parallel marking.
static void TestSharedMarkingAndCollectorElection(size_t workers)
{
	Entity entity;
	auto &enm = entity.evaluableNodeManager;
	auto root = enm.AllocNode(ENT_LIST);
	root->SetAnnotationsString("extended-root");
	enm.SetRootNode(root);
	for(size_t i = 0; i < 12000; ++i)
		root->AppendOrderedChildNode(enm.AllocNode(static_cast<double>(i)));
	auto assoc = enm.AllocNode(ENT_ASSOC);
	assoc->SetAnnotationsString("extended-assoc");
	assoc->SetMappedChildNode("cycle", root);
	root->AppendOrderedChildNode(assoc);
	root->SetNeedCycleCheck(true);
	assoc->SetNeedCycleCheck(true);

	std::vector<std::unique_ptr<RootedInterpreter>> interpreters;
	EvaluableNodeReference target(root, false);
	for(size_t i = 0; i < workers; ++i)
	{
		auto interpreter = std::make_unique<RootedInterpreter>(&enm, RandomStream("gc-regression"),
			nullptr, nullptr, nullptr, &entity, nullptr);
		interpreter->AddRoots(root, assoc, target);
		enm.AddActiveInterpreter(interpreter.get());
		interpreters.push_back(std::move(interpreter));
	}
	for(size_t repeat = 0; repeat < 12; ++repeat)
	{
		for(size_t i = 0; i < 12000; ++i) enm.AllocNode(-1.0);
		enm.UpdateGarbageCollectionTriggerForImmediateCollection();
		std::barrier ready(static_cast<std::ptrdiff_t>(workers));
		std::vector<std::thread> collectors;
		for(size_t i = 0; i < workers; ++i)
			collectors.emplace_back([&]
			{
				auto lock = enm.AcquireMemoryModificationReadLock();
				ready.arrive_and_wait();
				//Forced requests also write the threshold under shared access.
				enm.UpdateGarbageCollectionTriggerForImmediateCollection();
				enm.CollectGarbageWithConcurrentAccess(lock);
				Check(lock.owns_lock());
			});
		//Join before checking marks and object lifetimes.
		for(auto &collector : collectors) collector.join();
		Check(!enm.RecommendGarbageCollection());
		auto &children = root->GetOrderedChildNodesReference();
		Check(children.size() == 12001 && children.back() == assoc);
		Check(*assoc->GetMappedChildNode("cycle") == root);
		Check(!root->GetKnownToBeInUse() && !assoc->GetKnownToBeInUse());
		for(size_t i = 0; i < 12000; ++i)
		{
			Check(EvaluableNode::ToNumber(children[i]) == static_cast<double>(i));
			Check(!children[i]->GetKnownToBeInUse());
		}
	}
	for(auto &interpreter : interpreters) enm.RemoveActiveInterpreter(interpreter.get());
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
			TestConstructionTarget();
			TestSharedMarkingAndCollectorElection(workers);
		}
		std::cout << "Recursive Interpreter overlap, graph dependencies, saturation, exceptions, shared targets and GC passed; "
			"20 repetitions at each of 1/2/4 workers\n";
	}
	catch(const std::exception &e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}
}
