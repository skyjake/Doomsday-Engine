## deng build scripts. 
## Copyright © 2006 - 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include ${CMAKE_SOURCE_DIR}/utilities/include )
FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/* ${CMAKE_SOURCE_DIR}/utilities/src/*)

################################################################################
######################## jDoom Game Plugin  ####################################

ADD_DEFINITIONS (-D__JDOOM__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JDOOM_SOURCES ${CMAKE_SOURCE_DIR}/plugins/jdoom/src/*.c )

IF (ENABLEMAO)
	CREATE_MODULE_AT_ONCE_FILE (${CMAKE_BINARY_DIR}/_jdoom_mao.c ${JDOOM_SOURCES} )
	CREATE_MODULE_AT_ONCE_FILE (${CMAKE_BINARY_DIR}/_jdoom_common.c ${COMMON_PLUGIN_SOURCES} )
#	CREATE_MODULE_AT_ONCE_FILE (${CMAKE_BINARY_DIR}/_jdoom_mobj.c
# 	${CMAKE_SOURCE_DIR}/plugins/jdoom/src/p_mobj.c ${CMAKE_SOURCE_DIR}/plugins/jdoom/src/p_telept.c )

	ADD_LIBRARY (jdoom SHARED ${CMAKE_BINARY_DIR}/_jdoom_mao.c ${CMAKE_BINARY_DIR}/_jdoom_common.c )
#	${CMAKE_SOURCE_DIR}/plugins/jdoom/src/p_oldsvg.c ${CMAKE_SOURCE_DIR}/plugins/jdoom/src/m_random.c 
#	${CMAKE_BINARY_DIR}/_jdoom_mobj.c )

ELSE (ENABLEMAO)
	ADD_LIBRARY (jdoom SHARED ${JDOOM_SOURCES} ${COMMON_PLUGIN_SOURCES} )
ENDIF (ENABLEMAO)

IF (APPLE)
	SET_TARGET_PROPERTIES (jdoom PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} -bundle -bundle_loader doomsday")
ENDIF (APPLE)

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jdoom
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
