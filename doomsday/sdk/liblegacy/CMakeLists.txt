# Doomsday Engine -- liblegacy

cmake_minimum_required (VERSION 3.1)
project (DENG_LIBLEGACY)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)

# Options.
option (DENG_LIBLEGACY_WRITER_TYPE_CHECK 
    "Type-check Writer1/Reader1 behavior (incompatible with no type checking)" 
    OFF
)
if (DENG_LIBLEGACY_WRITER_TYPE_CHECK)
    add_definitions (-DDENG_WRITER_TYPECHECK)
endif ()

# Definitions.
add_definitions (-D__DENG__ -D__DOOMSDAY__)
if (DEFINED DENG_BUILD) 
    add_definitions (-DDOOMSDAY_BUILD_TEXT="${DENG_BUILD}")
endif ()

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/*)
file (GLOB_RECURSE SOURCES src/*.cpp src/*.c)
#file (GLOB_RECURSE C_SOURCES src/*.c)

include_directories (include include/de)

deng_add_library (liblegacy ${SOURCES} ${HEADERS})
target_link_libraries (liblegacy PUBLIC Deng::libcore)
if (WIN32)
    target_link_libraries (liblegacy PRIVATE winmm)
endif ()

deng_deploy_library (liblegacy DengLegacy)
