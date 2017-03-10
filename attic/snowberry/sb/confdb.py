# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2006
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

## @file confdb.py Configuration Database
##
## When the module is initialized, the configuration files are read
## from the configuration directories.  We'll expect to find both
## components and settings.

# Import modules required for initialization.
import os, re, string
import paths
import events    
import logger
import host
import language
import cfparser
import conf


# Dictionary for system configuration (fonts, buttons sizes).
sysConfig = {}

# Dictionary for all known components.
allComponents = {}

# Dictionary for all normal settings (no system settings).
allSettings = {}

# Array of setting identifiers used to select components.
# [number of settings, number of components, array]
# Recalculated when the first two change.
compSettings = [0, 0, []]

# Dictionary for system settings.
systemSettings = {}


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


def isSettingDefined(id):
    """Determines whether a setting with a specific identifier has been 
    defined."""
    return allSettings.has_key(id)


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
    
    
def getSystemSettings(func=None):
    """Returns a filtered list of system settings.

    @param func If this function returns True, the system setting will be
    included in the returned array.
    """
    return [s for s in systemSettings.values() if func is None or func(s)]   


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
                fullName = e.getName() + '-' + key.getName()
                sysConfig[fullName] = key.getValue()
        return
        
    # Check for languages.
    if e.getType() == 'language':
        language.processLanguageBlock(e)
        return
    
    setting = None

    # Each block represents either a component or a setting.
    if e.getType() == 'component':
        # Create a new component.
        _newComponent(conf.Component(e.getName(),
                                     e.findValue('setting'),
                                     e.findValue('option')))

    elif e.getType() == 'toggle':
        setting = conf.ToggleSetting(e.getName(),
                                     e.findValue('option'),
                                     e.findValue('default'),
                                     e.findValue('option-inactive'))

    elif e.getType() == 'implicit':
        setting = conf.Setting(e.getType(), e.getName(), e.findValue('option'))

    elif e.getType() == 'range':
        setting = conf.RangeSetting(e.getName(),
                                    e.findValue('option'),
                                    e.findValue('min'),
                                    e.findValue('max'),
                                    e.findValue('default'),
                                    e.findValue('suffix'))

    elif e.getType() == 'slider':
        setting = conf.SliderSetting(e.getName(),
                                     e.findValue('option'),
                                     e.findValue('min'),
                                     e.findValue('max'),
                                     e.findValue('step'),
                                     e.findValue('default'),
                                     e.findValue('suffix'))

    elif e.getType() == 'text':
        setting = conf.TextSetting(e.getName(),
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
        
        setting = conf.FileSetting(e.getName(),
                                   e.findValue('option'),
                                   e.findValue('must-exist'),
                                   allowedTypes,
                                   e.findValue('default'))

    elif e.getType() == 'folder':
        setting = conf.FolderSetting(e.getName(),
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
                
        # Check what to do if existing choices are already defined.
        existing = e.findValue('existing')
        if existing == 'merge' and isSettingDefined(e.getName()):
            oldSetting = getSetting(e.getName())
            oldSetting.merge(alts, opts)
        else:
            # A totally new setting.
            setting = conf.ChoiceSetting(e.getName(),
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

    import sb.profdb

    # Iterate through all the available settings.
    for setting in allSettings.values():

        # Check the required components.  All settings are compatible
        # with the Defaults profile.
        if profile is not sb.profdb.getDefaults():

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
    global compSettings

    # Do we need to look again?
    if len(allSettings) != compSettings[0] or \
       len(allComponents) != compSettings[1]:
        # Look again.
        compSettings[0] = len(allSettings)
        compSettings[1] = len(allComponents)
        compSettings[2] = []

        for s in allSettings.values():
            for c in allComponents.values():
                if c.getSetting() == s.getId():
                    compSettings[2].append(s.getId())
                    break

    return compSettings[2]


def haveSettingsForAddon(addon):
    """Checks if there are any settings for the specified addon. This
    only affects non-implicit settings, i.e. settings that the user
    can manipulate.

    @param addon Addon identifier.
    """
    for s in allSettings.values():
        if s.getType() == 'implicit':
            continue
        if addon in s.getRequiredAddons():
            return True

    return False


def _earliestInGroup(groupName):
    """Returns the earliest setting order number in the group."""
    earliest = conf.creationOrder

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

    @param id Identifier of the system configuration setting.

    @return True, if the setting has been defined.
    """
    return sysConfig.has_key(id)
    
    
def undefine(id):
    """Undefines a certain system configuration setting.
    
    @param id Identifier of the system configuration setting."""
    if isDefined(id):
        del sysConfig[id]


def getSystemString(id):
    """Return the value of a system configuration setting.  These
    include things such as fonts and button sizes."""
    return sysConfig[id]


def getSystemInteger(id, defaultValue=None):
    """Return the value of a system configuration setting.  These
    include things such as fonts and button sizes."""
    try:
        return int(getSystemString(id))
    except:
        if defaultValue != None:
            return defaultValue
        return 0
    

def getSystemBoolean(id):
    """Return the boolean value of a system configuration setting."""
    return isDefined(id) and getSystemString(id) == 'yes'        


def readConfigPath(path):
    """Load all the config files in the specified directory.

    @param path Directory path.
    """
    try:
        for name in os.listdir(path):
            # We'll load all the files with the .conf extension.
            if paths.hasExtension('conf', name):
                # Load this configuration file.
                readConfigFile(os.path.join(path, name))
    except OSError:
        # Path does not exist.
        pass


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
        f.write('configure snowberry ( previous-version = %s )\n' %
                getSystemString('snowberry-version'))

        for s in systemSettings:
            parts = s.split('-')
            f.write('configure %s ( ' % parts[0])
            f.write('%s = ' % string.join(parts[1:], '-'))
            f.write('%s )\n' % getSystemString(s))

    except: 
        # Paths not saved.
        import traceback
        traceback.print_exc()
        
        
def checkUpgrade():
    """Check the current version against the previous version."""
    
    if not isDefined('snowberry-previous-version'):
        previousVersion = '1.1 or earlier'
    else:
        previousVersion = getSystemString('snowberry-previous-version')

    currentVersion = getSystemString('snowberry-version')

    if previousVersion != currentVersion:
        if previousVersion == '1.1 or earlier':
            # Reset some settings to their default values.
            sysConfig['main-hide-title'] = 'yes'
            sysConfig['main-hide-help'] = 'yes'
            sysConfig['profile-large-icons'] = 'no'
            undefine('main-split-position')
            undefine('main-profile-split-position')
            undefine('main-width')
            undefine('main-height')
                        

def init():
    """Initialize the module."""
    
    # Built-in settings.
    allLangs = language.getLanguages()
    lang = conf.ChoiceSetting('language', '', allLangs, [], 'english')
    lang.setGroup('general-options')
    lang.setSorted(True)
    _newSetting(lang)

    quitLaunch = conf.ToggleSetting('quit-on-launch', '', 'yes', '')
    quitLaunch.setGroup('general-options')
    _newSetting(quitLaunch)

    # System settings.
    tog = conf.ToggleSetting('main-hide-title', '', 'yes', '')
    _newSystemSetting(tog)
    tog = conf.ToggleSetting('main-hide-help', '', 'yes', '')
    _newSystemSetting(tog)
    tog = conf.ToggleSetting('summary-profile-change-autoselect', '', 'no', '')
    _newSystemSetting(tog)
    tog = conf.ToggleSetting('profile-large-icons', '', 'no', '')
    _newSystemSetting(tog)
    tog = conf.ToggleSetting('profile-hide-buttons', '', 'no', '')
    _newSystemSetting(tog)
    tog = conf.ToggleSetting('profile-minimal-mode', '', 'no', '')
    _newSystemSetting(tog)

    # Load all .conf files.
    for path in paths.listPaths(paths.CONF, False):
        readConfigPath(path)

    # Any custom paths?
    if isDefined('doomsday-runtime'):
        paths.setCustomPath(paths.RUNTIME, getSystemString('doomsday-runtime'))

    # Check against previous version to detect upgrades.
    checkUpgrade()

    # Listen for the quit notification so we can save some settings.
    events.addNotifyListener(handleNotify, ['quit', 'widget-edited'])


# Initialize module.    
init()    
