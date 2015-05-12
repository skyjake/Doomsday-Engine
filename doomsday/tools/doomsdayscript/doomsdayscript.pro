include(../../config.pri)
include(../../dep_core.pri)

TEMPLATE = app
TARGET = doomsdayscript

win32: CONFIG += console
macx:  CONFIG -= app_bundle

SOURCES += main.cpp

macx: QMAKE_LFLAGS += \
    -Wl,-rpath,@loader_path/../../libcore \
    -Wl,-rpath,$$[QT_INSTALL_LIBS]

