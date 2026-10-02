#!/bin/bash
# IPC socket permission tests.
# Regression test for the unrestricted-bind issue: create_socket() bound the
# AF_UNIX socket without chmod, so the inode was created as 0777 & ~umask and
# other local users could connect when the socket was placed in a shared
# directory (e.g. -I /tmp/...). The socket must always be owner-only (0600),
# matching the state file's fchmod(fd, 0600) in state.c.

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

echo "=== gSlapper IPC Socket Permission Tests ==="
echo ""

if [[ ! -x "$GSLAPPER" ]]; then
    fail "gslapper binary not found at $GSLAPPER"
    echo "Run: ninja -C build"
    exit 1
fi

export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

if [[ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]]; then
    skip "No Wayland display at $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY - cannot run live IPC tests"
    exit 0
fi

# Prefer a static image: image mode needs no video codec, so the test works
# on minimal GStreamer installs. Fall back to a video if no image is found.
WALLPAPER=""
for candidate in "$PROJECT_ROOT"/docs-site/public/*.png "$PROJECT_ROOT"/*.png \
                 "$HOME/Pictures"/*.png "$HOME/Videos"/*.png; do
    if [[ -f "$candidate" ]]; then
        WALLPAPER="$candidate"
        break
    fi
done
if [[ -z "$WALLPAPER" ]]; then
    for candidate in "$HOME/Videos"/*.webm "$HOME/Videos"/*.mp4; do
        if [[ -f "$candidate" ]]; then
            WALLPAPER="$candidate"
            break
        fi
    done
fi
if [[ -z "$WALLPAPER" ]]; then
    skip "No image or video found to display - cannot run live IPC tests"
    exit 0
fi

WORK_DIR="$(mktemp -d)"
SOCK="$WORK_DIR/ipc.sock"
cleanup() {
    pkill -f "$GSLAPPER .*$SOCK" 2>/dev/null
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

run_ipc_case() {
    local label="$1" umask_val="$2"
    rm -f "$SOCK"
    (umask "$umask_val"; exec "$GSLAPPER" --no-save-state -I "$SOCK" '*' "$WALLPAPER" \
        > "$WORK_DIR/run.log" 2>&1 &)

    # Wait for the socket to appear
    local ok=0
    for _ in $(seq 1 50); do
        if [[ -S "$SOCK" ]]; then ok=1; break; fi
        sleep 0.1
    done
    if [[ $ok -eq 0 ]]; then
        fail "$label: IPC socket never appeared"
        cat "$WORK_DIR/run.log" | tail -5
        return
    fi

    local perms
    perms=$(stat -c '%a' "$SOCK")
    if [[ "$perms" == "600" ]]; then
        pass "$label: socket is 0600 under umask $umask_val"
    else
        fail "$label: socket is $perms under umask $umask_val (expected 600)"
    fi

    # IPC must still function
    if printf 'query\n' | socat - UNIX-CONNECT:"$SOCK" 2>/dev/null | grep -q "STATUS:"; then
        pass "$label: IPC query works"
    else
        fail "$label: IPC query failed"
    fi

    pkill -f "$GSLAPPER .*$SOCK" 2>/dev/null
    for _ in $(seq 1 50); do
        pgrep -f "$SOCK" >/dev/null 2>&1 || break
        sleep 0.1
    done
}

# umask 000 is the worst case: without the chmod fix the socket lands as 0777.
run_ipc_case "permissive" 000
# umask 022 is the common desktop default.
run_ipc_case "default" 022

echo ""
echo "=== Results: $TESTS_PASSED passed, $TESTS_FAILED failed ==="
[[ $TESTS_FAILED -eq 0 ]]
