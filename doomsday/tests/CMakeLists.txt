# The Doomsday Engine Project -- Tests
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

OPTION ( ENABLE_TESTS "Build the tests" ON )

# Macro for easily defining the test target.
MACRO ( DENG2_TEST_EXECUTABLE TEST_NAME )
    INCLUDE_DIRECTORIES ( 
        ${CMAKE_SOURCE_DIR}/libdeng2/include 
        ${CMAKE_SOURCE_DIR}/support/include
        ${SDL_INCLUDE_DIR} )
    FILE ( GLOB SOURCES *.cc *.h )
    ADD_EXECUTABLE ( ${TEST_NAME} ${SOURCES} ${APP_SUPPORT_SOURCES} )
    TARGET_LINK_LIBRARIES ( ${TEST_NAME} deng2 )
    IF ( WIN32 )
        SET_TARGET_PROPERTIES ( ${TEST_NAME} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:msvcrt" )
    ENDIF ( WIN32 )
    INSTALL ( TARGETS ${TEST_NAME} RUNTIME DESTINATION ${DENG_BINARY_DIR} )
ENDMACRO ( DENG2_TEST_EXECUTABLE )

IF ( ENABLE_TESTS )
    SUBDIRS ( 
        archive
        basiclink
        record
        script
        sdlwindow
        stoploop 
        vectors 
        )
        
    INSTALL ( FILES testapp.de DESTINATION ${DENG_DATA_DIR}/config )
ENDIF ()
