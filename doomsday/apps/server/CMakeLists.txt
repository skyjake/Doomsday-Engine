# The Doomsday Engine Project -- Server
# Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2015 Daniel Swanson <danij@dengine.net>

cmake_minimum_required (VERSION 3.1)
project (DE_SERVER)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (Amethyst QUIET)

include_directories (include ../client/include ${DE_API_DIR})

add_definitions (-D__DOOMSDAY__=1 -D__SERVER__=1)

file (GLOB API_HEADERS ../api/*.h)
file (GLOB_RECURSE HEADERS include/*.h)
file (GLOB SOURCES src/*.cpp)

deng_merge_sources (server src/server/*.cpp)

# TODO: Get rid of these, shared code should be in libdoomsday.
set (src ../client)
set (SHARED_WITH_CLIENT
    ${src}/include/audio/audiosystem.h
    ${src}/include/audio/s_cache.h
    ${src}/include/con_config.h
    ${src}/include/dd_def.h
    ${src}/include/dd_loop.h
    ${src}/include/dd_main.h
    ${src}/include/dd_pinit.h
    ${src}/include/def_main.h
    ${src}/include/de_base.h
    ${src}/include/de_platform.h
    ${src}/include/edit_map.h
    ${src}/include/misc/color.h
    ${src}/include/misc/r_util.h
    ${src}/include/misc/tab_anorms.h
    ${src}/include/m_profiler.h
    ${src}/include/network/net_buf.h
    ${src}/include/network/net_event.h
    ${src}/include/network/net_main.h
    ${src}/include/network/net_msg.h
    ${src}/include/sys_system.h
    ${src}/include/ui/busyvisual.h
    ${src}/include/ui/infine/finale.h
    ${src}/include/ui/infine/finaleanimwidget.h
    ${src}/include/ui/infine/finaleinterpreter.h
    ${src}/include/ui/infine/finalepagewidget.h
    ${src}/include/ui/infine/finaletextwidget.h
    ${src}/include/ui/infine/finalewidget.h
    ${src}/include/world/impulseaccumulator.h
    ${src}/include/world/map.h
    ${src}/include/world/maputil.h
    ${src}/include/world/p_object.h
    ${src}/include/world/p_players.h
    ${src}/include/world/p_ticker.h

    ${src}/src/api_console.cpp
    ${src}/src/api_filesys.cpp
    ${src}/src/api_mapedit.cpp
    ${src}/src/api_thinker.cpp
    ${src}/src/api_uri.cpp
    ${src}/src/con_config.cpp
    ${src}/src/dd_loop.cpp
    ${src}/src/dd_main.cpp
    ${src}/src/dd_pinit.cpp
    ${src}/src/def_main.cpp
    ${src}/src/sys_system.cpp
)

deng_merge_sources (audio_base    ${src}/src/audio/base/*.cpp)
deng_merge_sources (infine        ${src}/src/ui/infine/*.cpp)
deng_merge_sources (misc          ${src}/src/misc/*.cpp)
deng_merge_sources (net_base      ${src}/src/network/base/*.cpp)
deng_merge_sources (resource_base ${src}/src/resource/base/*.cpp)
deng_merge_sources (world_base    ${src}/src/world/base/*.cpp)

if (WIN32)
    include_directories (../client/include/windows)
    list (APPEND SHARED_WITH_CLIENT ${src}/src/init_windows.cpp)
endif ()

if (UNIX)
    include_directories (../client/include/unix)
    list (APPEND SHARED_WITH_CLIENT ${src}/src/init_unix.cpp)

    if (AMETHYST_FOUND)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday-server.6)
        list (APPEND SOURCES ${MAN_PAGE})
        deng_add_amedoc (MAN ${MAN_PAGE} ${DE_SOURCE_DIR}/doc/server server.ame)
    endif ()
endif ()

add_executable (server ${SOURCES} ${HEADERS} ${API_HEADERS} ${SHARED_WITH_CLIENT})
deng_target_defaults (server)
set_target_properties (server PROPERTIES
    OUTPUT_NAME "doomsday-server"
    FOLDER Apps
)

# There's some old code here so relax the warnings a bit.
relaxed_warnings (server)

deng_link_libraries (server PRIVATE
    DengCore
    DengDoomsday
    DengGameKit
)

# Server is installed as part of the "client" component instead of the
# default "tools".
deng_install_tool (server client)

deng_cotire (server include/precompiled.h)
