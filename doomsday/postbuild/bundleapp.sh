#!/bin/sh
# Helper script for bundling the application on OS X

CP="cp -fRp"
SRCDIR=$1
echo "Source directory: $SRCDIR"

echo "Bundling Doomsday.app..."

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

echo "Copying shared libraries..."
$CP $BUILDDIR/../libdeng2/libdeng2*dylib      $APPDIR/Frameworks
$CP $BUILDDIR/../libdeng1/libdeng1*dylib      $APPDIR/Frameworks
$CP $BUILDDIR/../libgui/libdeng_gui*dylib     $APPDIR/Frameworks
$CP $BUILDDIR/../libshell/libdeng_shell*dylib $APPDIR/Frameworks

echo "Copying server..."
$CP server/doomsday-server $APPDIR/Resources

echo "Copying shell-text..."
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

	GLIB_VER=`pkg-config --modversion glib-2.0`
    GETTEXT_VER=0.18.2

    echo "Installing deps for audio_fluidsynth..."
	echo "- glib version: $GLIB_VER"
	echo "- gettext version: $GETTEXT_VER"
    FWDIR=$BUILDDIR/Doomsday.app/Contents/Frameworks
    cp /usr/local/lib/libglib-2.0.0.dylib $FWDIR
    cp /usr/local/lib/libgthread-2.0.0.dylib $FWDIR
    cp /usr/local/Cellar/gettext/$GETTEXT_VER/lib/libintl.8.dylib $FWDIR
    chmod u+w $FWDIR/libglib-2.0.0.dylib $FWDIR/libgthread-2.0.0.dylib $FWDIR/libintl.8.dylib

    # IDs
    install_name_tool -id @executable_path/../Frameworks/libglib-2.0.0.dylib $FWDIR/libglib-2.0.0.dylib
    install_name_tool -id @executable_path/../Frameworks/libgthread-2.0.0.dylib $FWDIR/libgthread-2.0.0.dylib
    install_name_tool -id @executable_path/../Frameworks/libintl.8.dylib $FWDIR/libintl.8.dylib

    # glib-2.0.0
    install_name_tool -change /usr/local/Cellar/gettext/$GETTEXT_VER/lib/libintl.8.dylib \
    	@executable_path/../Frameworks/libintl.8.dylib $FWDIR/libglib-2.0.0.dylib
    install_name_tool -change /usr/local/opt/gettext/lib/libintl.8.dylib \
    	@executable_path/../Frameworks/libintl.8.dylib $FWDIR/libglib-2.0.0.dylib

    # gthread-2.0.0
    install_name_tool -change /usr/local/Cellar/glib/$GLIB_VER/lib/libglib-2.0.0.dylib \
    	@executable_path/../Frameworks/libglib-2.0.0.dylib $FWDIR/libgthread-2.0.0.dylib
    install_name_tool -change /usr/local/Cellar/gettext/$GETTEXT_VER/lib/libintl.8.dylib \
    	@executable_path/../Frameworks/libintl.8.dylib $FWDIR/libgthread-2.0.0.dylib
    install_name_tool -change /usr/local/opt/gettext/lib/libintl.8.dylib \
    	@executable_path/../Frameworks/libintl.8.dylib $FWDIR/libgthread-2.0.0.dylib

    # audio_fluidsynth
    DSFS=$PLUGDIR/audio_fluidsynth.bundle/audio_fluidsynth
    install_name_tool -change /usr/local/lib/libglib-2.0.0.dylib \
    	@executable_path/../Frameworks/libglib-2.0.0.dylib $DSFS
    install_name_tool -change /usr/local/lib/libgthread-2.0.0.dylib \
    	@executable_path/../Frameworks/libgthread-2.0.0.dylib $DSFS
    install_name_tool -change /usr/local/Cellar/gettext/$GETTEXT_VER/lib/libintl.8.dylib \
    	@executable_path/../Frameworks/libintl.8.dylib $DSFS
    install_name_tool -change /usr/local/opt/gettext/lib/libintl.8.dylib \
    	@executable_path/../Frameworks/libintl.8.dylib $DSFS
fi

echo "Bundling Doomsday Shell.app..."

BUILDDIR=tools/shell/shell-gui
APPDIR="$BUILDDIR/Doomsday Shell.app/Contents"

mkdir -p "$APPDIR/Frameworks"

$CP libdeng2/libdeng2*dylib      "$APPDIR/Frameworks"
$CP libshell/libdeng_shell*dylib "$APPDIR/Frameworks"

echo "Bundling done."
