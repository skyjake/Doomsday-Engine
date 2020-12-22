# This ensures that the old-style PK3 packages are compiled when CMake is run
# on any CMake project. All of these should be converted to Doomsday 2 packages.

if (NOT DE_ENABLE_PK3S OR TARGET doomsday.pk3)
    return ()
endif ()

set (outDir "${CMAKE_CURRENT_BINARY_DIR}")

execute_process (COMMAND ${PYTHON_EXECUTABLE}
    "${DE_SOURCE_DIR}/build/scripts/packres.py"
    "${outDir}"
    WORKING_DIRECTORY "${DE_SOURCE_DIR}/build/scripts"
    OUTPUT_VARIABLE msg
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message (STATUS "Compiling legacy PK3s...")

macro (deng_add_pk3 target)
    add_custom_target (${target})
    set_target_properties (${target} PROPERTIES
        DE_LOCATION ${outDir}/${target}
        FOLDER Packages
    )
    if (NOT APPLE)
        install (FILES ${outDir}/${target} DESTINATION ${DE_INSTALL_DATA_DIR})
    endif ()
    if (MSVC)
        # In addition to installing, copy the packages to the build products
        # directories so that executables can be run in them.
        foreach (cfg ${DE_CONFIGURATION_TYPES})
            file (MAKE_DIRECTORY ${DE_VS_STAGING_DIR}/${cfg}/data)
            file (COPY ${outDir}/${outName} DESTINATION ${DE_VS_STAGING_DIR}/${cfg}/data)
        endforeach (cfg)
    endif ()
endmacro (deng_add_pk3)

deng_add_pk3 (doomsday.pk3)
deng_add_pk3 (libdoom.pk3)
deng_add_pk3 (libheretic.pk3)
deng_add_pk3 (libhexen.pk3)
deng_add_pk3 (libdoom64.pk3)
