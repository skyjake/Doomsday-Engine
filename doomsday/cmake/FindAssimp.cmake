find_package (PkgConfig)

pkg_check_modules (ASSIMP REQUIRED assimp)

include_directories (${ASSIMP_INCLUDE_DIRS})
link_directories    (${ASSIMP_LIBRARY_DIRS})

macro (link_assimp target)
    target_link_libraries (${target} PUBLIC ${ASSIMP_LIBRARIES})
    set_property (TARGET ${target} 
        PROPERTY INTERFACE_LINK_LIBRARIES "-L${ASSIMP_LIBRARY_DIRS}"
    )
endmacro (link_assimp)
