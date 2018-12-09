# Doomsday Engine -- GameKit: game extensions

cmake_minimum_required (VERSION 3.1)
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

deng_merge_sources (libgamekit src/*.cpp)

deng_add_library (libgamekit ${SOURCES} ${HEADERS})
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
