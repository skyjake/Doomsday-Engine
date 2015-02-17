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

deng_add_package (net.dengine.stdlib.gui)

# Definitions.
add_definitions (-D__LIBGUI__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

deng_add_library (libgui ${SOURCES} ${HEADERS})

target_link_libraries (libgui 
    PUBLIC Deng::libcore Qt5::Gui Qt5::OpenGL  
    PRIVATE opengl assimp 
)
if (APPLE)
    link_framework (libgui Cocoa)
endif ()

deng_deploy_library (libgui DengGui)
