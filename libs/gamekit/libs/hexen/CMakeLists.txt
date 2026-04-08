# The Doomsday Engine Project
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.10)
project (DE_LIBGAMEKIT_HEXEN)
set (DE_ENABLE_PK3S ON) # uses the legacy libdoom.pk3
include (../GameConfig.cmake)
include (../common/Common.cmake)

add_definitions (
    -D__JHEXEN__=1
    -DLIBDOOMSDAY_CUSTOM_MOBJ=1
    -DHAVE_EARTHQUAKE=1
)

include_directories (include)
file (GLOB_RECURSE SOURCES include/*.h)
file (GLOB SOURCES src/*.c src/*.cpp)
deng_merge_sources (hud_widgets src/hud/widgets/*.cpp)

deng_find_resources (libhexen.pk3)

deng_add_gamelib (hexen ${SOURCES} ${COMMON_SOURCES})
target_precompile_headers (hexen PRIVATE "${DE_SOURCE_DIR}/cmake/pch/pch_jhexen.h")
