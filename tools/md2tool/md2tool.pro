# qmake project file for building md2tool
# (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPL 2.0+

include(../../doomsday/config.pri)

TEMPLATE = app
TARGET = md2tool

CONFIG -= app_bundle
QT -= core gui

SOURCES += md2tool.c

HEADERS += md2tool.h anorms.h

# Installation.
!macx {
    INSTALLS += target
    win32: target.path = $$DENG_WIN_PRODUCTS_DIR
     else: target.path = $$DENG_BIN_DIR
}
