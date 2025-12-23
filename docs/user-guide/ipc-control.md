# IPC Control

gSlapper supports runtime control via Unix domain sockets, allowing you to control playback from scripts or other applications.

## Enabling IPC

Start gSlapper with the `-I` or `--ipc-socket` option:

```bash
gslapper -I /tmp/gslapper.sock -o "loop" DP-1 video.mp4
```

## Sending Commands

Use `nc` (netcat) or `socat` to send commands:

```bash
# Using nc
echo "pause" | nc -U /tmp/gslapper.sock

# Using socat
echo "pause" | socat - UNIX-CONNECT:/tmp/gslapper.sock
```

## Available Commands

### `pause`

Pause video playback.

```bash
echo "pause" | nc -U /tmp/gslapper.sock
```

**Response:** `OK`

### `resume`

Resume paused playback.

```bash
echo "resume" | nc -U /tmp/gslapper.sock
```

**Response:** `OK`

### `query`

Get current state and wallpaper path.

```bash
echo "query" | nc -U /tmp/gslapper.sock
```

**Response:** `STATUS: playing image /path/to/wallpaper.jpg` or `STATUS: paused video /path/to/video.mp4`

### `change <path>`

Switch to a different wallpaper.

```bash
echo "change /path/to/new/video.mp4" | nc -U /tmp/gslapper.sock
```

**Response:** `OK: transition started` (if transitions enabled) or `OK`

### `stop`

Stop gSlapper.

```bash
echo "stop" | nc -U /tmp/gslapper.sock
```

**Response:** `OK` (gSlapper exits)

### `set-transition <type>`

Set transition effect type. Options: `none`, `fade`.

```bash
echo "set-transition fade" | nc -U /tmp/gslapper.sock
echo "set-transition none" | nc -U /tmp/gslapper.sock
```

**Response:** `OK` or `ERROR: <message>`

!!! note "Transitions"
    Transitions only work between static images. Videos always use instant switch regardless of transition settings.

### `set-transition-duration <seconds>`

Set transition duration in seconds (0.0-5.0).

```bash
echo "set-transition-duration 2.0" | nc -U /tmp/gslapper.sock
```

**Response:** `OK` or `ERROR: <message>`

### `get-transition`

Query current transition settings.

```bash
echo "get-transition" | nc -U /tmp/gslapper.sock
```

**Response:** `TRANSITION: <type> <enabled|disabled> <duration>`

Example: `TRANSITION: fade enabled 1.50`

## Response Format

- `OK` - Command succeeded
- `OK: <message>` - Command succeeded with additional info
- `ERROR: <message>` - Command failed with error message
- `STATUS: <state> <type> <path>` - Query response
- `TRANSITION: <type> <enabled|disabled> <duration>` - Transition query response

## Example Script

```bash
#!/bin/bash
SOCKET="/tmp/gslapper.sock"

case "$1" in
    pause)
        echo "pause" | nc -U "$SOCKET"
        ;;
    resume)
        echo "resume" | nc -U "$SOCKET"
        ;;
    next)
        echo "change /path/to/next/video.mp4" | nc -U "$SOCKET"
        ;;
    status)
        echo "query" | nc -U "$SOCKET"
        ;;
    transition)
        echo "set-transition fade" | nc -U "$SOCKET"
        echo "set-transition-duration 2.0" | nc -U "$SOCKET"
        ;;
    *)
        echo "Usage: $0 {pause|resume|next|status|transition}"
        exit 1
        ;;
esac
```

## Future Commands

The following commands are planned for future releases:

- `preload <path>` - Preload image into cache
- `unload <path>` - Remove image from cache
- `list` - List preloaded images
