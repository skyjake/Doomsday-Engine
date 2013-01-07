# qmake project file for building wadtool
# (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

include(../../config.pri)

TEMPLATE = app
TARGET = wadtool

win32: CONFIG += console
QT -= core gui

!win32: error("wadtool is currently only for Windows")

DEFINES += _CRT_SECURE_NO_WARNINGS

SOURCES += wadtool.c

HEADERS += wadtool.h

# Installation.
!macx {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
