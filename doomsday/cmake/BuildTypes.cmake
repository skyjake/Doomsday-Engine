if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL Debug)
    # For single-configuration generators.
    set (DENG_DEVELOPER ON)
endif ()

# Debug ----------------------------------------------------------------------

set (DENG_DEBUG_FLAGS "-D_DEBUG")

append (CMAKE_C_FLAGS         "${DENG_DEBUG_FLAGS}")
append (CMAKE_CXX_FLAGS       "${DENG_DEBUG_FLAGS}")
append (CMAKE_C_FLAGS_DEBUG   "${DENG_DEBUG_FLAGS}")
append (CMAKE_CXX_FLAGS_DEBUG "${DENG_DEBUG_FLAGS}")

# Release --------------------------------------------------------------------

set (DENG_RELEASE_FLAGS "-DDENG_NO_RANGECHECKING")

append (CMAKE_C_FLAGS_RELEASE          "${DENG_RELEASE_FLAGS}")
append (CMAKE_C_FLAGS_RELWITHDEBINFO   "${DENG_RELEASE_FLAGS}")
append (CMAKE_C_FLAGS_MINSIZEREL       "${DENG_RELEASE_FLAGS}")
append (CMAKE_CXX_FLAGS_RELEASE        "${DENG_RELEASE_FLAGS}")
append (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${DENG_RELEASE_FLAGS}")
append (CMAKE_CXX_FLAGS_MINSIZEREL     "${DENG_RELEASE_FLAGS}")
