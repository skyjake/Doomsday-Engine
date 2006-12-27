#!/bin/sh

#TARG=build/final 
#chmod -R u+w $TARG/Doomsday.app
#rm -rf $TARG/Doomsday.app
#xcodebuild -target Everything -configuration Development \
#  clean install \
#  SYMROOT=Build/Release \
#  DSTROOT=$TARG \
#  SEPARATE_STRIP=YES \
#  STRIP="/usr/bin/strip -x -u -A"

DEPLOY=build/Deployment
TARG=$DEPLOY/Doomsday.app/Contents
RES=$TARG/Resources

# Copy frameworks.
mkdir -p $TARG/Frameworks
cp -R $HOME/Library/Frameworks/{SDL,SDL_mixer,SDL_net}.framework \
  $TARG/Frameworks

# Rebuild PK3s.
cd ../scripts
./packres.py ../mac
cd ../mac

# Copy resources.
mv doomsday.pk3 $DEPLOY/Doomsday.app/Contents/Resources
mv jdoom.pk3 $DEPLOY/jDoom.bundle/Contents/Resources
mv jheretic.pk3 $DEPLOY/jHeretic.bundle/Contents/Resources
mv jhexen.pk3 $DEPLOY/jHexen.bundle/Contents/Resources
