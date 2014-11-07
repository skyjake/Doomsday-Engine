# The Doomsday Engine Project
# Copyright (c) 2011-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2014 Daniel Swanson <danij@dengine.net>

TEMPLATE = app
win32|macx: TARGET = Doomsday
      else: TARGET = doomsday

# Build Configuration ----------------------------------------------------------

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

# External Dependencies --------------------------------------------------------

CONFIG += deng_qtgui deng_qtopengl

include(../dep_sdl2.pri)
include(../dep_opengl.pri)
include(../dep_zlib.pri)
include(../dep_lzss.pri)
win32 {
    include(../dep_directx.pri)
}
include(../dep_core.pri)
include(../dep_legacy.pri)
include(../dep_shell.pri)
include(../dep_gui.pri)
include(../dep_appfw.pri)
include(../dep_doomsday.pri)

# Definitions ------------------------------------------------------------------

DEFINES += __DOOMSDAY__ __CLIENT__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

# Linking ----------------------------------------------------------------------

win32 {
    RC_FILE = res/windows/doomsday.rc
    OTHER_FILES += $$RC_FILE

    deng_msvc: QMAKE_LFLAGS += /NODEFAULTLIB:libcmt

    LIBS += -lkernel32 -lgdi32 -lole32 -luser32 -lwsock32 -lopengl32
}
else:macx {
    useFramework(Cocoa)
    useFramework(QTKit)
}
else {
    # Generic Unix.
    !freebsd-*: LIBS += -ldl
}

# Source Files -----------------------------------------------------------------

!deng_mingw: PRECOMPILED_HEADER = include/precompiled.h

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
    $$DENG_API_DIR/dd_share.h \
    $$DENG_API_DIR/dd_types.h \
    $$DENG_API_DIR/dd_version.h \
    $$DENG_API_DIR/def_share.h \
    $$DENG_API_DIR/dengproject.h \
    $$DENG_API_DIR/doomsday.h \
    $$DENG_API_DIR/xgclass.h

# Convenience headers.
DENG_CONVENIENCE_HEADERS += \
    include/AbstractFont \
    include/BiasDigest \
    include/BiasIllum \
    include/BiasSource \
    include/BiasTracker \
    include/BindContext \
    include/BitmapFont \
    include/BspLeaf \
    include/BspNode \
    include/CommandAction \
    include/CompositeBitmapFont \
    include/Contact \
    include/ContactSpreader \
    include/ConvexSubspace \
    include/Decoration \
    include/DrawList \
    include/DrawLists \
    include/EntityDatabase \
    include/Face \
    include/FontManifest \
    include/FontScheme \
    include/Game \
    include/Games \
    include/Generator \
    include/Grabbable \
    include/Hand \
    include/HEdge \
    include/HueCircle \
    include/HueCircleVisual \
    include/IHPlane \
    include/Interceptor \
    include/LightDecoration \
    include/Line \
    include/Lumobj \
    include/MapDef \
    include/MapElement \
    include/MapObject \
    include/Material \
    include/MaterialArchive \
    include/MaterialContext \
    include/MaterialManifest \
    include/MaterialScheme \
    include/MaterialSnapshot \
    include/MaterialVariantSpec \
    include/Mesh \
    include/Model \
    include/ModelDef \
    include/Plane \
    include/Polyobj \
    include/Sector \
    include/SectorCluster \
    include/SettingsRegister \
    include/Shard \
    include/SkyFixEdge \
    include/Sprite \
    include/Surface \
    include/SurfaceDecorator \
    include/Texture \
    include/TextureManifest \
    include/TextureScheme \
    include/TextureVariantSpec \
    include/TriangleStripBuilder \
    include/Vertex \
    include/WallEdge \
    include/WallSpec

# Private headers.
DENG_HEADERS += \
    $$DENG_CONVENIENCE_HEADERS \
    include/alertmask.h \
    include/audio/audiodriver.h \
    include/audio/audiodriver_music.h \
    include/audio/m_mus2midi.h \
    include/audio/s_cache.h \
    include/audio/s_environ.h \
    include/audio/s_main.h \
    include/audio/s_mus.h \
    include/audio/s_sfx.h \
    include/audio/sys_audio.h \
    include/audio/sys_audiod_dummy.h \
    include/busymode.h \
    include/client/cl_def.h \
    include/client/cl_frame.h \
    include/client/cl_infine.h \
    include/client/cl_mobj.h \
    include/client/cl_player.h \
    include/client/cl_sound.h \
    include/client/cl_world.h \
    include/client/clplanemover.h \
    include/client/clpolymover.h \
    include/clientapp.h \
    include/color.h \
    include/con_config.h \
    include/dd_def.h \
    include/dd_loop.h \
    include/dd_main.h \
    include/dd_pinit.h \
    include/de_audio.h \
    include/de_base.h \
    include/de_console.h \
    include/de_defs.h \
    include/de_edit.h \
    include/de_filesys.h \
    include/de_graphics.h \
    include/de_infine.h \
    include/de_misc.h \
    include/de_network.h \
    include/de_platform.h \
    include/de_play.h \
    include/de_render.h \
    include/de_resource.h \
    include/de_system.h \
    include/de_ui.h \
    include/def_main.h \
    include/edit_bias.h \
    include/edit_map.h \
    include/face.h \
    include/game.h \
    include/games.h \
    include/gl/gl_defer.h \
    include/gl/gl_deferredapi.h \
    include/gl/gl_draw.h \
    include/gl/gl_main.h \
    include/gl/gl_tex.h \
    include/gl/gl_texmanager.h \
    include/gl/gltextureunit.h \
    include/gl/svg.h \
    include/gl/sys_opengl.h \
    include/gl/texturecontent.h \
    include/hedge.h \
    include/ihplane.h \
    include/library.h \
    include/m_decomp64.h \
    include/m_misc.h \
    include/m_nodepile.h \
    include/m_profiler.h \
    include/mesh.h \
    include/network/masterserver.h \
    include/network/monitor.h \
    include/network/net_buf.h \
    include/network/net_demo.h \
    include/network/net_event.h \
    include/network/net_main.h \
    include/network/net_msg.h \
    include/network/protocol.h \
    include/network/serverlink.h \
    include/network/sys_network.h \
    include/partition.h \
    include/r_util.h \
    include/render/biasdigest.h \
    include/render/biasillum.h \
    include/render/biassource.h \
    include/render/biastracker.h \
    include/render/billboard.h \
    include/render/blockmapvisual.h \
    include/render/cameralensfx.h \
    include/render/consoleeffect.h \
    include/render/decoration.h \
    include/render/drawlist.h \
    include/render/drawlists.h \
    include/render/fx/bloom.h \
    include/render/fx/colorfilter.h \
    include/render/fx/lensflares.h \
    include/render/fx/postprocessing.h \
    include/render/fx/resize.h \
    include/render/fx/vignette.h \
    include/render/huecirclevisual.h \
    include/render/ilightsource.h \
    include/render/lightdecoration.h \
    include/render/lightgrid.h \
    include/render/lumobj.h \
    include/render/materialcontext.h \
    include/render/mobjanimator.h \
    include/render/modelrenderer.h \
    include/render/projector.h \
    include/render/r_draw.h \
    include/render/r_main.h \
    include/render/r_things.h \
    include/render/rend_clip.h \
    include/render/rend_dynlight.h \
    include/render/rend_fakeradio.h \
    include/render/rend_font.h \
    include/render/rend_halo.h \
    include/render/rend_main.h \
    include/render/rend_model.h \
    include/render/rend_particle.h \
    include/render/rend_shadow.h \
    include/render/rendpoly.h \
    include/render/rendersystem.h \
    include/render/shadowedge.h \
    include/render/shard.h \
    include/render/sky.h \
    include/render/skyfixedge.h \
    include/render/surfacedecorator.h \
    include/render/trianglestripbuilder.h \
    include/render/viewports.h \
    include/render/vissprite.h \
    include/render/vlight.h \
    include/render/vr.h \
    include/render/walledge.h \
    include/render/wallspec.h \
    include/resource/abstractfont.h \
    include/resource/animgroup.h \
    include/resource/bitmapfont.h \
    include/resource/colorpalette.h \
    include/resource/compositebitmapfont.h \
    include/resource/compositetexture.h \
    include/resource/fontmanifest.h \
    include/resource/fontscheme.h \
    include/resource/hq2x.h \
    include/resource/image.h \
    include/resource/manifest.h \
    include/resource/mapdef.h \
    include/resource/material.h \
    include/resource/materialarchive.h \
    include/resource/materialmanifest.h \
    include/resource/materialscheme.h \
    include/resource/materialsnapshot.h \
    include/resource/materialvariantspec.h \
    include/resource/model.h \
    include/resource/modeldef.h \
    include/resource/patch.h \
    include/resource/patchname.h \
    include/resource/pcx.h \
    include/resource/rawtexture.h \
    include/resource/resourcesystem.h \
    include/resource/sprite.h \
    include/resource/texture.h \
    include/resource/texturemanifest.h \
    include/resource/texturescheme.h \
    include/resource/texturevariantspec.h \
    include/resource/tga.h \
    include/settingsregister.h \
    include/sys_system.h \
    include/tab_anorms.h \
    include/ui/b_main.h \
    include/ui/b_util.h \
    include/ui/bindcontext.h \
    include/ui/busyvisual.h \
    include/ui/clientrootwidget.h \
    include/ui/clientwindow.h \
    include/ui/clientwindowsystem.h \
    include/ui/commandaction.h \
    include/ui/commandbinding.h \
    include/ui/ddevent.h \
    include/ui/dialogs/aboutdialog.h \
    include/ui/dialogs/alertdialog.h \
    include/ui/dialogs/audiosettingsdialog.h \
    include/ui/dialogs/coloradjustmentdialog.h \
    include/ui/dialogs/gamesdialog.h \
    include/ui/dialogs/inputsettingsdialog.h \
    include/ui/dialogs/logsettingsdialog.h \
    include/ui/dialogs/manualconnectiondialog.h \
    include/ui/dialogs/networksettingsdialog.h \
    include/ui/dialogs/renderersettingsdialog.h \
    include/ui/dialogs/videosettingsdialog.h \
    include/ui/dialogs/vrsettingsdialog.h \
    include/ui/editors/rendererappearanceeditor.h \
    include/ui/impulsebinding.h \
    include/ui/infine/finale.h \
    include/ui/infine/finaleanimwidget.h \
    include/ui/infine/finaleinterpreter.h \
    include/ui/infine/finalepagewidget.h \
    include/ui/infine/finaletextwidget.h \
    include/ui/infine/finalewidget.h \
    include/ui/infine/infinesystem.h \
    include/ui/inputdevice.h \
    include/ui/inputdeviceaxiscontrol.h \
    include/ui/inputdevicebuttoncontrol.h \
    include/ui/inputdevicehatcontrol.h \
    include/ui/progress.h \
    include/ui/widgets/busywidget.h \
    include/ui/widgets/consolecommandwidget.h \
    include/ui/widgets/consolewidget.h \
    include/ui/widgets/cvarchoicewidget.h \
    include/ui/widgets/cvarlineeditwidget.h \
    include/ui/widgets/cvarsliderwidget.h \
    include/ui/widgets/cvartogglewidget.h \
    include/ui/widgets/gamefilterwidget.h \
    include/ui/widgets/gameselectionwidget.h \
    include/ui/widgets/gamesessionwidget.h \
    include/ui/widgets/gameuiwidget.h \
    include/ui/widgets/gamewidget.h \
    include/ui/widgets/icvarwidget.h \
    include/ui/widgets/inputbindingwidget.h \
    include/ui/widgets/keygrabberwidget.h \
    include/ui/widgets/mpsessionmenuwidget.h \
    include/ui/widgets/multiplayermenuwidget.h \
    include/ui/widgets/profilepickerwidget.h \
    include/ui/widgets/savedsessionmenuwidget.h \
    include/ui/widgets/sessionmenuwidget.h \
    include/ui/widgets/singleplayersessionmenuwidget.h \
    include/ui/widgets/taskbarwidget.h \
    include/ui/widgets/tutorialwidget.h \
    include/ui/inputsystem.h \
    include/ui/joystick.h \
    include/ui/mouse_qt.h \
    include/ui/nativeui.h \
    include/ui/playerimpulse.h \
    include/ui/styledlogsinkformatter.h \
    include/ui/sys_input.h \
    include/ui/ui_main.h \
    include/ui/zonedebug.h \
    include/updater.h \
    include/updater/downloaddialog.h \
    include/updater/processcheckdialog.h \
    include/updater/updateavailabledialog.h \
    include/updater/updatersettings.h \
    include/updater/updatersettingsdialog.h \
    include/versioninfo.h \
    include/world/blockmap.h \
    include/world/bsp/convexsubspaceproxy.h \
    include/world/bsp/edgetip.h \
    include/world/bsp/hplane.h \
    include/world/bsp/linesegment.h \
    include/world/bsp/partitioner.h \
    include/world/bsp/partitionevaluator.h \
    include/world/bsp/superblockmap.h \
    include/world/bspleaf.h \
    include/world/bspnode.h \
    include/world/clientmobjthinkerdata.h \
    include/world/contact.h \
    include/world/contactspreader.h \
    include/world/convexsubspace.h \
    include/world/dmuargs.h \
    include/world/entitydatabase.h \
    include/world/entitydef.h \
    include/world/generator.h \
    include/world/grabbable.h \
    include/world/hand.h \
    include/world/huecircle.h \
    include/world/interceptor.h \
    include/world/line.h \
    include/world/lineblockmap.h \
    include/world/lineowner.h \
    include/world/linesighttest.h \
    include/world/map.h \
    include/world/mapelement.h \
    include/world/mapobject.h \
    include/world/maputil.h \
    include/world/p_object.h \
    include/world/p_players.h \
    include/world/p_ticker.h \
    include/world/plane.h \
    include/world/polyobj.h \
    include/world/polyobjdata.h \
    include/world/propertyvalue.h \
    include/world/reject.h \
    include/world/sector.h \
    include/world/sectorcluster.h \
    include/world/surface.h \
    include/world/thinkers.h \
    include/world/vertex.h \
    include/world/worldsystem.h

INCLUDEPATH += \
    $$DENG_INCLUDE_DIR \
    $$DENG_API_DIR

HEADERS += \
    include/precompiled.h \
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
        src/windows/joystick_win32.cpp \
        src/windows/mouse_win32.cpp
}
else:unix {
    # Common Unix (including Mac OS X).
    HEADERS += \
        $$DENG_UNIX_INCLUDE_DIR/dd_uinit.h

    INCLUDEPATH += $$DENG_UNIX_INCLUDE_DIR

    SOURCES += \
        src/unix/dd_uinit.cpp \
        src/unix/joystick.cpp
}

macx {
    # Mac OS X only.
    HEADERS += \
        $$DENG_MAC_INCLUDE_DIR/MusicPlayer.h

    OBJECTIVE_SOURCES += \
        src/macx/MusicPlayer.mm

    INCLUDEPATH += $$DENG_MAC_INCLUDE_DIR
}
else:unix {
    # Unix (non-Mac) only.
}

# Platform-independent sources.
SOURCES += \
    src/alertmask.cpp \
    src/api_console.cpp \
    src/api_filesys.cpp \
    src/api_uri.cpp \
    src/audio/audiodriver.cpp \
    src/audio/audiodriver_music.cpp \
    src/audio/m_mus2midi.cpp \
    src/audio/s_cache.cpp \
    src/audio/s_environ.cpp \
    src/audio/s_main.cpp \
    src/audio/s_mus.cpp \
    src/audio/s_sfx.cpp \
    src/audio/sys_audiod_dummy.cpp \
    src/busymode.cpp \
    src/client/cl_frame.cpp \
    src/client/cl_infine.cpp \
    src/client/cl_main.cpp \
    src/client/cl_mobj.cpp \
    src/client/cl_player.cpp \
    src/client/cl_sound.cpp \
    src/client/cl_world.cpp \
    src/client/clplanemover.cpp \
    src/client/clpolymover.cpp \
    src/clientapp.cpp \
    src/color.cpp \
    src/con_config.cpp \
    src/dd_loop.cpp \
    src/dd_main.cpp \
    src/dd_pinit.cpp \
    src/dd_plugin.cpp \
    src/def_main.cpp \
    src/edit_bias.cpp \
    src/face.cpp \
    src/game.cpp \
    src/games.cpp \
    src/gl/dgl_common.cpp \
    src/gl/dgl_draw.cpp \
    src/gl/gl_defer.cpp \
    src/gl/gl_deferredapi.cpp \
    src/gl/gl_draw.cpp \
    src/gl/gl_drawvectorgraphic.cpp \
    src/gl/gl_main.cpp \
    src/gl/gl_tex.cpp \
    src/gl/gl_texmanager.cpp \
    src/gl/texturecontent.cpp \
    src/gl/svg.cpp \
    src/gl/sys_opengl.cpp \
    src/hedge.cpp \
    src/library.cpp \
    src/m_decomp64.cpp \
    src/m_misc.cpp \
    src/m_nodepile.cpp \
    src/main_client.cpp \
    src/mesh.cpp \
    src/network/masterserver.cpp \
    src/network/monitor.cpp \
    src/network/net_buf.cpp \
    src/network/net_demo.cpp \
    src/network/net_event.cpp \
    src/network/net_main.cpp \
    src/network/net_msg.cpp \
    src/network/net_ping.cpp \
    src/network/serverlink.cpp \
    src/network/sys_network.cpp \
    src/r_util.cpp \
    src/render/api_render.cpp \
    src/render/biasdigest.cpp \
    src/render/biasillum.cpp \
    src/render/biassource.cpp \
    src/render/biastracker.cpp \
    src/render/billboard.cpp \
    src/render/blockmapvisual.cpp \
    src/render/cameralensfx.cpp \
    src/render/consoleeffect.cpp \
    src/render/decoration.cpp \
    src/render/drawlist.cpp \
    src/render/drawlists.cpp \
    src/render/fx/bloom.cpp \
    src/render/fx/colorfilter.cpp \
    src/render/fx/lensflares.cpp \
    src/render/fx/postprocessing.cpp \
    src/render/fx/resize.cpp \
    src/render/fx/vignette.cpp \
    src/render/huecirclevisual.cpp \
    src/render/lightdecoration.cpp \
    src/render/lightgrid.cpp \
    src/render/lumobj.cpp \
    src/render/mobjanimator.cpp \
    src/render/modelrenderer.cpp \
    src/render/projector.cpp \
    src/render/r_draw.cpp \
    src/render/r_fakeradio.cpp \
    src/render/r_main.cpp \
    src/render/r_things.cpp \
    src/render/rend_clip.cpp \
    src/render/rend_dynlight.cpp \
    src/render/rend_fakeradio.cpp \
    src/render/rend_font.cpp \
    src/render/rend_halo.cpp \
    src/render/rend_main.cpp \
    src/render/rend_model.cpp \
    src/render/rend_particle.cpp \
    src/render/rend_shadow.cpp \
    src/render/rendpoly.cpp \
    src/render/rendersystem.cpp \
    src/render/shadowedge.cpp \
    src/render/shard.cpp \
    src/render/sky.cpp \
    src/render/skyfixedge.cpp \
    src/render/surfacedecorator.cpp \
    src/render/trianglestripbuilder.cpp \
    src/render/viewports.cpp \
    src/render/vissprite.cpp \
    src/render/vlight.cpp \
    src/render/vr.cpp \
    src/render/walledge.cpp \
    src/render/wallspec.cpp \
    src/resource/abstractfont.cpp \
    src/resource/animgroup.cpp \
    src/resource/api_material.cpp \
    src/resource/api_resource.cpp \
    src/resource/bitmapfont.cpp \
    src/resource/colorpalette.cpp \
    src/resource/compositebitmapfont.cpp \
    src/resource/compositetexture.cpp \
    src/resource/fontmanifest.cpp \
    src/resource/fontscheme.cpp \
    src/resource/hq2x.cpp \
    src/resource/image.cpp \
    src/resource/manifest.cpp \
    src/resource/mapdef.cpp \
    src/resource/material.cpp \
    src/resource/materialanimation.cpp \
    src/resource/materialarchive.cpp \
    src/resource/materialmanifest.cpp \
    src/resource/materialscheme.cpp \
    src/resource/materialsnapshot.cpp \
    src/resource/materialvariant.cpp \
    src/resource/model.cpp \
    src/resource/patch.cpp \
    src/resource/patchname.cpp \
    src/resource/pcx.cpp \
    src/resource/resourcesystem.cpp \
    src/resource/sprite.cpp \
    src/resource/texture.cpp \
    src/resource/texturemanifest.cpp \
    src/resource/texturescheme.cpp \
    src/resource/texturevariant.cpp \
    src/resource/tga.cpp \
    src/settingsregister.cpp \
    src/sys_system.cpp \
    src/tab_tables.c \
    src/ui/b_main.cpp \
    src/ui/b_util.cpp \
    src/ui/bindcontext.cpp \
    src/ui/busyvisual.cpp \
    src/ui/clientrootwidget.cpp \
    src/ui/clientwindow.cpp \
    src/ui/clientwindowsystem.cpp \
    src/ui/commandaction.cpp \
    src/ui/dialogs/aboutdialog.cpp \
    src/ui/dialogs/alertdialog.cpp \
    src/ui/dialogs/audiosettingsdialog.cpp \
    src/ui/dialogs/coloradjustmentdialog.cpp \
    src/ui/dialogs/gamesdialog.cpp \
    src/ui/dialogs/inputsettingsdialog.cpp \
    src/ui/dialogs/logsettingsdialog.cpp \
    src/ui/dialogs/manualconnectiondialog.cpp \
    src/ui/dialogs/networksettingsdialog.cpp \
    src/ui/dialogs/videosettingsdialog.cpp \
    src/ui/dialogs/vrsettingsdialog.cpp \
    src/ui/dialogs/renderersettingsdialog.cpp \
    src/ui/editors/rendererappearanceeditor.cpp \
    src/ui/infine/finale.cpp \
    src/ui/infine/finaleanimwidget.cpp \
    src/ui/infine/finaleinterpreter.cpp \
    src/ui/infine/finalepagewidget.cpp \
    src/ui/infine/finaletextwidget.cpp \
    src/ui/infine/finalewidget.cpp \
    src/ui/infine/infinesystem.cpp \
    src/ui/inputdebug.cpp \
    src/ui/inputdevice.cpp \
    src/ui/inputdeviceaxiscontrol.cpp \
    src/ui/inputdevicebuttoncontrol.cpp \
    src/ui/inputdevicehatcontrol.cpp \
    src/ui/inputsystem.cpp \
    src/ui/mouse_qt.cpp \
    src/ui/nativeui.cpp \
    src/ui/playerimpulse.cpp \
    src/ui/progress.cpp \
    src/ui/styledlogsinkformatter.cpp \
    src/ui/sys_input.cpp \
    src/ui/ui_main.cpp \
    src/ui/widgets/busywidget.cpp \
    src/ui/widgets/consolecommandwidget.cpp \
    src/ui/widgets/consolewidget.cpp \
    src/ui/widgets/cvarchoicewidget.cpp \
    src/ui/widgets/cvarlineeditwidget.cpp \
    src/ui/widgets/cvarsliderwidget.cpp \
    src/ui/widgets/cvartogglewidget.cpp \
    src/ui/widgets/gamefilterwidget.cpp \
    src/ui/widgets/gameselectionwidget.cpp \
    src/ui/widgets/gamesessionwidget.cpp \
    src/ui/widgets/gamewidget.cpp \
    src/ui/widgets/gameuiwidget.cpp \
    src/ui/widgets/inputbindingwidget.cpp \
    src/ui/widgets/keygrabberwidget.cpp \
    src/ui/widgets/multiplayermenuwidget.cpp \
    src/ui/widgets/mpsessionmenuwidget.cpp \
    src/ui/widgets/profilepickerwidget.cpp \
    src/ui/widgets/savedsessionmenuwidget.cpp \
    src/ui/widgets/sessionmenuwidget.cpp \
    src/ui/widgets/singleplayersessionmenuwidget.cpp \
    src/ui/widgets/taskbarwidget.cpp \
    src/ui/widgets/tutorialwidget.cpp \
    src/ui/zonedebug.cpp \
    src/updater/downloaddialog.cpp \
    src/updater/processcheckdialog.cpp \
    src/updater/updateavailabledialog.cpp \
    src/updater/updater.cpp \
    src/updater/updatersettings.cpp \
    src/updater/updatersettingsdialog.cpp \
    src/world/api_map.cpp \
    src/world/api_mapedit.cpp \
    src/world/blockmap.cpp \
    src/world/bsp/convexsubspaceproxy.cpp \
    src/world/bsp/hplane.cpp \
    src/world/bsp/linesegment.cpp \
    src/world/bsp/partitioner.cpp \
    src/world/bsp/partitionevaluator.cpp \
    src/world/bsp/superblockmap.cpp \
    src/world/bspleaf.cpp \
    src/world/bspnode.cpp \
    src/world/clientmobjthinkerdata.cpp \
    src/world/contact.cpp \
    src/world/contactspreader.cpp \
    src/world/convexsubspace.cpp \
    src/world/dmuargs.cpp \
    src/world/entitydatabase.cpp \
    src/world/entitydef.cpp \
    src/world/generator.cpp \
    src/world/grabbable.cpp \
    src/world/hand.cpp \
    src/world/huecircle.cpp \
    src/world/interceptor.cpp \
    src/world/line.cpp \
    src/world/lineblockmap.cpp \
    src/world/linesighttest.cpp \
    src/world/map.cpp \
    src/world/mapelement.cpp \
    src/world/mapobject.cpp \
    src/world/maputil.cpp \
    src/world/p_mobj.cpp \
    src/world/p_players.cpp \
    src/world/p_ticker.cpp \
    src/world/plane.cpp \
    src/world/polyobj.cpp \
    src/world/polyobjdata.cpp \
    src/world/propertyvalue.cpp \
    src/world/reject.cpp \
    src/world/sector.cpp \
    src/world/sectorcluster.cpp \
    src/world/surface.cpp \
    src/world/thinkers.cpp \
    src/world/vertex.cpp \
    src/world/worldsystem.cpp

!deng_nosdlmixer:!deng_nosdl {
    HEADERS += include/audio/sys_audiod_sdlmixer.h
    SOURCES += src/audio/sys_audiod_sdlmixer.cpp
}

OTHER_FILES += \
    include/template.h.template \
    src/template.c.template \
    client-mac.doxy \
    client-win32.doxy

# Resources ------------------------------------------------------------------

buildPackage(net.dengine.client, $$OUT_PWD/..)

data.files = $$OUT_PWD/../doomsday.pk3

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

macx {
    xcodeFinalizeAppBuild()
    xcodeDeployDengLibs(shell.1 gui.1 appfw.1 doomsday.1 legacy.1)
    xcodeDeployDengPlugins(audio_fluidsynth audio_fmod \
        idtech1converter savegameconverter dehread \
        doom heretic hexen doom64)
    macx-xcode {
        QMAKE_BUNDLE_DATA += exectools
        exectools.files = $$DESTDIR/savegametool $$DESTDIR/doomsday-server
        exectools.path = Contents/Resources
    }

    res.path = Contents/Resources
    res.files = \
        res/macx/English.lproj \
        res/macx/deng.icns

    data.path         = $${res.path}
    startupfonts.path = $${res.path}/data/fonts

    QMAKE_BUNDLE_DATA += res data startupfonts

    QMAKE_INFO_PLIST = res/macx/Info.plist

    # Since qmake is unable to copy directories as bundle data, let's copy
    # the frameworks manually.
    FW_DIR = Doomsday.app/Contents/Frameworks/
    doPostLink("rm -rf $$FW_DIR")
    doPostLink("mkdir $$FW_DIR")
    !deng_nosdl {
        doPostLink("cp -fRp $${SDL2_FRAMEWORK_DIR}/SDL2.framework $$FW_DIR")
        !deng_nosdlmixer: doPostLink("cp -fRp $${SDL2_FRAMEWORK_DIR}/SDL2_mixer.framework $$FW_DIR")
    }
    deng_fmod {
        # Bundle the FMOD shared library under Frameworks.
        doPostLink("cp -f \"$$FMOD_DIR/api/lib/libfmodex.dylib\" $$FW_DIR")
        doPostLink("install_name_tool -id @rpath/libfmodex.dylib $${FW_DIR}libfmodex.dylib")
    }

    # Fix the dynamic linker paths so they point to ../Frameworks/ inside the bundle.
    fixInstallName(Doomsday.app/Contents/MacOS/Doomsday, libdeng_core.2.dylib, ..)
    fixInstallName(Doomsday.app/Contents/MacOS/Doomsday, libdeng_legacy.1.dylib, ..)
    linkBinaryToBundledLibshell(Doomsday.app/Contents/MacOS/Doomsday)

    # Clean up previous deployment.
    doPostLink("rm -rf Doomsday.app/Contents/PlugIns/")
    doPostLink("rm -f Doomsday.app/Contents/Resources/qt.conf")

    doPostLink("macdeployqt Doomsday.app")
}

# i18n -----------------------------------------------------------------------

#TRANSLATIONS = client_en.ts

# Installation ---------------------------------------------------------------

DENG_PACKAGES += net.dengine.client.pack

deployPackages($$DENG_PACKAGES, $$OUT_PWD/..)
deployTarget()

!macx {
    # Common (non-Mac) parts of the installation.
    INSTALLS += data startupfonts

    data.path         = $$DENG_DATA_DIR
    startupfonts.path = $$DENG_DATA_DIR/fonts

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
