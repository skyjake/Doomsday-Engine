# qmake project file for building texc
# (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

include(../../config.pri)

TEMPLATE = app
TARGET = texc

CONFIG -= app_bundle
win32: CONFIG += console
QT -= core gui

# TODO: need to test whether compiler supports this option
#!deng_macx4u_32bit : !deng_macx6_32bit_64bit {
#    *-g++* | *-gcc* | *-clang* {
#        QMAKE_CXXFLAGS_WARN_ON += -Wno-unused-result
#    }
#}

SOURCES += import.cpp texc.cpp

HEADERS += texc.h

# Installation.
!macx {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
