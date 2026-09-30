# Taskflow execution

Taskflow 4.1.0 is vendored under `src/3rd_party/taskflow`; builds are offline.
Maintenance call sites own `tf::Taskflow` graphs. Interpreter forks use runtime
child groups in a single root topology. Only external callers use `run(...).get()`;
no nested Interpreter submits another root graph. There are no detached tasks.

## Interpreter runtime and lock invariant

**A synchronous Interpreter join executes only its own unclaimed children.**
`InterpreterConcurrencyManager` collects child closures, then submits them through
`RunInterpreterTasks` at `EndConcurrency`. External entries create one runtime
root. Entries already executing in a graph use `RunInterpreterRuntime` to bind
that graph's runtime; nested forks inherit it. Bare worker submissions and
maintenance -> Interpreter submissions remain rejected.

Each group reserves its first child for the submitting worker and publishes at
most `min(children - 1, workers - 1)` Taskflow runtime runners before executing
child 0. Runners claim the remaining children in submission order using a
monotonic atomic index. The owner
executes its reserved child and drains unclaimed siblings. A short outer sibling
can return its worker to Taskflow while the first child recursively forks inner
work. Runtime runners can execute those inner children concurrently, even while
the outer Interpreter call remains active. Each child is executed exactly once;
there is no bounded queue capacity at which producers wait for consumers.
Tiny groups can therefore incur bounded wake/runner overhead even when the owner
finishes all children before a runner starts; such a late runner simply retires.

After draining its group, the caller sleeps using C++20 atomic wait/notify for
children already executing on other workers. It never waits for *unclaimed*
children in an executor queue. Inductively, a saturated worker can evaluate every
finite descendant tree itself, including at one worker, without replacement
threads, admission retries, polling, or arbitrary queue helping. As with ordinary
fork/join, child code must not block on an unstarted sibling or a resource retained
by its ancestor. Busy workers may cause a particular inner fork to run on one
worker; available runtime runners provide actual inner parallelism. A sleeping
ancestor does not steal another branch's work.

The group completion counter publishes all result slots before the synchronous
opcode continuation. Exceptions retain the lowest failing child index under a
mutex among tasks that began execution before observing cancellation. Unstarted
work may be skipped, so this is not unconditional submission-order failure
selection. All children are drained and joined before that failure is rethrown.
Failed runtime submissions roll back their anchor reference; already
published children are joined before caller captures can unwind. The manager
restores the memory lock and shared-scope state before propagation. Execution
registration is removed on both normal and exceptional exits.

Taskflow's runtime implicit anchor supplies the outer graph dependency: the
runtime task's successors cannot execute until its runners and their runtime
descendants retire. In `core/runtime.hpp`, `_invoke_runtime_task_impl` preempts
that graph node **after its callable returns** when its join counter is nonzero;
`core/async.hpp` reactivates it when its final child retires. No suspended C++
Interpreter stack is handed to the general scheduler. Late runners retain only a
shared group whose completed closures have been cleared; they cannot dereference
destroyed Interpreters, result slots or managers. External root completion also
waits for those runners, so generation retirement and shutdown remain safe.

The lock distinction matters:

- `Executor::_corun_until` steals from every worker and external queue. Both
  executor and runtime `corun` can invoke an unrelated writer on a stack holding
  the very entity/query lock that writer needs. Neither is used by Interpreter
  joins. Runtime `async` futures alone would instead deadlock at saturation.
- `move_entities` retains the source write reference while interpreting the
  destination. Query distance callbacks retain container and query-cache read
  locks. These remain held; descendants may compute under them, but unrelated
  queued writers run only after returning to Taskflow's scheduler on that worker.
- Before draining/waiting, `EndConcurrency` releases the parent's memory read
  lock. Child Interpreters acquire their own locks, and the registered parent
  opcode/result stacks remain GC roots. Scope mutexes protect shared ancestor
  contexts until the final child finishes. No entity or scope lock is silently
  unlocked and reacquired to accommodate scheduling.

Taskflow dependent async, subflows, modules and explicit preemption were also
considered. They express successor dependencies, but cannot suspend the middle of
an existing recursive synchronous opcode implementation. A joined subflow or
explicit runtime `corun` still enters unrestricted helping. Using implicit runtime
anchoring plus a descendant-only synchronous group keeps the opcode APIs and lock
ownership intact without requiring a coroutine conversion of every opcode.

Destroying a manager before submission discards its closures without starting
side effects. GC failures release waiters and discard partial marks as before.

Construction tasks write side effects into separate, preallocated records. They
borrow the parent's target reference without changing its uniqueness or node
flags when popping their copied construction entry. After joining every child,
the parent holds the memory read lock again and applies the existing construction
finalization rules. Target references, result slots and their registered GC roots
outlive that join, including when a child throws.

GC threshold checks and collector election both run under the memory read lock;
only then does a candidate release it. The election flag excludes a second
collector while the winner waits for exclusive access. The threshold itself is
atomic in multithreaded builds because forced-GC requests can write it alongside
readers; its default sequentially consistent accesses match the used-node counter.
Trigger recalculation uses one snapshot and publishes one final value. The exclusive lock freezes
node contents throughout marking and sweeping. In multithreaded builds, attribute
queries use atomic loads because immutable layout bits share a byte with
the mark bit. Relaxed loads and mark-claim RMWs suffice: they select who traverses
a node, without publishing its contents. Task submission publishes the frozen
graph and the maintenance join completes all traversals before sweeping or
ordinary mark resets. Initialization and other exclusive node writes remain
ordinary accesses; single-thread builds retain ordinary attribute reads.

## Maintenance and configuration

A process-wide generation has two executors. Interpreter roots use one;
maintenance graphs (GC marking/sweeping, cache columns, numeric queries) use the
other. Maintenance tasks must not execute Interpreter code or acquire locks held
by any synchronous caller. The current maintenance call sites operate on protected
memory, disjoint columns, or numeric result slots without acquiring those locks.
Only maintenance workers may use `corun`; Interpreter -> maintenance waits block
without helping. Distance callbacks execute on the calling Interpreter thread.
KnnCache snapshots callback capability in every ResetCache, before constructing
the density processor; no capability check dereferences a previous query's evaluator.

At one configured thread, marked Interpreter operations use the same runtime
groups and execute their children on the single worker. Maintenance loop call
sites retain their serial paths.
`SetMaxNumThreads(0)` selects hardware concurrency (at least one); OMP-only keeps
its half-core default. Counts beyond INT_MAX are ignored before changing
configuration; the void C API ignores them and the language rejects negative or
nonfinite counts. The next external submission creates a generation at the new
count. Existing work and maintenance descendants retain their old generation.
Each executor has that many workers; two domains can use twice the configured
count, and resizing can temporarily overlap generations. Only external synchronous
callers own generations; workers borrow them. Retired executors are joined outside
the state mutex and never destroyed on their own worker. Hosts must finish API
calls before unloading, as with other library state.

## Counters and verification

Allocation allowances retain the old sampled **active Interpreter count** policy,
not configured capacity. Taskflow observers count active tasks across generations;
synchronous group and maintenance waits are excluded. A serial caller counts itself, and the
factor is at least one. Idle workers do not inflate constrained allocations.
Numeric maintenance tasks are intentionally excluded because they do not allocate
Interpreter nodes; the old primary pool could include them in its sample.
The profiler retains all four elapsed-by-thread API/report sections, dividing by
the sampled active count across both domains. These remain estimates, not measured
wall time. Unlike the old two pools' permanently counted main threads, idle domains
contribute zero, so a serial operation divides by one rather than an artificial
two. Multiple external host threads are only counted when executing graph tasks
(or when the querying caller counts itself); this is not full host-thread tracking.
The debugger reports configured capacity and the sampled active Interpreter count.

`Concurrency.Storage` builds directly against the production header and vendored
Taskflow. It checks inline move-only closure lifetime, allocation counts after
reserve, nested overlap, saturation and late runners at 1/2/4 workers, retained-lock safety,
failure ordering among started tasks, runtime restoration, and allocation-failure recovery.
It is included in the native CTest smoke tests and needs no testing framework.

## Task storage and allocation boundary

`FixedSizeTask` has 96 bytes of inline capture storage and one pointer to a static
operations table. Construction checks size, alignment, nonthrowing move and
nonthrowing destruction at every production closure instantiation. There is no
heap fallback. Seed captures use indices into the manager's already-reserved
seed vector. Immediate-value copies only copy scalar bits and are declared
`noexcept`, allowing construction-index captures to move without throwing.

Task vectors still reserve at their original ownership sites. Moving a vector
into the group transfers that same contiguous allocation; closures never move to
a second task container. A child destroys its closure before decrementing the
completion counter. The vector size is immutable while runners exist, so a late
runner can safely discover that no unclaimed children remain. Results, seed
storage and the manager may then unwind without a late runner accessing them.
The scope mutex lives in the manager and is borrowed by the parent Interpreter
only until all children have joined (or until an unsubmitted manager is discarded).

One non-Taskflow allocation remains **per group that submits runtime runners**:
`make_shared<InterpreterTaskGroup>` combines the group and its ownership control
block. Its lifetime is distinct from child completion. The owning join can return
while runners are queued, or after the last child's decrement but before that
runner's notify/return. Every runner holds ownership until its callable is retired;
Taskflow's runtime anchor also retains descendants until graph completion.

A stack/raw-pointer replacement cannot just wait for runner retirement: at
saturation all workers can be inside nested joins with their runners still queued.
Unrestricted `corun` would run unrelated writers under retained locks. Taskflow's
public runtime API provides neither selective runner removal nor a synchronous
join that only helps this group. Retaining one shared allocation is the bounded
lifetime cost of preserving this scheduling/locking model without changing the
vendored scheduler or adding a separate reclamation pool. Groups with no runners
(one child or one worker) use stack storage and allocate nothing after reserve.

Generation creation/resizing still allocates executors, observers and worker
contexts once per generation. Taskflow owns graph/topology/node/callable/queue
allocations, including runtime runners. The allocation probe reports those
submission costs separately from its direct measurement of shared group storage.
On Linux amd64 with GCC 14/libstdc++, the probe measures a 112-byte task
holder and one 120-byte shared group allocation. At 1 worker, groups of
1/4/4096 tasks allocate zero bytes after reserve. At 2 workers, 4 and 4096 tasks
both use 3 allocations / 352 bytes including Taskflow; at 4 workers they both use
7 allocations / 816 bytes. Thus only 120 bytes are non-Taskflow plumbing, independent
of task count; the remaining 232 bytes per submitted runner belong to Taskflow's
node and callable storage. These byte counts are ABI-specific, not portable limits.

Vector reserves (tasks, seeds, construction effects, opcode/construction stacks)
and allocations performed by interpreted code are outside the zero-per-closure
claim. Thrown exception objects may allocate in the C++ runtime; the success path
has no exception-vector allocation.

Build and run the focused probe without CMake:

```sh
mkdir -p out/taskflow-probe
g++-14 -std=c++20 -O2 -pthread -DMULTITHREAD_SUPPORT \
  -Isrc/Amalgam -Isrc/3rd_party -Wall -Wextra -Werror \
  test/unit_test/concurrency_storage_test.cpp -o out/taskflow-probe/concurrency-storage
out/taskflow-probe/concurrency-storage
```

For CI or the canonical Linux build container
`ghcr.io/howsoai/amalgam-build-container-linux:2.0.10`:

```sh
cmake --preset amd64-release-linux
cmake --build --preset amd64-release-linux
ctest --preset amd64-release-linux -R 'Concurrency[.]|App.FullTest|Lib.SmokeTest'
```

`Concurrency.Convictions.1/2/4` restores the `203f3fb` query regression: the
first query is concurrent, then the cache is reused with changed dimensions and
callback capability, including callbacks with nested `||` inside an outer
parallel map. Callback-bearing query loops are intentionally serial: callbacks
mutate the shared calling Interpreter's stacks and memory lock. This corrects the
callback fan-out introduced in `68b51e0`. Only numeric loops use the maintenance
executor; a callback's own nested concurrent opcodes still parallelize through
Interpreter runtime groups.

The earlier `203f3fb` commit also contains additional Interpreter/GC probes and
`nested.amlg`, deleted by subsequent cleanup. Those are not current CTest targets.
Their graph entry calls need adaptation to the current header API, and the old
listener-based overlap/exception checks also require the virtual print-listener
interface removed by subsequent cleanup.
