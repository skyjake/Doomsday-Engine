## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################


################################################################################
######################## SDLMixer Output Plugin  ###############################
INCLUDE_DIRECTORIES ( portable/include )
IF (NOT APPLE)
FILE(GLOB SDLMIXER_PORTABLE_SOURCES portable/src/*.c)
ELSE (NOT APPLE)
SET (SDLMIXER_SOURCES portable/src/driver_sdlmixer.c
portable/src/mus2midi.c )
ENDIF (NOT APPLE)
SET (SDLMIXER_SOURCES ${SDLMIXER_PORTABLE_SOURCES} )

IF (APPLE)
FILE(GLOB SDLMIXER_APPLE_SOURCES mac/src/*.c)
SET (SDLMIXER_SOURCES ${SDLMIXER_SOURCES} ${SDLMIXER_APPLE_SOURCES} )
ENDIF (APPLE)

ADD_LIBRARY (dssdlmixer SHARED ${SDLMIXER_SOURCES})
TARGET_LINK_LIBRARIES (dssdlmixer ${SDL_LIBRARY} ${SDLMIXER_LIBRARY})
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS dssdlmixer
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
