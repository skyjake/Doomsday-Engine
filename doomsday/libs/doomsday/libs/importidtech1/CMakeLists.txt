cmake_minimum_required (VERSION 3.1)
project (DE_IMPORTIDTECH1)
include (../ExtensionConfig.cmake)

append (CMAKE_CXX_FLAGS_DEBUG "-DDE_IMPORTIDTECH1_DEBUG")

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_extlib (importidtech1 ${SOURCES})
