## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## Dehacked Reader Plugin  ###############################

FILE(GLOB DEHREAD_SOURCES src/*.c)
ADD_LIBRARY (dpdehread SHARED ${DEHREAD_SOURCES})

## Generate .bundle suffixes on OSX rather then .dylib for skyjake.
## Isn't this a nasty hack ?
IF (APPLE)
SET_TARGET_PROPERTIES (dpdehread
			PROPERTIES SUFFIX ".bundle")
ENDIF (APPLE)
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dpdehread
RUNTIME DESTINATION ${bindir}
LIBRARY DESTINATION ${libdir} )
