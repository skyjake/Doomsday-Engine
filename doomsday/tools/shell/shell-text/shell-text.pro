# The Doomsday Engine Project: Server Shell -- Text-Mode Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../../config.pri)

TEMPLATE = app
TARGET   = doomsday-shell-text
VERSION  = 1.0.0

# Build Configuration -------------------------------------------------------

CONFIG -= app_bundle

DEFINES += LIBSHELL_VERSION=\\\"$$VERSION\\\"

include(../../../dep_deng2.pri)
include(../../../dep_shell.pri)
include(../../../dep_curses.pri)

# Sources -------------------------------------------------------------------

HEADERS += \
    src/commandlinewidget.h \
    src/cursesapp.h \
    src/cursestextcanvas.h \
    src/logwidget.h \
    src/main.h \
    src/shellapp.h \
    src/statuswidget.h \
    src/openconnectiondialog.h \
    src/aboutdialog.h

SOURCES += \
    src/commandlinewidget.cpp \
    src/cursesapp.cpp \
    src/cursestextcanvas.cpp \
    src/logwidget.cpp \
    src/main.cpp \
    src/shellapp.cpp \
    src/statuswidget.cpp \
    src/openconnectiondialog.cpp \
    src/aboutdialog.cpp

# Installation --------------------------------------------------------------

macx {
    linkBinaryToBundledLibdeng2($$TARGET)
    linkBinaryToBundledLibdengShell($$TARGET)
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
