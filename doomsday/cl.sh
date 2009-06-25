#!/bin/sh
mkdir -p clientdir
export DYLD_LIBRARY_PATH=`pwd`
export LD_LIBRARY_PATH=`pwd`
./dengcl -iwad ~/IWADs/Doom.wad -wnd
