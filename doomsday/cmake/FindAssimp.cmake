find_package (PkgConfig QUIET)

set (_oldPath ${LIBASSIMP})

if (NOT TARGET libassimp)
    add_library (libassimp INTERFACE)

    # It is a bit unorthodox to access the build products of an external project via
    # the CMake target object. The current setup exists because the "libassimp"
    # interface target can either represent an Assimp installation somewhere in the
    # file system, or the build products of the Assimp that gets built as part of
    # Doomsday.

    if (TARGET assimp)
        # Assimp is built as a subdir.
        # Use the built target location from the "assimp" target.
        set (ASSIMP_INCLUDE_DIRS ${DE_EXTERNAL_SOURCE_DIR}/assimp/include)
        set (LIBASSIMP $<TARGET_LINKER_FILE:assimp>)
        if (APPLE)
            # The assimp library will be bundled into Doomsday.app. This will
            # inform the installer to include the real library in addition to
            # version symlinks (CMake bug?).
            # (see: deng_install_bundle_deps() in Macros.cmake)
            target_link_libraries (libassimp INTERFACE $<TARGET_FILE:assimp>;$<TARGET_LINKER_FILE:assimp>)
        elseif (UNIX)
            get_property (depLibs TARGET assimp PROPERTY LINK_LIBRARIES)
            target_link_libraries (libassimp INTERFACE ${depLibs})
            set (depLibs)
        endif ()
    else ()
        # Try to find assimp using pkg-config.
        if (PKG_CONFIG_FOUND AND NOT DEFINED ASSIMP_DIR)
            pkg_check_modules (ASSIMP QUIET assimp)
            if (NOT ASSIMP_LIBRARIES)
                set (ASSIMP_LIBRARIES assimp)
            endif ()
            find_library (LIBASSIMP ${ASSIMP_LIBRARIES}
                PATHS
                    ${ASSIMP_LIBRARY_DIRS}
                    /usr/local/lib
            )
        endif ()
        if (NOT LIBASSIMP)
            # Try to find assimp manually.
            find_library (LIBASSIMP NAMES assimp assimpd
                PATHS
                    ${DE_EXTERNAL_SOURCE_DIR}/assimp
                    ${ASSIMP_DIR}
                PATH_SUFFIXES lib/Release lib/Debug lib
                NO_DEFAULT_PATH
            )
        endif ()
    endif ()
    mark_as_advanced (LIBASSIMP)

    if (NOT LIBASSIMP)
        message (FATAL_ERROR "Open Asset Import Library not found. Go to ${DE_EXTERNAL_SOURCE_DIR}/assimp and compile. If you install it somewhere, set the ASSIMP_DIR variable to specify the location. If pkg-config is available, it is used to find libassimp.")
    endif ()

    if (ASSIMP_INCLUDE_DIRS)
        target_include_directories (libassimp INTERFACE ${ASSIMP_INCLUDE_DIRS})
    else ()
        # Try to deduce include dir from the library location.
        get_filename_component (_assimpBase ${LIBASSIMP} DIRECTORY)
        find_file (LIBASSIMP_IMPORTER_HPP
            assimp/Importer.hpp
            HINTS /usr/include /usr/local/include
                ${_assimpBase}/include
                ${_assimpBase}/../include
                ${_assimpBase}/../../include
        )
        mark_as_advanced (LIBASSIMP_IMPORTER_HPP)
        if (NOT LIBASSIMP_IMPORTER_HPP)
            message (FATAL_ERROR "Could not find Open Asset Import Library headers.\n")
        endif ()
        get_filename_component (_incDir ${LIBASSIMP_IMPORTER_HPP} DIRECTORY)
        get_filename_component (_incDir ${_incDir} DIRECTORY)
        target_include_directories (libassimp INTERFACE ${_incDir})
    endif ()

    target_link_libraries (libassimp INTERFACE ${LIBASSIMP})
endif ()

if (NOT _oldPath STREQUAL LIBASSIMP)
    message (STATUS "Found Open Asset Import Library: ${LIBASSIMP}")
endif ()
