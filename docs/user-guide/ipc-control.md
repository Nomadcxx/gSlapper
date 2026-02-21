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

### `help`

List all available IPC commands.

```bash
echo "help" | nc -U /tmp/gslapper.sock
```

**Response:** List of all supported commands with brief descriptions.

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

### `layer <name>`

Switch gSlapper to a different Wayland layer at runtime.

Supported layer names: `background`, `bottom`, `top`, `overlay`.

```bash
echo "layer top" | nc -U /tmp/gslapper.sock
echo "layer background" | nc -U /tmp/gslapper.sock
```

**Response:** `OK` or `ERROR: <message>`

!!! note "Compositor support"
    Dynamic layer switching requires layer-shell protocol support for `set_layer` (v2+). On older compositors, this command returns an error.

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
- `ERROR: File not found: <path>` - File access error with detailed reason
- `ERROR: Permission denied: <path>` - Permission error with system details
- `ERROR: Invalid input: <reason>` - Input validation failed
- `STATUS: <state> <type> <path>` - Query response
- `TRANSITION: <type> <enabled|disabled> <duration>` - Transition query response

### Error Messages

Recent improvements provide more detailed error messages:

- **File not found**: `ERROR: File not accessible: /path/to/file.mp4 (No such file or directory)`
- **Permission denied**: `ERROR: File not accessible: /path/to/file.mp4 (Permission denied)`
- **Path too long**: `ERROR: Path too long (max 4096 characters)`
- **Invalid characters**: `ERROR: Invalid input: contains control characters`

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
    layer)
        echo "layer ${2:-top}" | nc -U "$SOCKET"
        ;;
    *)
        echo "Usage: $0 {pause|resume|next|status|transition|layer [background|bottom|top|overlay]}"
        exit 1
        ;;
esac
```

## Cache Management Commands

These commands require `--cache-size` to be enabled.

### `cache-list`

List all cached images with dimensions and sizes.

```bash
echo "cache-list" | nc -U /tmp/gslapper.sock
```

**Response:** List of cached images, one per line:
```
/path/to/image1.jpg 3840x2160 31.64 MB [*]
/path/to/image2.png 2560x1440 14.06 MB
```

The `[*]` marker indicates currently displayed image.

### `cache-stats`

Show cache usage statistics.

```bash
echo "cache-stats" | nc -U /tmp/gslapper.sock
```

**Response:** `45.70/256.00 MB (2 images)` or `Cache disabled`

### `unload <target>`

Remove images from cache. Target can be:
- `unused` - Remove all images not currently displayed
- `all` - Clear entire cache (including displayed image)
- `<path>` - Remove specific image by path

```bash
echo "unload unused" | nc -U /tmp/gslapper.sock
echo "unload all" | nc -U /tmp/gslapper.sock
echo "unload /path/to/image.jpg" | nc -U /tmp/gslapper.sock
```

**Response:** `OK: Unloaded N image(s)` or `ERROR: <message>`

### `listactive`

Show currently displayed wallpaper(s) and output(s).

```bash
echo "listactive" | nc -U /tmp/gslapper.sock
```

**Response:** `ACTIVE: <output> <type> <path>`

Example: `ACTIVE: DP-1 image /home/user/wallpaper.jpg`
