cmake_minimum_required (VERSION 3.1)
project (DE_UDMFCONVERTER)
include (../ExtensionConfig.cmake)

append (CMAKE_CXX_FLAGS_DEBUG "-DDE_IMPORTUDMF_DEBUG")

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_extlib (importudmf ${SOURCES})
