# Source merging by default is enabled only for debug builds, or never with
# multiconfiguration projects because all configurations must use the
# same set of source files.
if (MSVC OR XCODE_VERSION)
    set (DE_ENABLE_TURBO_DEFAULT OFF) # multiconfiguration projects
else ()
    if (CMAKE_BUILD_TYPE MATCHES "^[DdEeBb].*")
        set (DE_ENABLE_TURBO_DEFAULT OFF) # debug builds shouldn't use merging
    else ()
        set (DE_ENABLE_TURBO_DEFAULT ON)
    endif ()
endif ()

option (DE_ENABLE_TURBO      "Enable Turbo build: merge sources for faster build (avoid when debugging)" ${DE_ENABLE_TURBO_DEFAULT})
option (DE_ENABLE_GUI        "Enable the client app and all GUI related functionality" ON)
option (DE_ENABLE_SHELL      "Enable the Shell GUI app" ON)
option (DE_ENABLE_GLOOM      "Enable use of libgloom for client rendering" OFF)
option (DE_ENABLE_GLOOMED    "Enable the Gloom Editor app" OFF)
option (DE_ENABLE_SERVER     "Enable the server app" ON)
option (DE_ENABLE_SDK        "Enable installation of the Doomsday SDK" OFF)
option (DE_ENABLE_TESTS      "Enable tests" OFF)
option (DE_ENABLE_TOOLS      "Enable tools" ON)
option (DE_ENABLE_DEPLOYMENT "Enable deployment of all dependencies" ON)
option (DE_USE_SYSTEM_ASSIMP "Use system-provided assimp library" OFF)
option (DE_USE_SYSTEM_GLBINDING "Use system-provided glbinding library" OFF)
option (DE_FIXED_ASM
    "Use inline assembler for fixed-point math"
    ${DE_FIXED_ASM_DEFAULT}
)
option (DE_FAKE_MEMORY_ZONE
    "(Debug) Replace memory zone allocs with real malloc() calls"
    OFF
)
option (DE_ENABLE_COUNTED_TRACING
    "(Debug) Keep track of where de::Counted objects are allocated"
    OFF
)

if (APPLE OR CCACHE_FOUND OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC seems to have trouble with cotire when using C++11.
    set (DE_ENABLE_COTIRE_DEFAULT OFF) # just use the cache
else ()
    set (DE_ENABLE_COTIRE_DEFAULT ON)
endif ()
option (DE_ENABLE_COTIRE "Enable precompiled headers (cotire) for faster builds"
    ${DE_ENABLE_COTIRE_DEFAULT}
)

if (NOT DE_FIXED_ASM)
    add_definitions (-DDE_NO_FIXED_ASM=1)
endif ()

if (DE_FAKE_MEMORY_ZONE)
    add_definitions (-DDE_FAKE_MEMORY_ZONE=1)
endif ()

if (DE_ENABLE_COUNTED_TRACING)
    add_definitions (-DDE_USE_COUNTED_TRACING=1)
endif ()

if (DE_ENABLE_GLOOM)
    add_definitions (-DDE_ENABLE_GLOOM=1)
endif ()
