# The Doomsday Engine Project
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DENG_FMOD)
include (../PluginConfig.cmake)

find_package (FMOD QUIET)

if (FMOD_FOUND)
    # Deploy the FMOD library.
    get_property (libPath TARGET fmodex PROPERTY INTERFACE_LINK_LIBRARIES)
    if (APPLE)
        deng_find_resources (${libPath},Frameworks)
    else ()
        install (FILES ${libPath} DESTINATION ${DENG_INSTALL_PLUGIN_DIR})
    endif ()
    
    append (CMAKE_CXX_FLAGS_DEBUG "-DDENG_DSFMOD_DEBUG")    
    
    include_directories (include)
    file (GLOB SOURCES src/*.cpp include/*.h)
    deng_add_plugin (audio_fmod ${SOURCES})
    target_link_libraries (audio_fmod PRIVATE fmodex)
    
    if (APPLE)
        deng_bundle_install_names (audio_fmod ${libPath})
    endif ()
endif ()
