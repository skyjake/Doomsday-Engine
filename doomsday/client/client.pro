# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

TEMPLATE = app
win32|macx: TARGET = Doomsday
      else: TARGET = doomsday

# Build Configuration --------------------------------------------------------

include(../config.pri)

VERSION = $$DENG_VERSION
echo(Doomsday Client $${DENG_VERSION}.)

# Some messy old code here:
*-g++*|*-gcc*|*-clang* {
    QMAKE_CXXFLAGS_WARN_ON += \
        -Wno-missing-field-initializers \
        -Wno-unused-parameter \
        -Wno-missing-braces
}

# External Dependencies ------------------------------------------------------

CONFIG += deng_qtopengl

include(../dep_sdl.pri)
include(../dep_opengl.pri)
include(../dep_zlib.pri)
include(../dep_curses.pri)
include(../dep_lzss.pri)
win32 {
    include(../dep_directx.pri)
}
include(../dep_deng2.pri)
include(../dep_deng1.pri)

# Definitions ----------------------------------------------------------------

DEFINES += __DOOMSDAY__ __CLIENT__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

# Linking --------------------------------------------------------------------

win32 {
    RC_FILE = res/windows/doomsday.rc
    OTHER_FILES += $$RC_FILE

    QMAKE_LFLAGS += /NODEFAULTLIB:libcmt

    LIBS += -lkernel32 -lgdi32 -lole32 -luser32 -lwsock32 -lopengl32 -lglu32
}
else:macx {
    useFramework(Cocoa)
    useFramework(QTKit)

    # The old 10.4 build uses a Carbon-based Qt.
    deng_carbonqt: useFramework(Carbon)
}
else {
    # Generic Unix.
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

# Source Files ---------------------------------------------------------------

DENG_API_HEADERS = \
    $$DENG_API_DIR/apis.h \
    $$DENG_API_DIR/api_audiod.h \
    $$DENG_API_DIR/api_audiod_mus.h \
    $$DENG_API_DIR/api_audiod_sfx.h \
    $$DENG_API_DIR/api_base.h \
    $$DENG_API_DIR/api_busy.h \
    $$DENG_API_DIR/api_client.h \
    $$DENG_API_DIR/api_console.h \
    $$DENG_API_DIR/api_def.h \
    $$DENG_API_DIR/api_event.h \
    $$DENG_API_DIR/api_infine.h \
    $$DENG_API_DIR/api_internaldata.h \
    $$DENG_API_DIR/api_filesys.h \
    $$DENG_API_DIR/api_fontrender.h \
    $$DENG_API_DIR/api_gameexport.h \
    $$DENG_API_DIR/api_gl.h \
    $$DENG_API_DIR/api_material.h \
    $$DENG_API_DIR/api_materialarchive.h \
    $$DENG_API_DIR/api_map.h \
    $$DENG_API_DIR/api_mapedit.h \
    $$DENG_API_DIR/api_player.h \
    $$DENG_API_DIR/api_plugin.h \
    $$DENG_API_DIR/api_render.h \
    $$DENG_API_DIR/api_resource.h \
    $$DENG_API_DIR/api_resourceclass.h \
    $$DENG_API_DIR/api_sound.h \
    $$DENG_API_DIR/api_svg.h \
    $$DENG_API_DIR/api_thinker.h \
    $$DENG_API_DIR/api_uri.h \
    $$DENG_API_DIR/api_wad.h \
    $$DENG_API_DIR/dd_share.h \
    $$DENG_API_DIR/dd_types.h \
    $$DENG_API_DIR/dd_version.h \
    $$DENG_API_DIR/def_share.h \
    $$DENG_API_DIR/dengproject.h \
    $$DENG_API_DIR/doomsday.h

# Convenience headers.
DENG_HEADERS += \
    include/BspBuilder \
    include/EntityDatabase \

# Private headers.
DENG_HEADERS += \
    include/MapElement \
    include/audio/audiodriver.h \
    include/audio/audiodriver_music.h \
    include/audio/m_mus2midi.h \
    include/audio/s_cache.h \
    include/audio/s_environ.h \
    include/audio/s_logic.h \
    include/audio/s_main.h \
    include/audio/s_mus.h \
    include/audio/s_sfx.h \
    include/audio/s_wav.h \
    include/audio/sys_audio.h \
    include/audio/sys_audiod_dummy.h \
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
    include/filehandle.h \
    include/filesys/file.h \
    include/filesys/filehandlebuilder.h \
    include/filesys/fileinfo.h \
    include/filesys/fs_main.h \
    include/filesys/fs_util.h \
    include/filesys/lumpindex.h \
    include/filesys/manifest.h \
    include/filesys/searchpath.h \
    include/filesys/sys_direc.h \
    include/filetype.h \
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
    include/library.h \
    include/m_decomp64.h \
    include/m_misc.h \
    include/m_nodepile.h \
    include/m_profiler.h \
    include/map/blockmap.h \
    include/map/blockmapvisual.h \
    include/map/bsp/bsptreenode.h \
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
    include/map/mapelement.h \
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
    include/render/lumobj.h \
    include/render/r_draw.h \
    include/render/r_main.h \
    include/render/r_lgrid.h \
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
    include/render/rendpoly.h \
    include/render/sky.h \
    include/render/sprite.h \
    include/render/vignette.h \
    include/render/vlight.h \
    include/render/walldiv.h \
    include/resource/animgroups.h \
    include/resource/bitmapfont.h \
    include/resource/colorpalette.h \
    include/resource/colorpalettes.h \
    include/resource/compositetexture.h \
    include/resource/font.h \
    include/resource/fonts.h \
    include/resource/hq2x.h \
    include/resource/image.h \
    include/resource/lumpcache.h \
    include/resource/material.h \
    include/resource/materialarchive.h \
    include/resource/materialbind.h \
    include/resource/materials.h \
    include/resource/materialscheme.h \
    include/resource/materialsnapshot.h \
    include/resource/materialvariant.h \
    include/resource/models.h \
    include/resource/patch.h \
    include/resource/patchname.h \
    include/resource/pcx.h \
    include/resource/r_data.h \
    include/resource/rawtexture.h \
    include/resource/texture.h \
    include/resource/texturemanifest.h \
    include/resource/texturescheme.h \
    include/resource/textures.h \
    include/resource/texturevariant.h \
    include/resource/texturevariantspecification.h \
    include/resource/tga.h \
    include/resource/wad.h \
    include/resource/zip.h \
    include/resourceclass.h \
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
    include/ui/dd_ui.h \
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
        src/windows/dd_winit.cpp \
        src/windows/directinput.cpp \
        src/windows/sys_console.cpp \
        src/windows/joystick_win32.cpp \
        src/windows/mouse_win32.cpp

    !deng_nodisplaymode: SOURCES += src/windows/displaymode_win32.cpp
}
else:unix {
    # Common Unix (including Mac OS X).
    HEADERS += \
        $$DENG_UNIX_INCLUDE_DIR/dd_uinit.h

    INCLUDEPATH += $$DENG_UNIX_INCLUDE_DIR

    SOURCES += \
        src/unix/dd_uinit.cpp \
        src/unix/joystick.cpp \
        src/unix/sys_console.cpp
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
    src/api_uri.cpp \
    src/audio/audiodriver.cpp \
    src/audio/audiodriver_music.cpp \
    src/audio/m_mus2midi.cpp \
    src/audio/s_cache.cpp \
    src/audio/s_environ.cpp \
    src/audio/s_logic.cpp \
    src/audio/s_main.cpp \
    src/audio/s_mus.cpp \
    src/audio/s_sfx.cpp \
    src/audio/s_wav.cpp \
    src/audio/sys_audiod_dummy.cpp \
    src/busymode.cpp \
    src/cbuffer.cpp \
    src/client/cl_frame.cpp \
    src/client/cl_infine.cpp \
    src/client/cl_main.cpp \
    src/client/cl_mobj.cpp \
    src/client/cl_player.cpp \
    src/client/cl_sound.cpp \
    src/client/cl_world.cpp \
    src/color.cpp \
    src/con_bar.cpp \
    src/con_config.cpp \
    src/con_data.cpp \
    src/con_main.cpp \
    src/dd_games.cpp \
    src/dd_help.cpp \
    src/dd_init.cpp \
    src/dd_loop.cpp \
    src/dd_main.cpp \
    src/dd_pinit.cpp \
    src/dd_plugin.cpp \
    src/dd_wad.cpp \
    src/def_data.cpp \
    src/def_main.cpp \
    src/def_read.cpp \
    src/dualstring.cpp \
    src/edit_bias.cpp \
    src/edit_bsp.cpp \
    src/edit_map.cpp \
    src/filesys/file.cpp \
    src/filesys/filehandle.cpp \
    src/filesys/fileid.cpp \
    src/filesys/fs_main.cpp \
    src/filesys/fs_scheme.cpp \
    src/filesys/fs_util.cpp \
    src/filesys/lumpindex.cpp \
    src/filesys/manifest.cpp \
    src/filesys/searchpath.cpp \
    src/filesys/sys_direc.cpp \
    src/game.cpp \
    src/gl/dgl_common.cpp \
    src/gl/dgl_draw.cpp \
    src/gl/gl_defer.cpp \
    src/gl/gl_deferredapi.cpp \
    src/gl/gl_draw.cpp \
    src/gl/gl_drawvectorgraphic.cpp \
    src/gl/gl_main.cpp \
    src/gl/gl_model.cpp \
    src/gl/gl_tex.cpp \
    src/gl/gl_texmanager.cpp \
    src/gl/svg.cpp \
    src/gl/sys_opengl.cpp \
    src/gridmap.cpp \
    src/library.cpp \
    src/m_decomp64.cpp \
    src/m_misc.cpp \
    src/m_nodepile.cpp \
    src/map/blockmap.cpp \
    src/map/blockmapvisual.cpp \
    src/map/bsp/hplane.cpp \
    src/map/bsp/partitioner.cpp \
    src/map/bsp/superblockmap.cpp \
    src/map/bspbuilder.cpp \
    src/map/bspleaf.cpp \
    src/map/bspnode.cpp \
    src/map/dam_file.cpp \
    src/map/dam_main.cpp \
    src/map/entitydatabase.cpp \
    src/map/gamemap.cpp \
    src/map/generators.cpp \
    src/map/hedge.cpp \
    src/map/linedef.cpp \
    src/map/p_data.cpp \
    src/map/p_dmu.cpp \
    src/map/p_intercept.cpp \
    src/map/p_maputil.cpp \
    src/map/p_mobj.cpp \
    src/map/p_objlink.cpp \
    src/map/p_particle.cpp \
    src/map/p_players.cpp \
    src/map/p_polyobjs.cpp \
    src/map/p_sight.cpp \
    src/map/p_think.cpp \
    src/map/p_ticker.cpp \
    src/map/plane.cpp \
    src/map/polyobj.cpp \
    src/map/propertyvalue.cpp \
    src/map/r_world.cpp \
    src/map/sector.cpp \
    src/map/sidedef.cpp \
    src/map/surface.cpp \
    src/map/vertex.cpp \
    src/network/masterserver.cpp \
    src/network/monitor.cpp \
    src/network/net_buf.cpp \
    src/network/net_demo.cpp \
    src/network/net_event.cpp \
    src/network/net_main.cpp \
    src/network/net_msg.cpp \
    src/network/net_ping.cpp \
    src/network/protocol.cpp \
    src/network/sys_network.cpp \
    src/network/ui_mpi.cpp \
    src/r_util.cpp \
    src/render/api_render.cpp \
    src/render/lumobj.cpp \
    src/render/r_draw.cpp \
    src/render/r_fakeradio.cpp \
    src/render/r_main.cpp \
    src/render/r_lgrid.cpp \
    src/render/r_shadow.cpp \
    src/render/r_things.cpp \
    src/render/rend_bias.cpp \
    src/render/rend_clip.cpp \
    src/render/rend_console.cpp \
    src/render/rend_decor.cpp \
    src/render/rend_dynlight.cpp \
    src/render/rend_fakeradio.cpp \
    src/render/rend_font.cpp \
    src/render/rend_halo.cpp \
    src/render/rend_list.cpp \
    src/render/rend_main.cpp \
    src/render/rend_model.cpp \
    src/render/rend_particle.cpp \
    src/render/rend_shadow.cpp \
    src/render/rendpoly.cpp \
    src/render/sky.cpp \
    src/render/sprite.cpp \
    src/render/vignette.cpp \
    src/render/vlight.cpp \
    src/resource/animgroups.cpp \
    src/resource/api_material.cpp \
    src/resource/api_resource.cpp \
    src/resource/bitmapfont.cpp \
    src/resource/colorpalette.cpp \
    src/resource/colorpalettes.cpp \
    src/resource/compositetexture.cpp \
    src/resource/fonts.cpp \
    src/resource/hq2x.cpp \
    src/resource/image.cpp \
    src/resource/material.cpp \
    src/resource/materialarchive.cpp \
    src/resource/materialbind.cpp \
    src/resource/materials.cpp \
    src/resource/materialscheme.cpp \
    src/resource/materialsnapshot.cpp \
    src/resource/materialvariant.cpp \
    src/resource/models.cpp \
    src/resource/patch.cpp \
    src/resource/patchname.cpp \
    src/resource/pcx.cpp \
    src/resource/r_data.cpp \
    src/resource/rawtexture.cpp \
    src/resource/texture.cpp \
    src/resource/texturemanifest.cpp \
    src/resource/texturescheme.cpp \
    src/resource/textures.cpp \
    src/resource/texturevariant.cpp \
    src/resource/tga.cpp \
    src/resource/wad.cpp \
    src/resource/zip.cpp \
    src/sys_system.cpp \
    src/tab_tables.c \
    src/ui/b_command.cpp \
    src/ui/b_context.cpp \
    src/ui/b_device.cpp \
    src/ui/b_main.cpp \
    src/ui/b_util.cpp \
    src/ui/busyvisual.cpp \
    src/ui/canvas.cpp \
    src/ui/canvaswindow.cpp \
    src/ui/dd_input.cpp \
    src/ui/displaymode.cpp \
    src/ui/fi_main.cpp \
    src/ui/finaleinterpreter.cpp \
    src/ui/keycode.cpp \
    src/ui/mouse_qt.cpp \
    src/ui/nativeui.cpp \
    src/ui/p_control.cpp \
    src/ui/sys_input.cpp \
    src/ui/ui2_main.cpp \
    src/ui/ui_main.cpp \
    src/ui/ui_panel.cpp \
    src/ui/window.cpp \
    src/ui/zonedebug.cpp \
    src/updater/downloaddialog.cpp \
    src/updater/processcheckdialog.cpp \
    src/updater/updateavailabledialog.cpp \
    src/updater/updater.cpp \
    src/updater/updaterdialog.cpp \
    src/updater/updatersettings.cpp \
    src/updater/updatersettingsdialog.cpp \
    src/uri.cpp

!deng_nosdlmixer:!deng_nosdl {
    HEADERS += include/audio/sys_audiod_sdlmixer.h
    SOURCES += src/audio/sys_audiod_sdlmixer.cpp
}

OTHER_FILES += \
    data/cphelp.txt \
    include/template.h.template \
    src/template.c.template

# Resources ------------------------------------------------------------------

data.files = $$OUT_PWD/../doomsday.pk3

mod.files = \
    $$DENG_MODULES_DIR/Config.de \
    $$DENG_MODULES_DIR/recutil.de

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

    data.path         = $${res.path}
    mod.path          = $${res.path}/modules
    startupfonts.path = $${res.path}/data/fonts
    startupgfx.path   = $${res.path}/data/graphics

    QMAKE_BUNDLE_DATA += mod res data startupfonts startupgfx

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
    INSTALLS += target data startupgfx startupfonts mod

    target.path       = $$DENG_BIN_DIR
    data.path         = $$DENG_DATA_DIR
    startupgfx.path   = $$DENG_DATA_DIR/graphics
    startupfonts.path = $$DENG_DATA_DIR/fonts
    mod.path          = $$DENG_BASE_DIR/modules

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
