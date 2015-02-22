# The Doomsday Engine Project
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DENG_HEXEN)
set (DENG_ENABLE_PK3S ON) # uses the legacy libdoom.pk3
include (../PluginConfig.cmake)
include (../common/Common.cmake)

add_definitions (-D__JHEXEN__)

include_directories (include)
file (GLOB SOURCES src/*.c src/*.cpp include/*.h)

deng_find_resources (libhexen.pk3)

deng_add_plugin (hexen ${SOURCES} ${COMMON_SOURCES} api/hexen.def)
