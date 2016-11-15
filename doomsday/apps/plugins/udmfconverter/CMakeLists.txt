cmake_minimum_required (VERSION 3.1)
project (DENG_UDMFCONVERTER)
include (../PluginConfig.cmake)

append (CMAKE_CXX_FLAGS_DEBUG "-DDENG_UDMFCONVERTER_DEBUG")

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_plugin (udmfconverter ${SOURCES})
