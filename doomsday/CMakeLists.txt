# Doomsday Engine - Main project file
# This CMakeLists builds everything.

cmake_minimum_required (VERSION 3.1)
project (Doomsday)

option (DENG_ENABLE_TOOLS "Compile the Doomsday tools" ON)

add_subdirectory (sdk)
if (DENG_ENABLE_TOOLS)
    add_subdirectory (tools)
endif ()
add_subdirectory (apps)
add_subdirectory (tests)
