## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################
PROJECT(deng C)
################################################################################
######################## Configure Build Options  ##############################
OPTION (BUILDJDOOM "Optionally build the jDoom plugin. Default ON" ON)
OPTION (BUILDJHERETIC "Optionally build the jHeretc plugin. Default ON" ON)
OPTION (BUILDJHEXEN "Optionally build the jHexen plugin. Default ON" ON)
OPTION (BUILDJWOLFTC "Optionally build the jWolf64 plugin. Default OFF" OFF)
OPTION (BUILDJDOOM64 "Optionally build the jDoom64 plugin. Default OFF" OFF)
OPTION (BUILDJSTRIFE "Optionally build the jStrife plugin. Default OFF" OFF)
OPTION (BUILDOPENGL "Optionally build the OpenGL renderer. Default ON" ON)
OPTION (BUILDSDLMIXER "Optionally build the SDLMixer plugin. Default ON" ON)
OPTION (BUILDOPENAL "Optionally build the SDLMixer plugin. Default ON" ON)
OPTION (BUILDDEDICATED "Build as a dedicated server. Default OFF" OFF)


################################################################################
######################## Include Standard Cmake Helpers  #######################
IF (NOT BUILDDEDICATED)
IF (BUILDOPENGL)
INCLUDE (FindOpenGL)
ENDIF (BUILDOPENGL)
ENDIF (NOT BUILDDEDICATED)

INCLUDE (FindSDL)

IF (NOT BUILDDEDICATED)
IF (BUILDSDLMIXER)
INCLUDE (FindSDL_mixer)
ENDIF (BUILDSDLMIXER)
ENDIF (NOT BUILDDEDICATED)

INCLUDE (FindSDL_net)

INCLUDE (FindZLIB)

INCLUDE (FindPNG)

INCLUDE (FindCurses)

IF (NOT BUILDDEDICATED)
IF (UNIX)
IF (NOT APPLE)
INCLUDE (FindX11)
ENDIF (NOT APPLE)
ENDIF (UNIX)

IF (BUILDOPENAL)
INCLUDE (FindOpenAL)
ENDIF (BUILDOPENAL)
ENDIF (NOT BUILDDEDICATED)

IF (NOT BUILDDEDICATED AND NOT BUILDOPENGL)
MESSAGE( FATAL_ERROR
"If not building a dedicated server, the OPENGL renderer must be built.")
ENDIF (NOT BUILDDEDICATED AND NOT BUILDOPENGL)

IF (NOT BUILDDEDICATED AND NOT BUILDOPENAL AND NOT BUILDSDLMIXER)
MESSAGE( FATAL_ERROR
"If not building a dedicated server, a sound plugin must be built")
ENDIF (NOT BUILDDEDICATED AND NOT BUILDOPENAL AND NOT BUILDSDLMIXER)
################################################################################
######################## Show Build Details  ###################################

MESSAGE( STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM} )
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR} )
MESSAGE( STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} )
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} )
MESSAGE( STATUS "BUILDJDOOM: " ${BUILDJDOOM} )
MESSAGE( STATUS "BUILDJHERETIC: " ${BUILDJHERETIC} )
MESSAGE( STATUS "BUILDJHEXEN: " ${BUILDJHEXEN} )
MESSAGE( STATUS "BUILDJWOLFTC: " ${BUILDJWOLFTC} )
MESSAGE( STATUS "BUILDJDOOM64: " ${BUILDJDOOM64} )
MESSAGE( STATUS "BUILDJSTRIFE: " ${BUILDJSTRIFE} )
MESSAGE( STATUS "BUILDOPENGL: " ${BUILDOPENGL} )
MESSAGE( STATUS "BUILDOPENAL: " ${BUILDOPENAL} )
MESSAGE( STATUS "BUILDSDLMIXER: " ${BUILDSDLMIXER} )
MESSAGE( STATUS "BUILDDEDICATED: " ${BUILDDEDICATED} )

################################################################################
######################## Find Our Build Deps  ##################################
IF (BUILDOPENGL)
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
ENDIF (BUILDOPENGL)

## Confirm we have SDL
IF (SDL_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL not found.
On Ubuntu install libsdl1.2-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (SDL_FOUND STREQUAL "NO")

INCLUDE_DIRECTORIES ( ${SDL_INCLUDE_DIR})

IF (BUILDSDLMIXER)
## Confirm we have SDL_mixer
IF (SDLMIXER_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL Mixer not found.
On Ubuntu install libsdl-mixer1.2-dev.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (SDLMIXER_FOUND STREQUAL "NO")

INCLUDE_DIRECTORIES ( ${SDLMIXER_INCLUDE_DIR})
ENDIF (BUILDSDLMIXER)

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
##ADD_DEFINITIONS (-DNORANGECHECKING)

## If x86 (32bit) we want this. FIXME
IF (WIN32)
ADD_DEFINITIONS (-DGNU_X86_FIXED_ASM )
ENDIF (WIN32)
## Otherwise we want this. FIXME
IF (NOT WIN32)
ADD_DEFINITIONS (-DNO_FIXED_ASM )
ENDIF (NOT WIN32)

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
## Seems not needed here
##INCLUDE_DIRECTORIES ( plugins/common/include )
##FILE(GLOB COMMON_PLUGIN_SOURCES plugins/common/src/*.c)
################################################################################
######################## External LZSS Files ###################################

INCLUDE_DIRECTORIES ( external/lzss/portable/include )
FILE(GLOB LZSS_SOURCES external/lzss/unix/src/*.c)


################################################################################
######################## The Doomsday Engine  ##################################

ADD_DEFINITIONS (-D__DOOMSDAY__ )

## FIXME FIXME FIXME FIXME FIXME - AWFUL HACK - HIGH PRIORITY
ADD_DEFINITIONS ('-DDENG_BASE_DIR=\"/usr/local/share/deng\"')
ADD_DEFINITIONS ('-DDENG_LIBRARY_DIR=\"/usr/local/lib/\"')

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
TARGET_LINK_LIBRARIES (doomsday ${SDL_LIBRARY}
${SDLNET_LIBRARY} ${ZLIB_LIBRARIES} ${CURSES_LIBRARY} ${PNG_LIBRARY} )

IF (UNIX)
IF (NOT APPLE)
IF (NOT WIN32)
TARGET_LINK_LIBRARIES (doomsday  ${X11_LIBRARIES})
ENDIF (NOT WIN32)
ENDIF (NOT APPLE)
ENDIF (UNIX)

################################################################################
######################## OpenGL Renderer Plugin ################################
IF (BUILDOPENGL)
ADD_SUBDIRECTORY (plugins/opengl )
ENDIF (BUILDOPENGL)
################################################################################
######################## Maploader Plugin  #####################################
ADD_SUBDIRECTORY (plugins/mapload )
################################################################################
######################## Dehacked Reader Plugin  ###############################
ADD_SUBDIRECTORY (plugins/dehread )
################################################################################
######################## SDLMixer Output Plugin  ###############################
IF (BUILDSDLMIXER)
ADD_SUBDIRECTORY (plugins/sdlmixer )
ENDIF (BUILDSDLMIXER)
################################################################################
######################## OpenAL Output Plugin  #################################
IF (BUILDOPENAL)
ADD_SUBDIRECTORY (plugins/openal )
ENDIF (BUILDOPENAL)
################################################################################
######################## jDoom Game Plugin  ####################################
IF (BUILDJDOOM)
ADD_SUBDIRECTORY (plugins/jdoom )
ENDIF (BUILDJDOOM)
################################################################################
######################## jHeretic Game Plugin  #################################
IF (BUILDJHERETIC)
ADD_SUBDIRECTORY (plugins/jheretic )
ENDIF (BUILDJHERETIC)
################################################################################
######################## jHexen Game Plugin  ###################################
IF (BUILDJHEXEN)
ADD_SUBDIRECTORY (plugins/jhexen )
ENDIF (BUILDJHEXEN)
################################################################################
######################## jWolfTC Game Plugin  ##################################
IF (BUILDJWOLFTC)
#ADD_SUBDIRECTORY (plugins/jwolftc )
ENDIF (BUILDJWOLFTC)
################################################################################
######################## jDoom64 Game Plugin  ##################################
IF (BUILDJDOOM64)
#ADD_SUBDIRECTORY (plugins/jdoom64 )
ENDIF (BUILDJDOOM64)
################################################################################
######################## jStrife Game Plugin  ##################################
IF (BUILDJSTRIFE)
#ADD_SUBDIRECTORY (plugins/jstrife )
ENDIF (BUILDJSTRIFE)
