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

CONFIG += deng_qtgui deng_qtopengl

include(../dep_sdl.pri)
include(../dep_opengl.pri)
include(../dep_zlib.pri)
include(../dep_lzss.pri)
win32 {
    include(../dep_directx.pri)
}
include(../dep_deng2.pri)
include(../dep_shell.pri)
include(../dep_gui.pri)
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
}
else {
    # Generic Unix.
    !freebsd-*: LIBS += -ldl
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
DENG_CONVENIENCE_HEADERS += \
    include/AtlasProceduralImage \
    include/BiasDigest \
    include/BiasIllum \
    include/BiasSource \
    include/BiasSurface \
    include/BiasTracker \
    include/BspLeaf \
    include/BspNode \
    include/CommandAction \
    include/ChildWidgetOrganizer \
    include/Contact \
    include/ContactSpreader \
    include/Decoration \
    include/DialogContentStylist \
    include/EntityDatabase \
    include/Face \
    include/FontLineWrapping \
    include/Game \
    include/Games \
    include/GLTextComposer \
    include/Grabbable \
    include/GridLayout \
    include/GuiRootWidget \
    include/GuiWidget \
    include/Hand \
    include/HEdge \
    include/HueCircle \
    include/HueCircleVisual \
    include/IHPlane \
    include/Interceptor \
    include/LightDecoration \
    include/Line \
    include/Lumobj \
    include/MapElement \
    include/MapObject \
    include/Material \
    include/MaterialArchive \
    include/MaterialContext \
    include/MaterialManifest \
    include/Materials \
    include/MaterialScheme \
    include/MaterialSnapshot \
    include/MaterialVariantSpec \
    include/Mesh \
    include/Plane \
    include/Polyobj \
    include/ProceduralImage \
    include/Sector \
    include/SequentialLayout \
    include/SettingsRegister \
    include/SignalAction \
    include/SkyFixEdge \
    include/Surface \
    include/SurfaceDecorator \
    include/TextDrawable \
    include/Texture \
    include/TextureManifest \
    include/Textures \
    include/TextureScheme \
    include/TextureVariantSpec \
    include/TriangleStripBuilder \    
    include/ui/Data \
    include/ui/Item \
    include/ui/ListData \
    include/ui/Margins \
    include/ui/Stylist \
    include/ui/SubmenuItem \
    include/ui/VariableToggleItem \
    include/Vertex \
    include/WallEdge \
    include/WallSpec \
    include/WidgetActions

# Private headers.
DENG_HEADERS += \
    $$DENG_CONVENIENCE_HEADERS \
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
    include/client/cl_def.h \
    include/client/cl_frame.h \
    include/client/cl_infine.h \
    include/client/cl_mobj.h \
    include/client/cl_player.h \
    include/client/cl_sound.h \
    include/client/cl_world.h \
    include/clientapp.h \
    include/color.h \
    include/con_bar.h \
    include/con_config.h \
    include/con_main.h \
    include/dd_def.h \
    include/dd_help.h \
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
    include/def_data.h \
    include/def_main.h \
    include/dualstring.h \
    include/edit_bias.h \
    include/edit_map.h \
    include/face.h \
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
    include/games.h \
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
    include/network/ui_mpi.h \
    include/partition.h \
    include/r_util.h \
    include/render/biasdigest.h \
    include/render/biasillum.h \
    include/render/biassource.h \
    include/render/biassurface.h \
    include/render/biastracker.h \
    include/render/blockmapvisual.h \
    include/render/decoration.h \
    include/render/huecirclevisual.h \
    include/render/lightdecoration.h \
    include/render/lightgrid.h \
    include/render/lumobj.h \
    include/render/materialcontext.h \
    include/render/projector.h \
    include/render/r_draw.h \
    include/render/r_main.h \
    include/render/r_things.h \
    include/render/rend_clip.h \
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
    include/render/shadowedge.h \
    include/render/sky.h \
    include/render/skyfixedge.h \
    include/render/sprite.h \
    include/render/surfacedecorator.h \
    include/render/trianglestripbuilder.h \
    include/render/vignette.h \
    include/render/vissprite.h \
    include/render/vlight.h \
    include/render/vr.h \
    include/render/walledge.h \
    include/render/wallspec.h \
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
    include/resource/materialmanifest.h \
    include/resource/materials.h \
    include/resource/materialscheme.h \
    include/resource/materialsnapshot.h \
    include/resource/materialvariantspec.h \
    include/resource/models.h \
    include/resource/patch.h \
    include/resource/patchname.h \
    include/resource/pcx.h \
    include/resource/r_data.h \
    include/resource/rawtexture.h \
    include/resource/sprites.h \
    include/resource/texture.h \
    include/resource/texturemanifest.h \
    include/resource/textures.h \
    include/resource/texturescheme.h \
    include/resource/texturevariantspec.h \
    include/resource/tga.h \
    include/resource/wad.h \
    include/resource/zip.h \
    include/resourceclass.h \
    include/settingsregister.h \
    include/sys_system.h \
    include/tab_anorms.h \
    include/ui/b_command.h \
    include/ui/b_context.h \
    include/ui/b_device.h \
    include/ui/b_main.h \
    include/ui/b_util.h \
    include/ui/busyvisual.h \
    include/ui/clientwindow.h \
    include/ui/dd_input.h \
    include/ui/dd_ui.h \
    include/ui/dialogs/aboutdialog.h \
    include/ui/dialogs/audiosettingsdialog.h \
    include/ui/dialogs/coloradjustmentdialog.h \
    include/ui/dialogs/inputdialog.h \
    include/ui/dialogs/inputsettingsdialog.h \
    include/ui/dialogs/messagedialog.h \
    include/ui/dialogs/networksettingsdialog.h \
    include/ui/dialogs/renderersettingsdialog.h \
    include/ui/dialogs/videosettingsdialog.h \
    include/ui/editors/rendererappearanceeditor.h \
    include/ui/fi_main.h \
    include/ui/finaleinterpreter.h \
    include/ui/framework/actionitem.h \
    include/ui/framework/atlasproceduralimage.h \
    include/ui/framework/commandaction.h \
    include/ui/framework/childwidgetorganizer.h \
    include/ui/framework/data.h \
    include/ui/framework/dialogcontentstylist.h \
    include/ui/framework/fontlinewrapping.h \
    include/ui/framework/gltextcomposer.h \
    include/ui/framework/gridlayout.h \
    include/ui/framework/guirootwidget.h \
    include/ui/framework/guiwidget.h \
    include/ui/framework/guiwidgetprivate.h \
    include/ui/framework/item.h \
    include/ui/framework/listdata.h \
    include/ui/framework/margins.h \
    include/ui/framework/proceduralimage.h \
    include/ui/framework/sequentiallayout.h \
    include/ui/framework/signalaction.h \
    include/ui/framework/stylist.h \
    include/ui/framework/submenuitem.h \
    include/ui/framework/textdrawable.h \
    include/ui/framework/variabletoggleitem.h \
    include/ui/framework/widgetactions.h \
    include/ui/inputsystem.h \
    include/ui/joystick.h \
    include/ui/mouse_qt.h \
    include/ui/nativeui.h \
    include/ui/p_control.h \
    include/ui/style.h \
    include/ui/styledlogsinkformatter.h \
    include/ui/sys_input.h \
    include/ui/ui2_main.h \
    include/ui/ui_main.h \
    include/ui/ui_panel.h \
    include/ui/uidefs.h \
    include/ui/widgets/blurwidget.h \
    include/ui/widgets/busywidget.h \
    include/ui/widgets/buttonwidget.h \
    include/ui/widgets/choicewidget.h \
    include/ui/widgets/commandwidget.h \
    include/ui/widgets/consolecommandwidget.h \
    include/ui/widgets/consolewidget.h \
    include/ui/widgets/cvarchoicewidget.h \
    include/ui/widgets/cvarsliderwidget.h \
    include/ui/widgets/cvartogglewidget.h \
    include/ui/widgets/dialogwidget.h \
    include/ui/widgets/documentwidget.h \
    include/ui/widgets/foldpanelwidget.h \
    include/ui/widgets/gameselectionwidget.h \
    include/ui/widgets/gridpopupwidget.h \
    include/ui/widgets/icvarwidget.h \
    include/ui/widgets/keygrabberwidget.h \
    include/ui/widgets/labelwidget.h \
    include/ui/widgets/legacywidget.h \
    include/ui/widgets/lineeditwidget.h \
    include/ui/widgets/logwidget.h \
    include/ui/widgets/menuwidget.h \
    include/ui/widgets/notificationwidget.h \
    include/ui/widgets/panelwidget.h \
    include/ui/widgets/popupmenuwidget.h \
    include/ui/widgets/popupwidget.h \
    include/ui/widgets/profilepickerwidget.h \
    include/ui/widgets/progresswidget.h \
    include/ui/widgets/scriptcommandwidget.h \
    include/ui/widgets/scrollareawidget.h \
    include/ui/widgets/sliderwidget.h \
    include/ui/widgets/taskbarwidget.h \
    include/ui/widgets/togglewidget.h \
    include/ui/widgets/variabletogglewidget.h \
    include/ui/windowsystem.h \
    include/ui/zonedebug.h \
    include/updater.h \
    include/updater/downloaddialog.h \
    include/updater/processcheckdialog.h \
    include/updater/updateavailabledialog.h \
    include/updater/updatersettings.h \
    include/updater/updatersettingsdialog.h \
    include/uri.hh \
    include/versioninfo.h \
    include/world/blockmap.h \
    include/world/bsp/bsptreenode.h \
    include/world/bsp/convexsubspace.h \
    include/world/bsp/edgetip.h \
    include/world/bsp/hplane.h \
    include/world/bsp/linesegment.h \
    include/world/bsp/partitioncost.h \
    include/world/bsp/partitioner.h \
    include/world/bsp/superblockmap.h \
    include/world/bspleaf.h \
    include/world/bspnode.h \
    include/world/contact.h \
    include/world/contactspreader.h \
    include/world/dmuargs.h \
    include/world/entitydatabase.h \
    include/world/entitydef.h \
    include/world/generators.h \
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
    include/world/p_particle.h \
    include/world/p_players.h \
    include/world/p_ticker.h \
    include/world/plane.h \
    include/world/polyobj.h \
    include/world/propertyvalue.h \
    include/world/reject.h \
    include/world/sector.h \
    include/world/surface.h \
    include/world/thinkers.h \
    include/world/vertex.h \
    include/world/world.h

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
    src/client/cl_frame.cpp \
    src/client/cl_infine.cpp \
    src/client/cl_main.cpp \
    src/client/cl_mobj.cpp \
    src/client/cl_player.cpp \
    src/client/cl_sound.cpp \
    src/client/cl_world.cpp \
    src/clientapp.cpp \
    src/color.cpp \
    src/con_bar.cpp \
    src/con_config.cpp \
    src/con_data.cpp \
    src/con_main.cpp \
    src/dd_help.cpp \
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
    src/face.cpp \
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
    src/games.cpp \
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
    src/network/ui_mpi.cpp \
    src/r_util.cpp \
    src/render/api_render.cpp \
    src/render/biasdigest.cpp \
    src/render/biasillum.cpp \
    src/render/biassource.cpp \
    src/render/biassurface.cpp \
    src/render/biastracker.cpp \
    src/render/blockmapvisual.cpp \
    src/render/decoration.cpp \
    src/render/huecirclevisual.cpp \
    src/render/lightdecoration.cpp \
    src/render/lightgrid.cpp \
    src/render/lumobj.cpp \
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
    src/render/rend_list.cpp \
    src/render/rend_main.cpp \
    src/render/rend_model.cpp \
    src/render/rend_particle.cpp \
    src/render/rend_shadow.cpp \
    src/render/rendpoly.cpp \
    src/render/shadowedge.cpp \
    src/render/sky.cpp \
    src/render/skyfixedge.cpp \
    src/render/sprite.cpp \
    src/render/surfacedecorator.cpp \
    src/render/trianglestripbuilder.cpp \
    src/render/vignette.cpp \
    src/render/vissprite.cpp \
    src/render/vlight.cpp \
    src/render/vr.cpp \
    src/render/walledge.cpp \
    src/render/wallspec.cpp \
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
    src/resource/materialanimation.cpp \
    src/resource/materialarchive.cpp \
    src/resource/materialmanifest.cpp \
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
    src/resource/sprites.cpp \
    src/resource/texture.cpp \
    src/resource/texturemanifest.cpp \
    src/resource/textures.cpp \
    src/resource/texturescheme.cpp \
    src/resource/texturevariant.cpp \
    src/resource/tga.cpp \
    src/resource/wad.cpp \
    src/resource/zip.cpp \
    src/settingsregister.cpp \
    src/sys_system.cpp \
    src/tab_tables.c \
    src/ui/b_command.cpp \
    src/ui/b_context.cpp \
    src/ui/b_device.cpp \
    src/ui/b_main.cpp \
    src/ui/b_util.cpp \
    src/ui/busyvisual.cpp \
    src/ui/clientwindow.cpp \
    src/ui/dd_input.cpp \
    src/ui/dialogs/aboutdialog.cpp \
    src/ui/dialogs/audiosettingsdialog.cpp \
    src/ui/dialogs/coloradjustmentdialog.cpp \
    src/ui/dialogs/inputdialog.cpp \
    src/ui/dialogs/inputsettingsdialog.cpp \
    src/ui/dialogs/messagedialog.cpp \
    src/ui/dialogs/networksettingsdialog.cpp \
    src/ui/dialogs/videosettingsdialog.cpp \
    src/ui/dialogs/renderersettingsdialog.cpp \
    src/ui/editors/rendererappearanceeditor.cpp \
    src/ui/fi_main.cpp \
    src/ui/finaleinterpreter.cpp \
    src/ui/framework/commandaction.cpp \
    src/ui/framework/childwidgetorganizer.cpp \
    src/ui/framework/data.cpp \
    src/ui/framework/dialogcontentstylist.cpp \
    src/ui/framework/fontlinewrapping.cpp \
    src/ui/framework/gltextcomposer.cpp \
    src/ui/framework/gridlayout.cpp \
    src/ui/framework/guirootwidget.cpp \
    src/ui/framework/guiwidget.cpp \
    src/ui/framework/item.cpp \
    src/ui/framework/listdata.cpp \
    src/ui/framework/margins.cpp \
    src/ui/framework/proceduralimage.cpp \
    src/ui/framework/sequentiallayout.cpp \
    src/ui/framework/signalaction.cpp \
    src/ui/framework/textdrawable.cpp \
    src/ui/framework/widgetactions.cpp \
    src/ui/inputsystem.cpp \
    src/ui/mouse_qt.cpp \
    src/ui/nativeui.cpp \
    src/ui/p_control.cpp \
    src/ui/style.cpp \
    src/ui/styledlogsinkformatter.cpp \
    src/ui/sys_input.cpp \
    src/ui/ui2_main.cpp \
    src/ui/ui_main.cpp \
    src/ui/ui_panel.cpp \
    src/ui/widgets/blurwidget.cpp \
    src/ui/widgets/busywidget.cpp \
    src/ui/widgets/buttonwidget.cpp \
    src/ui/widgets/choicewidget.cpp \
    src/ui/widgets/commandwidget.cpp \
    src/ui/widgets/consolecommandwidget.cpp \
    src/ui/widgets/consolewidget.cpp \
    src/ui/widgets/cvarchoicewidget.cpp \
    src/ui/widgets/cvarsliderwidget.cpp \
    src/ui/widgets/cvartogglewidget.cpp \
    src/ui/widgets/dialogwidget.cpp \
    src/ui/widgets/documentwidget.cpp \
    src/ui/widgets/foldpanelwidget.cpp \
    src/ui/widgets/gameselectionwidget.cpp \
    src/ui/widgets/gridpopupwidget.cpp \
    src/ui/widgets/keygrabberwidget.cpp \
    src/ui/widgets/labelwidget.cpp \
    src/ui/widgets/legacywidget.cpp \
    src/ui/widgets/lineeditwidget.cpp \
    src/ui/widgets/logwidget.cpp \
    src/ui/widgets/menuwidget.cpp \
    src/ui/widgets/notificationwidget.cpp \
    src/ui/widgets/panelwidget.cpp \
    src/ui/widgets/popupmenuwidget.cpp \
    src/ui/widgets/popupwidget.cpp \
    src/ui/widgets/profilepickerwidget.cpp \
    src/ui/widgets/progresswidget.cpp \
    src/ui/widgets/scriptcommandwidget.cpp \
    src/ui/widgets/scrollareawidget.cpp \
    src/ui/widgets/sliderwidget.cpp \
    src/ui/widgets/taskbarwidget.cpp \
    src/ui/widgets/togglewidget.cpp \
    src/ui/widgets/variabletogglewidget.cpp \
    src/ui/windowsystem.cpp \
    src/ui/zonedebug.cpp \
    src/updater/downloaddialog.cpp \
    src/updater/processcheckdialog.cpp \
    src/updater/updateavailabledialog.cpp \
    src/updater/updater.cpp \
    src/updater/updatersettings.cpp \
    src/updater/updatersettingsdialog.cpp \
    src/uri.cpp \
    src/world/api_map.cpp \
    src/world/api_mapedit.cpp \
    src/world/blockmap.cpp \
    src/world/bsp/convexsubspace.cpp \
    src/world/bsp/hplane.cpp \
    src/world/bsp/linesegment.cpp \
    src/world/bsp/partitioner.cpp \
    src/world/bsp/superblockmap.cpp \
    src/world/bspleaf.cpp \
    src/world/bspnode.cpp \
    src/world/contact.cpp \
    src/world/contactspreader.cpp \
    src/world/dmuargs.cpp \
    src/world/entitydatabase.cpp \
    src/world/entitydef.cpp \
    src/world/generators.cpp \
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
    src/world/p_particle.cpp \
    src/world/p_players.cpp \
    src/world/p_ticker.cpp \
    src/world/plane.cpp \
    src/world/polyobj.cpp \
    src/world/propertyvalue.cpp \
    src/world/reject.cpp \
    src/world/sector.cpp \
    src/world/sectorcluster.cpp \
    src/world/surface.cpp \
    src/world/thinkers.cpp \
    src/world/vertex.cpp \
    src/world/world.cpp

!deng_nosdlmixer:!deng_nosdl {
    HEADERS += include/audio/sys_audiod_sdlmixer.h
    SOURCES += src/audio/sys_audiod_sdlmixer.cpp
}

DOOMSDAY_SCRIPTS += \
    modules/appconfig.de \
    modules/bootstrap.de

OTHER_FILES += \
    $$DOOMSDAY_SCRIPTS \
    data/cphelp.txt \
    include/template.h.template \
    src/template.c.template \
    client-mac.doxy \
    client-win32.doxy

# Resources ------------------------------------------------------------------

data.files = $$OUT_PWD/../doomsday.pk3

mod.files = \
    $$DOOMSDAY_SCRIPTS \
    $$DENG_MODULES_DIR/Config.de \
    $$DENG_MODULES_DIR/gui.de \
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

    QMAKE_INFO_PLIST = res/macx/Info.plist

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
    linkBinaryToBundledLibdengShell(Doomsday.app/Contents/MacOS/Doomsday)

    # Clean up previous deployment.
    doPostLink("rm -rf Doomsday.app/Contents/PlugIns/")
    doPostLink("rm -f Doomsday.app/Contents/Resources/qt.conf")

    doPostLink("macdeployqt Doomsday.app")
}

# i18n -----------------------------------------------------------------------

#TRANSLATIONS = client_en.ts

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
