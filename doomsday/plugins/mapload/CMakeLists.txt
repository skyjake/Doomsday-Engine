## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## Maploader Plugin  #####################################

ADD_DEFINITIONS (-DGLBSP_PLUGIN )
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/glbsp/include )

FILE(GLOB MAPLOAD_SOURCES src/*.c)
FILE(GLOB GLBSP_SOURCES ${CMAKE_SOURCE_DIR}/external/glbsp/src/*.c)

ADD_LIBRARY (dpmapload SHARED ${MAPLOAD_SOURCES} ${GLBSP_SOURCES})

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dpmapload
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
