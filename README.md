<div align="center">
  <img src="gSlapper.png" alt="gSlapper Logo" width="400"/>
</div>

<br>

A drop-in replacement for [mpvpaper](https://github.com/GhostNaN/mpvpaper) using GStreamer instead of libmpv. Faster, more efficient, and fixes memory leaks on NVIDIA Wayland systems while providing better multi-monitor support.

## Installation

### Arch Linux
```bash
yay -S gslapper
```

### Manual Build
```bash
git clone https://github.com/Nomadcxx/gSlapper.git
cd gSlapper
meson setup build --prefix=/usr/local
ninja -C build
sudo ninja -C build install
```

### Dependencies
```bash
# Arch Linux (runtime)
sudo pacman -S gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav

# Arch Linux (build)
sudo pacman -S meson ninja wayland-protocols

# Ubuntu/Debian (runtime)
sudo apt install gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
                 gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav

# Ubuntu/Debian (build)
sudo apt install meson ninja-build wayland-protocols libunwind-dev
```

**Note:** `gst-plugins-ugly` and `gst-libav` provide codec support (H.264, H.265, etc.). Without these, videos may fail to play with "Pipeline failed to reach playing state" error.

## Usage

### Video Wallpapers

```bash
# Basic usage
gslapper DP-1 /path/to/video.mp4

# With looping and scaling
gslapper -v -o "loop panscan=0.8" DP-1 /path/to/video.mp4

# Stretch to fill (ignore aspect ratio)
gslapper -v -o "loop stretch" DP-1 /path/to/video.mp4

# Original resolution (1:1 pixel mapping)
gslapper -v -o "loop original" DP-1 /path/to/video.mp4

# All monitors
gslapper -v -o "loop" '*' /path/to/video.mp4

# Different videos per monitor
gslapper -o "loop" DP-1 /path/to/video1.mp4 &
gslapper -o "loop" DP-3 /path/to/video2.mp4 &
```

### Static Image Wallpapers

gSlapper supports static images (JPEG, PNG, WebP, GIF) with automatic format detection:

```bash
# Basic image wallpaper (defaults to fill mode)
gslapper DP-1 /path/to/wallpaper.jpg

# With different scaling modes
gslapper -o "fill" DP-1 /path/to/image.png       # Fill screen, crop excess (default)
gslapper -o "stretch" DP-1 /path/to/image.png    # Fill screen ignoring aspect ratio
gslapper -o "original" DP-1 /path/to/image.png   # Display at native resolution
gslapper -o "panscan=0.8" DP-1 /path/to/image.png # Fit inside with scaling

# Image on all monitors
gslapper -o "fill" '*' /path/to/wallpaper.jpg
```

**Scaling Modes:**
- `fill` - Fill screen maintaining aspect ratio, crop excess (default for images)
- `stretch` - Fill screen ignoring aspect ratio
- `original` - Display at native resolution (1:1 pixel mapping)
- `panscan=X` - Fit inside screen with scaling factor 0.0-1.0 (default for video)

**Supported Formats:**
- Video: MP4, MKV, WebM, AVI, MOV, and other GStreamer-supported formats
- Image: JPEG, PNG, WebP, GIF

### Options

- `-v, -vv` - Verbose output (use -vv for more detail)
- `-f` - Fork to background
- `-p` - Auto-pause when wallpaper is hidden
- `-s` - Auto-stop when wallpaper is hidden
- `-l LAYER` - Shell layer (background, bottom, top, overlay)
- `-I, --ipc-socket <path>` - Enable IPC control via Unix socket
- `-o "OPTIONS"` - Video options (space-separated):
  - `loop` - Seamless video looping
  - `fill` - Fill screen maintaining aspect ratio, crop excess (default for images)
  - `panscan=X` - Scale video (0.0-1.0, default 1.0 = fit to screen)
  - `stretch` - Stretch to fill screen (ignore aspect ratio)
  - `original` - Display at native resolution (1:1 pixel mapping)
  - `no-audio` - Disable audio playback

### IPC Control

gSlapper supports runtime control via Unix domain socket:

```bash
# Start with IPC enabled
gslapper --ipc-socket /tmp/gslapper.sock -o "loop" DP-1 video.mp4

# Send commands from another terminal
echo "pause" | nc -U /tmp/gslapper.sock
echo "resume" | nc -U /tmp/gslapper.sock
echo "query" | nc -U /tmp/gslapper.sock
echo "change /path/to/other.mp4" | nc -U /tmp/gslapper.sock
echo "stop" | nc -U /tmp/gslapper.sock

# Image preloading commands (coming soon)
echo "preload /path/to/image.jpg" | nc -U /tmp/gslapper.sock
echo "unload /path/to/image.jpg" | nc -U /tmp/gslapper.sock
echo "list" | nc -U /tmp/gslapper.sock
```

**Commands:**
- `pause` - Pause video playback
- `resume` - Resume video playback
- `query` - Get current state and video path
- `change <path>` - Switch to different video/image
- `stop` - Stop gslapper
- `preload <path>` - Preload image into cache (coming soon)
- `unload <path>` - Remove image from cache (coming soon)
- `list` - List preloaded images (coming soon)

**Responses:**
- `OK` - Command succeeded
- `ERROR: <message>` - Command failed
- `STATUS: <playing|paused> <path>` - Query response
- `PRELOADED: <list>` - List response

## Configuration

Uses mpvpaper config files:
- `~/.config/mpvpaper/pauselist` - Apps that pause playback
- `~/.config/mpvpaper/stoplist` - Apps that stop playback

Compatible with Hyprland, Sway, and wlroots compositors.

## Why GStreamer?

gSlapper replaces libmpv with GStreamer to solve memory leaks on NVIDIA Wayland systems, improve GPU resource management, and provide more reliable multi-monitor support. GStreamer offers mature Wayland/EGL integration and proper hardware acceleration.

## License

MIT License - see [LICENSE](LICENSE)

## Acknowledgments

- [mpvpaper](https://github.com/GhostNaN/mpvpaper) - Original inspiration
- [GStreamer](https://gstreamer.freedesktop.org/) - Multimedia framework
- [Clapper](https://github.com/Rafostar/clapper) - GStreamer integration patterns
