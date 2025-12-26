# Maintainer: Nomadcxx <noovie@gmail.com>
pkgname=gslapper
pkgver=1.3.0
pkgrel=1
pkgdesc="Wallpaper utility for Wayland that natively handles video and static wallpapers (designed to replace mpvpaper and swww)"
arch=('x86_64')
url="https://github.com/Nomadcxx/gSlapper"
license=('MIT')
depends=('gstreamer' 'gst-plugins-base' 'gst-plugins-good' 'gst-plugins-bad' 'wayland')
makedepends=('meson' 'ninja' 'wayland-protocols')
optdepends=('gst-plugins-ugly: additional codec support'
            'gst-libav: FFmpeg-based codec support')
source=("${pkgname}-${pkgver}.tar.gz::${url}/archive/v${pkgver}.tar.gz")
sha256sums=('8a8f38ddca90c49d277329131f5687a92b2e236a3599660c66f7cae4cf1371e1')
install=${pkgname}.install

prepare() {
    cd "${srcdir}/gSlapper-${pkgver}"
}

build() {
    cd "${srcdir}/gSlapper-${pkgver}"
    meson setup build --prefix=/usr --buildtype=release
    ninja -C build
}

package() {
    cd "${srcdir}/gSlapper-${pkgver}"
    DESTDIR="${pkgdir}" ninja -C build install

    # Install license
    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"

    # Install documentation
    install -Dm644 README.md "${pkgdir}/usr/share/doc/${pkgname}/README.md"
    
    # Install systemd user service
    install -Dm644 gslapper.service \
        "${pkgdir}/usr/lib/systemd/user/gslapper.service"
}
