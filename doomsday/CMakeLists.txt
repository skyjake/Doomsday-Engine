# The Doomsday Engine Project -- libdeng2
#
# Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

CMAKE_MINIMUM_REQUIRED ( VERSION 2.6 )

PROJECT ( deng )

# Check for an in-tree build.
IF ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR} )
    MESSAGE ( FATAL_ERROR 
        "In-tree builds are not supported. For instance, try building in ${CMAKE_SOURCE_DIR}/mybuild/." )
ENDIF ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR} )

# General configuration.
SET ( CMAKE_INSTALL_PREFIX ./install )
SET ( DENG_BINARY_DIR bin )
SET ( DENG_DATA_DIR data )

# Win32 configuration.
IF ( WIN32 )
    INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/external/opengl )
    SET ( CMAKE_INCLUDE_PATH 
        ${CMAKE_SOURCE_DIR}/external/libcurl/include 
        ${CMAKE_SOURCE_DIR}/external/zlib/include
        ${CMAKE_SOURCE_DIR}/external/libpng/include )
    SET ( CMAKE_LIBRARY_PATH 
        ${CMAKE_SOURCE_DIR}/external/libcurl/lib 
        ${CMAKE_SOURCE_DIR}/external/zlib/lib
        ${CMAKE_SOURCE_DIR}/external/libpng/lib )
    SET ( PNG_NAMES libpng13 )

    # This will take care of the slash issue.
    GET_FILENAME_COMPONENT ( SDLDIR $ENV{SDLDIR} ABSOLUTE )
    GET_FILENAME_COMPONENT ( SDLMIXERDIR $ENV{SDLMIXERDIR} ABSOLUTE )
    GET_FILENAME_COMPONENT ( SDLNETDIR $ENV{SDLNETDIR} ABSOLUTE )
ENDIF ( WIN32 )

# Dependencies.
FIND_PACKAGE ( OpenGL REQUIRED )
FIND_PACKAGE ( ZLIB REQUIRED )
FIND_PACKAGE ( PNG REQUIRED )
FIND_PACKAGE ( CURL REQUIRED )
FIND_PACKAGE ( SDL REQUIRED )
FIND_PACKAGE ( SDL_mixer REQUIRED )
FIND_PACKAGE ( SDL_image REQUIRED )
FIND_PACKAGE ( SDL_net REQUIRED )
FIND_PACKAGE ( SDL_ttf REQUIRED )
FIND_PACKAGE ( PythonInterp REQUIRED )
FIND_PACKAGE ( Doxygen )

# Set up project-wide variables and definitions.
IF ( WIN32 )
    ADD_DEFINITIONS ( -DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE )
ENDIF ( WIN32 )
IF ( APPLE )
    FIND_PACKAGE ( QuickTime REQUIRED )

    ADD_DEFINITIONS ( -DMACOSX )
    IF ( ${CMAKE_OSX_SYSROOT} STREQUAL /Developer/SDKs/MacOSX10.4u.sdk )
	    SET ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.4" )
	    SET ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.4" )
	ENDIF ( ${CMAKE_OSX_SYSROOT} STREQUAL /Developer/SDKs/MacOSX10.4u.sdk )
ENDIF ( APPLE )
IF ( UNIX )
    FIND_PACKAGE ( Curses REQUIRED )

    ADD_DEFINITIONS ( -DUNIX -DNO_FIXED_ASM )
ENDIF ( UNIX )

# Package the resources into PK3 files.
ADD_CUSTOM_TARGET ( packres 
    ${PYTHON_EXECUTABLE} ./packres.py ${CMAKE_CURRENT_BINARY_DIR} 
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build/scripts )
    
INSTALL ( FILES 
    ${CMAKE_CURRENT_BINARY_DIR}/doomsday.pk3 
    ${CMAKE_CURRENT_BINARY_DIR}/doom.pk3 
    ${CMAKE_CURRENT_BINARY_DIR}/heretic.pk3 
    ${CMAKE_CURRENT_BINARY_DIR}/hexen.pk3 
    DESTINATION ${DENG_DATA_DIR} )
    
INSTALL ( FILES sv.sh cl.sh DESTINATION ${DENG_BINARY_DIR} )

SUBDIRS ( libdeng libdeng2 server client plugins )
