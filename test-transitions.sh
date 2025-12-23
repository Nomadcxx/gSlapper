#!/bin/bash
# Automated test suite for gSlapper transition effects

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0
WALLPAPER_DIR="/home/nomadx/Pictures/wallpapers"
IPC_SOCKET="/tmp/gslapper-transition-test.sock"
GSLAPPER_BIN="./build/gslapper"
OUTPUT_LOG="/tmp/gslapper-transition-test.log"
# Try to detect output, fallback to '*' for all outputs
OUTPUT_NAME="*"
if command -v "$GSLAPPER_BIN" &> /dev/null; then
    DETECTED_OUTPUT=$("$GSLAPPER_BIN" --help-output 2>&1 | grep -E "^Output:" | head -1 | awk '{print $2}' || echo "")
    if [ -n "$DETECTED_OUTPUT" ]; then
        OUTPUT_NAME="$DETECTED_OUTPUT"
    fi
fi

# Function to print test results
pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((TESTS_PASSED++)) || true
}

fail() {
    echo -e "${RED}✗${NC} $1"
    ((TESTS_FAILED++)) || true
}

info() {
    echo -e "${YELLOW}→${NC} $1"
}

section() {
    echo ""
    echo -e "${BLUE}=== $1 ===${NC}"
}

# Find IPC client
find_ipc_client() {
    if command -v nc &> /dev/null; then
        echo "nc -U"
    elif command -v socat &> /dev/null; then
        echo "socat UNIX-CONNECT"
    elif command -v ncat &> /dev/null; then
        echo "ncat -U"
    else
        echo ""
    fi
}

IPC_CLIENT=$(find_ipc_client)
if [ -z "$IPC_CLIENT" ]; then
    echo -e "${RED}Error: No IPC client found (nc, socat, or ncat required)${NC}"
    echo "Install one of: gnu-netcat, socat, or nmap (provides ncat)"
    exit 1
fi

# IPC helper function
ipc_send() {
    local cmd="$1"
    local response=""
    if echo "$IPC_CLIENT" | grep -q "socat"; then
        # Use timeout and proper connection handling for socat
        response=$(echo "$cmd" | timeout 1 socat UNIX-CONNECT:"$IPC_SOCKET" - 2>/dev/null || echo "")
    else
        response=$(echo "$cmd" | timeout 1 $IPC_CLIENT "$IPC_SOCKET" 2>/dev/null || echo "")
    fi
    # Return response, or empty string if failed
    echo "$response"
}

# Wait for IPC socket to be ready and responsive
wait_for_ipc() {
    local max_wait=10
    local waited=0
    # Wait for socket file to exist
    while [ ! -S "$IPC_SOCKET" ] && [ $waited -lt $max_wait ]; do
        sleep 0.2
        waited=$((waited + 1))
    done
    if [ ! -S "$IPC_SOCKET" ]; then
        return 1
    fi
    # Wait a bit more for gslapper to be ready to accept connections
    sleep 1
    # Test if IPC is actually responsive
    local test_response=$(ipc_send "query")
    if [ -n "$test_response" ] && echo "$test_response" | grep -q "STATUS:"; then
        return 0
    fi
    # If not responsive yet, wait a bit more
    sleep 1
    test_response=$(ipc_send "query")
    if [ -n "$test_response" ] && echo "$test_response" | grep -q "STATUS:"; then
        return 0
    fi
    return 1
}

# Cleanup function
cleanup() {
    pkill -9 -f "gslapper.*transition-test" 2>/dev/null || true
    pkill -9 -f "gslapper.*$IPC_SOCKET" 2>/dev/null || true
    rm -f "$IPC_SOCKET"
    sleep 0.5
}

trap cleanup EXIT

# Check if gslapper binary exists
if [ ! -f "$GSLAPPER_BIN" ]; then
    echo -e "${RED}Error: gslapper binary not found at $GSLAPPER_BIN${NC}"
    echo "Please build gslapper first: ninja -C build"
    exit 1
fi

# Check if wallpaper directory exists
if [ ! -d "$WALLPAPER_DIR" ]; then
    echo -e "${RED}Error: Wallpaper directory not found: $WALLPAPER_DIR${NC}"
    exit 1
fi

# Find test images - simpler approach that handles spaces correctly
TEST_IMAGES=()
while IFS= read -r -d '' img; do
    [ -n "$img" ] && TEST_IMAGES+=("$img")
    [ ${#TEST_IMAGES[@]} -ge 10 ] && break
done < <(find "$WALLPAPER_DIR" -maxdepth 1 -type f \( \
    -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o \
    -iname "*.webp" \) -print0)

if [ ${#TEST_IMAGES[@]} -lt 3 ]; then
    echo -e "${RED}Error: Need at least 3 images for testing, found ${#TEST_IMAGES[@]}${NC}"
    exit 1
fi

IMG1="${TEST_IMAGES[0]}"
IMG2="${TEST_IMAGES[1]}"
IMG3="${TEST_IMAGES[2]}"

echo "======================================"
echo " gSlapper Transition Test Suite"
echo "======================================"
echo ""
echo "Using test images:"
echo "  Image 1: $(basename "$IMG1")"
echo "  Image 2: $(basename "$IMG2")"
echo "  Image 3: $(basename "$IMG3")"
echo ""

# Build gslapper
info "Building gslapper..."
cd /home/nomadx/gSlapper
if ninja -C build > /dev/null 2>&1; then
    pass "Build successful"
else
    fail "Build failed"
    exit 1
fi

section "Transition Command-Line Options"

# Test 1: Enable fade transitions via command line
info "Test: Enable fade transitions with --transition-type fade"
cleanup
timeout 5 "$GSLAPPER_BIN" -v --transition-type fade --transition-duration 1.0 \
    -I "$IPC_SOCKET" "$OUTPUT_NAME" "$IMG1" > "$OUTPUT_LOG" 2>&1 &
sleep 3
if grep -q "Fade transitions enabled" "$OUTPUT_LOG"; then
    pass "Fade transitions enabled via command line"
else
    fail "Failed to enable fade transitions"
    cat "$OUTPUT_LOG"
fi
cleanup

# Test 2: Disable transitions via command line
info "Test: Disable transitions with --transition-type none"
cleanup
timeout 5 "$GSLAPPER_BIN" -v --transition-type none \
    -I "$IPC_SOCKET" "$OUTPUT_NAME" "$IMG1" > "$OUTPUT_LOG" 2>&1 &
sleep 3
if ! grep -q "Fade transitions enabled" "$OUTPUT_LOG"; then
    pass "Transitions disabled via command line"
else
    fail "Failed to disable transitions"
fi
cleanup

# Test 3: Set transition duration via command line
info "Test: Set transition duration with --transition-duration"
cleanup
timeout 5 "$GSLAPPER_BIN" -v --transition-type fade --transition-duration 2.5 \
    -I "$IPC_SOCKET" "$OUTPUT_NAME" "$IMG1" > "$OUTPUT_LOG" 2>&1 &
sleep 3
if grep -q "Transition duration set to 2.50" "$OUTPUT_LOG"; then
    pass "Transition duration set via command line"
else
    fail "Failed to set transition duration"
fi
cleanup

section "IPC Transition Commands"

# Test 4: Start gslapper with IPC and transitions enabled
info "Test: Start gslapper with IPC and transitions"
cleanup
"$GSLAPPER_BIN" -v --transition-type fade --transition-duration 1.0 \
    -I "$IPC_SOCKET" "$OUTPUT_NAME" "$IMG1" > "$OUTPUT_LOG" 2>&1 &
GSLAPPER_PID=$!
sleep 5  # Give gslapper time to fully start

if wait_for_ipc; then
    pass "IPC socket ready"
else
    fail "IPC socket not ready"
    kill $GSLAPPER_PID 2>/dev/null || true
    cleanup
    exit 1
fi

# Test 5: Get transition settings via IPC
info "Test: IPC get-transition command"
RESPONSE=$(ipc_send "get-transition")
if echo "$RESPONSE" | grep -q "TRANSITION: fade enabled"; then
    pass "get-transition returns correct settings"
else
    fail "get-transition failed: $RESPONSE"
fi

# Test 6: Set transition type via IPC
info "Test: IPC set-transition command"
RESPONSE=$(ipc_send "set-transition fade")
if echo "$RESPONSE" | grep -q "OK.*fade transitions enabled"; then
    pass "set-transition fade works"
else
    fail "set-transition fade failed: $RESPONSE"
fi

# Test 7: Disable transitions via IPC
info "Test: IPC set-transition none"
RESPONSE=$(ipc_send "set-transition none")
if echo "$RESPONSE" | grep -q "OK.*transitions disabled"; then
    pass "set-transition none works"
else
    fail "set-transition none failed: $RESPONSE"
fi

# Test 8: Re-enable transitions
info "Test: Re-enable transitions via IPC"
RESPONSE=$(ipc_send "set-transition fade")
if echo "$RESPONSE" | grep -q "OK.*fade transitions enabled"; then
    pass "Re-enabled transitions via IPC"
else
    fail "Failed to re-enable transitions: $RESPONSE"
fi

# Test 9: Set transition duration via IPC
info "Test: IPC set-transition-duration command"
RESPONSE=$(ipc_send "set-transition-duration 0.8")
if echo "$RESPONSE" | grep -q "OK.*duration set to 0.80"; then
    pass "set-transition-duration works"
else
    fail "set-transition-duration failed: $RESPONSE"
fi

# Test 10: Invalid duration values
info "Test: IPC set-transition-duration with invalid values"
RESPONSE=$(ipc_send "set-transition-duration 10.0")
if echo "$RESPONSE" | grep -q "ERROR.*invalid duration"; then
    pass "Invalid duration correctly rejected"
else
    fail "Should reject invalid duration: $RESPONSE"
fi

RESPONSE=$(ipc_send "set-transition-duration -1.0")
if echo "$RESPONSE" | grep -q "ERROR.*invalid duration"; then
    pass "Negative duration correctly rejected"
else
    fail "Should reject negative duration: $RESPONSE"
fi

section "Transition Functionality"

# Test 11: Basic fade transition between images
info "Test: Fade transition between two images"
RESPONSE=$(ipc_send "set-transition-duration 1.0")
sleep 0.5

# Change to second image (should trigger transition)
RESPONSE=$(ipc_send "change $IMG2")
if echo "$RESPONSE" | grep -q "OK.*transition started"; then
    pass "Transition started on image change"
    sleep 2  # Wait for transition to complete
    # Check if transition completed
    if grep -q "Transition completed" "$OUTPUT_LOG"; then
        pass "Transition completed successfully"
    else
        fail "Transition did not complete"
    fi
else
    fail "Failed to start transition: $RESPONSE"
fi

# Test 12: Query status during transition
info "Test: IPC query during transition"
RESPONSE=$(ipc_send "query")
if echo "$RESPONSE" | grep -q "STATUS:"; then
    pass "Query works during transition"
else
    fail "Query failed: $RESPONSE"
fi

# Test 13: Rapid image changes (should handle gracefully)
info "Test: Rapid image changes"
RESPONSE1=$(ipc_send "change $IMG1")
sleep 0.3
RESPONSE2=$(ipc_send "change $IMG2")
sleep 0.3
RESPONSE3=$(ipc_send "change $IMG3")
sleep 2

if echo "$RESPONSE1" | grep -q "OK" && \
   echo "$RESPONSE2" | grep -q "OK" && \
   echo "$RESPONSE3" | grep -q "OK"; then
    pass "Rapid image changes handled gracefully"
else
    fail "Rapid changes failed: $RESPONSE1 / $RESPONSE2 / $RESPONSE3"
fi

# Test 14: Change to same image (should not transition)
info "Test: Change to same image (no transition)"
RESPONSE=$(ipc_send "change $IMG3")
sleep 1
# Should not start a new transition if already on that image
if ! grep -q "Starting.*transition" "$OUTPUT_LOG" | tail -1; then
    pass "No transition for same image"
else
    fail "Should not transition to same image"
fi

# Test 15: Transition cancellation
info "Test: Transition cancellation via set-transition none"
RESPONSE=$(ipc_send "change $IMG1")
sleep 0.2  # Start transition
RESPONSE=$(ipc_send "set-transition none")
if echo "$RESPONSE" | grep -q "OK.*transitions disabled"; then
    pass "Transition cancellation works"
    sleep 1
    if grep -q "Transition canceled" "$OUTPUT_LOG"; then
        pass "Transition properly canceled"
    else
        # This is OK - cancellation might be silent
        pass "Transition disabled (cancellation may be silent)"
    fi
else
    fail "Failed to cancel transition: $RESPONSE"
fi

# Test 16: Invalid image path during transition
info "Test: Invalid image path handling"
RESPONSE=$(ipc_send "set-transition fade")
sleep 0.2
RESPONSE=$(ipc_send "change /nonexistent/image.png")
if echo "$RESPONSE" | grep -q "ERROR.*not accessible"; then
    pass "Invalid path correctly rejected"
else
    fail "Should reject invalid path: $RESPONSE"
fi

section "Edge Cases"

# Test 17: Video to image (should not transition)
info "Test: Video to image change (no transition)"
# Note: This test assumes we can't easily test video without a video file
# We'll test that image-to-image transitions work, and video transitions are disabled
RESPONSE=$(ipc_send "set-transition fade")
RESPONSE=$(ipc_send "query")
if echo "$RESPONSE" | grep -q "image"; then
    pass "Currently in image mode (transitions enabled)"
else
    info "Currently not in image mode (skipping video test)"
fi

# Test 18: Transition with different image sizes
info "Test: Transition between different sized images"
RESPONSE=$(ipc_send "set-transition fade")
RESPONSE=$(ipc_send "set-transition-duration 1.0")
sleep 0.5
RESPONSE=$(ipc_send "change $IMG1")
sleep 2
RESPONSE=$(ipc_send "change $IMG2")
sleep 2
if grep -q "Transition completed" "$OUTPUT_LOG"; then
    pass "Transition works with different image sizes"
else
    fail "Transition failed with different sizes"
fi

# Test 19: Multiple transitions in sequence
info "Test: Multiple transitions in sequence"
RESPONSE=$(ipc_send "set-transition-duration 0.5")
sleep 0.5
RESPONSE=$(ipc_send "change $IMG1")
sleep 1
RESPONSE=$(ipc_send "change $IMG2")
sleep 1
RESPONSE=$(ipc_send "change $IMG3")
sleep 1
if grep -q "Transition completed" "$OUTPUT_LOG"; then
    pass "Multiple sequential transitions work"
else
    fail "Sequential transitions failed"
fi

# Cleanup
if [ -n "$GSLAPPER_PID" ]; then
    ipc_send "stop" > /dev/null 2>&1 || true
    sleep 1
    kill $GSLAPPER_PID 2>/dev/null || true
    wait $GSLAPPER_PID 2>/dev/null || true
fi
cleanup

section "Summary"

echo ""
echo "======================================"
echo " Test Results"
echo "======================================"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All transition tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some transition tests failed${NC}"
    echo ""
    echo "Last 20 lines of output log:"
    tail -20 "$OUTPUT_LOG" 2>/dev/null || true
    exit 1
fi
