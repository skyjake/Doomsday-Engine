find_package (OpenGL REQUIRED)

if (NOT TARGET opengl)
    find_file (OPENGL_GLEXT_H GL/glext.h
        HINTS ${OPENGL_DIR}
        PATH_SUFFIXES .. OpenGL include OpenGL/include
    )
    if (NOT OPENGL_GLEXT_H)
        message (FATAL_ERROR "OpenGL \"GL/glext.h\" header not found.")
    endif ()
    
    get_filename_component (glIncDir ${OPENGL_GLEXT_H} DIRECTORY)    

    add_library (opengl INTERFACE)
    target_include_directories (opengl INTERFACE ${OPENGL_INCLUDE_DIR} ${glIncDir}/..)
    target_link_libraries (opengl INTERFACE ${OPENGL_LIBRARIES})
endif ()
