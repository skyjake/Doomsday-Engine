find_package (PkgConfig)

pkg_check_modules (ASSIMP REQUIRED assimp)
find_library (LIBASSIMP ${ASSIMP_LIBRARIES} HINTS ${ASSIMP_LIBRARY_DIRS})
mark_as_advanced (LIBASSIMP)

if (NOT TARGET assimp)
    add_library (assimp INTERFACE)
    target_include_directories (assimp INTERFACE ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries (assimp INTERFACE ${LIBASSIMP})
endif ()
