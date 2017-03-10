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
import host, events, logger


def isHomeless():
    """Homeless setup means that Snowberry uses the main installation
    directory for user data.  In Windows, the default is homeless
    mode."""

    noHome = '-nohome' in sys.argv
    useHome = '-home' in sys.argv

    if useHome:
        return False

    if noHome:
        return True

    # The default is homeless in Windows versions earlier than Vista and
    # Win Server 2008.
    if host.isWindows() and not host.isWindowsVista():
        return True

    return False


# Directory Type constants to be used with getSystemPath() and
# getUserPath().
ADDONS = 'addons'
MANIFESTS = 'manifests'
UNINSTALLED = 'uninstalled'
CONF = 'conf'
LANG = 'lang'
PLUGINS = 'plugins'
PROFILES = 'profiles'
GRAPHICS = 'graphics'
RUNTIME = 'runtime'
SERVER_RUNTIME = 'server-runtime'

# List of custom addons directories.
addonPaths = []


# On the Mac, change the current directory to Resources inside the bundle.
if host.isMac():
    import __main__
    os.chdir(os.path.dirname(os.path.abspath(__main__.__file__)))

# This is the name of the directory under which Snowberry's files will
# be stored in the user's home directory.
if host.isMac():
    SNOWBERRY_HOME_DIR = 'Doomsday Engine'
elif host.isWindowsVista():
    SNOWBERRY_HOME_DIR = os.path.join('Documents', 'Doomsday Frontend')
elif host.isWindows():
    SNOWBERRY_HOME_DIR = os.path.join('My Documents', 'Doomsday Frontend')
else:
    SNOWBERRY_HOME_DIR = '.doomsday'
    
# In a homeless setup all the user data goes in the main installation folder.
if isHomeless():
    SNOWBERRY_HOME_DIR = '.'

# This is the full path of the user's Snowberry home directory.
homeDir = None

# Bundle paths.  Files will be searched in these after the user path,
# but before the system path.
bundlePaths = []

# The custom paths override the default user paths.
customPaths = {}


def _createDir(path):
    """Creates the specified directory if it doesn't already exist."""
    try:
        os.makedirs(path)
    except:
        # Must exist already.
        pass


def _checkSnowberryHome():
    """Checks if the Snowberry home directory exists.  If it doesn't, it
    is created and populated with the basic files and directories."""

    global homeDir

    # When this is run for the first time, we'll need to figure out
    # where the home directory should be.
    if not homeDir:
        if isHomeless():
            homeLocation = os.getcwd()           
        else:
            if host.isMac():
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
                if host.isWindows():
                    if os.getenv('HOMEPATH'):
                        homeLocation = os.getenv('HOMEDRIVE') + \
                                       os.getenv('HOMEPATH')
                    elif os.getenv('USERPROFILE'):
                        homeLocation = os.getenv('USERPROFILE')
                    elif os.getenv('APPDATA'):
                        homeLocation = os.getenv('APPDATA')

        homeDir = os.path.join(homeLocation, SNOWBERRY_HOME_DIR)
        
    homeDir = os.path.abspath(homeDir)

    # The home needs to be created if it doesn't exist yet.
    _createDir(homeDir)

    # Create the rest of the user directories if they don't exist yet.
    for p in [ADDONS, MANIFESTS, UNINSTALLED, CONF, LANG, PLUGINS, PROFILES,
              GRAPHICS, RUNTIME, SERVER_RUNTIME]:
        _createDir(getUserPath(p))


def isCaseSensitive():
    """Returns True if the file system is case sensitive."""

    return not host.isWindows() and not host.isMac()


def addBundlePath(path):
    """Add a path to the list of paths where files will be searched
    from.

    @param path The bundle contents path.
    """
    global bundlePaths
    bundlePaths.append(path)


def setCustomPath(path, custom):
    """Set a custom path.
    
    @param path The path to set (PROFILES, RUNTIME, CONF, etc.).
    
    @param custom The custom path to use.
    """
    customPaths[path] = custom


def getHomeDirectory():
    "Returns the current user's Snowberry home directory."
    return homeDir


def getSystemPath(which):
    """Returns the directory where the specified kind of data is
    stored.  System paths are assumed to be relative to the current
    working directory."""
    return os.path.abspath(which)


def getUserPath(which):
    """Returns the directory where the specified kind of data is
    stored for the current user.  The location of the user home
    directory depends on the operating system."""
    
    if customPaths.has_key(which):
        return customPaths[which]
    
    path = os.path.join(homeDir, which)

    # The system profiles directory and the user profiles directory can't
    # be the same.
    if which == PROFILES and path == getSystemPath(which):
        path = os.path.join(homeDir, 'user' + which)
    
    return path


def getBundlePaths(which):
    """Returns the directories where plugin bundles have their files
    of the specified kind."""
    return map(lambda p: os.path.join(p, which), bundlePaths)


def listPaths(which, userFirst=True):
    """Returns an array of search paths for the specified kind of files.
    """
    p = [getUserPath(which)] + getBundlePaths(which) + [getSystemPath(which)]
    if not userFirst:
        p.reverse()
    return p
               

def listFiles(which, userFirst=True):
    """Returns a list of all the files of the specified kind.

    @return An array of absolute file names.
    """
    files = []

    for path in listPaths(which, userFirst):
        try:
            for name in os.listdir(path):
                full = os.path.join(path, name)
                if full not in files:
                    files.append(os.path.abspath(full))
        except OSError:
            # The directory probably didn't exist.  We don't need to
            # do anything.
            pass

    files.sort()
    return files


def findBitmap(name):
    """Locates a bitmap file.  Files in the user's graphics directory
    override the contents of the system graphics directory.

    @param name Name of the bitmap without the file name extension.

    @return The full path to the bitmap file.
    """
    # First see if its in the user's graphics directory.
    for path in listPaths(GRAPHICS):
        for ext in ['jpg', 'bmp', 'png']:
            fileName = os.path.join(path, name + '.' + ext)
            if os.path.exists(fileName):
                return fileName

    # The file was not found.
    return ''


def getBase(path):
    """Returns the file name sans path and extension."""
    
    base = os.path.basename(path)
    pos = base.rfind('.')
    if pos < 0:
        # There is no extension.
        return base
    # Find the last dot.
    return base[:pos]


def hasExtension(extension, fileName):
    """Checks if the specified file name has the given extension.

    @param extension The extension to look for.

    @param fileName A file name.

    @return True, if the extension is found.
    """
    return re.search("^[^.#].*\." + extension + "$",
                     os.path.basename(fileName).lower()) != None


def quote(fileName):
    """Return the gives path in quotes so that it may be used as a
    command line option."""
    return '"' + fileName.replace('"', '""') + '"'


def fixSlashes(path):
    """Replace all forward and backward slashes with the correct
    slashes of the host OS."""  


def addAddonPath(path):
    """Define a new custom directory for addons.  Addons can be loaded
    from all the custom directories.  Installed addons always go to
    the default addons directory.

    @param path  Absolute path of the new custom addons directory.
    """
    if path not in addonPaths:
        addonPaths.append(path)


def removeAddonPath(path):
    """Remove a custom directory for addons.

    @param path  Absolute path of the custom addons directory.
    """
    if path in addonPaths:
        addonPaths.remove(path)
    else:
        logger.immediate(logger.HIGH, 'error-remove-addon-path-not-found', path)


def getAddonPaths():
    """Returns all the custom addon paths.

    @return An array containing full paths.
    """
    return [p for p in addonPaths]


def saveAddonPaths():
    """Write the custom addon paths to a configuration file."""

    fileName = os.path.join(getUserPath(CONF), 'addon-paths.conf')
    try:
        f = file(fileName, 'w')
        f.write('# This file is generated automatically.\n')
        f.write('configure addon-path (\n')
        for p in addonPaths:
            f.write('  readonly: %s\n' % p)
        f.write(')\n')
    except: 
        # Paths not saved.
        pass


def handleNotify(event):
    """When the <code>quit</code> notification is sent, save the
    custom addon paths."""

    if event.hasId('quit'):
        saveAddonPaths()


## When this module is initialized, check for the home directory.
## It is created automatically if it doesn't exist yet.
_checkSnowberryHome()

events.addNotifyListener(handleNotify, ['quit'])
