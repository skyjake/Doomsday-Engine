# The Doomsday Engine Project
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (DENG_DOOM)
set (DENG_ENABLE_PK3S ON) # uses the legacy libdoom.pk3
include (../PluginConfig.cmake)
include (../common/Common.cmake)

add_definitions (-D__JDOOM__)

include_directories (include)
file (GLOB_RECURSE SOURCES src/*.c src/*.cpp include/*.h)

deng_find_resources (libdoom.pk3)

deng_add_plugin (doom ${SOURCES} ${COMMON_SOURCES})
