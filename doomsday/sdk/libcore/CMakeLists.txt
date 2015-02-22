# Doomsday Engine -- libcore

cmake_minimum_required (VERSION 3.0)
project (DENG_LIBCORE VERSION 2.0.0)
include (../../cmake/Config.cmake)

set (DENG_LIBCORE_RELEASE_LABEL  "Dev")

# Dependencies.
include (ZLIB)
find_package (Qt5Core REQUIRED)
find_package (Qt5Network REQUIRED)

# Definitions.
add_definitions (
    -D__DENG2__
    -DLIBDENG2_MAJOR_VERSION=${DENG_LIBCORE_VERSION_MAJOR}
    -DLIBDENG2_MINOR_VERSION=${DENG_LIBCORE_VERSION_MINOR}
    -DLIBDENG2_PATCHLEVEL=${DENG_LIBCORE_VERSION_PATCH}
    -DLIBDENG2_RELEASE_LABEL="${DENG_LIBCORE_RELEASE_LABEL}"
    -DLIBDENG2_VERSION="${DENG_LIBCORE_VERSION}"
)
if (DEFINED DENG_BUILD)
    add_definitions (-DLIBDENG2_BUILD_TEXT="${DENG_BUILD}")
endif ()

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.h)

if (WIN32)
    # Update the resource file.
    string (REPLACE . , DENG_LIBCORE_VERSION_WINDOWS "${DENG_LIBCORE_VERSION}.${DENG_BUILD}")
    if (NOT DEFINED DENG_BUILD)
        append (DENG_LIBCORE_VERSION_WINDOWS 0)
    endif ()
    configure_file (res/win32/core.rc.in core.rc)
    list (APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/core.rc)
    include_directories (res/win32)
endif ()

deng_add_package (net.dengine.stdlib)

deng_add_library (libcore ${SOURCES} ${HEADERS})

# libcore uses the 2.x version numbering already; will be synced when 
# the main project reached 2.0.0.
set (DENG_LIBCORE_COMPAT_VERSION 2.0)
if (NOT APPLE)
    set_target_properties (libcore PROPERTIES 
        VERSION   ${DENG_LIBCORE_VERSION}
        SOVERSION ${DENG_LIBCORE_COMPAT_VERSION} # compatibility version
    )
endif ()

target_include_directories (libcore PRIVATE ${ZLIB_INCLUDE_DIR})
target_link_libraries (libcore 
    PUBLIC Qt5::Core Qt5::Network 
    PUBLIC ${ZLIB_LIBRARIES}
)

deng_deploy_library (libcore DengCore)

deng_cotire (libcore src/precompiled.h)
