# The Doomsday Engine Project -- Common build config for plugins
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DENG_PLUGINS)
include (${CMAKE_CURRENT_LIST_DIR}/../../cmake/Config.cmake)

find_package (DengDoomsday QUIET)
find_package (LZSS)

macro (deng_add_plugin target)
    sublist (_src 1 -1 ${ARGV})
    add_library (${target} MODULE ${_src} ${DENG_RESOURCES})
    target_include_directories (${target} PUBLIC ${DENG_API_DIR})
    target_link_libraries (${target} 
        PUBLIC Deng::libdoomsday
        PRIVATE lzss
    )
    enable_cxx11 (${target})
    set_target_properties (${target} PROPERTIES FOLDER Plugins)
    if (APPLE)
        set_target_properties (${target} PROPERTIES 
            BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST ${DENG_SOURCE_DIR}/cmake/MacOSXPluginBundleInfo.plist.in            
        )
        set (MACOSX_BUNDLE_BUNDLE_NAME "net.dengine.plugin.${target}")
        set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "${target}.bundle/Contents/MacOS/${target}")
        deng_bundle_resources ()
    else ()
        set_target_properties (${target} VERSION ${DENG_VERSION})
    endif ()
    install (TARGETS ${target} LIBRARY DESTINATION ${DENG_INSTALL_PLUGIN_DIR})
    set (_src)
endmacro (deng_add_plugin)
