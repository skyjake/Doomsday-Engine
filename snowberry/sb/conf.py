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

## @file conf.py Configuration Setting Classes
## 
## This module contains the classes that manage all the available
## settings and components.  The actual configuration with which
## Doomsday is launched is composed of the data in the settings and
## components.

# Import modules required for initialization.
import os, re, string
import paths
import sb.expressions as ex


# Setting creation order counter.
creationOrder = 1


class Component:
    """Components are dynamically loaded libraries that Doomsday
    loads.  Each component implements certain functionality."""
    
    def __init__(self, id, setting, option=''):
        self.id = id
        self.option = option
        self.setting = setting

    def getId(self):
        """Return the ID of the Component."""
        return self.id

    def getSetting(self):
        """Return the identifier of the setting that selects this
        component.
        """
        return self.setting

    def getOption(self):
        """Returns extra options to be used with the Component."""
        return ex.evaluate(self.option, None)


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
        return self.getOption()

    def getContextAddon(self):
        """Determines the addon with which the setting is associated.
        If the setting is not associated with an addon, return None.

        @return  Addon object.
        """
        if len(self.addons) > 0:
            # This is hardcoded to take the first addon.  Settings
            # don't need to be bound to just one addon...!
            import sb.aodb
            return sb.aodb.get(self.addons[0])
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
        
    def merge(self, alts, opts):
        """Merges the alternatives and command lines of the other choice
        setting with this one. Merging means that duplicates are not added."""
        
        for alt in alts:
            if alt not in self.alternatives:
                self.alternatives.append(alt)
        for cmd in opts:
            if cmd not in self.cmdLines:
                self.cmdLines.append(cmd)


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
            if self.getOption() != '':
                # An option has been defined for the setting.
                if self.isValueAfterOption():
                    v = self.getOption() + ' ' + paths.quote(value.getValue())
                else:
                    v = self.getOption()
            else:
                # No option has been defined, so just add the text as-is.
                v = value.getValue()
            return v
        return ""
