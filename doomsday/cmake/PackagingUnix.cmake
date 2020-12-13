# CPack: Generator-specific configuration for Unix
if (CPACK_GENERATOR STREQUAL DEB)
    string (REPLACE x86_64 amd64 CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})

elseif (CPACK_GENERATOR STREQUAL RPM)
    string (REPLACE _x86_64 -1.x86_64 CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
    string (REPLACE _i386 -1.i686 CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
    string (REPLACE y_ y- CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})

    # Exclude system directories; they are not owned by the Doomsday RPM package.
    list (APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        "/usr/share/icons"
        "/usr/share/icons/hicolor"
        "/usr/share/icons/hicolor/256x256"
        "/usr/share/icons/hicolor/256x256/apps"
        "/usr/lib64/cmake"
        "/usr/share/metainfo"
        "/usr/share/applications"
    )
endif ()
