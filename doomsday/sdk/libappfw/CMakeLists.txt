# Doomsday Engine -- libappfw

cmake_minimum_required (VERSION 3.0)
project (DENG_LIBAPPFW)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (Qt5Widgets)
find_package (Qt5OpenGL)
find_package (DengCore QUIET)
find_package (DengGui QUIET)
find_package (DengShell QUIET)
include (OpenGL)

# Definitions.
add_definitions (-D__LIBAPPFW__)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

deng_library (deng_appfw ${SOURCES} ${HEADERS})

target_link_libraries (deng_appfw 
    PUBLIC Deng::deng_core Deng::deng_gui Deng::deng_shell Qt5::Widgets Qt5::OpenGL
)
link_opengl (deng_appfw)

deng_deploy_library (deng_appfw DengAppfw)
