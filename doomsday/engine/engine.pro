# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>

TEMPLATE = app
win32|macx: TARGET = Doomsday
      else: TARGET = doomsday

# Build Configuration --------------------------------------------------------

include(../config.pri)

VERSION = $$DENG_VERSION

echo(Doomsday version $${DENG_VERSION}.)

# External Dependencies ------------------------------------------------------

include(../dep_sdl.pri)
include(../dep_opengl.pri)
include(../dep_zlib.pri)
include(../dep_curses.pri)
include(../dep_lzss.pri)
win32 {
    include(../dep_directx.pri)
}
include(../dep_deng2.pri)
include(../dep_deng.pri)

# Definitions ----------------------------------------------------------------

DEFINES += __DOOMSDAY__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

win32 {
    RC_FILE = res/windows/doomsday.rc
    OTHER_FILES += api/doomsday.def $$RC_FILE
}
else:macx {
    useFramework(Cocoa)
    useFramework(QTKit)
}
else {
    # Generic Unix.
    QMAKE_LFLAGS += -rdynamic

    !freebsd-*: LIBS += -ldl
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

# Linking --------------------------------------------------------------------

win32 {
    QMAKE_LFLAGS += \
        /NODEFAULTLIB:libcmt \
        /DEF:\"$$DENG_API_DIR/doomsday.def\" \
        /IMPLIB:\"$$DENG_EXPORT_LIB\"

    LIBS += -lkernel32 -lgdi32 -lole32 -luser32 -lwsock32 \
        -lopengl32 -lglu32
}
else:macx {
    useFramework(Cocoa)
    useFramework(QTKit)
}
else {
    # Allow exporting symbols out of the main executable.
    QMAKE_LFLAGS += -rdynamic
}

# Source Files ---------------------------------------------------------------

DENG_API_HEADERS = \
    api/busytask.h \
    api/dd_api.h \
    api/dd_fontrenderer.h \
    api/dd_gl.h \
    api/dd_infine.h \
    api/dd_maptypes.h \
    api/dd_plugin.h \
    api/dd_share.h \
    api/dd_types.h \
    api/dd_ui.h \
    api/dd_vectorgraphic.h \
    api/dd_version.h \
    api/dd_wad.h \
    api/dd_world.h \
    api/def_share.h \
    api/dengproject.h \
    api/doomsday.h \
    api/filehandle.h \
    api/filetype.h \
    api/materialarchive.h \
    api/resourceclass.h \
    api/sys_audiod.h \
    api/sys_audiod_mus.h \
    api/sys_audiod_sfx.h \
    api/thinker.h \
    api/uri.h

# Convenience headers.
DENG_HEADERS += \
    include/BspBuilder \
    include/EntityDatabase \

# Private headers.
DENG_HEADERS += \
    include/audio/audiodriver.h \
    include/audio/audiodriver_music.h \
    include/audio/s_cache.h \
    include/audio/s_environ.h \
    include/audio/s_logic.h \
    include/audio/s_main.h \
    include/audio/s_mus.h \
    include/audio/s_sfx.h \
    include/audio/s_wav.h \
    include/audio/sys_audio.h \
    include/audio/sys_audiod_dummy.h \
    include/binarytree.h \
    include/busymode.h \
    include/cbuffer.h \
    include/client/cl_def.h \
    include/client/cl_frame.h \
    include/client/cl_infine.h \
    include/client/cl_mobj.h \
    include/client/cl_player.h \
    include/client/cl_sound.h \
    include/client/cl_world.h \
    include/color.h \
    include/con_bar.h \
    include/con_bind.h \
    include/con_config.h \
    include/con_main.h \
    include/dd_def.h \
    include/dd_games.h \
    include/dd_help.h \
    include/dd_loop.h \
    include/dd_main.h \
    include/dd_pinit.h \
    include/de_audio.h \
    include/de_base.h \
    include/de_bsp.h \
    include/de_console.h \
    include/de_dam.h \
    include/de_defs.h \
    include/de_edit.h \
    include/de_filesys.h \
    include/de_graphics.h \
    include/de_infine.h \
    include/de_misc.h \
    include/de_network.h \
    include/de_platform.h \
    include/de_play.h \
    include/de_resource.h \
    include/de_render.h \
    include/de_system.h \
    include/de_ui.h \
    include/def_data.h \
    include/def_main.h \
    include/dualstring.h \
    include/edit_bias.h \
    include/edit_bsp.h \
    include/edit_map.h \
    include/filesys/file.h \
    include/filesys/filehandlebuilder.h \
    include/filesys/fileinfo.h \
    include/filesys/fs_main.h \
    include/filesys/fs_util.h \
    include/filesys/lumpindex.h \
    include/filesys/metafile.h \
    include/filesys/searchpath.h \
    include/filesys/sys_direc.h \
    include/filesys/sys_findfile.h \
    include/game.h \
    include/gl/gl_defer.h \
    include/gl/gl_deferredapi.h \
    include/gl/gl_draw.h \
    include/gl/gl_main.h \
    include/gl/gl_model.h \
    include/gl/gl_tex.h \
    include/gl/gl_texmanager.h \
    include/gl/svg.h \
    include/gl/sys_opengl.h \
    include/gl/texturecontent.h \
    include/gridmap.h \
    include/json.h \
    include/kdtree.h \
    include/library.h \
    include/m_bams.h \
    include/m_decomp64.h \
    include/m_linkedlist.h \
    include/m_md5.h \
    include/m_misc.h \
    include/m_mus2midi.h \
    include/m_nodepile.h \
    include/m_profiler.h \
    include/m_stack.h \
    include/m_vector.h \
    include/map/blockmap.h \
    include/map/blockmapvisual.h \
    include/map/bsp/hedgeinfo.h \
    include/map/bsp/hedgeintercept.h \
    include/map/bsp/hedgetip.h \
    include/map/bsp/hplane.h \
    include/map/bsp/linedefinfo.h \
    include/map/bsp/partitioncost.h \
    include/map/bsp/partitioner.h \
    include/map/bsp/superblockmap.h \
    include/map/bspbuilder.h \
    include/map/bspleaf.h \
    include/map/bspnode.h \
    include/map/dam_file.h \
    include/map/dam_main.h \
    include/map/entitydatabase.h \
    include/map/gamemap.h \
    include/map/generators.h \
    include/map/hedge.h \
    include/map/linedef.h \
    include/map/p_dmu.h \
    include/map/p_intercept.h \
    include/map/p_mapdata.h \
    include/map/p_maptypes.h \
    include/map/p_maputil.h \
    include/map/p_object.h \
    include/map/p_objlink.h \
    include/map/p_particle.h \
    include/map/p_players.h \
    include/map/p_polyobjs.h \
    include/map/p_sight.h \
    include/map/p_ticker.h \
    include/map/plane.h \
    include/map/polyobj.h \
    include/map/propertyvalue.h \
    include/map/r_world.h \
    include/map/sector.h \
    include/map/sidedef.h \
    include/map/surface.h \
    include/map/vertex.h \
    include/network/masterserver.h \
    include/network/monitor.h \
    include/network/net_buf.h \
    include/network/net_demo.h \
    include/network/net_event.h \
    include/network/net_main.h \
    include/network/net_msg.h \
    include/network/protocol.h \
    include/network/sys_network.h \
    include/network/ui_mpi.h \
    include/r_util.h \
    include/render/r_draw.h \
    include/render/r_main.h \
    include/render/r_lgrid.h \
    include/render/r_lumobjs.h \
    include/render/r_shadow.h \
    include/render/r_things.h \
    include/render/rend_bias.h \
    include/render/rend_clip.h \
    include/render/rend_console.h \
    include/render/rend_decor.h \
    include/render/rend_dynlight.h \
    include/render/rend_fakeradio.h \
    include/render/rend_font.h \
    include/render/rend_halo.h \
    include/render/rend_list.h \
    include/render/rend_main.h \
    include/render/rend_model.h \
    include/render/rend_particle.h \
    include/render/rend_shadow.h \
    include/render/rend_sprite.h \
    include/render/rendpoly.h \
    include/render/sky.h \
    include/render/vignette.h \
    include/render/vlight.h \
    include/resource/animgroups.h \
    include/resource/bitmapfont.h \
    include/resource/colorpalette.h \
    include/resource/colorpalettes.h \
    include/resource/font.h \
    include/resource/fonts.h \
    include/resource/hq2x.h \
    include/resource/image.h \
    include/resource/lumpcache.h \
    include/resource/material.h \
    include/resource/materials.h \
    include/resource/materialvariant.h \
    include/resource/models.h \
    include/resource/pcx.h \
    include/resource/r_data.h \
    include/resource/rawtexture.h \
    include/resource/texture.h \
    include/resource/textures.h \
    include/resource/texturevariant.h \
    include/resource/texturevariantspecification.h \
    include/resource/tga.h \
    include/resource/wad.h \
    include/resource/zip.h \
    include/server/sv_def.h \
    include/server/sv_frame.h \
    include/server/sv_infine.h \
    include/server/sv_missile.h \
    include/server/sv_pool.h \
    include/server/sv_sound.h \
    include/sys_console.h \
    include/sys_system.h \
    include/tab_anorms.h \
    include/ui/b_command.h \
    include/ui/b_context.h \
    include/ui/b_device.h \
    include/ui/b_main.h \
    include/ui/b_util.h \
    include/ui/busyvisual.h \
    include/ui/canvas.h \
    include/ui/canvaswindow.h \
    include/ui/consolewindow.h \
    include/ui/dd_input.h \
    include/ui/displaymode.h \
    include/ui/displaymode_native.h \
    include/ui/fi_main.h \
    include/ui/finaleinterpreter.h \
    include/ui/joystick.h \
    include/ui/keycode.h \
    include/ui/mouse_qt.h \
    include/ui/nativeui.h \
    include/ui/p_control.h \
    include/ui/sys_input.h \
    include/ui/ui2_main.h \
    include/ui/ui_main.h \
    include/ui/ui_panel.h \
    include/ui/window.h \
    include/ui/zonedebug.h \
    include/updater.h \
    include/uri.hh \
    src/updater/downloaddialog.h \
    src/updater/processcheckdialog.h \
    src/updater/updateavailabledialog.h \
    src/updater/updaterdialog.h \
    src/updater/updatersettings.h \
    src/updater/updatersettingsdialog.h \
    src/updater/versioninfo.h

INCLUDEPATH += \
    $$DENG_INCLUDE_DIR \
    $$DENG_API_DIR

HEADERS += \
    $$DENG_API_HEADERS \
    $$DENG_HEADERS

# Platform-specific sources.
win32 {
    # Windows.
    HEADERS += \
        $$DENG_WIN_INCLUDE_DIR/dd_winit.h \
        $$DENG_WIN_INCLUDE_DIR/directinput.h \
        $$DENG_WIN_INCLUDE_DIR/mouse_win32.h \
        $$DENG_WIN_INCLUDE_DIR/resource.h

    INCLUDEPATH += $$DENG_WIN_INCLUDE_DIR

    SOURCES += \
        src/windows/dd_winit.c \
        src/windows/directinput.cpp \
        src/windows/sys_console.c \
        src/windows/sys_findfile.c \
        src/windows/joystick_win32.cpp \
        src/windows/mouse_win32.cpp

    !deng_nodisplaymode: SOURCES += src/windows/displaymode_win32.cpp
}
else:unix {
    # Common Unix (including Mac OS X).
    HEADERS += \
        $$DENG_UNIX_INCLUDE_DIR/dd_uinit.h \
        $$DENG_UNIX_INCLUDE_DIR/sys_path.h

    INCLUDEPATH += $$DENG_UNIX_INCLUDE_DIR

    SOURCES += \
        src/unix/dd_uinit.c \
        src/unix/joystick.c \
        src/unix/sys_console.c \
        src/unix/sys_findfile.c \
        src/unix/sys_path.c
}

macx {
    # Mac OS X only.
    HEADERS += \
        $$DENG_MAC_INCLUDE_DIR/MusicPlayer.h

    OBJECTIVE_SOURCES += \
        src/macx/MusicPlayer.m

    !deng_nodisplaymode: OBJECTIVE_SOURCES += src/macx/displaymode_macx.mm

    INCLUDEPATH += $$DENG_MAC_INCLUDE_DIR
}
else:unix {
    !deng_nodisplaymode {
        # Unix (non-Mac) only.
        SOURCES += \
            src/unix/displaymode_x11.cpp \
            src/unix/imKStoUCS.c
    }
}

deng_nodisplaymode {
    SOURCES += src/ui/displaymode_dummy.c
}

# Platform-independent sources.
SOURCES += \
    src/audio/audiodriver.cpp \
    src/audio/audiodriver_music.c \
    src/audio/s_cache.c \
    src/audio/s_environ.cpp \
    src/audio/s_logic.c \
    src/audio/s_main.c \
    src/audio/s_mus.c \
    src/audio/s_sfx.c \
    src/audio/s_wav.c \
    src/audio/sys_audiod_dummy.c \
    src/binarytree.cpp \
    src/busymode.cpp \
    src/cbuffer.c \
    src/client/cl_frame.c \
    src/client/cl_infine.c \
    src/client/cl_main.c \
    src/client/cl_mobj.c \
    src/client/cl_player.c \
    src/client/cl_sound.c \
    src/client/cl_world.c \
    src/color.cpp \
    src/con_bar.c \
    src/con_config.c \
    src/con_data.cpp \
    src/con_main.c \
    src/dd_games.cpp \
    src/dd_help.c \
    src/dd_init.cpp \
    src/dd_loop.c \
    src/dd_main.cpp \
    src/dd_pinit.c \
    src/dd_plugin.c \
    src/dd_wad.cpp \
    src/def_data.c \
    src/def_main.cpp \
    src/def_read.cpp \
    src/dualstring.cpp \
    src/edit_bias.c \
    src/edit_bsp.cpp \
    src/edit_map.cpp \
    src/filesys/file.cpp \
    src/filesys/filehandle.cpp \
    src/filesys/fileid.cpp \
    src/filesys/fs_main.cpp \
    src/filesys/fs_scheme.cpp \
    src/filesys/fs_util.cpp \
    src/filesys/lumpindex.cpp \
    src/filesys/metafile.cpp \
    src/filesys/searchpath.cpp \
    src/filesys/sys_direc.c \
    src/game.cpp \
    src/gl/dgl_common.c \
    src/gl/dgl_draw.c \
    src/gl/gl_defer.c \
    src/gl/gl_deferredapi.c \
    src/gl/gl_draw.c \
    src/gl/gl_drawvectorgraphic.c \
    src/gl/gl_main.c \
    src/gl/gl_tex.c \
    src/gl/gl_texmanager.cpp \
    src/gl/svg.c \
    src/gl/sys_opengl.c \
    src/gridmap.c \
    src/json.cpp \
    src/kdtree.c \
    src/library.cpp \
    src/m_bams.c \
    src/m_decomp64.c \
    src/m_linkedlist.c \
    src/m_md5.c \
    src/m_misc.c \
    src/m_mus2midi.c \
    src/m_nodepile.c \
    src/m_stack.c \
    src/m_vector.c \
    src/map/blockmap.c \
    src/map/blockmapvisual.c \
    src/map/bsp/hplane.cpp \
    src/map/bsp/partitioner.cpp \
    src/map/bsp/superblockmap.cpp \
    src/map/bspbuilder.cpp \
    src/map/bspleaf.cpp \
    src/map/bspnode.c \
    src/map/dam_file.c \
    src/map/dam_main.cpp \
    src/map/entitydatabase.cpp \
    src/map/gamemap.c \
    src/map/generators.c \
    src/map/hedge.cpp \
    src/map/linedef.c \
    src/map/p_data.cpp \
    src/map/p_dmu.c \
    src/map/p_intercept.c \
    src/map/p_maputil.c \
    src/map/p_mobj.c \
    src/map/p_objlink.c \
    src/map/p_particle.c \
    src/map/p_players.c \
    src/map/p_polyobjs.c \
    src/map/p_sight.c \
    src/map/p_think.c \
    src/map/p_ticker.c \
    src/map/plane.c \
    src/map/polyobj.c \
    src/map/propertyvalue.cpp \
    src/map/r_world.c \
    src/map/sector.c \
    src/map/sidedef.c \
    src/map/surface.c \
    src/map/vertex.cpp \
    src/network/masterserver.cpp \
    src/network/monitor.c \
    src/network/net_buf.c \
    src/network/net_demo.c \
    src/network/net_event.c \
    src/network/net_main.c \
    src/network/net_msg.c \
    src/network/net_ping.c \
    src/network/protocol.c \
    src/network/sys_network.c \
    src/network/ui_mpi.c \
    src/r_util.c \
    src/render/r_draw.c \
    src/render/r_fakeradio.c \
    src/render/r_main.c \
    src/render/r_lgrid.c \
    src/render/r_lumobjs.c \
    src/render/r_shadow.c \
    src/render/r_things.c \
    src/render/rend_bias.c \
    src/render/rend_clip.cpp \
    src/render/rend_console.c \
    src/render/rend_decor.c \
    src/render/rend_dynlight.c \
    src/render/rend_fakeradio.c \
    src/render/rend_font.c \
    src/render/rend_halo.c \
    src/render/rend_list.c \
    src/render/rend_main.c \
    src/render/rend_model.c \
    src/render/rend_particle.c \
    src/render/rend_shadow.c \
    src/render/rend_sprite.c \
    src/render/rendpoly.cpp \
    src/render/sky.cpp \
    src/render/vignette.c \
    src/render/vlight.cpp \
    src/resource/animgroups.cpp \
    src/resource/bitmapfont.c \
    src/resource/colorpalette.c \
    src/resource/colorpalettes.cpp \
    src/resource/fonts.cpp \
    src/resource/hq2x.c \
    src/resource/image.cpp \
    src/resource/material.cpp \
    src/resource/materialarchive.c \
    src/resource/materials.cpp \
    src/resource/materialvariant.cpp \
    src/resource/models.cpp \
    src/resource/pcx.c \
    src/resource/r_data.c \
    src/resource/rawtexture.cpp \
    src/resource/texture.cpp \
    src/resource/textures.cpp \
    src/resource/texturevariant.cpp \
    src/resource/tga.c \
    src/resource/wad.cpp \
    src/resource/zip.cpp \
    src/server/sv_frame.c \
    src/server/sv_infine.c \
    src/server/sv_main.c \
    src/server/sv_missile.c \
    src/server/sv_pool.c \
    src/server/sv_sound.cpp \
    src/sys_system.c \
    src/tab_tables.c \
    src/ui/b_command.c \
    src/ui/b_context.c \
    src/ui/b_device.c \
    src/ui/b_main.c \
    src/ui/b_util.c \
    src/ui/busyvisual.c \
    src/ui/canvas.cpp \
    src/ui/canvaswindow.cpp \
    src/ui/dd_input.c \
    src/ui/displaymode.cpp \
    src/ui/fi_main.c \
    src/ui/finaleinterpreter.c \
    src/ui/keycode.cpp \
    src/ui/mouse_qt.cpp \
    src/ui/nativeui.cpp \
    src/ui/p_control.c \
    src/ui/sys_input.c \
    src/ui/ui2_main.c \
    src/ui/ui_main.c \
    src/ui/ui_panel.c \
    src/ui/window.cpp \
    src/ui/zonedebug.c \
    src/updater/downloaddialog.cpp \
    src/updater/processcheckdialog.cpp \
    src/updater/updateavailabledialog.cpp \
    src/updater/updater.cpp \
    src/updater/updaterdialog.cpp \
    src/updater/updatersettings.cpp \
    src/updater/updatersettingsdialog.cpp \
    src/uri.cpp \
    src/uri_wrapper.cpp

!deng_nosdlmixer:!deng_nosdl {
    HEADERS += include/audio/sys_audiod_sdlmixer.h
    SOURCES += src/audio/sys_audiod_sdlmixer.c
}

# Use the fixed-point math from libcommon.
# TODO: Move it to the engine.
SOURCES += ../plugins/common/src/m_fixed.c

OTHER_FILES += \
    data/cphelp.txt \
    include/mapdata.hs \
    include/template.h.template \
    src/template.c.template

# Resources ------------------------------------------------------------------

data.files = $$OUT_PWD/../doomsday.pk3

cfg.files = $$DENG_CONFIG_DIR/deng.de

startupdata.files = \
    data/cphelp.txt

# These fonts may be needed during the initial startup busy mode.
startupfonts.files = \
    data/fonts/console11.dfn \
    data/fonts/console14.dfn \
    data/fonts/console18.dfn \
    data/fonts/normal12.dfn \
    data/fonts/normal18.dfn \
    data/fonts/normal24.dfn \
    data/fonts/normalbold12.dfn \
    data/fonts/normalbold18.dfn \
    data/fonts/normalbold24.dfn \
    data/fonts/normallight12.dfn \
    data/fonts/normallight18.dfn \
    data/fonts/normallight24.dfn

startupgfx.files = \
    data/graphics/background.pcx \
    data/graphics/loading1.png \
    data/graphics/loading2.png \
    data/graphics/logo.png

macx {
    res.path = Contents/Resources
    res.files = \
        res/macx/English.lproj \
        res/macx/deng.icns

    cfg.path          = $${res.path}/config
    data.path         = $${res.path}
    startupdata.path  = $${res.path}/data
    startupfonts.path = $${res.path}/data/fonts
    startupgfx.path   = $${res.path}/data/graphics

    QMAKE_BUNDLE_DATA += cfg res data startupfonts startupdata startupgfx

    QMAKE_INFO_PLIST = ../build/mac/Info.plist

    # Since qmake is unable to copy directories as bundle data, let's copy
    # the frameworks manually.
    FW_DIR = \"$${OUT_PWD}/Doomsday.app/Contents/Frameworks/\"
    doPostLink("rm -rf $$FW_DIR")
    doPostLink("mkdir $$FW_DIR")
    !deng_nosdl {
        doPostLink("cp -fRp $${SDL_FRAMEWORK_DIR}/SDL.framework $$FW_DIR")
        !deng_nosdlmixer: doPostLink("cp -fRp $${SDL_FRAMEWORK_DIR}/SDL_mixer.framework $$FW_DIR")
    }
    deng_fmod {
        # Bundle the FMOD shared library under Frameworks.
        doPostLink("cp -f \"$$FMOD_DIR/api/lib/libfmodex.dylib\" $$FW_DIR")
        doPostLink("install_name_tool -id @executable_path/../Frameworks/libfmodex.dylib $${FW_DIR}libfmodex.dylib")
    }

    # libdeng1 and 2 dynamic libraries.
    doPostLink("cp -fRp $$OUT_PWD/../libdeng2/libdeng2*dylib $$FW_DIR")
    doPostLink("cp -fRp $$OUT_PWD/../libdeng/libdeng1*dylib $$FW_DIR")

    # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
    fixInstallName(Doomsday.app/Contents/MacOS/Doomsday, libdeng2.2.dylib, ..)
    fixInstallName(Doomsday.app/Contents/MacOS/Doomsday, libdeng1.1.dylib, ..)

    # Clean up previous deployment.
    doPostLink("rm -rf Doomsday.app/Contents/PlugIns/")
    doPostLink("rm -f Doomsday.app/Contents/Resources/qt.conf")

    doPostLink("macdeployqt Doomsday.app")
}

# Installation ---------------------------------------------------------------

!macx {
    # Common (non-Mac) parts of the installation.
    INSTALLS += target data startupdata startupgfx startupfonts cfg

    target.path       = $$DENG_BIN_DIR
    data.path         = $$DENG_DATA_DIR
    startupdata.path  = $$DENG_DATA_DIR
    startupgfx.path   = $$DENG_DATA_DIR/graphics
    startupfonts.path = $$DENG_DATA_DIR/fonts
    cfg.path          = $$DENG_BASE_DIR/config

    win32 {
        # Windows-specific installation.
        INSTALLS += license icon

        license.files = doc/LICENSE
        license.path = $$DENG_DOCS_DIR

        icon.files = res/windows/doomsday.ico
        icon.path = $$DENG_DATA_DIR/graphics
    }
    else {
        # Generic Unix installation.
        INSTALLS += readme

        readme.files = ../doc/output/doomsday.6
        readme.path = $$PREFIX/share/man/man6
    }
}
