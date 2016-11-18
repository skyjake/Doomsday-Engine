# Doomsday Engine -- libgamefw: Game base framework and common routines

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBGAMEFW)
include (../../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengLegacy)

# Definitions.
    add_definitions (
    -D__LIBGAMEFW__=1
)

# Set up the API.
include_directories (${DENG_API_DIR})

# Source and header files.
file (GLOB_RECURSE HEADERS include/gamefw/*)

deng_merge_sources (libgamefw src/*.cpp)

deng_add_library (libgamefw ${SOURCES} ${HEADERS})
target_link_libraries (libgamefw
    PUBLIC Deng::libcore Deng::liblegacy 
)

if (DENG_ENABLE_SDK)
    install (DIRECTORY include/gamefw
        DESTINATION include
        COMPONENT sdk
    )
endif ()
deng_deploy_library (libgamefw DengGamefw)
