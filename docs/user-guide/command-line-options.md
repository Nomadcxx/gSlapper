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
