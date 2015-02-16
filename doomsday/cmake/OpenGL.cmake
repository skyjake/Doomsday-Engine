find_package (OpenGL REQUIRED)

include_directories (${OPENGL_INCLUDE_DIR})

macro (target_link_opengl target)
    target_link_libraries (${target} PRIVATE ${OPENGL_LIBRARIES})
endmacro (target_link_opengl)

#add_library (OpenGL::OpenGL UNKNOWN IMPORTED)
#set_target_properties (OpenGL::OpenGL PROPERTIES
#    INTERFACE_INCLUDE_DIRECTORIES "${OPENGL_INCLUDE_DIR}"
#    INTERFACE_LINK_LIBRARIES "${OPENGL_LIBRARIES}"
#)
