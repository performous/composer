#!/bin/bash -e
# Creates a simple zip package.

source pathconfig.sh

ARCHIVE=editor.7z

ARCHIVER=`which 7z`
if [ $? -ne 0 ]; then
	echo "Couldn't find 7z, cannot create archive."
	exit 1
fi

cd "$BUILD_DIR"
rm -rf "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR"

# No install target yet
cp *.exe "$INSTALL_DIR"

# Copy DLLs
../copydlls.py "$CROSS_PREFIX/$CROSS_ID/bin" "$INSTALL_DIR"

# These ones are not detected by copydlls.py
cp "$CROSS_PREFIX/$CROSS_ID/bin/QtSvg4.dll" "$INSTALL_DIR"
cp -r "$CROSS_PREFIX/$CROSS_ID/plugins/phonon_backend" "$INSTALL_DIR"

# Create archive
cd "$INSTALL_DIR"
$ARCHIVER a -bd -r "$BUILD_DIR/$ARCHIVE" *

