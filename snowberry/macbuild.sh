#!/bin/sh

VERSION=$1
if test ! $VERSION; then
    echo "Must define version as argument."
    exit
fi
echo "Building version "$VERSION
echo $VERSION > VERSION

# Locations.
DENG_DIR=/Users/jaakko/Projects/deng/doomsday
TARGET_IMAGE=images/deng-$VERSION.dmg

# Clean.
chmod -R u+w build dist
rm -rf build dist
rm $TARGET_IMAGE

# Copy the resource files into the correct place.
mkdir -p build/addons
mkdir -p build/conf
mkdir -p build/graphics
mkdir -p build/lang
mkdir -p build/profiles
mkdir -p build/plugins

cp conf/{osx-appearance.conf,components.conf,osx-doomsday.conf,snowberry.conf} build/conf
cp graphics/{*.jpg,*.png,*.bmp,*.ico} build/graphics
cp lang/english.lang build/lang
cp profiles/*.prof build/profiles
cp plugins/tab*.py build/plugins
cp -R plugins/{example.plugin,about.py,help.py,launcher.py,preferences.py,profilelist.py,wizard.py} build/plugins

# Build the bundle.
python buildapp.py py2app

# Place Doomsday.app inside the bundle.
cp -R $DENG_DIR/Build/Deployment/Doomsday.app \
  $DENG_DIR/Build/Deployment/*.bundle "dist/Doomsday Engine.app/Contents"

# Make sure the readme is included in the image.
cp $DENG_DIR/Readme.rtf dist

# Create a disk image.
hdiutil create -srcfolder dist -volname "Doomsday Engine ${VERSION}" -nouuid -noanyowners $TARGET_IMAGE
