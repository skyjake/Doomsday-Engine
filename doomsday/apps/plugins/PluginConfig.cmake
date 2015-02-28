# The Doomsday Engine Project -- Common build config for plugins
# Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DENG_PLUGINS)
include (${CMAKE_CURRENT_LIST_DIR}/../../cmake/Config.cmake)

find_package (DengDoomsday QUIET)

macro (deng_add_plugin target)
    sublist (_src 1 -1 ${ARGV})
    if (WIN32)
        # Find the exports .def and .rc files.
        set (_exports "${CMAKE_CURRENT_SOURCE_DIR}/api/${target}.def")
        if (NOT EXISTS ${_exports})
            message (WARNING "Plugin \"${target}\" is missing the exports .def file.")
        endif ()
        list (APPEND _src ${_exports})
        set (_winres "${CMAKE_CURRENT_SOURCE_DIR}/res/${target}.rc")
        if (NOT EXISTS ${_winres})
            message (WARNING "Plugin \"${target}\" is missing the resource .rc file.")
        endif ()
        list (APPEND _src ${_winres})
    endif ()
    add_library (${target} MODULE ${_src} ${DENG_RESOURCES})
    target_include_directories (${target} 
        PUBLIC "${DENG_API_DIR}" 
        PRIVATE "${DENG_SOURCE_DIR}/sdk/libgui/include"
    )
    target_link_libraries (${target} PUBLIC Deng::libdoomsday)
    enable_cxx11 (${target})
    set_target_properties (${target} PROPERTIES FOLDER Plugins)

    if (APPLE)
        set_target_properties (${target} PROPERTIES 
            BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST ${DENG_SOURCE_DIR}/cmake/MacOSXPluginBundleInfo.plist.in
            BUILD_WITH_INSTALL_RPATH ON  # staging prevents CMake's own rpath fixing
            INSTALL_RPATH "@executable_path/../Frameworks"
        )
        macx_set_bundle_name ("net.dengine.plugin.${target}")
        set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "${target}.bundle/Contents/MacOS/${target}")
        # Stage plugins for symlinking/copying into the client app later.
        # This is needed because we want access to these even in a build where the
        # plugins are not installed yet -- the staging directory symlinks to the
        # individual build directories.
        set (stage "${DENG_BUILD_STAGING_DIR}/DengPlugins")
        add_custom_command (TARGET ${target} POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E make_directory "${stage}"
            COMMAND ${CMAKE_COMMAND} -E create_symlink 
                "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${target}.bundle" 
                "${stage}/${target}.bundle"
        )
        # Fix the Qt framework install names manually.
        deng_bundle_install_names (${target} 
            SCRIPT_NAME "qtlibs"
            LD_PATH "@executable_path/../Frameworks"
            QtCore.framework/Versions/5/QtCore
            QtNetwork.framework/Versions/5/QtNetwork
            VERBATIM
        )
        deng_xcode_attribs (${target})
        deng_bundle_resources ()
    else ()
        set_target_properties (${target} PROPERTIES 
            VERSION ${DENG_VERSION}
        )
        deng_target_rpath (${target})
    endif ()
    
    if (NOT APPLE)
        install (TARGETS ${target} LIBRARY DESTINATION ${DENG_INSTALL_PLUGIN_DIR})
    endif ()
    set (_src)
    set (_script)
endmacro (deng_add_plugin)
