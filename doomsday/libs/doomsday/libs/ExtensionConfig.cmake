# The Doomsday Engine Project -- Common build config for extensions
# Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DE_LIBDOOMSDAY_LIBS)
include (${CMAKE_CURRENT_LIST_DIR}/../../../cmake/Config.cmake)

macro (deng_add_extlib target)
    sublist (_src 1 -1 ${ARGV})
    
    add_library (${target} STATIC ${_src} ${DE_RESOURCES})
    target_include_directories (${target} PRIVATE 
        "${DE_API_DIR}"
        ${CMAKE_CURRENT_LIST_DIR}/../../include  # libdoomsday
    )
    deng_link_libraries (${target} PRIVATE DengCore)
    enable_cxx11 (${target})
    set_target_properties (${target} PROPERTIES FOLDER Extensions)
    if (UNIX_LINUX)
        target_compile_options (${target} PRIVATE -fPIC)
    endif ()
    if (APPLE)
        # The plugins have some messy code.
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-braces
        )
        deng_xcode_attribs (${target})
        deng_bundle_resources ()
    else ()
        set_target_properties (${target} PROPERTIES
            VERSION ${DE_VERSION}
        )
        deng_target_rpath (${target})
    endif ()
endmacro (deng_add_extlib)
