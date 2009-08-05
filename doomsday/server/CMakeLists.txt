# The Doomsday Engine Project
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

INCLUDE_DIRECTORIES ( 
    ${CMAKE_SOURCE_DIR}/libdeng2/include 
    ${CMAKE_SOURCE_DIR}/libdeng/api 
    ${CMAKE_SOURCE_DIR}/support/include
    ${SDL_INCLUDE_DIR} )

FILE ( GLOB SOURCES src/*.cc src/*.h )

ADD_EXECUTABLE ( dengsv ${SOURCES} ${APP_SUPPORT_SOURCES} )

TARGET_LINK_LIBRARIES ( dengsv deng2 deng )

IF ( WIN32 )
    SET_TARGET_PROPERTIES ( dengsv PROPERTIES LINK_FLAGS "/NODEFAULTLIB:msvcrt /SUBSYSTEM:console" ) #/SUBSYSTEM:windows" )
ENDIF ( WIN32 )

IF ( APPLE )
    # The server binary goes under Resources as it's not a GUI application.
    INSTALL ( TARGETS dengsv RUNTIME DESTINATION ${DENG_DATA_DIR} )
ELSE ()
    INSTALL ( TARGETS dengsv RUNTIME DESTINATION ${DENG_BINARY_DIR} )
ENDIF ()

INSTALL ( FILES config/server.de config/net.de DESTINATION ${DENG_DATA_DIR}/config/server )

# Documentation.
IF ( DOXYGEN_FOUND )
    ADD_CUSTOM_TARGET ( serverdocs
        ${DOXYGEN_EXECUTABLE} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
ENDIF ( DOXYGEN_FOUND )
