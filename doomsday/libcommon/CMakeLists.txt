# The Doomsday Engine Project -- libcommon
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
    -DLIBCOMMON_MAJOR_VERSION=${LIBCOMMON_MAJOR_VERSION}
    -DLIBCOMMON_MINOR_VERSION=${LIBCOMMON_MINOR_VERSION}
    -DLIBCOMMON_PATCHLEVEL=${LIBCOMMON_PATCHLEVEL}
    -DLIBCOMMON_RELEASE_LABEL="${LIBCOMMON_RELEASE_LABEL}" )

MESSAGE ( STATUS "libcommon version ${LIBCOMMON_RELEASE_LABEL} ${LIBCOMMON_MAJOR_VERSION}.${LIBCOMMON_MINOR_VERSION}.${LIBCOMMON_PATCHLEVEL}")

# Configuration.
IF ( WIN32 )
    # Tell Windows to export symbols.
    ADD_DEFINITIONS ( -DLIBCOMMON_EXPORTS )
ENDIF ()

INCLUDE_DIRECTORIES ( include 
    ${CMAKE_SOURCE_DIR}/libdeng2/include
    ${CMAKE_SOURCE_DIR}/libdeng/api )

# Sources.
FILE ( GLOB_RECURSE SOURCES src/*.cc src/*.h include/common/*.h )

ADD_LIBRARY ( common SHARED ${SOURCES} )

TARGET_LINK_LIBRARIES ( common deng2 deng )

# Installation.
INSTALL ( TARGETS common
    RUNTIME DESTINATION ${DENG_BINARY_DIR} 
    LIBRARY DESTINATION ${DENG_BINARY_DIR} )

IF ( APPLE )
    # Make sure the library is found inside the bundle.
    SET_TARGET_PROPERTIES ( common PROPERTIES INSTALL_NAME_DIR "@executable_path/../MacOS")
ENDIF ( APPLE )
