## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/plugins/common/src/*.c)

################################################################################
######################## jDoom Game Plugin  ####################################

ADD_DEFINITIONS (-D__JDOOM__ )

INCLUDE_DIRECTORIES ( include )

FILE(GLOB JDOOM_SOURCES src/*.c)

ADD_LIBRARY (jdoom SHARED ${JDOOM_SOURCES} ${COMMON_PLUGIN_SOURCES} )
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS jdoom
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
