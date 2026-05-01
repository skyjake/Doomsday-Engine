cmake_minimum_required (VERSION 3.10)
project (DE_LIBGLOOM)

include (../../cmake/Config.cmake)

find_package (FMOD)
find_package (Json)
find_package (SDL3Libs)

if (TARGET fmodex)
    set (HAVE_FMOD YES)
endif ()

list (APPEND DE_REQUIRED_PACKAGES
    net.dengine.stdlib
    net.dengine.stdlib.gui
    net.dengine.base
)
deng_add_package (net.dengine.gloom)

# Definitions.
add_definitions (-D__LIBGLOOM__=1)

file (GLOB_RECURSE HEADERS include/gloom/[a-zA-Z]*)

deng_sources (src/*.cpp)
deng_sources (src/geo/*.cpp)
deng_sources (src/render/*.cpp)
deng_sources (src/world/*.cpp)
deng_sources (src/audio/*.cpp)

# TODO: This should be a static library.
deng_add_library (libgloom ${SOURCES} ${HEADERS})

deng_link_libraries (libgloom PUBLIC DengGui DengDoomsday)
target_link_libraries (libgloom PRIVATE SDL3::SDL3 json)
if (HAVE_FMOD)
    target_compile_definitions (libgloom PRIVATE -DLIBGLOOM_HAVE_FMOD=1)
    target_link_libraries (libgloom PRIVATE fmodex)
endif ()
deng_deploy_library (libgloom DengGloom)
