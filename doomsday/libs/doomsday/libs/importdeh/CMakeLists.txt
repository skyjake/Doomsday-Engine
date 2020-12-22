cmake_minimum_required (VERSION 3.1)
project (DE_IMPORTDEH)
include (../ExtensionConfig.cmake)

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_extlib (importdeh ${SOURCES})
