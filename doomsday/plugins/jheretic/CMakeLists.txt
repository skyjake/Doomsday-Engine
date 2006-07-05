## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/*.c)

################################################################################
######################## jHeretic Game Plugin  ####################################

ADD_DEFINITIONS (-D__JHERETIC__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JHERETIC_SOURCES src/*.c)

ADD_LIBRARY (jheretic SHARED ${JHERETIC_SOURCES} ${COMMON_PLUGIN_SOURCES} )

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jheretic
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
