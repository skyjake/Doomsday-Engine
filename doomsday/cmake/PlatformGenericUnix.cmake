add_definitions (-DUNIX)

if (CMAKE_VERSION VERSION_LESS 3.2)
    # This is unnecessary with CMake 3.2+; setting a target property should be enough.
    append_unique (CMAKE_C_FLAGS   "-std=c11")
    append_unique (CMAKE_CXX_FLAGS "-std=c++11")
endif ()
