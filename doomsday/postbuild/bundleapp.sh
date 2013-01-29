#!/bin/sh
# Helper script for bundling the application on OS X

echo "Bundling Doomsday.app..."

CP="cp -fRp"

SRCDIR=$1
echo "Source directory: $SRCDIR"

BUILDDIR=client

if [ ! -e $BUILDDIR/doomsday.app ]; then
    echo "Built product not found, skipping bundling."
    exit
fi

mv $BUILDDIR/doomsday.app $BUILDDIR/Doomsday.app

APPDIR=$BUILDDIR/Doomsday.app/Contents
echo "Bundle directory: $APPDIR"

echo "Clearing existing bundles..."
rm -rf $BUILDDIR/*.bundle

echo "Copying server..."
$CP server/doomsday-server $APPDIR/Resources

echo "Copying doomsday-shell-text..."
$CP tools/shell/shell-text/doomsday-shell-text $APPDIR/Resources

echo "Copying bundles from plugins..."
PLUGDIR=$APPDIR/DengPlugins
rm -rf $PLUGDIR
mkdir -p $PLUGDIR
$CP plugins/dehread/dehread.bundle                 $PLUGDIR/
$CP plugins/wadmapconverter/wadmapconverter.bundle $PLUGDIR/
$CP plugins/doom/doom.bundle                       $PLUGDIR/
$CP plugins/heretic/heretic.bundle                 $PLUGDIR/
$CP plugins/hexen/hexen.bundle                     $PLUGDIR/
$CP plugins/doom64/doom64.bundle                   $PLUGDIR/
$CP plugins/fmod/audio_fmod.bundle                 $PLUGDIR/
$CP plugins/example/example.bundle                 $PLUGDIR/

# Tools
#$CP tools/wadtool/wadtool $APPDIR/Resources
$CP tools/texc/texc $APPDIR/Resources
$CP tools/md2tool/md2tool $APPDIR/Resources

if [ -e plugins/fluidsynth/audio_fluidsynth.bundle ]; then
    $CP plugins/fluidsynth/audio_fluidsynth.bundle $PLUGDIR/

    echo "Installing deps for audio_fluidsynth..."
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

    # audio_fluidsynth
    DSFS=$PLUGDIR/audio_fluidsynth.bundle/audio_fluidsynth
    install_name_tool -change /usr/local/lib/libglib-2.0.0.dylib \
    	@executable_path/../Frameworks/libglib-2.0.0.dylib $DSFS
    install_name_tool -change /usr/local/lib/libgthread-2.0.0.dylib \
    	@executable_path/../Frameworks/libgthread-2.0.0.dylib $DSFS
    install_name_tool -change /usr/local/Cellar/gettext/0.18.1.1/lib/libintl.8.dylib \
    	@executable_path/../Frameworks/libintl.8.dylib $DSFS
fi

echo "Bundling done."
