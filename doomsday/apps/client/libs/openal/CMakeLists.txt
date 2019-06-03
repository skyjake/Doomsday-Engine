# The Doomsday Engine Project
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.1)
project (DE_OPENAL)
include (CheckIncludeFile)
include (../ExtensionConfig.cmake)

find_package (OpenAL QUIET)

if (OPENAL_FOUND)
    # Check if the headers are in a plain folder.
    set (CMAKE_REQUIRED_INCLUDES ${OPENAL_INCLUDE_DIR})
    check_include_file (al.h HAVE_AL_H)
    if (HAVE_AL_H)
        add_definitions (-DHAVE_AL_H=1)
    endif ()

    append (CMAKE_CXX_FLAGS_DEBUG "-DDE_DSOPENAL_DEBUG")

    include_directories (include)
    include_directories (SYSTEM ${OPENAL_INCLUDE_DIR})

    file (GLOB SOURCES src/*.cpp include/*.h)
    deng_add_extlib (audio_openal ${SOURCES})
    target_link_libraries (audio_openal PRIVATE ${OPENAL_LIBRARY})

    if (WIN32 AND NOT MINGW)
        find_program (OPENAL_DLL OpenAL32.dll)
        mark_as_advanced (OPENAL_DLL)
        deng_install_library (${OPENAL_DLL})
    endif ()
endif ()
