#!/bin/bash
# Positive/negative detection test for the /proc-based watch-list check:
# 1) a valid entry in pauselist must activate the monitor thread
# 2) a running process whose comm matches the entry must pause the wallpaper
# 3) entries with shell metacharacters are rejected at load time (no exec)
#
# Logs are asserted only after gslapper has fully exited: stdout redirected
# to a file is block-buffered, so lines written while it runs may still be
# sitting in the stdio buffer.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
GSLAPPER="$PROJECT_ROOT/build/gslapper"

TESTS_PASSED=0
TESTS_FAILED=0
pass() { echo "PASS: $1"; TESTS_PASSED=$((TESTS_PASSED + 1)); }
fail() { echo "FAIL: $1"; TESTS_FAILED=$((TESTS_FAILED + 1)); }

echo "=== gSlapper Watch List Detection Tests ==="
echo ""

if [[ ! -x "$GSLAPPER" ]]; then
    fail "gslapper binary not found at $GSLAPPER"
    exit 1
fi

export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
if [[ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]]; then
    echo "SKIP: no Wayland display"
    exit 0
fi

WALLPAPER=""
for candidate in "$HOME/Pictures"/*.png "$HOME/Videos"/*.png "$PROJECT_ROOT"/docs-site/public/*.png; do
    [[ -f "$candidate" ]] && { WALLPAPER="$candidate"; break; }
done
if [[ -z "$WALLPAPER" ]]; then
    echo "SKIP: no image wallpaper found"
    exit 0
fi

PAUSELIST="$HOME/.config/mpvpaper/pauselist"
mkdir -p "$(dirname "$PAUSELIST")"
cp "$PAUSELIST" "$PAUSELIST.bak.$$" 2>/dev/null
restore() {
    if [[ -f "$PAUSELIST.bak.$$" ]]; then
        mv "$PAUSELIST.bak.$$" "$PAUSELIST"
    else
        rm -f "$PAUSELIST"
    fi
    pkill -f "build/gslapper --no-save-state -v" 2>/dev/null
    pkill -x sleeper-probe 2>/dev/null
    rm -f /tmp/gslapper-pidof-pwned /tmp/sleeper-probe
}
trap restore EXIT

# Run gslapper in the background, wait for it to exit, then assert on its log
run_and_stop() {
    local logfile="$1" runtime="$2"
    "$GSLAPPER" --no-save-state -v '*' "$WALLPAPER" > "$logfile" 2>&1 &
    GS_PID=$!
    sleep "$runtime"
    pkill -f "build/gslapper --no-save-state -v" 2>/dev/null
    for _ in $(seq 1 50); do
        kill -0 "$GS_PID" 2>/dev/null || break
        sleep 0.1
    done
    wait "$GS_PID" 2>/dev/null
}

echo "--- Case 1: metacharacter payload is rejected and never executes"
rm -f /tmp/gslapper-pidof-pwned
printf '%s\n' 'x$(touch /tmp/gslapper-pidof-pwned)' > "$PAUSELIST"
run_and_stop /tmp/gslapper-wl-case1.log 4
if [[ -f /tmp/gslapper-pidof-pwned ]]; then
    fail "payload executed"
else
    pass "payload did not execute"
fi
if grep -q "Ignoring invalid watch list entry" /tmp/gslapper-wl-case1.log; then
    pass "invalid entry rejected with warning"
else
    fail "no rejection warning in log"
fi

echo "--- Case 2: valid entry activates monitor and detects a matching process"
# comm is truncated to 15 chars by the kernel; 'sleeper-probe' is 13, safe
printf '%s\n' 'sleeper-probe' > "$PAUSELIST"
cp /bin/sleep /tmp/sleeper-probe
chmod +x /tmp/sleeper-probe
/tmp/sleeper-probe 300 &
PROBE=$!
sleep 0.3
run_and_stop /tmp/gslapper-wl-case2.log 5
if grep -q "Pausing for sleeper-probe" /tmp/gslapper-wl-case2.log; then
    pass "monitor detected running process and paused"
else
    fail "monitor did not pause for sleeper-probe"
    tail -5 /tmp/gslapper-wl-case2.log
fi
kill "$PROBE" 2>/dev/null
wait "$PROBE" 2>/dev/null

echo "--- Case 3: no spurious pause with entries absent"
printf '%s\n' 'definitely-not-running-app' > "$PAUSELIST"
run_and_stop /tmp/gslapper-wl-case3.log 4
if ! grep -q "Pausing for" /tmp/gslapper-wl-case3.log; then
    pass "no spurious pause for absent process"
else
    fail "unexpected pause state"
fi

echo ""
echo "=== Results: $TESTS_PASSED passed, $TESTS_FAILED failed ==="
[[ $TESTS_FAILED -eq 0 ]]
