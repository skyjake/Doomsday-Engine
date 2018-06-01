# Doomsday Engine -- liblegacy

cmake_minimum_required (VERSION 3.1)
project (DE_LIBLEGACY)
include (../../cmake/Config.cmake)

# Dependencies.
find_package (DengCore)

# Options.
option (DE_LIBLEGACY_WRITER_TYPE_CHECK
    "Type-check Writer1/Reader1 behavior (incompatible with no type checking)"
    OFF
)
if (DE_LIBLEGACY_WRITER_TYPE_CHECK)
    add_definitions (-DDE_WRITER_TYPECHECK=1)
endif ()

# Definitions.
add_definitions (-D__DE__=1 -D__DOOMSDAY__=1)
if (DEFINED DE_BUILD)
    add_definitions (-DDOOMSDAY_BUILD_TEXT="${DE_BUILD}")
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
