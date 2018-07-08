cmake_minimum_required (VERSION 3.1)
project (GLOOM)

include (../../cmake/Config.cmake)
include (QtConfig)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)

find_package (Qt5 COMPONENTS Gui Widgets)
find_package (FMOD)
find_package (Json)

list (APPEND DE_REQUIRED_PACKAGES
    net.dengine.stdlib
    net.dengine.stdlib.gui
    net.dengine.base
)
deng_add_package (net.dengine.gloom)

set (SOURCES
    gloom/audio/audiosystem.cpp
    gloom/audio/audiosystem.h
    gloom/identity.h
    gloom/geo/geomath.cpp
    gloom/geo/geomath.h
    gloom/geo/polygon.cpp
    gloom/geo/polygon.h
    gloom/gloomwidget.cpp
    gloom/gloomwidget.h
    gloom/gloomworld.cpp
    gloom/gloomworld.h
    gloom/render/bloom.cpp
    gloom/render/bloom.h
    gloom/render/context.cpp
    gloom/render/context.h
    gloom/render/databuffer.h
    gloom/render/defs.h
    gloom/render/entityrender.cpp
    gloom/render/entityrender.h
    gloom/render/gbuffer.cpp
    gloom/render/gbuffer.h
    gloom/render/icamera.h
    gloom/render/light.cpp
    gloom/render/light.h
    gloom/render/lightrender.cpp
    gloom/render/lightrender.h
    gloom/render/mapbuild.cpp
    gloom/render/mapbuild.h
    gloom/render/maprender.cpp
    gloom/render/maprender.h
    gloom/render/materiallib.cpp
    gloom/render/materiallib.h
    gloom/render/render.cpp
    gloom/render/render.h
    gloom/render/screenquad.cpp
    gloom/render/screenquad.h
    gloom/render/shadow.cpp
    gloom/render/shadow.h
    gloom/render/skybox.cpp
    gloom/render/skybox.h
    gloom/render/ssao.cpp
    gloom/render/ssao.h
    gloom/render/tonemap.cpp
    gloom/render/tonemap.h
    gloom/render/view.cpp
    gloom/render/view.h
    gloom/world/entity.cpp
    gloom/world/entity.h
    gloom/world/entitymap.cpp
    gloom/world/entitymap.h
    gloom/world/environment.cpp
    gloom/world/environment.h
    gloom/world/map.cpp
    gloom/world/map.h
    gloom/world/mapimport.cpp
    gloom/world/mapimport.h
    gloom/world/sectorpolygonizer.cpp
    gloom/world/sectorpolygonizer.h
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
    src/editorwindow.cpp
    src/editorwindow.h
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/utils.h
)

# Deploy the FMOD library.
if (NOT TARGET fmodex)
    message (FATAL_ERROR "FMOD is required to build Gloom - Set the FMOD_DIR variable")
endif ()
if (APPLE)
    get_property (fmodLibPath TARGET fmodex PROPERTY INTERFACE_LINK_LIBRARIES)
    deng_find_resources (${fmodLibPath},Frameworks)
endif ()

deng_add_application (gloom ${SOURCES} ${HEADERS})

target_include_directories (gloom PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${DE_EXTERNAL_SOURCE_DIR}/gpc)
deng_link_libraries (gloom PRIVATE DengAppfw DengDoomsday fmodex json)
deng_target_link_qt (gloom PRIVATE Widgets)

if (APPLE)
    set_target_properties (gloom PROPERTIES
        OUTPUT_NAME gloom
        MACOSX_BUNDLE_INFO_PLIST ${DE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
     )
    deng_install_bundle_deps (gloom
        Deng::libcore
        Deng::libgui
        Deng::libappfw
        Deng::libshell
        Deng::liblegacy
        Deng::libdoomsday
        CPlus::cplus
        fmodex
    )
    deng_install_deployqt (gloom)
endif ()
