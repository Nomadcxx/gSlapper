#!/bin/sh
set -eu
repo=$(cd "$1" && pwd)
root=$(mktemp -d /tmp/gslapper-check.XXXXXXXX)
arch=$(xbps-uhelper arch)
export XBPS_ARCH="$arch"
set -- "$repo"/gslapper-*."$arch".xbps
test "$#" -eq 1 && test -f "$1"
expected=${1##*/}
expected=${expected%."$arch".xbps}
mirror=https://repo-default.voidlinux.org/current
[ "$arch" != x86_64-musl ] || mirror=$mirror/musl
mkdir -p "$root/var/db/xbps/keys"
cp /var/db/xbps/keys/* "$root/var/db/xbps/keys/"
xbps-install -r "$root" -R "$mirror" -Sy base-files dash coreutils
xbps-install -r "$root" -R "$mirror" -R "$repo" -Sy gslapper
xbps-pkgdb -r "$root" gslapper
test "$(xbps-query -r "$root" -p pkgver gslapper)" = "$expected"
test "$(xbps-query -r "$root" -p architecture gslapper)" = "$arch"
test "$(xbps-query -r "$root" -p license gslapper)" = GPL-3.0-only
for binary in gslapper gslapper-holder; do
    test "$(stat -c %u:%g "$root/usr/bin/$binary")" = 0:0
    chroot "$root" /usr/bin/"$binary" --help >/dev/null
done
printf 'Verified native %s gSlapper package installation.\n' "$arch"
