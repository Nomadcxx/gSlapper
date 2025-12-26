# Command Line Options

## Basic Options

### `-v, --verbose`

Enable verbose output. Use `-vv` for more detail.

```bash
gslapper -v DP-1 video.mp4
gslapper -vv DP-1 video.mp4  # More verbose
```

### `-f, --fork`

Fork to background after starting.

```bash
gslapper -f -o "loop" DP-1 video.mp4
```

### `-d, --help-output`

Display all available outputs and quit.

```bash
gslapper -d
```

### `-h, --help`

Display help message.

```bash
gslapper --help
```

## Playback Control

### `-p, --auto-pause`

Automatically pause when wallpaper is hidden.

```bash
gslapper -p -o "loop" DP-1 video.mp4
```

### `-s, --auto-stop`

Automatically stop when wallpaper is hidden.

```bash
gslapper -s -o "loop" DP-1 video.mp4
```

## Display Options

### `-l, --layer LAYER`

Specify shell surface layer. Options:

- `background` (default)
- `bottom`
- `top`
- `overlay`

```bash
gslapper -l overlay DP-1 video.mp4
```

## IPC Options

### `-I, --ipc-socket PATH`

Enable IPC control via Unix socket.

```bash
gslapper -I /tmp/gslapper.sock DP-1 video.mp4
```

## Video Options

### `-o, --gst-options "OPTIONS"`

Pass options to GStreamer. Space-separated list:

- `loop` - Seamless video looping
- `fill` - Fill screen maintaining aspect ratio (default for images)
- `panscan=X` - Scale video (0.0-1.0, default 1.0)
- `stretch` - Stretch to fill screen (ignore aspect ratio)
- `original` - Display at native resolution
- `no-audio` - Disable audio playback

```bash
gslapper -o "loop panscan=0.8" DP-1 video.mp4
```

## Transition Options

### `--transition-type TYPE`

Set transition effect type. Options:

- `none` (default)
- `fade`

```bash
gslapper --transition-type fade -I /tmp/sock DP-1 image.jpg
```

### `--transition-duration SECS`

Set transition duration in seconds (default: 0.5).

```bash
gslapper --transition-type fade --transition-duration 2.0 -I /tmp/sock DP-1 image.jpg
```

## Systemd Options

### `-S, --systemd`

Enable systemd service mode. Enables systemd readiness notifications.

```bash
gslapper -S --restore DP-1 /path/to/video.mp4
```

**Use Case**: Only needed when running as systemd user service. The service files (`gslapper.service`, `gslapper@.service`) automatically include this flag.

### `-R, --restore`

Restore wallpaper from saved state file.

```bash
# Restore for specific output
gslapper -R DP-1

# Restore for all monitors
gslapper -R '*'
```

**Behavior**: Loads state from `~/.local/state/gslapper/state-<output>.txt` (or `state.txt` for `'*')` and resumes wallpaper with saved video position and pause state.

## State Management Options

### `--save-state`

Save current wallpaper state and exit.

```bash
gslapper -o "loop" DP-1 /path/to/video.mp4
# Later, save state
gslapper --save-state
```

**Use Case**: Manual state save without stopping gSlapper. Also available via IPC: `echo "save-state" | nc -U /tmp/sock`.

### `--state-file <path>`

Use custom state file path (instead of default).

```bash
gslapper --state-file /tmp/my-state.txt --restore DP-1
```

**Default Location**: `~/.local/state/gslapper/state-<output>.txt` (or `$XDG_STATE_HOME/gslapper/` if set).

### `--no-save-state`

Disable automatic state saving on exit.

```bash
gslapper --no-save-state -o "loop" DP-1 /path/to/video.mp4
```

**Use Case**: Testing or temporary wallpapers where you don't want state saved.

## Examples

```bash
# Basic video with looping
gslapper -o "loop" DP-1 video.mp4

# Image with fill mode and IPC
gslapper -o "fill" -I /tmp/sock DP-1 image.jpg

# Video with auto-pause and verbose output
gslapper -v -p -o "loop" DP-1 video.mp4

# Background mode with transitions
gslapper -f --transition-type fade --transition-duration 1.5 -I /tmp/sock DP-1 image.jpg
```
