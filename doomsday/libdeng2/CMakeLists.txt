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

# Version numbering.
ADD_DEFINITIONS ( 
    -DLIBDENG2_MAJOR_VERSION=${LIBDENG2_MAJOR_VERSION}
    -DLIBDENG2_MINOR_VERSION=${LIBDENG2_MINOR_VERSION}
    -DLIBDENG2_PATCHLEVEL=${LIBDENG2_PATCHLEVEL}
    -DLIBDENG2_RELEASE_LABEL="${LIBDENG2_RELEASE_LABEL}" )

MESSAGE ( STATUS "libdeng2 version ${LIBDENG2_RELEASE_LABEL} ${LIBDENG2_MAJOR_VERSION}.${LIBDENG2_MINOR_VERSION}.${LIBDENG2_PATCHLEVEL}")

# Configuration.
IF ( WIN32 )
    # Tell Windows to export symbols.
    ADD_DEFINITIONS ( -DLIBDENG2_EXPORTS )
ENDIF ()
# Configure behavior for waiting on socket activity.
IF ( APPLE OR WIN32 )
    ADD_DEFINITIONS ( -DLIBDENG2_SOCKET_RECV_TIMEOUT=10000 )
ELSE ()
    ADD_DEFINITIONS ( -DLIBDENG2_SOCKET_RECV_TIMEOUT=100 )
ENDIF ()

INCLUDE_DIRECTORIES ( include ${CMAKE_SOURCE_DIR}/libdeng/api 
    ${SDL_INCLUDE_DIR} ${SDLNET_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} )

# Sources.
FILE ( GLOB_RECURSE SOURCES src/*.cc src/*.h include/de/*.h )

ADD_LIBRARY ( deng2 SHARED ${SOURCES} )

TARGET_LINK_LIBRARIES ( deng2 ${SDL_LIBRARY} ${SDLNET_LIBRARY} ${ZLIB_LIBRARY} )

# Installation.
INSTALL ( TARGETS deng2 
    RUNTIME DESTINATION ${DENG_BINARY_DIR} 
    LIBRARY DESTINATION ${DENG_BINARY_DIR} )
INSTALL ( FILES config/deng.de DESTINATION ${DENG_DATA_DIR}/config )
#INSTALL ( DIRECTORY data/modules DESTINATION ${DENG_DATA_DIR} )
    
# Documentation.
IF ( DOXYGEN_FOUND )
    ADD_CUSTOM_TARGET ( deng2docs
        ${DOXYGEN_EXECUTABLE} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
ENDIF ( DOXYGEN_FOUND )

IF ( APPLE )
    # Make sure the library is found inside the bundle.
    SET_TARGET_PROPERTIES ( deng2 PROPERTIES INSTALL_NAME_DIR "@executable_path/../MacOS")
ENDIF ( APPLE )
