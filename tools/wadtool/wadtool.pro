# qmake project file for building wadtool
# (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

TEMPLATE = app
TARGET = wadtool

QT -= core gui

!win32: error("wadtool is currently only for Windows")

SOURCES += wadtool.c

HEADERS += wadtool.h
