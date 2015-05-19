option (DENG_ENABLE_GUI "Enable/disable the client and all GUI related functionality" ON)

option (DENG_ENABLE_SDK "Enable/disable installation of the Doomsday 2 SDK" ON)

if (CCACHE_FOUND)
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
