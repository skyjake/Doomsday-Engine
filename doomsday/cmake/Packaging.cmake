set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Doomsday Engine")
set (CPACK_PACKAGE_VENDOR "Jaakko Keränen (skyjake)")
set (CPACK_PACKAGE_VERSION_MAJOR ${DENG_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${DENG_VERSION_MINOR})
if (DENG_BUILD AND NOT DENG_STABLE)
    set (CPACK_PACKAGE_VERSION_PATCH "${DENG_VERSION_PATCH}-build${DENG_BUILD}")
else ()
    set (CPACK_PACKAGE_VERSION_PATCH ${DENG_VERSION_PATCH})
endif ()
set (CPACK_PACKAGE_INSTALL_DIRECTORY "Doomsday ${DENG_VERSION}")

set (CPACK_PACKAGE_EXECUTABLES "client;Doomsday ${DENG_VERSION};shell;Doomsday Shell ${DENG_VERSION}")

if (APPLE)
    set (CPACK_GENERATOR DragNDrop)
    set (CPACK_DMG_FORMAT UDZO)
elseif (UNIX)
    set (CPACK_GENERATOR TGZ)
else ()
    set (CPACK_GENERATOR ZIP)
endif ()

# Install types.

# Source packaging.
set (CPACK_SOURCE_GENERATOR TXZ;ZIP)
set (CPACK_SOURCE_IGNORE_FILES "\\\\.DS_Store$;\\\\.user$;\\\\.user\\\\.")

if (NOT CPack_CMake_INCLUDED)
    include (CPack)

    cpack_add_component (packs
        DISPLAY_NAME "Required Resources"
        HIDDEN        
    )
    cpack_add_component (libs
        DISPLAY_NAME "Doomsday 2 Libraries"
        INSTALL_TYPES gui
    )
    cpack_add_component (client
        DISPLAY_NAME "Doomsday Engine and Plugins"
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
        DISPLAY_NAME "Doomsday 2 SDK"
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
    
    cpack_add_install_type (gui DISPLAY_NAME "Normal")
    cpack_add_install_type (sdk DISPLAY_NAME "Developer")
endif ()
