#!/bin/sh
# Helper script for bundling the application on OS X

echo "Bundling Doomsday.app..."

CP="cp -fRp"

SRCDIR=$1
echo "Source directory: $SRCDIR"

BUILDDIR=engine

mv $BUILDDIR/doomsday.app $BUILDDIR/Doomsday.app

APPDIR=$BUILDDIR/Doomsday.app/Contents
echo "Bundle directory: $APPDIR"

echo "Clearing existing bundles..."
rm -rf $BUILDDIR/*.bundle

echo "Copying bundles from plugins..."
$CP plugins/dehread/dpDehRead.bundle $BUILDDIR/dpDehRead.bundle
$CP plugins/wadmapconverter/dpWadMapConverter.bundle $BUILDDIR/dpWadMapConverter.bundle
$CP plugins/jdoom/jDoom.bundle $BUILDDIR/jDoom.bundle
$CP plugins/jheretic/jHeretic.bundle $BUILDDIR/jHeretic.bundle
$CP plugins/jhexen/jHexen.bundle $BUILDDIR/jHexen.bundle
$CP plugins/jdoom64/jDoom64.bundle $BUILDDIR/jDoom64.bundle
$CP plugins/fmod/dsFMOD.bundle $BUILDDIR/dsFMOD.bundle
$CP plugins/fluidsynth/dsFluidSynth.bundle $BUILDDIR/dsFluidSynth.bundle

echo "Installing deps for dsFluidSynth..."
FWDIR=$BUILDDIR/Doomsday.app/Contents/Frameworks
cp /usr/local/lib/libglib-2.0.0.dylib $FWDIR
cp /usr/local/lib/libgthread-2.0.0.dylib $FWDIR
cp /usr/local/Cellar/gettext/0.18.1.1/lib/libintl.8.dylib $FWDIR
chmod u+w $FWDIR/libglib-2.0.0.dylib $FWDIR/libgthread-2.0.0.dylib $FWDIR/libintl.8.dylib

# IDs
install_name_tool -id @executable_path/../Frameworks/libglib-2.0.0.dylib $FWDIR/libglib-2.0.0.dylib
install_name_tool -id @executable_path/../Frameworks/libgthread-2.0.0.dylib $FWDIR/libgthread-2.0.0.dylib
install_name_tool -id @executable_path/../Frameworks/libintl.8.dylib $FWDIR/libintl.8.dylib

# glib-2.0.0
install_name_tool -change /usr/local/Cellar/gettext/0.18.1.1/lib/libintl.8.dylib \
	@executable_path/../Frameworks/libintl.8.dylib $FWDIR/libglib-2.0.0.dylib

# gthread-2.0.0
install_name_tool -change /usr/local/Cellar/glib/2.32.3/lib/libglib-2.0.0.dylib \
	@executable_path/../Frameworks/libglib-2.0.0.dylib $FWDIR/libgthread-2.0.0.dylib
install_name_tool -change /usr/local/Cellar/gettext/0.18.1.1/lib/libintl.8.dylib \
	@executable_path/../Frameworks/libintl.8.dylib $FWDIR/libgthread-2.0.0.dylib

# dsFluidSynth
DSFS=$BUILDDIR/dsFluidSynth.bundle/dsFluidSynth
install_name_tool -change /usr/local/lib/libglib-2.0.0.dylib \
	@executable_path/../Frameworks/libglib-2.0.0.dylib $DSFS
install_name_tool -change /usr/local/lib/libgthread-2.0.0.dylib \
	@executable_path/../Frameworks/libgthread-2.0.0.dylib $DSFS
install_name_tool -change /usr/local/Cellar/gettext/0.18.1.1/lib/libintl.8.dylib \
	@executable_path/../Frameworks/libintl.8.dylib $DSFS

echo "Bundling done."
