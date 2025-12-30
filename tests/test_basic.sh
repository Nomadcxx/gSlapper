#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
GSLAPPER="$PROJECT_ROOT/build/gslapper"
GSLAPPER_HOLDER="$PROJECT_ROOT/build/gslapper-holder"

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

echo "=== gSlapper Integration Tests ==="
echo ""

if [[ -x "$GSLAPPER" ]]; then
    pass "gslapper binary exists and is executable"
else
    fail "gslapper binary not found at $GSLAPPER"
    echo "Run: ninja -C build"
    exit 1
fi

if [[ -x "$GSLAPPER_HOLDER" ]]; then
    pass "gslapper-holder binary exists and is executable"
else
    fail "gslapper-holder binary not found at $GSLAPPER_HOLDER"
fi

if timeout 2s "$GSLAPPER" --help > /dev/null 2>&1; then
    pass "gslapper --help works"
else
    fail "gslapper --help failed or timed out"
fi

if timeout 2s "$GSLAPPER_HOLDER" --help > /dev/null 2>&1; then
    pass "gslapper-holder --help works"
else
    fail "gslapper-holder --help failed or timed out"
fi

HELP_OUTPUT=$("$GSLAPPER" --help 2>&1 || true)

if echo "$HELP_OUTPUT" | grep -q "Usage\|output"; then
    pass "gslapper shows usage info"
else
    fail "gslapper doesn't show usage info"
fi

if echo "$HELP_OUTPUT" | grep -q "ipc-socket\|IPC"; then
    pass "IPC socket option documented"
else
    fail "IPC socket option missing from help"
fi

if echo "$HELP_OUTPUT" | grep -q "fill\|stretch\|panscan"; then
    pass "Scaling modes documented"
else
    skip "Scaling modes not in help output"
fi

if echo "$HELP_OUTPUT" | grep -q "auto-pause\|auto-stop"; then
    pass "Auto-pause/stop options documented"
else
    fail "Auto-pause/stop options missing"
fi

echo ""
echo "=== Results ==="
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [[ $TESTS_FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
