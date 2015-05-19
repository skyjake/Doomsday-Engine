# The Doomsday Engine Project
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (DENG_FMOD)
include (../PluginConfig.cmake)

set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "fmod")

find_package (FMOD QUIET)

if (TARGET fmodex)
    # Deploy the FMOD library.
    get_property (libPath TARGET fmodex PROPERTY INTERFACE_LINK_LIBRARIES)
    if (APPLE)
        deng_find_resources (${libPath},Frameworks)
    endif ()
    
    append (CMAKE_CXX_FLAGS_DEBUG "-DDENG_DSFMOD_DEBUG")    
    
    include_directories (include)
    file (GLOB SOURCES src/*.cpp include/*.h)
    deng_add_plugin (audio_fmod ${SOURCES})
    target_link_libraries (audio_fmod PRIVATE fmodex)
    
    if (APPLE)
        deng_bundle_install_names (audio_fmod ${libPath})
    endif ()
else ()
	message (STATUS "audio_fmod will not be built because FMOD is missing.")
endif ()
