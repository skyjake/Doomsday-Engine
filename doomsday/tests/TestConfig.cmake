# The Doomsday Engine Project -- Common build configuration for tests
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
include (${CMAKE_CURRENT_LIST_DIR}/../cmake/Macros.cmake)

find_package (DengCore REQUIRED)
find_package (CPlus REQUIRED)

set (DE_INSTALL_DATA_DIR share/doomsday)

macro (deng_test target)
    sublist (_src 1 -1 ${ARGV})
    add_executable (${target} ${_src})
    target_link_libraries (${target} Deng::libcore)
    if (UNIX)
        target_compile_definitions (${target} PRIVATE -DUNIX)
    endif ()
    deng_target_defaults (${target})
    deng_target_rpath (${target})
    set_property (TARGET ${target} PROPERTY FOLDER Tests)
    install (TARGETS ${target} DESTINATION bin COMPONENT test)
endmacro (deng_test)
