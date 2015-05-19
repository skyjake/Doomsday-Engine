cmake_minimum_required (VERSION 3.1)
project (DENG_DIRECTSOUND)
include (../PluginConfig.cmake)

find_package (DirectX)
find_package (EAX)

if (TARGET EAX2)
    include_directories (include)
    file (GLOB SOURCES src/*.cpp include/*.h)

    deng_add_plugin (audio_directsound ${SOURCES})
    target_link_libraries (audio_directsound PRIVATE DirectX EAX2)
else ()	
	message (STATUS "audio_directsound will not be built because EAX2 is missing.")
endif ()
