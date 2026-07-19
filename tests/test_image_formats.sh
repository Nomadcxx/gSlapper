#!/bin/bash
# Image format decode tests.
# Regression test for issue #21: GIF wallpapers failed because the image
# pipeline used decodebin, which has no image/gif decoder on a standard
# GStreamer install. PNG is tested alongside to guard the common path.

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

echo "=== gSlapper Image Format Tests ==="
echo ""

if [[ ! -x "$GSLAPPER" ]]; then
    fail "gslapper binary not found at $GSLAPPER"
    echo "Run: ninja -C build"
    exit 1
fi

export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

if [[ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]]; then
    skip "No Wayland display at $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY - cannot run live decode tests"
    exit 0
fi

WORK_DIR="$(mktemp -d)"
cleanup() {
    pkill -9 -f "no-save-state" 2>/dev/null
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# 1x1 fixtures, embedded so the test has no external file dependencies
base64 -d > "$WORK_DIR/test.gif" <<'EOF'
R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7
EOF
base64 -d > "$WORK_DIR/test.png" <<'EOF'
iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==
EOF

# Runs gslapper with an image wallpaper for a few seconds, then SIGTERMs it.
# Success means the file decoded with no decode error: static images log
# "Cache added"; GIFs route through the video pipeline when gst-libav is
# installed and log "Loaded" instead.
test_image_decode() {
    local file="$1" label="$2" log="$WORK_DIR/$label.log"

    timeout -s TERM 8 stdbuf -oL -eL "$GSLAPPER" --no-save-state -v '*' "$file" > "$log" 2>&1

    if grep -qE "Image decode error|GStreamer error" "$log"; then
        fail "$label wallpaper decodes ($(grep -E 'Image decode error|GStreamer error' "$log" | head -1))"
    elif grep -qE "Cache added|Loaded " "$log"; then
        pass "$label wallpaper decodes"
    else
        fail "$label wallpaper decodes (no decode error but no success line; log tail: $(tail -3 "$log" | tr '\n' ' '))"
    fi
}

test_image_decode "$WORK_DIR/test.png" "PNG"
test_image_decode "$WORK_DIR/test.gif" "GIF"

echo ""
echo "=== Results: $TESTS_PASSED passed, $TESTS_FAILED failed ==="
[[ $TESTS_FAILED -eq 0 ]]
