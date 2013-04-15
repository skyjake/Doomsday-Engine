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

CONFIG += deng_qtgui deng_qtopengl

include(../dep_deng2.pri)

DEFINES += __LIBGUI__
INCLUDEPATH += include

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}
else:macx {
    useFramework(Cocoa)
}
else:unix {
    LIBS += -lX11

    # DisplayMode uses the Xrandr and XFree86-VideoMode extensions.
    !deng_nodisplaymode {
        # Check that the X11 extensions exist.
        !system(pkg-config --exists xxf86vm) {
            error(Missing dependency: X11 XFree86 video mode extension library (development headers). Alternatively disable display mode functionality with: CONFIG+=deng_nodisplaymode)
        }
        !system(pkg-config --exists xrandr) {
            error(Missing dependency: X11 RandR extension library (development headers). Alternatively disable display mode functionality with: CONFIG+=deng_nodisplaymode)
        }

        QMAKE_CXXFLAGS += $$system(pkg-config xrandr xxf86vm --cflags)
                  LIBS += $$system(pkg-config xrandr xxf86vm --libs)
    }
}

# Public headers.
HEADERS += \
    include/de/Canvas \
    include/de/CanvasWindow \
    include/de/DisplayMode \
    include/de/GLBuffer \
    include/de/GLProgram \
    include/de/GLShader \
    include/de/GLState \
    include/de/GLTarget \
    include/de/GLTexture \
    include/de/GLUniform \
    include/de/GuiApp \
    include/de/KeyEvent \
    include/de/KeyEventSource \
    include/de/MouseEventSource \
    include/de/PersistentCanvasWindow \
    \
    include/de/gui/canvas.h \
    include/de/gui/canvaswindow.h \
    include/de/gui/ddkey.h \
    include/de/gui/displaymode.h \
    include/de/gui/displaymode_native.h \
    include/de/gui/glbuffer.h \
    include/de/gui/glprogram.h \
    include/de/gui/glshader.h \
    include/de/gui/glstate.h \
    include/de/gui/gltarget.h \
    include/de/gui/gltexture.h \
    include/de/gui/gluniform.h \
    include/de/gui/guiapp.h \
    include/de/gui/keyevent.h \
    include/de/gui/keyeventsource.h \
    include/de/gui/libgui.h \
    include/de/gui/mouseeventsource.h \
    include/de/gui/persistentcanvaswindow.h

# Sources and private headers.
SOURCES += \
    src/canvas.cpp \
    src/canvaswindow.cpp \
    src/displaymode.cpp \
    src/glbuffer.cpp \
    src/glprogram.cpp \
    src/glshader.cpp \
    src/glstate.cpp \
    src/gltarget.cpp \
    src/gltexture.cpp \
    src/gluniform.cpp \
    src/guiapp.cpp \
    src/keyevent.cpp \
    src/persistentcanvaswindow.cpp

# DisplayMode
!deng_nodisplaymode {
    win32:      SOURCES += src/displaymode_windows.cpp
    else:macx:  OBJECTIVE_SOURCES += src/displaymode_macx.mm
    else:       SOURCES += src/displaymode_x11.cpp
}
else {
    SOURCES += src/displaymode_dummy.c
}

unix:!macx: SOURCES += src/imKStoUCS_x11.c

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibdeng2(libdeng_gui)

    doPostLink("install_name_tool -id @executable_path/../Frameworks/libdeng_gui.1.dylib libdeng_gui.1.dylib")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng_gui*dylib ../client/Doomsday.app/Contents/Frameworks")
}
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
