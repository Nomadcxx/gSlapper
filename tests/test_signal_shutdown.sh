#!/bin/bash
# Signal shutdown tests.
# Regression test for the exit_cleanup SEGV: handle_signal() called
# exit_cleanup() directly from signal context. exit_cleanup is not
# async-signal-safe (file I/O, thread joins, g_object_unref of the
# pipeline) and races the GStreamer streaming threads, crashing on
# greeter session teardown in production. gslapper must exit cleanly
# from SIGTERM no matter when the signal lands.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
GSLAPPER="$PROJECT_ROOT/build/gslapper"

TESTS_PASSED=0
TESTS_FAILED=0

pass() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip() {
    echo "SKIP: $1"
}

echo "=== gSlapper Signal Shutdown Tests ==="
echo ""

if [[ ! -x "$GSLAPPER" ]]; then
    fail "gslapper binary not found at $GSLAPPER"
    echo "Run: ninja -C build"
    exit 1
fi

export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

if [[ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]]; then
    skip "No Wayland display at $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY - cannot run live signal tests"
    exit 0
fi

WORK_DIR="$(mktemp -d)"
cleanup() {
    pkill -9 -f "no-save-state" 2>/dev/null
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# Short test video so GStreamer streaming threads are active (the racy
# condition from the production coredump)
if ! gst-launch-1.0 -q videotestsrc num-buffers=150 ! vp8enc deadline=1 \
        ! webmmux ! filesink location="$WORK_DIR/test.webm" 2>/dev/null; then
    skip "Could not generate test video (vp8enc missing?)"
    exit 0
fi

ITERATIONS=20
DELAYS=(0.15 0.3 0.6 1.0)
failures=0

for i in $(seq 1 $ITERATIONS); do
    delay=${DELAYS[$(( (i - 1) % ${#DELAYS[@]} ))]}

    "$GSLAPPER" --no-save-state -o "loop" '*' "$WORK_DIR/test.webm" \
        > "$WORK_DIR/run-$i.log" 2>&1 &
    pid=$!

    sleep "$delay"
    # Two quick SIGTERMs: session teardown often delivers more than one
    kill -TERM "$pid" 2>/dev/null
    sleep 0.05
    kill -TERM "$pid" 2>/dev/null

    rc=0
    for _ in $(seq 1 100); do
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
        sleep 0.1
    done
    if kill -0 "$pid" 2>/dev/null; then
        kill -9 "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
        echo "  iteration $i (delay ${delay}s): HUNG, needed SIGKILL"
        failures=$((failures + 1))
        continue
    fi
    wait "$pid"
    rc=$?
    if [[ $rc -ne 0 ]]; then
        sig=$((rc - 128))
        echo "  iteration $i (delay ${delay}s): exit code $rc (signal $sig)"
        failures=$((failures + 1))
    fi
done

if [[ $failures -eq 0 ]]; then
    pass "SIGTERM exits cleanly across $ITERATIONS timing variations"
else
    fail "SIGTERM shutdown: $failures of $ITERATIONS runs crashed or hung"
fi

echo ""
echo "=== Results: $TESTS_PASSED passed, $TESTS_FAILED failed ==="
[[ $TESTS_FAILED -eq 0 ]]
