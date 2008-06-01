## deng build scripts. 
## Copyright © 2006 - 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
## Copyright © 2007 Daniel Swanson <danij@dengine.net>
##
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
######################## WAD Map Converter Plugin ##############################

INCLUDE_DIRECTORIES ( include )
FILE(GLOB WADMAPCONVERTER_SOURCES src/*.c)
ADD_LIBRARY (dpwadmapconverter SHARED ${WADMAPCONVERTER_SOURCES})

#IF (APPLE)
#    SET_TARGET_PROPERTIES (dpwadmapconverter PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} -bundle -bundle_loader doomsday")
#ENDIF (APPLE)
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dpwadmapconverter
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )

IF (APPLE)
    # Bundling.
    SET (BUNDLE_BASE dpWADMapConverter)
    SET (BUNDLE ${CMAKE_BINARY_DIR}/${BUNDLE_BASE}.bundle)
    ADD_CUSTOM_COMMAND( TARGET dpwadmapconverter POST_BUILD
        COMMAND rm -rf ${BUNDLE}
        COMMAND mkdir -p ${BUNDLE}/Contents/MacOS
        COMMAND cp libdpwadmapconverter.dylib ${BUNDLE}/Contents/MacOS/${BUNDLE_BASE}
        COMMAND cp ${CMAKE_SOURCE_DIR}/build/mac/${BUNDLE_BASE}-Info.plist ${BUNDLE}/Contents/Info.plist
        #COMMAND mkdir -p ${BUNDLE}/Contents/Resources/English.lproj
        #COMMAND cp ${CMAKE_SOURCE_DIR}/build/mac/InfoPlist.strings ${BUNDLE}/Contents/Resources/English.lproj/
        COMMAND echo 'APPL????' > ${BUNDLE}/Contents/PkgInfo
        )
ENDIF (APPLE)
