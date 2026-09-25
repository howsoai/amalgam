# Vendored Taskflow

- Upstream: https://github.com/taskflow/taskflow
- Release: v4.1.0
- Commit: 45366fe5bc4f2f8ec9aa590b40c504e296886865
- License: MIT, reproduced unchanged in `LICENSE` and in the repository's third-party notices.
- Contents: transitive local-include closure of `taskflow/taskflow.hpp`
  (33 CPU headers). CUDA, SYCL, examples, benchmarks and upstream build files are excluded.

To reproduce, download the upstream archive at this commit and recursively copy
all quoted local includes starting with `taskflow/taskflow.hpp`, retaining paths
relative to upstream `taskflow/`. Copy the upstream `LICENSE` as well.
`SHA256SUMS` records each included header and license after the local patches below. Normal builds need no fetch,
package manager, generated header, or external Taskflow installation.

Taskflow is private implementation detail: Amalgam exports no Taskflow types in
its public C API and does not install internal headers or a CMake development
package. Binary installs/packages include the Taskflow license under
`share/licenses/amalgam/taskflow`. Static Visual Studio projects use their existing
`src/3rd_party` include root and list the vendored headers for navigation.

Local exception-safety patches:
- `core/runtime.hpp`: roll back the `silent_async` parent join reference if child
  construction or queue publication throws, preventing a permanently anchored runtime.
  Only `Runtime::silent_async` was patched; other Runtime async APIs are unchanged
  and are not used by Amalgam.
- `core/async.hpp`: recycle an async node if queue publication throws, including
  releasing the executor's node ownership for immediately scheduled dependent
  async tasks. Roll back the executor topology reference when `async`,
  `silent_async`, `dependent_async`, or `silent_dependent_async` submission throws.
  Successful submissions retain that reference until normal task teardown, so
  there is no double-decrement. Queue pushes allocate before publication;
  notification after publication does not throw.

`Concurrency.Taskflow` exercises the construction failure using a throwing callable
copy and verifies the runtime graph successor still executes. That runtime test
covers the parent join counter, not the executor topology counter.
`Concurrency.TaskflowExceptions` uses a standalone allocation-injection executable
with one blocked worker and a two-slot overflow queue. It forces construction and
queue-growth allocation failures for all four executor async families, with and
without task parameters, using both static and runtime callables. Both callable
forms capture 64 bytes of padding by value to exceed expected STL small-function
buffers; the test asserts that construction allocates before injecting failures.
It checks the topology count and callback release after each failure, successful
recovery and `wait_for_all`, plus a live dependent chain. The dependent failure cases cover
construction and immediate queue publication, not allocation while registering
edges on unfinished predecessors.

Remaining upstream limitations: `ObjectPool::animate` may leak a popped block
from its free list if the object constructor throws; the patches do not restore
that block to the pool. The tests verify counters and callback release, not full
object-pool exception safety. Dependent async edge growth on unfinished
predecessors also remains unpatched; Amalgam does not use dependent async APIs.

These patches do not alter Taskflow queue ordering, stealing, worker limits, or
normal scheduling.
