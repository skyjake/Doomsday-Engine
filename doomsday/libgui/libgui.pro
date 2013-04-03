# The Doomsday Engine Project: GUI extension for libdeng2
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.

include(../config.pri)

TEMPLATE = lib
TARGET   = deng_gui
VERSION  = $$DENG_VERSION

CONFIG += deng_qtgui
include(../dep_deng2.pri)

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

DEFINES += __LIBGUI__

INCLUDEPATH += include

# Public headers.
HEADERS += \
    include/de/GuiApp \
    include/de/Window \
    \
    include/de/gui/guiapp.h \
    include/de/gui/libgui.h \
    include/de/gui/window.h

# Sources and private headers.
SOURCES += \
    src/guiapp.cpp \
    src/window.cpp

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibdeng2(libdeng_gui)

    doPostLink("install_name_tool -id @executable_path/../Frameworks/libdeng_gui.0.dylib libdeng_gui.0.dylib")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng_gui*dylib ../client/Doomsday.app/Contents/Frameworks")
}
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
