## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## Dehacked Reader Plugin  ###############################

FILE(GLOB DEHREAD_SOURCES src/*.c)
ADD_LIBRARY (dpdehread SHARED ${DEHREAD_SOURCES})
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dpdehread
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
