#!/bin/bash
# Automated test suite for gSlapper static image support

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0
TEST_DIR="/tmp/gslapper-image-tests"

# Function to print test results
pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((TESTS_PASSED++))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    ((TESTS_FAILED++))
}

info() {
    echo -e "${YELLOW}→${NC} $1"
}

# Cleanup function
cleanup() {
    pkill -9 -f "gslapper.*DP-1" 2>/dev/null || true
    rm -f /tmp/gslapper-test.sock
}

trap cleanup EXIT

# Create test directory
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

echo "======================================"
echo " gSlapper Image Support Test Suite"
echo "======================================"
echo ""

# Generate test images with ImageMagick (if available) or download
info "Generating test images..."

if command -v convert &> /dev/null; then
    # Create test images with different aspect ratios
    convert -size 1920x1080 plasma:fractal "$TEST_DIR/16x9.png"
    convert -size 2560x1440 plasma:fractal "$TEST_DIR/16x9-large.png"
    convert -size 3440x1440 plasma:fractal "$TEST_DIR/21x9.png"
    convert -size 1080x1920 plasma:fractal "$TEST_DIR/portrait.png"
    convert -size 800x600 plasma:fractal "$TEST_DIR/4x3.png"

    # Different formats
    convert "$TEST_DIR/16x9.png" "$TEST_DIR/16x9.jpg"
    convert "$TEST_DIR/16x9.png" "$TEST_DIR/16x9.webp" 2>/dev/null || cp "$TEST_DIR/16x9.png" "$TEST_DIR/16x9.webp"

    pass "Generated test images with ImageMagick"
else
    # Fallback: download placeholder images
    info "ImageMagick not found, downloading placeholder images..."
    curl -s -o "$TEST_DIR/16x9.png" "https://via.placeholder.com/1920x1080/FF6B6B/FFFFFF.png?text=16:9+Test+Image" || fail "Download failed"
    curl -s -o "$TEST_DIR/16x9.jpg" "https://via.placeholder.com/1920x1080/4ECDC4/FFFFFF.jpg?text=16:9+JPEG" || fail "Download failed"
    curl -s -o "$TEST_DIR/portrait.png" "https://via.placeholder.com/1080x1920/95E1D3/FFFFFF.png?text=Portrait" || fail "Download failed"
    pass "Downloaded test images"
fi

# Build gslapper
info "Building gslapper..."
cd /home/nomadx/Documents/GitHub/gSlapper
ninja -C build > /dev/null 2>&1
if [ $? -eq 0 ]; then
    pass "Build successful"
else
    fail "Build failed"
    exit 1
fi

echo ""
echo "=== File Detection Tests ==="

# Test 1: Image extension detection
info "Test: PNG file detection"
timeout 3 ./build/gslapper -v DP-1 "$TEST_DIR/16x9.png" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Image detected" /tmp/test-output.log && grep -q "defaulting to fill mode" /tmp/test-output.log; then
    pass "PNG detected as image, fill mode default activated"
else
    fail "PNG detection or fill mode default failed"
fi
cleanup

# Test 2: JPEG detection
info "Test: JPEG file detection"
timeout 3 ./build/gslapper -v DP-1 "$TEST_DIR/16x9.jpg" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Image detected" /tmp/test-output.log; then
    pass "JPEG detected as image"
else
    fail "JPEG detection failed"
fi
cleanup

# Test 3: Case-insensitive extension
info "Test: Case-insensitive extension (.PNG)"
cp "$TEST_DIR/16x9.png" "$TEST_DIR/test.PNG"
timeout 3 ./build/gslapper -v DP-1 "$TEST_DIR/test.PNG" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Image detected" /tmp/test-output.log; then
    pass "Uppercase .PNG extension detected"
else
    fail "Case-insensitive detection failed"
fi
cleanup

echo ""
echo "=== Scaling Mode Tests ==="

# Test 4: Fill mode (explicit)
info "Test: Fill mode (explicit)"
timeout 3 ./build/gslapper -v -o "fill" DP-1 "$TEST_DIR/16x9.png" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Fill mode enabled" /tmp/test-output.log && grep -q "Fill mode: scale_x" /tmp/test-output.log; then
    pass "Fill mode works with -o fill"
else
    fail "Fill mode not activated"
fi
cleanup

# Test 5: Stretch mode with image
info "Test: Stretch mode with image"
timeout 3 ./build/gslapper -v -o "stretch" DP-1 "$TEST_DIR/16x9.png" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Stretch mode:" /tmp/test-output.log; then
    pass "Stretch mode works with images"
else
    fail "Stretch mode failed"
fi
cleanup

# Test 6: Panscan mode with image
info "Test: Panscan mode with image"
timeout 3 ./build/gslapper -v -o "panscan=0.8" DP-1 "$TEST_DIR/16x9.png" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Panscan mode:" /tmp/test-output.log; then
    pass "Panscan mode works with images"
else
    fail "Panscan mode failed"
fi
cleanup

echo ""
echo "=== Image Loading Tests ==="

# Test 7: Image pipeline creation
info "Test: Image pipeline loads correctly"
timeout 3 ./build/gslapper -v DP-1 "$TEST_DIR/16x9.png" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Loading image:" /tmp/test-output.log && grep -q "Image loaded:" /tmp/test-output.log; then
    pass "Image pipeline created and loaded frame"
else
    fail "Image pipeline failed"
    cat /tmp/test-output.log
fi
cleanup

# Test 8: Different aspect ratios
info "Test: Portrait image (9:16)"
timeout 3 ./build/gslapper -v DP-1 "$TEST_DIR/portrait.png" > /tmp/test-output.log 2>&1 &
sleep 2
if grep -q "Image loaded:" /tmp/test-output.log; then
    pass "Portrait image loaded"
else
    fail "Portrait image failed"
fi
cleanup

echo ""
echo "=== IPC Tests with Images ==="

# Test 9: IPC with image
info "Test: IPC query with image"
./build/gslapper --ipc-socket /tmp/gslapper-test.sock -v DP-1 "$TEST_DIR/16x9.png" > /tmp/test-output.log 2>&1 &
sleep 3
RESPONSE=$(echo "query" | nc -U /tmp/gslapper-test.sock 2>&1)
if echo "$RESPONSE" | grep -q "STATUS: playing.*16x9.png"; then
    pass "IPC query returns image path"
else
    fail "IPC query failed: $RESPONSE"
fi

# Test 10: IPC change between images
info "Test: IPC change from PNG to JPEG"
RESPONSE=$(echo "change $TEST_DIR/16x9.jpg" | nc -U /tmp/gslapper-test.sock 2>&1)
if echo "$RESPONSE" | grep -q "OK"; then
    pass "IPC change command accepted"
    sleep 2
    if grep -q "16x9.jpg" /tmp/test-output.log; then
        pass "Image actually changed to JPEG"
    else
        fail "Image did not change"
    fi
else
    fail "IPC change command failed: $RESPONSE"
fi

# Test 11: Preload command (stub)
info "Test: IPC preload command (stub)"
RESPONSE=$(echo "preload $TEST_DIR/portrait.png" | nc -U /tmp/gslapper-test.sock 2>&1)
if echo "$RESPONSE" | grep -q "OK"; then
    pass "IPC preload command works (stub)"
else
    fail "IPC preload failed: $RESPONSE"
fi

# Test 12: Unload command (stub)
info "Test: IPC unload command (stub)"
RESPONSE=$(echo "unload $TEST_DIR/portrait.png" | nc -U /tmp/gslapper-test.sock 2>&1)
if echo "$RESPONSE" | grep -q "OK"; then
    pass "IPC unload command works (stub)"
else
    fail "IPC unload failed: $RESPONSE"
fi

# Test 13: List command (stub)
info "Test: IPC list command (stub)"
RESPONSE=$(echo "list" | nc -U /tmp/gslapper-test.sock 2>&1)
if echo "$RESPONSE" | grep -q "PRELOADED"; then
    pass "IPC list command works (stub)"
else
    fail "IPC list failed: $RESPONSE"
fi

# Test 14: Preload rejects non-image
info "Test: Preload rejects non-image files"
RESPONSE=$(echo "preload /etc/passwd" | nc -U /tmp/gslapper-test.sock 2>&1)
if echo "$RESPONSE" | grep -q "ERROR.*not an image"; then
    pass "Preload correctly rejects non-image"
else
    fail "Preload should reject non-image: $RESPONSE"
fi

# Cleanup IPC test
echo "stop" | nc -U /tmp/gslapper-test.sock 2>&1 || true
sleep 1

echo ""
echo "=== Help Text Tests ==="

# Test 15: Help text includes image modes
info "Test: Help text documents image support"
HELP_OUTPUT=$(./build/gslapper --help 2>&1)
if echo "$HELP_OUTPUT" | grep -q "fill.*Fill screen maintaining aspect" && \
   echo "$HELP_OUTPUT" | grep -q "Image: JPEG, PNG"; then
    pass "Help text includes image documentation"
else
    fail "Help text missing image documentation"
fi

echo ""
echo "======================================"
echo " Test Results"
echo "======================================"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi
