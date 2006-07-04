## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################

PROJECT(deng C)

################################################################################
######################## Configure Build Options  ##############################


################################################################################
######################## Include Standard Cmake Helpers  #######################

INCLUDE (FindOpenGL)
INCLUDE (FindSDL)
INCLUDE (FindSDL_mixer)
INCLUDE (FindSDL_net)
INCLUDE (FindZLIB)
INCLUDE (FindPNG)
INCLUDE (FindCurses)
INCLUDE (FindX11)
INCLUDE (FindOpenAL)
################################################################################
######################## Find Our Build Deps  ##################################

## Confirm we have OpenGL
IF (OPENGL_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** libGL not found.
On Ubuntu install libgl1-mesa-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (OPENGL_FOUND STREQUAL "NO")

## Confirm we have GLU
IF (OPENGL_GLU_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** libGLU not found.
On Ubuntu install libglu1-mesa-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (OPENGL_GLU_FOUND STREQUAL "NO")

INCLUDE_DIRECTORIES ( ${OPENGL_INCLUDE_DIR})

## Confirm we have SDL
IF (SDL_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL not found.
On Ubuntu install libsdl1.2-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (SDL_FOUND STREQUAL "NO")

INCLUDE_DIRECTORIES ( ${SDL_INCLUDE_DIR})

## Confirm we have SDL_mixer
IF (SDLMIXER_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL Mixer not found.
On Ubuntu install libsdl-mixer1.2-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (SDLMIXER_FOUND STREQUAL "NO")

INCLUDE_DIRECTORIES ( ${SDLMIXER_INCLUDE_DIR})

## Confirm we have SDL_net
IF (SDLNET_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL Net not found.
On Ubuntu install libsdl-net1.2-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (SDLNET_FOUND STREQUAL "NO")

INCLUDE_DIRECTORIES ( ${SDLNET_INCLUDE_DIR})

## Confirm we have zlib
IF (NOT ZLIB_FOUND STREQUAL "YES")
MESSAGE (FATAL_ERROR
"** zlib not found.
On Ubuntu install zlib1g-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (NOT ZLIB_FOUND STREQUAL "YES")


## No test yet for finding libPNG. FIXME
## No test yet for finding ncurses. FIXME
## No test yet for finding X11. FIXME
## No test yet for finding openAl. FIXME

################################################################################
######################## Set Common Definitions  ###############################

ADD_DEFINITIONS (-DNORANGECHECKING)

## If x86 (32bit) we want this. FIXME
ADD_DEFINITIONS (-DGNU_X86_FIXED_ASM )
## Otherwise we want this. FIXME
##　ADD_DEFINITIONS (-DNO_FIXED_ASM )

IF (UNIX)
IF (NOT APPLE)
IF (NOT WIN32)
ADD_DEFINITIONS ( -DUNIX)
ENDIF (NOT WIN32)
ENDIF (NOT APPLE)
ENDIF (UNIX)

IF (WIN32)
ADD_DEFINITIONS ( -DWIN32)
ENDIF (WIN32)


################################################################################
######################## Plugin Common Files ###################################

INCLUDE_DIRECTORIES ( plugins/common/include )
FILE(GLOB COMMON_PLUGIN_SOURCES plugins/common/src/*.c)


################################################################################
######################## External LZSS Files ###################################

INCLUDE_DIRECTORIES ( external/lzss/portable/include )
FILE(GLOB LZSS_SOURCES external/lzss/unix/src/*.c)


################################################################################
######################## The Doomsday Engine  ##################################

ADD_DEFINITIONS (-D__DOOMSDAY__ )

## FIXME FIXME FIXME FIXME FIXME - AWFUL HACK - HIGH PRIORITY
ADD_DEFINITIONS ('-DDENG_BASE_DIR=\"/usr/local/share/deng\"')

INCLUDE_DIRECTORIES (engine/api engine/portable/include )

FILE(GLOB DENG_PORTABLE_SOURCES engine/portable/src/*.c)

SET (DENG_SOURCES ${DENG_PORTABLE_SOURCES} ${LZSS_SOURCES}
 plugins/common/src/m_fixed.c)

IF (UNIX)
IF (NOT APPLE)
IF (NOT WIN32)
INCLUDE_DIRECTORIES (engine/unix/include )
FILE(GLOB DENG_UNIX_SOURCES engine/unix/src/*.c)

SET (DENG_SOURCES ${DENG_SOURCES} ${DENG_UNIX_SOURCES} )

ENDIF (NOT WIN32)
ENDIF (NOT APPLE)
ENDIF (UNIX)

IF (WIN32)
INCLUDE_DIRECTORIES (engine/win32/include )
FILE(GLOB DENG_WIN32_SOURCES engine/win32/src/*.c)

SET (DENG_SOURCES ${DENG_SOURCES} ${DENG_WIN32_SOURCES})
ENDIF (WIN32)

## FIXME - Add OSX here

ADD_EXECUTABLE (doomsday ${DENG_SOURCES})
TARGET_LINK_LIBRARIES (doomsday ${SDL_LIBRARY} ${SDLMIXER_LIBRARY}
${SDLNET_LIBRARY} ${ZLIB_LIBRARIES} ${CURSES_LIBRARY} ${PNG_LIBRARY}
dropengl dpdehread dpmapload)

IF (UNIX)
IF (NOT APPLE)
IF (NOT WIN32)
TARGET_LINK_LIBRARIES (doomsday  ${X11_LIBRARIES})
ENDIF (NOT WIN32)
ENDIF (NOT APPLE)
ENDIF (UNIX)

################################################################################
######################## OpenGL Renderer Plugin ################################

ADD_DEFINITIONS (-D__DROPENGL__ )
ADD_DEFINITIONS (-DUSE_MULTITEXTURE )

INCLUDE_DIRECTORIES ( plugins/opengl/portable/include )

FILE(GLOB OPENGL_PORTABLE_SOURCES plugins/opengl/portable/src/*.c)

SET (OPENGL_SOURCES ${OPENGL_PORTABLE_SOURCES} )

IF (UNIX)
FILE(GLOB OPENGL_UNIX_SOURCES plugins/opengl/unix/src/*.c)

SET (OPENGL_SOURCES ${OPENGL_SOURCES} ${OPENGL_UNIX_SOURCES} )
ENDIF (UNIX)

IF (WIN32)
FILE(GLOB OPENGL_WIN32_SOURCES plugins/opengl/win32/src/*.c)

SET (OPENGL_SOURCES ${OPENGL_SOURCES} ${OPENGL_WIN32_SOURCES} )
ENDIF (WIN32)

SET (OPENGL_SOURCES ${OPENGL_SOURCES})
ADD_LIBRARY (dropengl SHARED ${OPENGL_SOURCES})

TARGET_LINK_LIBRARIES (dropengl ${OPENGL_LIBRARIES})
IF (UNIX)
IF (NOT WIN32)
TARGET_LINK_LIBRARIES (dropengl  ${SDL_LIBRARIES} )
ENDIF (NOT WIN32)
ENDIF (UNIX)


################################################################################
######################## Maploader Plugin  #####################################

ADD_DEFINITIONS (-DGLBSP_PLUGIN )

INCLUDE_DIRECTORIES ( external/glbsp/include )

FILE(GLOB MAPLOAD_SOURCES plugins/mapload/src/*.c)
FILE(GLOB GLBSP_SOURCES external/glbsp/src/*.c)

ADD_LIBRARY (dpmapload SHARED ${MAPLOAD_SOURCES} ${GLBSP_SOURCES})


################################################################################
######################## Dehacked Reader Plugin  ###############################

FILE(GLOB DEHREAD_SOURCES plugins/dehread/src/*.c)

ADD_LIBRARY (dpdehread SHARED ${DEHREAD_SOURCES})

################################################################################
######################## SDLMixer Output Plugin  ###############################

################################################################################
######################## OpenAL Output Plugin  #################################

################################################################################
######################## jDoom Game Plugin  ####################################

################################################################################
######################## jHeretic Game Plugin  #################################

################################################################################
######################## jHexen Game Plugin  ###################################

################################################################################
######################## jWolfTC Game Plugin  ##################################

################################################################################
######################## jDoom64 Game Plugin  ##################################

################################################################################
######################## jStrife Game Plugin  ##################################
