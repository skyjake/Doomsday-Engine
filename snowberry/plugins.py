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

## @file plugins.py Plugin Loader
##
## If a plugin init() raises an exception, the plugin is ignored.

import sys, os, re, string
import sb.confdb as st
import language, paths, logger

PLUGIN_PATH = 'plugins'

# An array of Python statements that will initialize all loaded
# plugins.
initCommands = []


def sortKey(name):
    return paths.getBase(name).lower()


def loadAll():
    "Load all the plugins from the plugins directory."

    global initCommands
    initCommands = []

    # We'll only use the modified sys.path during this function.
    oldPath = sys.path

    # Make the plugins directory the one where to look first.
    sys.path = [PLUGIN_PATH] + sys.path

    found = []

    for fileName in paths.listFiles(paths.PLUGINS):
        if paths.hasExtension('py', fileName) or \
           (paths.hasExtension('plugin', fileName) and os.path.isdir(fileName)):
            if fileName not in found:
                found.append(fileName)
                
    # Sort alphabetically across all located plugins.
    found.sort(key=sortKey)
    for fileName in found:
        if paths.hasExtension('py', fileName):
            loadSingle(fileName) # Single Python module.
        elif paths.hasExtension('plugin', fileName):            
            loadBundle(fileName) # A plugin bundle.

    # Import all plugin modules.
    for importStatement, initStatement, searchPath in initCommands:
        sys.path = searchPath
        try:
            exec importStatement
        except:
            logger.add(logger.HIGH, 'error-plugin-init-failed', importStatement,
                       logger.formatTraceback())

    # Initialize all plugins.
    for importStatement, initStatement, searchPath in initCommands:
        sys.path = searchPath
        try:
            exec initStatement
        except AttributeError, ex:
            if "'init'" not in str(ex):
                logger.add(logger.HIGH, 'error-plugin-init-failed', initStatement,
                           logger.formatTraceback())
        except:
            # Any problems during init cause the plugin to be ignored.
            logger.add(logger.HIGH, 'error-plugin-init-failed', initStatement,
                       logger.formatTraceback())
    
    sys.path = oldPath


def loadSingle(fileName, package=None):
    """Load a plugin that consists of a single .py module in a plugins
    directory.

    @param fileName File name of the the plugin.
    """
    name = paths.getBase(fileName)

    obsoletePluginNames = ['tab1_summary', 'tab2_addons', 'tab3_settings', 'tab30_addons']
    
    # Old plugin names.
    if name in obsoletePluginNames:
        logger.add(logger.MEDIUM, 'warning-obsolete-plugin', name, fileName)
    
    sysPath = sys.path
    if os.path.dirname(fileName) not in sysPath:
        sysPath = [os.path.dirname(fileName)] + sys.path   
        
    if package:
        prefix = package + '.'
    else:
        prefix = ''
   
    # Initialization will be done after everything has been loaded.
    initCommands.append(('import ' + prefix + name, prefix + name + '.init()', sysPath))


def loadBundle(path):
    """Load a plugin bundle that may contain multiple .py modules,
    plus directories.  The bundle directories will be included in file
    searching in paths.py.

    @param path The location of the plugin bundle.
    """
    contentsPath = os.path.join(path, 'Contents')

    # Include the bundle contents in the module search path (for the
    # duration of this function).
    oldSysPath = sys.path
    sys.path = [contentsPath] + sys.path
    
    # Register the bundle path into paths.py.
    paths.addBundlePath(contentsPath)

    # Make the appropriate calls to read the configuration and
    # language files in the bundle.
    readBundleConfig(contentsPath)

    # Load all of the .py modules in the bundle's Python package.
    package = paths.getBase(path)
    packagePath = os.path.join(contentsPath, package)
    for name in os.listdir(packagePath):
        fullName = os.path.join(packagePath, name)
        if name != '__init__.py' and paths.hasExtension('py', fullName):
            loadSingle(fullName, package)

    # Restore the previous module search path.
    sys.path = oldSysPath


def readBundleConfig(path):
    st.readConfigPath(os.path.join(path, 'conf'))
    language.loadPath(os.path.join(path, 'lang'))
