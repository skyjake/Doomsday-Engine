# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
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

## @file addons.py Addon Management
##
## This module handles the functionality related to addons including,
## for instance, database administration, installing and un-installing
## addons.

import os, re, string, zipfile, shutil, struct
import logger, events, paths, profiles as pr, language, cfparser
import settings as st

# This is a list of all the categories that include exclusions regarding
# the other categories. That is, some addons cannot be included in Doomsday
# Engine launch parameters at the same time.
categories = []
rootCategory = None

# The addon dictionary and the metadata cache.  Maps addon identifiers
# to Addon objects.
addons = {}

# File name extensions of the supported addon formats.
ADDON_EXTENSIONS = ['box', 'addon', 'pk3', 'zip', 'lmp', 'wad', 'ded', 'deh']

# All the possible names of the addon metadata file.
META_NAMES = ['Info', 'Info.conf']
if paths.isCaseSensitive():
    META_NAMES += ['info', 'info.conf']
    

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


class Addon:
    """Addon class serves as an abstract base class to the concrete
    addon implementations."""

    EXCLUDES = 0
    REQUIRES = 1
    PROVIDES = 2
    OFFERS = 3

    def __init__(self, identifier, source):
        """Construct a new Addon object.

        @param identifier The identifier of the addon.

        @param source The full path name of the source file.
        """
        self.id = identifier
        self.name = ''
        self.category = rootCategory
        self.source = source
        self.priority = 'z'
        self.requiredComponents = []
        self.excludedCategories = []
        self.box = None

        # When an addon is uninstalled, it will be hidden in the user
        # interface. 
        self.uninstalled = False

        # Contains tuples: (type, identifier).
        self.keywords = {}
        for t in [Addon.EXCLUDES, Addon.REQUIRES, Addon.PROVIDES,
                  Addon.OFFERS]:
            self.keywords[t] = []

        # Every addon provides itself.
        self.keywords[Addon.PROVIDES].append(identifier)
        
        self.lastModified = None

        # If no other translation is given, the identifier of the
        # addon is shown as the base name of the source.
        language.define('english', identifier, os.path.basename(source))

    def getId(self):
        return self.id

    def getType(self):
        """Returns the type identifier of the addon class."""

        return 'addon-type-generic'

    def getName(self):
        return self.name

    def setName(self, name):
        self.name = name

    def getSource(self):
        """The source path is the location of the addon.

        @return The source path of the addon as a string.
        """
        return self.source

    def getCategory(self):
        return self.category

    def getExcludedCategories(self):
        return self.excludedCategories

    def makeLocalId(self, identifier):
        return self.id + '-' + identifier

    def getContentPath(self):
        """The content path is the location on the file system where
        the contents of the addon are stored.  This is not meaningful
        with all addons.
        """
        return self.source

    def getLastModified(self):
        """Returns the time when the addon data was last modified.
        This is used to determine if the metadata needs to be updated.
        """
        if self.isUninstalled():
            # Uninstalled addons don't have a meaningful last modified
            # timestamp.
            return 0
        
        if self.lastModified == None:
            self.lastModified = self.determineLastModified()

        return self.lastModified

    def determineLastModified(self):
        """Read the source file(s) to see when they've been modified
        last.  This implementation assumes the source is a single
        file.

        @return The last modification time.
        """
        try:
            return os.path.getmtime(self.source)
        except OSError:
            return 0

    def setBox(self, box):
        """Set the BoxAddon this addon is inside of.

        @param box A BoxAddon object."""

        self.box = box

    def getBox(self):
        """Returns the box of this addon.

        @param BoxAddon object.  May be None.
        """

        return self.box

    def isBox(self):
        return False

    def isUninstalled(self):
        return self.uninstalled

    def uninstall(self):
        """Mark this addon as uninstalled.  It will be hidden in the
        user interface."""
        
        self.uninstalled = True

    def isVisible(self):
        """All visible addons are listen in the addons tree.  The
        required parts of a box are not visible, as they are loaded
        automatically if the box itself is selected for loading."""

        if self.isUninstalled():
            # Uninstalled addons are not visible.
            return False

        if not self.getBox():
            return True

        return not self.getBox().isRequired(self.getId())

    def isOptIn(self, contextProfile):
        """Opt-in addons must be attached to a profile to become used.

        @param contextProfile The profiles.Profile object that will be
        used as the context.
        """
        defaults = pr.getDefaults()

        optIn = True

        if contextProfile is not defaults:
            if self.getId() in defaults.getAddons():
                # This addon is attached to the Defaults.
                optIn = not optIn

        if self.isInversed():
            optIn = not optIn

        return optIn

    def isOptOut(self, contextProfile):
        """Opt-out addons must be detached from a profile to become
        used.

        @param contextProfile The profiles.Profile object that will be
        used as the context.
        """
        
        return not self.isOptIn(self, contextProfile)

    def isInversed(self):
        """Inversed addons are used when not attached, and not used
        when attached to a profile.  For example: optional parts of a
        box."""

        if not self.getBox():
            return False

        # Optional parts of boxes.
        if self.getBox().isOptional(self.getId()):
            return True

        return False

    def setPriority(self, priorityClass):
        """The priority class is an ASCII character.

        @param priorityClass Expected to be a character between A and
        Z (inclusive).
        """
        self.priority = priorityClass.lower()

    def getPriority(self):
        return self.priority

    def readMetaData(self):
        """Read the source file(s) and read/generate the metadata for
        the addon.  The data is stored in the Addon object.  This
        implementation in the base class handles the data is common to
        all addons.
        """
        pass

    def getCommandLine(self, profile):
        """Return the command line options to load and configure the
        addon.  This basic implementation assumes no configurability
        and a single source file.
        """
        if not paths.hasExtension('manifest', self.source):
            return '-file ' + paths.quote(self.source)
        else:
            return ''

    def addKeywords(self, type, identifiers):
        """Define a new keyword for the addon.

        @param type One of the constants:
        - EXCLUDES: Identifier must not be used by anyone else.
        - REQUIRES: Identifier must be used by someone else.
        - PROVIDES: Identifier is provided by this addon.
        - OFFERS: Identifier is provided, unless a higher priority
          addon provides or offers the identifier.

        @param identifiers An array of identifiers.  These identifiers
        live in their own namespace, but the values of settings are
        also considered keywords.
        """
        self.keywords[type] += identifiers

    def getKeywords(self, type):
        return self.keywords[type]    

    def isCompatibleWith(self, profile):
        """Tests if this addon is compatible with the profile.

        @return Boolean.  True, if compatible.
        """
        if self.isUninstalled():
            return False
        
        # The Defaults profile is compatible with everything.
        if profile is pr.getDefaults():
            return True

        # Check the required components.  The profile must have all of
        # them.
        profComps = profile.getComponents()
        for req in self.requiredComponents:
            if req not in profComps:
                return False

        return True

    def parseConfiguration(self, configSource):
        """Parse the given configuration data and process it as this
        addon's metadata.

        @param configSource The raw configuration data.
        """
        try:
            p = cfparser.Parser(configSource)

            while True:
                elem = p.get()

                if elem.isBlock() and elem.getType() == 'language':
                    language.processLanguageBlock(elem, self.getId())

                elif elem.isKey() and elem.getName() == 'name':
                    self.setName(elem.getValue())
                    
                    # A shortcut for defining the name of the addon.
                    language.define('english', self.getId(), self.getName())
                    
                    # Automatically define the name of a box part.
                    if self.getBox():
                        language.define('english',
                                        self.getBox().getId() + '-' +
                                        self.getId(), self.getName())
                    
                elif elem.getName() == 'component':
                    self.requiredComponents += elem.getContents()

                elif elem.isKey() and elem.getName() == 'category':
                    # The category of an addon inside a box is
                    # relative to the box.
                    if self.getBox():
                        catPath = self.getBox().getCategory().getPath() + \
                                  '/' + self.getBox().getId() + '/'
                    else:
                        catPath = ''
                    catPath += elem.getValue()
                    self.category = _getCategory(catPath)

                elif elem.isKey() and elem.getName() == 'order':
                    self.setPriority(elem.getValue())

                elif elem.getName() == 'excludes-category':
                    identifiers = elem.getContents()
                    for id in identifiers:
                        self.excludedCategories.append(_getCategory(id))

                elif elem.getName() == 'excludes':
                    self.addKeywords(Addon.EXCLUDES, elem.getContents())

                elif elem.getName() == 'requires':
                    self.addKeywords(Addon.REQUIRES, elem.getContents())
                    
                elif elem.getName() == 'provides':
                    self.addKeywords(Addon.PROVIDES, elem.getContents())

                elif elem.getName() == 'offers':
                    self.addKeywords(Addon.OFFERS, elem.getContents())

                elif elem.isBlock():
                    # Convert the identifier to local form.
                    elem.setName(self.makeLocalId(elem.getName()))

                    # Determine the owner of this setting.
                    if self.getBox() == None:
                        ownerId = self.getId()
                    else:
                        ownerId = self.getBox().getId()
                        # The addon's identifier determines the group.
                        elem.add(cfparser.KeyElement('group', self.getId()))
                    elem.add(cfparser.KeyElement('addon', ownerId))
                    st.processSettingBlock(elem)

        except cfparser.OutOfElements:
            pass


class BoxAddon (Addon):
    """A Box groups other addons together.  A box can't contain
    another box."""

    def __init__(self, identifier, source):
        Addon.__init__(self, identifier, source)

        # Register the source directory into the paths.
        paths.addBundlePath(self.getContentPath())

        # Lists of addon identifiers.  These are the parts of the box.
        self.required = []
        self.optional = []
        self.extra = []

    def getType(self):
        return 'addon-type-box'

    def isBox(self):
        return True

    def isRequired(self, addonIdentifier):
        return addonIdentifier in self.required

    def isOptional(self, addonIdentifier):
        return addonIdentifier in self.optional

    def getRequiredParts(self):
        return self.required

    def __makePath(self, name):
        return os.path.join(self.getContentPath(), name)

    def uninstall(self):
        """Mark the box and its contents uninstalled."""

        Addon.uninstall(self)

        for ident in self.required + self.optional + self.extra:
            get(ident).uninstall()

    def loadContents(self):
        """Load all the addons inside the box."""
        
        self.loadAll(self.getContentPath(), self.optional)
        self.loadAll(self.__makePath('Required'), self.required)
        self.loadAll(self.__makePath('Extra'), self.extra)
        if paths.isCaseSensitive():
            self.loadAll(self.__makePath('required'), self.required)
            self.loadAll(self.__makePath('extra'), self.extra)

    def loadAll(self, path, role):
        """Load all addons on the given path."""

        if not os.path.exists(path):
            return

        for fileName in os.listdir(path):
            try:
                ident = load(os.path.join(path, fileName), self)
                role.append(ident)

                # Using any parts of an addon requires that the box
                # itself is marked for loading.
                get(ident).addKeywords(Addon.REQUIRES, [self.getId()])

            except Exception, x:
                # Perhaps it wasn't an addon?
                #print ident
                #import traceback
                #traceback.print_exc()
                #print x
                pass
            
    def readMetaData(self):
        """Read the metadata files (e.g. Info)."""

        try:
            for info in META_NAMES:
                try:
                    name = os.path.join(self.getContentPath(), info)
                    conf = file(name).read()
                    self.parseConfiguration(conf)
                except OSError:
                    pass
                except IOError:
                    pass
                except:
                    logger.add(logger.HIGH, 'error-read-info-file',
                               name, self.getId())
        except:
            pass

        # Load a readme from a separate file.
        readme = self.__makePath('Readme.html')
        if not os.path.exists(readme):
            readme = self.__makePath('readme.html')
        if os.path.exists(readme):
            language.define('english',
                            self.makeLocalId('readme'),
                            file(readme).read())
                            
    def getCommandLine(self, profile):
        """Returns the command line that is given to Doomsday Engine.

        @param profile The profile for which the command line is being
        generated.
        @return String The generated command line.
        """
        return ""
                            

class BundleAddon (Addon):
    """The Bundle is the most sophicated of the addons.  It is a
    directory that follows a specific struture."""

    def __init__(self, identifier, source):
        Addon.__init__(self, identifier, source)

        # Register the source directory into the paths.
        paths.addBundlePath(self.getContentPath())

    def getType(self):
        return 'addon-type-bundle'

    def determineLastModified(self):
        """Read the source file(s) to see when they've been modified
        last.  This implementation assumes the source is a single
        file.

        @return The last modification time.
        """
        return _getLatestModTime(self.source)

    def __makePath(self, fileName):
        """Make a path name for a file inside the bundle."""
        return os.path.join(self.source, 'Contents', fileName)

    def getContentPath(self):
        """The content path of a bundle is well-defined.
        """
        return os.path.join(self.source, 'Contents')

    def readMetaData(self):
        """Read the metadata files (e.g. Contents/Info)."""

        try:
            for info in META_NAMES:
                try:
                    conf = file(self.__makePath(info)).read()
                    self.parseConfiguration(conf)
                except OSError:
                    pass
                except IOError:
                    pass
                except Exception, x:
                    logger.add(logger.HIGH, 'error-read-info-file',
                               self.__makePath(info), self.getId())
        except:
            # Log a warning?
            pass

        # Load a readme from a separate file.
        readme = self.__makePath('Readme.html')
        if not os.path.exists(readme):
            readme = self.__makePath('readme.html')
        if os.path.exists(readme):
            language.define('english',
                            self.makeLocalId('readme'),
                            file(readme).read())

    def getCommandLine(self, profile):
        """Bundles are loaded with -file and -def.  We'll load the
        contents of the Defs and Data directories.
        """
        param = ''

        defsPath = self.__makePath('Defs')
        if os.path.exists(defsPath):
            defs = []

            # Only include the files that have the .ded extension.
            for name in os.listdir(defsPath):
                path = os.path.join(defsPath, name)
                if paths.hasExtension('ded', path):
                    defs.append(paths.quote(path))

            if len(defs) > 0:
                # At least one definition file was found.
                param += '-def ' + string.join(defs)

        dataPath = self.__makePath('Data')
        if os.path.exists(dataPath):
            data = []

            # Only include the files with known extensions.
            for name in os.listdir(dataPath):
                path = os.path.join(dataPath, name)
                if paths.hasExtension('pk3', path) or \
                   paths.hasExtension('wad', path) or \
                   paths.hasExtension('lmp', path):
                    data.append(paths.quote(path))

            if len(data):
                # At least one data file was found.
                param += ' -file ' + string.join(data)

        return param


class PK3Addon (Addon):
    def __init__(self, identifier, source):
        Addon.__init__(self, identifier, source)

    def getType(self):
        return 'addon-type-pk3'

    def readMetaData(self):
        """Read the metadata file(s)."""

        # Check if there is an Info lump.
        try:
            z = zipfile.ZipFile(self.source)
            for info in META_NAMES:
                try:
                    self.parseConfiguration(z.read(info))
                except KeyError:
                    pass
                except:
                    logger.add(logger.HIGH, 'error-read-zip-info-file',
                               self.source, self.getId())

        except:
            # Log a warning?
            print "%s: Failed to open archive." % self.source


class WADAddon (Addon):
    def __init__(self, identifier, source):
        Addon.__init__(self, identifier, source)

        # We assume this is a PWAD until closer inspection.
        self.wadType = 'PWAD'
        self.lumps = []

    def getType(self):
        return 'addon-type-wad'

    def readMetaData(self):
        """Generate metadata by making guesses based on the WAD file
        name and contents."""

        metadata = ''

        # Defaults.
        game = ''

        # Look at the path where the WAD file is located.
        path = self.getContentPath().lower()

        if 'heretic' in path or 'htic' in path:
            game = 'jheretic'

        elif 'hexen' in path or 'hxn' in path or 'hexendk' in path or \
             'deathkings' in path:
            game = 'jhexen'

        elif 'doom' in path or 'doom2' in path or 'final' in path or \
             'finaldoom' in path or 'tnt' in path or 'plutonia' in path:
            game = 'jdoom'

        # Read the WAD directory.
        self.lumps = self.readLumpDirectory()
        lumps = self.lumps

        # What can be determined by looking at the lump names?
        for lump in lumps:
            if lump[:2] == 'D_':
                # Doom music lump.
                game = 'jdoom'

        if 'BEHAVIOR' in lumps or 'MAPINFO' in lumps:
            # Hexen-specific lumps.
            game = 'jhexen'

        if 'ADVISOR' in lumps or 'M_HTIC' in lumps:
            game = 'jheretic'

        if 'M_DOOM' in lumps:
            game = 'jdoom'

        episodic = False
        maps = False

        # Are there any ExMy maps in the WAD?
        for lump in lumps:
            if len(lump) == 4 and lump[0] == 'E' and lump[2] == 'M' and \
               lump[1] in string.digits and lump[3] in string.digits:
                episodic = True

                # Episodes 5 and 6 exist in Heretic.
                if lump[1] == '5' or lump[1] == '6':
                    game = 'jheretic'

        # Are there any MAPxy in the WAD?
        for lump in lumps:
            if len(lump) == 5 and lump[:3] == 'MAP' and \
               lump[3] in string.digits and lump[4] in string.digits:
                maps = True
                break

        if episodic and game == 'jhexen':
            # Guessed wrong.
            game = ''

        if maps and game == 'jheretic':
            # Guessed wrong.
            game = ''

        # Determine a category for the WAD.
        if self.wadType == 'IWAD':
            metadata += "category: gamedata/primary\n"
        elif self.wadType == 'JWAD':
            metadata += "category: gamedata/doomsday\n"
        elif self.wadType == 'PWAD':
            category = 'gamedata/'

            if maps or episodic:
                category += 'maps/'

                # Category based on the name.
                base = paths.getBase(self.getContentPath()).lower()

                if base[0] in string.digits:
                    category += '09/'
                if base[0] in 'abcdefg':
                    category += 'ag/'
                if base[0] in 'hijklm':
                    category += 'hm/'
                if base[0] in 'nopqrs':
                    category += 'ns/'
                if base[0] in 'tuvwxyz':
                    category += 'tz/'
            
            metadata += "category: %s\n" % category

        # Game component.
        if game != '':
            metadata += "component: game-%s\n" % game

        self.parseConfiguration(metadata)

    def readLumpDirectory(self):
        """Open the WAD file and read the lump directory at the end of
        the file."""

        lumps = []

        try:
            f = file(self.getContentPath(), 'rb')

            # Type of the WAD file. This affects categorization.
            self.wadType = f.read(4)

            count, dirOffset = struct.unpack('II', f.read(8))
            
            f.seek(dirOffset)

            while count > 0:
                # Pos and size ignored for now.
                #filePos, lumpSize = struct.unpack('II', f.read(8))
                f.seek(8, 1)

                # Make sure the name is in upper case.
                name = f.read(8).upper()
                if len(name) != 8:
                    break
                
                for i in range(len(name)):
                    if ord(name[i]) < 32:
                        name = name[:i]
                        break

                if len(name) > 0:
                    lumps.append(name)

                count -= 1

            f.close()

        except Exception, x:
            raise x
            # Ignore errors.
            #pass

        return lumps

        
class DehackedAddon (Addon):
    def __init__(self, identifier, source):
        Addon.__init__(self, identifier, source)

    def getType(self):
        return 'addon-type-dehacked'

    def getCommandLine(self, profile):
        """Return the command line options to load and configure the
        addon.  Use <tt>-deh</tt> for loading DeHackEd files.
        
        @todo  Check profile for extra options?
        """
        return '-deh ' + paths.quote(self.source)

    def readMetaData(self):
        """Generate metadata by making guesses based on the DEH patch
        name and contents."""

        metadata = 'category: patches'

        self.parseConfiguration(metadata)
    
        

class DEDAddon (Addon):
    def __init__(self, identifier, source):
        Addon.__init__(self, identifier, source)

    def getType(self):
        return 'addon-type-ded'
        
    def getCommandLine(self, profile):
        """Return the command line options to load and configure the
        addon.  Use <tt>-def</tt> for loading definition files.
        
        @todo  Check profile for extra options?
        """
        return '-def ' + paths.quote(self.source)

    def readMetaData(self):
        """Generate metadata by making guesses based on the DED patch
        name and contents."""

        metadata = 'category: definitions'

        self.parseConfiguration(metadata)


class LumpAddon (Addon):
    def __init__(self, identifier, source):
        Addon.__init__(self, identifier, source)

    def getType(self):
        return 'addon-type-lump'
        
    def readMetaData(self):
        """Generate metadata for the lump."""

        metadata = 'category: gamedata/lumps'

        self.parseConfiguration(metadata)


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
    return ADDON_EXTENSIONS


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
    return [a for a in addons.values()
            if profile is pr.getDefaults() or a.isCompatibleWith(profile)]


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


def uninstall(identifier):
    """Uninstall an addon.  Uninstalling an addon causes a full reload
    of the entire addon database.
    
    @param identifier Identifier of the addon to uninstall.
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
    for p in pr.getProfiles():
        p.dontUseAddon(identifier)

    # Send a notification.
    notify = events.Notify('addon-uninstalled')
    notify.addon = identifier
    events.send(notify)
    
    # Reload all addons.
    


def formIdentifier(fileName):
    """Forms the identifier of the addon based on the addon's file name.

    @param fileName  File name of the addon.

    @return  Tuple: (identifier, effective extension)
    """
    baseName = os.path.basename(fileName)
    found = re.search("(?i)(.*)\.(" + \
                      string.join(ADDON_EXTENSIONS, '|') + ")$", baseName)
    if not found:
        return ('', '')
    
    identifier = found.group(1).lower()
    extension = found.group(2).lower()
    if extension == 'addon':
        extension = 'bundle'

    # The identifier also includes the type of the addon.
    identifier += "-" + extension

    return (identifier, extension)


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
    #baseName = os.path.basename(fileName)
    #found = re.search("(?i)(.*)\.(" + \
    #string.join(ADDON_EXTENSIONS, '|') + ")$", baseName)
    #identifier = found.group(1).lower()
    #extension = found.group(2).lower()
    #if extension == 'addon':
    #    extension = 'bundle'

    # The identifier also includes the type of the addon.
    #identifier += "-" + extension

    # Form the identifier of the addon.
    identifier, extension = formIdentifier(fileName)

    addon = None

    # Construct a new Addon object based on the file name extension.
    if extension == 'box':
        addon = BoxAddon(identifier, fileName)
    elif extension == 'bundle':
        addon = BundleAddon(identifier, fileName)
    elif extension == 'pk3' or extension == 'zip':
        addon = PK3Addon(identifier, fileName)
    elif extension == 'wad':
        addon = WADAddon(identifier, fileName)
    elif extension == 'ded':
        addon = DEDAddon(identifier, fileName)
    elif extension == 'deh':
        addon = DehackedAddon(identifier, fileName)
    elif extension == 'lmp':
        addon = LumpAddon(identifier, fileName)

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
        addon = get(identifier)
    else:
        # Create a new addon.
        addon = Addon(identifier, fileName)
        addons[identifier] = addon

    # The manifest contains metadata configuration.
    addon.parseConfiguration(file(fileName).read())

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
        allKeywords += a.getKeywords(Addon.PROVIDES) + \
                       a.getKeywords(Addon.OFFERS)

    # Required keywords.
    for a in roster:

        missing = []
        
        for req in a.getKeywords(Addon.REQUIRES):
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

        for x in a.getKeywords(Addon.EXCLUDES):
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
            for x in a.getKeywords(Addon.EXCLUDES):
                if b.getId() == x or x in b.getKeywords(Addon.PROVIDES) or \
                   x in b.getKeywords(Addon.OFFERS):
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

            for prov in a.getKeywords(Addon.PROVIDES):
                if prov in b.getKeywords(Addon.PROVIDES):
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
            for prov in a.getKeywords(Addon.PROVIDES):
                if prov in b.getKeywords(Addon.OFFERS):
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
            for offer in a.getKeywords(Addon.OFFERS):
                if offer in b.getKeywords(Addon.OFFERS):
                    if b not in overrides:
                        overrides.append((b, offer))

        if overrides:
            result.append(('override', a, overrides))

    if result:
        return result

    # No conflicts found.  Yay!
    return []


#
# Module Initialization
#

# Create the root category (for unclassified/generic addons).
rootCategory = Category('')
categories.append(rootCategory)

# Load the metadata cache.
readMetaCache()

# Load all the installed addons in the system and user addon
# databases.
for repository in [paths.ADDONS] + paths.getAddonPaths():
    for name in paths.listFiles(repository):
        # Case insensitive.
        if re.search("(?i)^([^.#].*)\.(" + \
                     string.join(ADDON_EXTENSIONS, '|') + ")$",
                     os.path.basename(name)):
            load(name)

# Load all the manifests.
for folder in [paths.getSystemPath(paths.MANIFESTS),
               paths.getUserPath(paths.MANIFESTS)]:
    for name in paths.listFiles(folder):
        # Case insensitive.
        if paths.hasExtension('manifest', name):
            loadManifest(name)

# Save the updated cache.
writeMetaCache()
