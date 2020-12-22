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

option (DE_ENABLE_TURBO      "Enable/disable Turbo mode (source merging)" ${DE_ENABLE_TURBO_DEFAULT})
option (DE_ENABLE_GUI        "Enable/disable the client and all GUI related functionality" ON)
option (DE_ENABLE_GLOOM      "Enable/disable use of libgloom for rendering" OFF)
option (DE_ENABLE_SERVER     "Enable/disable the server executable" ON)
option (DE_ENABLE_SDK        "Enable/disable installation of the Doomsday 2 SDK" ON)
option (DE_ENABLE_TESTS      "Enable/disable tests" OFF)
option (DE_ENABLE_TOOLS      "Compile the Doomsday tools" ON)
option (DE_ENABLE_DEPLOYMENT "Enable/disable the deployment script" ON)
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
option (DE_ENABLE_COTIRE "Enable/disable precompiled headers (cotire) for faster builds"
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
