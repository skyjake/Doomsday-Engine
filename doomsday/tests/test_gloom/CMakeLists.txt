cmake_minimum_required (VERSION 3.1)
project (GLOOM)

set (DENG_OPENGL_API 3.3)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (DENG_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set (DENG_INSTALL_LIB_DIR lib)
if (WIN32)
    set (DENG_INSTALL_DATA_DIR data)
else ()
    set (DENG_INSTALL_DATA_DIR share/doomsday)
endif ()

# Note: Additionally CMAKE_INSTALL_PREFIX must point to the SDK install dir.
# This is used for finding the .pack files.

include (../../cmake/Macros.cmake)

find_package (Qt5 COMPONENTS Gui Widgets Network OpenGL OpenGLExtensions)
find_package (DengAppfw)
include (${DENG_SOURCE_DIR}/cmake/FindFMOD.cmake)

deng_add_package (net.dengine.gloom)

set (SOURCES
    gloom/audio/audiosystem.cpp
    gloom/audio/audiosystem.h
    gloom/icamera.h
    gloom/ilight.h
    gloom/ident.h
    gloom/geomath.cpp
    gloom/geomath.h
    gloom/gloomwidget.cpp
    gloom/gloomwidget.h
    gloom/gloomworld.cpp
    gloom/gloomworld.h
    gloom/render/databuffer.h
    gloom/render/entityrender.cpp
    gloom/render/entityrender.h
    gloom/render/gbuffer.cpp
    gloom/render/gbuffer.h
    gloom/render/mapbuild.cpp
    gloom/render/mapbuild.h
    gloom/render/maprender.cpp
    gloom/render/maprender.h
    gloom/render/polygon.cpp
    gloom/render/polygon.h
    gloom/render/skybox.cpp
    gloom/render/skybox.h
    gloom/world/entity.cpp
    gloom/world/entity.h
    gloom/world/entitymap.cpp
    gloom/world/entitymap.h
    gloom/world/environment.cpp
    gloom/world/environment.h
    gloom/world/map.cpp
    gloom/world/map.h
    gloom/world/user.cpp
    gloom/world/user.h
    gloom/world/world.cpp
    gloom/world/world.h
    src/approotwidget.cpp
    src/approotwidget.h
    src/appwindowsystem.cpp
    src/appwindowsystem.h
    src/globalshortcuts.cpp
    src/globalshortcuts.h
    src/gloomapp.cpp
    src/gloomapp.h
    src/editor.cpp
    src/editor.h
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

deng_add_application (test_gloom ${SOURCES} ${HEADERS})

target_link_libraries (test_gloom Deng::libappfw fmodex)

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
