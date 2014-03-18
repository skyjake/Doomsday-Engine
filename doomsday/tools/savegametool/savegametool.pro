# The Doomsday Engine Project: Savegame Tool
# Copyright (c) 2014 Daniel Swanson <danij@dengine.net>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../config.pri)
include(../../dep_deng2.pri)
include(../../dep_lzss.pri)

TEMPLATE = app
TARGET   = savegametool

# Build Configuration ----------------------------------------------------------

CONFIG += console
CONFIG -= app_bundle

# Sources ----------------------------------------------------------------------

SOURCES += \
    src/id1translator.h \
    src/packageformatter.h \
    src/nativetranslator.h \
    \
    src/id1translator.cpp \
    src/main.cpp \
    src/nativetranslator.cpp \
    src/packageformatter.cpp

# Deployment -------------------------------------------------------------------

macx {
    linkBinaryToBundledLibdeng2($$TARGET)
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
