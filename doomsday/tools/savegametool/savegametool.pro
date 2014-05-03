# The Doomsday Engine Project: Savegame Tool
# Copyright (c) 2014 Daniel Swanson <danij@dengine.net>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../../config.pri)
include(../../dep_core.pri)
include(../../dep_legacy.pri)
include(../../dep_lzss.pri)

TEMPLATE = app
TARGET   = savegametool

# Build Configuration ----------------------------------------------------------

CONFIG -= app_bundle
win32: CONFIG += console

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
    linkBinaryToBundledLibcore($$TARGET)
    linkBinaryToBundledLiblegacy($$TARGET)
}
else {
    INSTALLS += target
    target.path = $$DENG_BIN_DIR
}
