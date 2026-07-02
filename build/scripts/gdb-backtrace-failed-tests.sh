#!/usr/bin/env bash
#
# Re-runs any CTest tests that failed in the preceding `ctest` invocation,
# this time attaching gdb-multiarch to the QEMU user-mode gdbstub so a
# symbolicated guest backtrace can be captured. Intended to run only as a
# diagnostic step after `ctest` has already reported failures (see
# .github/workflows/build.yml) -- it does not affect test pass/fail status.
#
# Usage: gdb-backtrace-failed-tests.sh <ctest-build-dir>

set -uo pipefail

BUILD_DIR="${1:?usage: $0 <ctest-build-dir>}"
cd "$BUILD_DIR" || exit 1

FAILED_LOG="Testing/Temporary/LastTestsFailed.log"
if [ ! -f "$FAILED_LOG" ]; then
    echo "No $FAILED_LOG found, nothing to do."
    exit 0
fi

TEST_JSON="$(ctest --show-only=json-v1)"

while IFS=: read -r _ TEST_NAME; do
    [ -z "$TEST_NAME" ] && continue

    mapfile -t CMD < <(jq -r --arg name "$TEST_NAME" \
        '.tests[] | select(.name == $name) | .command[]' <<< "$TEST_JSON")
    WORKDIR="$(jq -r --arg name "$TEST_NAME" \
        '.tests[] | select(.name == $name) | (.properties[]? | select(.name == "WORKING_DIRECTORY") | .value) // empty' <<< "$TEST_JSON")"

    if [ "${#CMD[@]}" -eq 0 ]; then
        echo "::warning::Could not find command for failed test '$TEST_NAME'"
        continue
    fi

    if [ "${CMD[0]}" != "qemu-aarch64" ]; then
        # Only the QEMU-emulated arm64 tests can use the gdbstub trick below.
        echo "Skipping '$TEST_NAME' (not run under qemu-aarch64)"
        continue
    fi

    echo "::group::GDB backtrace for failed test: $TEST_NAME"
    echo "+ ${CMD[*]}"

    if [ -n "$WORKDIR" ]; then
        pushd "$WORKDIR" > /dev/null || continue
    fi

    PORT=$(( (RANDOM % 20000) + 20000 ))
    # Insert "-g <port>" right after the qemu-aarch64 binary so QEMU starts a
    # gdbstub and pauses the guest until a debugger attaches.
    GDB_QEMU_CMD=("${CMD[0]}" "-g" "$PORT" "${CMD[@]:1}")

    "${GDB_QEMU_CMD[@]}" &
    QEMU_PID=$!

    # QEMU blocks until a debugger connects, so poll the port rather than
    # sleeping a fixed amount of time.
    CONNECTED=0
    for _ in $(seq 1 50); do
        if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
            exec 3<&-
            CONNECTED=1
            break
        fi
        sleep 0.1
    done

    if [ "$CONNECTED" -eq 1 ]; then
        gdb-multiarch -q -batch \
            -ex "set pagination off" \
            -ex "target remote localhost:$PORT" \
            -ex "continue" \
            -ex "thread apply all bt full" \
            -ex "quit" \
            "${CMD[3]}"
    else
        echo "::warning::Timed out waiting for QEMU gdbstub on port $PORT for test '$TEST_NAME'"
        kill "$QEMU_PID" 2>/dev/null
    fi

    wait "$QEMU_PID" 2>/dev/null

    if [ -n "$WORKDIR" ]; then
        popd > /dev/null || true
    fi

    echo "::endgroup::"
done < "$FAILED_LOG"
