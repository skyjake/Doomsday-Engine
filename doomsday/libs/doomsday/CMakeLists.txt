# Doomsday Engine -- libdoomsday

cmake_minimum_required (VERSION 3.1)
project (DE_LIBDOOMSDAY)
include (../../cmake/Config.cmake)

# Extensions.
add_subdirectory (libs/importdeh)
add_subdirectory (libs/importidtech1)
add_subdirectory (libs/importudmf)

# Dependencies.
find_package (LZSS)
include (ZLIB)

# Definitions.
add_definitions (
    -D__DE__=1
    -D__LIBDOOMSDAY__=1
)

# Set up the API.
configure_file (
    ${DE_API_DIR}/dd_version.h.in
    ${DE_API_DIR}/dd_version.h
)

# Source and header files.
file (GLOB_RECURSE HEADERS include/doomsday/*)
file (GLOB SOURCES src/c_wrapper/*.cpp)

deng_merge_sources (libdoomsday src/*.cpp)
deng_merge_sources (audio       src/audio/*.cpp)
deng_merge_sources (defs        src/defs/*.cpp)
deng_merge_sources (console     src/console/*.cpp)
deng_merge_sources (filesys     src/filesys/*.cpp)
deng_merge_sources (gamefw      src/gamefw/*.cpp)
deng_merge_sources (mesh        src/mesh/*.cpp)
deng_merge_sources (network     src/network/*.cpp)
deng_merge_sources (resource    src/resource/*.cpp)
deng_merge_sources (world       src/world/*.cpp)
deng_merge_sources (world_bsp   src/world/bsp/*.cpp)

deng_add_package (net.dengine.base)

deng_add_library (libdoomsday ${SOURCES} ${HEADERS})
relaxed_warnings (libdoomsday)
deng_link_libraries (libdoomsday PUBLIC DengCore)
target_link_libraries (libdoomsday PRIVATE importdeh importidtech1 importudmf lzss)
target_include_directories (libdoomsday
    PUBLIC $<BUILD_INTERFACE:${DE_API_DIR}>
    PRIVATE ${ZLIB_INCLUDE_DIR}
)

if (DE_ENABLE_SDK)
    install (DIRECTORY include/doomsday
        DESTINATION include
        COMPONENT sdk
    )
endif ()
deng_deploy_library (libdoomsday DengDoomsday)
