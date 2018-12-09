# The Doomsday Engine Project
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (DE_LIBGAMEKIT_DOOM)
set (DE_ENABLE_PK3S ON) # uses the legacy libdoom.pk3
include (../GameConfig.cmake)
include (../common/Common.cmake)

add_definitions (
    -D__JDOOM__=1
    -DLIBDOOMSDAY_CUSTOM_MOBJ=1
)

include_directories (include)
file (GLOB_RECURSE SOURCES include/*.h)
file (GLOB SOURCES src/*.c src/*.cpp)
deng_merge_sources (hud_widgets src/hud/widgets/*.cpp)

deng_find_resources (libdoom.pk3)

deng_add_gamelib (doom ${SOURCES} ${COMMON_SOURCES})
