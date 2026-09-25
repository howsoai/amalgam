# Taskflow execution

Taskflow 4.1.0 is vendored under `src/3rd_party/taskflow`; builds are offline.
Call sites own ordinary `tf::Taskflow` graphs. Synchronous joins publish child
results before the caller continues. There are no detached tasks. External
callers submit root graphs with `run(...).get()`. Taskflow propagates the first
exception after running tasks finish; pending work may be cancelled. Successful
graphs execute each node exactly once per run.

## Interpreter lock invariant

**An Interpreter worker never submits or helps another Interpreter graph.** Every
Interpreter concurrency entry checks `CanRunInterpreterConcurrently()`. Within a
worker, nested `||` uses the opcode's ordinary serial path on that same thread.
This guarantees progress even with every worker occupied, without running an
unrelated task on a stack retaining entity, query-cache, or scope locks.
`RunTaskflow` rejects worker submissions with `logic_error` before executing work;
this also rejects maintenance -> Interpreter submissions. Root parallelism is
retained; inner parallelism is deliberately sacrificed for this lock invariant.
Graphs must not contain blocking dependencies on separately queued Interpreter
work. Explicit DAG edges are appropriate for dependencies within a root graph.

The review's `corun` concern is real, and was introduced by the migration:

- Old `ThreadPool::CountableTaskSet::WaitForTasks` waited on a condition variable;
  `ChangeCurrentThreadStateFromActiveToWaiting` made replacement capacity available.
  It never executed another task on the waiting stack. `AreThreadsAvailable` let
  nested opcodes fall back to serial execution when saturated.
- Vendored `core/executor.hpp`, `Executor::_corun_until`, pops its worker queue
  and steals from *all* worker queues and external buffers, invoking any task it
  finds. It does not restrict helping to descendants of the joined graph.
- `OpcodesEntityQueryEngine.cpp` retains `source_entity` (`EntityReadReference`)
  across `GetEntitiesMatchingQuery`. `EntityQueryCaches::GetMatchingEntities`
  retains its cache read lock during distance callbacks. The called entity's own
  read lock is explicitly released in `ComputeDistanceTermFromEvaluatingOnEntity`,
  but the outer container/cache locks remain. `InterpretNode_ENT_MOVE_ENTITIES`
  retains the source write reference while interpreting the destination.
  Releasing only the Interpreter memory lock does not release those locks.

Thus an unrelated writer stolen by `corun` could block on its own suspended
caller's lock. Separating maintenance from Interpreter work alone was insufficient.
The enforced serial nested path removes that scheduler-induced lock cycle. It
cannot make programs that explicitly access a lock held by their own caller safe;
that is a pre-existing entity/callback restriction, not a scheduling guarantee.

InterpreterConcurrencyManager builds a graph without starting tasks. Its explicit
completion point releases the parent's memory read lock, waits for the root graph,
then restores the lock and releases the shared-scope mutex, also on exceptions.
Destroying an unsubmitted graph cancels construction without running side effects.
Graph captures and result slots outlive the join. Interpreter registration is
removed on both normal and exceptional exits. GC failures release waiters and
discard partial marks before propagating.

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

At one configured thread, Interpreter opcodes and maintenance loop call sites use
their serial paths, preserving the old no-graph/no-worker-handoff behavior. Direct
scheduler unit-test submissions can still exercise a one-worker executor.
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
synchronous maintenance waits are excluded. A serial caller counts itself, and the
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
resize, maintenance isolation, nested serial progress at saturation, rejection of
worker Interpreter submissions, and sampled allowances at 1/2/4 workers.
`nested.amlg` checks nested arithmetic/containers, scope writes, retained allocations,
and move destination interpretation. `convictions.amlg` starts with the exact
concurrent query that previously crashed, then compares serial results and reuses
the cache with different dimensions and callback capability. Callback queries also
run within an outer parallel map, reaching nested `||` on lock-holding workers.
CTest runs both files
in fresh processes at 1/2/4 threads. `concurrency-stress` runs a 500,000-node graph
twice with exact-once and visibility checks.
