# The Doomsday Engine Project: Server Shell -- Text-Mode Interface
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../../config.pri)

TEMPLATE = app
TARGET   = doomsday-shell-curses
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
    src/main.h \
    src/textcanvas.h \
    src/cursestextcanvas.h

SOURCES += \
    src/cursesapp.cpp \
    src/main.cpp \
    src/textcanvas.cpp \
    src/cursestextcanvas.cpp

# Installation --------------------------------------------------------------

macx {
    linkBinaryToBundledLibdeng2($$TARGET)
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
