#!/bin/bash

echo "=== gSlapper Systemd Service Test Scenarios ==="
echo ""
echo "NOTE: These are manual test scenarios. Run on a system with:"
echo "  - Wayland compositor running"
echo "  - gslapper installed"
echo "  - systemd user session"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; }
info() { echo "INFO: $1"; }
skip() { echo "SKIP: $1"; }

echo "=== Pre-flight Checks ==="

if [[ -z "$WAYLAND_DISPLAY" ]]; then
    skip "WAYLAND_DISPLAY not set - systemd tests require Wayland"
    exit 0
fi

if ! systemctl --user --version &>/dev/null; then
    skip "systemd user session not available"
    exit 0
fi

if ! command -v gslapper &>/dev/null && [[ ! -x "$PROJECT_ROOT/build/gslapper" ]]; then
    fail "gslapper not found in PATH or build directory"
    exit 1
fi

pass "Pre-flight checks passed"

echo ""
echo "=== Service File Validation ==="

for svc in gslapper.service gslapper@.service; do
    if [[ -f "$PROJECT_ROOT/$svc" ]]; then
        if systemd-analyze verify "$PROJECT_ROOT/$svc" 2>&1 | grep -q "error"; then
            fail "$svc has syntax errors"
        else
            pass "$svc syntax is valid"
        fi
    else
        skip "$svc not found"
    fi
done

echo ""
echo "=== Environment Setup Check ==="

ENV_FILE="$HOME/.config/gslapper/environment"
if [[ -f "$ENV_FILE" ]]; then
    if grep -q "WAYLAND_DISPLAY" "$ENV_FILE"; then
        pass "Environment file exists with WAYLAND_DISPLAY"
    else
        info "Environment file exists but missing WAYLAND_DISPLAY"
    fi
else
    info "Environment file not found at $ENV_FILE"
    info "Create with: mkdir -p ~/.config/gslapper && echo \"WAYLAND_DISPLAY=$WAYLAND_DISPLAY\" > $ENV_FILE"
fi

echo ""
echo "=== State Directory Check ==="

STATE_DIR="$HOME/.local/state/gslapper"
if [[ -d "$STATE_DIR" ]]; then
    pass "State directory exists: $STATE_DIR"
    if ls "$STATE_DIR"/state-*.txt &>/dev/null; then
        info "Found state files:"
        ls -la "$STATE_DIR"/state-*.txt 2>/dev/null
    fi
else
    info "State directory not found (will be created on first use)"
fi

echo ""
echo "=== Manual Test Scenarios ==="
echo ""
echo "Run these tests manually to verify systemd integration:"
echo ""
echo "1. START SERVICE:"
echo "   systemctl --user start gslapper.service"
echo "   Expected: Service starts, wallpaper displays, status shows 'active (running)'"
echo ""
echo "2. CHECK READINESS NOTIFICATION:"
echo "   systemctl --user status gslapper.service"
echo "   Expected: Shows 'Status: Wallpapers loaded' (if Type=notify works)"
echo ""
echo "3. RELOAD SERVICE (SIGHUP):"
echo "   systemctl --user reload gslapper.service"
echo "   Expected: State saved, service restarts, wallpaper restored"
echo ""
echo "4. STOP SERVICE:"
echo "   systemctl --user stop gslapper.service"
echo "   Expected: State saved, clean shutdown, service inactive"
echo ""
echo "5. RESTART TEST:"
echo "   systemctl --user restart gslapper.service"
echo "   Expected: Previous state restored, same wallpaper displays"
echo ""
echo "6. PER-MONITOR SERVICE:"
echo "   systemctl --user start gslapper@DP-1.service"
echo "   Expected: Service runs only on DP-1 monitor"
echo ""
echo "7. CHECK LOGS:"
echo "   journalctl --user -u gslapper.service -f"
echo "   Expected: Startup messages, no errors"
echo ""
echo "=== Service Test Scenarios Complete ==="
