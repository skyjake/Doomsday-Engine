cmake_minimum_required (VERSION 3.1)
project (DE_UDMFCONVERTER)
include (../PluginConfig.cmake)

append (CMAKE_CXX_FLAGS_DEBUG "-DDE_IMPORTUDMF_DEBUG")

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_plugin (importudmf ${SOURCES})
