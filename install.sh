#!/bin/bash

# gSlapper Installation Script
# High-performance drop-in replacement for mpvpaper

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Check if running as root for system installation
check_root() {
    if [[ $EUID -eq 0 ]]; then
        INSTALL_PREFIX="/usr"
        print_info "Installing system-wide to /usr"
    else
        INSTALL_PREFIX="$HOME/.local"
        print_info "Installing to user directory: $HOME/.local"
        mkdir -p "$HOME/.local/bin"
    fi
}

# Check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for required packages
    if ! pkg-config --exists gstreamer-1.0; then
        missing_deps+=("gstreamer")
    fi
    
    if ! pkg-config --exists gstreamer-video-1.0; then
        missing_deps+=("gst-plugins-base")
    fi
    
    if ! pkg-config --exists wayland-client; then
        missing_deps+=("wayland")
    fi
    
    if ! command -v meson &> /dev/null; then
        missing_deps+=("meson")
    fi
    
    if ! command -v ninja &> /dev/null; then
        missing_deps+=("ninja")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_info "On Arch Linux, install with:"
        print_info "sudo pacman -S gstreamer gst-plugins-base gst-plugins-good wayland meson ninja"
        exit 1
    fi
    
    print_success "All dependencies found"
}

# Build the project
build_project() {
    print_info "Building gSlapper..."
    
    # Clean any existing build directory
    if [ -d "build" ]; then
        rm -rf build
    fi
    
    # Configure build
    meson setup build --prefix="$INSTALL_PREFIX" --buildtype=release
    
    # Build
    ninja -C build
    
    print_success "Build completed successfully"
}

# Install the project
install_project() {
    print_info "Installing gSlapper..."
    
    # Install binaries
    ninja -C build install
    
    print_success "Installation completed successfully"
}

# Optional mpvpaper compatibility symlink
create_mpvpaper_symlink() {
    print_info "Would you like to create a mpvpaper compatibility symlink? (y/N)"
    read -r response
    
    if [[ "$response" =~ ^[Yy]$ ]]; then
        if [[ $EUID -eq 0 ]]; then
            if [ ! -e "/usr/bin/mpvpaper" ]; then
                ln -sf /usr/bin/gslapper /usr/bin/mpvpaper
                print_success "Created mpvpaper compatibility symlink at /usr/bin/mpvpaper"
                print_info "You can now use 'mpvpaper' command to run gSlapper"
            else
                print_warning "mpvpaper already exists at /usr/bin/mpvpaper"
                print_info "Use 'gslapper' command instead, or remove existing mpvpaper first"
            fi
        else
            if [ ! -e "$HOME/.local/bin/mpvpaper" ]; then
                ln -sf "$HOME/.local/bin/gslapper" "$HOME/.local/bin/mpvpaper"
                print_success "Created mpvpaper compatibility symlink at ~/.local/bin/mpvpaper"
                print_info "You can now use 'mpvpaper' command to run gSlapper"
            else
                print_warning "mpvpaper already exists in ~/.local/bin"
                print_info "Use 'gslapper' command instead, or remove existing mpvpaper first"
            fi
        fi
    else
        print_info "Skipped mpvpaper symlink creation"
        print_info "Use 'gslapper' command to run the application"
    fi
}

# Test installation
test_installation() {
    print_info "Testing installation..."
    
    local gslapper_path
    if [[ $EUID -eq 0 ]]; then
        gslapper_path="/usr/bin/gslapper"
    else
        gslapper_path="$HOME/.local/bin/gslapper"
    fi
    
    if [ -x "$gslapper_path" ]; then
        print_success "gSlapper installed successfully"
        print_info "Test with: gslapper --help"
    else
        print_error "Installation verification failed"
    fi
}

# Print usage information
print_usage() {
    print_info "gSlapper Installation Complete!"
    echo
    print_info "Usage examples:"
    echo "  gslapper -vs -o \"loop panscan=1.0\" DP-1 /path/to/video.mp4"
    echo "  gslapper -vs -o \"loop panscan=1.0\" '*' /path/to/video.mp4"
    echo
    print_info "gSlapper is available alongside mpvpaper, offering:"
    echo "  • Better NVIDIA Wayland compatibility"
    echo "  • Improved multi-monitor support"
    echo "  • Memory leak prevention"
    echo "  • Enhanced hardware acceleration"
    echo "  • 10x better performance (236 FPS vs 23 FPS)"
    echo
    print_info "For more information, see: README.md"
}

# Main installation process
main() {
    print_info "gSlapper Installation Script"
    print_info "============================"
    
    check_root
    check_dependencies
    build_project
    install_project
    test_installation
    
    echo
    create_mpvpaper_symlink
    
    echo
    print_usage
    
    print_success "Installation completed successfully!"
}

# Run main function
main "$@"