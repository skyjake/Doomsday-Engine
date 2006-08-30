## deng build scripts. 
## Copyright © 2006:	Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## This file is licensed under the GNU GPLv2 or any later versions,
## or at your option the BSD 3 clause license. Both license texts follow.
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
################################################################################
##
## Copyright (c) © 2006 - Jamie Jones (Yagisan) <jamie_jones_au@yahoo.com.au>
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##
##  * Redistributions of source code must retain the above copyright notice,
##    this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright notice,
##    this list of conditions and the following disclaimer in the documentation
##    and/or other materials provided with the distribution.
##  * Neither the name of the deng team nor the names of its
##    contributors may be used to endorse or promote products derived
##    from this software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.
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
