cmake_minimum_required (VERSION 3.1)
project (GLOOM)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (DENG_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set (DENG_INSTALL_DATA_DIR share/doomsday)

# Note: Additionally CMAKE_INSTALL_PREFIX must point to the SDK install dir.
# This is used for finding the .pack files.

include (../../cmake/Macros.cmake)

find_package (Qt5 COMPONENTS Gui Widgets Network OpenGL OpenGLExtensions)
find_package (DengAppfw)

deng_add_package (net.dengine.gloom)

set (SOURCES
    src/approotwidget.cpp
    src/appwindowsystem.cpp
    src/globalshortcuts.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/gloomapp.cpp

    src/approotwidget.h
    src/appwindowsystem.h
    src/globalshortcuts.h
    src/mainwindow.h
    src/gloomapp.h
)

deng_add_application (test_gloom ${SOURCES} ${HEADERS})

target_link_libraries (test_gloom Deng::libappfw)

if (APPLE)
    set_target_properties (test_gloom PROPERTIES
        OUTPUT_NAME test_gloom
        MACOSX_BUNDLE_INFO_PLIST ${DENG_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
     )
    deng_install_bundle_deps (test_gloom
        Deng::libcore Deng::libgui Deng::libappfw Deng::libshell
    )
    deng_install_deployqt (test_gloom)
endif ()
