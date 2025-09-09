# gSlapper

A replacement for [mpvpaper](https://github.com/GhostNaN/mpvpaper) that uses GStreamer instead of libmpv as the backend. gSlapper maintains command-line interface compatibility with [mpvpaper](https://github.com/GhostNaN/mpvpaper) while addressing memory leak issues on Wayland systems, particularly with NVIDIA drivers and multi-monitor setups.

## Why gSlapper?

### NVIDIA Wayland Compatibility
- **Memory Leak Resolution**: Addresses memory leaks that can occur with [mpvpaper](https://github.com/GhostNaN/mpvpaper) on NVIDIA Wayland systems during extended use
- **GPU Resource Management**: Improved cleanup of GPU decoder resources
- **Hardware Acceleration**: GStreamer integration for NVIDIA GPU utilization

### Multi-Monitor Support
- **Stable Multi-Output**: Improved handling of dual/triple monitor configurations
- **Per-Monitor Resource Allocation**: Independent resource management per display output
- **Scaling Support**: Video scaling and positioning across different resolution displays

## Performance Comparison

Based on our testing setup (NVIDIA RTX system, dual monitors, 60-minute test):

| Metric | gSlapper | [mpvpaper](https://github.com/GhostNaN/mpvpaper) | Improvement |
|--------|----------|-----------|-------------|
| Average FPS | 60.0 | 6.2 | ~10x better |
| Frame Drops | 0 | Frequent | Eliminated |
| GPU Memory Growth | +47MB | +101MB | 53% less |
| GPU Decoder Usage | 4-6% | 0% (software) | Hardware accelerated |

**Note**: Performance may vary based on your hardware configuration, video files, and system setup. We encourage running your own benchmarks using the included script.

### Run Your Own Benchmark

We've included a comprehensive benchmarking script to test performance on your setup:

```bash
# Run 5-minute benchmark (recommended for quick testing)
./benchmark.sh -t 300

# Run full 30-minute benchmark (comprehensive testing)
./benchmark.sh -t 1800

# Benchmark specific video file
./benchmark.sh -t 300 -v /path/to/your/video.mp4
```

The script will automatically:
- Test both gSlapper and [mpvpaper](https://github.com/GhostNaN/mpvpaper) with the same settings
- Monitor CPU, GPU usage, memory consumption, and frame rates
- Generate a CSV report with detailed metrics
- Provide a performance comparison summary

**Requirements for benchmarking:**
- Both gSlapper and [mpvpaper](https://github.com/GhostNaN/mpvpaper) installed
- NVIDIA GPU (for GPU metrics)
- Test video file (script includes sample if none provided)

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
- GStreamer 1.0 (gstreamer-1.0, gstreamer-video-1.0, gstreamer-gl-1.0)
- Wayland libraries (wayland-client, wayland-egl, wayland-protocols)
- EGL/GLES libraries
- Meson build system

## Usage

gSlapper is a **replacement** for [mpvpaper](https://github.com/GhostNaN/mpvpaper). You can replace `mpvpaper` with `gslapper` in your existing scripts and configurations.

### Basic Usage
```bash
# Same syntax as mpvpaper
gslapper -vs -o "loop panscan=1.0" DP-1 /path/to/video.mp4
gslapper -vs -o "loop panscan=1.0" DP-3 /path/to/video.mp4
```

### Multi-Monitor Example
```bash
# Apply video wallpaper to all monitors
gslapper -vs -o "loop panscan=1.0" '*' /path/to/video.mp4

# Apply to specific monitors
gslapper -vs -o "loop panscan=1.0" DP-1 /path/to/main_display.mp4 &
gslapper -vs -o "loop panscan=1.0" DP-3 /path/to/side_display.mp4 &
```

### Command Line Options
All [mpvpaper](https://github.com/GhostNaN/mpvpaper) command line options are supported:
- `-v, -vv`: Verbose output levels
- `-s`: Fork into background
- `-l`: Layer (background, bottom, top, overlay)
- `-o "OPTIONS"`: Pass options to GStreamer backend
- `-p`: Stop other instances
- `--help`: Show help information

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

### Resource Management
- **Memory Leak Prevention**: Improved cleanup of GPU and system memory resources
- **EGL Context Lifecycle**: OpenGL context management for stable rendering
- **Process Monitoring**: Built-in monitoring system prevents resource accumulation

## Configuration

gSlapper uses the same configuration files as [mpvpaper](https://github.com/GhostNaN/mpvpaper):
- `~/.config/mpvpaper/pauselist`: Applications that pause video playback
- `~/.config/mpvpaper/stoplist`: Applications that stop video playback

## Troubleshooting

### NVIDIA Issues
If experiencing issues on NVIDIA systems:
```bash
# Enable GStreamer debug output
GST_DEBUG=2 gslapper [options]

# Check NVIDIA driver status
nvidia-smi

# Verify Wayland EGL support
eglinfo | grep -i nvidia
```

### Wayland Compositor Compatibility
Tested and working with:
- Hyprland
- Sway  
- wlroots-based compositors

## Development

### Building from Source
```bash
# Configure debug build
meson setup build --buildtype=debug
ninja -C build

# Run with debug output
GST_DEBUG=2 ./build/gslapper [options]
```

### Architecture
- **Main Process**: Wayland client with layer shell integration
- **GStreamer Pipeline**: Hardware-accelerated video decoding and rendering
- **Holder Process**: Background monitoring and process management
- **EGL Integration**: OpenGL rendering backend for video output

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please ensure:
- Maintain [mpvpaper](https://github.com/GhostNaN/mpvpaper) command-line compatibility
- Test on both NVIDIA and AMD systems
- Verify multi-monitor functionality
- Include appropriate test cases

## Acknowledgments

We gratefully acknowledge the following projects and developers:

- **[mpvpaper](https://github.com/GhostNaN/mpvpaper)** - The original video wallpaper application that inspired gSlapper. Created by GhostNaN and contributors.
- **[GStreamer](https://gstreamer.freedesktop.org/)** - The powerful multimedia framework that powers gSlapper's backend. Developed by the GStreamer team and community.
- **[Clapper](https://github.com/Rafostar/clapper)** - Inspiration for GStreamer integration patterns and Wayland video rendering approaches. Created by Rafostar.
- **Wayland Protocol Developers** - For the layer shell protocol and EGL integration that enables seamless wallpaper rendering.
- **NVIDIA and Mesa Teams** - For graphics driver development that enables hardware-accelerated video processing.

gSlapper builds upon the excellent work of these projects and their maintainers. Without their contributions to the open source ecosystem, gSlapper would not be possible.