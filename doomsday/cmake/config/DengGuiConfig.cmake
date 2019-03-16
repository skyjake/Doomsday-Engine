find_package (Qt5 COMPONENTS OpenGL OpenGLExtensions REQUIRED)
find_package (DengCore REQUIRED)

# Deng::libgui may exist in the current build, in which case using
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libgui)
    include ("${DENG_SDK_DIR}/lib/cmake/DengGui/DengGui.cmake")
endif ()

list (APPEND DENG_REQUIRED_PACKAGES net.dengine.stdlib.gui)

if (DENG_OPENGL_API STREQUAL "3.3")
    add_definitions (-DDENG_OPENGL=330)
elseif (DENG_OPENGL_API STREQUAL "GLES3")
    add_definitions (-DDENG_OPENGL_ES=30)
elseif (DENG_OPENGL_API STREQUAL "GLES2")
    add_definitions (-DDENG_OPENGL_ES=20)
else ()
    message (FATAL_ERROR "Invalid value for OpenGL API: ${DENG_OPENGL_API}")
endif ()
