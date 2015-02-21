option (DENG_ENABLE_GUI "Enable/disable the client and all GUI related functionality" ON)

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
