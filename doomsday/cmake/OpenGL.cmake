find_package (OpenGL REQUIRED)

if (NOT TARGET opengl)
    add_library (opengl INTERFACE)

    if (NOT OPENGL_INCLUDE_DIR)
        # Let's look manually.
        find_file (OPENGL_GLEXT_H GL/glext.h
            PATHS ${OPENGL_DIR}
            HINTS ${OPENGL_LIBRARIES} ENV DENG_DEPEND_PATH
            PATH_SUFFIXES .. OpenGL include OpenGL/include Headers
        )
        mark_as_advanced (OPENGL_GLEXT_H)
        if (NOT OPENGL_GLEXT_H)
            message (FATAL_ERROR "OpenGL \"GL/glext.h\" header not found. Set the OPENGL_DIR variable to specify the location of the OpenGL headers.")
        endif ()
    
        get_filename_component (glIncDir ${OPENGL_GLEXT_H} DIRECTORY)    
        target_include_directories (opengl INTERFACE ${glIncDir}/..)
    else ()
        target_include_directories (opengl INTERFACE ${OPENGL_INCLUDE_DIR})
    endif ()

    target_link_libraries (opengl INTERFACE ${OPENGL_LIBRARIES})
endif ()
