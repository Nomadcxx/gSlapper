#!/bin/bash
# Comprehensive test suite for gSlapper image support using actual wallpapers
# Run this in a Wayland session

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
WALLPAPER_DIR="/home/nomadx/Pictures/wallpapers"
TEST_OUTPUT="/tmp/gslapper-test-output.log"
IPC_SOCKET="/tmp/gslapper-test.sock"

pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((TESTS_PASSED++))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    ((TESTS_FAILED++))
}

info() {
    echo -e "${BLUE}→${NC} $1"
}

section() {
    echo ""
    echo -e "${YELLOW}═══ $1 ═══${NC}"
    echo ""
}

cleanup() {
    pkill -9 -f "gslapper.*DP-1" 2>/dev/null || true
    rm -f "$IPC_SOCKET"
}

trap cleanup EXIT

# Select test images with different characteristics
PNG_16x9="$WALLPAPER_DIR/404-page_not_found.png"
PNG_LARGE="$WALLPAPER_DIR/Anime-City-Night.png"
JPEG_SMALL="$WALLPAPER_DIR/archPCX1.jpeg"
JPEG_ABSTRACT="$WALLPAPER_DIR/Abstract - Nature.jpg"
PNG_GIRL="$WALLPAPER_DIR/Anime-Girl1.png"

echo "╔════════════════════════════════════════════╗"
echo "║  gSlapper Image Support Test Suite        ║"
echo "║  Testing with Real Wallpaper Files        ║"
echo "╚════════════════════════════════════════════╝"
echo ""

section "Image Detection Tests"

info "Test 1: PNG file detection"
if timeout 3 ./build/gslapper -v DP-1 "$PNG_16x9" > "$TEST_OUTPUT" 2>&1; then
    if grep -q "Image detected" "$TEST_OUTPUT" && grep -q "defaulting to fill mode" "$TEST_OUTPUT"; then
        pass "PNG detected as image, fill mode default activated"
    else
        fail "PNG detection or fill mode default failed"
        cat "$TEST_OUTPUT" | grep -E "(Image|Fill|Error)" | head -10
    fi
else
    fail "gslapper failed to start with PNG"
fi
cleanup

info "Test 2: JPEG file detection"
if timeout 3 ./build/gslapper -v DP-1 "$JPEG_SMALL" > "$TEST_OUTPUT" 2>&1; then
    if grep -q "Image detected" "$TEST_OUTPUT"; then
        pass "JPEG detected as image"
    else
        fail "JPEG detection failed"
    fi
else
    fail "gslapper failed to start with JPEG"
fi
cleanup

section "Scaling Mode Tests"

info "Test 3: Fill mode (default for images)"
if timeout 3 ./build/gslapper -v DP-1 "$PNG_16x9" > "$TEST_OUTPUT" 2>&1; then
    if grep -q "Fill mode enabled" "$TEST_OUTPUT" || grep -q "fill mode" "$TEST_OUTPUT"; then
        pass "Fill mode activated by default for images"
    else
        fail "Fill mode not activated"
        grep -E "(mode|scale)" "$TEST_OUTPUT" | head -5
    fi
else
    fail "Fill mode test failed to start"
fi
cleanup

info "Test 4: Explicit fill mode"
if timeout 3 ./build/gslapper -v -o "fill" DP-1 "$JPEG_ABSTRACT" > "$TEST_OUTPUT" 2>&1; then
    if grep -q -i "fill" "$TEST_OUTPUT"; then
        pass "Explicit fill mode works"
    else
        fail "Explicit fill mode failed"
    fi
else
    fail "Explicit fill mode test failed"
fi
cleanup

info "Test 5: Stretch mode with image"
if timeout 3 ./build/gslapper -v -o "stretch" DP-1 "$PNG_GIRL" > "$TEST_OUTPUT" 2>&1; then
    if grep -q -i "stretch" "$TEST_OUTPUT" || ! grep -q "Fill mode" "$TEST_OUTPUT"; then
        pass "Stretch mode overrides fill default"
    else
        fail "Stretch mode didn't override fill"
    fi
else
    fail "Stretch mode test failed"
fi
cleanup

info "Test 6: Panscan mode with image"
if timeout 3 ./build/gslapper -v -o "panscan=0.8" DP-1 "$PNG_LARGE" > "$TEST_OUTPUT" 2>&1; then
    if grep -q -i "panscan" "$TEST_OUTPUT" || ! grep -q "Fill mode" "$TEST_OUTPUT"; then
        pass "Panscan mode overrides fill default"
    else
        fail "Panscan mode didn't override fill"
    fi
else
    fail "Panscan mode test failed"
fi
cleanup

section "Image Loading Tests"

info "Test 7: Large PNG loading"
if timeout 3 ./build/gslapper -v DP-1 "$PNG_LARGE" > "$TEST_OUTPUT" 2>&1; then
    if grep -q "Image loaded" "$TEST_OUTPUT" || grep -q "Loading image" "$TEST_OUTPUT"; then
        pass "Large PNG (22MB) loaded successfully"
    else
        fail "Large PNG failed to load"
        tail -20 "$TEST_OUTPUT"
    fi
else
    fail "Large PNG test failed"
fi
cleanup

info "Test 8: Small JPEG loading"
if timeout 3 ./build/gslapper -v DP-1 "$JPEG_SMALL" > "$TEST_OUTPUT" 2>&1; then
    if grep -q "Image loaded" "$TEST_OUTPUT" || grep -q "Loading image" "$TEST_OUTPUT"; then
        pass "Small JPEG (74KB) loaded successfully"
    else
        fail "Small JPEG failed to load"
    fi
else
    fail "Small JPEG test failed"
fi
cleanup

section "IPC Tests with Images"

info "Test 9: IPC query with image"
./build/gslapper --ipc-socket "$IPC_SOCKET" -v DP-1 "$PNG_16x9" > "$TEST_OUTPUT" 2>&1 &
sleep 3

RESPONSE=$(echo "query" | nc -U "$IPC_SOCKET" 2>&1)
if echo "$RESPONSE" | grep -q "STATUS.*404-page_not_found.png"; then
    pass "IPC query returns correct image path"
else
    fail "IPC query failed: $RESPONSE"
fi

info "Test 10: IPC change between images"
RESPONSE=$(echo "change $JPEG_ABSTRACT" | nc -U "$IPC_SOCKET" 2>&1)
if echo "$RESPONSE" | grep -q "OK"; then
    pass "IPC change command accepted"
    sleep 2
    if grep -q "Abstract - Nature.jpg" "$TEST_OUTPUT"; then
        pass "Image actually changed"
    else
        fail "Image didn't change in logs"
    fi
else
    fail "IPC change failed: $RESPONSE"
fi

info "Test 11: IPC preload command"
RESPONSE=$(echo "preload $PNG_GIRL" | nc -U "$IPC_SOCKET" 2>&1)
if echo "$RESPONSE" | grep -q "OK"; then
    pass "IPC preload command works (stub)"
else
    fail "IPC preload failed: $RESPONSE"
fi

info "Test 12: IPC unload command"
RESPONSE=$(echo "unload $PNG_GIRL" | nc -U "$IPC_SOCKET" 2>&1)
if echo "$RESPONSE" | grep -q "OK"; then
    pass "IPC unload command works (stub)"
else
    fail "IPC unload failed: $RESPONSE"
fi

info "Test 13: IPC list command"
RESPONSE=$(echo "list" | nc -U "$IPC_SOCKET" 2>&1)
if echo "$RESPONSE" | grep -q "PRELOADED"; then
    pass "IPC list command works (stub)"
else
    fail "IPC list failed: $RESPONSE"
fi

info "Test 14: Preload rejects non-image"
RESPONSE=$(echo "preload /etc/passwd" | nc -U "$IPC_SOCKET" 2>&1)
if echo "$RESPONSE" | grep -q "ERROR.*not an image"; then
    pass "Preload correctly rejects non-image files"
else
    fail "Preload should reject non-image: $RESPONSE"
fi

echo "stop" | nc -U "$IPC_SOCKET" 2>&1 || true
sleep 1
cleanup

section "Help Text Verification"

info "Test 15: Help includes image documentation"
HELP_OUTPUT=$(./build/gslapper --help 2>&1)
if echo "$HELP_OUTPUT" | grep -q "fill.*Fill screen maintaining aspect" && \
   echo "$HELP_OUTPUT" | grep -q -E "(JPEG|PNG|WebP)"; then
    pass "Help text includes image scaling and format documentation"
else
    fail "Help text missing image documentation"
fi

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║           Test Results Summary             ║"
echo "╚════════════════════════════════════════════╝"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    echo ""
    echo "Test log available at: $TEST_OUTPUT"
    exit 1
fi
