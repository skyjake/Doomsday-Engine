# The Doomsday Engine Project -- Common build configuration for tests
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
include (${CMAKE_CURRENT_LIST_DIR}/../cmake/Config.cmake)

find_package (DengCore QUIET)

macro (deng_test target)
    sublist (_src 1 -1 ${ARGV})
    add_executable (${target} ${_src})
    target_link_libraries (${target} Deng::libcore)
    deng_target_defaults (${target})
    set_property (TARGET ${target} PROPERTY FOLDER Tests)    
    install (TARGETS ${target} DESTINATION bin COMPONENT test)
endmacro (deng_test)
