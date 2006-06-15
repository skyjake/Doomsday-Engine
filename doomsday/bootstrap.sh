#!/bin/sh
# Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
# Licensed under the GNU GPLv2 or later versions.

test -w config.cache && rm config.cache
test -w config.cross.cache && rm config.cross.cache
libtoolize -c --ltdl --automake
autoreconf -i
echo " You are now ready to run \"cd ./build/unix; ../../configure && make \" "
