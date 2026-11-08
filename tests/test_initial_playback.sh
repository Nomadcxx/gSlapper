#!/bin/bash
# Video playback must not jump back to the start shortly after it begins
# (issue #40), and a resume position must survive the loop setup.

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
TESTS_RUN=3

if [[ ! -x "$GSLAPPER" ]]; then
    echo "FAIL: gslapper binary not found at $GSLAPPER"
    exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "SKIP: ffmpeg not available to generate the test video"
    exit 0
fi

export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

if [[ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]]; then
    echo "SKIP: no Wayland display at $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
    exit 0
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

# Low frame rate matches the report: the restart happened about a second in
VIDEO="$WORK_DIR/test.mp4"
ffmpeg -loglevel error -f lavfi -i "testsrc2=size=320x240:rate=6:duration=20" \
    -f lavfi -i "sine=frequency=440:duration=20" \
    -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest "$VIDEO"

# Prints the appsink buffer timestamps (in seconds) seen during a short run
run_pts() {
    local log="$1"
    shift
    GST_DEBUG=basesink:6 GST_DEBUG_NO_COLOR=1 timeout -s TERM 7s stdbuf -oL -eL \
        "$GSLAPPER" --no-save-state "$@" -o "no-audio" '*' "$VIDEO" > "$log" 2>&1 || true
    sed -nE 's/.*gst_base_sink_chain_unlocked:<app-sink> got times start: ([0-9]+):([0-9]+):([0-9.]+),.*/\1 \2 \3/p' "$log" |
        awk '{ printf "%.3f\n", $1 * 3600 + $2 * 60 + $3 }'
}

# The 20s video is only played for ~7s, so timestamps must never go backwards
check_monotonic() {
    local label="$1" pts_file="$2"
    local count backwards
    count=$(wc -l < "$pts_file")
    backwards=$(awk 'NR > 1 && $1 < prev { print prev " -> " $1; exit } { prev = $1 }' "$pts_file")

    if [[ $count -lt 12 ]]; then
        fail "$label: only $count frames reached the sink"
    elif [[ -n "$backwards" ]]; then
        fail "$label: playback jumped back ($backwards)"
    else
        pass "$label: $count frames, timestamps never jump back"
    fi
}

run_pts "$WORK_DIR/start.log" > "$WORK_DIR/start.pts"
check_monotonic "from start" "$WORK_DIR/start.pts"

# -Z is the position the holder passes back after an auto-stop restart. The
# preroll frame from the start of the file may reach the sink before the seek.
run_pts "$WORK_DIR/resume.log" -Z "4 0" > "$WORK_DIR/resume-all.pts"
awk '$1 >= 3.5 { found = 1 } found' "$WORK_DIR/resume-all.pts" > "$WORK_DIR/resume.pts"
if [[ -s "$WORK_DIR/resume.pts" ]]; then
    pass "resume at 4s: playback reached $(head -1 "$WORK_DIR/resume.pts")s"
else
    fail "resume at 4s: playback never reached the resume position"
fi
check_monotonic "resume at 4s" "$WORK_DIR/resume.pts"

# IPC change restarts through the holder too, but the new file must start from
# the beginning rather than at the old video's position
if command -v socat >/dev/null 2>&1; then
    TESTS_RUN=$((TESTS_RUN + 1))
    SOCK="$WORK_DIR/ipc.sock"
    cp "$VIDEO" "$WORK_DIR/other.mp4"
    GST_DEBUG=basesink:6 GST_DEBUG_NO_COLOR=1 stdbuf -oL -eL "$GSLAPPER" --no-save-state \
        -s -I "$SOCK" -o "no-audio" '*' "$VIDEO" > "$WORK_DIR/change.log" 2>&1 &
    pid=$!
    sleep 5
    mark=$(wc -l < "$WORK_DIR/change.log")
    echo "change $WORK_DIR/other.mp4" | socat -t2 - "UNIX-CONNECT:$SOCK" > /dev/null 2>&1
    sleep 4
    echo "quit" | socat -t2 - "UNIX-CONNECT:$SOCK" > /dev/null 2>&1
    sleep 1
    kill -TERM "$pid" 2>/dev/null

    # Frames from the first one near the start of the new file onwards
    tail -n +"$mark" "$WORK_DIR/change.log" |
        sed -nE 's/.*gst_base_sink_chain_unlocked:<app-sink> got times start: ([0-9]+):([0-9]+):([0-9.]+),.*/\1 \2 \3/p' |
        awk '{ t = $1 * 3600 + $2 * 60 + $3 } t < 1 { found = 1 } found { printf "%.3f\n", t }' > "$WORK_DIR/change.pts"
    skip_ahead=$(awk 'NR > 1 && ($1 < prev || $1 > prev + 1) { print prev " -> " $1; exit } { prev = $1 }' "$WORK_DIR/change.pts")
    if [[ ! -s "$WORK_DIR/change.pts" ]]; then
        fail "change video: new file never started from the beginning"
    elif [[ -n "$skip_ahead" ]]; then
        fail "change video: playback skipped after the change ($skip_ahead)"
    else
        pass "change video: new file plays from the start ($(tail -1 "$WORK_DIR/change.pts")s reached)"
    fi
else
    echo "SKIP: socat not available for the IPC change check"
fi

echo "=== Results: $((TESTS_RUN - TESTS_FAILED)) passed, $TESTS_FAILED failed ==="
[[ $TESTS_FAILED -eq 0 ]]
