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

INCLUDE_DIRECTORIES ( include ${CMAKE_SOURCE_DIR}/libdeng/api )

FILE ( GLOB SOURCES portable/*.cc portable/*.h include/de/*.h )

ADD_LIBRARY ( deng2 SHARED ${SOURCES} )

INSTALL ( TARGETS deng2 
    RUNTIME DESTINATION bin 
    LIBRARY DESTINATION bin )