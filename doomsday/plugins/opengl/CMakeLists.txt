## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################



################################################################################
######################## OpenGL Renderer Plugin ################################

ADD_DEFINITIONS (-D__DROPENGL__ )
ADD_DEFINITIONS (-DUSE_MULTITEXTURE )

INCLUDE_DIRECTORIES ( portable/include )
FILE(GLOB OPENGL_PORTABLE_SOURCES portable/src/*.c)
SET (OPENGL_SOURCES ${OPENGL_PORTABLE_SOURCES} )

IF (UNIX)
FILE(GLOB OPENGL_UNIX_SOURCES unix/src/*.c)
SET (OPENGL_SOURCES ${OPENGL_SOURCES} ${OPENGL_UNIX_SOURCES} )
ENDIF (UNIX)

IF (WIN32)
SET (OPENGL_WIN32_SOURCES win32/src/main.c)
SET (OPENGL_SOURCES ${OPENGL_SOURCES} ${OPENGL_WIN32_SOURCES} )
ENDIF (WIN32)

SET (OPENGL_SOURCES ${OPENGL_SOURCES})
ADD_LIBRARY (dropengl SHARED ${OPENGL_SOURCES})

TARGET_LINK_LIBRARIES (dropengl ${OPENGL_LIBRARIES})
IF (UNIX)
IF (NOT WIN32)
TARGET_LINK_LIBRARIES (dropengl  ${SDL_LIBRARIES} )
ENDIF (NOT WIN32)
ENDIF (UNIX)

IF (WIN32)
TARGET_LINK_LIBRARIES (dropengl deng)
ENDIF (WIN32)
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dropengl
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
