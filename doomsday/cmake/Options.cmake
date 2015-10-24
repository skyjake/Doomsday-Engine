option (DENG_ENABLE_GUI "Enable/disable the client and all GUI related functionality" ON)
option (DENG_ENABLE_SDK "Enable/disable installation of the Doomsday 2 SDK" ON)
option (DENG_ENABLE_TOOLS "Compile the Doomsday tools" ON)

if (CCACHE_FOUND OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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
    add_definitions (-DDENG_NO_FIXED_ASM)
endif ()

option (DENG_FAKE_MEMORY_ZONE
    "(Debug) Replace memory zone allocs with real malloc() calls"
    OFF
)
if (DENG_FAKE_MEMORY_ZONE)
    add_definitions (-DLIBDENG_FAKE_MEMORY_ZONE)
endif ()

option (DENG_ENABLE_COUNTED_TRACING
    "(Debug) Keep track of where de::Counted objects are allocated"
    OFF
)
if (DENG_ENABLE_COUNTED_TRACING)
    add_definitions (-DDENG_USE_COUNTED_TRACING)
endif ()

option (DENG_ENABLE_EAX "(DirectSound | OpenAL) Enable/disable EAX (2.0 or later)" ON)
