## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################
PROJECT(deng C)
CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
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

IF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
MESSAGE( FATAL_ERROR
"In-tree Builds are NOT supported. cd ${CMAKE_SOURCE_DIR}/build and try again")
ENDIF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
################################################################################
######################## Doomsday Paths  ##############################
## Default Install Prefix on Win32 is C:/Program Files/deng
## Default Install Prefix on *NIX is /usr/local
## Default Install Prefix on OSX is FIXME
## This is set by Cmake automagically in CMAKE_INSTALL_PREFIX

## Default *NIX paths
## /usr/local/bin			<- Doomsday binary
## /usr/local/lib			<- libdropengl, libjdoom etc
## /usr/local/share/games/deng		<- jPacks and other data

IF (UNIX)
IF (NOT WIN32)
IF (APPLE)
## FIXME. Add location here for OSX
ELSE (APPLE)
SET (DENG_BASE_DIR "/share/games/deng")
SET (DENG_LIBRARY_DIR "/lib")
SET (DENG_BINARY_DIR "/bin")
ENDIF (APPLE)
ENDIF (NOT WIN32)
ENDIF (UNIX)
IF (WIN32)
## FIXME. Add location here for WIN32
ENDIF (WIN32)

ADD_DEFINITIONS ('-DDENG_BASE_DIR="${CMAKE_INSTALL_PREFIX}${DENG_BASE_DIR}/" ')
ADD_DEFINITIONS ('-DDENG_LIBRARY_DIR="${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR}/" ')

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
######################## Set Common Definitions  ###############################
##ADD_DEFINITIONS (-DNORANGECHECKING)

IF (WIN32 OR CMAKE_SYSTEM_MACHINE MATCHES "i.86")
ADD_DEFINITIONS (-DGNU_X86_FIXED_ASM )
SET (BUILDFIXEDASM ON)
ELSE (WIN32 OR CMAKE_SYSTEM_MACHINE MATCHES "i.86")
ADD_DEFINITIONS (-DNO_FIXED_ASM )
SET (BUILDFIXEDASM OFF)
ENDIF (WIN32 OR CMAKE_SYSTEM_MACHINE MATCHES "i.86")

## Apple also needs -DUNIX in places
IF (UNIX)
IF (NOT WIN32)
ADD_DEFINITIONS ( -DUNIX)
SET (BUILDSYSTEM UNIX)
ENDIF (NOT WIN32)
ENDIF (UNIX)

IF (WIN32)
ADD_DEFINITIONS ( -DWIN32)
SET (BUILDSYSTEM WIN32)
ENDIF (WIN32)

IF (APPLE)
ADD_DEFINITIONS ( -DMACOSX)
SET (BUILDSYSTEM MACOSX)
ENDIF (APPLE)
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

IF (UNIX)
IF (NOT WIN32)
CHECK_INCLUDE_FILE("dlfcn.h" HAVE_DL)
IF(HAVE_DL)
CHECK_LIBRARY_EXISTS("dl" "dlopen" "" HAVE_LIB_DL)
IF(NOT HAVE_LIB_DL)
MESSAGE (FATAL_ERROR
"** dlopen not found.
On Ubuntu install FIXME
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF(NOT HAVE_LIB_DL)
ELSE(HAVE_DL)
MESSAGE (FATAL_ERROR
"** dlopen not found.
On Ubuntu install FIXME
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF(HAVE_DL)
ENDIF (NOT WIN32)
ENDIF (UNIX)

FIND_PROGRAM(ZIP zip)
IF (ZIP STREQUAL "ZIP-NOTFOUND")
MESSAGE( FATAL_ERROR
"** zip not found.
On Ubuntu install zip.
On Mac OSX install FIXME.
On Windows install FIXME.")
ENDIF (ZIP STREQUAL "ZIP-NOTFOUND")

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

INCLUDE_DIRECTORIES (engine/api engine/portable/include )

FILE(GLOB DENG_PORTABLE_SOURCES engine/portable/src/*.c)

SET (DENG_SOURCES ${DENG_PORTABLE_SOURCES} ${LZSS_SOURCES}
 plugins/common/src/m_fixed.c)

IF (UNIX)
IF (NOT WIN32)
INCLUDE_DIRECTORIES (engine/unix/include )
FILE(GLOB DENG_UNIX_SOURCES engine/unix/src/*.c)

SET (DENG_SOURCES ${DENG_SOURCES} ${DENG_UNIX_SOURCES} )

ENDIF (NOT WIN32)
ENDIF (UNIX)

IF (WIN32)
INCLUDE_DIRECTORIES (engine/win32/include )
FILE(GLOB DENG_WIN32_SOURCES engine/win32/src/*.c)

SET (DENG_SOURCES ${DENG_SOURCES} ${DENG_WIN32_SOURCES})
ENDIF (WIN32)

## FIXME - Add OSX here
## What is a .m file ???

ADD_EXECUTABLE (doomsday ${DENG_SOURCES})
TARGET_LINK_LIBRARIES (doomsday ${SDL_LIBRARY}
${SDLNET_LIBRARY} ${ZLIB_LIBRARIES} ${CURSES_LIBRARY} ${PNG_LIBRARY} )


IF (UNIX)
IF (NOT WIN32)
TARGET_LINK_LIBRARIES (doomsday dl)
IF (NOT APPLE)
TARGET_LINK_LIBRARIES (doomsday  ${X11_LIBRARIES})
ENDIF (NOT APPLE)
ADD_CUSTOM_COMMAND(TARGET doomsday POST_BUILD
   COMMAND ${ZIP} -v -0 ${CMAKE_CURRENT_BINARY_DIR}/Doomsday.pk3 ${CMAKE_SOURCE_DIR}/engine/data/graphics/* ${CMAKE_SOURCE_DIR}/engine/data/fonts/* ${CMAKE_SOURCE_DIR}/engine/data/keymaps/*   )
ENDIF (NOT WIN32)
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
################################################################################
######################## Show Build Details  ###################################
MESSAGE( STATUS "CMAKE_SYSTEM:           " ${CMAKE_SYSTEM} )
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR} )
MESSAGE( STATUS "CMAKE_C_FLAGS:          " ${CMAKE_C_FLAGS} )
MESSAGE( STATUS "CMAKE_C_COMPILER:       " ${CMAKE_C_COMPILER} )
MESSAGE( STATUS "CMAKE_SOURCE_DIR:       " ${CMAKE_SOURCE_DIR} )
MESSAGE( STATUS "CMAKE_BINARY_DIR:       " ${CMAKE_BINARY_DIR} )
MESSAGE( STATUS "BUILDJDOOM:             " ${BUILDJDOOM} )
MESSAGE( STATUS "BUILDJHERETIC:          " ${BUILDJHERETIC} )
MESSAGE( STATUS "BUILDJHEXEN:            " ${BUILDJHEXEN} )
MESSAGE( STATUS "BUILDJWOLFTC:           " ${BUILDJWOLFTC} )
MESSAGE( STATUS "BUILDJDOOM64:           " ${BUILDJDOOM64} )
MESSAGE( STATUS "BUILDJSTRIFE:           " ${BUILDJSTRIFE} )
MESSAGE( STATUS "BUILDOPENGL:            " ${BUILDOPENGL} )
MESSAGE( STATUS "BUILDOPENAL:            " ${BUILDOPENAL} )
MESSAGE( STATUS "BUILDSDLMIXER:          " ${BUILDSDLMIXER} )
MESSAGE( STATUS "BUILDDEDICATED:         " ${BUILDDEDICATED} )
MESSAGE( STATUS "BUILDFIXEDASM:          " ${BUILDFIXEDASM} )
MESSAGE( STATUS "BUILDSYSTEM:            " ${BUILDSYSTEM} )
MESSAGE( STATUS "CMAKE_INSTALL_PREFIX    " ${CMAKE_INSTALL_PREFIX} )
MESSAGE( STATUS "DENG_BASE_DIR           " ${DENG_BASE_DIR} )
MESSAGE( STATUS "DENG_LIBRARY_DIR        " ${DENG_LIBRARY_DIR} )
MESSAGE( STATUS "DENG_BINARY_DIR         " ${DENG_BINARY_DIR} )

################################################################################
######################## Install Targets  ######################################
##INSTALL(TARGETS doomsday libdpdehread libdpmapload libdropengl libdsopenal
##libdssdlmixer libjdoom libjheretic libjhexen
INSTALL(TARGETS doomsday
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR}
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
