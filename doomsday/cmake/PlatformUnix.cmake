# Linux / BSD / other Unix
include (PlatformGenericUnix)

if (NOT CMAKE_CXX_COMPILER_ID)
    if (CMAKE_CXX_COMPILER MATCHES ".*g\\+\\+.*")
        message (STATUS "CMake did not detect the compiler; based on name we assume GNU C++")
        set (CMAKE_CXX_COMPILER_ID "GNU")
        set (CMAKE_COMPILER_IS_GNUCXX YES)
    endif ()
endif ()

set (DENG_X11 ON)
set (DENG_PLATFORM_SUFFIX x11)
set (DENG_AMETHYST_PLATFORM UNIX)

set (DENG_BASE_DIR    "" CACHE STRING "Base directory path (defaults to {prefix}/${DENG_INSTALL_DATA_DIR})")
set (DENG_LIBRARY_DIR "" CACHE STRING "Plugin directory path (defaults to {prefix}/${DENG_INSTALL_PLUGIN_DIR})")

add_definitions (
    -DDENG_X11
    -D__USE_BSD 
    -D_GNU_SOURCE=1
)
if (DENG_BASE_DIR)
    add_definitions (-DDENG_BASE_DIR="${DENG_BASE_DIR}")
else ()
    add_definitions (-DDENG_BASE_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}")
endif ()
if (DENG_LIBRARY_DIR)
    add_definitions (-DDENG_LIBRARY_DIR="${DENG_LIBRARY_DIR}")
else ()
    add_definitions (-DDENG_LIBRARY_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_PLUGIN_DIR}")
endif ()

if (CPACK_GENERATOR STREQUAL DEB)
    add_definitions (-DDENG_PLATFORM_ID="ubuntu18-${DENG_ARCH}")
elseif (CPACK_GENERATOR STREQUAL RPM)
    add_definitions (-DDENG_PLATFORM_ID="fedora23-${DENG_ARCH}")
else ()
    add_definitions (-DDENG_PLATFORM_ID="source")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
    # The tree FRE optimization causes crashes with GCC 6 (Yakkety).
    append_unique (CMAKE_CXX_FLAGS_RELEASE        -fno-tree-fre)
    append_unique (CMAKE_CXX_FLAGS_RELWITHDEBINFO -fno-tree-fre)
    append_unique (CMAKE_CXX_FLAGS_MINSIZEREL     -fno-tree-fre)
endif ()
