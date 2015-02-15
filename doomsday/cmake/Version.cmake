set (DENG_VERSION 1.15.0)

# Binary compatibility version for shared libraries / APIs.
set (DENG_COMPAT_VERSION 1.15)

set (DENG_RELEASE_TYPE 
    Unstable
    #Candidate
    #Stable
)

# Build Configuration --------------------------------------------------------

if (DENG_RELEASE_TYPE STREQUAL "Stable")
    add_definitions (-DDENG_STABLE)
    set (DENG_STABLE 1)
endif ()
if (DEFINED DENG_BUILD)
    add_definitions (-DDOOMSDAY_BUILD_TEXT="${DENG_BUILD}")
endif ()
