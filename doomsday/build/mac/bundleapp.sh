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

echo "Bundling done."
