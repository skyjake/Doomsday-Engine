cmake_minimum_required (VERSION 3.1)
project (DENG_IMPORTSAVE)
include (../PluginConfig.cmake)

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_plugin (importsave ${SOURCES})
