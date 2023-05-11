# The Doomsday Engine Project -- Client
# Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2014 Daniel Swanson <danij@dengine.net>

cmake_minimum_required (VERSION 3.1)
project (DE_CLIENT)
set (DE_ENABLE_PK3S ON)
include (../../cmake/Config.cmake)

add_subdirectory (libs/importsave)
add_subdirectory (libs/fmod)
if (NOT APPLE)
    # 10.15 deprecates OpenAL in favor of AVAudioEngine
    add_subdirectory (libs/openal)
endif ()
add_subdirectory (libs/fluidsynth)
# if (WIN32)
#     add_subdirectory (libs/directsound)
#     add_subdirectory (libs/winmm)
# endif ()

# Dependencies --------------------------------------------------------------------------

find_package (the_Foundation REQUIRED)
find_package (glbinding REQUIRED)
find_package (Amethyst QUIET)
find_package (SDL2Libs)

# Sources and includes ------------------------------------------------------------------

include_directories (include ${DE_API_DIR})

add_definitions (-D__DOOMSDAY__=1 -D__CLIENT__=1)

file (GLOB         API_HEADERS ../api/*.h)
file (GLOB_RECURSE HEADERS     include/[A-Za-z]*)
file (GLOB         SOURCES     src/*.cpp src/*.c)

deng_merge_sources (audio         src/audio/*.cpp)
deng_merge_sources (audio_base    src/audio/base/*.cpp)
deng_merge_sources (client        src/client/*.cpp)
deng_merge_sources (misc          src/misc/*.cpp)
deng_merge_sources (network       src/network/*.cpp)
deng_merge_sources (network_base  src/network/base/*.cpp)
deng_merge_sources (render        src/render/*.cpp)
deng_merge_sources (render_fx     src/render/fx/*.cpp)
deng_merge_sources (resource      src/resource/*.cpp)
deng_merge_sources (resource_base src/resource/base/*.cpp)
deng_merge_sources (ui            src/ui/*.cpp)
deng_merge_sources (ui_dialogs    src/ui/dialogs/*.cpp)
deng_merge_sources (ui_editors    src/ui/editors/*.cpp)
deng_merge_sources (ui_home       src/ui/home/*.cpp)
deng_merge_sources (ui_infine     src/ui/infine/*.cpp)
deng_merge_sources (ui_widgets    src/ui/widgets/*.cpp)
if (NOT IOS)
    add_definitions (-DDE_HAVE_UPDATER)
    deng_merge_sources (updater   src/updater/*.cpp)
endif ()
deng_merge_sources (world         src/world/*.cpp)
deng_merge_sources (world_base    src/world/base/*.cpp)

deng_glob_sources (SOURCES src/gl/*.cpp) # Can't merge due to GL deferred calls mechanism.

if (APPLE)
    include_directories (include/macx)
    deng_glob_sources (HEADERS include/macx/*.h)
    deng_glob_sources (SOURCES src/macx/*.mm)
elseif (WIN32)
    include_directories (include/windows)
    deng_glob_sources (SOURCES src/windows/*.cpp)
    list (APPEND SOURCES res/windows/doomsday.rc)
endif ()
if (MSYS)
    list (APPEND SOURCES res/windows/doomsday.rc)
endif ()
if (UNIX)
    include_directories (include/unix)
    deng_merge_sources (unix src/unix/*.cpp)
endif ()

# Documentation sources.
if (AMETHYST_FOUND)
    if (APPLE)
        set (readme "${CMAKE_CURRENT_BINARY_DIR}/Read Me.html")
        deng_add_amedoc (HTML "${readme}" ${DE_SOURCE_DIR}/doc/readme readme.ame)
        install (FILES ${readme} DESTINATION .)
        list (APPEND SOURCES ${readme})
    else ()
        set (readme "${CMAKE_CURRENT_BINARY_DIR}/readme.html")
        deng_add_amedoc (HTML ${readme} ${DE_SOURCE_DIR}/doc/readme readme.ame)
        list (APPEND SOURCES ${readme})
        if (WIN32)
            # WIX generator requires setting this manually.
            set_property (INSTALL "${DE_INSTALL_DOC_DIR}/readme.html"
                PROPERTY CPACK_START_MENU_SHORTCUTS "Read Me"
            )
        endif ()
    endif ()
    # Manual page.
    if (UNIX)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday.6)
        deng_add_amedoc (MAN ${MAN_PAGE} ${DE_SOURCE_DIR}/doc/readme readme.ame)
        list (APPEND SOURCES ${MAN_PAGE})
    endif ()
endif ()

deng_filter_platform_sources (src ${SOURCES} ${HEADERS} ${API_HEADERS})

list (APPEND DE_REQUIRED_PACKAGES
    net.dengine.stdlib
    net.dengine.stdlib.gui
    net.dengine.base
    net.dengine.gloom
)
deng_add_package (net.dengine.client)

if (IOS)
    set (MACX_RESOURCES
        doomsday.pk3
    )

    file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/qt_plugin_import.cpp
        "#include <QtPlugin>
        Q_IMPORT_PLUGIN(QIOSIntegrationPlugin)
        Q_IMPORT_PLUGIN(QGifPlugin)
        Q_IMPORT_PLUGIN(QICNSPlugin)
        Q_IMPORT_PLUGIN(QJpegPlugin)
        Q_IMPORT_PLUGIN(QTgaPlugin)")
    list (APPEND src ${CMAKE_CURRENT_BINARY_DIR}/qt_plugin_import.cpp)

    file (GLOB IOS_ICON_FILES res/ios/AppIcon*.png)

    list (APPEND src
        res/ios/LaunchScreen.xib
        ${IOS_ICON_FILES}
    )
endif ()

# OS X: Packages and other resources to bundle with the application.
if (APPLE AND NOT IOS)
    set (MACX_RESOURCES
        res/macx/deng.icns
        res/macx/English.lproj
        doomsday.pk3
        libdoom.pk3
        libdoom64.pk3
        libheretic.pk3
        libhexen.pk3
    )
endif ()

deng_add_application (client ${src} EXTRA_RESOURCES ${MACX_RESOURCES})

# There's some old code here so relax the warnings a bit.
relaxed_warnings (client)

# Libraries -----------------------------------------------------------------------------

if (IOS)
    link_framework (client PUBLIC CoreFoundation)
    link_framework (client PUBLIC QuartzCore)
    link_framework (client PUBLIC CoreGraphics)
    # link_framework (client PUBLIC CoreText)
    link_framework (client PUBLIC Foundation)
    link_framework (client PUBLIC ImageIO)
    link_framework (client PUBLIC MobileCoreServices)
    link_framework (client PUBLIC Security)
    link_framework (client PUBLIC UIKit)

    # Static Qt libraries.
    target_link_libraries (client PUBLIC
        debug ${QT_LIBS}/libqtlibpng_debug.a
        debug ${QT_LIBS}/libqtharfbuzz_debug.a
        debug ${QT_LIBS}/libqtfreetype_debug.a
        debug ${QT_LIBS}/libqtpcre_debug.a
        debug ${QT_LIBS}/libQt5FontDatabaseSupport_debug.a
        debug ${QT_LIBS}/libQt5GraphicsSupport_debug.a
        debug ${QT_LIBS}/libQt5ClipboardSupport_debug.a
        debug ${QT_PLUGINS}/platforms/libqios_debug.a
        debug ${QT_PLUGINS}/imageformats/libqgif_debug.a
        debug ${QT_PLUGINS}/imageformats/libqicns_debug.a
        debug ${QT_PLUGINS}/imageformats/libqtga_debug.a
        debug ${QT_PLUGINS}/imageformats/libqjpeg_debug.a

        optimized ${QT_LIBS}/libqtlibpng.a
        optimized ${QT_LIBS}/libqtharfbuzz.a
        optimized ${QT_LIBS}/libqtfreetype.a
        optimized ${QT_LIBS}/libqtpcre.a
        optimized ${QT_LIBS}/libQt5FontDatabaseSupport.a
        optimized ${QT_LIBS}/libQt5GraphicsSupport.a
        optimized ${QT_LIBS}/libQt5ClipboardSupport.a
        optimized ${QT_PLUGINS}/platforms/libqios.a
        optimized ${QT_PLUGINS}/imageformats/libqgif.a
        optimized ${QT_PLUGINS}/imageformats/libqicns.a
        optimized ${QT_PLUGINS}/imageformats/libqtga.a
        optimized ${QT_PLUGINS}/imageformats/libqjpeg.a

        importdeh
        importidtech1
        importudmf
        doom
        doom64
        heretic
        hexen
        audio_fmod
    )
elseif (APPLE)
    link_framework (client PUBLIC Cocoa)
    if (DE_HAVE_QTKIT)
        link_framework (client PUBLIC QTKit) # MusicPlayer
    endif ()
endif ()
target_link_sdl2 (client PUBLIC)
if (TARGET DirectX)
    target_link_libraries (client PUBLIC DirectX)
endif ()
if (CMAKE_SYSTEM_NAME MATCHES "BSD")
    target_link_libraries (client PUBLIC execinfo) 
endif()

target_link_libraries (client PUBLIC doomsdaygui)
deng_link_libraries (client PUBLIC DengGui DengDoomsday DengGameKit DengGloom)

if (DE_USE_SYSTEM_GLBINDING)
  target_link_libraries (client PUBLIC glbinding)
  deng_link_libraries (client PUBLIC glbinding)
endif()

if (DE_USE_SYSTEM_ASSIMP)
  target_link_libraries (client PUBLIC assimp)
  deng_link_libraries (client PUBLIC assimp)
endif()

# Extensions ----------------------------------------------------------------------------

target_link_libraries (client PRIVATE importsave)

if (TARGET audio_fmod)
    target_link_libraries (client PRIVATE audio_fmod)
    add_definitions (-DDE_HAVE_AUDIO_FMOD=1)
    message (STATUS "Extension: audio_fmod")
endif ()
if (TARGET audio_openal)
    target_link_libraries (client PRIVATE audio_openal)
    add_definitions (-DDE_HAVE_AUDIO_OPENAL=1)
    message (STATUS "Extension: audio_openal")
endif ()
if (TARGET audio_fluidsynth)
    target_link_libraries (client PRIVATE audio_fluidsynth)
    add_definitions (-DDE_HAVE_AUDIO_FLUIDSYNTH=1)
    message (STATUS "Extension: audio_fluidsynth")
endif ()
if (TARGET audio_directsound)
    target_link_libraries (client PRIVATE audio_directsound)
    add_definitions (-DDE_HAVE_AUDIO_DIRECTSOUND=1)
    message (STATUS "Extension: audio_directsound")
endif ()
if (TARGET audio_winmm)
    target_link_libraries (client PRIVATE audio_winmm)
    add_definitions (-DDE_HAVE_AUDIO_WINMM=1)
    message (STATUS "Extension: audio_winmm")
endif ()

# Deployment ----------------------------------------------------------------------------

if (WIN32 OR APPLE)
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "Doomsday")
    set (MACOSX_BUNDLE_BUNDLE_NAME "net.dengine.doomsday")
    set (MACOSX_BUNDLE_ICON_FILE "deng.icns")
    set_target_properties (client PROPERTIES
        OUTPUT_NAME "Doomsday"
        MACOSX_BUNDLE_INFO_PLIST MacOSXBundleInfo.plist.in
    )
else ()
    set_property (TARGET client PROPERTY OUTPUT_NAME "doomsday")
endif ()

if (UNIX_LINUX)
    configure_file (res/linux/net.dengine.Doomsday.desktop.in
        net.dengine.Doomsday.desktop)
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/net.dengine.Doomsday.desktop
        DESTINATION share/applications)
    foreach (dest ${DE_INSTALL_DATA_DIR};share/icons/hicolor/256x256/apps)
        install (FILES res/linux/deng-logo-256.png DESTINATION ${dest}
                 RENAME net.dengine.Doomsday.png)
    endforeach (dest)
    install (FILES res/linux/net.dengine.Doomsday.metainfo.xml
        DESTINATION share/metainfo
        RENAME net.dengine.Doomsday.appdata.xml)
endif ()

if (WIN32)
    # WIX generator requires setting Start Menu shortcut manually.
    set_property (INSTALL "bin/$<TARGET_FILE_NAME:client>"
        PROPERTY CPACK_START_MENU_SHORTCUTS "Doomsday Engine"
    )
    # Also include OpenSSL support.
    if (OPENSSL_DIR)
        find_file (OPENSSL_LIBEAY libeay32.dll
            PATHS ${OPENSSL_DIR}
            PATH_SUFFIXES ${DE_ARCH})
        find_file (OPENSSL_SSLEAY ssleay32.dll
            PATHS ${OPENSSL_DIR}
            PATH_SUFFIXES ${DE_ARCH})
        if (NOT OPENSSL_LIBEAY OR NOT OPENSSL_SSLEAY)
            message (FATAL_ERROR "Could not find OpenSSL libraries (libeay32.dll, ssleay32.dll)")
        endif ()
        deng_install_library (${OPENSSL_LIBEAY})
        deng_install_library (${OPENSSL_SSLEAY})
    endif ()
endif ()

#if (IOS)
#    set_target_properties (client PROPERTIES
#        LINK_FLAGS "-Wl,-e,_qt_main_wrapper"
#        RESOURCE "res/ios/LaunchScreen.xib;${IOS_ICON_FILES}"
#        MACOSX_BUNDLE_INFO_PLIST iOSBundleInfo.plist.in
#        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
#        XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES NO
#    )
#endif ()

if (APPLE AND NOT IOS)
    set_target_properties (client PROPERTIES
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER  net.dengine.doomsday 
        MACOSX_BUNDLE_BUNDLE_NAME                  net.dengine.doomsday
        MACOSX_BUNDLE_INFO_PLIST                   MacOSXBundleInfo.plist.in
    )
    # Install binaries built by targets in the project.
    deng_install_bundle_deps (client
        Deng::libcore
        Deng::libgui
        Deng::libdoomsday
        Deng::libgamekit
        Deng::libgloom
        doom
        doom64
        heretic
        hexen
        the_Foundation::the_Foundation
        glbinding::glbinding
    )
    if (TARGET audio_fmod)
        deng_install_bundle_deps (client fmodex)
    endif ()

    # Plugins are bundled inside the client app. We'll run macdeployqt on the
    # installed app before the plugins are there so it won't do unnecessary
    # work on them -- plugins are already self-contained.
#    get_property (outName TARGET client PROPERTY OUTPUT_NAME)
#    set (stageDir "${DE_BUILD_STAGING_DIR}/DengPlugins")
#    if (IOS)
#        set (destDir "${outName}.app/PlugIns")
#    else ()
#        set (destDir "${outName}.app/Contents/PlugIns/Doomsday")
#    endif ()
#    install (CODE "execute_process (COMMAND ${CMAKE_COMMAND} -E remove_directory
#        \"\${CMAKE_INSTALL_PREFIX}/${destDir}\")")
#    #deng_install_deployqt (client)
#    install (CODE "execute_process (COMMAND ${CMAKE_COMMAND} -E copy_directory \"${stageDir}\"
#        \"\${CMAKE_INSTALL_PREFIX}/${destDir}\")")
else ()
    #deng_install_deployqt (client)
endif ()

deng_deploy_target (client)

if (VC_REDIST_LIBS_DEBUG)
    install (PROGRAMS ${VC_REDIST_LIBS_DEBUG}
        DESTINATION ${DE_INSTALL_LIB_DIR}
        CONFIGURATIONS Debug
        COMPONENT libs
    )
endif ()

if (VC_REDIST_LIBS)
    install (PROGRAMS ${VC_REDIST_LIBS}
        DESTINATION ${DE_INSTALL_LIB_DIR}
        CONFIGURATIONS Release
        COMPONENT libs
    )
endif ()

# if (MSVC)
#     # Bless the build products folder for running the executables.
#     # We have to write a batch file that adds the correct Qt to the path,
#     # or otherwise windeployqt cannot find the correct libraries to deploy.
#     get_filename_component (QT_BINS ${WINDEPLOYQT_COMMAND} DIRECTORY)
#     file (TO_NATIVE_PATH ${QT_BINS} qtBins)
#     file (TO_NATIVE_PATH ${WINDEPLOYQT_COMMAND} winDepCmd)
#     set (fn ${CMAKE_BINARY_DIR}/deployqt.bat)
#     file (WRITE ${fn}
#         "set PATH=\"${qtBins}\";%PATH%
#         \"${winDepCmd}\" -opengl --no-compiler-runtime --no-translations \"${DE_VS_STAGING_DIR}/%1/bin/doomsday.exe\""
#     )
#     set (_intDir ${CMAKE_CFG_INTDIR})
#     if (_intDir STREQUAL ".")
#         set (_intDir ${CMAKE_BUILD_TYPE})
#     endif ()
#     add_custom_command (TARGET client POST_BUILD COMMAND ${fn} ${_intDir})
# endif ()

deng_cotire (client include/precompiled.h)
