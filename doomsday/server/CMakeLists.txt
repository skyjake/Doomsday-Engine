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
    ${CMAKE_SOURCE_DIR}/libdeng/api )

FILE ( GLOB SOURCES src/*.cc src/*.h )

ADD_EXECUTABLE ( dengsv ${SOURCES} )

TARGET_LINK_LIBRARIES ( dengsv deng2 deng )

IF ( APPLE )
    INSTALL ( TARGETS dengsv RUNTIME DESTINATION ${DENG_DATA_DIR} )
ELSE ()
    INSTALL ( TARGETS dengsv RUNTIME DESTINATION ${DENG_BINARY_DIR} )
ENDIF ()
