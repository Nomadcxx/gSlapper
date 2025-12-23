# gSlapper Documentation

<div align="center">
  <img src="assets/images/gslapper-logo.png" alt="gSlapper Logo" style="background: transparent;"/>
</div>

<br>

**gSlapper** is a drop-in replacement for [mpvpaper](https://github.com/GostNaN/mpvpaper) using GStreamer instead of libmpv. Faster, more efficient, and fixes memory leaks on NVIDIA Wayland systems while providing better multi-monitor support.

## Quick Links

- [Installation](getting-started/installation.md) - Get gSlapper installed on your system
- [Quick Start](getting-started/quick-start.md) - Get up and running in minutes
- [Video Wallpapers](user-guide/video-wallpapers.md) - Set up animated wallpapers
- [Static Images](user-guide/static-images.md) - Use images as wallpapers
- [IPC Control](user-guide/ipc-control.md) - Control gSlapper at runtime
- [Troubleshooting](advanced/troubleshooting.md) - Common issues and solutions

## Key Features

- ✅ **Video Wallpapers** - Play MP4, MKV, WebM, and other video formats
- ✅ **Static Images** - Support for JPEG, PNG, WebP, GIF
- ✅ **Fade Transitions** - Smooth transitions between wallpapers
- ✅ **Multi-Monitor** - Full support for multiple displays
- ✅ **IPC Control** - Runtime control via Unix socket
- ✅ **Scaling Modes** - Fill, stretch, original, and panscan options
- ✅ **Wayland Native** - Built for wlroots compositors (Hyprland, Sway, etc.)

## Why GStreamer?

gSlapper replaces libmpv with GStreamer to solve memory leaks on NVIDIA Wayland systems, improve GPU resource management, and provide more reliable multi-monitor support. GStreamer offers mature Wayland/EGL integration and proper hardware acceleration.

## License

MIT License - see [LICENSE](../LICENSE)

## Acknowledgments

- [mpvpaper](https://github.com/GhostNaN/mpvpaper) - Original inspiration
- [GStreamer](https://gstreamer.freedesktop.org/) - Multimedia framework
- [Clapper](https://github.com/Rafostar/clapper) - GStreamer integration patterns
