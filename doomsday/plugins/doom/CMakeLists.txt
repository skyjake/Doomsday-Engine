# The Doomsday Engine Project -- jDoom
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

ADD_DEFINITIONS ( -D__JDOOM__ )

INCLUDE_DIRECTORIES ( include )

FILE ( GLOB SOURCES src/*.c include/*.h )

ADD_LIBRARY ( deng_doom SHARED ${SOURCES} ${COMMON_GAME_SOURCES} )

TARGET_LINK_LIBRARIES ( deng_doom deng )

INSTALL ( TARGETS deng_doom 
    RUNTIME DESTINATION ${PLUGIN_BINARY_DIR}
    LIBRARY DESTINATION ${PLUGIN_BINARY_DIR} )
