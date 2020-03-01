cmake_minimum_required (VERSION 3.1)
project (DE_GLOOM)

include (../../cmake/Config.cmake)

find_package (FMOD)
find_Package (SDL2Libs)

list (APPEND DE_REQUIRED_PACKAGES
    net.dengine.stdlib
    net.dengine.stdlib.gui
    net.dengine.base
    net.dengine.gloom
)
deng_add_package (net.dengine.gloom.test)

set (SOURCES
    src/approotwidget.cpp
    src/approotwidget.h
    src/globalshortcuts.cpp
    src/globalshortcuts.h
    src/gloomapp.cpp
    src/gloomapp.h
    src/gloomwidget.cpp
    src/gloomwidget.h
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

# Deploy the FMOD library.
if (APPLE)
    if (TARGET fmodex)
        get_property (fmodLibPath TARGET fmodex PROPERTY INTERFACE_LINK_LIBRARIES)
        deng_find_resources (${fmodLibPath},Frameworks)
    endif ()
endif ()

deng_add_application (gloom ${SOURCES} ${HEADERS})
deng_link_libraries (gloom PRIVATE DengGloom SDL2)

if (APPLE)
    set_target_properties (gloom PROPERTIES
        OUTPUT_NAME Gloom
        MACOSX_BUNDLE_INFO_PLIST ${DE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
     )
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "Gloom")
    deng_install_bundle_deps (gloom
        the_Foundation::the_Foundation
        Deng::libcore
        Deng::libgui
        Deng::libdoomsday
        Deng::libgloom
    )
endif ()

if (WIN32)
    set_property (TARGET gloom PROPERTY OUTPUT_NAME "Gloom")
endif ()
