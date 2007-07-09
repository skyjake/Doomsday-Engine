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
######################## OpenAL Output Plugin  #################################
FILE(GLOB OPENAL_SOURCES src/*.c)
ADD_LIBRARY (dsopenal SHARED ${OPENAL_SOURCES})

TARGET_LINK_LIBRARIES (dsopenal ${OPENAL_LIBRARY} )

IF (APPLE)
	SET_TARGET_PROPERTIES (dsopenal PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} -bundle -bundle_loader doomsday")
ENDIF (APPLE)

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dsopenal
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
