# Doomsday Engine - Applications

cmake_minimum_required (VERSION 3.0)
project (DENG_APPS)
include (../cmake/Config.cmake)

add_subdirectory (libdoomsday)
add_subdirectory (server)
#add_subdirectory (plugins)
add_subdirectory (client)
