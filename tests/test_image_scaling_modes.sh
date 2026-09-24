#!/bin/bash
# Explicit scaling options must apply to static images as well as video.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
GSLAPPER="$PROJECT_ROOT/build/gslapper"

pass() {
    echo "PASS: $1"
}

fail() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

TESTS_FAILED=0

if [[ ! -x "$GSLAPPER" ]]; then
    echo "FAIL: gslapper binary not found at $GSLAPPER"
    exit 1
fi

export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

if [[ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]]; then
    echo "SKIP: no Wayland display at $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
    exit 0
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

# A valid 1x1 PNG fixture; geometry selection is logged independently of size.
base64 -d > "$WORK_DIR/test.png" <<'EOF'
iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==
EOF

test_image_scale() {
    local options="$1" expected="$2" label="$3" log="$WORK_DIR/$3.log"

    timeout -s TERM 3s stdbuf -oL -eL "$GSLAPPER" --no-save-state -vv \
        -o "$options" '*' "$WORK_DIR/test.png" > "$log" 2>&1 || true

    if grep -qF "$expected" "$log"; then
        pass "$label image scaling"
    else
        fail "$label image scaling (expected '$expected'; log tail: $(tail -4 "$log" | tr '\n' ' '))"
    fi
}

test_image_scale "fill" "Fill mode:" "fill"
test_image_scale "stretch" "Stretch mode:" "stretch"
test_image_scale "original" "Original resolution mode:" "original"
test_image_scale "panscan=1.0" "Panscan mode:" "panscan"
test_image_scale "panscan" "Panscan mode:" "panscan-default"
test_image_scale "" "Fill mode:" "default"

echo "=== Results: $((6 - TESTS_FAILED)) passed, $TESTS_FAILED failed ==="
[[ $TESTS_FAILED -eq 0 ]]
