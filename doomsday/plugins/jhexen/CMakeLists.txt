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

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/*.c ${CMAKE_SOURCE_DIR}/plugins/common/include/*.h )

################################################################################
######################## External LZSS Files ###################################
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/lzss/portable/include )

IF (UNIX)
IF (NOT WIN32)
FILE(GLOB LZSS_SOURCES ${CMAKE_SOURCE_DIR}/external/lzss/unix/src/*.c)
ENDIF (NOT WIN32)
ENDIF (UNIX)

IF (WIN32)
SET (LZSS_SOURCES "")
ENDIF (WIN32)
################################################################################
######################## jHexen Game Plugin  ####################################

ADD_DEFINITIONS (-D__JHEXEN__ )

INCLUDE_DIRECTORIES ( include )

IF (ENABLEMAO)
	SET (JHEXEN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/jhexen/src/a_action.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/h2_actn.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/hrefresh.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/m_misc.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_ceilng.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_floor.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_maputl.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_plats.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_spec.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_things.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/s_sound.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/tables.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/acfnlink.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/in_lude.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_acs.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_doors.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_inter.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_pspr.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_switch.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/sc_man.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/st_stuff.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/x_api.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/g_ctrl.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/hconsole.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/m_cheat.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_anim.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_enemy.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_lights.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/po_man.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_setup.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/sn_sonix.c )
ELSE (ENABLEMAO)
	FILE(GLOB JHEXEN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/jhexen/src/*.c ${CMAKE_SOURCE_DIR}/plugins/jhexen/include/*.h )
ENDIF (ENABLEMAO)

IF (ENABLEMAO)
	CREATE_MODULE_AT_ONCE_FILE (${CMAKE_BINARY_DIR}/_jhexen_mao.c ${JHEXEN_SOURCES} )
	CREATE_MODULE_AT_ONCE_FILE (${CMAKE_BINARY_DIR}/_jhexen_common.c ${COMMON_PLUGIN_SOURCES} )	
	CREATE_MODULE_AT_ONCE_FILE (${CMAKE_BINARY_DIR}/_jhexen_mobj.c ${CMAKE_SOURCE_DIR}/plugins/jhexen/src/h2_main.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_mobj.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/p_telept.c
	${CMAKE_SOURCE_DIR}/plugins/jhexen/src/sv_save.c )
	ADD_LIBRARY (jhexen SHARED ${CMAKE_BINARY_DIR}/_jhexen_common.c
	${CMAKE_BINARY_DIR}/_jhexen_mao.c  ${CMAKE_BINARY_DIR}/_jhexen_mobj.c ${LZSS_SOURCES})
ELSE (ENABLEMAO)
	ADD_LIBRARY (jhexen SHARED ${JHEXEN_SOURCES} ${LZSS_SOURCES} ${COMMON_PLUGIN_SOURCES} )
ENDIF (ENABLEMAO)

IF (WIN32)
TARGET_LINK_LIBRARIES (jhexen ${CMAKE_SOURCE_DIR}/external/lzss/win32/lzss)
ENDIF (WIN32)

#IF (APPLE)
#	SET_TARGET_PROPERTIES (jhexen PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} -bundle -bundle_loader doomsday")
#ENDIF (APPLE)
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jhexen
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )

IF (APPLE)
    # Bundling.
    SET (BUNDLE_BASE jHexen)
    SET (BUNDLE ${CMAKE_BINARY_DIR}/${BUNDLE_BASE}.bundle)
    ADD_CUSTOM_COMMAND( TARGET jhexen POST_BUILD
        COMMAND rm -rf ${BUNDLE}
        COMMAND mkdir -p ${BUNDLE}/Contents/MacOS
        COMMAND cp libjhexen.dylib ${BUNDLE}/Contents/MacOS/${BUNDLE_BASE}
        COMMAND cp ${CMAKE_SOURCE_DIR}/build/mac/${BUNDLE_BASE}-Info.plist ${BUNDLE}/Contents/Info.plist
        COMMAND mkdir -p ${BUNDLE}/Contents/Resources
        #COMMAND mkdir -p ${BUNDLE}/Contents/Resources/English.lproj
        #COMMAND cp ${CMAKE_SOURCE_DIR}/build/mac/InfoPlist.strings ${BUNDLE}/Contents/Resources/English.lproj/
        COMMAND echo 'APPL????' > ${BUNDLE}/Contents/PkgInfo
        COMMAND cp ${CMAKE_BINARY_DIR}/${BUNDLE_BASE}.pk3 ${BUNDLE}/Contents/Resources/
        )
ENDIF (APPLE)
