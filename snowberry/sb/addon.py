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

## @file addon.py Addon Classes

import os, re, string, zipfile, shutil, struct, traceback
import logger, paths, language, cfparser
import aodb, confdb


EXCLUDES = 0
REQUIRES = 1
PROVIDES = 2
OFFERS = 3

# File name extensions of the supported addon formats.
EXTENSIONS = ['box', 'addon', 'pk3', 'zip', 'lmp', 'wad', 'ded', 'deh']

# All the possible names of the addon metadata file.
META_NAMES = ['Info', 'Info.conf', 'info', 'info.conf']
    

class Addon:
    """Addon class serves as an abstract base class to the concrete
    addon implementations."""

    def __init__(self, identifier, source):
        """Construct a new Addon object.

        @param identifier The identifier of the addon.

        @param source The full path name of the source file.
        """
        self.id = identifier
        self.name = ''
        self.category = aodb.getRootCategory()
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
        for t in [EXCLUDES, REQUIRES, PROVIDES, OFFERS]:
            self.keywords[t] = []

        # Every addon provides itself.
        self.keywords[PROVIDES].append(identifier)
        
        self.lastModified = None

        # If no other translation is given, the identifier of the
        # addon is shown as the base name of the source.
        if not language.isDefined(identifier):
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
        import sb.profdb
        defaults = sb.profdb.getDefaults()

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
        import sb.profdb
        if profile is sb.profdb.getDefaults():
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
                    self.category = aodb._getCategory(catPath)

                elif elem.isKey() and elem.getName() == 'order':
                    self.setPriority(elem.getValue())

                elif elem.getName() == 'excludes-category':
                    identifiers = elem.getContents()
                    for id in identifiers:
                        self.excludedCategories.append(aodb._getCategory(id))

                elif elem.getName() == 'excludes':
                    self.addKeywords(EXCLUDES, elem.getContents())

                elif elem.getName() == 'requires':
                    self.addKeywords(REQUIRES, elem.getContents())
                    
                elif elem.getName() == 'provides':
                    self.addKeywords(PROVIDES, elem.getContents())

                elif elem.getName() == 'offers':
                    self.addKeywords(OFFERS, elem.getContents())

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
                    import confdb
                    confdb.processSettingBlock(elem)

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

    def getContentCategoryLongId(self):
        """Forms the category long identifier, in which the contents of this box 
        will be under.
        @return Category long id.
        """
        return self.getCategory().getLongId() + '-' + self.getId()

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
                ident = aodb.load(os.path.join(path, fileName), self)
                role.append(ident)

                # Using any parts of an addon requires that the box
                # itself is marked for loading.
                aodb.get(ident).addKeywords(REQUIRES, [self.getId()])

            except aodb.LoadError, x:
                # Attempted to load a file that is not an addon.
                # Just ignore; boxes can contain all kinds of stuff.
                pass

            except Exception, x:
                # Perhaps it wasn't an addon?
                traceback.print_exc()
            
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
            traceback.print_exc()
 
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
        return aodb._getLatestModTime(self.source)

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
            traceback.print_exc()

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
        
    def isPWAD(self):   
        """Determines if this addon is a PWAD addon."""
        return self.wadType == 'PWAD'
        
    def getCommandLine(self, profile):
        # The Deathkings WAD is a special case and will get always loaded
        # with the -iwad option.
        if self.getId() == 'hexdd-wad':
            return '-iwad ' + paths.quote(self.source)
        return Addon.getCommandLine(self, profile)

    def readMetaData(self):
        """Generate metadata by making guesses based on the WAD file
        name and contents."""

        metadata = ''

        # Defaults.
        game = ''

        # Look at the path where the WAD file is located.
        path = self.getContentPath().lower()
        # But ignore the user addon path.
        if path.find(paths.getUserPath(paths.ADDONS).lower()) == 0:
            path = path[len(paths.getUserPath(paths.ADDONS)) + 1:]

        if 'heretic' in path or 'htic' in path:
            game = 'jheretic'

        elif 'hexen' in path or 'hxn' in path or 'hexendk' in path or \
             'deathkings' in path:
            game = 'jhexen'

        elif 'doom' in path or 'doom2' in path or 'final' in path or \
             'finaldoom' in path or 'tnt' in path or 'plutonia' in path:
            game = 'jdoom'

        # Check for a game component name in the path.
        for component in confdb.getGameComponents():
            compName = component.getId()[5:] # skip "game-" prefix
            if compName in path:
                game = compName
                break

        # Read the WAD directory.
        self.lumps = self.readLumpDirectory()
        lumps = self.lumps
        
        if 'BEHAVIOR' in lumps:
            # Hexen-specific lumps.
            game = 'jhexen'

        if 'ADVISOR' in lumps or 'M_HTIC' in lumps:
            game = 'jheretic'

        if 'M_DOOM' in lumps:
            game = 'jdoom'

        # What can be determined by looking at the lump names?
        for lump in lumps:
            if lump[:2] == 'D_':
                # Doom music lump.
                game = 'jdoom'

        episodic = False
        maps = False

        # Are there any ExMy maps in the WAD?
        for lump in lumps:
            if len(lump) == 4 and lump[0] == 'E' and lump[2] == 'M' and \
               lump[1] in string.digits and lump[3] in string.digits:
                episodic = True

                # Episodes 5 and 6 exist in Heretic.
                if game == '' and (lump[1] == '5' or lump[1] == '6'):
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

    def getShortContentAnalysis(self):
        """Forms a short single line of text that describes the contents
        of the WAD."""

        # The directory must already be read by this time.
        analysis = []              
        mapCount = 0
        hasCustomTextures = False
        
        for lump in self.lumps:
            # Count the number of maps.
            if len(lump) == 4 and lump[0] == 'E' and lump[2] == 'M' and \
               lump[1] in string.digits and lump[3] in string.digits:
                mapCount += 1
            elif len(lump) == 5 and lump[:3] == 'MAP' and \
               lump[3] in string.digits and lump[4] in string.digits:
                mapCount += 1
            # Any custom textures?
            elif lump == 'PNAMES' or lump == 'TEXTURE1' or lump == 'TEXTURE2':
                hasCustomTextures = True
        
        if mapCount > 0:
            analysis.append( language.expand(
                language.translate('wad-analysis-maps'), str(mapCount)) )
        if hasCustomTextures:
            analysis.append(language.translate('wad-analysis-custom-textures'))
        
        return string.join(analysis, ', ').capitalize()

    def readLumpDirectory(self):
        """Open the WAD file and read the lump directory at the end of
        the file."""

        lumps = []

        try:
            f = file(self.getContentPath(), 'rb')

            # Type of the WAD file. This affects categorization.
            self.wadType = f.read(4)

            # WADs use little-endian byte ordering.
            count, dirOffset = struct.unpack('<II', f.read(8))
           
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
