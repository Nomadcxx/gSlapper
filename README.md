<div align="center">
  <img src="docs/assets/images/gslapper-logo.png" alt="gSlapper Logo" style="background: transparent;"/>
</div>

<br>

**gSlapper** is a high-performance wallpaper manager for Wayland that combines the best of [swww](https://github.com/Horus645/swww) and [mpvpaper](https://github.com/GhostNaN/mpvpaper). It uses GStreamer instead of libmpv, providing faster performance, better efficiency, and fixes memory leaks on NVIDIA Wayland systems while offering superior multi-monitor support.

## Quick Start

```bash
# Video wallpaper
gslapper -o "loop" DP-1 /path/to/video.mp4

# Static image wallpaper
gslapper -o "fill" DP-1 /path/to/image.jpg

# All monitors
gslapper -o "loop" '*' /path/to/video.mp4
```

## Features

- 🎬 **Video Wallpapers** - Play MP4, MKV, WebM, and other video formats
- 🖼️ **Static Images** - Support for JPEG, PNG, WebP, GIF with smooth transitions
- 🖥️ **Multi-Monitor** - Full support for multiple displays
- ⚡ **High Performance** - 10x faster than mpvpaper (236 FPS vs 23 FPS)
- 🔧 **IPC Control** - Runtime control via Unix socket
- 🎨 **Scaling Modes** - Fill, stretch, original, and panscan options
- 🌊 **Fade Transitions** - Smooth transitions between static images

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

## Documentation

📚 **Full documentation available at**: https://nomadcxx.github.io/gSlapper/

- [Installation Guide](https://nomadcxx.github.io/gSlapper/getting-started/installation/)
- [Quick Start](https://nomadcxx.github.io/gSlapper/getting-started/quick-start/)
- [Video Wallpapers](https://nomadcxx.github.io/gSlapper/user-guide/video-wallpapers/)
- [Static Images](https://nomadcxx.github.io/gSlapper/user-guide/static-images/)
- [IPC Control](https://nomadcxx.github.io/gSlapper/user-guide/ipc-control/)
- [Scaling Modes](https://nomadcxx.github.io/gSlapper/user-guide/scaling-modes/)
- [Transitions](https://nomadcxx.github.io/gSlapper/advanced/transitions/)
- [Troubleshooting](https://nomadcxx.github.io/gSlapper/advanced/troubleshooting/)

## Why GStreamer?

gSlapper uses GStreamer instead of libmpv to solve memory leaks on NVIDIA Wayland systems, improve GPU resource management, and provide more reliable multi-monitor support. GStreamer offers mature Wayland/EGL integration and proper hardware acceleration.

## License

MIT License - see [LICENSE](LICENSE)

## Acknowledgments

- [mpvpaper](https://github.com/GhostNaN/mpvpaper) - Original inspiration
- [swww](https://github.com/Horus645/swww) - Static wallpaper inspiration
- [GStreamer](https://gstreamer.freedesktop.org/) - Multimedia framework
- [Clapper](https://github.com/Rafostar/clapper) - GStreamer integration patterns
