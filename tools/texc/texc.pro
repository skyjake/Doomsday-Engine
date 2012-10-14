# qmake project file for building texc
# (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

include(../../doomsday/config.pri)

TEMPLATE = app
TARGET = texc

CONFIG -= app_bundle
win32: CONFIG += console
QT -= core gui

SOURCES += import.cpp texc.cpp

HEADERS += texc.h

# Installation.
!macx {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
