find_package (PkgConfig QUIET)

# Prefer to use Assimp installed on the system.
pkg_check_modules (ASSIMP assimp)
if (ASSIMP_FOUND)
    #add_library (assimp ALIAS PkgConfig::ASSIMP)
    status (FATAL_ERROR "TODO: Add INTERFACE library target with ASSIMP libs")
else ()
    set (ASSIMP_RELEASE 4.1.0)
    message (STATUS "Open Asset Import Library ${ASSIMP_RELEASE} will be downloaded and built")
    include (ExternalProject)
    set (assimpOpts
        -Wno-dev
        -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
        -DASSIMP_BUILD_TESTS=OFF)
    if (MSVC)
        # Don't bother with multiconfig, always use the release build.
        list (APPEND assimpOpts -DCMAKE_BUILD_TYPE=Release)
    endif ()
    set (_allAssimpFormats
        3DS AC ASE ASSBIN ASSXML B3D BVH COLLADA DXF CSM HMP IRR LWO LWS MD2 MD3 MD5
        MDC MDL NFF NDO OFF OBJ OGRE OPENGEX PLY MS3D COB BLEND IFC XGL FBX Q3D Q3BSP RAW SMD
        STL TERRAGEN 3D X)
    set (_enabledAssimpFormats 3DS COLLADA MD2 MD3 MD5 MDL OBJ BLEND FBX IRR)
    foreach (_fmt ${_allAssimpFormats})
        list (FIND _enabledAssimpFormats ${_fmt} _pos)
        if (_pos GREATER -1)
            set (_enabled YES)
        else ()
            set (_enabled NO)
        endif ()
        list (APPEND assimpOpts -DASSIMP_BUILD_${_fmt}_IMPORTER=${_enabled})
    endforeach (_fmt)
    set (assimpLibName libassimp.4.dylib)
    ExternalProject_Add (github-assimp
        GIT_REPOSITORY   https://github.com/assimp/assimp.git
        GIT_TAG          v${ASSIMP_RELEASE}
        CMAKE_ARGS       ${assimpOpts}
        PREFIX           assimp
        BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/assimp/src/github-assimp-build/code/${assimpLibName}
        INSTALL_COMMAND  ""
    )
    add_library (assimp INTERFACE)
    target_include_directories (assimp INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/assimp/src/github-assimp/include
        ${CMAKE_CURRENT_BINARY_DIR}/assimp/src/github-assimp-build/include)
    target_link_libraries (assimp INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/assimp/src/github-assimp-build/code/${assimpLibName})
    add_dependencies (assimp github-assimp)
endif ()

if (TARGET assimp)
    set (ASSIMP_TARGET assimp)
endif ()

#if (assimp_FOUND)
#    message (STATUS "Using libassimp on the system")
#endif ()

#set (_oldPath ${LIBASSIMP})

#if (NOT TARGET libassimp)
#    add_library (libassimp INTERFACE)

#    # It is a bit unorthodox to access the build products of an external project via
#    # the CMake target object. The current setup exists because the "libassimp"
#    # interface target can either represent an Assimp installation somewhere in the
#    # file system, or the build products of the Assimp that gets built as part of
#    # Doomsday.

#    if (TARGET assimp)
#        # Assimp is built as a subdir.
#        # Use the built target location from the "assimp" target.
#        set (ASSIMP_INCLUDE_DIRS ${DE_EXTERNAL_SOURCE_DIR}/assimp/include)
#        set (LIBASSIMP $<TARGET_LINKER_FILE:assimp>)
#        if (APPLE)
#            # The assimp library will be bundled into Doomsday.app. This will
#            # inform the installer to include the real library in addition to
#            # version symlinks (CMake bug?).
#            # (see: deng_install_bundle_deps() in Macros.cmake)
#            target_link_libraries (libassimp INTERFACE $<TARGET_FILE:assimp>;$<TARGET_LINKER_FILE:assimp>)
#        elseif (UNIX)
#            get_property (depLibs TARGET assimp PROPERTY LINK_LIBRARIES)
#            target_link_libraries (libassimp INTERFACE ${depLibs})
#            set (depLibs)
#        endif ()
#    else ()
#        # Try to find assimp using pkg-config.
#        if (PKG_CONFIG_FOUND AND NOT DEFINED ASSIMP_DIR)
#            pkg_check_modules (ASSIMP QUIET assimp)
#            if (NOT ASSIMP_LIBRARIES)
#                set (ASSIMP_LIBRARIES assimp)
#            endif ()
#            find_library (LIBASSIMP ${ASSIMP_LIBRARIES}
#                PATHS
#                    ${ASSIMP_LIBRARY_DIRS}
#                    /usr/local/lib
#            )
#        endif ()
#        if (NOT LIBASSIMP)
#            # Try to find assimp manually.
#            find_library (LIBASSIMP NAMES assimp assimpd
#                PATHS
#                    ${DE_EXTERNAL_SOURCE_DIR}/assimp
#                    ${ASSIMP_DIR}
#                PATH_SUFFIXES lib/Release lib/Debug lib
#                NO_DEFAULT_PATH
#            )
#        endif ()
#    endif ()
#    mark_as_advanced (LIBASSIMP)

#    if (NOT LIBASSIMP)
#        message (FATAL_ERROR "Open Asset Import Library not found. Go to ${DE_EXTERNAL_SOURCE_DIR}/assimp and compile. If you install it somewhere, set the ASSIMP_DIR variable to specify the location. If pkg-config is available, it is used to find libassimp.")
#    endif ()

#    if (ASSIMP_INCLUDE_DIRS)
#        target_include_directories (libassimp INTERFACE ${ASSIMP_INCLUDE_DIRS})
#    else ()
#        # Try to deduce include dir from the library location.
#        get_filename_component (_assimpBase ${LIBASSIMP} DIRECTORY)
#        find_file (LIBASSIMP_IMPORTER_HPP
#            assimp/Importer.hpp
#            HINTS /usr/include /usr/local/include
#                ${_assimpBase}/include
#                ${_assimpBase}/../include
#                ${_assimpBase}/../../include
#        )
#        mark_as_advanced (LIBASSIMP_IMPORTER_HPP)
#        if (NOT LIBASSIMP_IMPORTER_HPP)
#            message (FATAL_ERROR "Could not find Open Asset Import Library headers.\n")
#        endif ()
#        get_filename_component (_incDir ${LIBASSIMP_IMPORTER_HPP} DIRECTORY)
#        get_filename_component (_incDir ${_incDir} DIRECTORY)
#        target_include_directories (libassimp INTERFACE ${_incDir})
#    endif ()

#    target_link_libraries (libassimp INTERFACE ${LIBASSIMP})
#endif ()

#if (NOT _oldPath STREQUAL LIBASSIMP)
#    message (STATUS "Found Open Asset Import Library: ${LIBASSIMP}")
#endif ()
