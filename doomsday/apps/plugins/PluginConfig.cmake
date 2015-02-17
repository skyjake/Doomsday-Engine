# The Doomsday Engine Project -- Common build config for plugins
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DENG_PLUGINS)
include (${CMAKE_CURRENT_LIST_DIR}/../../cmake/Config.cmake)

find_package (DengDoomsday QUIET)

macro (deng_add_plugin target)
    sublist (_src 1 -1 ${ARGV})
    add_library (${target} SHARED ${_src})
    target_link_libraries (${target} Deng::libdoomsday)
    enable_cxx11 (${target})
    if (APPLE)
        set_property (TARGET ${target} PROPERTY BUNDLE ON)
    endif ()
    install (TARGETS ${target} 
        LIBRARY DESTINATION ${DENG_INSTALL_PLUGIN_DIR}
    )
    set (_src)
endmacro (deng_add_plugin)
