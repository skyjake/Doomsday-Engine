# Doomsday Engine -- libappfw

cmake_minimum_required (VERSION 3.1)
project (DE_LIBAPPFW)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengGui)
find_package (DengShell)
find_package (glbinding REQUIRED)
find_package (SDL2Libs)
find_package (OculusVR QUIET)

# Definitions.
add_definitions (-D__LIBAPPFW__=1)
add_definitions (-DDE_OPENGL=330) # TODO: common include w/libgui?

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB SOURCES src/*.h)

deng_merge_sources (libappfw src/*.cpp)
deng_merge_sources (dialogs  src/dialogs/*.cpp)
deng_merge_sources (vr       src/vr/*.cpp)
deng_merge_sources (widgets  src/widgets/*.cpp)

deng_add_library (libappfw ${SOURCES} ${HEADERS})

target_link_libraries (libappfw
    PUBLIC Deng::libcore Deng::libgui Deng::libshell
    PRIVATE SDL2
)
if (TARGET LibOVR)
    target_link_libraries (libappfw PRIVATE LibOVR)
endif ()

deng_deploy_library (libappfw DengAppfw)

deng_cotire (libappfw src/precompiled.h)

if (DE_ENABLE_TESTS)
    set (appfwTests
        test_appfw
    )
    foreach (test ${appfwTests})
        add_subdirectory (../../tests/${test} ${CMAKE_CURRENT_BINARY_DIR}/${test})
    endforeach (test)
endif ()
