# Linux / BSD / other Unix
include (PlatformGenericUnix)

if (NOT CMAKE_CXX_COMPILER_ID)
    if (CMAKE_CXX_COMPILER MATCHES ".*g\\+\\+.*")
        message (STATUS "CMake did not detect the compiler; based on name we assume GNU C++")
        set (CMAKE_CXX_COMPILER_ID "GNU")
        set (CMAKE_COMPILER_IS_GNUCXX YES)
    endif ()
endif ()

set (DE_X11 ON)
set (DE_PLATFORM_SUFFIX x11)
set (DE_AMETHYST_PLATFORM UNIX)

set (DE_BASE_DIR    "" CACHE STRING "Base directory path (defaults to {prefix}/${DE_INSTALL_DATA_DIR})")
set (DE_LIBRARY_DIR "" CACHE STRING "Plugin directory path (defaults to {prefix}/${DE_INSTALL_PLUGIN_DIR})")

add_definitions (
    -DDE_X11
    -D__USE_BSD
    -D_GNU_SOURCE=1
)

if (DE_UPDATER_PLATFORM)
    add_definitions (-DDE_PLATFORM_ID="${DE_UPDATER_PLATFORM}-${DE_ARCH}")
else ()
    add_definitions (-DDE_PLATFORM_ID="source")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
    foreach (cxxOpt -Wno-deprecated-copy;-Wno-class-memaccess;-Wno-address-of-packed-member;-Wno-psabi)
        append_unique (CMAKE_CXX_FLAGS ${cxxOpt})
    endforeach (cxxOpt)

    # The tree FRE optimization causes crashes with GCC 6 (Yakkety).
    append_unique (CMAKE_CXX_FLAGS_RELEASE        -fno-tree-fre)
    append_unique (CMAKE_CXX_FLAGS_RELWITHDEBINFO -fno-tree-fre)
    append_unique (CMAKE_CXX_FLAGS_MINSIZEREL     -fno-tree-fre)
endif ()

# All symbols are hidden by default.
append_unique (CMAKE_C_FLAGS   "-fvisibility=hidden")
append_unique (CMAKE_CXX_FLAGS "-fvisibility=hidden")
