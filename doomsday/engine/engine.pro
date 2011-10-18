# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011 Daniel Swanson <danij@dengine.net>

TEMPLATE = app
TARGET = doomsday

# Build Configuration --------------------------------------------------------

include(../config.pri)

VERSION = $$DENG_VERSION

echo(Doomsday version $${DENG_VERSION}.)

# External Dependencies ------------------------------------------------------

include(../dep_sdl.pri)
include(../dep_opengl.pri)
include(../dep_zlib.pri)
include(../dep_png.pri)
include(../dep_curses.pri)
include(../dep_curl.pri)
include(../dep_lzss.pri)
win32 {
    include(../dep_directx.pri)
}
include(../dep_deng2.pri)

QT += core network

# Definitions ----------------------------------------------------------------

DEFINES += __DOOMSDAY__

!isEmpty(DENG_BUILD) {
    echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    echo(DENG_BUILD is not defined.)
}

win32 {
    DEFINES += WIN32_GAMMA

    RC_FILE = win32/res/doomsday.rc
    OTHER_FILES += api/doomsday.def
}
else:macx {
}
else {
    # Generic Unix.
    DEFINES += DENG_BASE_DIR=\\\"$${DENG_BASE_DIR}/\\\"
    DEFINES += DENG_LIBRARY_DIR=\\\"$${DENG_LIB_DIR}/\\\"
}

# Build Configuration --------------------------------------------------------

deng_writertypecheck {
    DEFINES += DENG_WRITER_TYPECHECK
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
    api/dd_api.h \
    api/dd_gl.h \
    api/dd_maptypes.h \
    api/dd_plugin.h \
    api/dd_share.h \
    api/dd_types.h \
    api/dd_world.h \
    api/doomsday.h \
    api/reader.h \
    api/smoother.h \
    api/sys_audiod.h \
    api/sys_audiod_mus.h \
    api/sys_audiod_sfx.h \
    api/writer.h

DENG_HEADERS = \
    portable/include/b_command.h \
    portable/include/b_context.h \
    portable/include/b_device.h \
    portable/include/b_main.h \
    portable/include/b_util.h \
    portable/include/bsp_edge.h \
    portable/include/bsp_intersection.h \
    portable/include/bsp_main.h \
    portable/include/bsp_map.h \
    portable/include/bsp_node.h \
    portable/include/bsp_superblock.h \
    portable/include/cl_def.h \
    portable/include/cl_frame.h \
    portable/include/cl_mobj.h \
    portable/include/cl_player.h \
    portable/include/cl_sound.h \
    portable/include/cl_world.h \
    portable/include/con_bar.h \
    portable/include/con_bind.h \
    portable/include/con_buffer.h \
    portable/include/con_busy.h \
    portable/include/con_config.h \
    portable/include/con_decl.h \
    portable/include/con_main.h \
    portable/include/dam_blockmap.h \
    portable/include/dam_file.h \
    portable/include/dam_main.h \
    portable/include/dd_def.h \
    portable/include/dd_help.h \
    portable/include/dd_input.h \
    portable/include/dd_loop.h \
    portable/include/dd_main.h \
    portable/include/dd_pinit.h \
    portable/include/dd_version.h \
    portable/include/dd_wad.h \
    portable/include/dd_zip.h \
    portable/include/dd_zone.h \
    portable/include/de_audio.h \
    portable/include/de_base.h \
    portable/include/de_bsp.h \
    portable/include/de_console.h \
    portable/include/de_dam.h \
    portable/include/de_defs.h \
    portable/include/de_edit.h \
    portable/include/de_graphics.h \
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
    portable/include/def_share.h \
    portable/include/edit_bias.h \
    portable/include/edit_map.h \
    portable/include/gl_defer.h \
    portable/include/gl_draw.h \
    portable/include/gl_font.h \
    portable/include/gl_hq2x.h \
    portable/include/gl_main.h \
    portable/include/gl_model.h \
    portable/include/gl_pcx.h \
    portable/include/gl_png.h \
    portable/include/gl_tex.h \
    portable/include/gl_texmanager.h \
    portable/include/gl_tga.h \
    portable/include/m_args.h \
    portable/include/m_bams.h \
    portable/include/m_binarytree.h \
    portable/include/m_decomp64.h \
    portable/include/m_filehash.h \
    portable/include/m_gridmap.h \
    portable/include/m_huffman.h \
    portable/include/m_linkedlist.h \
    portable/include/m_md5.h \
    portable/include/m_misc.h \
    portable/include/m_mus2midi.h \
    portable/include/m_nodepile.h \
    portable/include/m_profiler.h \
    portable/include/m_stack.h \
    portable/include/m_string.h \
    portable/include/m_vector.h \
    portable/include/net_buf.h \
    portable/include/net_demo.h \
    portable/include/net_event.h \
    portable/include/net_main.h \
    portable/include/net_msg.h \
    portable/include/p_bmap.h \
    portable/include/p_cmd.h \
    portable/include/p_control.h \
    portable/include/p_dmu.h \
    portable/include/p_intercept.h \
    portable/include/p_linedef.h \
    portable/include/p_mapdata.h \
    portable/include/p_maptypes.h \
    portable/include/p_maputil.h \
    portable/include/p_material.h \
    portable/include/p_materialmanager.h \
    portable/include/p_object.h \
    portable/include/p_objlink.h \
    portable/include/p_particle.h \
    portable/include/p_plane.h \
    portable/include/p_players.h \
    portable/include/p_polyob.h \
    portable/include/p_polyobj.h \
    portable/include/p_sector.h \
    portable/include/p_seg.h \
    portable/include/p_sidedef.h \
    portable/include/p_sight.h \
    portable/include/p_subsector.h \
    portable/include/p_surface.h \
    portable/include/p_think.h \
    portable/include/p_ticker.h \
    portable/include/p_vertex.h \
    portable/include/r_data.h \
    portable/include/r_draw.h \
    portable/include/r_extres.h \
    portable/include/r_lgrid.h \
    portable/include/r_lumobjs.h \
    portable/include/r_main.h \
    portable/include/r_model.h \
    portable/include/r_shadow.h \
    portable/include/r_sky.h \
    portable/include/r_things.h \
    portable/include/r_util.h \
    portable/include/r_world.h \
    portable/include/rend_bias.h \
    portable/include/rend_clip.h \
    portable/include/rend_console.h \
    portable/include/rend_decor.h \
    portable/include/rend_dyn.h \
    portable/include/rend_fakeradio.h \
    portable/include/rend_halo.h \
    portable/include/rend_list.h \
    portable/include/rend_main.h \
    portable/include/rend_model.h \
    portable/include/rend_particle.h \
    portable/include/rend_shadow.h \
    portable/include/rend_sky.h \
    portable/include/rend_sprite.h \
    portable/include/s_cache.h \
    portable/include/s_environ.h \
    portable/include/s_logic.h \
    portable/include/s_main.h \
    portable/include/s_mus.h \
    portable/include/s_sfx.h \
    portable/include/s_wav.h \
    portable/include/sv_def.h \
    portable/include/sv_frame.h \
    portable/include/sv_missile.h \
    portable/include/sv_pool.h \
    portable/include/sv_sound.h \
    portable/include/sys_audio.h \
    portable/include/sys_audiod_dummy.h \
    portable/include/sys_audiod_loader.h \
    portable/include/sys_audiod_sdlmixer.h \
    portable/include/sys_console.h \
    portable/include/sys_direc.h \
    portable/include/sys_file.h \
    portable/include/sys_findfile.h \
    portable/include/sys_input.h \
    portable/include/sys_master.h \
    portable/include/sys_network.h \
    portable/include/sys_opengl.h \
    portable/include/sys_sock.h \
    portable/include/sys_system.h \
    portable/include/sys_timer.h \
    portable/include/sys_window.h \
    portable/include/tab_anorms.h \
    portable/include/ui_main.h \
    portable/include/ui_mpi.h \
    portable/include/ui_panel.h

# Platform-specific configuration.
unix:!win32 {
    DENG_PLATFORM_HEADERS += \
        $$DENG_UNIX_INCLUDE_DIR/dd_uinit.h \
        $$DENG_UNIX_INCLUDE_DIR/sys_dylib.h \
        $$DENG_UNIX_INCLUDE_DIR/sys_findfile.h \
        $$DENG_UNIX_INCLUDE_DIR/sys_path.h

    INCLUDEPATH += $$DENG_UNIX_INCLUDE_DIR
}
macx {
    DENG_PLATFORM_HEADERS += \
        $$DENG_MAC_INCLUDE_DIR/DoomsdayRunner.h \
        $$DENG_MAC_INCLUDE_DIR/MusicPlayer.h \
        $$DENG_MAC_INCLUDE_DIR/SDLMain.h \
        $$DENG_MAC_INCLUDE_DIR/StartupWindowController.h

    SOURCES += \
        mac/src/DoomsdayRunner.m \
        mac/src/MusicPlayer.m \
        mac/src/SDLMain.m \
        mac/src/StartupWindowController.m

    INCLUDEPATH += $$DENG_MAC_INCLUDE_DIR
}
win32 {
    DENG_PLATFORM_HEADERS += \
        $$DENG_WIN_INCLUDE_DIR/dd_winit.h \
        $$DENG_WIN_INCLUDE_DIR/resource.h

    INCLUDEPATH += $$DENG_WIN_INCLUDE_DIR
}

INCLUDEPATH += \
    $$DENG_INCLUDE_DIR \
    $$DENG_API_DIR

HEADERS += \
    $$DENG_API_HEADERS \
    $$DENG_PLATFORM_HEADERS \
    $$DENG_HEADERS

DENG_UNIX_SOURCES += \
    portable/src/sys_sdl_window.c \
    unix/src/dd_uinit.c \
    unix/src/sys_audiod_loader.c \
    unix/src/sys_console.c \
    unix/src/sys_dylib.c \
    unix/src/sys_findfile.c \
    unix/src/sys_input.c \
    unix/src/sys_path.c

DENG_WIN32_SOURCES += \
    win32/src/dd_winit.c \
    win32/src/sys_audiod_loader.c \
    win32/src/sys_console.c \
    win32/src/sys_findfile.c \
    win32/src/sys_input.c \
    win32/src/sys_window.c

SOURCES += \
    portable/src/b_command.c \
    portable/src/b_context.c \
    portable/src/b_device.c \
    portable/src/b_main.c \
    portable/src/b_util.c \
    portable/src/bsp_edge.c \
    portable/src/bsp_intersection.c \
    portable/src/bsp_main.c \
    portable/src/bsp_map.c \
    portable/src/bsp_node.c \
    portable/src/bsp_superblock.c \
    portable/src/cl_frame.c \
    portable/src/cl_main.c \
    portable/src/cl_mobj.c \
    portable/src/cl_player.c \
    portable/src/cl_sound.c \
    portable/src/cl_world.c \
    portable/src/con_bar.c \
    portable/src/con_buffer.c \
    portable/src/con_busy.c \
    portable/src/con_config.c \
    portable/src/con_data.c \
    portable/src/con_main.c \
    portable/src/dam_blockmap.c \
    portable/src/dam_file.c \
    portable/src/dam_main.c \
    portable/src/dd_help.c \
    portable/src/dd_input.c \
    portable/src/dd_loop.c \
    portable/src/dd_main.c \
    portable/src/dd_pinit.c \
    portable/src/dd_plugin.c \
    portable/src/dd_wad.c \
    portable/src/dd_zip.c \
    portable/src/dd_zone.c \
    portable/src/def_data.c \
    portable/src/def_main.c \
    portable/src/def_read.c \
    portable/src/dgl_common.c \
    portable/src/dgl_draw.c \
    portable/src/dgl_texture.c \
    portable/src/edit_bias.c \
    portable/src/edit_map.c \
    portable/src/gl_defer.c \
    portable/src/gl_draw.c \
    portable/src/gl_font.c \
    portable/src/gl_hq2x.c \
    portable/src/gl_main.c \
    portable/src/gl_pcx.c \
    portable/src/gl_png.c \
    portable/src/gl_tex.c \
    portable/src/gl_texmanager.c \
    portable/src/gl_tga.c \
    portable/src/m_args.c \
    portable/src/m_bams.c \
    portable/src/m_binarytree.c \
    portable/src/m_decomp64.c \
    portable/src/m_filehash.c \
    portable/src/m_gridmap.c \
    portable/src/m_huffman.c \
    portable/src/m_linkedlist.c \
    portable/src/m_md5.c \
    portable/src/m_misc.c \
    portable/src/m_mus2midi.c \
    portable/src/m_nodepile.c \
    portable/src/m_stack.c \
    portable/src/m_string.c \
    portable/src/m_vector.c \
    portable/src/net_buf.c \
    portable/src/net_demo.c \
    portable/src/net_event.c \
    portable/src/net_main.c \
    portable/src/net_msg.c \
    portable/src/net_ping.c \
    portable/src/p_bmap.c \
    portable/src/p_cmd.c \
    portable/src/p_control.c \
    portable/src/p_data.c \
    portable/src/p_dmu.c \
    portable/src/p_intercept.c \
    portable/src/p_linedef.c \
    portable/src/p_maputil.c \
    portable/src/p_material.c \
    portable/src/p_materialmanager.c \
    portable/src/p_mobj.c \
    portable/src/p_objlink.c \
    portable/src/p_particle.c \
    portable/src/p_plane.c \
    portable/src/p_players.c \
    portable/src/p_polyob.c \
    portable/src/p_sector.c \
    portable/src/p_seg.c \
    portable/src/p_sidedef.c \
    portable/src/p_sight.c \
    portable/src/p_subsector.c \
    portable/src/p_surface.c \
    portable/src/p_think.c \
    portable/src/p_ticker.c \
    portable/src/p_vertex.c \
    portable/src/r_data.c \
    portable/src/r_draw.c \
    portable/src/r_extres.c \
    portable/src/r_lgrid.c \
    portable/src/r_lumobjs.c \
    portable/src/r_main.c \
    portable/src/r_model.c \
    portable/src/r_shadow.c \
    portable/src/r_sky.c \
    portable/src/r_things.c \
    portable/src/r_util.c \
    portable/src/r_world.c \
    portable/src/reader.c \
    portable/src/rend_bias.c \
    portable/src/rend_clip.c \
    portable/src/rend_console.c \
    portable/src/rend_decor.c \
    portable/src/rend_dyn.c \
    portable/src/rend_fakeradio.c \
    portable/src/rend_halo.c \
    portable/src/rend_list.c \
    portable/src/rend_main.c \
    portable/src/rend_model.c \
    portable/src/rend_particle.c \
    portable/src/rend_shadow.c \
    portable/src/rend_sky.c \
    portable/src/rend_sprite.c \
    portable/src/s_cache.c \
    portable/src/s_environ.c \
    portable/src/s_logic.c \
    portable/src/s_main.c \
    portable/src/s_mus.c \
    portable/src/s_sfx.c \
    portable/src/s_wav.c \
    portable/src/smoother.c \
    portable/src/sv_frame.c \
    portable/src/sv_main.c \
    portable/src/sv_missile.c \
    portable/src/sv_pool.c \
    portable/src/sv_sound.c \
    portable/src/sys_audiod_dummy.c \
    portable/src/sys_audiod_sdlmixer.c \
    portable/src/sys_direc.c \
    portable/src/sys_filein.c \
    portable/src/sys_master.c \
    portable/src/sys_network.c \
    portable/src/sys_opengl.c \
    portable/src/sys_sock.c \
    portable/src/sys_system.c \
    portable/src/sys_timer.c \
    portable/src/tab_tables.c \
    portable/src/ui_main.c \
    portable/src/ui_mpi.c \
    portable/src/ui_panel.c \
    portable/src/writer.c

unix {
    SOURCES += $$DENG_UNIX_SOURCES
}

win32 {
    SOURCES += $$DENG_WIN32_SOURCES
}

# Use the fixed-point math from libcommon.
# TODO: Move it to the engine.
SOURCES += ../plugins/common/src/m_fixed.c

# Resources ------------------------------------------------------------------

data.files = $$OUT_PWD/../doomsday.pk3

startupfonts.files = \
    data/fonts/normal12.dfn \
    data/fonts/normal18.dfn

startupgfx.files = \
    data/graphics/background.pcx \
    data/graphics/loading1.png \
    data/graphics/loading2.png \
    data/graphics/logo.png

macx {
    res.files = \
        mac/res/English.lproj \
        mac/res/Startup.nib \
        mac/res/deng.icns
    res.path = Contents/Resources

    data.path = $${res.path}
    startupfonts.path = $${res.path}/Data/Fonts
    startupgfx.path = $${res.path}/Data/Graphics

    QMAKE_BUNDLE_DATA += res data startupfonts startupgfx

    QMAKE_INFO_PLIST = ../build/mac/Info.plist

    # Since qmake is unable to copy directories as bundle data, let's copy
    # the frameworks manually.
    FW_DIR = \"$${OUT_PWD}/doomsday.app/Contents/Frameworks/\"
    doPostLink("rm -rf $$FW_DIR")
    doPostLink("mkdir $$FW_DIR")
    doPostLink("cp -fRp $${SDL_FRAMEWORK_DIR}/SDL.framework $$FW_DIR")
    doPostLink("cp -fRp $${SDL_FRAMEWORK_DIR}/SDL_mixer.framework $$FW_DIR")

    # Bundle the Qt frameworks.
    doPostLink("cp -fRp $$[QT_INSTALL_LIBS]/QtCore.framework $$FW_DIR")
    doPostLink("cp -fRp $$[QT_INSTALL_LIBS]/QtNetwork.framework $$FW_DIR")

    # libdeng2 dynamic library.
    doPostLink("cp -fRp $$OUT_PWD/../libdeng2/libdeng2*dylib $$FW_DIR")

    # Fix the dynamic linker paths so they point to ../Frameworks/.
    defineTest(fixInstallName) {
        doPostLink("install_name_tool -change $$1 @executable_path/../Frameworks/$$1 doomsday.app/Contents/MacOS/doomsday")
    }
    fixInstallName("libdeng2.2.dylib")
    fixInstallName("QtCore.framework/Versions/4/QtCore")
    fixInstallName("QtNetwork.framework/Versions/4/QtNetwork")
}

# Installation ---------------------------------------------------------------

win32 {
    # Windows installation.
    INSTALLS += target data startupgfx startupfonts license

    target.path = $$DENG_LIB_DIR

    data.path = $$DENG_DATA_DIR
    startupfonts.path = $$DENG_DATA_DIR/fonts
    startupgfx.path = $$DENG_DATA_DIR/graphics

    license.files = doc/LICENSE
    license.path = $$DENG_DOCS_DIR
}
else:unix:!macx {
    # Generic Unix installation.
    INSTALLS += target data startupgfx desktop

    target.path = $$DENG_BIN_DIR

    data.path = $$DENG_DATA_DIR
    startupfonts.path = $$DENG_DATA_DIR/fonts
    startupgfx.path = $$DENG_DATA_DIR/graphics

    desktop.files = ../../distrib/linux/doomsday-engine.desktop
    desktop.path = /usr/share/applications
}
