cmake_minimum_required (VERSION 3.1)
project (DE_WINMM)
include (../ExtensionConfig.cmake)

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)

deng_add_extlib (audio_winmm ${SOURCES})

target_link_libraries (audio_winmm PRIVATE winmm)
