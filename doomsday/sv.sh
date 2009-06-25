#!/bin/sh
mkdir -p serverdir
export DYLD_LIBRARY_PATH=`pwd`
export LD_LIBRARY_PATH=`pwd`
./dengsv -iwad ~/IWADs/Doom.wad
