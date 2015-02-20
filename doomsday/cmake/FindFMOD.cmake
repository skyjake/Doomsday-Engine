set (FMOD_DIR "" CACHE PATH "Location of the FMOD Programmer's API SDK")

set (_oldPath ${FMOD_FMOD_H})

find_file (FMOD_FMOD_H api/inc/fmod.h
    PATHS 
        "${FMOD_DIR}"
        "${FMOD_DIR}/FMOD Programmers API"
    NO_DEFAULT_PATH
)
mark_as_advanced (FMOD_FMOD_H)

if (NOT _oldPath STREQUAL FMOD_FMOD_H)
    if (FMOD_FMOD_H)
        message (STATUS "Looking for FMOD Ex - found")
    else ()
        message (STATUS "Looking for FMOD Ex - not found")
    endif ()
endif ()

if (NOT FMOD_FMOD_H STREQUAL "FMOD_FMOD_H-NOTFOUND" AND NOT TARGET fmodex)
    get_filename_component (fmodInc "${FMOD_FMOD_H}" DIRECTORY)
    get_filename_component (fmodApi "${fmodInc}" DIRECTORY)

    add_library (fmodex INTERFACE)
    target_include_directories (fmodex INTERFACE ${fmodInc})
    if (APPLE)
        target_link_libraries (fmodex INTERFACE ${fmodApi}/lib/libfmodex.dylib)
    endif ()
endif ()
