# Doomsday Engine -- Network Communications

cmake_minimum_required (VERSION 3.1)
project (DE_LIBCOMMS)
include (../../cmake/Config.cmake)

# Definitions.
add_definitions (-D__LIBSHELL__=1)

# Source and header files.
file (GLOB_RECURSE HEADERS include/de/comms/*)
file (GLOB SOURCES src/*.h)

deng_merge_sources (libcomms src/*.cpp)

deng_add_library (libcomms ${SOURCES} ${HEADERS})
deng_link_libraries (libcomms PUBLIC DengCore)
deng_deploy_library (libcomms DengComms)
deng_cotire (libcomms src/precompiled.h)
