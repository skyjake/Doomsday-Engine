# The Doomsday Engine Project -- libdeng2
#
# Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

INCLUDE_DIRECTORIES ( api portable/include 
    ../libdeng2/include
    ../external/lzss/portable/include 
    ${SDL_INCLUDE_DIR} ${SDLMIXER_INCLUDE_DIR} ${SDLNET_INCLUDE_DIR} 
    ${OPENGL_INCLUDE_DIR} 
    ${PNG_PNG_INCLUDE_DIR} 
    ${CURL_INCLUDE_DIR} 
    ${CURSES_INCLUDE_PATH} 
    ${ZLIB_INCLUDE_DIR} )

ADD_DEFINITIONS ( -D__INTERNAL_MAP_DATA_ACCESS__ )
    
FILE ( GLOB SOURCES 
    portable/src/*.c portable/src/*.cc 
    portable/include/*.h )

# Platform-specific configuration.
IF ( WIN32 )
    # Tell Windows to export.
    ADD_DEFINITIONS ( -DDENG_EXPORTS )
    
    INCLUDE_DIRECTORIES ( win32/include
        $ENV{PLATFORMSDKDIR}/include 
        $ENV{DXDIR}/include )
    
    FILE ( GLOB WIN_SOURCES win32/src/*.c win32/src/*.cc win32/include/*.h )
    SET ( SOURCES ${SOURCES} ${WIN_SOURCES} )
ENDIF ( WIN32 )

IF ( APPLE )
    INCLUDE_DIRECTORIES ( ${QUICKTIME_INCLUDE_DIR} )
    
    FILE ( GLOB MAC_SOURCES mac/src/*.c )
    SET ( SOURCES ${SOURCES} ${MAC_SOURCES} )
ENDIF ( APPLE )

IF ( UNIX )
    # Note that this applies to OS X builds also.
	ADD_DEFINITIONS ( -DDENG_BASE_DIR="./" )

    INCLUDE_DIRECTORIES ( unix/include )

    FILE ( GLOB UNIX_SOURCES 
        unix/src/*.c unix/src/*.cc unix/include/*.h
        ../external/lzss/unix/src/*.cc )
    SET ( SOURCES ${SOURCES} ${UNIX_SOURCES} )
ENDIF ( UNIX )

ADD_LIBRARY ( deng SHARED ${SOURCES} )

TARGET_LINK_LIBRARIES ( deng deng2
    ${SDL_LIBRARY} ${SDLMIXER_LIBRARY} ${SDLNET_LIBRARY} 
    ${OPENGL_gl_LIBRARY} ${OPENGL_glu_LIBRARY} 
    ${CURL_LIBRARY} 
    ${CURSES_NCURSES_LIBRARY} 
    ${PNG_LIBRARY} 
    ${ZLIB_LIBRARY} )

IF ( WIN32 )
    TARGET_LINK_LIBRARIES ( deng 
        wsock32 winmm
        "${DXLIBDIR}/dinput8.lib" 
        "${DXLIBDIR}/dxguid.lib"
        "${CMAKE_SOURCE_DIR}/external/lzss/win32/lzss.lib" )
        
    SET_TARGET_PROPERTIES ( deng PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT" )
ENDIF ( WIN32 )
    
IF ( APPLE )
    TARGET_LINK_LIBRARIES ( deng ${QUICKTIME_LIBRARY} )
ENDIF ( APPLE )

# Installation.
INSTALL ( TARGETS deng 
    RUNTIME DESTINATION ${DENG_BINARY_DIR} 
    LIBRARY DESTINATION ${DENG_BINARY_DIR} )

IF ( APPLE )
    # Make sure the library is found inside the bundle.
    SET_TARGET_PROPERTIES ( deng PROPERTIES INSTALL_NAME_DIR "@executable_path/../MacOS")
ENDIF ( APPLE )
