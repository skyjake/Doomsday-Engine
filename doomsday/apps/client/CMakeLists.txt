# The Doomsday Engine Project -- Client
# Copyright (c) 2012-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2014 Daniel Swanson <danij@dengine.net>

cmake_minimum_required (VERSION 3.0)
project (DENG_CLIENT)
set (DENG_ENABLE_PK3S ON)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengAppfw QUIET)
find_package (DengDoomsday QUIET)
find_package (Amethyst QUIET)
find_package (SDL2)
find_package (LZSS)
include (OpenGL)

include_directories (include ${DENG_API_DIR})

add_definitions (-D__DOOMSDAY__ -D__CLIENT__)

file (GLOB API_HEADERS ../api/*.h)
file (GLOB_RECURSE HEADERS include/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.c)

if (APPLE)
    include_directories (include/macx)
    list (APPEND HEADERS include/macx/MusicPlayer.h)
    list (APPEND SOURCES src/macx/MusicPlayer.mm)
endif ()

deng_filter_platform_sources (src ${SOURCES} ${HEADERS} ${API_HEADERS})

if (UNIX)
    include_directories (include/unix)
    if (AMETHYST_FOUND)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday.6)
        list (APPEND src ${MAN_PAGE})
        set (DOC_DIR ${DENG_SOURCE_DIR}/doc/readme)
        file (GLOB ameSrc ${DOC_DIR}/*.ame)
        add_custom_command (
            OUTPUT ${MAN_PAGE}
            COMMAND "${AMETHYST_EXECUTABLE}" -dMAN -dUNIX -odoomsday.6 ${DOC_DIR}/readme.ame
            DEPENDS ${ameSrc}
            COMMENT "Compiling manual page..."
        )
        install (FILES ${MAN_PAGE} DESTINATION share/man/man6)
    endif ()
endif ()

deng_add_package (net.dengine.client)

# OS X: Resources, plugins, and other files to bundle with the application.
set (MACX_RESOURCES 
    res/macx/deng.icns
    res/macx/English.lproj
    doomsday.pk3
    plugin-idtech1converter,DengPlugins
    plugin-savegameconverter,DengPlugins
    plugin-dehread,DengPlugins
    plugin-audio_fmod,DengPlugins
    plugin-audio_openal,DengPlugins
    plugin-audio_fluidsynth,DengPlugins
    plugin-doom,DengPlugins
    plugin-doom64,DengPlugins
    plugin-heretic,DengPlugins
    plugin-hexen,DengPlugins
)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list (APPEND MACX_RESOURCES plugin-example,DengPlugins)
endif ()

deng_add_application (client ${src} EXTRA_RESOURCES ${MACX_RESOURCES})

# There's some old code here so relax the warnings a bit.
if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set_property (TARGET client
        APPEND PROPERTY COMPILE_OPTIONS
            -Wno-missing-field-initializers 
            -Wno-missing-braces
            -Wno-nested-anon-types
            -Wno-gnu-anonymous-struct
            -Wno-deprecated-declarations
    )
endif ()

if (WIN32 OR APPLE)
    set_property (TARGET client PROPERTY OUTPUT_NAME "Doomsday")
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "Doomsday")
    set (MACOSX_BUNDLE_ICON_FILE "deng.icns")
else ()
    set_property (TARGET client PROPERTY OUTPUT_NAME "doomsday")
endif ()

# Libraries.
target_link_libraries (client PUBLIC Deng::libappfw Deng::libdoomsday lzss opengl)
if (APPLE)
    link_framework (client PUBLIC Cocoa)
    link_framework (client PUBLIC QTKit) # MusicPlayer
endif ()
if (TARGET SDL2)
    target_link_libraries (client PUBLIC SDL2)
endif ()
if (TARGET SDL2_mixer)
    target_link_libraries (client PUBLIC SDL2_mixer)
endif ()
