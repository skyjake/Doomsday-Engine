option (DENG_FIXED_ASM 
    "Use inline assembler for fixed-point math"
    ${DENG_FIXED_ASM_DEFAULT}
)
if (NOT DENG_FIXED_ASM)
    add_definitions (-DDENG_NO_FIXED_ASM)
endif ()

option (DENG_FAKE_MEMORY_ZONE
    "Replace memory zone allocs with real malloc() calls (for debugging)"
    OFF
)
if (DENG_FAKE_MEMORY_ZONE)
    add_definitions (-DLIBDENG_FAKE_MEMORY_ZONE)
endif ()
