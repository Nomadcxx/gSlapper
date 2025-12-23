#!/bin/bash
# Interactive wrapper for testing gSlapper transitions

set -e

WALLPAPER_DIR="/home/nomadx/Pictures/wallpapers"
IPC_SOCKET="/tmp/gslapper-transition-test.sock"
GSLAPPER_BIN="./build/gslapper"

# Colors
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
    echo "Error: No IPC client found (nc, socat, or ncat required)"
    exit 1
fi

# IPC helper
ipc_send() {
    if echo "$IPC_CLIENT" | grep -q "socat"; then
        echo "$1" | socat UNIX-CONNECT:"$IPC_SOCKET" - 2>/dev/null || echo ""
    else
        echo "$1" | $IPC_CLIENT "$IPC_SOCKET" 2>/dev/null || echo ""
    fi
}

# Cleanup
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    pkill -9 -f "gslapper.*transition-test" 2>/dev/null || true
    rm -f "$IPC_SOCKET"
}

trap cleanup EXIT

# Check binary
if [ ! -f "$GSLAPPER_BIN" ]; then
    echo -e "${RED}Error: gslapper binary not found${NC}"
    exit 1
fi

# Find images
IMAGES=($(find "$WALLPAPER_DIR" -maxdepth 1 -type f \( \
    -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o \
    -iname "*.webp" -o -iname "*.jxl" \) | head -10))

if [ ${#IMAGES[@]} -lt 3 ]; then
    echo "Error: Need at least 3 images"
    exit 1
fi

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  gSlapper Transition Interactive Test${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Available images:"
for i in "${!IMAGES[@]}"; do
    echo "  [$i] $(basename "${IMAGES[$i]}")"
done
echo ""

# Start gslapper
echo -e "${YELLOW}Starting gslapper with transitions enabled...${NC}"
"$GSLAPPER_BIN" -v --transition-type fade --transition-duration 1.0 \
    -I "$IPC_SOCKET" DP-1 "${IMAGES[0]}" > /tmp/gslapper-transition.log 2>&1 &

# Wait for IPC
sleep 3
if [ ! -S "$IPC_SOCKET" ]; then
    echo "Error: IPC socket not ready"
    exit 1
fi

echo -e "${GREEN}gSlapper is running!${NC}"
echo ""
echo "Commands:"
echo "  change <n>     - Change to image number n"
echo "  duration <sec>  - Set transition duration"
echo "  status         - Get current status"
echo "  enable         - Enable fade transitions"
echo "  disable        - Disable transitions"
echo "  quit           - Exit"
echo ""

# Interactive loop
while true; do
    echo -ne "${BLUE}gslapper> ${NC}"
    read -r cmd arg
    
    case "$cmd" in
        change)
            if [ -z "$arg" ]; then
                echo "Usage: change <image_number>"
                continue
            fi
            if [ "$arg" -ge 0 ] && [ "$arg" -lt ${#IMAGES[@]} ]; then
                echo -e "${YELLOW}Changing to: $(basename "${IMAGES[$arg]}")${NC}"
                RESPONSE=$(ipc_send "change ${IMAGES[$arg]}")
                echo "$RESPONSE"
            else
                echo "Invalid image number (0-$((${#IMAGES[@]} - 1)))"
            fi
            ;;
        duration)
            if [ -z "$arg" ]; then
                echo "Usage: duration <seconds>"
                continue
            fi
            RESPONSE=$(ipc_send "set-transition-duration $arg")
            echo "$RESPONSE"
            ;;
        status)
            RESPONSE=$(ipc_send "query")
            echo "$RESPONSE"
            TRANSITION=$(ipc_send "get-transition")
            echo "$TRANSITION"
            ;;
        enable)
            RESPONSE=$(ipc_send "set-transition fade")
            echo "$RESPONSE"
            ;;
        disable)
            RESPONSE=$(ipc_send "set-transition none")
            echo "$RESPONSE"
            ;;
        quit|exit)
            echo "Stopping gslapper..."
            ipc_send "stop" > /dev/null 2>&1
            sleep 1
            break
            ;;
        help)
            echo "Commands:"
            echo "  change <n>     - Change to image number n"
            echo "  duration <sec> - Set transition duration"
            echo "  status         - Get current status"
            echo "  enable         - Enable fade transitions"
            echo "  disable        - Disable transitions"
            echo "  quit           - Exit"
            ;;
        *)
            if [ -n "$cmd" ]; then
                echo "Unknown command: $cmd (type 'help' for commands)"
            fi
            ;;
    esac
done
