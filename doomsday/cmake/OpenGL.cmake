find_package (OpenGL REQUIRED)

if (NOT TARGET opengl)
    add_library (opengl INTERFACE)
    target_include_directories (opengl INTERFACE ${OPENGL_INCLUDE_DIR})
    target_link_libraries (opengl INTERFACE ${OPENGL_LIBRARIES})
endif ()
