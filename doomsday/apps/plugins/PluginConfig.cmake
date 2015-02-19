# The Doomsday Engine Project -- Common build config for plugins
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DENG_PLUGINS)
include (${CMAKE_CURRENT_LIST_DIR}/../../cmake/Config.cmake)

find_package (DengDoomsday QUIET)

macro (deng_add_plugin target)
    sublist (_src 1 -1 ${ARGV})
    add_library (${target} MODULE ${_src} ${DENG_RESOURCES})
    target_include_directories (${target} 
        PUBLIC "${DENG_API_DIR}" 
        PRIVATE "${DENG_SOURCE_DIR}/sdk/libgui/include"
    )
    target_link_libraries (${target} PUBLIC Deng::libdoomsday)
    enable_cxx11 (${target})
    set_target_properties (${target} PROPERTIES FOLDER Plugins)
    if (APPLE)
        set_target_properties (${target} PROPERTIES BUNDLE ON)
        # Stage plugins for symlinking/copying into the client app later.
        # This is needed because we want access to these even in a build where the
        # plugins are not installed yet -- the staging directory symlinks to the
        # build directories.
        set (stage "${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_STAGING_DIR}/DengPlugins")
        add_custom_command (TARGET ${target} POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E make_directory "${stage}"
            COMMAND ${CMAKE_COMMAND} -E create_symlink 
                "${CMAKE_CURRENT_BINARY_DIR}/${target}.bundle" 
                "${stage}/${target}.bundle"
        )
        deng_xcode_attribs (${target})
        set_target_properties (${target} PROPERTIES 
            MACOSX_BUNDLE_INFO_PLIST ${DENG_SOURCE_DIR}/cmake/MacOSXPluginBundleInfo.plist.in            
        )        
        macx_set_bundle_name ("net.dengine.plugin.${target}")
        set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "${target}.bundle/Contents/MacOS/${target}")
        deng_bundle_resources ()
    else ()
        set_target_properties (${target} VERSION ${DENG_VERSION})
    endif ()
    install (TARGETS ${target} LIBRARY DESTINATION ${DENG_INSTALL_PLUGIN_DIR})
    set (_src)
endmacro (deng_add_plugin)
