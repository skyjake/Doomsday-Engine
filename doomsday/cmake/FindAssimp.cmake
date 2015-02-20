find_package (PkgConfig)

set (_oldPath ${LIBASSIMP})

if (NOT TARGET assimp)
    pkg_check_modules (ASSIMP QUIET assimp)
    if (NOT ASSIMP_LIBRARIES)
        set (ASSIMP_LIBRARIES assimp)
    endif ()
    find_library (LIBASSIMP ${ASSIMP_LIBRARIES} 
        HINTS 
            ${ASSIMP_LIBRARY_DIRS}
            /usr/local/lib
    )
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
        get_filename_component (_assimpBase ${_assimpBase} DIRECTORY)
        find_file (LIBASSIMP_IMPORTER_HPP assimp/Importer.hpp
            HINTS /usr/include /usr/local/include
                ${_assimpBase}/include
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

    deng_install_library (${LIBASSIMP})
endif ()

if (NOT _oldPath STREQUAL ${LIBASSIMP})
    message (STATUS "Found Open Asset Import Library: ${LIBASSIMP}")
endif ()

