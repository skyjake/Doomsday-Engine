# The Doomsday Engine Project -- Client
# Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2014 Daniel Swanson <danij@dengine.net>

cmake_minimum_required (VERSION 3.1)
project (DENG_CLIENT)
set (DENG_ENABLE_PK3S ON)
include (../../cmake/Config.cmake)

# Dependencies --------------------------------------------------------------------------

find_package (DengAppfw)
find_package (DengDoomsday)
find_package (Amethyst QUIET)
find_package (SDL2)
if (WIN32)
    find_package (DirectX)
endif ()

# Sources and includes ------------------------------------------------------------------

include_directories (include ${DENG_API_DIR})

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
    add_definitions (-DDENG_HAVE_UPDATER)
    deng_merge_sources (updater   src/updater/*.cpp)
endif ()
deng_merge_sources (world         src/world/*.cpp)
deng_merge_sources (world_base    src/world/base/*.cpp)
deng_merge_sources (world_bsp     src/world/bsp/*.cpp)

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
if (UNIX)
    include_directories (include/unix)
    deng_merge_sources (unix src/unix/*.cpp)
endif ()

deng_filter_platform_sources (src ${SOURCES} ${HEADERS} ${API_HEADERS})

deng_add_package (net.dengine.client)

# OS X: Packages and other resources to bundle with the application.
set (MACX_RESOURCES
    res/macx/deng.icns
    res/macx/English.lproj
    doomsday.pk3
)

# Windows: Use Qt resources.
if (WIN32)
    qt5_add_resources (RCC_SOURCES res/windows/client.qrc)
    list (APPEND src ${RCC_SOURCES})
endif ()

if (IOS)
    file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/qt_plugin_import.cpp
"#include <QtPlugin>
Q_IMPORT_PLUGIN(QIOSIntegrationPlugin)
Q_IMPORT_PLUGIN(QGifPlugin)
Q_IMPORT_PLUGIN(QICNSPlugin)
Q_IMPORT_PLUGIN(QJpegPlugin)
Q_IMPORT_PLUGIN(QTgaPlugin)")
    list (APPEND src ${CMAKE_CURRENT_BINARY_DIR}/qt_plugin_import.cpp)
endif ()

deng_add_application (client ${src} EXTRA_RESOURCES ${MACX_RESOURCES})

# There's some old code here so relax the warnings a bit.
relaxed_warnings (client)

# Libraries -----------------------------------------------------------------------------

if (IOS)
    link_framework (client PUBLIC CoreFoundation)
    link_framework (client PUBLIC QuartzCore)
    link_framework (client PUBLIC CoreGraphics)
    link_framework (client PUBLIC CoreText)
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
    )
elseif (APPLE)
    link_framework (client PUBLIC Cocoa)
    if (DENG_HAVE_QTKIT)
        link_framework (client PUBLIC QTKit) # MusicPlayer
    endif ()
endif ()
if (TARGET SDL2)
    target_link_libraries (client PUBLIC SDL2)
endif ()
if (TARGET SDL2_mixer)
    target_link_libraries (client PUBLIC SDL2_mixer)
endif ()
if (TARGET DirectX)
    target_link_libraries (client PUBLIC DirectX)
endif ()

target_link_libraries (client PUBLIC Deng::libappfw Deng::libdoomsday)

# Deployment ----------------------------------------------------------------------------

if (AMETHYST_FOUND)
    if (APPLE)
        set (readme "${CMAKE_CURRENT_BINARY_DIR}/Read Me.html")
        deng_add_amedoc (HTML ${readme} ${DENG_SOURCE_DIR}/doc/readme readme.ame)
        install (FILES ${readme} DESTINATION .)
        list (APPEND SOURCES ${readme})
    else ()
        set (readme "${CMAKE_CURRENT_BINARY_DIR}/readme.html")
        deng_add_amedoc (HTML ${readme} ${DENG_SOURCE_DIR}/doc/readme readme.ame)
        list (APPEND SOURCES ${readme})
        if (WIN32)
            # WIX generator requires setting this manually.
            set_property (INSTALL "${DENG_INSTALL_DOC_DIR}/readme.html"
                PROPERTY CPACK_START_MENU_SHORTCUTS "Read Me"
            )
        endif ()
    endif ()
    # Manual page.
    if (UNIX)
        set (MAN_PAGE ${CMAKE_CURRENT_BINARY_DIR}/doomsday.6)
        deng_add_amedoc (MAN ${MAN_PAGE} ${DENG_SOURCE_DIR}/doc/readme readme.ame)
        list (APPEND SOURCES ${MAN_PAGE})
    endif ()
endif ()

if (WIN32 OR APPLE)
    set_property (TARGET client PROPERTY OUTPUT_NAME "Doomsday")
    set (MACOSX_BUNDLE_BUNDLE_EXECUTABLE "Doomsday")
    if (IOS)
        set (MACOSX_BUNDLE_ICON_FILE "deng.icns")
    else ()
        set (MACOSX_BUNDLE_ICON_FILE "deng.icns")
    endif ()
else ()
    set_property (TARGET client PROPERTY OUTPUT_NAME "doomsday")
endif ()

if (UNIX_LINUX)
    configure_file (doomsday-engine.desktop.in net.dengine.Doomsday.desktop)
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/net.dengine.Doomsday.desktop
        DESTINATION share/applications)
    install (FILES res/linux/deng-logo-256.png
        DESTINATION ${DENG_INSTALL_DATA_DIR})
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
            PATH_SUFFIXES ${DENG_ARCH})
        find_file (OPENSSL_SSLEAY ssleay32.dll
            PATHS ${OPENSSL_DIR}
            PATH_SUFFIXES ${DENG_ARCH})
        if (NOT OPENSSL_LIBEAY OR NOT OPENSSL_SSLEAY)
            message (FATAL_ERROR "Could not find OpenSSL libraries (libeay32.dll, ssleay32.dll)")
        endif ()
        deng_install_library (${OPENSSL_LIBEAY})
        deng_install_library (${OPENSSL_SSLEAY})
    endif ()
endif ()

if (IOS)
    set_target_properties (client PROPERTIES
        LINK_FLAGS "-Wl,-e,_qt_main_wrapper"
        MACOSX_BUNDLE_INFO_PLIST iOSBundleInfo.plist.in
    )
endif ()

if (APPLE)
    deng_install_bundle_deps (client
        Deng::libcore Deng::libshell Deng::libgui Deng::libappfw
        Deng::liblegacy Deng::libdoomsday Deng::libgamefw
        SDL2 SDL2_mixer
    )

    # Plugins are bundled inside the client app. We'll run macdeployqt on the
    # installed app before the plugins are there so it won't do unnecessary
    # work on them -- plugins are already self-contained.
    get_property (outName TARGET client PROPERTY OUTPUT_NAME)
    set (stageDir "${DENG_BUILD_STAGING_DIR}/DengPlugins")
    if (IOS)
        set (destDir "${outName}.app/PlugIns")
    else ()
        set (destDir "${outName}.app/Contents/PlugIns/Doomsday")
    endif ()
    install (CODE "execute_process (COMMAND ${CMAKE_COMMAND} -E remove_directory
        \"\${CMAKE_INSTALL_PREFIX}/${destDir}\")")
    deng_install_deployqt (client)
    install (CODE "execute_process (COMMAND ${CMAKE_COMMAND} -E copy_directory \"${stageDir}\"
        \"\${CMAKE_INSTALL_PREFIX}/${destDir}\")")
else ()
    deng_install_deployqt (client)
endif ()

if (VC_REDIST_LIBS_DEBUG)
    install (PROGRAMS ${VC_REDIST_LIBS_DEBUG}
        DESTINATION ${DENG_INSTALL_LIB_DIR}
        CONFIGURATIONS Debug
        COMPONENT libs
    )
endif ()

if (VC_REDIST_LIBS)
    install (PROGRAMS ${VC_REDIST_LIBS}
        DESTINATION ${DENG_INSTALL_LIB_DIR}
        CONFIGURATIONS Release
        COMPONENT libs
    )
endif ()

if (MSVC)
    # Bless the build products folder for running the executables.
    # We have to write a batch file that adds the correct Qt to the path,
    # or otherwise windeployqt cannot find the correct libraries to deploy.
    get_filename_component (QT_BINS ${WINDEPLOYQT_COMMAND} DIRECTORY)
    file (TO_NATIVE_PATH ${QT_BINS} qtBins)
    file (TO_NATIVE_PATH ${WINDEPLOYQT_COMMAND} winDepCmd)
    set (fn ${CMAKE_BINARY_DIR}/deployqt.bat)
    file (WRITE ${fn}
        "set PATH=\"${qtBins}\";%PATH%
        \"${winDepCmd}\" -opengl --no-compiler-runtime --no-translations \"${DENG_VS_STAGING_DIR}/%1/bin/doomsday.exe\""
    )
    set (_intDir ${CMAKE_CFG_INTDIR})
    if (_intDir STREQUAL ".")
        set (_intDir ${CMAKE_BUILD_TYPE})
    endif ()
    add_custom_command (TARGET client POST_BUILD COMMAND ${fn} ${_intDir})
endif ()

deng_codesign (client)
deng_cotire (client include/precompiled.h)
