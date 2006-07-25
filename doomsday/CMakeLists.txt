## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## Licensed under the GNU GPLv2 or later versions.
################################################################################
PROJECT(deng C)
CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
SET (BUILDSCRIPTSDATE "$LastChangedDate$")
################################################################################
######################## Additional Win32 Path Setup  ##########################
IF (WIN32)
## Cmake NEEDS UNIX style paths ie / not \
SET(ENV{SDLDIR} "C:/SDK/SDL")
SET(ENV{OPENALDIR} "C:/SDK/OPENAL")
SET(ENV{ZLIBDIR} "C:/SDK/ZLIB")
SET(ENV{LIBPNGDIR} "C:/SDK/LIBPNG")
SET(ENV{DIRECTXDIR} "C:/SDK/DX9SDK")
ENDIF (WIN32)
################################################################################
######################## Configure Build Options  ##############################
SET(BUILD_SHARED_LIBS ON) 
SET (CMAKE_BUILD_TYPE RelWithDebInfo)
## Default Build type is RelWithDebInfo. Supported Build Types are:
##    * None (CMAKE_C_FLAGS) Cmake gcc default of $CFLAGS.
##    * Debug (CMAKE_C_FLAGS_DEBUG) Cmake gcc default of "-g"
##    * Release (CMAKE_C_FLAGS_RELEASE) Cmake gcc default of "-O3 -DNDEBUG"
##    * RelWithDebInfo (CMAKE_C_FLAGS_RELWITHDEBINFO) Cmake default of "-O2 -g"
##    * MinSizeRel (CMAKE_C_FLAGS_MINSIZEREL) Cmake default of "-Os -DNDEBUG"

OPTION (BUILDJDOOM "Optionally build the jDoom plugin. Default ON" ON)
OPTION (BUILDJHERETIC "Optionally build the jHeretc plugin. Default ON" ON)
OPTION (BUILDJHEXEN "Optionally build the jHexen plugin. Default ON" ON)
OPTION (BUILDJWOLFTC "Optionally build the jWolf64 plugin. Default OFF" OFF)
OPTION (BUILDJDOOM64 "Optionally build the jDoom64 plugin. Default OFF" OFF)
OPTION (BUILDJSTRIFE "Optionally build the jStrife plugin. Default OFF" OFF)
OPTION (BUILDOPENGL "Optionally build the OpenGL renderer. Default ON" ON)
OPTION (BUILDSDLMIXER "Optionally build the SDLMixer plugin. Default ON" ON)
OPTION (BUILDOPENAL "Optionally build the OpenAL plugin. Default ON" ON)
OPTION (BUILDDEDICATED "Build as a dedicated server. Default OFF" OFF)

IF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
MESSAGE( FATAL_ERROR
"In-tree Builds are NOT supported. cd ${CMAKE_SOURCE_DIR}/build and try again")
ENDIF (${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

IF (CMAKE_BUILD_TYPE STREQUAL Debug)
SET (CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS}${CMAKE_C_FLAGS_DEBUG} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL Debug)

IF (CMAKE_BUILD_TYPE STREQUAL Release)
SET (CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS}${CMAKE_C_FLAGS_RELEASE} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL Release)

IF (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
SET (CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS}${CMAKE_C_FLAGS_RELWITHDEBINFO} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)

IF (CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
SET (CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS}${CMAKE_C_FLAGS_MINSIZEREL} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL MinSizeRel)

################################################################################
######################## Doomsday Paths  #######################################
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

IF (UNIX)
IF (NOT WIN32)
## Where does it go on OSX ? FIXME
## This .pk3 is generated in the doomsday target.
SET (DENG_ENGINE_DATA ${CMAKE_CURRENT_BINARY_DIR}/doomsday.pk3 ${CMAKE_SOURCE_DIR}/engine/data/cphelp.txt)
FILE(GLOB DENG_ENGINE_DEFS ${CMAKE_SOURCE_DIR}/engine/defs/*.ded)
SET (DENG_ENGINE_DATA_DIR ${CMAKE_INSTALL_PREFIX}${DENG_BASE_DIR}/data)
SET (DENG_ENGINE_DEFS_DIR ${CMAKE_INSTALL_PREFIX}${DENG_BASE_DIR}/defs)

SET (DENG_JDOOM_DATA ${CMAKE_SOURCE_DIR}/plugins/jdoom/data/jdoom.wad)
FILE(GLOB DENG_JDOOM_DEFS ${CMAKE_SOURCE_DIR}/plugins/jdoom/defs/*.ded)
SET (DENG_JDOOM_DATA_DIR ${DENG_ENGINE_DATA_DIR}/jdoom)
SET (DENG_JDOOM_DEFS_DIR ${DENG_ENGINE_DEFS_DIR}/jdoom)

SET (DENG_JHERETIC_DATA ${CMAKE_SOURCE_DIR}/plugins/jheretic/data/jheretic.wad)
FILE(GLOB DENG_JHERETIC_DEFS ${CMAKE_SOURCE_DIR}/plugins/jheretic/defs/*.ded)
SET (DENG_JHERETIC_DATA_DIR ${DENG_ENGINE_DATA_DIR}/jheretic)
SET (DENG_JHERETIC_DEFS_DIR ${DENG_ENGINE_DEFS_DIR}/jheretic)

SET (DENG_JHEXEN_DATA ${CMAKE_SOURCE_DIR}/plugins/jhexen/data/jhexen.wad)
FILE(GLOB DENG_JHEXEN_DEFS ${CMAKE_SOURCE_DIR}/plugins/jhexen/defs/*.ded)
SET (DENG_JHEXEN_DATA_DIR ${DENG_ENGINE_DATA_DIR}/jhexen)
SET (DENG_JHEXEN_DEFS_DIR ${DENG_ENGINE_DEFS_DIR}/jhexen)

SET (DENG_JSTRIFE_DATA ${CMAKE_SOURCE_DIR}/plugins/jstrife/data/jstrife.wad)
FILE(GLOB DENG_JSTRIFE_DEFS ${CMAKE_SOURCE_DIR}/plugins/jstrife/defs/*.ded)
SET (DENG_JSTRIFE_DATA_DIR ${DENG_ENGINE_DATA_DIR}/jstrife)
SET (DENG_JSTRIFE_DEFS_DIR ${DENG_ENGINE_DEFS_DIR}/jstrife)

SET (DENG_JWOLFTC_DATA ${CMAKE_SOURCE_DIR}/plugins/jwolftc/data/jwolftc.wad)
FILE(GLOB DENG_JWOLFTC_DEFS ${CMAKE_SOURCE_DIR}/plugins/jwolftc/defs/*.ded)
SET (DENG_JWOLFTC_DATA_DIR ${DENG_ENGINE_DATA_DIR}/jwolftc)
SET (DENG_JWOLFTC_DEFS_DIR ${DENG_ENGINE_DEFS_DIR}/jwolftc)

ENDIF (NOT WIN32)
ENDIF (UNIX)

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

INCLUDE (${CMAKE_SOURCE_DIR}/build/cmake/FindZLIB.cmake)

INCLUDE (${CMAKE_SOURCE_DIR}/build/cmake/FindPNG.cmake)

IF (UNIX)
INCLUDE (FindCurses)
ENDIF (UNIX)

IF (WIN32)
INCLUDE (${CMAKE_SOURCE_DIR}/build/cmake/FindDX_Dinput.cmake)
ENDIF (WIN32)

IF (NOT BUILDDEDICATED)
IF (UNIX)
IF (NOT APPLE)
INCLUDE (FindX11)
ENDIF (NOT APPLE)
ENDIF (UNIX)

IF (BUILDOPENAL)
INCLUDE (${CMAKE_SOURCE_DIR}/build/cmake/FindOpenAL.cmake)
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

IF (NOT MSVC)
IF (CMAKE_SYSTEM_MACHINE MATCHES "i.86" OR MAKE_SYSTEM_MACHINE MATCHES "x86")
ADD_DEFINITIONS (-DGNU_X86_FIXED_ASM )
SET (BUILDFIXEDASM ON)
ELSE (CMAKE_SYSTEM_MACHINE MATCHES "i.86" OR MAKE_SYSTEM_MACHINE MATCHES "x86")
ADD_DEFINITIONS (-DNO_FIXED_ASM )
SET (BUILDFIXEDASM OFF)
ENDIF (CMAKE_SYSTEM_MACHINE MATCHES "i.86" OR MAKE_SYSTEM_MACHINE MATCHES "x86")
ELSE (NOT MSVC)
ADD_DEFINITIONS (-DNO_FIXED_ASM )
SET (BUILDFIXEDASM OFF)
ENDIF (NOT MSVC)

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
IF (MSVC80)
ADD_DEFINITIONS ( -D_CRT_SECURE_NO_DEPRECATE)
SET (BUILDSYSTEM WIN32-MSVC80)
ENDIF (MSVC80)
IF (MINGW)
ADD_DEFINITIONS ( -DUNICODE)
SET (BUILDSYSTEM WIN32-MINGW)
ENDIF (MINGW)
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
INCLUDE_DIRECTORIES (${OPENGL_INCLUDE_DIR})
ENDIF (BUILDOPENGL)

## Confirm we have SDL
IF (SDL_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL not found.
On Ubuntu install libsdl1.2-dev.
On Mac OSX install FIXME.
On Windows/MSVC install SDL-devel-1.2.11-VC6.zip.")
ENDIF (SDL_FOUND STREQUAL "NO")
INCLUDE_DIRECTORIES (${SDL_INCLUDE_DIR})

IF (BUILDSDLMIXER)
## Confirm we have SDL_mixer
IF (SDLMIXER_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL Mixer not found.
On Ubuntu install libsdl-mixer1.2-dev.
On Mac OSX install FIXME.
On Windows/MSVC install SDL_mixer-devel-1.2.7-VC6.zip.")
ENDIF (SDLMIXER_FOUND STREQUAL "NO")
INCLUDE_DIRECTORIES (${SDLMIXER_INCLUDE_DIR})
ENDIF (BUILDSDLMIXER)

## Confirm we have SDL_net
IF (SDLNET_FOUND STREQUAL "NO")
MESSAGE (FATAL_ERROR
"** SDL Net not found.
On Ubuntu install libsdl-net1.2-dev.
On Mac OSX install FIXME.
On Windows/MSVC install SDL_net-devel-1.2.6-VC6.zip.")
ENDIF (SDLNET_FOUND STREQUAL "NO")
INCLUDE_DIRECTORIES (${SDLNET_INCLUDE_DIR})

## Confirm we have zlib
IF (NOT ZLIB_FOUND STREQUAL "YES")
MESSAGE (FATAL_ERROR
"** zlib not found.
On Ubuntu install zlib1g-dev.
On Mac OSX install FIXME.
On Windows install zlib123-dll.zip.")
ENDIF (NOT ZLIB_FOUND STREQUAL "YES")
INCLUDE_DIRECTORIES (${ZLIB_INCLUDE_DIR})

## confirm we have libPNG.
IF (NOT PNG_FOUND)
MESSAGE (FATAL_ERROR
"** libpng not found.
On Ubuntu install libpng-dev.
On Mac OSX install FIXME.
On Windows install libpng-1.2.8-lib.zip.")
ENDIF (NOT PNG_FOUND)
INCLUDE_DIRECTORIES (${PNG_INCLUDE_DIR})

## Confirm we have ncurses
IF (UNIX)
IF (CURSES_LIBRARIES STREQUAL "")
MESSAGE (FATAL_ERROR
"** ncurses not found.
On Ubuntu install libncurses5-dev.
On Mac OSX install FIXME.")
ENDIF (CURSES_LIBRARIES STREQUAL "")
INCLUDE_DIRECTORIES (${CURSES_INCLUDE_DIR})
ENDIF (UNIX)

## Confirm we have X11
IF (UNIX)
IF (NOT APPLE)
IF (NOT X11_FOUND)
MESSAGE (FATAL_ERROR
"** X11 not found.
On Ubuntu install libx11-dev, libxext-dev.")
ENDIF (NOT X11_FOUND)
INCLUDE_DIRECTORIES (${X11_INCLUDE_DIR})
ENDIF (NOT APPLE)
ENDIF (UNIX)

## Confirm we have DirectX
IF (WIN32)
IF (NOT DIRECTX_DINPUT_FOUND STREQUAL "YES")
MESSAGE (FATAL_ERROR
"** DirectX Dinput not found.
On Windows install dx9sdk.exe.")
ENDIF (NOT DIRECTX_DINPUT_FOUND  STREQUAL "YES")
INCLUDE_DIRECTORIES (${DIRECTX_DINPUT_INCLUDE_DIR})
ENDIF (WIN32)

IF (BUILDOPENAL)
## Confirm we have OpenAL
IF (NOT OPENAL_FOUND STREQUAL "YES")
MESSAGE (FATAL_ERROR
"** Openal not found.
On Ubuntu install libopenal-dev.
On Mac OSX install FIXME.
On Windows install OpenAL11CoreSDK.exe.")
ENDIF (NOT OPENAL_FOUND STREQUAL "YES")
INCLUDE_DIRECTORIES (${OPENAL_INCLUDE_DIR})
ENDIF (BUILDOPENAL)

IF (UNIX)
IF (NOT WIN32)
## Confirm we have dlopen
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
On Windows install zip232xN.zip.")
ENDIF (ZIP STREQUAL "ZIP-NOTFOUND")
## Generate doomsday.pk3
EXEC_PROGRAM(${ZIP} ${CMAKE_SOURCE_DIR}/engine/ ARGS " -r -q -0 ${CMAKE_CURRENT_BINARY_DIR}/doomsday.pk3 data/*")


################################################################################
######################## External LZSS Files ###################################
INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/lzss/portable/include )

IF (UNIX)
IF (NOT WIN32)
FILE(GLOB LZSS_SOURCES ${CMAKE_SOURCE_DIR}/external/lzss/unix/src/*.c)
ENDIF (NOT WIN32)
ENDIF (UNIX)

IF (WIN32)
SET (LZSS_SOURCES "")
ENDIF (WIN32)
################################################################################
######################## The Doomsday Engine  ##################################
ADD_DEFINITIONS (-D__DOOMSDAY__ )

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/engine/api ${CMAKE_SOURCE_DIR}/engine/portable/include )

FILE(GLOB DENG_PORTABLE_SOURCES ${CMAKE_SOURCE_DIR}/engine/portable/src/*.c)

SET (DENG_SOURCES ${DENG_PORTABLE_SOURCES} ${LZSS_SOURCES}
 plugins/common/src/m_fixed.c)

IF (UNIX)
IF (NOT WIN32)
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/engine/unix/include )
FILE(GLOB DENG_UNIX_SOURCES ${CMAKE_SOURCE_DIR}/engine/unix/src/*.c)

SET (DENG_SOURCES ${DENG_SOURCES} ${DENG_UNIX_SOURCES} )

ENDIF (NOT WIN32)
ENDIF (UNIX)

IF (WIN32)
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/engine/win32/include )
##FILE(GLOB DENG_WIN32_SOURCES ${CMAKE_SOURCE_DIR}/engine/win32/src/*.c)
SET (DENG_WIN32_SOURCES
${CMAKE_SOURCE_DIR}/engine/win32/src/dd_winit.c
${CMAKE_SOURCE_DIR}/engine/win32/src/sys_console.c
${CMAKE_SOURCE_DIR}/engine/win32/src/sys_findfile.c
${CMAKE_SOURCE_DIR}/engine/win32/src/sys_input.c
${CMAKE_SOURCE_DIR}/engine/win32/src/sys_mixer.c
${CMAKE_SOURCE_DIR}/engine/win32/src/sys_musd_win.c
${CMAKE_SOURCE_DIR}/engine/win32/src/sys_sfxd_loader.c
${CMAKE_SOURCE_DIR}/engine/win32/src/sys_stwin.c )

SET (DENG_SOURCES ${DENG_SOURCES} ${DENG_WIN32_SOURCES})
ENDIF (WIN32)

## FIXME - Add OSX here
## What is a .m file ???

ADD_EXECUTABLE (doomsday ${DENG_SOURCES})
TARGET_LINK_LIBRARIES (doomsday ${SDL_LIBRARY}
${SDLNET_LIBRARY} ${ZLIB_LIBRARIES} ${CURSES_LIBRARIES} ${PNG_LIBRARIES} )


IF (UNIX)
IF (NOT WIN32)
TARGET_LINK_LIBRARIES (doomsday dl)
IF (NOT APPLE)
TARGET_LINK_LIBRARIES (doomsday  ${X11_LIBRARIES})
ENDIF (NOT APPLE)
ENDIF (NOT WIN32)
ENDIF (UNIX)

IF (WIN32)
TARGET_LINK_LIBRARIES (doomsday ${CMAKE_SOURCE_DIR}/external/lzss/win32/lzss)
ENDIF (WIN32)
################################################################################
######################## OpenGL Renderer Plugin ################################
IF (BUILDOPENGL)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/opengl )
ENDIF (BUILDOPENGL)
################################################################################
######################## Maploader Plugin  #####################################
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/mapload )
################################################################################
######################## Dehacked Reader Plugin  ###############################
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/dehread )
################################################################################
######################## SDLMixer Output Plugin  ###############################
IF (BUILDSDLMIXER)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/sdlmixer )
ENDIF (BUILDSDLMIXER)
################################################################################
######################## OpenAL Output Plugin  #################################
IF (BUILDOPENAL)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/openal )
ENDIF (BUILDOPENAL)
################################################################################
######################## jDoom Game Plugin  ####################################
IF (BUILDJDOOM)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/jdoom )
ENDIF (BUILDJDOOM)
################################################################################
######################## jHeretic Game Plugin  #################################
IF (BUILDJHERETIC)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/jheretic )
ENDIF (BUILDJHERETIC)
################################################################################
######################## jHexen Game Plugin  ###################################
IF (BUILDJHEXEN)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/jhexen )
ENDIF (BUILDJHEXEN)
################################################################################
######################## jWolfTC Game Plugin  ##################################
IF (BUILDJWOLFTC)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/jwolftc )
ENDIF (BUILDJWOLFTC)
################################################################################
######################## jDoom64 Game Plugin  ##################################
IF (BUILDJDOOM64)
#ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/jdoom64 )
ENDIF (BUILDJDOOM64)
################################################################################
######################## jStrife Game Plugin  ##################################
IF (BUILDJSTRIFE)
ADD_SUBDIRECTORY (${CMAKE_SOURCE_DIR}/plugins/jstrife )
ENDIF (BUILDJSTRIFE)
################################################################################
######################## Show Build Details  ###################################
MESSAGE( STATUS "BUILDSCRIPTSDATE:             " ${BUILDSCRIPTSDATE} )
MESSAGE( STATUS "CMAKE_SYSTEM:                 " ${CMAKE_SYSTEM} )
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR:       " ${CMAKE_SYSTEM_PROCESSOR} )
MESSAGE( STATUS "CMAKE_BUILD_TYPE:             " ${CMAKE_BUILD_TYPE} )
IF (CMAKE_BUILD_TYPE STREQUAL None)
MESSAGE( STATUS "CMAKE_C_FLAGS:                " ${CMAKE_C_FLAGS} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL None)
IF (CMAKE_BUILD_TYPE STREQUAL Debug)
MESSAGE( STATUS "CMAKE_C_FLAGS_DEBUG:          " ${CMAKE_C_FLAGS_DEBUG} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL Debug)
IF (CMAKE_BUILD_TYPE STREQUAL Release)
MESSAGE( STATUS "CMAKE_C_FLAGS_RELEASE:        " ${CMAKE_C_FLAGS_RELEASE} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL Release)
IF (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
MESSAGE( STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO: " ${CMAKE_C_FLAGS_RELWITHDEBINFO} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
IF (CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
MESSAGE( STATUS "CMAKE_C_FLAGS_MINSIZEREL:     " ${CMAKE_C_FLAGS_MINSIZEREL} )
ENDIF (CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
MESSAGE( STATUS "CMAKE_C_COMPILER:             " ${CMAKE_C_COMPILER} )
MESSAGE( STATUS "CMAKE_SOURCE_DIR:             " ${CMAKE_SOURCE_DIR} )
MESSAGE( STATUS "CMAKE_BINARY_DIR:             " ${CMAKE_BINARY_DIR} )
MESSAGE( STATUS "BUILDJDOOM:                   " ${BUILDJDOOM} )
MESSAGE( STATUS "BUILDJHERETIC:                " ${BUILDJHERETIC} )
MESSAGE( STATUS "BUILDJHEXEN:                  " ${BUILDJHEXEN} )
MESSAGE( STATUS "BUILDJWOLFTC:                 " ${BUILDJWOLFTC} )
MESSAGE( STATUS "BUILDJDOOM64:                 " ${BUILDJDOOM64} )
MESSAGE( STATUS "BUILDJSTRIFE:                 " ${BUILDJSTRIFE} )
MESSAGE( STATUS "BUILDOPENGL:                  " ${BUILDOPENGL} )
MESSAGE( STATUS "BUILDOPENAL:                  " ${BUILDOPENAL} )
MESSAGE( STATUS "BUILDSDLMIXER:                " ${BUILDSDLMIXER} )
MESSAGE( STATUS "BUILDDEDICATED:               " ${BUILDDEDICATED} )
MESSAGE( STATUS "BUILDFIXEDASM:                " ${BUILDFIXEDASM} )
MESSAGE( STATUS "BUILDSYSTEM:                  " ${BUILDSYSTEM} )
MESSAGE( STATUS "CMAKE_INSTALL_PREFIX          " ${CMAKE_INSTALL_PREFIX} )
MESSAGE( STATUS "DENG_BASE_DIR                 " ${CMAKE_INSTALL_PREFIX}${DENG_BASE_DIR} )
MESSAGE( STATUS "DENG_LIBRARY_DIR              " ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
MESSAGE( STATUS "DENG_BINARY_DIR               " ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR} )
MESSAGE( STATUS "DENG_ENGINE_DATA_DIR          " ${CMAKE_INSTALL_PREFIX}${DENG_ENGINE_DATA_DIR} )
MESSAGE( STATUS "DENG_ENGINE_DEFS_DIR          " ${CMAKE_INSTALL_PREFIX}${DENG_ENGINE_DEFS_DIR} )
MESSAGE( STATUS "DENG_JDOOM_DATA_DIR           " ${CMAKE_INSTALL_PREFIX}${DENG_JDOOM_DATA_DIR} )
MESSAGE( STATUS "DENG_JDOOM_DEFS_DIR           " ${CMAKE_INSTALL_PREFIX}${DENG_JDOOM_DEFS_DIR} )
MESSAGE( STATUS "DENG_JHERETIC_DATA_DIR        " ${CMAKE_INSTALL_PREFIX}${DENG_JHERETIC_DATA_DIR} )
MESSAGE( STATUS "DENG_JHERETIC_DEFS_DIR        " ${CMAKE_INSTALL_PREFIX}${DENG_JHERETIC_DEFS_DIR} )
MESSAGE( STATUS "DENG_JHEXEN_DATA_DIR          " ${CMAKE_INSTALL_PREFIX}${DENG_JHEXEN_DATA_DIR} )
MESSAGE( STATUS "DENG_JHEXEN_DEFS_DIR          " ${CMAKE_INSTALL_PREFIX}${DENG_JHEXEN_DEFS_DIR} )
MESSAGE( STATUS "DENG_JSTRIFE_DATA_DIR         " ${CMAKE_INSTALL_PREFIX}${DENG_JSTRIFE_DATA_DIR} )
MESSAGE( STATUS "DENG_JSTRIFE_DEFS_DIR         " ${CMAKE_INSTALL_PREFIX}${DENG_JSTRIFE_DEFS_DIR} )
MESSAGE( STATUS "DENG_JWOLFTC_DATA_DIR         " ${CMAKE_INSTALL_PREFIX}${DENG_JWOLFTC_DATA_DIR} )
MESSAGE( STATUS "DENG_JWOLFTC_DEFS_DIR         " ${CMAKE_INSTALL_PREFIX}${DENG_JWOLFTC_DEFS_DIR} )
################################################################################
######################## Install Targets  ######################################
INSTALL(TARGETS doomsday
RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_BINARY_DIR} 
LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}${DENG_LIBRARY_DIR} )
INSTALL(FILES ${DENG_ENGINE_DATA} DESTINATION ${DENG_ENGINE_DATA_DIR} )
INSTALL(FILES ${DENG_ENGINE_DEFS} DESTINATION ${DENG_ENGINE_DEFS_DIR} )

IF (BUILDJDOOM)
INSTALL(FILES ${DENG_JDOOM_DATA} DESTINATION ${DENG_JDOOM_DATA_DIR} )
INSTALL(FILES ${DENG_JDOOM_DEFS} DESTINATION ${DENG_JDOOM_DEFS_DIR} )
ENDIF (BUILDJDOOM)

IF (BUILDJHERETIC)
INSTALL(FILES ${DENG_JHERETIC_DATA} DESTINATION ${DENG_JHERETIC_DATA_DIR} )
INSTALL(FILES ${DENG_JHERETIC_DEFS} DESTINATION ${DENG_JHERETIC_DEFS_DIR} )
ENDIF (BUILDJHERETIC)

IF (BUILDJHEXEN)
INSTALL(FILES ${DENG_JHEXEN_DATA} DESTINATION ${DENG_JHEXEN_DATA_DIR} )
INSTALL(FILES ${DENG_JHEXEN_DEFS} DESTINATION ${DENG_JHEXEN_DEFS_DIR} )
ENDIF (BUILDJHEXEN)

IF (BUILDJSTRIFE)
INSTALL(FILES ${DENG_JSTRIFE_DATA} DESTINATION ${DENG_JSTRIFE_DATA_DIR} )
INSTALL(FILES ${DENG_JSTRIFE_DEFS} DESTINATION ${DENG_JSTRIFE_DEFS_DIR} )
ENDIF (BUILDJSTRIFE)

IF (BUILDJWOLFTC)
INSTALL(FILES ${DENG_JWOLFTC_DATA} DESTINATION ${DENG_JWOLFTC_DATA_DIR} )
INSTALL(FILES ${DENG_JWOLFTC_DEFS} DESTINATION ${DENG_JWOLFTC_DEFS_DIR} )
ENDIF (BUILDJWOLFTC)
