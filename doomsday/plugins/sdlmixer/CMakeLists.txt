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
######################## SDLMixer Output Plugin  ###############################
INCLUDE_DIRECTORIES ( portable/include )
IF (NOT APPLE)
FILE(GLOB SDLMIXER_PORTABLE_SOURCES portable/src/*.c)
ELSE (NOT APPLE)
SET (SDLMIXER_SOURCES portable/src/driver_sdlmixer.c
portable/src/mus2midi.c )
ENDIF (NOT APPLE)
SET (SDLMIXER_SOURCES ${SDLMIXER_PORTABLE_SOURCES} )

IF (APPLE)
FILE(GLOB SDLMIXER_APPLE_SOURCES mac/src/*.c)
SET (SDLMIXER_SOURCES ${SDLMIXER_SOURCES} ${SDLMIXER_APPLE_SOURCES} )
ENDIF (APPLE)

ADD_LIBRARY (dssdlmixer SHARED ${SDLMIXER_SOURCES})
TARGET_LINK_LIBRARIES (dssdlmixer ${SDL_LIBRARY} ${SDLMIXER_LIBRARY})

IF (APPLE)
	SET_TARGET_PROPERTIES (dssdlmixer PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} -bundle -bundle_loader doomsday")
ENDIF (APPLE)
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dssdlmixer
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
