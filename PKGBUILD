# Maintainer: Nomadcxx <noovie@gmail.com>
pkgname=gslapper
pkgver=1.2.0
pkgrel=1
pkgdesc="High-performance drop-in replacement for mpvpaper using GStreamer backend with panscan and original resolution features"
arch=('x86_64')
url="https://github.com/Nomadcxx/gSlapper"
license=('MIT')
depends=(
    'gstreamer'
    'gst-plugins-base'
    'gst-plugins-good'
    'gst-plugins-bad'
    'wayland'
    'wayland-protocols'
    'mesa'
    'glibc'
)
makedepends=(
    'meson'
    'ninja'
    'wayland-protocols'
    'wayland-scanner'
)
optdepends=(
    'gst-plugins-ugly: additional codec support'
    'gst-libav: FFmpeg-based codec support'
    'nvidia-utils: NVIDIA hardware acceleration'
    'mesa-vdpau: AMD/Intel hardware acceleration'
)
provides=('mpvpaper')
conflicts=('mpvpaper')
source=("$pkgname-$pkgver.tar.gz::https://github.com/Nomadcxx/gSlapper/archive/v$pkgver.tar.gz")
sha256sums=('f16a7f641d14379166d37eb112093d2fbd9cc084616fadf2031e9f830278348c')

prepare() {
    cd "$pkgname-$pkgver"
    # Any preparation steps if needed
}

build() {
    cd "$pkgname-$pkgver"
    
    # Configure meson build
    arch-meson build \
        --prefix=/usr \
        --buildtype=release
    
    # Build the project
    meson compile -C build
}

check() {
    cd "$pkgname-$pkgver"
    # Run any tests if available
    # meson test -C build
}

package() {
    cd "$pkgname-$pkgver"
    
    # Install the main application
    meson install -C build --destdir "$pkgdir"
    
    # Install documentation
    install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
    
    # Create mpvpaper compatibility symlink
    ln -s /usr/bin/gslapper "$pkgdir/usr/bin/mpvpaper"
    
    # Install license
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}