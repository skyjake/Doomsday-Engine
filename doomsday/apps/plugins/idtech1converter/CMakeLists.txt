cmake_minimum_required (VERSION 3.1)
project (DENG_IDTECH1CONVERTER)
include (../PluginConfig.cmake)

append (CMAKE_CXX_FLAGS_DEBUG "-DDENG_IDTECH1CONVERTER_DEBUG")

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_plugin (idtech1converter ${SOURCES})
