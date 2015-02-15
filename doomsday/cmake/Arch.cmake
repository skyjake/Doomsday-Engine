math (EXPR _bits "8*${CMAKE_SIZEOF_VOID_P}")
set (ARCH_BITS "${_bits}" CACHE STRING "CPU architecture bits (32/64)")
set (_bits)

if (ARCH_BITS EQUAL 64)
    add_definitions (-DDENG_64BIT_HOST)
endif ()
