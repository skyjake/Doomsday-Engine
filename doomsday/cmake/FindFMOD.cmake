set (FMOD_DIR "FMOD Programmers API" CACHE PATH
    "Location of the FMOD Programmer's API SDK"
)

if (NOT FMOD_FOUND)
    get_filename_component (fullPath "${FMOD_DIR}" ABSOLUTE)
    if (EXISTS "${fullPath}/api")
        set (FMOD_FOUND YES CACHE BOOL "FMOD SDK found")
        mark_as_advanced (FMOD_FOUND)
        message (STATUS "Found FMOD: ${FMOD_DIR}")        
    endif ()
endif ()

if (FMOD_FOUND AND NOT TARGET fmodex)
    add_library (fmodex INTERFACE)
    target_include_directories (fmodex INTERFACE ${FMOD_DIR}/api/inc)
    if (APPLE)
        target_link_libraries (fmodex INTERFACE
            ${FMOD_DIR}/api/lib/libfmodex.dylib
        )
    endif ()
endif ()
