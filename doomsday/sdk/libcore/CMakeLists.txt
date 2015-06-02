# Doomsday Engine -- libcore

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBCORE)
include (../../cmake/Config.cmake)

# Dependencies.
include (ZLIB)
find_package (Git QUIET)

# Definitions.
add_definitions (
    -D__DENG2__
    -DLIBDENG2_MAJOR_VERSION=${DENG_VERSION_MAJOR}
    -DLIBDENG2_MINOR_VERSION=${DENG_VERSION_MINOR}
    -DLIBDENG2_PATCHLEVEL=${DENG_VERSION_PATCH}
    -DLIBDENG2_RELEASE_LABEL="${DENG_RELEASE_TYPE}"
    -DLIBDENG2_VERSION="${DENG_VERSION}"
)
if (DEFINED DENG_BUILD)
    add_definitions (-DLIBDENG2_BUILD_TEXT="${DENG_BUILD}")
endif ()
if (GIT_FOUND)
    execute_process (
        COMMAND ${GIT_EXECUTABLE} describe --long 
        OUTPUT_VARIABLE DENG_GIT_DESCRIPTION 
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (DENG_GIT_DESCRIPTION)
        message (STATUS "Revision: ${DENG_GIT_DESCRIPTION}")
        add_definitions (-DLIBDENG2_GIT_DESCRIPTION="${DENG_GIT_DESCRIPTION}")
    endif ()
endif ()

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

if (WIN32)
    # Update the resource file.
    set (DENG_LIBCORE_VERSION_WINDOWS ${DENG_VERSION_WINDOWS})
    configure_file (res/win32/core.rc.in core.rc)
    list (APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/core.rc)
    include_directories (res/win32)
endif ()

deng_add_package (net.dengine.stdlib)

deng_add_library (libcore ${SOURCES} ${HEADERS})
target_include_directories (libcore PRIVATE ${ZLIB_INCLUDE_DIR})
deng_target_link_qt (libcore PUBLIC Core Network)
target_link_libraries (libcore PUBLIC ${ZLIB_LIBRARIES})
deng_deploy_library (libcore DengCore)

deng_cotire (libcore src/precompiled.h)
