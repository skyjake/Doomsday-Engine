# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2006
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#   Veikko Eeva
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

## @file aodb.py Addon Database
##
## This module handles the functionality related to addons including,
## for instance, database administration, installing and un-installing
## addons.

import os, re, string, zipfile, shutil, struct
import logger, events, paths, language, cfparser
import addon as ao
import sb.profdb

# This is a list of all the categories that include exclusions regarding
# the other categories. That is, some addons cannot be included in Doomsday
# Engine launch parameters at the same time.
categories = []
rootCategory = None

# The addon dictionary and the metadata cache.  Maps addon identifiers
# to Addon objects.
addons = {}


class Category:
    """The category tree is built automatically based on the loaded
    addons."""

    def __init__(self, id, parent=None):
        self.id = id
        self.parent = parent

    def getId(self):
        return self.id

    def getLongId(self):
        """The long identifier includes the identifiers of the parent
        categories."""
        
        longId = self.id
        p = self.parent
        while p:
            longId = p.id + '-' + longId
            p = p.parent
        return 'category' + longId

    def getParent(self):
        """Returns the parent category.

        @param A Category object.
        """
        return self.parent

    def getName(self):
        """Returns the user-visible name of the category.

        @param A string.  The translated name of the category.
        """
        return language.translate(self.getLongId())

    def isAncestorOf(self, subCategory):
        """Checks if this Category is an ancestor of another category.
        Also true if the categories are the same object.

        @param subCategory The potential subcategory.

        @return True, if subCategory is a descendant of this category.
        """
        i = subCategory
        while i:
            if i is self:
                return True
            i = i.getParent()
        return False

    def getPath(self):
        """Returns the category in the path notation: aaa/bbb/ccc.

        @return A string.
        """
        path = ''
        iter = self
        while iter.getParent():
            path = '/' + iter.getId() + path
            iter = iter.getParent()
        return path


def _getLatestModTime(startPath):
    """Descends recursively into the subdirectories in startPath to
    find out which file has the latest modification time.

    @return The latest modification time.
    """
    latest = 0

    for f in os.listdir(startPath):
        path = os.path.join(startPath, f)
        if os.path.isdir(path):
            # Check which is the latest file in this subdirectory.
            latest = max(latest, _getLatestModTime(path))
        else:
            latest = max(latest, os.path.getmtime(path))

    return latest


def _newCategory(identifier, parent):
    """Append a new Category object into the array of categories.
    This is only done if the specified category does not already
    exist.  This shouldn't be called directly.  Call _getCategory()
    instead.

    @param category A Category object.

    @return The real Category object that must be used.  This may be
    different than the provided Category object, since the category
    may already exist.
    """
    for c in categories:
        if c.getId() == identifier and c.getParent() is parent:
            # This already exists.
            return c

    # Construct a new category object.
    c = Category(identifier, parent)
    categories.append(c)
    return c


def _getCategory(name, prefix=None):
    """Finds the Category object that corresponds the specified
    category path.  If the object doesn't exist yet, it will be
    created.  The categories form a tree hierarchy.

    @param The category path.  Separate identifiers with slashes.
    """
    trimmed = name.lower()

    # Remove any spaces and a trailing/initial slash.
    while trimmed.startswith('/'):
        trimmed = trimmed[1:]
    while trimmed.endswith('/'):
        trimmed = trimmed[:-1]

    # Get rid of all duplicate slashes.
    while True:
        cleaned = trimmed.replace('//', '/')
        if cleaned == trimmed:
            break
        trimmed = cleaned

    if len(trimmed) == 0:
        return rootCategory

    # Get the category identifiers in the path, stripped of
    # whitespace.
    idents = map(lambda a: a.strip(), trimmed.split('/'))

    cat = rootCategory

    # Build the category objects in the path, if they don't yet exist.
    for i in idents:
        # Create the new categories and make sure it's in the category
        # tree.
        newCat = _newCategory(i, cat)
        cat = newCat

    return cat


def getAddonExtensions():
    """Returns a list of all possible file name extensions for addons."""
    return ao.EXTENSIONS


def getCategory(longId):
    """Finds a Category by its long identifier.
    
    @param longId  Long category identifier.
    
    @return Category, or None.
    """
    for c in categories:
        if c.getLongId() == longId:
            return c
    return None


def getRootCategory():
    """Returns the root category.

    @return A Category object."""
    return rootCategory


def getSubCategories(parent):
    """Return the sub-categories of a parent category.

    @param parent A Category object.

    @return An array of Category objects.
    """
    return [c for c in categories if c.getParent() is parent]


def getAvailableAddons(profile):
    """Get all addons compatible with the profile.

    @return An array of Addon objects.
    """
    import sb.profdb
    return [a for a in addons.values()
            if profile is sb.profdb.getDefaults() or a.isCompatibleWith(profile)]


def getAddonCount():
    """Returns the number of addons in the addon database."""
    return len(addons)


def getAddons(func=None):
    """Return the addons in a given category.

    @param func If this function returns True, the addon will be
    returned.

    @return An array of Addon objects.
    """
    return [a for a in addons.values() if func == None or func(a)]


def sortIdentifiersByName(addonIdentifiers):
    """Sort a list of addon identifiers based on their visible names
    in the current language."""
    addonIdentifiers.sort(lambda a, b: cmp(language.translate(a),
                                           language.translate(b)))


def exists(addonId):
    return addons.has_key(addonId)


def get(addonId):
    """Returns the specified addon.

    @param addonId A string identifier.

    @return An Addon object.
    """
    return addons[addonId]


def install(sourceName):
    """Installs the specified addon into the user's addon database.

    @param fileName Full path of the addon to install.

    @throw AddonFormatError If the addon is not one of the recognized
    formats, this exception is raised.

    @return Identifier of the installed addon.

    @throws Exception The installation failed.
    """
    isManifest = paths.hasExtension('manifest', sourceName)
    
    if isManifest:
        # Manifests are copied to the manifest directory.
        destPath = paths.getUserPath(paths.MANIFESTS)
    else:
        destPath = paths.getUserPath(paths.ADDONS)

    destName = os.path.join(destPath,
                            os.path.basename(sourceName))
    
    try:
        # Directories must be copied as a tree.
        if os.path.isdir(sourceName):
            shutil.copytree(sourceName, destName)
        else:
            shutil.copy2(sourceName, destName)

        # Load the new addon.
        if not isManifest:
            identifier = load(destName)
        else:
            identifier = loadManifest(destName)

        # Now that the addon has been installed, save the updated cache.
        writeMetaCache()

        # Send a notification of the new addon.
        notify = events.Notify('addon-installed')
        notify.addon = identifier
        events.send(notify)

    except Exception, x:
        # Unsuccessful.
        raise x

    return identifier


def isUninstallable(identifier):
    """Determines whether the addon is uninstallable.
    @param identifier  Addon identifier.
    @return Bool."""
    return exists(identifier) and get(identifier).getBox() == None
    

def uninstall(identifier, doNotify=False):
    """Uninstall an addon.  Uninstalling an addon causes a full reload
    of the entire addon database.
    
    @param identifier  Identifier of the addon to uninstall.
    @param doNotify  True, if a notification should be sent.
    """
    addon = get(identifier)
    path = addon.getSource()
    destPath = os.path.join(paths.getUserPath(paths.UNINSTALLED),
                            os.path.basename(path))

    if os.path.exists(destPath):
        # Simply remove any existing addons with the same name.
        # TODO: Is this wise? Rename old.
        if os.path.isdir(destPath):
            shutil.rmtree(destPath)
        else:
            os.remove(destPath)

    shutil.move(path, destPath)

    # Is there a manifest to uninstall?
    manifest = addon.getId() + '.manifest'
    manifestFile = os.path.join(paths.getUserPath(paths.MANIFESTS), manifest)
    if os.path.exists(manifestFile):
        # Move it to the uninstalled folder.
        destPath = os.path.join(paths.getUserPath(paths.UNINSTALLED),
                                manifest)
        if os.path.exists(destPath):
            os.remove(destPath)
        shutil.move(manifestFile, destPath)

    # Mark as uninstalled.
    addon.uninstall()

    # Detach from all profiles.
        
    for p in sb.profdb.getProfiles():
        p.dontUseAddon(identifier)

    # Send a notification.
    if doNotify:
        notify = events.Notify('addon-uninstalled')
        notify.addon = identifier
        events.send(notify)   


def formIdentifier(fileName):
    """Forms the identifier of the addon based on the addon's file name.

    @param fileName  File name of the addon.

    @return  Tuple: (identifier, effective extension)
    """
    baseName = os.path.basename(fileName)
    found = re.search("(?i)(.*)\.(" + \
                      string.join(ao.EXTENSIONS, '|') + ")$", baseName)
    if not found:
        return ('', '')
    
    identifier = found.group(1).lower()
    extension = found.group(2).lower()
    if extension == 'addon':
        extension = 'bundle'

    # The identifier also includes the type of the addon.
    identifier += "-" + extension
    
    # Replace spaces with dashes.
    identifier = identifier.replace(' ', '-')
    
    # Replace reserved characters with underscores.
    identifier = identifier.replace('(', '_')
    identifier = identifier.replace(')', '_')
    identifier = identifier.replace('>', '_')
    identifier = identifier.replace('<', '_')
    identifier = identifier.replace(',', '_')

    return (identifier, extension)


class LoadError (Exception):
    """Exception that is raised when the loading of an addon fails."""
    def __init__(self, msg=''):
        Exception.__init__(self, msg)


def load(fileName, containingBox=None):
    """Loads the specific addon.  If its metadata hasn't yet been
    cached, it will be retrieved and stored into the data cache.  All
    addons in the system and user databases are loaded on program
    startup.

    @param fileName Full path of the addon to load.

    @param containingBox Optionally the BoxAddon that contains the
    loaded addon.

    @return The identifier of the loaded addon.
    """
    # Form the identifier of the addon.
    identifier, extension = formIdentifier(fileName)

    addon = None
    
    # Construct a new Addon object based on the file name extension.
    if extension == 'box':
        addon = ao.BoxAddon(identifier, fileName)
    elif extension == 'bundle':
        addon = ao.BundleAddon(identifier, fileName)
    elif extension == 'pk3' or extension == 'zip':
        addon = ao.PK3Addon(identifier, fileName)
    elif extension == 'wad':
        addon = ao.WADAddon(identifier, fileName)
    elif extension == 'ded':
        addon = ao.DEDAddon(identifier, fileName)
    elif extension == 'deh':
        addon = ao.DehackedAddon(identifier, fileName)
    elif extension == 'lmp':
        addon = ao.LumpAddon(identifier, fileName)
    else:
        raise LoadError('unknown format')

    # Set the container.
    if containingBox:
        addon.setBox(containingBox)

    # Check against the current data in the cache.  Is it recent
    # enough?
    lastModified = addon.getLastModified()

    if (not addons.has_key(identifier) or
        addons[identifier].getLastModified() != lastModified):

        # The metadata needs to be updated.
        addon.readMetaData()

        # This Addon object contains the latest metadata.
        addons[identifier] = addon

    # If the loaded addon is a box, also load the contents.
    if extension == 'box':
        addon.loadContents()

    # TODO: Define the strings of this addon in the language.

    return identifier


def loadManifest(fileName):
    """Manifests contain metadata for other addons.  A manifest may
    augment or replace the metadata of an existing addon, or define an
    entirely new addon.

    @param fileName  Path of the manifest file.

    @return  Identifier of the addon the manifest is associated with.
    """
    identifier = paths.getBase(fileName)

    if exists(identifier):
        a = get(identifier)
    else:
        # Create a new addon.
        a = ao.Addon(identifier, fileName)
        addons[identifier] = a

    # The manifest contains metadata configuration.
    try:
        a.parseConfiguration(file(fileName).read())
        
    except Exception, x:
        logger.add(logger.HIGH, 'error-read-manifest', fileName, str(x))

    return identifier


def readMetaCache():
    """The metadata cache contains the metadata extracted from all the
    installed addons.  The cache is stored in the user's addon
    database directory.  The entries whose source file is missing will
    be ignored.
    """


def writeMetaCache():
    """Write the metadata cache to disk.
    """


def findConflicts(roster, profile):
    """Find conflicts in the given list of addon identifiers.  This
    function only finds any conflicts that exist, and then returns a
    tuple describing what must be done to resolve the conflict.  This
    is called repeatedly by the launcher plugin until no more
    conflicts are found.

    @param roster An array of Addon objects.  The addons with the
    highest priority are in the end of the list (first in the load
    order; overrides the addons loaded earlier).

    @param profile The profile that is used with the addons.  The
    values of settings are taken for here.

    @return An empty list if no conflicts were found.  Otherwise a list
    of conflict reports (as tuples).
    """
    rosterSize = len(roster)

    result = []

    # Get the keywords defined by the profile's values.
    settingKeywords = profile.getKeywords()
    
    # Collect a list of all the keywords that are provided and
    # offered.
    allKeywords = []
    for a in roster:
        allKeywords += a.getKeywords(ao.PROVIDES) + \
                       a.getKeywords(ao.OFFERS)

    # Required keywords.
    for a in roster:

        missing = []
        
        for req in a.getKeywords(ao.REQUIRES):
            if req not in settingKeywords and req not in allKeywords:
                missing.append(req)

        if missing:
            result.append(('missing-requirements', a, missing))

    if result:
        return result        
    
    # Category exclusion.
    #for i in range(rosterSize):
    #    a = roster[i]
    for a in roster:

        exclusions = []
        
        # Does this addon exclude any of the other addons in the
        # roster?
        #for j in range(i + 1, rosterSize):
        #    b = roster[j]
        for b in roster:
            if a is b:
                continue
            
            for category in a.getExcludedCategories():
                if category.isAncestorOf(b.getCategory()):
                    # This is a conflict: a excludes b.
                    exclusions.append(b)
                    break

        if exclusions:
            result.append(('exclusion-by-category', a, exclusions))

    # If category exclusions were found, we won't check any further.
    if result:
        return result

    # Exclusion by keyword.
    for a in roster:

        exclusions = []

        for x in a.getKeywords(ao.EXCLUDES):
            if x in settingKeywords:
                exclusions.append(x)

        if exclusions:
            result.append(('exclusion-by-value', a, exclusions))

        exclusions = []

        # Does this addon exclude any of the other addons in the
        # roster, using keywords?
        for b in roster:
            if a is b:
                continue
            for x in a.getKeywords(ao.EXCLUDES):
                if b.getId() == x or x in b.getKeywords(ao.PROVIDES) or \
                   x in b.getKeywords(ao.OFFERS):
                    exclusions.append((b, x))

        if exclusions:
            result.append(('exclusion-by-keyword', a, exclusions))
        
    # If exclusions were found, we won't check any further.
    if result:
        return result

    # Check for provide conflicts.  Two addons can't provide the
    # keyword.
    for i in range(rosterSize):
        a = roster[i]

        conflicts = []

        for j in range(i + 1, rosterSize):
            b = roster[j]

            for prov in a.getKeywords(ao.PROVIDES):
                if prov in b.getKeywords(ao.PROVIDES):
                    conflicts.append((a, b, prov))

        if conflicts:
            result.append(('provide-conflict', conflicts))

    if result:
        return result

    # Check for offers.  Overrides should be resolved automatically.
    for a in roster:
        overrides = []
        for b in roster:
            if a is b:
                continue
            
            # Does a provide what b only offers?
            for prov in a.getKeywords(ao.PROVIDES):
                if prov in b.getKeywords(ao.OFFERS):
                    if b not in overrides:
                        overrides.append((b, prov))
                    break

        if overrides:
            result.append(('override', a, overrides))

    if result:
        return result

    for i in range(rosterSize):
        a = roster[i]

        overrides = []

        for j in range(i + 1, rosterSize):
            b = roster[j]

            # Does a offer the same thing that b offers?
            for offer in a.getKeywords(ao.OFFERS):
                if offer in b.getKeywords(ao.OFFERS):
                    if b not in overrides:
                        overrides.append((b, offer))

        if overrides:
            result.append(('override', a, overrides))

    if result:
        return result

    # No conflicts found.  Yay!
    return []


def loadAll():
    """Load all addons and manifests."""
    
    # Load all the installed addons in the system and user addon
    # databases.
    for repository in [paths.ADDONS] + paths.getAddonPaths():
        for name in paths.listFiles(repository):
            # Case insensitive.
            if re.search("(?i)^([^.#].*)\.(" + \
                         string.join(ao.EXTENSIONS, '|') + ")$",
                         os.path.basename(name)):
                load(name)

    # Load all the manifests.
    for folder in [paths.getSystemPath(paths.MANIFESTS),
                   paths.getUserPath(paths.MANIFESTS)] + \
                   paths.getAddonPaths():
        for name in paths.listFiles(folder):
            # Case insensitive.
            if paths.hasExtension('manifest', name):
                loadManifest(name)
                
                
def refresh():
    """Empties the data of all loaded addons from the database and reloads
    everything. Sends out a 'addon-database-reloaded' notification."""
    
    init()   
    loadAll()
    events.send(events.Notify('addon-database-reloaded'))
    
    
def init():
    """Initialize the addon database."""
    
    global categories, rootCategory, addons
    
    # Empty the cache.
    categories = []
    addons = {}

    # Create the root category (for unclassified/generic addons).
    rootCategory = Category('')
    categories.append(rootCategory)
    

#
# Module Initialization
#

# Load the metadata cache.
init()
readMetaCache()

loadAll()

# Save the updated cache.
writeMetaCache()
