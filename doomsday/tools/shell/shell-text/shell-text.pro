# The Doomsday Engine Project: Server Shell -- Text-Mode Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../../config.pri)

TEMPLATE = app
TARGET   = doomsday-shell-text
VERSION  = $$DENG_VERSION

# Build Configuration -------------------------------------------------------

CONFIG -= app_bundle

DEFINES += SHELL_VERSION=\\\"$$VERSION\\\"

include(../../../dep_deng2.pri)
include(../../../dep_shell.pri)
include(../../../dep_curses.pri)

# Sources -------------------------------------------------------------------

HEADERS += \
    src/aboutdialog.h \
    src/cursesapp.h \
    src/cursestextcanvas.h \
    src/localserverdialog.h \
    src/main.h \
    src/openconnectiondialog.h \
    src/persistentdata.h \
    src/shellapp.h \
    src/statuswidget.h

SOURCES += \
    src/aboutdialog.cpp \
    src/cursesapp.cpp \
    src/cursestextcanvas.cpp \
    src/localserverdialog.cpp \
    src/main.cpp \
    src/openconnectiondialog.cpp \
    src/persistentdata.cpp \
    src/shellapp.cpp \
    src/statuswidget.cpp 

# Installation --------------------------------------------------------------

macx {
    linkBinaryToBundledLibdeng2($$TARGET)
    linkBinaryToBundledLibdengShell($$TARGET)
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR

    unix {
        INSTALLS += readme
        readme.files = ../../../doc/output/doomsday-shell-text.6
        readme.path  = $$PREFIX/share/man/man6
    }
}
