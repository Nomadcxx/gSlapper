#!/bin/bash
# Quick transition test - runs a few key tests and shows visual results

set -e

WALLPAPER_DIR="/home/nomadx/Pictures/wallpapers"
IPC_SOCKET="/tmp/gslapper-quick-test.sock"
GSLAPPER_BIN="./build/gslapper"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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
    echo "Error: No IPC client found"
    exit 1
fi

ipc_send() {
    if echo "$IPC_CLIENT" | grep -q "socat"; then
        echo "$1" | socat UNIX-CONNECT:"$IPC_SOCKET" - 2>/dev/null || echo ""
    else
        echo "$1" | $IPC_CLIENT "$IPC_SOCKET" 2>/dev/null || echo ""
    fi
}

cleanup() {
    pkill -9 -f "gslapper.*quick-test" 2>/dev/null || true
    rm -f "$IPC_SOCKET"
}

trap cleanup EXIT

# Get 3 random images
IMAGES=($(find "$WALLPAPER_DIR" -maxdepth 1 -type f \( \
    -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" \) | shuf -n 3))

if [ ${#IMAGES[@]} -lt 3 ]; then
    echo "Error: Need at least 3 images"
    exit 1
fi

echo -e "${BLUE}Quick Transition Test${NC}"
echo "Using images:"
for img in "${IMAGES[@]}"; do
    echo "  - $(basename "$img")"
done
echo ""

# Build if needed
if [ ! -f "$GSLAPPER_BIN" ]; then
    echo "Building gslapper..."
    cd /home/nomadx/gSlapper
    ninja -C build
fi

# Start with transitions enabled
echo -e "${YELLOW}Starting gslapper with 1.5s fade transitions...${NC}"
"$GSLAPPER_BIN" -v --transition-type fade --transition-duration 1.5 \
    -I "$IPC_SOCKET" DP-1 "${IMAGES[0]}" > /dev/null 2>&1 &

sleep 3

echo -e "${GREEN}Test 1: Basic fade transition${NC}"
echo "  Changing from $(basename "${IMAGES[0]}") to $(basename "${IMAGES[1]}")..."
ipc_send "change ${IMAGES[1]}" > /dev/null
sleep 2

echo -e "${GREEN}Test 2: Faster transition${NC}"
echo "  Setting duration to 0.5s..."
ipc_send "set-transition-duration 0.5" > /dev/null
sleep 0.5
echo "  Changing to $(basename "${IMAGES[2]}")..."
ipc_send "change ${IMAGES[2]}" > /dev/null
sleep 1

echo -e "${GREEN}Test 3: Disable transitions${NC}"
ipc_send "set-transition none" > /dev/null
sleep 0.5
echo "  Changing to $(basename "${IMAGES[0]}") (no transition)..."
ipc_send "change ${IMAGES[0]}" > /dev/null
sleep 1

echo -e "${GREEN}Test 4: Re-enable and test${NC}"
ipc_send "set-transition fade" > /dev/null
ipc_send "set-transition-duration 1.0" > /dev/null
sleep 0.5
echo "  Changing to $(basename "${IMAGES[1]}")..."
ipc_send "change ${IMAGES[1]}" > /dev/null
sleep 2

echo ""
echo -e "${BLUE}Status check:${NC}"
ipc_send "query"
ipc_send "get-transition"

echo ""
echo -e "${GREEN}Tests complete! Check your screen for visual results.${NC}"
echo "Press Enter to stop..."
read

ipc_send "stop" > /dev/null 2>&1
sleep 1
