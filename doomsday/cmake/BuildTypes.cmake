if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL Debug)
    # For single-configuration generators.
    set (DENG_DEVELOPER ON)
endif ()

# Debug ----------------------------------------------------------------------

set (DENG_DEBUG_FLAGS "-D_DEBUG")

append_unique (CMAKE_C_FLAGS_DEBUG   "${DENG_DEBUG_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_DEBUG "${DENG_DEBUG_FLAGS}")

# Release --------------------------------------------------------------------

set (DENG_RELEASE_FLAGS "-DDENG_NO_RANGECHECKING")

append_unique (CMAKE_C_FLAGS_RELEASE          "${DENG_RELEASE_FLAGS}")
append_unique (CMAKE_C_FLAGS_RELWITHDEBINFO   "${DENG_RELEASE_FLAGS}")
append_unique (CMAKE_C_FLAGS_MINSIZEREL       "${DENG_RELEASE_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_RELEASE        "${DENG_RELEASE_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${DENG_RELEASE_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_MINSIZEREL     "${DENG_RELEASE_FLAGS}")
