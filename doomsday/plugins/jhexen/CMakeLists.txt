## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/*.c)

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

FILE(GLOB JHEXEN_SOURCES src/*.c)

ADD_LIBRARY (jhexen SHARED ${JHEXEN_SOURCES} ${LZSS_SOURCES}
${COMMON_PLUGIN_SOURCES} )

IF (WIN32)
TARGET_LINK_LIBRARIES (jhexen ${CMAKE_SOURCE_DIR}/external/lzss/win32/lzss)
ENDIF (WIN32)

## Generate .bundle suffixes on OSX rather then .dylib for skyjake.
## Isn't this a nasty hack ?
IF (APPLE)
SET_TARGET_PROPERTIES (jhexen
			PROPERTIES SUFFIX ".bundle")
ENDIF (APPLE)
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jhexen
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
