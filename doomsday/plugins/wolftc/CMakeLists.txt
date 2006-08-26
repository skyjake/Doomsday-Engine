## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/*.c)

################################################################################
######################## WolfTC Game Plugin  ###################################

ADD_DEFINITIONS (-D__WOLFTC__ )
ADD_DEFINITIONS (-D__JDOOM__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB WOLFTC_SOURCES src/*.c)

ADD_LIBRARY (wolftc SHARED ${WOLFTC_SOURCES} ${COMMON_PLUGIN_SOURCES} )
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS wolftc
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
