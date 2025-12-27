# gSlapper Documentation

<div align="center">
  <img src="assets/images/gslapper-logo.png" alt="gSlapper Logo" style="background: transparent;"/>
</div>

<br>

**gSlapper** is a wallpaper utility for Wayland that combines the best of [swww](https://github.com/Horus645/swww) and [mpvpaper](https://github.com/GhostNaN/mpvpaper) by allowing both static and video wallpapers. It uses GStreamer instead of libmpv making it more efficient and NVIDIA friendly for Wayland.

## Quick Start

```bash
# Video wallpaper
gslapper -o "loop" DP-1 /path/to/video.mp4

# Static image wallpaper
gslapper -o "fill" DP-1 /path/to/image.jpg

# All monitors
gslapper -o "loop" '*' /path/to/video.mp4
```

## Documentation

- [Installation](getting-started/installation.md)
- [User Guide](user-guide/index.md)
- [Command Line Options](user-guide/command-line-options.md)
- [IPC Control](user-guide/ipc-control.md)
- [Systemd Service](systemd-service-setup.md)
- [Troubleshooting](advanced/troubleshooting.md)

## Features

- **Video & Image Support** - Play videos (MP4, MKV, WebM) and display static images (JPEG, PNG, WebP, GIF)
- **Instant Switching** - RAM cache for near-instant wallpaper changes
- **Smooth Transitions** - Fade transitions between static images
- **Multi-Monitor** - Independent wallpaper control for each display
- **IPC Control** - Runtime control via Unix domain socket (pause, resume, change wallpaper)
- **Flexible Scaling** - Fill, stretch, original, and panscan modes
- **NVIDIA Optimized** - Fixes memory leaks and improves compatibility on NVIDIA Wayland systems

## License

MIT License - see [LICENSE](../LICENSE)

## Acknowledgments

- [mpvpaper](https://github.com/GhostNaN/mpvpaper) - Original inspiration
- [swww](https://github.com/Horus645/swww) - Static wallpaper inspiration
- [GStreamer](https://gstreamer.freedesktop.org/) - Multimedia framework
- [Clapper](https://github.com/Rafostar/clapper) - GStreamer integration patterns
