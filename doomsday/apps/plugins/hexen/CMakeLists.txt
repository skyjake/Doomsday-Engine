# The Doomsday Engine Project
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (DENG_HEXEN)
set (DENG_ENABLE_PK3S ON) # uses the legacy libdoom.pk3
include (../PluginConfig.cmake)
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

deng_add_plugin (hexen ${SOURCES} ${COMMON_SOURCES})
