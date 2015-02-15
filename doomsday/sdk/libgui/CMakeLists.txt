# Doomsday Engine -- libgui

cmake_minimum_required (VERSION 3.0)
project (DENG_LIBGUI)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore QUIET)
find_package (Qt5Gui REQUIRED)
find_package (Qt5OpenGL REQUIRED)
find_package (Assimp REQUIRED)
include (OpenGL)

# Definitions.
add_definitions (-D__LIBGUI__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

deng_library (deng_gui ${SOURCES} ${HEADERS})

target_link_libraries (deng_gui PUBLIC Deng::deng_core Qt5::Gui Qt5::OpenGL)
link_opengl (deng_gui)
link_assimp (deng_gui)
if (APPLE)
    link_framework (deng_gui Cocoa)
endif ()

deng_deploy_library (deng_gui DengGui)
