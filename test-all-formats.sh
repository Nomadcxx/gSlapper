#!/bin/bash
# Extended format testing for gSlapper image support
# Tests GIF, WebP, and all supported image formats

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

WALLPAPER_DIR="/home/nomadx/Pictures/wallpapers"
TEST_OUTPUT="/tmp/gslapper-format-test.log"

info() {
    echo -e "${BLUE}→${NC} $1"
}

pass() {
    echo -e "${GREEN}✓${NC} $1"
}

fail() {
    echo -e "${RED}✗${NC} $1"
}

section() {
    echo ""
    echo -e "${YELLOW}═══ $1 ═══${NC}"
    echo ""
}

cleanup() {
    pkill -9 -f "gslapper.*DP-1" 2>/dev/null || true
}

trap cleanup EXIT

echo "╔════════════════════════════════════════════╗"
echo "║  gSlapper Extended Format Testing          ║"
echo "╚════════════════════════════════════════════╝"

section "Format Support Verification"

# Test each supported format
declare -A FORMAT_TESTS=(
    ["PNG"]="$WALLPAPER_DIR/404-page_not_found.png"
    ["JPEG"]="$WALLPAPER_DIR/archPCX1.jpeg"
    ["JPG"]="$WALLPAPER_DIR/Abstract - Nature.jpg"
    ["WebP"]="$WALLPAPER_DIR/test-image.webp"
    ["GIF"]="$WALLPAPER_DIR/test-animated.gif"
)

for format in PNG JPEG JPG WebP GIF; do
    file="${FORMAT_TESTS[$format]}"

    if [ ! -f "$file" ]; then
        fail "$format - Test file not found: $file"
        continue
    fi

    info "Testing $format format: $(basename "$file")"

    if timeout 3 ./build/gslapper -v DP-1 "$file" > "$TEST_OUTPUT" 2>&1; then
        if grep -q -i "image detected" "$TEST_OUTPUT" || grep -q -i "loading image" "$TEST_OUTPUT"; then
            pass "$format format detected and loaded"
        else
            fail "$format format not detected as image"
            echo "    Log excerpt:"
            grep -E "(detect|image|error|warning)" "$TEST_OUTPUT" | head -3 | sed 's/^/    /'
        fi
    else
        fail "$format format failed to load"
        echo "    Last 5 log lines:"
        tail -5 "$TEST_OUTPUT" | sed 's/^/    /'
    fi

    cleanup
    sleep 1
done

section "Scaling Mode Tests Across Formats"

MODES=("fill" "stretch" "panscan=1.0" "original")
TEST_FILES=(
    "$WALLPAPER_DIR/404-page_not_found.png"
    "$WALLPAPER_DIR/test-image.webp"
    "$WALLPAPER_DIR/archPCX1.jpeg"
)

for mode in "${MODES[@]}"; do
    info "Testing $mode mode"

    for file in "${TEST_FILES[@]}"; do
        format=$(basename "$file" | sed 's/.*\.//')

        if timeout 3 ./build/gslapper -v -o "$mode" DP-1 "$file" > "$TEST_OUTPUT" 2>&1; then
            pass "  $mode works with $format"
        else
            fail "  $mode failed with $format"
        fi
        cleanup
    done
done

section "Edge Cases"

info "Test: Case-insensitive extension (.PNG)"
cp "$WALLPAPER_DIR/404-page_not_found.png" "/tmp/TEST.PNG"
if timeout 3 ./build/gslapper -v DP-1 "/tmp/TEST.PNG" > "$TEST_OUTPUT" 2>&1; then
    if grep -q -i "image detected" "$TEST_OUTPUT"; then
        pass "Uppercase .PNG extension detected"
    else
        fail "Case-insensitive detection failed"
    fi
else
    fail "Uppercase extension test failed"
fi
rm -f "/tmp/TEST.PNG"
cleanup

info "Test: Mixed case extension (.JpEg)"
cp "$WALLPAPER_DIR/archPCX1.jpeg" "/tmp/test.JpEg"
if timeout 3 ./build/gslapper -v DP-1 "/tmp/test.JpEg" > "$TEST_OUTPUT" 2>&1; then
    if grep -q -i "image detected" "$TEST_OUTPUT"; then
        pass "Mixed case .JpEg extension detected"
    else
        fail "Mixed case detection failed"
    fi
else
    fail "Mixed case extension test failed"
fi
rm -f "/tmp/test.JpEg"
cleanup

section "File Size Tests"

info "Large PNG (22MB): Anime-City-Night.png"
if timeout 5 ./build/gslapper -v DP-1 "$WALLPAPER_DIR/Anime-City-Night.png" > "$TEST_OUTPUT" 2>&1; then
    pass "Large PNG loaded successfully"
else
    fail "Large PNG failed to load"
fi
cleanup

info "Small JPEG (74KB): archPCX1.jpeg"
if timeout 3 ./build/gslapper -v DP-1 "$WALLPAPER_DIR/archPCX1.jpeg" > "$TEST_OUTPUT" 2>&1; then
    pass "Small JPEG loaded successfully"
else
    fail "Small JPEG failed to load"
fi
cleanup

info "Animated GIF (979KB): test-animated.gif"
if timeout 3 ./build/gslapper -v DP-1 "$WALLPAPER_DIR/test-animated.gif" > "$TEST_OUTPUT" 2>&1; then
    pass "Animated GIF loaded (shows first frame)"
else
    fail "Animated GIF failed to load"
fi
cleanup

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║         Extended Format Tests Complete     ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Test log: $TEST_OUTPUT"
