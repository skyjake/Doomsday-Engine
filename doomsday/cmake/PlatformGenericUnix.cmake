add_definitions (-DUNIX=1)

# Convince the compiler to enable C++11.
include (CheckCXXCompilerFlag)
check_cxx_compiler_flag ("-std=c++11" COMPILER_SUPPORTS_CXX11)
check_cxx_compiler_flag ("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if (COMPILER_SUPPORTS_CXX11)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    set (CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -std=c11")
elseif (COMPILER_SUPPORTS_CXX0X)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else ()
    message (FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif ()
