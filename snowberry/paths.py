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

## @file paths.py File Path Services
##
## This module contains functions that can be used to determine where
## in the filesystem the various parts of the Snowberry application
## and data are located.
##
## It is assumed that the current working directory is Snowberry's
## installation directory, and also contains Doomsday.

import os, re, sys

# Directory Type constants to be used with getSystemPath() and
# getUserPath().
ADDONS = 'addons'
UNINSTALLED = 'uninstalled'
CONF = 'conf'
LANG = 'lang'
PLUGINS = 'plugins'
PROFILES = 'profiles'
GRAPHICS = 'graphics'
RUNTIME = 'runtime'

# This is the name of the directory under which Snowberry's files will
# be stored in the user's home directory.
if sys.platform == 'darwin':
    SNOWBERRY_HOME_DIR = 'Snowberry'
else:
    SNOWBERRY_HOME_DIR = '.snowberry'

# This is the full path of the user's Snowberry home directory.
homeDir = None

# Bundle paths.  Files will be searched in these after the user path,
# but before the system path.
bundlePaths = []


def _createDir(path):
    """Creates the specified directory if it doesn't already exist."""
    if not os.path.exists(path):
        os.mkdir(path)


def _checkSnowberryHome():
    """Checks if the Snowberry home directory exists.  If it doesn't, it
    is created and populated with the basic files and directories."""

    global homeDir

    # When this is run for the first time, we'll need to figure out
    # where the home directory should be.
    if not homeDir:
        if sys.platform == 'darwin':
            # Home on the Mac.
            homeLocation = os.path.join(os.getenv('HOME'), 
                                        'Library/Application Support')
        else:
            # First see if a HOME environment variable has been defined.
            homeLocation = os.getenv('HOME')
            
        if not homeLocation:
            # The failsafe.
            homeLocation = os.getcwd()
            # The environment variable was not defined.  Let's try
            # something else.
            if sys.platform == 'win32':
                if os.getenv('HOMEPATH'):
                    homeLocation = os.getenv('HOMEDRIVE') + \
                                   os.getenv('HOMEPATH')
                elif os.getenv('USERPROFILE'):
                    homeLocation = os.getenv('USERPROFILE')
                elif os.getenv('APPDATA'):
                    homeLocation = os.getenv('APPDATA')

        homeDir = os.path.join(homeLocation, SNOWBERRY_HOME_DIR)

    # The home needs to be created if it doesn't exist yet.
    _createDir(homeDir)

    # Create the rest of the user directories if they don't exist yet.
    for p in [ADDONS, UNINSTALLED, CONF, LANG, PLUGINS, PROFILES, GRAPHICS,
              RUNTIME]:
        _createDir(getUserPath(p))


def addBundlePath(path):
    """Add a path to the list of paths where files will be searched
    from.

    @param path The bundle contents path.
    """
    global bundlePaths
    bundlePaths.append(path)


def getHomeDirectory():
    "Returns the current user's Snowberry home directory."
    return homeDir


def getSystemPath(which):
    """Returns the directory where the specified kind of data is
    stored.  System paths are assumed to be relative to the current
    working directory."""
    return which


def getUserPath(which):
    """Returns the directory where the specified kind of data is
    stored for the current user.  The location of the user home
    directory depends on the operating system."""
    return os.path.join(homeDir, which)


def getBundlePaths(which):
    """Returns the directories where plugin bundles have their files
    of the specified kind."""
    return map(lambda p: os.path.join(p, which), bundlePaths)


def listPaths(which):
    """Returns an array of search paths for the specified kind of files.
    """
    return [getUserPath(which)] + getBundlePaths(which) + \
           [getSystemPath(which)]


def listFiles(which):
    """Returns a list of all the files of the specified kind.

    @return An array of absolute file names.
    """
    files = []

    for path in listPaths(which):
        try:
            for name in os.listdir(path):
                full = os.path.join(path, name)
                if full not in files:
                    files.append(os.path.abspath(full))
        except OSError:
            # The directory probably didn't exist.  We don't need to
            # do anything.
            pass

    return files


def findBitmap(name):
    """Locates a bitmap file.  Files in the user's graphics directory
    override the contents of the system graphics directory.

    @param name Name of the bitmap without the file name extension.

    @return The full path to the bitmap file.
    """
    # First see if its in the user's graphics directory.
    for path in listPaths(GRAPHICS):
        for ext in ['bmp', 'png']:
            fileName = os.path.join(path, name + '.' + ext)
            if os.path.exists(fileName):
                return fileName

    # The file was not found.
    return ''


def getBase(path):
    """Returns the file name sans path and extension."""
    base = os.path.basename(path)
    if base.find('.') < 0:
        # There is no extension.
        return base
    # Find the last dot.
    pos = base.find('.')
    return base[:pos]


def hasExtension(extension, fileName):
    """Checks if the specified file name has the given extension.

    @param extension The extension to look for.

    @param fileName A file name.

    @return True, if the extension is found.
    """
    return re.search("^[^.#][^.]*\." + extension + "$", 
                     os.path.basename(fileName).lower()) != None


def quote(fileName):
    """Return the gives path in quotes so that it may be used as a
    command line option."""
    return '"' + fileName.replace('"', '""') + '"'


## When this module is initialized, check for the home directory.
## It is created automatically if it doesn't exist yet.
_checkSnowberryHome()
