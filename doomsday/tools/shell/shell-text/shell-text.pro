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

include(../../../dep_deng2.pri)
include(../../../dep_curses.pri)

QT -= opengl

INCLUDEPATH += ../libshell/include

# Sources -------------------------------------------------------------------

HEADERS += \
    src/cursesapp.h \
    src/cursestextcanvas.h \
    src/main.h \
    src/shellapp.h \
    src/textcanvas.h \
    src/logwidget.h \
    src/textwidget.h \
    src/textrootwidget.h

SOURCES += \
    src/cursesapp.cpp \
    src/cursestextcanvas.cpp \
    src/main.cpp \
    src/shellapp.cpp \
    src/textcanvas.cpp \
    src/logwidget.cpp \
    src/textwidget.cpp \
    src/textrootwidget.cpp

# Installation --------------------------------------------------------------

macx {
    linkBinaryToBundledLibdeng2($$TARGET)
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
