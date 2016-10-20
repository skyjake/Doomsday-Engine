# Linux / BSD / other Unix
include (PlatformGenericUnix)

set (DENG_X11 ON)
set (DENG_PLATFORM_SUFFIX x11)
set (DENG_AMETHYST_PLATFORM UNIX)

add_definitions (
    -DDENG_X11
    -D__USE_BSD 
    -D_GNU_SOURCE=1
    -DDENG_BASE_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_DATA_DIR}"
    -DDENG_LIBRARY_DIR="${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_PLUGIN_DIR}"
)

if (CMAKE_COMPILER_IS_GNUCXX)
    # The tree FRE optimization causes crashes with GCC 6 (Yakkety).
    append_unique (CMAKE_CXX_FLAGS_RELEASE        -fno-tree-fre)
    append_unique (CMAKE_CXX_FLAGS_RELWITHDEBINFO -fno-tree-fre)
    append_unique (CMAKE_CXX_FLAGS_MINSIZEREL     -fno-tree-fre)
endif ()
