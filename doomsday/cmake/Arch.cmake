math (EXPR _bits "8*${CMAKE_SIZEOF_VOID_P}")
set (ARCH_BITS "${_bits}" CACHE STRING "CPU architecture bits (32/64)")
set (_bits)

if (ARCH_BITS EQUAL 64)
    add_definitions (-DDE_64BIT_HOST=1 -D__64BIT__=1)
    if (WIN32)
        set (DE_ARCH x64)
    else ()
        set (DE_ARCH x86_64)
    endif ()
else ()
    if (WIN32)
        set (DE_ARCH x86)
    else ()
        set (DE_ARCH i386)
    endif ()
endif ()
