#!/bin/sh
RUNDIR=`pwd`/server-runtime
mkdir -p $RUNDIR

if [ "$1" == "doom" ]
then
    commonstart.sh jDoom Doom -server -dedicated -userdir $RUNDIR
fi

if [ "$1" == "doom2" ]
then
    commonstart.sh jDoom Doom2 -server -dedicated -userdir $RUNDIR
fi

if [ "$1" == "heretic" ]
then
    commonstart.sh jHeretic Heretic -server -dedicated -userdir $RUNDIR
fi

if [ "$1" == "hexen" ]
then
    commonstart.sh jHexen Hexen -server -dedicated -userdir $RUNDIR
fi
