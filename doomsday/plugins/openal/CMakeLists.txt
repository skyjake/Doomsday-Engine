## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## OpenAL Output Plugin  #################################
FILE(GLOB OPENAL_SOURCES src/*.c)
ADD_LIBRARY (dsopenal SHARED ${OPENAL_SOURCES})

TARGET_LINK_LIBRARIES (dsopenal ${OPENAL_LIBRARY} )

## Generate .bundle suffixes on OSX rather then .dylib for skyjake.
## Isn't this a nasty hack ?
IF (APPLE)
SET_TARGET_PROPERTIES (dsopenal
			PROPERTIES SUFFIX ".bundle")
ENDIF (APPLE)
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dsopenal
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
