cmake_minimum_required (VERSION 3.1)
project (DENG_DIRECTSOUND)
include (../PluginConfig.cmake)

find_package (DirectX)
if (DENG_ENABLE_EAX)
    find_package (EAX)
endif ()

include_directories (include)
file (GLOB SOURCES src/*.cpp include/*.h)
deng_add_plugin (audio_directsound ${SOURCES})
target_link_libraries (audio_directsound PRIVATE DirectX)

if (TARGET EAX2)
    add_definitions (-DHAVE_EAX)
    target_link_libraries (audio_directsound PRIVATE EAX2)
endif ()
