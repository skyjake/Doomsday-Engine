# Doomsday Engine -- GameKit: game extensions

cmake_minimum_required (VERSION 3.10)
project (DE_LIBGAMEKIT)
include (../../cmake/Config.cmake)

add_subdirectory (libs/doom)
add_subdirectory (libs/doom64)
add_subdirectory (libs/heretic)
add_subdirectory (libs/hexen)

# Definitions.
add_definitions (
    -D__LIBGAMEKIT__=1
)

# Set up the API.
include_directories (${DE_API_DIR})

# Source and header files.
file (GLOB_RECURSE HEADERS include/gamekit/*)

deng_sources (src/*.cpp)

deng_add_library (libgamekit ${SOURCES} ${HEADERS})
target_precompile_headers (libgamekit PRIVATE "${DE_SOURCE_DIR}/cmake/pch/pch_gamekit_lib.h")
deng_link_libraries (libgamekit PUBLIC DengCore)

# Game extensions.
target_link_libraries (libgamekit PRIVATE doom doom64 heretic hexen)

# if (DE_ENABLE_SDK)
#     install (DIRECTORY include/gamekit
#         DESTINATION include
#         COMPONENT sdk
#     )
# endif ()
deng_deploy_library (libgamekit DengGameKit)
