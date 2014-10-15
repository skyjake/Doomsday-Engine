# The Doomsday Engine Project -- Graphics, Audio and Input Library
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU Lesser General Public License
# version 3 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/lgpl.html for details.

include(../config.pri)

TEMPLATE = lib
TARGET   = deng_gui
VERSION  = $$DENG_VERSION

CONFIG += deng_qtgui deng_qtopengl

deng_qt5:unix:!macx: QT += x11extras

include(../dep_core.pri)
include(../dep_opengl.pri)
include(../dep_assimp.pri)

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

PRECOMPILED_HEADER = src/precompiled.h

HEADERS += src/precompiled.h

# Public headers.
publicHeaders(root, \
    include/de/Atlas \
    include/de/AtlasTexture \
    include/de/Canvas \
    include/de/CanvasWindow \
    include/de/ColorBank \
    include/de/DisplayMode \
    include/de/Drawable \
    include/de/Font \
    include/de/FontBank \
    include/de/GLBuffer \
    include/de/GLFramebuffer \
    include/de/GLInfo \
    include/de/GLPixelFormat \
    include/de/GLProgram \
    include/de/GLShader \
    include/de/GLShaderBank \
    include/de/GLState \
    include/de/GLTarget \
    include/de/GLTexture \
    include/de/GLUniform \
    include/de/GuiApp \
    include/de/HeightMap \
    include/de/Image \
    include/de/ImageBank \
    include/de/KdTreeAtlasAllocator \
    include/de/KeyEvent \
    include/de/KeyEventSource \
    include/de/MouseEvent \
    include/de/MouseEventSource \
    include/de/ModelBank \
    include/de/ModelDrawable \
    include/de/NativeFont \
    include/de/PersistentCanvasWindow \
    include/de/RowAtlasAllocator \
    include/de/Sound \
    include/de/TextureBank \
    include/de/VertexBuilder \
    include/de/Waveform \
    include/de/WaveformBank \
)

publicHeaders(gui, \
    include/de/gui/canvas.h \
    include/de/gui/canvaswindow.h \
    include/de/gui/displaymode.h \
    include/de/gui/displaymode_native.h \
    include/de/gui/guiapp.h \
    include/de/gui/libgui.h \
    include/de/gui/persistentcanvaswindow.h \
)

publicHeaders(audio, \
    include/de/audio/sound.h \
    include/de/audio/waveform.h \
    include/de/audio/waveformbank.h \
)

publicHeaders(graphics, \
    include/de/graphics/atlas.h \
    include/de/graphics/atlastexture.h \
    include/de/graphics/colorbank.h \
    include/de/graphics/drawable.h \
    include/de/graphics/glbuffer.h \
    include/de/graphics/glentrypoints.h \
    include/de/graphics/glframebuffer.h \
    include/de/graphics/glinfo.h \
    include/de/graphics/glpixelformat.h \
    include/de/graphics/glprogram.h \
    include/de/graphics/glshader.h \
    include/de/graphics/glshaderbank.h \
    include/de/graphics/glstate.h \
    include/de/graphics/gltarget.h \
    include/de/graphics/gltexture.h \
    include/de/graphics/gluniform.h \
    include/de/graphics/heightmap.h \
    include/de/graphics/image.h \
    include/de/graphics/imagebank.h \
    include/de/graphics/kdtreeatlasallocator.h \
    include/de/graphics/opengl.h \
    include/de/graphics/modeldrawable.h \
    include/de/graphics/modelbank.h \
    include/de/graphics/rowatlasallocator.h \
    include/de/graphics/texturebank.h \
    include/de/graphics/vertexbuilder.h \
)

publicHeaders(input, \
    include/de/input/ddkey.h \
    include/de/input/keyevent.h \
    include/de/input/keyeventsource.h \
    include/de/input/mouseevent.h \
    include/de/input/mouseeventsource.h \
)

publicHeaders(text, \
    include/de/text/font.h \
    include/de/text/fontbank.h \
    include/de/text/nativefont.h \
)

# Sources and private headers.
SOURCES +=  \
    src/audio/sound.cpp \
    src/audio/waveform.cpp \
    src/audio/waveformbank.cpp \
    src/canvas.cpp \
    src/canvaswindow.cpp \
    src/displaymode.cpp \
    src/graphics/atlas.cpp \
    src/graphics/atlastexture.cpp \
    src/graphics/colorbank.cpp \
    src/graphics/drawable.cpp \
    src/graphics/glbuffer.cpp \
    src/graphics/glentrypoints.cpp \
    src/graphics/glentrypoints_x11.cpp \
    src/graphics/glframebuffer.cpp \
    src/graphics/glinfo.cpp \
    src/graphics/glprogram.cpp \
    src/graphics/glshaderbank.cpp \
    src/graphics/glshader.cpp \
    src/graphics/glstate.cpp \
    src/graphics/gltarget_alternativebuffer.cpp \
    src/graphics/gltarget.cpp \
    src/graphics/gltexture.cpp \
    src/graphics/gluniform.cpp \
    src/graphics/heightmap.cpp \
    src/graphics/imagebank.cpp \
    src/graphics/image.cpp \
    src/graphics/kdtreeatlasallocator.cpp \
    src/graphics/modelbank.cpp \
    src/graphics/modeldrawable.cpp \
    src/graphics/rowatlasallocator.cpp \
    src/graphics/texturebank.cpp \
    src/guiapp.cpp \
    src/input/keyevent.cpp \
    src/input/keyeventsource.cpp \
    src/input/mouseevent.cpp \
    src/input/mouseeventsource.cpp \
    src/persistentcanvaswindow.cpp \
    src/text/fontbank.cpp \
    src/text/font.cpp \
    src/text/font_richformat.cpp \
    src/text/nativefont.cpp \
    src/text/qtnativefont.cpp \
    src/text/qtnativefont.h

macx:!deng_macx6_32bit_64bit: SOURCES += \
    src/text/coretextnativefont_macx.h \
    src/text/coretextnativefont_macx.cpp

# DisplayMode
!deng_nodisplaymode {
    win32:      SOURCES += src/displaymode_windows.cpp
    else:macx:  OBJECTIVE_SOURCES += src/displaymode_macx.mm
    else:       SOURCES += src/displaymode_x11.cpp
}
else {
    SOURCES += src/displaymode_dummy.cpp
}

unix:!macx: SOURCES += src/input/imKStoUCS_x11.c

scripts.files = \
    modules/gui.de

OTHER_FILES += \
    $$scripts.files

# Installation ---------------------------------------------------------------

macx {
    xcodeFinalizeBuild($$TARGET)
    linkDylibToBundledLibcore(libdeng_gui)

    doPostLink("install_name_tool -id @rpath/libdeng_gui.1.dylib libdeng_gui.1.dylib")

    # Prepare Assimp for deployment.
    doPostLink("cp -fRp $$ASSIMP_DIR/lib/libassimp*dylib .")
    doPostLink("install_name_tool -id @rpath/libassimp.3.dylib libassimp.3.dylib")
    linkBinaryToBundledAssimp(libdeng_gui.1.dylib, ..)
}

buildPackage(net.dengine.stdlib.gui, $$OUT_PWD/..)
deployLibrary()
