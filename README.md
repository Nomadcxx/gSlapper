<div align="center">
  <img src="docs/assets/images/gslapper-logo.png" alt="gSlapper Logo" style="background: transparent;"/>
</div>

<br>

**gSlapper** is a Wayland wallpaper utility that plays both videos and static images. Uses GStreamer instead of libmpv, which fixes memory leaks on NVIDIA systems.

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

- Play videos (MP4, MKV, WebM) and images (JPEG, PNG, WebP, GIF)
- Fade transitions between images
- Multi-monitor support
- IPC control via Unix socket (pause, resume, change wallpaper)
- Scaling modes: fill, stretch, original, panscan
- Fixes NVIDIA memory leaks that mpvpaper has

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

### Nix / NixOS

```bash
git clone https://github.com/Nomadcxx/gSlapper.git
cd gSlapper
nix build
./result/bin/gslapper --help
```

For development:

```bash
nix develop
```

See [Nix Installation Guide](docs/nix-installation.md) for system integration options.

## License

GPL-3.0 License - see [LICENSE](LICENSE)

## Credits

- [mpvpaper](https://github.com/GhostNaN/mpvpaper)
- [swww](https://github.com/Horus645/swww)
- [GStreamer](https://gstreamer.freedesktop.org/)
- [Clapper](https://github.com/Rafostar/clapper)
