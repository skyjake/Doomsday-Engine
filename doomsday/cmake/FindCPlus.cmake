# c_Plus provides a number of low-level C features.

set (CPLUS_DIR "" CACHE PATH "Location of the c_Plus library")

set (_oldPath ${CPLUS_DEFS_H})
find_file (CPLUS_DEFS_H include/c_plus/defs.h
    PATHS "${CPLUS_DIR}"
    HINTS ENV DENG_DEPEND_PATH
    PATH_SUFFIXES cplus c_plus
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
mark_as_advanced (CPLUS_DEFS_H)

if (NOT _oldPath STREQUAL CPLUS_DEFS_H)
    if (CPLUS_DEFS_H)
        message (STATUS "Looking for c_Plus - found")
    else ()
        message (STATUS "Looking for c_Plus - not found (set the CPLUS_DIR variable)")
    endif ()
endif ()

if (CPLUS_DEFS_H AND NOT TARGET cplus)
    get_filename_component (cplusInc "${CPLUS_DEFS_H}" DIRECTORY)
    get_filename_component (cplusInc "${cplusInc}" DIRECTORY)

    add_library (cplus INTERFACE)
    target_include_directories (cplus INTERFACE ${cplusInc})
    # target_link_libraries (cplus INTERFACE xxx)
endif ()

