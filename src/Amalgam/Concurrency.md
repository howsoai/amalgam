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
opcode continuation. Exceptions are retained per child; unstarted work may be cancelled, and all
children are drained and joined before the first failure in submission order is
rethrown. Failed runtime submissions roll back their anchor reference; already
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
its half-core default. Counts beyond INT_MAX throw internally before changing
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
The debugger distinguishes configured capacity from execution generation size.

`Concurrency.Taskflow` checks exact-once execution/visibility, exception recovery,
resize, maintenance isolation, descendant progress at saturation, rejection of
nested root submissions, and sampled allowances at 1/2/4 workers.
`Concurrency.InterpreterOverlap` links the production Interpreter and runtime.
A recursive Amalgam function forks at each level; two print callbacks from the
same innermost opcode rendezvous with a five-second failure deadline. The listener
checks simultaneous activity on two distinct threads, and both opcode and graph
continuations check completed children and result visibility. The test also fills
every worker with an actual nested Interpreter while queuing unrelated writers
against retained locks, checks graph successors wait for those writers, and tests
nested exceptions and recovery. It repeats overlap and saturation 20 times at
each of 1/2/4 workers (one worker checks correctness without requiring overlap).
It also checks that shared construction targets stay unchanged while children
execute (a deterministic check at one worker), and repeatedly collects a shared
12,000-value cyclic graph with extended list/assoc storage and simultaneous
collector requests, including forced requests under shared read locks. Every retained value, cycle edge and cleared mark is checked.
The print listener is the normal production output interface, with an override
for synchronization; there is no test-specific scheduling path.

`nested.amlg` checks nested arithmetic/containers, scope writes, retained allocations,
and move destination interpretation. `convictions.amlg` starts with the exact
concurrent query that previously crashed, then compares serial results and reuses
the cache with different dimensions and callback capability. Callback queries also
run within an outer parallel map, reaching nested `||` on lock-holding workers.
CTest runs both files in fresh processes at 1/2/4 threads. `concurrency-stress`
runs a 500,000-node graph twice with exact-once and visibility checks.
