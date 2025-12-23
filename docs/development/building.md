# Building from Source

This guide covers building gSlapper from source for development and customization.

## Prerequisites

### Arch Linux

```bash
# Runtime dependencies
sudo pacman -S gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav

# Build dependencies
sudo pacman -S meson ninja wayland-protocols
```

### Ubuntu/Debian

```bash
# Runtime dependencies
sudo apt install gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
                 gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav

# Build dependencies
sudo apt install meson ninja-build wayland-protocols libunwind-dev
```

### Fedora/RHEL

```bash
# Runtime dependencies
sudo dnf install gstreamer1 gstreamer1-plugins-base gstreamer1-plugins-good \
                 gstreamer1-plugins-bad gstreamer1-plugins-ugly gstreamer1-libav

# Build dependencies
sudo dnf install meson ninja-build wayland-protocols-devel
```

## Building

### Standard Build

```bash
git clone https://github.com/Nomadcxx/gSlapper.git
cd gSlapper
meson setup build --prefix=/usr/local --buildtype=release
ninja -C build
sudo ninja -C build install
```

### Debug Build

For development with debug symbols and verbose logging:

```bash
meson setup build --buildtype=debug
ninja -C build
./build/gslapper --help
```

### Custom Installation Prefix

```bash
meson setup build --prefix=/usr
ninja -C build
sudo ninja -C build install
```

## Build Options

### Available Options

Check available build options:

```bash
meson configure build
```

### Common Options

```bash
# Set custom prefix
meson setup build --prefix=/opt/gslapper

# Enable/disable features (if available)
meson setup build -Dfeature=enabled

# Set build type
meson setup build --buildtype=release  # or debug, debugoptimized
```

## Testing the Build

After building, test the binary:

```bash
# Check help output
./build/gslapper --help

# List available outputs
./build/gslapper -d

# Test with a video
./build/gslapper -v DP-1 /path/to/video.mp4

# Test with an image
./build/gslapper -v DP-1 /path/to/image.jpg
```

## Rebuilding

After making code changes:

```bash
# Quick rebuild
ninja -C build

# Clean rebuild
rm -rf build
meson setup build --prefix=/usr/local --buildtype=release
ninja -C build
```

## Troubleshooting

### Missing Codecs

If videos fail to play, ensure codec plugins are installed:

```bash
# Arch Linux
sudo pacman -S gst-plugins-ugly gst-libav

# Ubuntu/Debian
sudo apt install gstreamer1.0-plugins-ugly gstreamer1.0-libav
```

### Wayland Protocol Errors

If you see protocol errors, ensure wayland-protocols is installed:

```bash
# Arch Linux
sudo pacman -S wayland-protocols

# Ubuntu/Debian
sudo apt install wayland-protocols
```

### GStreamer Not Found

If meson can't find GStreamer:

```bash
# Check pkg-config
pkg-config --modversion gstreamer-1.0

# Install development packages
# Arch Linux
sudo pacman -S gstreamer gst-plugins-base

# Ubuntu/Debian
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

## Development Workflow

1. **Make changes** to source files in `src/`
2. **Rebuild** with `ninja -C build`
3. **Test** with `./build/gslapper`
4. **Debug** with `-vv` flag for verbose output
5. **Iterate** as needed

## Package Building

For Arch Linux package building:

```bash
# Build package
makepkg -si

# Update .SRCINFO after PKGBUILD changes
makepkg --printsrcinfo > .SRCINFO
```

See [PKGBUILD](../PKGBUILD) for package configuration.
