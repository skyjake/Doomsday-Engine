# The Doomsday Engine Project
# Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2013 Daniel Swanson <danij@dengine.net>

TEMPLATE = app
TARGET = doomsday-server

# Build Configuration --------------------------------------------------------

include(../config.pri)

VERSION = $$DENG_VERSION

echo(Doomsday Server $${DENG_VERSION}.)

CONFIG -= app_bundle

# External Dependencies ------------------------------------------------------

CONFIG += deng_nogui

include(../dep_zlib.pri)
include(../dep_curses.pri)
include(../dep_lzss.pri)
win32 {
    include(../dep_directx.pri)
}
include(../dep_deng2.pri)
include(../dep_deng.pri)

# TODO: Get rid of this. The dedicated server should need no GL code.
win32: include(../dep_opengl.pri)

# Definitions ----------------------------------------------------------------

DEFINES += __DOOMSDAY__ __SERVER__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

win32 {
    RC_FILE = res/windows/doomsday.rc
    OTHER_FILES += ../engine/api/doomsday.def $$RC_FILE
}
else:macx {
}
else {
    DEFINES += __USE_BSD _GNU_SOURCE=1

    # Generic Unix.
    QMAKE_LFLAGS += -rdynamic

    LIBS += -lX11 # TODO: Get rid of this! -jk

    !freebsd-*: LIBS += -ldl
}

# Linking --------------------------------------------------------------------

win32 {
    QMAKE_LFLAGS += \
        /NODEFAULTLIB:libcmt \
        /DEF:\"$$DENG_API_DIR/doomsday.def\" \
        /IMPLIB:\"$$OUT_PWD/../server/doomsday-server.lib\"

    LIBS += -lkernel32 -lgdi32 -lole32 -luser32 -lwsock32 \
        -lopengl32 -lglu32
}
else:macx {
    #useFramework(Cocoa)
}
else {
    # Allow exporting symbols out of the main executable.
    QMAKE_LFLAGS += -rdynamic
}

# Source Files ---------------------------------------------------------------

SRC = ../engine

DENG_API_HEADERS = \
    $$SRC/api/busytask.h \
    $$SRC/api/dd_api.h \
    $$SRC/api/dd_fontrenderer.h \
    $$SRC/api/dd_gl.h \
    $$SRC/api/dd_infine.h \
    $$SRC/api/dd_maptypes.h \
    $$SRC/api/dd_plugin.h \
    $$SRC/api/dd_share.h \
    $$SRC/api/dd_types.h \
    $$SRC/api/dd_ui.h \
    $$SRC/api/dd_vectorgraphic.h \
    $$SRC/api/dd_version.h \
    $$SRC/api/dd_wad.h \
    $$SRC/api/dd_world.h \
    $$SRC/api/def_share.h \
    $$SRC/api/dengproject.h \
    $$SRC/api/doomsday.h \
    $$SRC/api/filehandle.h \
    $$SRC/api/filetype.h \
    $$SRC/api/materialarchive.h \
    $$SRC/api/resourceclass.h \
    $$SRC/api/sys_audiod.h \
    $$SRC/api/sys_audiod_mus.h \
    $$SRC/api/sys_audiod_sfx.h \
    $$SRC/api/thinker.h \
    $$SRC/api/uri.h

# Convenience headers.
DENG_HEADERS += \
    $$SRC/include/BspBuilder \
    $$SRC/include/EntityDatabase \

# Private headers.
DENG_HEADERS += \
    include/server_dummies.h \
    $$SRC/include/audio/s_cache.h \
    $$SRC/include/audio/s_environ.h \
    $$SRC/include/audio/s_logic.h \
    $$SRC/include/audio/s_main.h \
    $$SRC/include/audio/s_wav.h \
    $$SRC/include/busymode.h \
    $$SRC/include/cbuffer.h \
    $$SRC/include/color.h \
    $$SRC/include/con_bar.h \
    $$SRC/include/con_bind.h \
    $$SRC/include/con_config.h \
    $$SRC/include/con_main.h \
    $$SRC/include/dd_def.h \
    $$SRC/include/dd_games.h \
    $$SRC/include/dd_help.h \
    $$SRC/include/dd_loop.h \
    $$SRC/include/dd_main.h \
    $$SRC/include/dd_pinit.h \
    $$SRC/include/de_audio.h \
    $$SRC/include/de_base.h \
    $$SRC/include/de_bsp.h \
    $$SRC/include/de_console.h \
    $$SRC/include/de_dam.h \
    $$SRC/include/de_defs.h \
    $$SRC/include/de_edit.h \
    $$SRC/include/de_filesys.h \
    $$SRC/include/de_graphics.h \
    $$SRC/include/de_infine.h \
    $$SRC/include/de_misc.h \
    $$SRC/include/de_network.h \
    $$SRC/include/de_platform.h \
    $$SRC/include/de_play.h \
    $$SRC/include/de_resource.h \
    $$SRC/include/de_render.h \
    $$SRC/include/de_system.h \
    $$SRC/include/de_ui.h \
    $$SRC/include/def_data.h \
    $$SRC/include/def_main.h \
    $$SRC/include/dualstring.h \
    $$SRC/include/edit_bsp.h \
    $$SRC/include/edit_map.h \
    $$SRC/include/filesys/file.h \
    $$SRC/include/filesys/filehandlebuilder.h \
    $$SRC/include/filesys/fileinfo.h \
    $$SRC/include/filesys/fs_main.h \
    $$SRC/include/filesys/fs_util.h \
    $$SRC/include/filesys/lumpindex.h \
    $$SRC/include/filesys/manifest.h \
    $$SRC/include/filesys/searchpath.h \
    $$SRC/include/filesys/sys_direc.h \
    $$SRC/include/filesys/sys_findfile.h \
    $$SRC/include/game.h \
    $$SRC/include/gl/gl_texmanager.h \
    $$SRC/include/gridmap.h \
    $$SRC/include/kdtree.h \
    $$SRC/include/library.h \
    $$SRC/include/m_bams.h \
    $$SRC/include/m_decomp64.h \
    $$SRC/include/m_misc.h \
    $$SRC/include/m_nodepile.h \
    $$SRC/include/m_profiler.h \
    $$SRC/include/m_stack.h \
    $$SRC/include/map/blockmap.h \
    $$SRC/include/map/bsp/bsptreenode.h \
    $$SRC/include/map/bsp/hedgeinfo.h \
    $$SRC/include/map/bsp/hedgeintercept.h \
    $$SRC/include/map/bsp/hedgetip.h \
    $$SRC/include/map/bsp/hplane.h \
    $$SRC/include/map/bsp/linedefinfo.h \
    $$SRC/include/map/bsp/partitioncost.h \
    $$SRC/include/map/bsp/partitioner.h \
    $$SRC/include/map/bsp/superblockmap.h \
    $$SRC/include/map/bspbuilder.h \
    $$SRC/include/map/bspleaf.h \
    $$SRC/include/map/bspnode.h \
    $$SRC/include/map/dam_file.h \
    $$SRC/include/map/dam_main.h \
    $$SRC/include/map/entitydatabase.h \
    $$SRC/include/map/gamemap.h \
    $$SRC/include/map/generators.h \
    $$SRC/include/map/hedge.h \
    $$SRC/include/map/linedef.h \
    $$SRC/include/map/p_dmu.h \
    $$SRC/include/map/p_intercept.h \
    $$SRC/include/map/p_mapdata.h \
    $$SRC/include/map/p_maptypes.h \
    $$SRC/include/map/p_maputil.h \
    $$SRC/include/map/p_object.h \
    $$SRC/include/map/p_objlink.h \
    $$SRC/include/map/p_particle.h \
    $$SRC/include/map/p_players.h \
    $$SRC/include/map/p_polyobjs.h \
    $$SRC/include/map/p_sight.h \
    $$SRC/include/map/p_ticker.h \
    $$SRC/include/map/plane.h \
    $$SRC/include/map/polyobj.h \
    $$SRC/include/map/propertyvalue.h \
    $$SRC/include/map/r_world.h \
    $$SRC/include/map/sector.h \
    $$SRC/include/map/sidedef.h \
    $$SRC/include/map/surface.h \
    $$SRC/include/map/vertex.h \
    $$SRC/include/network/masterserver.h \
    $$SRC/include/network/monitor.h \
    $$SRC/include/network/net_buf.h \
    $$SRC/include/network/net_event.h \
    $$SRC/include/network/net_main.h \
    $$SRC/include/network/net_msg.h \
    $$SRC/include/network/protocol.h \
    $$SRC/include/network/sys_network.h \
    $$SRC/include/r_util.h \
    $$SRC/include/render/r_main.h \
    $$SRC/include/render/r_things.h \
    $$SRC/include/render/rend_main.h \
    $$SRC/include/resource/animgroups.h \
    $$SRC/include/resource/colorpalette.h \
    $$SRC/include/resource/colorpalettes.h \
    $$SRC/include/resource/compositetexture.h \
    $$SRC/include/resource/font.h \
    $$SRC/include/resource/hq2x.h \
    $$SRC/include/resource/image.h \
    $$SRC/include/resource/lumpcache.h \
    $$SRC/include/resource/material.h \
    $$SRC/include/resource/materials.h \
    $$SRC/include/resource/materialscheme.h \
    $$SRC/include/resource/materialsnapshot.h \
    $$SRC/include/resource/materialvariant.h \
    $$SRC/include/resource/patch.h \
    $$SRC/include/resource/patchname.h \
    $$SRC/include/resource/pcx.h \
    $$SRC/include/resource/r_data.h \
    $$SRC/include/resource/rawtexture.h \
    $$SRC/include/resource/texture.h \
    $$SRC/include/resource/texturemanifest.h \
    $$SRC/include/resource/texturescheme.h \
    $$SRC/include/resource/textures.h \
    $$SRC/include/resource/texturevariant.h \
    $$SRC/include/resource/texturevariantspecification.h \
    $$SRC/include/resource/tga.h \
    $$SRC/include/resource/wad.h \
    $$SRC/include/resource/zip.h \
    $$SRC/include/server/sv_def.h \
    $$SRC/include/server/sv_frame.h \
    $$SRC/include/server/sv_infine.h \
    $$SRC/include/server/sv_missile.h \
    $$SRC/include/server/sv_pool.h \
    $$SRC/include/server/sv_sound.h \
    $$SRC/include/sys_console.h \
    $$SRC/include/sys_system.h \
    $$SRC/include/tab_anorms.h \
    $$SRC/include/ui/b_command.h \
    $$SRC/include/ui/b_context.h \
    $$SRC/include/ui/b_device.h \
    $$SRC/include/ui/b_main.h \
    $$SRC/include/ui/b_util.h \
    $$SRC/include/ui/busyvisual.h \
    $$SRC/include/ui/canvas.h \
    $$SRC/include/ui/canvaswindow.h \
    $$SRC/include/ui/consolewindow.h \
    $$SRC/include/ui/dd_input.h \
    $$SRC/include/ui/displaymode.h \
    $$SRC/include/ui/displaymode_native.h \
    $$SRC/include/ui/fi_main.h \
    $$SRC/include/ui/finaleinterpreter.h \
    $$SRC/include/ui/keycode.h \
    $$SRC/include/ui/p_control.h \
    $$SRC/include/ui/sys_input.h \
    $$SRC/include/ui/ui2_main.h \
    $$SRC/include/ui/window.h \
    $$SRC/include/updater.h \
    $$SRC/include/uri.hh \
    $$SRC/src/updater/downloaddialog.h \
    $$SRC/src/updater/processcheckdialog.h \
    $$SRC/src/updater/updateavailabledialog.h \
    $$SRC/src/updater/updaterdialog.h \
    $$SRC/src/updater/updatersettings.h \
    $$SRC/src/updater/updatersettingsdialog.h \
    $$SRC/src/updater/versioninfo.h

INCLUDEPATH += \
    include \
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
        $$DENG_WIN_INCLUDE_DIR/resource.h

    INCLUDEPATH += $$DENG_WIN_INCLUDE_DIR

    SOURCES += \
        $$SRC/src/windows/dd_winit.c \
        $$SRC/src/windows/directinput.cpp \
        $$SRC/src/windows/sys_console.c \
        $$SRC/src/windows/sys_findfile.c 
}
else:unix {
    # Common Unix (including Mac OS X).
    HEADERS += \
        $$DENG_UNIX_INCLUDE_DIR/dd_uinit.h \
        $$DENG_UNIX_INCLUDE_DIR/sys_path.h

    INCLUDEPATH += $$DENG_UNIX_INCLUDE_DIR

    SOURCES += \
        $$SRC/src/unix/dd_uinit.c \
        $$SRC/src/unix/sys_console.c \
        $$SRC/src/unix/sys_findfile.c \
        $$SRC/src/unix/sys_path.c
}

macx {
    # Mac OS X only.
    INCLUDEPATH += $$DENG_MAC_INCLUDE_DIR
}
else:unix {
}

SOURCES += $$SRC/src/ui/displaymode_dummy.c

# Platform-independent sources.
SOURCES += \
    src/server_dummies.c \
    $$SRC/src/audio/s_cache.c \
    $$SRC/src/audio/s_environ.cpp \
    $$SRC/src/audio/s_logic.c \
    $$SRC/src/audio/s_main.c \
    $$SRC/src/audio/s_wav.c \
    $$SRC/src/busymode.cpp \
    $$SRC/src/cbuffer.c \
    $$SRC/src/color.cpp \
    $$SRC/src/con_bar.c \
    $$SRC/src/con_config.c \
    $$SRC/src/con_data.cpp \
    $$SRC/src/con_main.c \
    $$SRC/src/dd_games.cpp \
    $$SRC/src/dd_help.cpp \
    $$SRC/src/dd_init.cpp \
    $$SRC/src/dd_loop.c \
    $$SRC/src/dd_main.cpp \
    $$SRC/src/dd_pinit.c \
    $$SRC/src/dd_plugin.c \
    $$SRC/src/dd_wad.cpp \
    $$SRC/src/def_data.c \
    $$SRC/src/def_main.cpp \
    $$SRC/src/def_read.cpp \
    $$SRC/src/dualstring.cpp \
    $$SRC/src/edit_bsp.cpp \
    $$SRC/src/edit_map.cpp \
    $$SRC/src/filesys/file.cpp \
    $$SRC/src/filesys/filehandle.cpp \
    $$SRC/src/filesys/fileid.cpp \
    $$SRC/src/filesys/fs_main.cpp \
    $$SRC/src/filesys/fs_scheme.cpp \
    $$SRC/src/filesys/fs_util.cpp \
    $$SRC/src/filesys/lumpindex.cpp \
    $$SRC/src/filesys/manifest.cpp \
    $$SRC/src/filesys/searchpath.cpp \
    $$SRC/src/filesys/sys_direc.c \
    $$SRC/src/game.cpp \
    $$SRC/src/gl/gl_texmanager.cpp \
    $$SRC/src/gridmap.c \
    $$SRC/src/kdtree.c \
    $$SRC/src/library.cpp \
    $$SRC/src/m_bams.c \
    $$SRC/src/m_decomp64.c \
    $$SRC/src/m_misc.c \
    $$SRC/src/m_nodepile.c \
    $$SRC/src/m_stack.c \
    $$SRC/src/map/blockmap.c \
    $$SRC/src/map/bsp/hplane.cpp \
    $$SRC/src/map/bsp/partitioner.cpp \
    $$SRC/src/map/bsp/superblockmap.cpp \
    $$SRC/src/map/bspbuilder.cpp \
    $$SRC/src/map/bspleaf.cpp \
    $$SRC/src/map/bspnode.c \
    $$SRC/src/map/dam_file.c \
    $$SRC/src/map/dam_main.cpp \
    $$SRC/src/map/entitydatabase.cpp \
    $$SRC/src/map/gamemap.c \
    $$SRC/src/map/generators.c \
    $$SRC/src/map/hedge.cpp \
    $$SRC/src/map/linedef.c \
    $$SRC/src/map/p_data.cpp \
    $$SRC/src/map/p_dmu.cpp \
    $$SRC/src/map/p_intercept.c \
    $$SRC/src/map/p_maputil.c \
    $$SRC/src/map/p_mobj.c \
    $$SRC/src/map/p_objlink.c \
    $$SRC/src/map/p_particle.cpp \
    $$SRC/src/map/p_players.c \
    $$SRC/src/map/p_polyobjs.c \
    $$SRC/src/map/p_sight.c \
    $$SRC/src/map/p_think.c \
    $$SRC/src/map/p_ticker.c \
    $$SRC/src/map/plane.c \
    $$SRC/src/map/polyobj.c \
    $$SRC/src/map/propertyvalue.cpp \
    $$SRC/src/map/r_world.cpp \
    $$SRC/src/map/sector.c \
    $$SRC/src/map/sidedef.c \
    $$SRC/src/map/surface.cpp \
    $$SRC/src/map/vertex.cpp \
    $$SRC/src/network/masterserver.cpp \
    $$SRC/src/network/monitor.c \
    $$SRC/src/network/net_buf.c \
    $$SRC/src/network/net_event.c \
    $$SRC/src/network/net_main.c \
    $$SRC/src/network/net_msg.c \
    $$SRC/src/network/net_ping.c \
    $$SRC/src/network/protocol.c \
    $$SRC/src/network/sys_network.c \
    $$SRC/src/r_util.c \
    $$SRC/src/render/r_main.cpp \
    $$SRC/src/render/r_things.cpp \
    $$SRC/src/render/rend_main.cpp \
    $$SRC/src/resource/animgroups.cpp \
    $$SRC/src/resource/colorpalette.c \
    $$SRC/src/resource/colorpalettes.cpp \
    $$SRC/src/resource/compositetexture.cpp \
    $$SRC/src/resource/hq2x.c \
    $$SRC/src/resource/image.cpp \
    $$SRC/src/resource/material.cpp \
    $$SRC/src/resource/materialarchive.c \
    $$SRC/src/resource/materialbind.cpp \
    $$SRC/src/resource/materials.cpp \
    $$SRC/src/resource/materialscheme.cpp \
    $$SRC/src/resource/materialsnapshot.cpp \
    $$SRC/src/resource/materialvariant.cpp \
    $$SRC/src/resource/patch.cpp \
    $$SRC/src/resource/patchname.cpp \
    $$SRC/src/resource/pcx.c \
    $$SRC/src/resource/r_data.cpp \
    $$SRC/src/resource/rawtexture.cpp \
    $$SRC/src/resource/texture.cpp \
    $$SRC/src/resource/texturemanifest.cpp \
    $$SRC/src/resource/texturescheme.cpp \
    $$SRC/src/resource/textures.cpp \
    $$SRC/src/resource/texturevariant.cpp \
    $$SRC/src/resource/tga.c \
    $$SRC/src/resource/wad.cpp \
    $$SRC/src/resource/zip.cpp \
    $$SRC/src/server/sv_frame.c \
    $$SRC/src/server/sv_infine.c \
    $$SRC/src/server/sv_main.c \
    $$SRC/src/server/sv_missile.c \
    $$SRC/src/server/sv_pool.c \
    $$SRC/src/server/sv_sound.cpp \
    $$SRC/src/sys_system.c \
    $$SRC/src/tab_tables.c \
    $$SRC/src/ui/b_command.c \
    $$SRC/src/ui/b_context.c \
    $$SRC/src/ui/b_device.c \
    $$SRC/src/ui/b_main.c \
    $$SRC/src/ui/b_util.c \
    $$SRC/src/ui/canvas.cpp \
    $$SRC/src/ui/canvaswindow.cpp \
    $$SRC/src/ui/dd_input.c \
    $$SRC/src/ui/displaymode.cpp \
    $$SRC/src/ui/fi_main.c \
    $$SRC/src/ui/finaleinterpreter.c \
    $$SRC/src/ui/keycode.cpp \
    $$SRC/src/ui/p_control.c \
    $$SRC/src/ui/sys_input.c \
    $$SRC/src/ui/ui2_main.cpp \
    $$SRC/src/ui/window.cpp \
    $$SRC/src/updater/downloaddialog.cpp \
    $$SRC/src/updater/processcheckdialog.cpp \
    $$SRC/src/updater/updateavailabledialog.cpp \
    $$SRC/src/updater/updater.cpp \
    $$SRC/src/updater/updaterdialog.cpp \
    $$SRC/src/updater/updatersettings.cpp \
    $$SRC/src/updater/updatersettingsdialog.cpp \
    $$SRC/src/uri.cpp \
    $$SRC/src/uri_wrapper.cpp

OTHER_FILES += \
    data/cphelp.txt \
    include/mapdata.hs \
    include/template.h.template \
    src/template.c.template

# Resources ------------------------------------------------------------------

data.files = $$OUT_PWD/../doomsday.pk3

mod.files = \
    $$DENG_MODULES_DIR/Config.de \
    $$DENG_MODULES_DIR/recutil.de

macx {
    res.path = Contents/Resources
    res.files = \
        $$SRC/res/macx/English.lproj \
        $$SRC/res/macx/deng.icns

    data.path = $${res.path}
    mod.path  = $${res.path}/modules

    QMAKE_BUNDLE_DATA += mod res data

    QMAKE_INFO_PLIST = ../build/mac/Info.plist

    linkBinaryToBundledLibdeng2($$TARGET)
}

# Installation ---------------------------------------------------------------

!macx {
    # Common (non-Mac) parts of the installation.
    INSTALLS += target data mod

    target.path = $$DENG_BIN_DIR
    data.path   = $$DENG_DATA_DIR
    mod.path    = $$DENG_BASE_DIR/modules

    win32 {
        # Windows-specific installation.
        INSTALLS += license icon

        license.files = $$SRC/doc/LICENSE
        license.path = $$DENG_DOCS_DIR

        icon.files = $$SRC/res/windows/doomsday.ico
        icon.path = $$DENG_DATA_DIR/graphics
    }
    else {
        # Generic Unix installation.
        INSTALLS += readme

        readme.files = ../doc/output/doomsday.6
        readme.path = $$PREFIX/share/man/man6
    }
}
