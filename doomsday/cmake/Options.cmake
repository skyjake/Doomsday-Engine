# Source merging by default is enabled only for debug builds, or never with
# multiconfiguration projects because all configurations must use the
# same set of source files.
if (MSVC OR XCODE_VERSION)
    set (DENG_ENABLE_TURBO_DEFAULT OFF) # multiconfiguration projects
else ()
    if (CMAKE_BUILD_TYPE MATCHES "^[DdEeBb].*")
        set (DENG_ENABLE_TURBO_DEFAULT OFF) # debug builds shouldn't use merging
    else ()
        set (DENG_ENABLE_TURBO_DEFAULT ON)
    endif ()
endif ()

option (DENG_ENABLE_TURBO    "Enable/disable Turbo mode (source merging)" ${DENG_ENABLE_TURBO_DEFAULT})
option (DENG_ENABLE_GUI      "Enable/disable the client and all GUI related functionality" ON)
option (DENG_ENABLE_SERVER   "Enable/disable the server executable" ON)
option (DENG_ENABLE_SDK      "Enable/disable installation of the Doomsday 2 SDK" ON)
option (DENG_ENABLE_TOOLS    "Compile the Doomsday tools" ON)
option (DENG_ENABLE_DEPLOYQT "Enable/disable the *deployqt tool" ON)

if (APPLE OR CCACHE_FOUND OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC seems to have trouble with cotire when using C++11.
    set (DENG_ENABLE_COTIRE_DEFAULT OFF) # just use the cache
else ()
    set (DENG_ENABLE_COTIRE_DEFAULT ON)
endif ()
option (DENG_ENABLE_COTIRE "Enable/disable precompiled headers (cotire) for faster builds"
    ${DENG_ENABLE_COTIRE_DEFAULT}
)

option (DENG_FIXED_ASM
    "Use inline assembler for fixed-point math"
    ${DENG_FIXED_ASM_DEFAULT}
)
if (NOT DENG_FIXED_ASM)
    add_definitions (-DDENG_NO_FIXED_ASM=1)
endif ()

option (DENG_FAKE_MEMORY_ZONE
    "(Debug) Replace memory zone allocs with real malloc() calls"
    OFF
)
if (DENG_FAKE_MEMORY_ZONE)
    add_definitions (-DLIBDENG_FAKE_MEMORY_ZONE=1)
endif ()

option (DENG_ENABLE_COUNTED_TRACING
    "(Debug) Keep track of where de::Counted objects are allocated"
    OFF
)
if (DENG_ENABLE_COUNTED_TRACING)
    add_definitions (-DDENG_USE_COUNTED_TRACING=1)
endif ()

option (DENG_ASSIMP_EMBEDDED "Use the Assimp from 'external/assimp' instead of system libraries" YES)
