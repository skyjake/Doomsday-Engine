## deng build scripts. 
## Copyright © 2006 - 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
## This file is licensed under the GNU GPLv3 or any later versions,
##
## the deng build scripts is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## the deng build scripts is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with the deng build scripts; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, 
## Boston, MA  02110-1301  USA
##
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


IF (ENABLEMAO)
	CREATE_MODULE_AT_ONCE_FILE (${CMAKE_BINARY_DIR}/_opengl_mao.c ${OPENGL_SOURCES} )
	ADD_LIBRARY (dropengl SHARED ${CMAKE_BINARY_DIR}/_opengl_mao.c)
ELSE (ENABLEMAO)
	ADD_LIBRARY (dropengl SHARED ${OPENGL_SOURCES})
ENDIF (ENABLEMAO)

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
