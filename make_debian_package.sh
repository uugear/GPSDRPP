#!/bin/bash

VERSION=$(sed -n 's/#define VERSION_STR "\(.*\)"/\1/p' core/src/version.h)
DIRECTORY="gpsdrpp_$VERSION-1_arm64"

# Create directory structure
echo 'Create directory structure'
mkdir $DIRECTORY
mkdir "$DIRECTORY/DEBIAN"

# Create package info
echo "Create package info for GPSDR++ v$VERSION"
echo 'Package: gpsdrpp' >> "$DIRECTORY/DEBIAN/control"
echo "Version: $VERSION" >> "$DIRECTORY/DEBIAN/control"
echo 'Maintainer: UUGear' >> "$DIRECTORY/DEBIAN/control"
echo 'Architecture: arm64' >> "$DIRECTORY/DEBIAN/control"
echo 'Description: Receiver software for VU GPSDR, based on SDR++' >> "$DIRECTORY/DEBIAN/control"
echo 'Depends: libglfw3,libvolk2-bin,librtlsdr0,librtaudio6' >> "$DIRECTORY/DEBIAN/control"

# Create postinst script
echo 'Copy postinst script'
cp postinst "$DIRECTORY/DEBIAN/postinst"

# Copying files
ORIG_DIR=$PWD
cd ./build
make install DESTDIR="$ORIG_DIR/$DIRECTORY"
cd $ORIG_DIR

# Create package
echo 'Create package'
dpkg-deb --build "$DIRECTORY"

# Cleanup
echo 'Cleanup'
rm -rf $DIRECTORY
