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

INCLUDE_DIRECTORIES ( ../libdeng/api common/include ../external/lzss/portable/include )

FILE ( GLOB COMMON_GAME_SOURCES common/src/*.c )

# Building the plugins is optional.
OPTION ( ENABLE_DOOM "Build the Doom game plugin" ON )
OPTION ( ENABLE_HERETIC "Build the Heretic game plugin" ON )
OPTION ( ENABLE_HEXEN "Build the Hexen game plugin" ON )
OPTION ( ENABLE_DOOM64 "Build the Doom64 game plugin" ON )

IF ( ENABLE_DOOM )
    SUBDIRS ( doom )
ENDIF ()

IF ( ENABLE_HERETIC )
    SUBDIRS ( heretic )
ENDIF ()

IF ( ENABLE_HEXEN )
    SUBDIRS ( hexen )
ENDIF ()

IF ( ENABLE_DOOM64 )
    SUBDIRS ( jdoom64 )
ENDIF ()

SUBDIRS ( dehread wadmapconverter )

IF ( APPLE )
    SET ( PLUGIN_BINARY_DIR ${DENG_BINARY_DIR} )
ELSE ()
    SET ( PLUGIN_BINARY_DIR ${DENG_BINARY_DIR}/plugins )
ENDIF ()
