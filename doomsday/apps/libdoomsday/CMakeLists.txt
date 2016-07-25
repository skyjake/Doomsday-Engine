# Doomsday Engine -- libdoomsday

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBDOOMSDAY)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)
find_package (DengLegacy)
find_package (DengShell)
include (ZLIB)

# Definitions.
add_definitions (
    -D__DENG__=1
    -D__LIBDOOMSDAY__=1
)

# Set up the API.
include_directories (${DENG_API_DIR})
configure_file (
    ${DENG_API_DIR}/dd_version.h.in
    ${DENG_API_DIR}/dd_version.h
)

# Source and header files.
file (GLOB_RECURSE HEADERS include/doomsday/*)
file (GLOB SOURCES src/c_wrapper/*.cpp)

deng_merge_sources (libdoomsday src/*.cpp)
deng_merge_sources (defs        src/defs/*.cpp)
deng_merge_sources (console     src/console/*.cpp)
deng_merge_sources (filesys     src/filesys/*.cpp)
deng_merge_sources (resource    src/resource/*.cpp)
deng_merge_sources (world       src/world/*.cpp)

deng_add_package (net.dengine.base)

deng_add_library (libdoomsday ${SOURCES} ${HEADERS})
relaxed_warnings (libdoomsday)
target_link_libraries (libdoomsday
    PUBLIC Deng::libcore Deng::liblegacy Deng::libshell
)
target_include_directories (libdoomsday PRIVATE ${ZLIB_INCLUDE_DIR})

if (DENG_ENABLE_SDK)
    install (DIRECTORY include/doomsday
        DESTINATION include
        COMPONENT sdk
    )
endif ()
deng_deploy_library (libdoomsday DengDoomsday)
