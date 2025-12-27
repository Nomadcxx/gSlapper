<div align="center">
  <img src="docs/assets/images/gslapper-logo.png" alt="gSlapper Logo" style="background: transparent;"/>
</div>

<br>

**gSlapper** is a wallpaper utility for wayland that combines the best of [swww](https://github.com/Horus645/swww) and [mpvpaper](https://github.com/GhostNaN/mpvpaper) by allowing both static and video wallpapers. It uses GStreamer instead of libmpv making it more efficient and NVIDIA friendly for wayland.

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

- **Video & Image Support** - Play videos (MP4, MKV, WebM) and display static images (JPEG, PNG, WebP, GIF)
- **Smooth Transitions** - Fade transitions between static images
- **Multi-Monitor** - Independent wallpaper control for each display
- **High Performance** - Efficient GPU resource management with smart texture management and frame rate limiting
- **IPC Control** - Runtime control via Unix domain socket (pause, resume, change wallpaper)
- **Flexible Scaling** - Fill, stretch, original, and panscan modes for perfect display
- **NVIDIA Optimized** - Fixes memory leaks and improves compatibility on NVIDIA Wayland systems

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
