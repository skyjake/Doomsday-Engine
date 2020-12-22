# Raspberry Pi (no X11)
include (PlatformGenericUnix)

set (RASPBERRYPI YES)
set (DE_PLATFORM_SUFFIX raspi)
set (DE_AMETHYST_PLATFORM UNIX)

set (DE_BASE_DIR    "" CACHE STRING "Base directory path (defaults to {prefix}/${DE_INSTALL_DATA_DIR})")
set (DE_LIBRARY_DIR "" CACHE STRING "Plugin directory path (defaults to {prefix}/${DE_INSTALL_PLUGIN_DIR})")

add_definitions (
    -DDE_RASPBERRYPI
    -D__USE_BSD
    -D_GNU_SOURCE=1
    -DDE_PLATFORM_ID="source"
)

if (CMAKE_COMPILER_IS_GNUCXX)
    set (cxxOpts
        -Wno-psabi
    )
    foreach (cxxOpt ${cxxOpts})
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

# Broadcom Videocore IV OpenGL ES library.
if (NOT TARGET bcm)
    add_library (bcm INTERFACE)
    target_link_libraries (bcm INTERFACE 
        -L/opt/vc/lib 
        /opt/vc/lib/libGLESv2_static.a 
        brcmEGL 
        brcmGLESv2 
        vcos 
        m
    )
endif ()
