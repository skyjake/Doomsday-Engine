# The Doomsday Engine Project -- Client
# Copyright (c) 2012-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2014 Daniel Swanson <danij@dengine.net>

cmake_minimum_required (VERSION 3.1)
project (DENG_CLIENT)
set (DENG_ENABLE_PK3S ON)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengAppfw)
find_package (DengDoomsday)
find_package (Amethyst QUIET)
find_package (SDL2)
find_package (LZSS)
include (OpenGL)
if (WIN32)
    find_package (DirectX)
endif ()

include_directories (include ${DENG_API_DIR})

add_definitions (-D__DOOMSDAY__ -D__CLIENT__)

file (GLOB API_HEADERS ../api/*.h)
file (GLOB_RECURSE HEADERS include/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.c)

if (APPLE)
    include_directories (include/macx)
    list (APPEND HEADERS include/macx/MusicPlayer.h)
    list (APPEND SOURCES src/macx/MusicPlayer.mm)
elseif (WIN32)
    include_directories (include/windows)
    list (APPEND SOURCES res/windows/doomsday.rc)
endif ()
if (UNIX)
    include_directories (include/unix)
endif ()

if (AMETHYST_FOUND)
    if (APPLE)    
        set (readme "${CMAKE_CURRENT_BINARY_DIR}/Read Me.rtf")
        deng_add_amedoc (RTF ${readme} ${DENG_SOURCE_DIR}/doc/readme readme.ame)
        install (FILES ${readme} DESTINATION .)
        list (APPEND SOURCES ${readme})
    else ()
        # Plain text readme for other platforms.
        set (readme "${CMAKE_CURRENT_BINARY_DIR}/readme.txt")
        deng_add_amedoc (TXT ${readme} ${DENG_SOURCE_DIR}/doc/readme readme.ame)
        list (APPEND SOURCES ${readme})
    endif ()
    # Manual page.
    if (UNIX)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday.6)
        deng_add_amedoc (MAN ${MAN_PAGE} ${DENG_SOURCE_DIR}/doc/readme readme.ame)
        list (APPEND SOURCES ${MAN_PAGE})
    endif ()
endif ()

deng_filter_platform_sources (src ${SOURCES} ${HEADERS} ${API_HEADERS})

deng_add_package (net.dengine.client)

# OS X: Packages and other resources to bundle with the application.
set (MACX_RESOURCES 
    res/macx/deng.icns
    res/macx/English.lproj
    doomsday.pk3
)

deng_add_application (client ${src} EXTRA_RESOURCES ${MACX_RESOURCES})

# There's some old code here so relax the warnings a bit.
relaxed_warnings (client)

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
if (TARGET DirectX)
    target_link_libraries (client PUBLIC DirectX)
endif ()

# Deployment -----------------------------------------------------------------

if (WIN32 OR APPLE)
    set_property (TARGET client PROPERTY OUTPUT_NAME "Doomsday")
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "Doomsday")
    set (MACOSX_BUNDLE_ICON_FILE "deng.icns")
else ()
    set_property (TARGET client PROPERTY OUTPUT_NAME "doomsday")
endif ()

if (APPLE)
    deng_install_bundle_deps (client 
        Deng::libcore Deng::libshell
        Deng::libgui Deng::libappfw
        Deng::liblegacy Deng::libdoomsday
		libassimp SDL2 SDL2_mixer
    )
    
    set_target_properties (client PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    
    # Plugins are bundled inside the client app. We'll run macdeployqt on the 
    # installed app before the plugins are there so it won't do unnecessary 
    # work on them -- plugins are already self-contained.
    get_property (outName TARGET client PROPERTY OUTPUT_NAME)
    set (stageDir "${DENG_BUILD_STAGING_DIR}/DengPlugins")
    set (destDir "${outName}.app/Contents/PlugIns/Doomsday")
    install (CODE "execute_process (COMMAND ${CMAKE_COMMAND} -E remove_directory 
        \"\${CMAKE_INSTALL_PREFIX}/${destDir}\")")
    deng_install_deployqt (client)
    install (CODE "execute_process (COMMAND ${CMAKE_COMMAND} -E copy_directory \"${stageDir}\"
        \"\${CMAKE_INSTALL_PREFIX}/${destDir}\")")
else ()
    deng_install_deployqt (client)
endif ()

if (VC_REDIST_LIBS_DEBUG)
    install (PROGRAMS ${VC_REDIST_LIBS_DEBUG}
        DESTINATION ${DENG_INSTALL_LIB_DIR}
        CONFIGURATIONS Debug
        COMPONENT libs
    )
endif ()

if (VC_REDIST_LIBS)
    install (PROGRAMS ${VC_REDIST_LIBS}
        DESTINATION ${DENG_INSTALL_LIB_DIR}
        CONFIGURATIONS Release
        COMPONENT libs
    )
endif ()

deng_codesign (client)
deng_cotire (client include/precompiled.h)
