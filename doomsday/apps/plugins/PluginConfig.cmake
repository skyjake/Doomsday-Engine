# The Doomsday Engine Project -- Common build config for plugins
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DENG_PLUGINS)
include (${CMAKE_CURRENT_LIST_DIR}/../../cmake/Config.cmake)

find_package (DengDoomsday)
find_package (DengGamefw)   # Game framework is available in all plugins.

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
    target_link_libraries (${target} PUBLIC Deng::libdoomsday Deng::libgamefw)
    enable_cxx11 (${target})
    set_target_properties (${target} PROPERTIES FOLDER Plugins)

    if (APPLE)
        # The plugins have some messy code.
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-braces
        )
        set (_extraRPath)
        if (NOT DENG_ENABLE_DEPLOYQT)
            set (_extraRPath ${QT_LIBS})
        endif ()
        set_target_properties (${target} PROPERTIES
            BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST ${DENG_SOURCE_DIR}/cmake/MacOSXPluginBundleInfo.plist.in
            BUILD_WITH_INSTALL_RPATH ON  # staging prevents CMake's own rpath fixing
            INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../Frameworks;${_extraRPath}"
        )
        set (_extraRPath)
        macx_set_bundle_name ("net.dengine.plugin.${target}")
        if (IOS)
            set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "${target}.bundle/${target}")
        else ()
            set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "${target}.bundle/Contents/MacOS/${target}")
        endif ()
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
        if (DENG_ENABLE_DEPLOYQT)
            deng_bundle_install_names (${target}
                SCRIPT_NAME "qtlibs"
                LD_PATH "@executable_path/../Frameworks"
                QtCore.framework/Versions/5/QtCore
                QtNetwork.framework/Versions/5/QtNetwork
                VERBATIM
            )
        endif ()
        deng_xcode_attribs (${target})
        deng_bundle_resources ()
    else ()
        set_target_properties (${target} PROPERTIES
            VERSION ${DENG_VERSION}
        )
        deng_target_rpath (${target})
    endif ()

    if (MSVC)
        set_target_properties (${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DENG_VS_STAGING_DIR}/Debug/${DENG_INSTALL_PLUGIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DENG_VS_STAGING_DIR}/Release/${DENG_INSTALL_PLUGIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${DENG_VS_STAGING_DIR}/MinSizeRel/${DENG_INSTALL_PLUGIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DENG_VS_STAGING_DIR}/RelWithDebInfo/${DENG_INSTALL_PLUGIN_DIR}"

            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${DENG_VS_STAGING_DIR}/Debug/${DENG_INSTALL_PLUGIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${DENG_VS_STAGING_DIR}/Release/${DENG_INSTALL_PLUGIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${DENG_VS_STAGING_DIR}/MinSizeRel/${DENG_INSTALL_PLUGIN_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DENG_VS_STAGING_DIR}/RelWithDebInfo/${DENG_INSTALL_PLUGIN_DIR}"
        )
    endif ()

    if (NOT APPLE)
        install (TARGETS ${target} LIBRARY DESTINATION ${DENG_INSTALL_PLUGIN_DIR})
    endif ()
    set (_src)
    set (_script)
endmacro (deng_add_plugin)
