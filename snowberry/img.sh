#!/bin/sh

MASTER_DMG=$1
VOLUME_NAME=$2
WC_DMG=workc.dmg
WC_DIR=workc

cp template-image/template.dmg "$WC_DMG"
mkdir -p "$WC_DIR"
hdiutil attach "$WC_DMG" -noautoopen -quiet -mountpoint "$WC_DIR"

rm -rf "$WC_DIR/Doomsday Engine.app"
rm -rf "$WC_DIR/Read Me.rtf"

ditto "dist/Doomsday Engine.app" "$WC_DIR/Doomsday Engine.app"
ditto "../doomsday/build/mac/Read Me.rtf" "$WC_DIR/Read Me.rtf"

diskutil rename `pwd`/"$WC_DIR" "$VOLUME_NAME"

hdiutil detach -quiet "$WC_DIR"
rm -f "$MASTER_DMG"
hdiutil convert "$WC_DMG" -format UDZO -imagekey zlib-level=9 -o "$MASTER_DMG"
rm -f "$WC_DMG"
