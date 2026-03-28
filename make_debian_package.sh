#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

VERSION="$(sed -n 's/^#define VERSION_STR "\(.*\)"$/\1/p' core/src/version.h)"
if [[ -z "$VERSION" ]]; then
    echo "Failed to read VERSION_STR from core/src/version.h" >&2
    exit 1
fi

PKG_NAME="gpsdrpp"
PKG_RELEASE="${PKG_RELEASE:-1}"
DEB_VERSION="${VERSION}-${PKG_RELEASE}"
ARCH="$(dpkg --print-architecture)"
BUILD_DIR="$SCRIPT_DIR/build-deb"
STAGE_DIR="$SCRIPT_DIR/.pkgroot"
PKG_BASENAME="${PKG_NAME}_${DEB_VERSION}_${ARCH}"
OUTPUT_DEB="$SCRIPT_DIR/${PKG_BASENAME}.deb"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}"

cleanup() {
    rm -rf "$BUILD_DIR" "$STAGE_DIR"
}
trap cleanup EXIT

rm -rf "$BUILD_DIR" "$STAGE_DIR" "$OUTPUT_DEB"
mkdir -p "$BUILD_DIR" "$STAGE_DIR/DEBIAN"

echo "==> Configuring CMake in a clean build directory"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

echo "==> Building GPSDR++"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "==> Installing into package staging tree"
DESTDIR="$STAGE_DIR" cmake --install "$BUILD_DIR"

if [[ -f "$SCRIPT_DIR/postinst" ]]; then
    echo "==> Installing maintainer script: postinst"
    install -m 0755 "$SCRIPT_DIR/postinst" "$STAGE_DIR/DEBIAN/postinst"
fi

INSTALLED_SIZE="$({
    find "$STAGE_DIR" -mindepth 1 -maxdepth 1 ! -name DEBIAN -print0 \
        | xargs -0r du -sk 2>/dev/null \
        | awk '{sum += $1} END {print sum + 0}'
} || echo 0)"

echo "==> Writing DEBIAN/control"
cat > "$STAGE_DIR/DEBIAN/control" <<CONTROL
Package: ${PKG_NAME}
Version: ${DEB_VERSION}
Section: utils
Priority: optional
Maintainer: UUGear
Homepage: https://github.com/uugear/GPSDRPP
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_SIZE}
Depends: libglfw3,libvolk2-bin,librtlsdr0,librtaudio6,libopengl0
Description: Receiver software for VU GPSDR, based on SDR++
CONTROL

echo "==> Building ${OUTPUT_DEB##*/}"
if dpkg-deb --help 2>&1 | grep -q -- '--root-owner-group'; then
    dpkg-deb --build --root-owner-group "$STAGE_DIR" "$OUTPUT_DEB"
elif command -v fakeroot >/dev/null 2>&1; then
    fakeroot dpkg-deb --build "$STAGE_DIR" "$OUTPUT_DEB"
else
    echo "Warning: neither dpkg-deb --root-owner-group nor fakeroot is available." >&2
    echo "Warning: packaged file ownership may reflect the current user." >&2
    dpkg-deb --build "$STAGE_DIR" "$OUTPUT_DEB"
fi

echo "==> Package created: $OUTPUT_DEB"
