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
        # build directories.
        set (stage "${CMAKE_INSTALL_PREFIX}/${DENG_INSTALL_STAGING_DIR}/DengPlugins")
        add_custom_command (TARGET ${target} POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E make_directory "${stage}"
            COMMAND ${CMAKE_COMMAND} -E create_symlink 
                "${CMAKE_CURRENT_BINARY_DIR}/${target}.bundle" 
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
#         set (_script "postbuild-${target}.cmake")
#         file (WRITE ${_script} "
# include (${DENG_SOURCE_DIR}/cmake/Macros.cmake)
# set (CMAKE_INSTALL_NAME_TOOL ${CMAKE_INSTALL_NAME_TOOL})
# fix_bundled_install_names (\"${CMAKE_INSTALL_PREFIX}/${target}.bundle/Contents/MacOS/${target}\"
# )
# ")
#         add_custom_command (TARGET ${target} POST_BUILD
#             COMMAND ${CMAKE_COMMAND} -P ${_script}
#         )
        deng_xcode_attribs (${target})
        deng_bundle_resources ()
    else ()
        set_target_properties (${target} VERSION ${DENG_VERSION})
    endif ()
    install (TARGETS ${target} LIBRARY DESTINATION ${DENG_INSTALL_PLUGIN_DIR})
    set (_src)
    set (_script)
endmacro (deng_add_plugin)
