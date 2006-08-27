#!/bin/sh

if [ -d ~/.deng ];
then    /bin/true;
else    mkdir ~/.deng;
fi

/usr/games/doomsday.real -userdir ~/.deng $@


