find_package (PkgConfig QUIET)

set (_oldPath ${LIBASSIMP})

if (NOT TARGET assimp)
    if (PKG_CONFIG_FOUND)
        # Try to find assimp using pkg-config.
        pkg_check_modules (ASSIMP QUIET assimp)
        if (NOT ASSIMP_LIBRARIES)
            set (ASSIMP_LIBRARIES assimp)
        endif ()
        find_library (LIBASSIMP ${ASSIMP_LIBRARIES} 
            HINTS 
                ${ASSIMP_LIBRARY_DIRS}
                /usr/local/lib
        )
    else ()
        # Try to find assimp manually.
        find_library (LIBASSIMP assimp
            HINTS ${DENG_EXTERNAL_SOURCE_DIR}/assimp
            PATH_SUFFIXES lib/Release lib/Debug lib
        )        
    endif ()
    mark_as_advanced (LIBASSIMP)
    
    if (NOT LIBASSIMP)
        message (FATAL_ERROR "Open Asset Import Library not found. Go to doomsday/external/assimp, compile, and install.\n")
    endif ()

    add_library (assimp INTERFACE)
    
    if (ASSIMP_INCLUDE_DIRS)
        target_include_directories (assimp INTERFACE ${ASSIMP_INCLUDE_DIRS})
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
        target_include_directories (assimp INTERFACE ${_incDir})
    endif ()
    
    target_link_libraries (assimp INTERFACE ${LIBASSIMP})

    if (NOT WIN32)
        deng_install_library (${LIBASSIMP})
    else ()
        # Locate the DLL.
        find_file (LIBASSIMP_DLL assimp.dll HINTS ${_assimpBase}/..
            PATH_SUFFIXES bin ../bin/Release ../bin/Debug
        )
        get_filename_component (LIBASSIMP_DLL ${LIBASSIMP_DLL} REALPATH)
        deng_install_library (${LIBASSIMP_DLL})
    endif ()
endif ()

if (NOT _oldPath STREQUAL ${LIBASSIMP})
    message (STATUS "Found Open Asset Import Library: ${LIBASSIMP}")
endif ()

