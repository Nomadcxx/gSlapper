# gSlapper

A replacement for [mpvpaper](https://github.com/GhostNaN/mpvpaper) that uses GStreamer instead of libmpv as the backend. gSlapper maintains command-line interface compatibility while addressing memory leak issues on Wayland systems, particularly with NVIDIA drivers and multi-monitor setups.

## Why gSlapper?

### NVIDIA Wayland Compatibility
- **Memory Leak Resolution**: Addresses memory leaks that can occur on NVIDIA Wayland systems during extended use
- **GPU Resource Management**: Improved cleanup of GPU decoder resources
- **Hardware Acceleration**: GStreamer integration for NVIDIA GPU utilization

### Multi-Monitor Support
- **Stable Multi-Output**: Improved handling of dual/triple monitor configurations
- **Per-Monitor Resource Allocation**: Independent resource management per display output
- **Scaling Support**: Video scaling and positioning across different resolution displays

## Performance Comparison

Based on our testing setup (NVIDIA RTX system, dual monitors, 60-minute test):

| Metric | gSlapper | mpvpaper | Improvement |
|--------|----------|-----------|-------------|
| Average FPS | 60.0 | 6.2 | ~10x better |
| Frame Drops | 0 | Frequent | Eliminated |
| GPU Memory Growth | +47MB | +101MB | 53% less |
| GPU Decoder Usage | 4-6% | 0% (software) | Hardware accelerated |

**Note**: Performance may vary based on your hardware configuration, video files, and system setup. We encourage running your own benchmarks using the included script.

### Run Your Own Benchmark

```bash
# Run 5-minute benchmark (recommended for quick testing)
./benchmark.sh -t 300

# Run full 30-minute benchmark (comprehensive testing)
./benchmark.sh -t 1800
```

The script will:
- Test both gSlapper and mpvpaper with identical settings
- Generate detailed performance comparison reports

**Requirements**: Both gSlapper and mpvpaper must be installed.

## Installation

### Arch Linux (AUR)
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

## Dependencies

You will need to install:
- **gstreamer**: Core multimedia framework
- **gst-plugins-base**: Essential GStreamer plugins
- **gst-plugins-good**: Additional codec support
- **gst-plugins-bad**: Advanced codec support (optional but recommended)

On Arch Linux:
```bash
sudo pacman -S gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad
```

On Ubuntu/Debian:
```bash
sudo apt install gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad
```

## Usage

### Basic Usage
```bash
# Display video on specific monitor
gslapper DP-1 /path/to/video.mp4

# Verbose output with background fork
gslapper -vs DP-1 /path/to/video.mp4

# Loop video with proper scaling (panscan=1.0 fills screen while maintaining aspect ratio)
gslapper -vs -o "loop panscan=1.0" DP-1 /path/to/video.mp4
```

### Multi-Monitor Examples
```bash
# Apply same video to all monitors
gslapper -vs -o "loop panscan=1.0" '*' /path/to/video.mp4

# Different videos per monitor
gslapper -vs -o "loop panscan=1.0" DP-1 /path/to/main_display.mp4 &
gslapper -vs -o "loop panscan=1.0" DP-3 /path/to/side_display.mp4 &

# Apply to multiple specific monitors
gslapper -vs DP-1,DP-3 /path/to/video.mp4
```

### Layer Examples
```bash
# Run on background layer (default)
gslapper -l background DP-1 /path/to/video.mp4

# Run on bottom layer (above background)
gslapper -l bottom DP-1 /path/to/video.mp4

# Run on top layer (above windows)
gslapper -l top DP-1 /path/to/video.mp4
```

### Advanced GStreamer Options
```bash
# Disable audio completely
gslapper -o "no-audio loop" DP-1 /path/to/video.mp4

# Custom scaling and positioning
gslapper -o "loop panscan=0.5" DP-1 /path/to/video.mp4

# Hardware acceleration hints
gslapper -o "loop hardware-acceleration" DP-1 /path/to/video.mp4
```

### Command Line Options
- `-v, -vv`: Verbose output (use -vv for maximum verbosity)
- `-s`: Fork into background (daemon mode)
- `-l LAYER`: Specify layer (background, bottom, top, overlay)
- `-o "OPTIONS"`: Pass options to GStreamer backend
- `-p`: Stop other instances before starting
- `--help`: Show comprehensive help information

## Features

### Core Functionality
- **Seamless Video Looping**: Advanced segment-based looping prevents gaps between video loops
- **Multi-Output Support**: Native handling of multiple monitors with independent video streams
- **Layer Shell Integration**: Uses wlr-layer-shell protocol for proper wallpaper rendering
- **Background Process Management**: Automatic process monitoring and revival capabilities

### GStreamer Backend Advantages
- **Automatic Codec Selection**: Intelligent codec detection and hardware acceleration
- **Robust Pipeline Management**: Advanced error handling and state recovery
- **Extensive Format Support**: Supports all formats available in your GStreamer installation
- **Plugin Ecosystem**: Access to the full GStreamer plugin ecosystem

## Configuration

gSlapper uses the same configuration files as mpvpaper:
- `~/.config/mpvpaper/pauselist`: Applications that pause video playback
- `~/.config/mpvpaper/stoplist`: Applications that stop video playback

Works on Hyprland, Sway, and wlroots-based compositors.

## Development

### Technical Differences from mpvpaper

**Core Architecture Changes:**
gSlapper replaces libmpv with GStreamer as the multimedia backend. This fundamental change addresses several critical issues:

**libmpv + Wayland + NVIDIA Problems:**
- **Memory Leak Issues**: libmpv's Wayland implementation has documented memory leaks on NVIDIA systems, particularly with EGL context management during extended playback sessions
- **Resource Cleanup**: Poor cleanup of GPU decoder resources leads to memory accumulation over time
- **Multi-Monitor Instability**: libmpv struggles with multiple EGL contexts across different monitors, causing crashes and resource conflicts

**GStreamer Advantages:**
- **Native Wayland Support**: GStreamer has mature, well-tested Wayland integration with proper EGL lifecycle management
- **NVIDIA Hardware Acceleration**: Direct NVDEC/NVENC integration without the problematic libmpv abstraction layer
- **Resource Management**: Explicit resource control with proper cleanup hooks and memory management
- **Multi-Monitor Architecture**: Each output gets independent pipeline management, preventing cross-contamination

**Technical Implementation:**
- **Pipeline Architecture**: Uses GStreamer's playbin element with custom video sink for direct EGL rendering
- **Threading Model**: Separates multimedia processing from Wayland event handling, preventing deadlocks
- **Memory Management**: Explicit EGL context lifecycle management with proper cleanup on surface destruction
- **Hardware Acceleration**: Direct GStreamer plugin selection for optimal codec and acceleration path selection

**Why This Solves NVIDIA Issues:**
- **Driver Integration**: GStreamer's NVIDIA plugins are actively maintained and optimized for Wayland
- **Memory Patterns**: Predictable allocation/deallocation patterns prevent the accumulation issues seen with libmpv
- **Context Management**: Proper EGL context sharing between monitors without resource conflicts

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please maintain command-line compatibility and test on both NVIDIA and AMD systems.

## Acknowledgments

- **[mpvpaper](https://github.com/GhostNaN/mpvpaper)** - The original video wallpaper application that inspired gSlapper. Created by GhostNaN and contributors.
- **[GStreamer](https://gstreamer.freedesktop.org/)** - The powerful multimedia framework that powers gSlapper's backend.
- **[Clapper](https://github.com/Rafostar/clapper)** - Inspiration for GStreamer integration patterns and Wayland video rendering approaches.