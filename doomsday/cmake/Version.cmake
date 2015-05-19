set (DENG_VERSION_MAJOR 2)
set (DENG_VERSION_MINOR 0)
set (DENG_VERSION_PATCH 0)

set (DENG_VERSION ${DENG_VERSION_MAJOR}.${DENG_VERSION_MINOR}.${DENG_VERSION_PATCH})

# Binary compatibility version for shared libraries / APIs.
set (DENG_COMPAT_VERSION 2.0)

string (REPLACE . , DENG_VERSION_WINDOWS "${DENG_VERSION}.${DENG_BUILD}")
if (NOT DEFINED DENG_BUILD)
    set (DENG_VERSION_WINDOWS "${DENG_VERSION_WINDOWS}0")
endif ()

set (DENG_RELEASE_TYPE 
    Unstable
    #Candidate
    #Stable
)

set (DENG_TEAM_COPYRIGHT "Copyright (c) 2003-2015 Deng Team")

# Build Configuration --------------------------------------------------------

if (DENG_RELEASE_TYPE STREQUAL "Stable")
    add_definitions (-DDENG_STABLE)
    set (DENG_STABLE 1)
endif ()
if (DEFINED DENG_BUILD)
    add_definitions (-DDOOMSDAY_BUILD_TEXT="${DENG_BUILD}")
endif ()
