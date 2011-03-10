## deng build scripts. 
## Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
## Copyright (c) 2006 - 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/*.c
	${CMAKE_SOURCE_DIR}/plugins/common/include/*.h )

################################################################################
######################## Doom64TC Game Plugin ##################################

ADD_DEFINITIONS (-D__JDOOM64__ )
#ADD_DEFINITIONS (-D__JDOOM__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB DOOM64TC_SOURCES src/*.c include/*.h )

ADD_LIBRARY (jdoom64 SHARED ${DOOM64TC_SOURCES} ${COMMON_PLUGIN_SOURCES} )

IF (APPLE)
	SET_TARGET_PROPERTIES (jdoom64 PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} -bundle -bundle_loader doomsday")
ENDIF (APPLE)

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jdoom64
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
