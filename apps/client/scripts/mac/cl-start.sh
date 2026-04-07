#!/bin/sh
RUNDIR=`pwd`/client-runtime
mkdir -p $RUNDIR

if [ "$1" == "doom" ]
then
    commonstart.sh jDoom Doom -userdir $RUNDIR -connect localhost
fi

if [ "$1" == "doom2" ]
then
    commonstart.sh jDoom Doom2 -userdir $RUNDIR -connect localhost
fi

if [ "$1" == "heretic" ]
then
    commonstart.sh jHeretic Heretic -userdir $RUNDIR -connect localhost
fi

if [ "$1" == "hexen" ]
then
    commonstart.sh jHexen Hexen -userdir $RUNDIR -connect localhost
fi
