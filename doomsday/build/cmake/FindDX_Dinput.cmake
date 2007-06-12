## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## This file is licensed under the GNU GPLv2 or any later versions,
##
## This file is part of the deng build scripts
##
## Copyright © 2006 - Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
##
## the deng build scripts is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## the deng build scripts is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with the deng build scripts; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, 
## Boston, MA  02110-1301  USA
##

# - Find DirectX Direct Input
# Find the DirectX includes and libraries
#
#  DIRECTX_DINPUT_INCLUDE_DIR - where to find dinput.h
#  DIRECTX_DINPUT_LIBRARIES   - List of libraries when using DirectX DInput.
#  DIRECTX_DINPUT_FOUND       - True if DirectX DInputfound.

FIND_PATH(DIRECTX_DINPUT_INCLUDE_DIR dinput.h
  $ENV{DIRECTXDIR}/include
  /usr/local/include/directx
  /usr/local/include
  /usr/include
)

SET(DIRECTX_DINPUT_NAMES ${DIRECTX_DINPUT_NAMES} dinput)
FIND_LIBRARY(DIRECTX_DINPUT_LIBRARY
  NAMES ${DIRECTX_DINPUT_NAMES}
  PATHS 
  $ENV{DIRECTXDIR}/lib
  /usr/lib
  /usr/local/lib
  /usr/local/lib/directx
)

IF(DIRECTX_INCLUDE_DIR)
  IF(DIRECTX_DINPUT_LIBRARY)
    SET( DIRECTX_DINPUT_LIBRARIES ${DIRECTX_DINPUT_LIBRARY} )
  ENDIF(DIRECTX_DINPUT_LIBRARY)
ENDIF(DIRECTX_INCLUDE_DIR)

SET(DIRECTX_DINPUT_FOUND "NO")
IF(DIRECTX_DINPUT_LIBRARY)
  SET(DIRECTX_DINPUT_FOUND "YES")
ENDIF(DIRECTX_DINPUT_LIBRARY)
