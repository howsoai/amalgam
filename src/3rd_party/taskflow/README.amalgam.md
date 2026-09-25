# Vendored Taskflow

- Upstream: https://github.com/taskflow/taskflow
- Release: v4.1.0
- Commit: 45366fe5bc4f2f8ec9aa590b40c504e296886865
- License: MIT, reproduced unchanged in `LICENSE` and in the repository's third-party notices.
- Contents: unmodified transitive local-include closure of `taskflow/taskflow.hpp`
  (33 CPU headers). CUDA, SYCL, examples, benchmarks and upstream build files are excluded.

To reproduce, download the upstream archive at this commit and recursively copy
all quoted local includes starting with `taskflow/taskflow.hpp`, retaining paths
relative to upstream `taskflow/`. Copy the upstream `LICENSE` as well.
`SHA256SUMS` records each included header and license. Normal builds need no fetch,
package manager, generated header, or external Taskflow installation.

Taskflow is private implementation detail: Amalgam exports no Taskflow types in
its public C API and does not install internal headers or a CMake development
package. Binary installs/packages include the Taskflow license under
`share/licenses/amalgam/taskflow`. Static Visual Studio projects use their existing
`src/3rd_party` include root and list the vendored headers for navigation.
