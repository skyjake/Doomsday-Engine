## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES p${CMAKE_SOURCE_DIR}/plugins/common/src/*.c)

################################################################################
######################## External LZSS Files ###################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/lzss/portable/include )
FILE(GLOB LZSS_SOURCES ${CMAKE_SOURCE_DIR}/external/lzss/unix/src/*.c)

################################################################################
######################## jHexen Game Plugin  ####################################

ADD_DEFINITIONS (-D__JHEXEN__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JHEXEN_SOURCES src/*.c)

ADD_LIBRARY (jhexen SHARED ${JHEXEN_SOURCES} ${LZSS_SOURCES}
${COMMON_PLUGIN_SOURCES} )

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jhexen
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
