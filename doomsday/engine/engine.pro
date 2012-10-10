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
    RC_FILE = win32/res/doomsday.rc
    OTHER_FILES += api/doomsday.def
}
else:macx {
    useFramework(Cocoa)
    useFramework(QTKit)
}
else {
    # Generic Unix.
    DEFINES += DENG_BASE_DIR=\"\\\"$${DENG_BASE_DIR}/\\\"\"
    DEFINES += DENG_LIBRARY_DIR=\"\\\"$${DENG_LIB_DIR}/\\\"\"

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

    LIBS += -lkernel32 -lgdi32 -lole32 -luser32 -lwsock32 -lwinmm \
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
    api/dd_animator.h \
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
    api/dd_wad.h \
    api/dd_world.h \
    api/def_share.h \
    api/dengproject.h \
    api/dfile.h \
    api/doomsday.h \
    api/materialarchive.h \
    api/point.h \
    api/rect.h \
    api/size.h \
    api/sys_audiod.h \
    api/sys_audiod_mus.h \
    api/sys_audiod_sfx.h \
    api/thinker.h \
    api/uri.h

# Convenience headers.
DENG_HEADERS += \
    portable/include/BspBuilder \
    portable/include/EntityDatabase \

# Private headers.
DENG_HEADERS += \
    portable/include/abstractfile.h \
    portable/include/abstractresource.h \
    portable/include/audiodriver.h \
    portable/include/audiodriver_music.h \
    portable/include/b_command.h \
    portable/include/b_context.h \
    portable/include/b_device.h \
    portable/include/b_main.h \
    portable/include/b_util.h \
    portable/include/binarytree.h \
    portable/include/bitmapfont.h \
    portable/include/blockmap.h \
    portable/include/blockmapvisual.h \
    portable/include/blockset.h \
    portable/include/busymode.h \
    portable/include/map/bspbuilder.h \
    portable/include/map/entitydatabase.h \
    portable/include/map/bsp/hedgeinfo.h \
    portable/include/map/bsp/hedgeintercept.h \
    portable/include/map/bsp/hedgetip.h \
    portable/include/map/bsp/hplane.h \
    portable/include/map/bsp/linedefinfo.h \
    portable/include/map/bsp/partitioncost.h \
    portable/include/map/bsp/partitioner.h \
    portable/include/map/bsp/superblockmap.h \
    portable/include/bspleaf.h \
    portable/include/bspnode.h \
    portable/include/canvas.h \
    portable/include/canvaswindow.h \
    portable/include/cbuffer.h \
    portable/include/cl_def.h \
    portable/include/cl_frame.h \
    portable/include/cl_infine.h \
    portable/include/cl_mobj.h \
    portable/include/cl_player.h \
    portable/include/cl_sound.h \
    portable/include/cl_world.h \
    portable/include/colorpalette.h \
    portable/include/con_bar.h \
    portable/include/con_bind.h \
    portable/include/con_config.h \
    portable/include/con_main.h \
    portable/include/consolewindow.h \
    portable/include/dam_file.h \
    portable/include/dam_main.h \
    portable/include/dd_def.h \
    portable/include/dd_games.h \
    portable/include/dd_help.h \
    portable/include/dd_input.h \
    portable/include/dd_loop.h \
    portable/include/dd_main.h \
    portable/include/dd_pinit.h \
    portable/include/dd_version.h \
    portable/include/de_audio.h \
    portable/include/de_base.h \
    portable/include/de_bsp.h \
    portable/include/de_console.h \
    portable/include/de_dam.h \
    portable/include/de_defs.h \
    portable/include/de_edit.h \
    portable/include/de_filesys.h \
    portable/include/de_graphics.h \
    portable/include/de_infine.h \
    portable/include/de_misc.h \
    portable/include/de_network.h \
    portable/include/de_platform.h \
    portable/include/de_play.h \
    portable/include/de_refresh.h \
    portable/include/de_render.h \
    portable/include/de_system.h \
    portable/include/de_ui.h \
    portable/include/def_data.h \
    portable/include/def_main.h \
    portable/include/displaymode.h \
    portable/include/displaymode_native.h \
    portable/include/dfilebuilder.h \
    portable/include/edit_bias.h \
    portable/include/edit_bsp.h \
    portable/include/edit_map.h \
    portable/include/fi_main.h \
    portable/include/filedirectory.h \
    portable/include/finaleinterpreter.h \
    portable/include/font.h \
    portable/include/fonts.h \
    portable/include/fs_main.h \
    portable/include/fs_util.h \
    portable/include/game.h \
    portable/include/gamemap.h \
    portable/include/generators.h \
    portable/include/genericfile.h \
    portable/include/gl_defer.h \
    portable/include/gl_deferredapi.h \
    portable/include/gl_draw.h \
    portable/include/gl_hq2x.h \
    portable/include/gl_main.h \
    portable/include/gl_model.h \
    portable/include/gl_pcx.h \
    portable/include/gl_tex.h \
    portable/include/gl_texmanager.h \
    portable/include/gl_tga.h \
    portable/include/gridmap.h \
    portable/include/hedge.h \
    portable/include/image.h \
    portable/include/joystick.h \
    portable/include/json.h \
    portable/include/kdtree.h \
    portable/include/keycode.h \
    portable/include/library.h \
    portable/include/linedef.h \
    portable/include/lumpcache.h \
    portable/include/lumpfile.h \
    portable/include/lumpindex.h \
    portable/include/lumpinfo.h \
    portable/include/m_bams.h \
    portable/include/m_decomp64.h \
    portable/include/m_linkedlist.h \
    portable/include/m_md5.h \
    portable/include/m_misc.h \
    portable/include/m_mus2midi.h \
    portable/include/m_nodepile.h \
    portable/include/m_profiler.h \
    portable/include/m_stack.h \
    portable/include/m_vector.h \
    portable/include/masterserver.h \
    portable/include/monitor.h \
    portable/include/mouse_qt.h \
    portable/include/nativeui.h \
    portable/include/net_buf.h \
    portable/include/net_demo.h \
    portable/include/net_event.h \
    portable/include/net_main.h \
    portable/include/net_msg.h \
    portable/include/p_cmd.h \
    portable/include/p_control.h \
    portable/include/p_dmu.h \
    portable/include/p_intercept.h \
    portable/include/p_mapdata.h \
    portable/include/p_maptypes.h \
    portable/include/p_maputil.h \
    portable/include/p_object.h \
    portable/include/p_objlink.h \
    portable/include/p_particle.h \
    portable/include/p_players.h \
    portable/include/p_polyobjs.h \
    portable/include/p_sight.h \
    portable/include/p_ticker.h \
    portable/include/pathdirectory.h \
    portable/include/pathmap.h \
    portable/include/plane.h \
    portable/include/polyobj.h \
    portable/include/propertyvalue.h \
    portable/include/protocol.h \
    portable/include/r_data.h \
    portable/include/r_draw.h \
    portable/include/r_fakeradio.h \
    portable/include/r_lgrid.h \
    portable/include/r_lumobjs.h \
    portable/include/r_main.h \
    portable/include/r_model.h \
    portable/include/r_shadow.h \
    portable/include/r_sky.h \
    portable/include/r_things.h \
    portable/include/r_util.h \
    portable/include/r_world.h \
    portable/include/render/busyvisual.h \
    portable/include/render/rend_bias.h \
    portable/include/render/rend_clip.h \
    portable/include/render/rend_console.h \
    portable/include/render/rend_decor.h \
    portable/include/render/rend_dynlight.h \
    portable/include/render/rend_fakeradio.h \
    portable/include/render/rend_font.h \
    portable/include/render/rend_halo.h \
    portable/include/render/rend_list.h \
    portable/include/render/rend_main.h \
    portable/include/render/rend_model.h \
    portable/include/render/rend_particle.h \
    portable/include/render/rend_shadow.h \
    portable/include/render/rend_sky.h \
    portable/include/render/rend_sprite.h \
    portable/include/render/vignette.h \
    portable/include/resource/material.h \
    portable/include/resource/materials.h \
    portable/include/resource/materialvariant.h \
    portable/include/resource/texture.h \
    portable/include/resource/textures.h \
    portable/include/resource/texturevariant.h \
    portable/include/resource/texturevariantspecification.h \
    portable/include/resourcenamespace.h \
    portable/include/s_cache.h \
    portable/include/s_environ.h \
    portable/include/s_logic.h \
    portable/include/s_main.h \
    portable/include/s_mus.h \
    portable/include/s_sfx.h \
    portable/include/s_wav.h \
    portable/include/sector.h \
    portable/include/sidedef.h \
    portable/include/stringarray.h \
    portable/include/surface.h \
    portable/include/sv_def.h \
    portable/include/sv_frame.h \
    portable/include/sv_infine.h \
    portable/include/sv_missile.h \
    portable/include/sv_pool.h \
    portable/include/sv_sound.h \
    portable/include/svg.h \
    portable/include/sys_audio.h \
    portable/include/sys_audiod_dummy.h \
    portable/include/sys_console.h \
    portable/include/sys_direc.h \
    portable/include/sys_findfile.h \
    portable/include/sys_input.h \
    portable/include/sys_network.h \
    portable/include/sys_opengl.h \
    portable/include/sys_reslocator.h \
    portable/include/sys_system.h \
    portable/include/tab_anorms.h \
    portable/include/texturecontent.h \
    portable/include/timer.h \
    portable/include/ui2_main.h \
    portable/include/ui_main.h \
    portable/include/ui_mpi.h \
    portable/include/ui_panel.h \
    portable/include/updater.h \
    portable/include/vertex.h \
    portable/include/wadfile.h \
    portable/include/window.h \
    portable/include/zipfile.h \
    portable/include/zonedebug.h \
    portable/src/updater/downloaddialog.h \
    portable/src/updater/processcheckdialog.h \
    portable/src/updater/updateavailabledialog.h \
    portable/src/updater/updaterdialog.h \
    portable/src/updater/updatersettings.h \
    portable/src/updater/updatersettingsdialog.h \
    portable/src/updater/versioninfo.h

INCLUDEPATH += \
    $$DENG_INCLUDE_DIR \
    $$DENG_INCLUDE_DIR/render \
    $$DENG_INCLUDE_DIR/resource \
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
        win32/src/dd_winit.c \
        win32/src/directinput.cpp \
        win32/src/sys_console.c \
        win32/src/sys_findfile.c \
        win32/src/joystick_win32.cpp \
        win32/src/mouse_win32.cpp

    !deng_nodisplaymode: SOURCES += win32/src/displaymode_win32.cpp
}
else:unix {
    # Common Unix (including Mac OS X).
    HEADERS += \
        $$DENG_UNIX_INCLUDE_DIR/dd_uinit.h \
        $$DENG_UNIX_INCLUDE_DIR/sys_path.h

    INCLUDEPATH += $$DENG_UNIX_INCLUDE_DIR

    SOURCES += \
        unix/src/dd_uinit.c \
        unix/src/joystick.c \
        unix/src/sys_console.c \
        unix/src/sys_findfile.c \
        unix/src/sys_path.c
}

macx {
    # Mac OS X only.
    HEADERS += \
        $$DENG_MAC_INCLUDE_DIR/MusicPlayer.h

    OBJECTIVE_SOURCES += \
        mac/src/MusicPlayer.m

    !deng_nodisplaymode: OBJECTIVE_SOURCES += mac/src/displaymode_macx.mm

    INCLUDEPATH += $$DENG_MAC_INCLUDE_DIR
}
else:unix {
    !deng_nodisplaymode {
        # Unix (non-Mac) only.
        SOURCES += \
            unix/src/displaymode_x11.cpp \
            unix/src/imKStoUCS.c
    }
}

deng_nodisplaymode {
    SOURCES += portable/src/displaymode_dummy.c
}

# Platform-independent sources.
SOURCES += \
    portable/src/abstractfile.cpp \
    portable/src/abstractresource.c \
    portable/src/animator.c \
    portable/src/audiodriver.c \
    portable/src/audiodriver_music.c \
    portable/src/b_command.c \
    portable/src/b_context.c \
    portable/src/b_device.c \
    portable/src/b_main.c \
    portable/src/b_util.c \
    portable/src/binarytree.cpp \
    portable/src/bitmapfont.c \
    portable/src/blockmap.c \
    portable/src/blockmapvisual.c \
    portable/src/blockset.c \
    portable/src/busymode.cpp \
    portable/src/map/bspbuilder.cpp \
    portable/src/map/bsp/hplane.cpp \
    portable/src/map/bsp/partitioner.cpp \
    portable/src/map/bsp/superblockmap.cpp \
    portable/src/map/entitydatabase.cpp \
    portable/src/bspleaf.cpp \
    portable/src/bspnode.c \
    portable/src/canvas.cpp \
    portable/src/canvaswindow.cpp \
    portable/src/cbuffer.c \
    portable/src/cl_frame.c \
    portable/src/cl_infine.c \
    portable/src/cl_main.c \
    portable/src/cl_mobj.c \
    portable/src/cl_player.c \
    portable/src/cl_sound.c \
    portable/src/cl_world.c \
    portable/src/colorpalette.c \
    portable/src/con_bar.c \
    portable/src/con_config.c \
    portable/src/con_data.c \
    portable/src/con_main.c \
    portable/src/dam_file.c \
    portable/src/dam_main.c \
    portable/src/dd_games.cpp \
    portable/src/dd_help.c \
    portable/src/dd_init.cpp \
    portable/src/dd_input.c \
    portable/src/dd_loop.c \
    portable/src/dd_main.c \
    portable/src/dd_pinit.c \
    portable/src/dd_plugin.c \
    portable/src/dd_wad.cpp \
    portable/src/def_data.c \
    portable/src/def_main.c \
    portable/src/def_read.c \
    portable/src/dfile.cpp \
    portable/src/dgl_common.c \
    portable/src/dgl_draw.c \
    portable/src/displaymode.cpp \
    portable/src/edit_bias.c \
    portable/src/edit_bsp.cpp \
    portable/src/edit_map.c \
    portable/src/fi_main.c \
    portable/src/filedirectory.cpp \
    portable/src/fileid.cpp \
    portable/src/finaleinterpreter.c \
    portable/src/fonts.c \
    portable/src/fs_main.cpp \
    portable/src/fs_util.cpp \
    portable/src/game.cpp \
    portable/src/gamemap.c \
    portable/src/generators.c \
    portable/src/genericfile.cpp \
    portable/src/gl_defer.c \
    portable/src/gl_deferredapi.c \
    portable/src/gl_draw.c \
    portable/src/gl_drawvectorgraphic.c \
    portable/src/gl_hq2x.c \
    portable/src/gl_main.c \
    portable/src/gl_pcx.c \
    portable/src/gl_tex.c \
    portable/src/gl_texmanager.c \
    portable/src/gl_tga.c \
    portable/src/gridmap.c \
    portable/src/hedge.cpp \
    portable/src/image.cpp \
    portable/src/json.cpp \
    portable/src/kdtree.c \
    portable/src/keycode.cpp \
    portable/src/library.c \
    portable/src/linedef.c \
    portable/src/lumpfile.cpp \
    portable/src/lumpindex.cpp \
    portable/src/m_bams.c \
    portable/src/m_decomp64.c \
    portable/src/m_linkedlist.c \
    portable/src/m_md5.c \
    portable/src/m_misc.c \
    portable/src/m_mus2midi.c \
    portable/src/m_nodepile.c \
    portable/src/m_stack.c \
    portable/src/m_vector.c \
    portable/src/masterserver.cpp \
    portable/src/materialarchive.c \
    portable/src/monitor.c \
    portable/src/mouse_qt.cpp \
    portable/src/nativeui.cpp \
    portable/src/net_buf.c \
    portable/src/net_demo.c \
    portable/src/net_event.c \
    portable/src/net_main.c \
    portable/src/net_msg.c \
    portable/src/net_ping.c \
    portable/src/p_cmd.c \
    portable/src/p_control.c \
    portable/src/p_data.cpp \
    portable/src/p_dmu.c \
    portable/src/p_intercept.c \
    portable/src/p_maputil.c \
    portable/src/p_mobj.c \
    portable/src/p_objlink.c \
    portable/src/p_particle.c \
    portable/src/p_players.c \
    portable/src/p_polyobjs.c \
    portable/src/p_sight.c \
    portable/src/p_think.c \
    portable/src/p_ticker.c \
    portable/src/pathdirectory.cpp \
    portable/src/pathdirectorynode.cpp \
    portable/src/pathmap.c \
    portable/src/plane.c \
    portable/src/point.c \
    portable/src/polyobj.c \
    portable/src/protocol.c \
    portable/src/propertyvalue.cpp \
    portable/src/r_data.c \
    portable/src/r_draw.c \
    portable/src/r_fakeradio.c \
    portable/src/r_lgrid.c \
    portable/src/r_lumobjs.c \
    portable/src/r_main.c \
    portable/src/r_model.c \
    portable/src/r_shadow.c \
    portable/src/r_sky.c \
    portable/src/r_things.c \
    portable/src/r_util.c \
    portable/src/r_world.c \
    portable/src/rect.c \
    portable/src/render/busyvisual.c \
    portable/src/render/rend_bias.c \
    portable/src/render/rend_clip.cpp \
    portable/src/render/rend_console.c \
    portable/src/render/rend_decor.c \
    portable/src/render/rend_dynlight.c \
    portable/src/render/rend_fakeradio.c \
    portable/src/render/rend_font.c \
    portable/src/render/rend_halo.c \
    portable/src/render/rend_list.c \
    portable/src/render/rend_main.c \
    portable/src/render/rend_model.c \
    portable/src/render/rend_particle.c \
    portable/src/render/rend_shadow.c \
    portable/src/render/rend_sky.c \
    portable/src/render/rend_sprite.c \
    portable/src/render/vignette.c \
    portable/src/resource/material.cpp \
    portable/src/resource/materials.cpp \
    portable/src/resource/materialvariant.cpp \
    portable/src/resource/texture.cpp \
    portable/src/resource/textures.cpp \
    portable/src/resource/texturevariant.cpp \
    portable/src/resourcenamespace.c \
    portable/src/s_cache.c \
    portable/src/s_environ.cpp \
    portable/src/s_logic.c \
    portable/src/s_main.c \
    portable/src/s_mus.c \
    portable/src/s_sfx.c \
    portable/src/s_wav.c \
    portable/src/sector.c \
    portable/src/sidedef.c \
    portable/src/size.c \
    portable/src/stringarray.cpp \
    portable/src/surface.c \
    portable/src/sv_frame.c \
    portable/src/sv_infine.c \
    portable/src/sv_main.c \
    portable/src/sv_missile.c \
    portable/src/sv_pool.c \
    portable/src/sv_sound.cpp \
    portable/src/svg.c \
    portable/src/sys_audiod_dummy.c \
    portable/src/sys_direc.c \
    portable/src/sys_input.c \
    portable/src/sys_network.c \
    portable/src/sys_opengl.c \
    portable/src/sys_reslocator.c \
    portable/src/sys_system.c \
    portable/src/tab_tables.c \
    portable/src/timer.cpp \
    portable/src/ui2_main.c \
    portable/src/ui_main.c \
    portable/src/ui_mpi.c \
    portable/src/ui_panel.c \
    portable/src/updater.cpp \
    portable/src/updater/downloaddialog.cpp \
    portable/src/updater/processcheckdialog.cpp \
    portable/src/updater/updateavailabledialog.cpp \
    portable/src/updater/updaterdialog.cpp \
    portable/src/updater/updatersettings.cpp \
    portable/src/updater/updatersettingsdialog.cpp \
    portable/src/uri.c \
    portable/src/vertex.cpp \
    portable/src/wadfile.cpp \
    portable/src/window.cpp \
    portable/src/zipfile.cpp \
    portable/src/zonedebug.c

!deng_nosdlmixer:!deng_nosdl {
    HEADERS += portable/include/sys_audiod_sdlmixer.h
    SOURCES += portable/src/sys_audiod_sdlmixer.c
}

# Use the fixed-point math from libcommon.
# TODO: Move it to the engine.
SOURCES += ../plugins/common/src/m_fixed.c

OTHER_FILES += \
    data/cphelp.txt \
    portable/include/mapdata.hs \
    portable/include/template.h.template \
    portable/src/template.c.template

# Resources ------------------------------------------------------------------

data.files = $$OUT_PWD/../doomsday.pk3

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
        mac/res/English.lproj \
        mac/res/deng.icns

    data.path         = $${res.path}
    startupdata.path  = $${res.path}/data
    startupfonts.path = $${res.path}/data/fonts
    startupgfx.path   = $${res.path}/data/graphics

    QMAKE_BUNDLE_DATA += res data startupfonts startupdata startupgfx

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

win32 {
    # Windows installation.
    INSTALLS += target data startupdata startupgfx startupfonts license icon

    target.path = $$DENG_LIB_DIR

    data.path = $$DENG_DATA_DIR
    startupdata.path = $$DENG_DATA_DIR
    startupfonts.path = $$DENG_DATA_DIR/fonts
    startupgfx.path = $$DENG_DATA_DIR/graphics

    license.files = doc/LICENSE
    license.path = $$DENG_DOCS_DIR

    icon.files = win32/res/doomsday.ico
    icon.path = $$DENG_DATA_DIR/graphics
}
else:unix:!macx {
    # Generic Unix installation.
    INSTALLS += target data startupdata startupgfx startupfonts readme

    target.path = $$DENG_BIN_DIR

    data.path = $$DENG_DATA_DIR
    startupdata.path = $$DENG_DATA_DIR
    startupfonts.path = $$DENG_DATA_DIR/fonts
    startupgfx.path = $$DENG_DATA_DIR/graphics

    readme.files = ../doc/output/doomsday.6
    readme.path = $$PREFIX/share/man/man6
}
