# The Doomsday Engine Project
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (DENG_EXAMPLE)
include (../PluginConfig.cmake)

include_directories (include)

deng_add_plugin (example src/example.c include/version.h)
