# Basic package metadata.
set (CPACK_PACKAGE_NAME doomsday)
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Doomsday Engine")
set (CPACK_PACKAGE_VENDOR "Jaakko Keränen (skyjake)")
set (CPACK_PACKAGE_VERSION_MAJOR ${DENG_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${DENG_VERSION_MINOR})

set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Doom/Heretic/Hexen port with enhanced graphics")
set (CPACK_PACKAGE_DESCRIPTION_FILE ${DENG_SOURCE_DIR}/../distrib/description.txt)

set (CPACK_DEBIAN_PACKAGE_MAINTAINER "Jaakko Keränen (skyjake) <jaakko.keranen@iki.fi>")
set (CPACK_DEBIAN_PACKAGE_SECTION universe/games)

set (CPACK_RPM_PACKAGE_SUMMARY ${summary})
set (CPACK_RPM_PACKAGE_LICENSE "GPL3, LGPL3")
set (CPACK_RPM_PACKAGE_GROUP Amusements/Games)
set (CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/share/man /usr/share/man/man6)
set (CPACK_RPM_PACKAGE_REQUIRES "qt5-qtx11extras >= 5.2")

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
    set (CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}_${DENG_VERSION}-build${DENG_BUILD}_${DENG_ARCH})
else ()
    set (CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}_${DENG_VERSION}_${DENG_ARCH})
endif ()

# File formats.
if (APPLE)
    set (CPACK_GENERATOR DragNDrop)
    set (CPACK_DMG_FORMAT UDZO)
elseif (UNIX)
    #set (CPACK_GENERATOR RPM;DEB)
    # Set CPACK_GENERATOR manually.	    
    set (CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/PackagingUnix.cmake)
else ()
    set (CPACK_GENERATOR WIX)
    set (CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/WIX.cmake)
endif ()

# Source packaging.
set (CPACK_SOURCE_GENERATOR TGZ)
set (CPACK_SOURCE_IGNORE_FILES "\\\\.DS_Store$;\\\\.user$;\\\\.user\\\\.")

# Targets and components.
set (CPACK_PACKAGE_EXECUTABLES "client;Doomsday ${DENG_VERSION};shell;Doomsday Shell ${DENG_VERSION}")
set (CPACK_PACKAGE_INSTALL_DIRECTORY "Doomsday ${DENG_VERSION}")

if (NOT CPack_CMake_INCLUDED)
    include (CPack)

    cpack_add_component (packs
        DISPLAY_NAME "Required Resources"
        HIDDEN        
    )
    cpack_add_component (libs
        DISPLAY_NAME "Runtime Libraries"
        REQUIRED
        INSTALL_TYPES gui
    )
    cpack_add_component (client
        DISPLAY_NAME "Engine and Plugins"
        DESCRIPTION "The client and server executables plus game, audio, and other plugins."
        DEPENDS packs libs
        INSTALL_TYPES gui        
    )
    cpack_add_component (fmod
        DISPLAY_NAME "FMOD Ex Audio Plugin"
        DESCRIPTION "Audio plugin supporting 3D effects and SF2 soundfonts (non-GPL)."
        DEPENDS client
        INSTALL_TYPES gui        
    )
    cpack_add_component (sdk
        DISPLAY_NAME "SDK"
        DESCRIPTION "C++ headers and build configuration files for Doomsday 2."
        DEPENDS libs packs
        INSTALL_TYPES sdk
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
