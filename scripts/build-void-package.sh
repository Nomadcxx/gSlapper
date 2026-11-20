#!/bin/bash
set -euo pipefail

pkgname=gslapper
tag=${1:-}
tag=${tag#v}
if [[ ! $tag =~ ^[0-9]+(\.[0-9]+)*(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$ ]]; then
    printf 'Invalid release version: %s\n' "$tag" >&2
    exit 1
fi
version=${tag%%+*}
version=${version//-/}
source_dir=$(cd "$(dirname "$0")/.." && pwd)
output=$(mkdir -p "${2:-dist/void}" && cd "${2:-dist/void}" && pwd)
work=${VOID_BUILD_DIR:-$(mktemp -d /tmp/gslapper-void.XXXXXXXX)}
repo=$work/void-packages

arch=$(xbps-uhelper arch)
case "$arch" in x86_64|x86_64-musl) ;; *) echo "Unsupported architecture: $arch" >&2; exit 1 ;; esac
mkdir "$repo"
git init -q "$repo"
# CHANGED 2026-11-20 - Build against current void-packages master instead of a pinned revision - Problem:
# binary-bootstrap installs from the live binary repo, so a pinned tree drifts behind it within weeks;
# its dependency versions are then missing as binaries and xbps-src rebuilds them from source, which
# failed on the v1.6.0 tag (bluez on glibc, tar on musl)
git -C "$repo" fetch -q --depth=1 https://github.com/void-linux/void-packages.git master
git -C "$repo" checkout -q --detach FETCH_HEAD
mkdir -p "$repo/srcpkgs/$pkgname"
cp -R "$source_dir/packaging/void/." "$repo/srcpkgs/$pkgname/"
archive=$pkgname-$version-void-source.tar.gz
git -C "$source_dir" archive --format=tar --prefix="$pkgname-$version/" HEAD | gzip -n > "$output/$archive"
checksum=$(sha256sum "$output/$archive")
checksum=${checksum%% *}
sed -i -e "s|^version=.*|version=$version|" \
    -e "s|^checksum=.*|checksum=$checksum|" \
    -e "s|^distfiles=.*|distfiles=\"https://github.com/Nomadcxx/gSlapper/releases/download/v$tag/$archive\"|" \
    "$repo/srcpkgs/$pkgname/template"
mkdir -p "$repo/hostdir/sources/$pkgname-$version"
cp "$output/$archive" "$repo/hostdir/sources/$pkgname-$version/"
printf 'XBPS_CHROOT_CMD=uchroot\nXBPS_MAKEJOBS=2\n' > "$repo/etc/conf"
cd "$repo"
./xbps-src binary-bootstrap
./xbps-src -Q pkg "$pkgname"
cp "hostdir/binpkgs/$pkgname-${version}_1.$arch.xbps" "$output/"
