# Doomsday Engine -- GameKit: base framework and common routines

cmake_minimum_required (VERSION 3.1)
project (DE_LIBGAMEKIT_GAMEFW)
include (../../../../cmake/Config.cmake)

# Definitions.
add_definitions (
    -D__LIBGAMEFW__=1
)

# Set up the API.
include_directories (${DE_API_DIR})

# Source and header files.
file (GLOB_RECURSE HEADERS include/gamefw/*)

deng_merge_sources (libgamefw src/*.cpp)

deng_add_library (libgamefw ${SOURCES} ${HEADERS})
deng_link_libraries (libgamefw PUBLIC DengCore)

if (DE_ENABLE_SDK)
    install (DIRECTORY include/gamefw
        DESTINATION include
        COMPONENT sdk
    )
endif ()
deng_deploy_library (libgamefw DengGameFw)
