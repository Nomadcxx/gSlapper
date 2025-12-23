# Installation

## Arch Linux

Install from AUR:

```bash
yay -S gslapper
```

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
# Runtime dependencies
sudo apt install gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
                 gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav

# Build dependencies
sudo apt install meson ninja-build wayland-protocols libunwind-dev
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

!!! important "Codec Support"
    `gst-plugins-ugly` and `gst-libav` provide codec support (H.264, H.265, etc.). Without these, videos may fail to play with "Pipeline failed to reach playing state" error.

## Verification

After installation, verify gSlapper is working:

```bash
gslapper --help
```

You should see the help message with available options.
