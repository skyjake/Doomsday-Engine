cmake_minimum_required (VERSION 3.1)
project (GLOOM)

# set (DENG_OPENGL_API 3.3)
# set (CMAKE_AUTOMOC ON)
# set (CMAKE_AUTORCC ON)
# set (DENG_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
# set (DENG_INSTALL_LIB_DIR lib)
# if (WIN32)
#     set (DENG_INSTALL_DATA_DIR data)
# else ()
#     set (DENG_INSTALL_DATA_DIR share/doomsday)
# endif ()

# Note: Additionally CMAKE_INSTALL_PREFIX must point to the SDK install dir.
# This is used for finding the .pack files.

# find_package (PythonInterp REQUIRED)
# include (../../cmake/Arch.cmake)
# include (../../cmake/Macros.cmake)

include (../../cmake/Config.cmake)

find_package (Qt5 COMPONENTS Gui Widgets Network OpenGL OpenGLExtensions)
find_package (DengAppfw)
find_package (DengDoomsday)
find_package (FMOD)

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
    # Polygon clipper
    #${DENG_EXTERNAL_SOURCE_DIR}/gpc/gpc.c
    #${DENG_EXTERNAL_SOURCE_DIR}/gpc/gpc.h
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

target_include_directories (gloom PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${DENG_EXTERNAL_SOURCE_DIR}/gpc)
target_link_libraries (gloom Deng::libappfw Deng::libdoomsday fmodex)

if (APPLE)
    set_target_properties (gloom PROPERTIES
        OUTPUT_NAME gloom
        MACOSX_BUNDLE_INFO_PLIST ${DENG_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
     )
    deng_install_bundle_deps (gloom
        Deng::libcore Deng::libgui Deng::libappfw Deng::libshell Deng::libdoomsday
    )
    deng_install_deployqt (gloom)
endif ()
