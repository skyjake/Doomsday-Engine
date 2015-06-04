# The Doomsday Engine Project -- Server
# Copyright (c) 2012-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2014 Daniel Swanson <danij@dengine.net>

cmake_minimum_required (VERSION 3.1)
project (DENG_SERVER)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengLegacy)
find_package (DengShell)
find_package (DengDoomsday)
find_package (LZSS)
find_package (Amethyst QUIET)

include_directories (include ../client/include ${DENG_API_DIR})

add_definitions (-D__DOOMSDAY__ -D__SERVER__)

file (GLOB API_HEADERS ../api/*.h)
file (GLOB_RECURSE HEADERS include/*)
file (GLOB_RECURSE SOURCES src/*.cpp)

# TODO: Get rid of these, shared code should be in libdoomsday.
set (src ../client)
set (SHARED_WITH_CLIENT
    ${src}/include/audio/s_cache.h
    ${src}/include/audio/s_environ.h
    ${src}/include/audio/s_main.h
    ${src}/include/busymode.h
    ${src}/include/color.h
    ${src}/include/con_config.h
    ${src}/include/dd_def.h
    ${src}/include/games.h
    ${src}/include/dd_loop.h
    ${src}/include/dd_main.h
    ${src}/include/dd_pinit.h
    ${src}/include/de_audio.h
    ${src}/include/de_base.h
    ${src}/include/de_console.h
    ${src}/include/de_defs.h
    ${src}/include/de_edit.h
    ${src}/include/de_filesys.h
    ${src}/include/de_graphics.h
    ${src}/include/de_infine.h
    ${src}/include/de_misc.h
    ${src}/include/de_network.h
    ${src}/include/de_platform.h
    ${src}/include/de_play.h
    ${src}/include/de_resource.h
    ${src}/include/de_render.h
    ${src}/include/de_system.h
    ${src}/include/de_ui.h
    ${src}/include/def_main.h
    ${src}/include/edit_map.h
    ${src}/include/face.h
    ${src}/include/game.h
    ${src}/include/hedge.h
    ${src}/include/library.h
    ${src}/include/m_decomp64.h
    ${src}/include/m_misc.h
    ${src}/include/m_nodepile.h
    ${src}/include/m_profiler.h
    ${src}/include/mesh.h
    ${src}/include/network/masterserver.h
    ${src}/include/network/monitor.h
    ${src}/include/network/net_buf.h
    ${src}/include/network/net_event.h
    ${src}/include/network/net_main.h
    ${src}/include/network/net_msg.h
    ${src}/include/partition.h
    ${src}/include/r_util.h
    ${src}/include/resource/animgroup.h
    ${src}/include/resource/colorpalette.h
    ${src}/include/resource/compositetexture.h
    ${src}/include/resource/image.h
    ${src}/include/resource/manifest.h
    ${src}/include/resource/mapdef.h
    ${src}/include/resource/material.h
    ${src}/include/resource/materialarchive.h
    ${src}/include/resource/materialdetaillayer.h
    ${src}/include/resource/materialmanifest.h
    ${src}/include/resource/materialscheme.h
    ${src}/include/resource/materialshinelayer.h
    ${src}/include/resource/materialtexturelayer.h
    ${src}/include/resource/patch.h
    ${src}/include/resource/patchname.h
    ${src}/include/resource/rawtexture.h
    ${src}/include/resource/resourcesystem.h
    ${src}/include/resource/sprite.h
    ${src}/include/resource/texture.h
    ${src}/include/resource/texturemanifest.h
    ${src}/include/resource/texturescheme.h
    ${src}/include/sys_system.h
    ${src}/include/tab_anorms.h
    ${src}/include/ui/busyvisual.h
    ${src}/include/ui/infine/finale.h
    ${src}/include/ui/infine/finaleanimwidget.h
    ${src}/include/ui/infine/finaleinterpreter.h
    ${src}/include/ui/infine/finalepagewidget.h
    ${src}/include/ui/infine/finaletextwidget.h
    ${src}/include/ui/infine/finalewidget.h
    ${src}/include/world/dmuargs.h
    ${src}/include/world/blockmap.h
    ${src}/include/world/bsp/convexsubspaceproxy.h
    ${src}/include/world/bsp/edgetip.h
    ${src}/include/world/bsp/hplane.h
    ${src}/include/world/bsp/linesegment.h
    ${src}/include/world/bsp/partitioner.h
    ${src}/include/world/bsp/partitionevaluator.h
    ${src}/include/world/bsp/superblockmap.h
    ${src}/include/world/bspleaf.h
    ${src}/include/world/bspnode.h
    ${src}/include/world/convexsubspace.h
    ${src}/include/world/entitydatabase.h
    ${src}/include/world/entitydef.h
    ${src}/include/world/impulseaccumulator.h
    ${src}/include/world/interceptor.h
    ${src}/include/world/line.h
    ${src}/include/world/lineblockmap.h
    ${src}/include/world/lineowner.h
    ${src}/include/world/linesighttest.h
    ${src}/include/world/map.h
    ${src}/include/world/maputil.h
    ${src}/include/world/p_object.h
    ${src}/include/world/p_players.h
    ${src}/include/world/p_ticker.h
    ${src}/include/world/plane.h
    ${src}/include/world/polyobj.h
    ${src}/include/world/polyobjdata.h
    ${src}/include/world/propertyvalue.h
    ${src}/include/world/reject.h
    ${src}/include/world/sector.h
    ${src}/include/world/sky.h
    ${src}/include/world/surface.h
    ${src}/include/world/thinkers.h
    ${src}/include/world/vertex.h
    ${src}/include/world/worldsystem.h    

    ${src}/src/api_console.cpp
    ${src}/src/api_filesys.cpp
    ${src}/src/api_uri.cpp
    ${src}/src/audio/s_cache.cpp
    ${src}/src/audio/s_main.cpp
    ${src}/src/busymode.cpp
    ${src}/src/color.cpp
    ${src}/src/con_config.cpp
    ${src}/src/games.cpp
    ${src}/src/dd_loop.cpp
    ${src}/src/dd_main.cpp
    ${src}/src/dd_pinit.cpp
    ${src}/src/dd_plugin.cpp
    ${src}/src/def_main.cpp
    ${src}/src/face.cpp
    ${src}/src/game.cpp
    ${src}/src/hedge.cpp
    ${src}/src/library.cpp
    ${src}/src/m_decomp64.cpp
    ${src}/src/m_misc.cpp
    ${src}/src/m_nodepile.cpp
    ${src}/src/mesh.cpp
    ${src}/src/network/masterserver.cpp
    ${src}/src/network/monitor.cpp
    ${src}/src/network/net_buf.cpp
    ${src}/src/network/net_event.cpp
    ${src}/src/network/net_main.cpp
    ${src}/src/network/net_msg.cpp
    ${src}/src/network/net_ping.cpp
    ${src}/src/r_util.cpp
    ${src}/src/resource/animgroup.cpp
    ${src}/src/resource/api_material.cpp
    ${src}/src/resource/api_resource.cpp
    ${src}/src/resource/colorpalette.cpp
    ${src}/src/resource/compositetexture.cpp
    ${src}/src/resource/hq2x.cpp
    ${src}/src/resource/image.cpp
    ${src}/src/resource/manifest.cpp
    ${src}/src/resource/mapdef.cpp
    ${src}/src/resource/material.cpp
    ${src}/src/resource/materialarchive.cpp
    ${src}/src/resource/materialdetaillayer.cpp
    ${src}/src/resource/materialmanifest.cpp
    ${src}/src/resource/materialscheme.cpp
    ${src}/src/resource/materialshinelayer.cpp
    ${src}/src/resource/materialtexturelayer.cpp
    ${src}/src/resource/patch.cpp
    ${src}/src/resource/patchname.cpp
    ${src}/src/resource/pcx.cpp
    ${src}/src/resource/resourcesystem.cpp
    ${src}/src/resource/sprite.cpp
    ${src}/src/resource/texture.cpp
    ${src}/src/resource/texturemanifest.cpp
    ${src}/src/resource/texturescheme.cpp
    ${src}/src/resource/tga.cpp
    ${src}/src/sys_system.cpp
    ${src}/src/tab_tables.c
    ${src}/src/ui/infine/finale.cpp
    ${src}/src/ui/infine/finaleanimwidget.cpp
    ${src}/src/ui/infine/finaleinterpreter.cpp
    ${src}/src/ui/infine/finalepagewidget.cpp
    ${src}/src/ui/infine/finaletextwidget.cpp
    ${src}/src/ui/infine/finalewidget.cpp
    ${src}/src/ui/infine/infinesystem.cpp
    ${src}/src/world/api_map.cpp
    ${src}/src/world/api_mapedit.cpp
    ${src}/src/world/blockmap.cpp
    ${src}/src/world/bsp/convexsubspaceproxy.cpp
    ${src}/src/world/bsp/hplane.cpp
    ${src}/src/world/bsp/linesegment.cpp
    ${src}/src/world/bsp/partitioner.cpp
    ${src}/src/world/bsp/partitionevaluator.cpp
    ${src}/src/world/bsp/superblockmap.cpp
    ${src}/src/world/bspleaf.cpp
    ${src}/src/world/bspnode.cpp
    ${src}/src/world/convexsubspace.cpp
    ${src}/src/world/dmuargs.cpp
    ${src}/src/world/entitydatabase.cpp
    ${src}/src/world/entitydef.cpp
    ${src}/src/world/impulseaccumulator.cpp
    ${src}/src/world/interceptor.cpp
    ${src}/src/world/line.cpp
    ${src}/src/world/lineblockmap.cpp
    ${src}/src/world/linesighttest.cpp
    ${src}/src/world/map.cpp
    ${src}/src/world/mapelement.cpp
    ${src}/src/world/maputil.cpp
    ${src}/src/world/p_mobj.cpp
    ${src}/src/world/p_players.cpp
    ${src}/src/world/p_ticker.cpp
    ${src}/src/world/plane.cpp
    ${src}/src/world/polyobj.cpp
    ${src}/src/world/polyobjdata.cpp
    ${src}/src/world/propertyvalue.cpp
    ${src}/src/world/reject.cpp
    ${src}/src/world/sector.cpp
    ${src}/src/world/sectorcluster.cpp
    ${src}/src/world/sky.cpp
    ${src}/src/world/surface.cpp
    ${src}/src/world/thinkers.cpp
    ${src}/src/world/vertex.cpp
    ${src}/src/world/worldsystem.cpp
)

if (WIN32)
    include_directories (../client/include/windows)    
    list (APPEND SHARED_WITH_CLIENT ${src}/src/windows/dd_winit.cpp)
endif ()

if (UNIX)
    include_directories (../client/include/unix)
    list (APPEND SHARED_WITH_CLIENT ${src}/src/unix/dd_uinit.cpp)

    if (AMETHYST_FOUND)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday-server.6)
        list (APPEND SOURCES ${MAN_PAGE})
        deng_add_amedoc (MAN ${MAN_PAGE} ${DENG_SOURCE_DIR}/doc/server server.ame)
    endif ()        
endif ()

add_executable (server ${SOURCES} ${HEADERS} ${API_HEADERS} ${SHARED_WITH_CLIENT})
deng_target_defaults (server)
set_target_properties (server PROPERTIES
    OUTPUT_NAME "doomsday-server"
    FOLDER Apps
)

# There's some old code here so relax the warnings a bit.
if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set_property (TARGET server 
        APPEND PROPERTY COMPILE_OPTIONS
            -Wno-missing-field-initializers 
            -Wno-missing-braces
            -Wno-nested-anon-types
            -Wno-gnu-anonymous-struct
    )
endif ()

target_link_libraries (server
    Deng::libcore
    Deng::liblegacy
    Deng::libshell
    Deng::libdoomsday
    lzss
)

deng_install_tool (server)

deng_cotire (server include/precompiled.h)
