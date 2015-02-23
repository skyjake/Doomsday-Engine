find_package (OpenGL REQUIRED)

if (NOT TARGET opengl)
    find_file (OPENGL_GLEXT_H GL/glext.h
        HINTS ${OPENGL_DIR}
        PATH_SUFFIXES .. OpenGL include OpenGL/include
    )
    mark_as_advanced (OPENGL_GLEXT_H)
    if (NOT OPENGL_GLEXT_H)
        message (FATAL_ERROR "OpenGL \"GL/glext.h\" header not found. Set the OPENGL_DIR variable to specify the location of the OpenGL headers.")
    endif ()
    
    get_filename_component (glIncDir ${OPENGL_GLEXT_H} DIRECTORY)    

    add_library (opengl INTERFACE)
    target_include_directories (opengl INTERFACE ${OPENGL_INCLUDE_DIR} ${glIncDir}/..)
    target_link_libraries (opengl INTERFACE ${OPENGL_LIBRARIES})
endif ()
