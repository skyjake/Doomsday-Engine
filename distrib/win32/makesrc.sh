#!/bin/sh
# Build the source package (with everything!).
# Includes MSVC files, sources (.C, .H), documentation and data files.

if [ "$1" == "" ]; then echo "Which release, please?"; exit; fi

echo "========================== MAKING SOURCE PACKAGE ==========================="

# Prepare output directory.
if [ ! -d Out ]; then mkdir Out; fi

FILE=`pwd`/Out/deng-src-1.8.$1.zip
cd ../doomsday

rm -f ${FILE}
zip -9 ${FILE} LICENSE *.dsp *.dsw Makefile Makefile.{jDoom,jHeretic,jHexen,ogl}   {Src,Include}/{.,Common,jDoom,jHeretic,jHexen,dp*,dr*,ds*,Unix}/*.{c,cpp,h,def,rc,rc2} Src/drOpenGL/Unix/*.c Doc/SrcNotes.txt Doc/Ame/{beginner,cmdline,infine,readme}.ame Doc/ChangeLog.txt Doc/Network.txt Doc/DEDDoc.txt Data/cphelp.txt Data/*.ico Data/*.wad Data/{Fonts,Graphics}/* Data/{jDoom,jHeretic,jHexen}/*.{ico,wad} Data/KeyMaps/*.dkm Defs/*.ded Defs/{jDoom,jHeretic,jHexen}/*.ded Runtime/{jDoom,jHeretic,jHexen}/*.{rsp,sh} Runtime/{jDoom,jHeretic,jHexen}/Startup.cfg
