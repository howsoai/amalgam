# SBFDS nearest-query regressions

`nearest_regression.amlg` compares optimized nearest queries with an exhaustive
oracle, checking distances and each returned ID's distance while allowing ties.
It covers nominal expansion, asymmetric boolean distances, weighted nulls,
continuous and nominal surprisal, row/feature subsets, mutations, cache rebuilds,
and concurrent reads. CTest registers it for each app target and requires
`SBFDS checks=99 failures=0`, failing on any `FAIL` output.

From the repository root, after [building](../../README.md) the Linux release preset:

```sh
ctest --preset amd64-release-linux -R SBFDS --output-on-failure
```

`engine_reproduction.amlg` optionally checks nominal expansion, asymmetric boolean
and weighted null distances, and surprisal through Howso Engine's `get_distances`
API against exhaustive results. It is not registered in CTest. Replace the path
below with an Engine source checkout, retaining the trailing slash; require
`Engine checks=7 failures=0` in the output, since exit status alone is insufficient.

```sh
out/build/amd64-release-linux/amalgam-mt test/sbfds/engine_reproduction.amlg /path/to/howso-engine/
```
