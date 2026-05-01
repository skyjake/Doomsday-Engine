cmake_minimum_required (VERSION 3.10)
project (DE_GLOOMED)

include (../../cmake/Config.cmake)
include (QtConfig)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)

find_package (FMOD)
find_package (SDL3Libs)

set (SOURCES
    src/commander.cpp
    src/commander.h
    src/editor.cpp
    src/editor.h
    src/editorapp.cpp
    src/editorapp.h
    src/editorwindow.cpp
    src/editorwindow.h
    src/main.cpp
)

list (APPEND DE_REQUIRED_PACKAGES
    net.dengine.stdlib
    net.dengine.base
    net.dengine.gloom
)

deng_add_application (gloomed ${SOURCES} ${HEADERS})
deng_link_libraries (gloomed PRIVATE DengGloom SDL3::SDL3)
deng_target_link_qt (gloomed PRIVATE Widgets)

if (APPLE)
    set (MACOSX_BUNDLE_BUNDLE_NAME "net.dengine.gloomed")
    set_target_properties (gloomed PROPERTIES
        OUTPUT_NAME gloomed
        MACOSX_BUNDLE_INFO_PLIST ${DE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
    )
    find_package (the_Foundation REQUIRED)
    find_package (glbinding REQUIRED MODULE)
    deng_install_bundle_deps (gloomed
        fmodex
        the_Foundation::the_Foundation
        Deng::libcore
        Deng::libgui
        Deng::libappfw
        Deng::libshell
        Deng::liblegacy
        Deng::libdoomsday
        Deng::libgloom
        glbinding::glbinding
        assimp::assimp
    )
    deng_bundle_sdl3 (gloomed)
    deng_install_deployqt (gloomed)
endif ()

if (APPLE OR WIN32)
    set_property (TARGET gloomed PROPERTY OUTPUT_NAME "GloomEd")
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "GloomEd")
endif ()

deng_deploy_target (gloomed)
