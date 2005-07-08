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

## @file settings.py Settings and Components
## 
## This module contains the classes that manage all the available
## settings and components.  The actual configuration with which
## Doomsday is launched is composed of the data in the settings and
## components.
##
## When the module is initialized, the configuration files are read
## from the configuration directories.  We'll expect to find both
## components and settings.

# The addons may contain extra settings.
import os, re, string
import logger
import host
import language
import paths, cfparser
import addons as ao
import profiles as pr
import expressions as ex
import events

# Dictionary for system configuration (fonts, buttons sizes).
sysConfig = {}

# Dictionary for all known components.
allComponents = {}

# Dictionary for all normal settings (no system settings).
allSettings = {}

# Dictionary for system settings.
systemSettings = {}

# Setting creation order counter.
creationOrder = 1


class Component:
    """Components are dynamically loaded libraries that Doomsday
    loads.  Each component implements certain functionality."""
    
    def __init__(self, id, setting):
        self.id = id
        #self.library = library
        #self.option = option
        self.setting = setting

    def getId(self):
        """Return the ID of the Component."""
        return self.id

    def getSetting(self):
        """Return the identifier of the setting that selects this
        component.
        """
        return self.setting


class Setting:
    """Setting is the abstract base class for all the settings."""    
    
    def __init__(self, type, id, option):
        self.type = type
        self.id = id
        self.option = option

        # Append value after option on the command line.
        self.valueAfterOption = True

        # Settings will appear in the order they have been created.
        global creationOrder
        self.order = creationOrder
        creationOrder += 1

        self.default = None

        # Group determines the tab on which the setting will be
        # displayed in Settings.
        self.group = None

        # Subgroup is an optional extra grouping within the setting
        # tab.
        self.subGroup = None

        # An array of tuples: (setting-id, value).
        self.requiredValues = []

        # Lists of required components and addons (identifiers).
        self.components = []
        self.addons = []            

    def getId(self):
        """Return the ID of the Setting."""
        return self.id

    def getOption(self):
        """Return the command line option of the Setting."""
        return self.option

    def getOrder(self):
        """Return the creation order number of the Setting.

        @return An integer.
        """
        return self.order
    
    def getType(self):
        """Returns the type of the Setting.
        @return Text string that is one of 'toggle', 'range', etc.
        """
        return self.type

    def getDefault(self):
        """Returns the default value for the Setting, if one has been
        defined.

        @param The default value.  May be None.
        """
        return self.default

    def getRequiredComponents(self):
        """Returns a listing of the components required by the setting.

        @return An array of string identifiers.
        """
        return self.components

    def getRequiredAddons(self):
        """Returns a listing of the addons required by the setting.

        @return An array of string identifiers.
        """
        return self.addons

    def getRequiredValues(self):
        """@return An array of tuples: (setting-id, value)
        """
        return self.requiredValues

    def getOption(self):
        return self.option

    def setGroup(self, group):
        self.group = group

    def setSubGroup(self, subGroup):
        self.subGroup = subGroup

    def getGroup(self):
        return self.group

    def getSubGroup(self):
        return self.subGroup

    def setValueAfterOption(self, doAppend):
        self.valueAfterOption = (doAppend == 'yes')

    def isValueAfterOption(self):
        return self.valueAfterOption

    def addRequiredComponent(self, component):
        if component:
            self.components.append(component)

    def addRequiredAddon(self, addon):
        if addon:
            self.addons.append(addon)

    def addRequiredValue(self, settingId, value):
        self.requiredValues.append((settingId, value))

    def isEnabled(self, profile):
        """Checks if the setting can be used based on the current
        values of the profile.  Some settings conflict each other.
        This method decides which settings will actually be used for
        launching the game.
        """
        # Check that all the required components are in use.
        if len(self.components) > 0:
            profComps = profile.getComponents()
            for c in self.components:
                if c not in profComps:
                    return False
            
        for settingId, requiredValue in self.getRequiredValues():
            v = profile.getValue(settingId)

            # If there is no value and something is required, the
            # requirement isn't met.
            if not v and requiredValue != '':
                return False

            # If the setting has the wrong value, the requirement
            # isn't met.
            if v.getValue() != requiredValue:
                return False

        return True
        
    def getCommandLine(self, profile):
        """Form the command line options that represent the value of
        the setting.  The value is taken from the specified profile.
        If the value is unspecified, no command line option is
        generated.  This happens when the Defaults profile doesn't
        have a default for the setting.

        @param profile A profiles.Profile object.  The values of the
        settings will be taken from this profile.

        @return A string that contains all the necessary command line
        options.
        """
        cmdLine = self.composeCommandLine(profile)

        # Evaluate expressions in the command line.
        return ex.evaluate(cmdLine, self.getContextAddon())

    def composeCommandLine(self, profile):
        """Overridden by subclasses."""
        return ''

    def getContextAddon(self):
        """Determines the addon with which the setting is associated.
        If the setting is not associated with an addon, return None.

        @return  Addon object.
        """
        if len(self.addons) > 0:
            # This is hardcoded to take the first addon.  Settings
            # don't need to be bound to just one addon...!
            return ao.get(self.addons[0])
        else:
            return None                
    

class ToggleSetting (Setting):
    def __init__(self, id, option, default, inactiveOption=''):
        Setting.__init__(self, 'toggle', id, option)
        if default == 'yes':
            self.default = default
        else:
            self.default = 'no'
        self.inactiveOption = inactiveOption

    def composeCommandLine(self, profile):
        value = profile.getValue(self.getId())
        if value:
            if value.getValue() == 'yes':
                return self.getOption()
            elif self.inactiveOption:
                return self.inactiveOption
        return ""
        

class RangeSetting (Setting):
    def __init__(self, id, option, minimum, maximum, default, suffix=''):
        Setting.__init__(self, 'range', id, option)
        self.min = int(minimum)
        self.max = int(maximum)
        self.default = int(default)
        self.suffix = suffix

    def getMinimum(self):
        return self.min

    def getMaximum(self):
        return self.max
    
    def composeCommandLine(self, profile):
        value = profile.getValue(self.getId())
        if value:
            if self.isValueAfterOption():
                return self.getOption() + ' ' + value.getValue() + self.suffix
            else:
                # No value after option.
                return self.getOption() 
        return ""


class SliderSetting (RangeSetting):
    """A RangeSetting with a step constraint."""

    def __init__(self, id, option, minimum, maximum, step, default, suffix=''):
        RangeSetting.__init__(self, id, option, minimum, maximum,
                              default, suffix)
        self.type = 'slider'
        self.step = int(step)

    def getStep(self):
        return self.step


class ChoiceSetting (Setting):
    def __init__(self, id, option, alternatives, cmdLines, default):
        """Construct a ChoiceSetting.

        @param id Identifier of the setting (string).

        @param option Command line option associated with this setting.

        @param alternatives Array of strings, representing the
        identifiers of each choice.

        @param cmdLines Array of strings, containing the command-line
        equivalent of each choice.

        @param default Identifier of the default choice.
        """
        Setting.__init__(self, 'choice', id, option)
        # The identifiers of the choices.  Add the setting identifier
        # to the beginning of each identifier.
        self.alternatives = map(lambda alt: self.getId() + '-' + alt,
                                alternatives)
        # The command line equivalents of each choice.
        self.cmdLines = cmdLines

        self.sortChoices = False

        # Identifier of the default choice.
        if len(default) > 0:
            self.default = self.getId() + '-' + default
        
    def getAlternatives(self):
        """Returns the list of possible choices.

        @return Array of strings, containing the identifiers of all
        the choices.
        """
        return self.alternatives

    def composeCommandLine(self, profile):
        value = profile.getValue(self.getId())
        if value:
            # Find the corresponding command line parameter.
            try:
                index = self.alternatives.index(value.getValue())
                return self.getOption() + ' ' + self.cmdLines[index]
            except:
                # Unknown choice, how strange.
                pass
        return ""

    def isSorted(self):
        return self.sortChoices
   
    def setSorted(self, shouldSort):
        self.sortChoices = shouldSort


class FileSetting (Setting):
    """A file name setting.  If the file must exist, it is selected
    using a standard file selection dialog.  If not, the name can be
    entered in a text field."""

    def __init__(self, id, option, mustExist, allowedTypes, default):
        Setting.__init__(self, 'file', id, option)
        self.mustExist = (mustExist == 'yes')
        if len(default) > 0:
            self.default = default
        self.allowedTypes = allowedTypes

    def hasToExist(self):
        return self.mustExist

    def getAllowedTypes(self):
        """@return Allowed types as a an array of tuples (name, extension).
        """
        return self.allowedTypes

    def composeCommandLine(self, profile):
        value = profile.getValue(self.getId())
        if value:
            if self.isValueAfterOption():
                v = self.getOption() + ' $PATH<' + value.getValue() + '>'
            else:
                v = self.getOption()
            return v
        return ""


class FolderSetting (FileSetting):
    """A folder name setting, selected using a standard folder
    selection dialog."""

    def __init__(self, id, option, mustExist, default):
        FileSetting.__init__(self, id, option, mustExist, [], default)
        self.type = 'folder'


class TextSetting (Setting):
    """A text setting with an arbitrary text string that is added
    unmodified to the command line."""

    def __init__(self, id, option, default):
        Setting.__init__(self, 'text', id, option)
        if len(default):
            self.default = default

    def composeCommandLine(self, profile):
        value = profile.getValue(self.getId())
        if value:
            if self.isValueAfterOption():
                v = self.getOption() + ' ' + paths.quote(value.getValue())
            else:
                v = self.getOption()
            return v
        return ""


def _newComponent(component):
    """Add a new Component to the allComponents dictionary."""
    # Dictionaries are good because they don't allow duplicate IDs.
    allComponents[component.getId()] = component
    

def getComponent(id):
    """Returns the component with the specified identifier."""
    return allComponents[id]


def getComponents():
    """Returns an array of all the defined components."""
    return allComponents.values()


def getGameComponents():
    """Returns an array of all the game components."""
    games = []
    for c in getComponents():
        if c.getId()[:5] == 'game-':
            games.append(c)
    return games


def _newSetting(setting):
    """Add a new Setting to the allSettings dictionary.

    @param setting A Setting object.

    @return The same Setting object that was given as a parameter.
    """
    # Dictionaries are good because they don't allow duplicate IDs.
    allSettings[setting.getId()] = setting
    return setting


def _newSystemSetting(systemSetting):
    """Add a new Setting to the systemSettings dictionary.

    @param systemSetting A Setting object.

    @return The same Setting object that was given as a parameter.
    """
    # Dictionaries are good because they don't allow duplicate IDs.
    systemSettings[systemSetting.getId()] = systemSetting
    sysConfig[systemSetting.getId()] = systemSetting.getDefault()
    return systemSetting


def getSetting(id):
    """Returns the setting with the specified identifier."""
    return allSettings[id]


def getSystemSetting(id):
    """Returns the system setting with the specified identifier."""
    return systemSettings[id]


def getSettings(func=None):
    """Returns a filtered list of settings.

    @param func If this function returns True, the setting will be
    included in the returned array.
    """
    filtered = []
    for s in allSettings.values():
        if func == None or func(s):
            filtered.append(s)
    return filtered


def readConfigFile(fileName):
    """Reads a configuration file from the specified path.  The file
    must be a .conf file that contains either settings or components,
    or both."""
    p = cfparser.FileParser(fileName)

    # We'll collect all the elements into this array.
    elements = []
    
    try:
        # Loop through all the elements in the config file.
        while True:
            e = p.get()

            if e.isKey() and e.getName() == 'required-platform':
                # This configuration file is specific to a particular
                # platform.
                if (e.getValue() == 'windows' and not host.isWindows()) or \
                   (e.getValue() == 'mac' and not host.isMac()) or \
                   (e.getValue() == 'unix' and not host.isUnix()):
                    # Look, we ran out early.
                    raise cfparser.OutOfElements

            # Config files may contain blocks that group similar
            # settings together.
            if e.isBlock() and e.getType() == 'group':
                # Add everything inside the group into the elements.
                # A group can only contain settings.
                for sub in e.getContents():
                    # There can only be blocks in a group.
                    if not sub.isBlock():
                        continue
                    # Add the 'group' key element to the setting.
                    sub.add(cfparser.KeyElement('group', e.getName()))
                    elements.append(sub)
            else:
                elements.append(e)

    except cfparser.OutOfElements:
        # Parsing was completed.
        pass

    except Exception, exception:
        # Log an error message.
        import traceback
        traceback.print_exc()
        logger.add(logger.HIGH, 'error-read-config-file', fileName,
                  str(exception))

    # Process all the elements we got from the configuration files.
    for e in elements:

        # There should only be blocks in the array.
        if not e.isBlock():
            continue

        processSettingBlock(e)


def processSettingBlock(e):
    """Process a Block that contains the definition of a setting.

    @param e A cfparser.BlockElement object.
    """
    if not e.isBlock():
        return

    # First check for generic configuration options.  These can't be
    # changed in the user interface.
    if e.getType() == 'appearance' or e.getType() == 'configure':
        # Insert the keys into the system configuration dictionary.
        for key in e.getContents():
            if e.getName() == 'addon-path':
                # A custom addon path.
                paths.addAddonPath(key.getValue())
            else:
                # Regular sysConfig string.
                sysConfig[e.getName() + '-' + key.getName()] = key.getValue()
        return
    
    setting = None

    # Each block represents either a component or a setting.
    if e.getType() == 'component':
        # Create a new component.
        _newComponent(Component(e.getName(),
                                e.findValue('setting')))

    elif e.getType() == 'toggle':
        setting = ToggleSetting(e.getName(),
                                e.findValue('option'),
                                e.findValue('default'),
                                e.findValue('option-inactive')) 

    elif e.getType() == 'range':
        setting = RangeSetting(e.getName(),
                               e.findValue('option'),
                               e.findValue('min'),
                               e.findValue('max'),
                               e.findValue('default'),
                               e.findValue('suffix'))

    elif e.getType() == 'slider':
        setting = SliderSetting(e.getName(),
                                e.findValue('option'),
                                e.findValue('min'),
                                e.findValue('max'),
                                e.findValue('step'),
                                e.findValue('default'),
                                e.findValue('suffix'))

    elif e.getType() == 'text':
        setting = TextSetting(e.getName(),
                              e.findValue('option'),
                              e.findValue('default'))

    elif e.getType() == 'file':
        # Is there a list of allowed file types?
        allowedTypes = []
        typeBlock = e.find('types')
        if typeBlock and typeBlock.isBlock() and \
           typeBlock.getType() == 'allowed':
            for typeKey in typeBlock.getContents():
                allowedTypes.append((typeKey.getName(), typeKey.getValue()))
        
        setting = FileSetting(e.getName(),
                              e.findValue('option'),
                              e.findValue('must-exist'),
                              allowedTypes,
                              e.findValue('default'))

    elif e.getType() == 'folder':
        setting = FolderSetting(e.getName(),
                                e.findValue('option'),
                                e.findValue('must-exist'),
                                e.findValue('default'))
                                      
    elif e.getType() == 'choice':
        # Identifiers of the choices.
        altsElement = e.find('alts')
        if altsElement:
            alts = altsElement.getContents()
        else:
            alts = []
            
        # Command line parameters of each choice.
        optsElement = e.find('opts')
        if optsElement:
            opts = optsElement.getContents()
        else:
            opts = []
                
        setting = ChoiceSetting(e.getName(),
                                e.findValue('option'),
                                alts, opts,
                                e.findValue('default'))

    if setting:
        # Any required values?
        req = e.find('equals')
        if req and req.getType() == 'require':
            for r in req.getContents():
                setting.addRequiredValue(r.getName(), r.getValue())
            
        # Grouping information?
        if e.find('group'):
            setting.setGroup(e.findValue('group'))
        if e.find('subgroup'):
            setting.setSubGroup(e.findValue('subgroup'))

        # Automatically append value after option on the command line?
        if e.find('value-after-option'):
            setting.setValueAfterOption(e.findValue('value-after-option'))
            
        # Any required components or addons?
        setting.addRequiredComponent(e.findValue('component'))
        setting.addRequiredAddon(e.findValue('addon'))
            
        # Add the setting to the dictionary of all settings.
        _newSetting(setting)
        

def getCompatibleSettings(profile):
    """Compose a list of all the settings that are compatible with the
    given profile.  Compatibility is determined based on the
    components and addons required by the settings.

    @param profile A profiles.Profile object.

    @return An array of Setting objects.
    """
    compatible = []

    # The profile's components and addons that have been selected for
    # loading.
    profileComponents = profile.getComponents()
    profileAddons = profile.getAddons()

    # Iterate through all the available settings.
    for setting in allSettings.values():

        # Check the required components.  All settings are compatible
        # with the Defaults profile.
        if profile is not pr.getDefaults():

            for requirement in setting.getRequiredComponents():
                if requirement not in profileComponents:
                    # This setting is incompatible.
                    continue

            for requirement in setting.getRequiredAddons():
                if requirement not in profileAddons:
                    # This setting is incompatible.
                    continue

        # This appears to be a compatible setting.
        compatible.append(setting)

    return compatible


def getComponentSettings():
    """Compile an array of settings that are used to select components.

    @return An array of setting identifiers.
    """
    compSettings = []

    for s in allSettings.values():
        for c in allComponents.values():
            if c.getSetting() == s.getId():
                compSettings.append(s.getId())
                break

    return compSettings


def haveSettingsForAddon(addon):
    """Checks if there are any settings for the specified addon.

    @param addon Addon identifier.
    """
    for s in allSettings.values():
        if addon in s.getRequiredAddons():
            return True

    return False


def _earliestInGroup(groupName):
    """Returns the earliest setting order number in the group."""
    earliest = creationOrder

    for s in allSettings.values():
        if s.getGroup() == groupName:
            if earliest == None or s.getOrder() < earliest:
               earliest = min(s.getOrder(), earliest)

    return earliest


def getSettingGroups(addonIdentifier=None):
    """Compose a list of all groups that appear in the defined
    settings.  The list won't include groups that are used by settings
    that are associated with addons.

    @param addonIdentifier If not None, only finds the groups that are
    associated with the given addon.

    @return An array of identifiers.
    """
    groups = []

    for s in allSettings.values():
        if (addonIdentifier == None and len(s.getRequiredAddons()) or
            addonIdentifier != None and
            addonIdentifier not in s.getRequiredAddons()):
            # Settings associated with addons are ignored here.
            continue
        if s.getGroup() and s.getGroup() not in groups:
            groups.append(s.getGroup())

    groups.sort(lambda a, b: cmp(_earliestInGroup(a),
                                 _earliestInGroup(b)))

    return groups


def isDefined(id):
    """Checks if a certain system configuration setting has been defined.

    @param id Identifier of the system configuration settings.

    @return True, if the setting has been defined.
    """
    return sysConfig.has_key(id)


def getSystemString(id):
    """Return the value of a system configuration setting.  These
    include things such as fonts and button sizes."""
    return sysConfig[id]


def getSystemInteger(id):
    """Return the value of a system configuration setting.  These
    include things such as fonts and button sizes."""
    return int(getSystemString(id))
    

def getSystemBoolean(id):
    """Return the boolean value of a system configuration setting."""
    return isDefined(id) and getSystemString(id) == 'yes'        


def readConfigPath(path):
    """Load all the config files in the specified directory.

    @param path Directory path.
    """
    for name in os.listdir(path):
        # We'll load all the files with the .conf extension.
        if paths.hasExtension('conf', name):
            # Load this configuration file.
            readConfigFile(os.path.join(path, name))


def handleNotify(event):
    """Handle notifications.

    @param event An events.Notify event.
    """
    if event.hasId('quit'):
        # The system settings aren't part of any profile, so they need
        # to be saved separately.
        saveSystemConfigSettings()

    elif event.hasId('widget-edited'):
        # When system settings are modified, we immediately update the
        # sysConfig dictionary here.
        if isDefined(event.getWidget()):
            sysConfig[event.getWidget()] = event.getValue()


def saveSystemConfigSettings():
    """Write the values of the system config settings into a
    configuration file called <tt>system.conf</tt>."""

    fileName = os.path.join(paths.getUserPath(paths.CONF), 'system.conf')
    try:
        f = file(fileName, 'w')
        f.write('# This file is generated automatically.\n')

        for s in systemSettings:
            parts = s.split('-')
            f.write('configure %s ( ' % parts[0])
            f.write('%s = ' % string.join(parts[1:], '-'))
            f.write('%s )\n' % getSystemString(s))

    except: 
        # Paths not saved.
        import traceback
        traceback.print_exc()


#
# Module Initialization:
#

# Built-in settings.
allLangs = language.getLanguages()
lang = ChoiceSetting('language', '', allLangs, [], 'english')
lang.setGroup('general-options')
lang.setSorted(True)
_newSetting(lang)

quitLaunch = ToggleSetting('quit-on-launch', '', 'yes', '')
quitLaunch.setGroup('general-options')
_newSetting(quitLaunch)

# System settings.
tog = ToggleSetting('main-hide-title', '', 'no', '')
_newSystemSetting(tog)
tog = ToggleSetting('main-hide-help', '', 'no', '')
_newSystemSetting(tog)

# Load all .conf files.
for path in paths.listPaths(paths.CONF):
    readConfigPath(path)

# Any custom paths?
if isDefined('doomsday-runtime'):
    paths.setCustomPath(paths.RUNTIME, getSystemString('doomsday-runtime'))

# Listen for the quit notification so we can save some settings.
events.addNotifyListener(handleNotify)
