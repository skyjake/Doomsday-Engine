# The Doomsday Engine Project -- Common build config for plugins
# Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>

cmake_minimum_required (VERSION 3.0)
project (DE_CLIENT_LIBS)
include (${CMAKE_CURRENT_LIST_DIR}/../../../cmake/Config.cmake)

macro (deng_add_extlib target)
    sublist (_src 1 -1 ${ARGV})
    
    # Link as a shared library so duplicate symbols are hidden.
    add_library (${target} STATIC ${_src} ${DE_RESOURCES})
    target_include_directories (${target} PRIVATE 
        "${DE_API_DIR}"
        "${DE_SOURCE_DIR}/libs/gui/include" 
    )
    deng_link_libraries (${target} PRIVATE DengDoomsday)
    enable_cxx11 (${target})
    set_target_properties (${target} PROPERTIES FOLDER Extensions)

    if (APPLE)
        # if (IOS)
        #     set_property (TARGET ${target} PROPERTY XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING NO)
        #     link_framework (${target} PUBLIC Foundation)
        #     link_framework (${target} PUBLIC CoreFoundation)
        #     link_framework (${target} PUBLIC MobileCoreServices)
        #     link_framework (${target} PUBLIC UIKit)
        #     link_framework (${target} PUBLIC Security)
        #     target_link_libraries (${target} PUBLIC
        #         debug ${QT_LIBS}/libqtpcre_debug.a
        #         optimized ${QT_LIBS}/libqtpcre.a
        #     )
        # endif ()
        # The plugins have some messy code.
        set_property (TARGET ${target}
            APPEND PROPERTY COMPILE_OPTIONS -Wno-missing-braces
        )
        # set (_extraRPath)
        # if (NOT DE_ENABLE_DEPLOYQT)
        #     set (_extraRPath ${QT_LIBS})
        # endif ()
        # set_target_properties (${target} PROPERTIES
        #     BUNDLE ON
        #     MACOSX_BUNDLE_INFO_PLIST ${DE_SOURCE_DIR}/cmake/MacOSXPluginBundleInfo.plist.in
        #     BUILD_WITH_INSTALL_RPATH ON  # staging prevents CMake's own rpath fixing
        #     INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../Frameworks;${_extraRPath}"
        # )
        # set (_extraRPath)
        # macx_set_bundle_name ("net.dengine.plugin.${target}")
        # if (IOS)
        #     set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "${target}.bundle/${target}")
        # else ()
        #     set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "${target}.bundle/Contents/MacOS/${target}")
        # endif ()
        
        # Stage plugins for symlinking/copying into the client app later.
        # This is needed because we want access to these even in a build where the
        # plugins are not installed yet -- the staging directory symlinks to the
        # individual build directories.
        # set (stage "${DE_BUILD_STAGING_DIR}/DengPlugins")
        # add_custom_command (TARGET ${target} POST_BUILD
        #     COMMAND ${CMAKE_COMMAND} -E make_directory "${stage}"
        #     COMMAND ${CMAKE_COMMAND} -E create_symlink
        #         "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${target}.bundle"
        #         "${stage}/${target}.bundle"
        # )
        deng_xcode_attribs (${target})
        deng_bundle_resources ()
    else ()
        set_target_properties (${target} PROPERTIES
            VERSION ${DE_VERSION}
        )
        deng_target_rpath (${target})
    endif ()

    # if (NOT APPLE)
    # install (TARGETS ${target}
    #     EXPORT ${target}
    #     ARCHIVE DESTINATION ${DE_INSTALL_LIB_DIR}
    #     COMPONENT libs
    # )
    # install (EXPORT ${target}
    #     DESTINATION ${DE_INSTALL_CMAKE_DIR}/${target}
    #     FILE ${target}-config.cmake
    #     NAMESPACE Deng::
    #     COMPONENT sdk
    # )
    # endif ()
    # set (_src)
    # set (_script)
endmacro (deng_add_extlib)
