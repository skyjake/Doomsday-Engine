cmake_minimum_required (VERSION 3.1)
project (DE_GLOOMED)

include (../../cmake/Config.cmake)
include (QtConfig)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)

find_package (Qt5 COMPONENTS Gui Widgets)

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

deng_add_application (gloomed ${SOURCES} ${HEADERS})
deng_link_libraries (gloomed PRIVATE DengGloom SDL2)
deng_target_link_qt (gloomed PRIVATE Widgets)

if (APPLE)
    set_target_properties (gloomed PROPERTIES
        OUTPUT_NAME gloomed
        MACOSX_BUNDLE_INFO_PLIST ${DE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
     )
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
    )
    deng_install_deployqt (gloomed)
endif ()

if (APPLE OR WIN32)
    set_property (TARGET gloomed PROPERTY OUTPUT_NAME "GloomEd")
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "GloomEd")
endif ()
