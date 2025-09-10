<div align="center">
  <img src="gSlapper.png" alt="gSlapper Logo" width="400"/>
</div>

<br>

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

## Performance Results

Verified benchmark data from comprehensive testing (NVIDIA RTX system, dual monitors, 1-hour test sessions):

| Performance Metric | gSlapper | mpvpaper | Improvement |
|-------------------|----------|-----------|-------------|
| Frame Rate | 236.2 FPS | 23.5 FPS | ✅ 10.1x higher |
| Working GPU Memory | 1668.0MB | 1887.9MB | ✅ 12% less usage |
| Memory Stability | Stable after 5min | Stable after 5min | ✅ Both stable (no leaks) |
| Hardware Acceleration | NVDEC (12.8% avg) | Software only (0%) | ✅ GPU accelerated |
| Frame Drops | 0 | 0 | ✅ Both stable |
| CPU Usage | 0.0% | 0.0% | ✅ Equal efficiency |
| System RAM | 4.4MB | 5.4MB | ✅ 19% less usage |

**Note**: Extended testing shows both applications allocate memory at startup then remain stable - no memory leaks detected. Benchmark conducted using 4K video content ([benchmark-test-video.mp4](benchmark-test-video.mp4)). Performance varies by hardware configuration, video format, and system load. Run the included [benchmark.sh](benchmark.sh) script for testing on your specific setup.

### Run Your Own Benchmark

Use the included [benchmark.sh](benchmark.sh) script:

```bash
# Run 5-minute benchmark (recommended for quick testing)
./benchmark.sh benchmark-test-video.mp4 -t 300

# Run full 1-hour benchmark (comprehensive testing)
./benchmark.sh benchmark-test-video.mp4 -t 3600
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

**Core Change**: gSlapper replaces libmpv with GStreamer, solving critical issues:

**Problems with libmpv**:
- Memory leaks on NVIDIA Wayland systems
- Poor GPU decoder resource cleanup
- Multi-monitor instability and crashes

**gSlapper and GStreamer Benefits**:
- **gSlapper's Architecture**: Custom video sink with direct EGL rendering integration
- **GStreamer Foundation**: Mature Wayland integration with proper EGL lifecycle management
- **gSlapper's NVIDIA Integration**: Optimized pipeline configuration for NVDEC/NVENC hardware acceleration
- **gSlapper's Multi-Monitor Design**: Independent pipeline management per monitor preventing resource conflicts
- **Combined Result**: Predictable memory allocation patterns eliminating accumulation issues

**Result**: Eliminates the resource accumulation and stability issues that plague libmpv on NVIDIA systems.

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please maintain command-line compatibility and test on both NVIDIA and AMD systems.

## Acknowledgments

- **[mpvpaper](https://github.com/GhostNaN/mpvpaper)** - The original video wallpaper application that inspired gSlapper. Created by GhostNaN and contributors.
- **[GStreamer](https://gstreamer.freedesktop.org/)** - The powerful multimedia framework that powers gSlapper's backend.
- **[Clapper](https://github.com/Rafostar/clapper)** - Inspiration for GStreamer integration patterns and Wayland video rendering approaches.
