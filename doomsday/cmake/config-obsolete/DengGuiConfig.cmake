find_package (Qt5 COMPONENTS OpenGL OpenGLExtensions REQUIRED)
find_package (DengCore REQUIRED)

# Deng::libgui may exist in the current build, in which case using
# a previously installed version is inappropriate.
if (NOT TARGET Deng::libgui)
    include ("${DENG_SDK_DIR}/lib/cmake/DengGui/DengGui.cmake")
endif ()

list (APPEND DE_REQUIRED_PACKAGES net.dengine.stdlib.gui)

if (DE_OPENGL_API STREQUAL "3.3")
    add_definitions (-DDE_OPENGL=330)
elseif (DE_OPENGL_API STREQUAL "GLES3")
    add_definitions (-DDE_OPENGL_ES=30)
elseif (DE_OPENGL_API STREQUAL "GLES2")
    add_definitions (-DDE_OPENGL_ES=20)
else ()
    message (FATAL_ERROR "Invalid value for OpenGL API: ${DE_OPENGL_API}")
endif ()
