#!/bin/sh
IWADDIR="/Users/jaakko/Dropbox/IWADs"
CFGDIR="/Users/jaakko"

# Use Guard Malloc for debugging.
export DYLD_INSERT_LIBRARIES="/usr/lib/libgmalloc.dylib"
DEBUGGER="gdb --args"

GAME=$1
IWAD=$2
ROPTS="-file ../../doomsday.pk3 ../../${GAME}.pk3"

$DEBUGGER Doomsday.app/Contents/MacOS/Doomsday -appdir . -game ${GAME}.bundle -iwad $IWADDIR/${IWAD}.wad -basedir Doomsday.app/Contents/Resources -wh 800 600 -wnd -nomouse -nomusic $ROPTS -cparse $CFGDIR/util.cfg $3 $4 $5 $6 $7 $8 $9
