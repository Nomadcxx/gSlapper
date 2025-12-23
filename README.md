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

Full documentation available at: https://nomadcxx.github.io/gSlapper/

## Features

- **Video Wallpapers** - Play MP4, MKV, WebM, and other video formats
- **Static Images** - Support for JPEG, PNG, WebP, GIF with smooth transitions
- **Multi-Monitor** - Full support for multiple displays
- **High Performance** - 10x faster than mpvpaper (236 FPS vs 23 FPS)
- **IPC Control** - Runtime control via Unix socket
- **Scaling Modes** - Fill, stretch, original, and panscan options
- **Fade Transitions** - Smooth transitions between static images

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

## License

MIT License - see [LICENSE](LICENSE)

## Acknowledgments

- [mpvpaper](https://github.com/GhostNaN/mpvpaper) - Original inspiration
- [swww](https://github.com/Horus645/swww) - Static wallpaper inspiration
- [GStreamer](https://gstreamer.freedesktop.org/) - Multimedia framework
- [Clapper](https://github.com/Rafostar/clapper) - GStreamer integration patterns
