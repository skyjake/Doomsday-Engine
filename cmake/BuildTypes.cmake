if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL Debug)
    # For single-configuration generators.
    set (DE_DEVELOPER ON)
endif ()

# Debug ----------------------------------------------------------------------

set (DE_DEBUG_FLAGS "-D_DEBUG")

append_unique (CMAKE_C_FLAGS_DEBUG   "${DE_DEBUG_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_DEBUG "${DE_DEBUG_FLAGS}")

# Release --------------------------------------------------------------------

set (DE_RELEASE_FLAGS "-DDE_NO_RANGECHECKING")

append_unique (CMAKE_C_FLAGS_RELEASE          "${DE_RELEASE_FLAGS}")
append_unique (CMAKE_C_FLAGS_RELWITHDEBINFO   "${DE_RELEASE_FLAGS}")
append_unique (CMAKE_C_FLAGS_MINSIZEREL       "${DE_RELEASE_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_RELEASE        "${DE_RELEASE_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${DE_RELEASE_FLAGS}")
append_unique (CMAKE_CXX_FLAGS_MINSIZEREL     "${DE_RELEASE_FLAGS}")
