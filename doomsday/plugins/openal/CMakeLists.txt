## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## OpenAL Output Plugin  #################################
FILE(GLOB OPENAL_SOURCES src/*.c)
ADD_LIBRARY (dsopenal SHARED ${OPENAL_SOURCES})

## We should link to openal here. FIXME

################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dsopenal
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
