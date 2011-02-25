# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not: http://www.opensource.org/

## @file host.py Information about the Host Environment
##
## This module contains routines that deal with the host environment
## where Snowberry is running.

import platform
import string
import sys


def isWindows():
    """Check if the host is running Windows.

    @return True, if the operating system is Windows.
    """
    system = platform.system().lower()
    if 'microsoft' in system or 'windows' in system:
        return True
    return False


def isWindowsVista():
    """Check if the host is running Windows Vista or newer."""
    if sys.platform != "win32":
        return False
    winVersion = sys.getwindowsversion()
    return (winVersion[0] >= 6)


def isMac():
    """Check if the host is running a Macintosh operating system.

    @return True, if the operating system is Mac OS X.
    """
    return platform.system() == 'Darwin'


def isUnix():
    """Check if the host is running a variant of Unix.

    @return True, if the operating system is Unix.
    """
    return not isWindows() and not isMac()
    
    
def getEncoding():
    """Returns the character encoding."""
    
    if isMac():
        return "iso-8859-1" #"mac-roman"
    else:
        return "iso-8859-1"
