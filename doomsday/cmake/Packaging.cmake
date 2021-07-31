# Basic package metadata.
set (CPACK_PACKAGE_NAME doomsday)
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Doomsday Engine")
set (CPACK_PACKAGE_VENDOR "Jaakko Keränen (skyjake)")
set (CPACK_PACKAGE_VERSION_MAJOR ${DENG_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${DENG_VERSION_MINOR})

set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Doom/Heretic/Hexen port with enhanced graphics")
set (CPACK_PACKAGE_DESCRIPTION_FILE ${DENG_SOURCE_DIR}/build/description.txt)

set (CPACK_DEBIAN_PACKAGE_MAINTAINER "Jaakko Keränen (skyjake) <jaakko.keranen@iki.fi>")
set (CPACK_DEBIAN_PACKAGE_SECTION universe/games)
set (CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5gui5, libqt5x11extras5, libsdl2-mixer-2.0-0, libxrandr2, libxxf86vm1, libncurses5, libminizip1")
if (NOT DENG_FLUIDSYNTH_EMBEDDED)
    string (APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", libfluidsynth1")
endif ()
if (NOT DENG_ASSIMP_EMBEDDED)
    string (APPEND CPACK_DEBIAN_PACKAGE_DEPENDS ", libassimp4")
endif ()

set (CPACK_RPM_PACKAGE_SUMMARY ${summary})
set (CPACK_RPM_PACKAGE_LICENSE "GPL3, LGPL3")
set (CPACK_RPM_PACKAGE_GROUP Amusements/Games)
set (CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/share/man /usr/share/man/man6)
set (CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase-gui >= 5.2, qt5-qtx11extras >= 5.2, SDL2 >= 2.0, SDL2_mixer >= 2.0, libXrandr, libXxf86vm, libxcb, glib2, zlib, ncurses-libs")
if (NOT DENG_FLUIDSYNTH_EMBEDDED)
    string (APPEND CPACK_RPM_PACKAGE_REQUIRES ", fluidsynth-libs")
endif ()
if (NOT DENG_ASSIMP_EMBEDDED)
    string (APPEND CPACK_RPM_PACKAGE_REQUIRES ", assimp")
endif ()
set (CPACK_RPM_PACKAGE_AUTOREQ NO)

set (CPACK_WIX_LICENSE_RTF ${DENG_SOURCE_DIR}/build/win32/license.rtf)
set (CPACK_WIX_PRODUCT_ICON ${DENG_SOURCE_DIR}/apps/client/res/windows/doomsday.ico)

if (DENG_BUILD)
    # Whenever the build number is specified, include it in the package version.
    # This ensures newer builds of the same version will have a greater number.
    if (DENG_STABLE OR WIN32)
        set (CPACK_PACKAGE_VERSION_PATCH "${DENG_VERSION_PATCH}.${DENG_BUILD}")
    else ()
        set (CPACK_PACKAGE_VERSION_PATCH "${DENG_VERSION_PATCH}-build${DENG_BUILD}")
    endif ()
else ()
    set (CPACK_PACKAGE_VERSION_PATCH ${DENG_VERSION_PATCH})
endif ()

if (DENG_BUILD AND NOT DENG_STABLE)
    set (CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}_${DENG_VERSION}-build${DENG_BUILD}_${DENG_ARCH}${DENG_PACKAGE_SUFFIX})
else ()
    set (CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}_${DENG_VERSION}_${DENG_ARCH}${DENG_PACKAGE_SUFFIX})
endif ()

# File formats.
if (APPLE)
    set (CPACK_GENERATOR DragNDrop)
    set (CPACK_DMG_FORMAT UDZO)
    set (CPACK_DMG_BACKGROUND_IMAGE "${DENG_SOURCE_DIR}/build/macx/dmg_background.jpg")
    set (CPACK_DMG_DS_STORE_SETUP_SCRIPT "${DENG_CMAKE_DIR}/DMGSetup.scpt")
elseif (UNIX)
    #set (CPACK_GENERATOR RPM;DEB)
    # Set CPACK_GENERATOR manually.
    set (CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/PackagingUnix.cmake)
else ()
	set (CPACK_PACKAGE_NAME "Doomsday ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
    set (CPACK_GENERATOR WIX;ZIP)
	set (CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/WIX.cmake)
    set (CPACK_WIX_UI_DIALOG "${DENG_SOURCE_DIR}/build/win32/installer_dialog.png")
    set (CPACK_WIX_UI_BANNER "${DENG_SOURCE_DIR}/build/win32/installer_banner.png")
endif ()

# Source packaging.
set (CPACK_SOURCE_GENERATOR TGZ)
set (CPACK_SOURCE_IGNORE_FILES "\\\\.DS_Store$;\\\\.user$;\\\\.user\\\\.")

# Targets and components.
set (CPACK_PACKAGE_EXECUTABLES "client;Doomsday ${DENG_VERSION};shell;Doomsday Shell ${DENG_VERSION}")
set (CPACK_PACKAGE_INSTALL_DIRECTORY "Doomsday ${DENG_VERSION}")

if (NOT CPack_CMake_INCLUDED)
    # We have to define the components manually because otherwise Assimp's
    # components would get automatically included.
    set (CPACK_COMPONENTS_ALL packs libs fmod)
    if (DENG_ENABLE_GUI)
        list (APPEND CPACK_COMPONENTS_ALL client)
    endif ()
    if (DENG_ENABLE_SDK)
        list (APPEND CPACK_COMPONENTS_ALL sdk)
    endif ()
    if (DENG_ENABLE_TOOLS)
        list (APPEND CPACK_COMPONENTS_ALL tools)
    endif ()
    if (DENG_ENABLE_TESTS)
        list (APPEND CPACK_COMPONENTS_ALL tests)
    endif ()

    include (CPack)

    cpack_add_component (packs
        DISPLAY_NAME "Required Resources"
        HIDDEN
    )
    cpack_add_component (libs
        DISPLAY_NAME "Runtime Libraries"
        HIDDEN REQUIRED
        INSTALL_TYPES gui
    )
    cpack_add_component (client
        DISPLAY_NAME "Engine and Plugins"
        DESCRIPTION "The client and server executables plus game, audio, and other plugins."
        DEPENDS packs libs
        INSTALL_TYPES gui
    )
    cpack_add_component (fmod
        DISPLAY_NAME "FMOD Audio Plugin"
        DESCRIPTION "Audio plugin supporting 3D effects and SF2 soundfonts (non-GPL)."
        DEPENDS client
        INSTALL_TYPES gui
    )
    cpack_add_component (sdk
        DISPLAY_NAME "SDK"
        DESCRIPTION "C++ headers, libraries, and build configuration files to create Doomsday 2 plugins."
        DISABLED
        DEPENDS libs packs
        INSTALL_TYPES sdk
    )
    cpack_add_component (tools
        DISPLAY_NAME "Tools"
        DESCRIPTION "Command line tools."
        DEPENDS libs
        INSTALL_TYPES gui sdk
    )
    # Don't package tests.
    cpack_add_component (test
        DISPLAY_NAME "Tests"
        DESCRIPTION "Various test applications."
        DEPENDS libs packs
        INSTALL_TYPES sdk
    )

    cpack_add_install_type (gui DISPLAY_NAME "Standard")
    cpack_add_install_type (sdk DISPLAY_NAME "Developer")
endif ()
