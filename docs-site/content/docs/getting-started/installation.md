---
title: Installation
description: "Install gSlapper on Arch Linux, Debian, Ubuntu, Fedora, Nix, or from source."
---

## Arch Linux

Install from AUR:

```bash
yay -S gslapper
```

## Debian / Ubuntu / Fedora Packages

Prebuilt packages are attached to every [GitHub release](https://github.com/Nomadcxx/gSlapper/releases/latest). Pick the one matching your distro:

| Distro | Package |
|---|---|
| Debian 13 (and Ubuntu 25.x) | `gslapper_<version>_debian13_amd64.deb` |
| Ubuntu 24.04 | `gslapper_<version>_ubuntu24.04_amd64.deb` |
| Fedora 42+ | `gslapper-<version>-1.fedora42.x86_64.rpm` |

```bash
# Debian / Ubuntu
sudo apt install ./gslapper_<version>_<distro>_amd64.deb

# Fedora
sudo dnf install ./gslapper-<version>-1.fedora42.x86_64.rpm
```

The packages install the `gslapper` and `gslapper-holder` binaries plus the systemd user services, and pull in all runtime dependencies. For extra codec support (H.264/H.265 via FFmpeg), also install `gstreamer1.0-libav`/`gstreamer1.0-plugins-ugly` (deb) or `gstreamer1-libav` from RPM Fusion (Fedora).

## Manual Build

### Prerequisites

**Arch Linux:**
```bash
# Runtime dependencies
sudo pacman -S gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav

# Build dependencies
sudo pacman -S meson ninja wayland-protocols
```

**Ubuntu/Debian:**
```bash
# Build dependencies
sudo apt install meson ninja-build pkg-config wayland-protocols libwayland-dev \
                 libegl-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libsystemd-dev

# Runtime GStreamer plugins
sudo apt install gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
                 gstreamer1.0-plugins-ugly gstreamer1.0-libav
```

**Fedora:**
```bash
# Build dependencies
sudo dnf install meson ninja-build gcc pkgconf-pkg-config wayland-devel \
                 wayland-protocols-devel libglvnd-devel gstreamer1-devel \
                 gstreamer1-plugins-base-devel systemd-devel

# Runtime GStreamer plugins
sudo dnf install gstreamer1-plugins-good gstreamer1-plugins-bad-free
```

### Build Steps

```bash
git clone https://github.com/Nomadcxx/gSlapper.git
cd gSlapper
meson setup build --prefix=/usr/local
ninja -C build
sudo ninja -C build install
```

## Codec Support

> [!IMPORTANT] **Codec Support**
> `gst-plugins-ugly` and `gst-libav` provide codec support (H.264, H.265, etc.). Without these, videos may fail to play with "Pipeline failed to reach playing state" error.
>
## Verification

After installation, verify gSlapper is working:

```bash
gslapper --help
```

You should see the help message with available options.
