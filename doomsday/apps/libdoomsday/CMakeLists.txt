# Doomsday Engine -- libdoomsday

cmake_minimum_required (VERSION 3.0)
project (DENG_LIBDOOMSDAY)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore QUIET)
find_package (DengLegacy QUIET)
find_package (DengShell QUIET)

# Definitions.
add_definitions (
    -D__DENG__
    -D__DOOMSDAY__
    -D__LIBDOOMSDAY__
)

# Set up the API.
include_directories (${DENG_API_DIR})
configure_file (
    ${DENG_API_DIR}/dd_version.h.in
    ${DENG_API_DIR}/dd_version.h
)

# Source and header files.
file (GLOB_RECURSE HEADERS include/doomsday/*)
file (GLOB_RECURSE SOURCES src/*.cpp)

deng_add_package (net.dengine.base)

deng_add_library (libdoomsday ${SOURCES} ${HEADERS})
install (DIRECTORY include/doomsday DESTINATION include)

target_link_libraries (libdoomsday 
    PUBLIC Deng::libcore Deng::liblegacy Deng::libshell
)

deng_deploy_library (libdoomsday DengDoomsday)
