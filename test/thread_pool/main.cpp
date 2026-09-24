#include <cstddef>
#include <cstdint>
#include "ThreadPool.h"

#include <cstdlib>
#include <iostream>
#include <latch>
#include <string_view>

// Checks remain enabled in release builds. CTest bounds the entire process,
// including joins/destructors, so a regression cannot hang the test runner.
void Check(bool condition, const char *message)
{
	if(!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

void QueueTransitions()
{
	MultiProducerMultiConsumerQueue<size_t, 8> queue;
	for(size_t cycle = 0; cycle < 100; ++cycle)
	{
		for(size_t poll = 0; poll < 32; ++poll)
			Check(!queue.pop(), "empty pop");
		Check(queue.empty(), "empty polls must not move the consumer cursor");
		for(size_t i = 0; i < 257; ++i)
			queue.push(size_t(i));
		Check(queue.size() == 257, "published queue size");
		for(size_t i = 0; i < 257; ++i)
		{
			auto value = queue.pop();
			Check(value && *value == i, "FIFO across full block boundaries");
		}
		Check(queue.empty(), "drained queue");
	}
}

struct PausedMove
{
	int value = -1;
	std::latch *entered = nullptr;
	std::latch *release = nullptr;
	PausedMove() = default;
	PausedMove(int v, std::latch *e = nullptr, std::latch *r = nullptr)
		: value(v), entered(e), release(r) {}
	PausedMove(PausedMove &&other) noexcept { *this = std::move(other); }
	PausedMove &operator=(PausedMove &&other) noexcept
	{
		value = other.value;
		if(other.entered)
		{
			other.entered->count_down();
			other.release->wait();
			other.entered = nullptr;
		}
		return *this;
	}
};

void Publication()
{
	MultiProducerMultiConsumerQueue<PausedMove, 2> queue;
	std::latch entered(1), release(1), second_started(1);
	std::thread first([&] { queue.push(PausedMove(0, &entered, &release)); });
	entered.wait();
	std::thread second([&] {
		second_started.count_down();
		queue.push(PausedMove(1));
	});
	second_started.wait();
	// The first insertion is not published: pop must neither claim it nor
	// dispatch the later producer's item. In particular it must not block.
	Check(!queue.pop(), "unpublished front is unavailable");
	release.count_down();
	first.join();
	second.join();
	for(int i = 0; i < 2; ++i)
	{
		auto value = queue.pop();
		Check(value && value->value == i, "publication preserves insertion order");
	}
}

// Pause after a consumer claims an old slot while other consumers advance the
// head through many blocks. Its node must remain alive until the move finishes.
struct ConsumerPause
{
	int value;
	std::atomic<bool> *armed;
	std::latch *entered;
	std::latch *release;
	ConsumerPause(int v, std::atomic<bool> *a = nullptr, std::latch *e = nullptr, std::latch *r = nullptr)
		: value(v), armed(a), entered(e), release(r) {}
	ConsumerPause(ConsumerPause &&other) noexcept
		: value(other.value), armed(other.armed), entered(other.entered), release(other.release)
	{
		if(armed && armed->exchange(false))
		{
			entered->count_down();
			release->wait();
			Check(other.value == value, "claimed node remains alive during move");
		}
	}
};

void StalledConsumer()
{
	MultiProducerMultiConsumerQueue<ConsumerPause, 2> queue;
	std::atomic<bool> armed{false};
	std::latch entered(1), release(1);
	queue.push(ConsumerPause(0, &armed, &entered, &release));
	armed = true;
	std::thread stalled([&] {
		auto value = queue.pop();
		Check(value && value->value == 0, "stalled consumer owns its original ticket");
	});
	entered.wait();
	for(int i = 1; i < 4000; ++i)
	{
		queue.push(ConsumerPause(i));
		auto value = queue.pop();
		Check(value && value->value == i, "other consumers progress during stalled move");
	}
	release.count_down();
	stalled.join();
	for(int i = 4000; i < 4010; ++i)
	{
		queue.push(ConsumerPause(i));
		auto value = queue.pop();
		Check(value && value->value == i, "reclamation after stalled reader leaves");
	}
}

void QueueConcurrent()
{
	constexpr size_t producers = 4, per_producer = 5000, total = producers * per_producer;
	MultiProducerMultiConsumerQueue<size_t, 8> queue;
	std::vector<std::atomic<unsigned>> seen(total);
	std::atomic<size_t> consumed{0};
	std::latch start(producers + 4);
	std::vector<std::thread> threads;
	for(size_t p = 0; p < producers; ++p)
		threads.emplace_back([&, p] {
			start.arrive_and_wait();
			for(size_t i = 0; i < per_producer; ++i)
				queue.push(size_t(p * per_producer + i));
		});
	for(size_t c = 0; c < 4; ++c)
		threads.emplace_back([&] {
			start.arrive_and_wait();
			while(consumed.load() < total)
			{
				if(auto value = queue.pop())
				{
					Check(*value < total, "valid MPMC item");
					Check(seen[*value].fetch_add(1) == 0, "MPMC exactly once");
					++consumed;
				}
				else
					std::this_thread::yield();
			}
		});
	for(auto &thread : threads)
		thread.join();
	for(auto &count : seen)
		Check(count == 1, "all MPMC items consumed");
	Check(queue.empty(), "MPMC drained");

	// A single consumer can observe dispatch order without confusing it with
	// the scheduling/completion order of multiple consumers.
	std::array<size_t, producers> next{};
	threads.clear();
	for(size_t p = 0; p < producers; ++p)
		threads.emplace_back([&, p] {
			for(size_t i = 0; i < per_producer; ++i)
				queue.push(size_t(p * per_producer + i));
		});
	for(size_t i = 0; i < total;)
		if(auto value = queue.pop())
		{
			Check(*value % per_producer == next[*value / per_producer]++, "producer FIFO");
			++i;
		}
	for(auto &thread : threads)
		thread.join();
}

class ObservablePool : public ThreadPool
{
public:
	using ThreadPool::ThreadPool;
	void WaitUntilIdle()
	{
		for(;;)
		{
			{
				auto lock = AcquireTaskLock();
				if(numActiveThreads == 1 && taskQueue.empty() && numThreadsToTransitionToReserved == 0)
					return;
			}
			std::this_thread::yield();
		}
	}
};

void PoolLifecycle()
{
	for(size_t repetition = 0; repetition < 12; ++repetition)
	{
		ObservablePool pool(4);
		for(size_t cycle = 0; cycle < 30; ++cycle)
		{
			pool.WaitUntilIdle();
			std::latch completed(1);
			pool.EnqueueTask([&] { completed.count_down(); });
			completed.wait();
			pool.WaitUntilIdle();
			auto lock = pool.AcquireTaskLock();
			Check(pool.AreThreadsAvailable(), "idle pool has capacity");
			auto tasks = pool.CreateCountableTaskSet(32);
			std::atomic<unsigned> count{0};
			for(size_t i = 0; i < 32; ++i)
				pool.BatchEnqueueTask([&] { ++count; tasks.MarkTaskCompleted(); });
			tasks.WaitForTasks(&lock);
			Check(count == 32, "serialized batch completed");
		}
		pool.WaitUntilIdle();
		pool.SetMaxNumActiveThreads(2);
		pool.WaitUntilIdle();
		pool.SetMaxNumActiveThreads(5);
		pool.WaitUntilIdle();
		// Nested waits create replacement/reserved workers and exercise resize
		// after those transitions, rather than only an idle fixed-size pool.
		auto outer = pool.CreateCountableTaskSet(12);
		for(size_t i = 0; i < 12; ++i)
			pool.EnqueueTask([&] {
				auto lock = pool.AcquireTaskLock();
				if(!pool.AreThreadsAvailable())
				{
					lock.unlock();
					outer.MarkTaskCompleted();
					return;
				}
				auto inner = pool.CreateCountableTaskSet(3);
				for(size_t j = 0; j < 3; ++j)
					pool.BatchEnqueueTask([&] { inner.MarkTaskCompleted(); });
				inner.WaitForTasks(&lock);
				outer.MarkTaskCompleted();
			});
		outer.WaitForTasks();
		pool.WaitUntilIdle();
		pool.SetMaxNumActiveThreads(2);
		pool.WaitUntilIdle();
	}
	// One worker makes actual dispatch order observable.
	ObservablePool fifo(2);
	std::latch done(2000);
	size_t next = 0;
	for(size_t i = 0; i < 2000; ++i)
		fifo.EnqueueTask([&, i] { Check(next++ == i, "pool FIFO dispatch"); done.count_down(); });
	done.wait();
	fifo.WaitUntilIdle();
}

void PoolProducers()
{
	constexpr size_t total = 16000;
	ObservablePool pool(5);
	std::vector<std::atomic<unsigned>> seen(total);
	auto tasks = pool.CreateCountableTaskSet(total);
	std::vector<std::thread> producers;
	for(size_t p = 0; p < 4; ++p)
		producers.emplace_back([&, p] {
			for(size_t i = p; i < total; i += 4)
			{
				auto task = [&, i] {
					Check(seen[i].fetch_add(1) == 0, "pool producer exactly once");
					tasks.MarkTaskCompleted();
				};
				if(p % 2)
					pool.EnqueueTask(std::move(task));
				else
					pool.BatchEnqueueTask(std::move(task)); // AddLabels calling convention
			}
		});
	for(auto &producer : producers)
		producer.join();
	tasks.WaitForTasks();
	for(auto &count : seen)
		Check(count == 1, "all pool producer tasks completed");
	pool.WaitUntilIdle();
	// Independent AddLabels-style callers each enqueue and wait on their own
	// set, without participating in the serialized availability protocol.
	producers.clear();
	for(size_t p = 0; p < 4; ++p)
		producers.emplace_back([&] {
			for(size_t cycle = 0; cycle < 20; ++cycle)
			{
				auto independent = pool.CreateCountableTaskSet(64);
				for(size_t i = 0; i < 64; ++i)
					pool.BatchEnqueueTask([&] { independent.MarkTaskCompleted(); });
				independent.WaitForTasks();
			}
		});
	for(auto &producer : producers)
		producer.join();
	pool.WaitUntilIdle();

	// An entirely unnotified batch must also awaken sleeping workers at wait.
	auto batch = pool.CreateCountableTaskSet(20);
	for(size_t i = 0; i < 20; ++i)
		pool.BatchEnqueueTask([&] { batch.MarkTaskCompleted(); });
	batch.WaitForTasks();
}

struct Lifetime
{
	std::atomic<int> &live;
	std::atomic<int> &executed;
	Lifetime(std::atomic<int> &l, std::atomic<int> &e) : live(l), executed(e) { ++live; }
	Lifetime(Lifetime &&other) noexcept : live(other.live), executed(other.executed) { ++live; }
	~Lifetime() { --live; }
	void operator()() { ++executed; }
};

void TaskLifetime()
{
	std::atomic<int> live{0}, executed{0};
	{
		ObservablePool pool(4);
		for(int i = 0; i < 1000; ++i)
		{
			pool.EnqueueTask(Lifetime(live, executed));
			pool.EnqueueTask([payload = std::make_unique<int>(42), &executed] {
				Check(*payload == 42, "move-only task"); ++executed;
			});
			pool.EnqueueTask([large = std::array<int, 256>{}, &executed] {
				Check(large[0] == 0, "heap task"); ++executed;
			});
		}
		// Destruction must join workers even while they are draining tasks.
	}
	Check(live == 0 && executed == 3000, "task lifetime and shutdown drain");
	{
		ThreadPool no_workers(1);
		no_workers.BatchEnqueueTask(Lifetime(live, executed));
	}
	Check(live == 0 && executed == 3000, "pending task destruction");
}

int main(int argc, char **argv)
{
	const std::string_view selection = argc > 1 ? argv[1] : "all";
	const auto run = [&](std::string_view name, auto test) {
		if(selection == "all" || selection == name)
		{
			std::cout << "Running " << name << std::endl;
			test();
			std::cout << "Passed " << name << std::endl;
		}
	};
	run("queue", QueueTransitions);
	run("publication", Publication);
	run("mpmc", QueueConcurrent);
	run("reclamation", StalledConsumer);
	run("pool", PoolLifecycle);
	run("producers", PoolProducers);
	run("lifetime", TaskLifetime);
}
