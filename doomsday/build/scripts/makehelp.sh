#!/bin/sh

cd ../../doc
CONHELP=../build/scripts/conhelp.py

echo "Engine help"
python $CONHELP ../client/data/cphelp.txt engine
amethyst -dHELP engine/controlpanel.ame >> ../client/data/cphelp.txt

echo "libdoom help"
python $CONHELP ../plugins/doom/data/conhelp.txt libcommon libdoom

echo "libheretic help"
python $CONHELP ../plugins/heretic/data/conhelp.txt libcommon libheretic

echo "libhexen help"
python $CONHELP ../plugins/hexen/data/conhelp.txt libcommon libhexen
