# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2006
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#   Antti Kopponen
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

## @file profile.py Profile Class

import os, re, string, shutil
import events, paths, cfparser, language, logger
import sb.confdb as st
import aodb
import profdb


class Value:
    """Instances of the Value class are stored inside Profile objects.
    Each Value is essentially a tuple that pairs a setting identifier
    with a string value."""

    def __init__(self, settingId, value):
        self.settingId = settingId
        self.value = value

    def getId(self):
        return self.settingId

    def getValue(self):
        return self.value

    def setValue(self, newValue):
        self.value = newValue


class Profile:
    """A Profile is a collection of information about setting values
    and addons marked for loading."""

    def __init__(self, identifier):
        """Construct a new empty Profile object.  The profile will
        contain no values or attached addons.

        @param identifier The identifier of the profile.  This
        determines the file name of the profile file.  Identifiers are
        always lower case strings.
        """
        self.identifier = identifier.lower()
        self.name = ''
        self.values = []
        self.addons = []
        self.loadOrder = []
        self.story = ''
        self.hidden = False
        self.banner = 'banner-default'

    def duplicate(self, newIdentifier):
        """Create a duplicate of the profile.

        @param newIdentifier Identifier of the new Profile object.

        @return The newly created Profile object.
        """

        p = Profile(newIdentifier)
        p.name = self.name
        for v in self.values:
            p.values.append(v)
        for a in self.addons:
            p.addons.append(a)
        for o in self.loadOrder:
            p.loadOrder.append(o)
        p.story = self.story
        p.hidden = self.hidden
        p.banner = self.banner
        return p

    def getId(self):
        """Returns the identifier of the profile.  This is determined
        by the file name of the profile.
        """
        return self.identifier

    def getName(self):
        """Returns the name of the profile.  This is the name provided
        by the user.
        """
        if self is profdb.defaults:
            # The defaults profile has a translateable name.
            return language.translate('defaults-profile')
        
        return self.name

    def getLoadOrder(self):
        return self.loadOrder

    def setLoadOrder(self, order):
        """Set the load order for the profile.

        @param order An array of addon identifiers.
        """
        
        self.loadOrder = order

    def isHidden(self):
        return self.hidden

    def setHidden(self, hide):
        """Hidden profiles aren't listed in the profiles list."""
        self.hidden = hide

    def isSystemProfile(self):
        """Checks if this profile is a system profile.  System profiles can 
        only be hidden, not removed completely.

        @return True, if this is a system profile.
        """
        locations = [paths.getSystemPath(paths.PROFILES)] + \
                     paths.getBundlePaths(paths.PROFILES)
        for loc in locations:
            if os.path.exists(os.path.join(loc, self.getFileName())):
                return True
        return False

    def getFileName(self):
        """Return the file name this profile uses."""
        return self.identifier + ".prof"

    def setName(self, name):
        self.name = name

        # Define this in the english language so it will available for
        # translation.
        language.define('english', self.identifier, name)

    def getAllValues(self):
        """Returns a list of all the values defined in the profile.

        @return An array of Value objects.
        """
        return self.values

    def hasValue(self, id):
        "Checks if the specified setting Id has a value in this profile."
        for v in self.values:
            if v.getId() == id:
                return True
        return False

    def getValue(self, id, fallbackToDefaults=True):
        """Returns the Value for the given setting. Falls back to the
        defaults profile.

        @param id Setting identifier.

        @param fallbackToDefaults If True, the value is retrieved from
        the Defaults profile if it isn't defined in this one.
        """
        defaults = profdb.defaults

        # If this is a system setting, let's consult the system
        # configuration.
        if st.isDefined(id):
            return Value(id, st.getSystemString(id))

        # First look in the Values in this profile.
        for v in self.values:
            if v.getId() == id:
                return v
                
        if id == 'game-mode':
            # We can look for the game mode in the system profile.
            sysProfFileName = os.path.join(paths.getSystemPath(paths.PROFILES), self.getFileName())
            if os.path.exists(sysProfFileName):
                try:
                    p = cfparser.FileParser(sysProfFileName)
                    while True:
                        element = p.get()
                        if element.isKey() and element.getName() == 'game-mode':
                            return Value(element.getName(), element.getValue())
                except cfparser.OutOfElements:
                    # The file ended normally.
                    pass           

        # Fall back to the Defaults profile.
        if fallbackToDefaults and self is not defaults:
            return defaults.getValue(id)
        elif self is defaults:
            # See if the setting itself has a default for this.
            try:
                setting = st.getSetting(id)
                if setting.getDefault():
                    return Value(id, str(setting.getDefault()))
            except:
                pass
            # No Value for this setting could be found.
            return None
        else:
            # A normal profile with no value for this setting.
            return None

    def setValue(self, settingId, newValue, doNotify=True):
        """Set the value of the setting in this profile.

        @param settingId Identifier of the setting to set.

        @param newValue New value for the setting (as a string).

        @param doNotify If True, a notification event will be sent.
        """
        value = None

        if st.isDefined(settingId):
            # System settings are not saved into profiles.
            return

        # Change the value of an existing Value.
        for v in self.values:
            if v.getId() == settingId:
                value = v

        if value:
            value.setValue(newValue)
        else:
            value = Value(settingId, newValue)
            self.values.append(value)

        # A kludge for detecting a change of the language.
        if self is profdb.defaults and settingId == 'language':
            language.change(newValue)
                
        # Send a notification of this.
        if doNotify:
            events.send(events.ValueNotify(settingId, newValue, self))

    def removeValue(self, settingId):
        """Remove this value from the profile entirely.

        @param settingId Identifier of the setting.
        """
        # Kludge for detecting the change of the language.
        if self is profdb.defaults and settingId == 'language':
            language.change(None)

        for v in self.values:
            if v.getId() == settingId:
                self.values.remove(v)
                events.send(events.ValueNotify(settingId, None, self))

    def setStory(self, id):
        self.story = id

    def getStory(self):
        return self.story

    def getBanner(self):
        return self.banner

    def setBanner(self, banner):
        self.banner = banner

    def setAddons(self, addonIdentifiers):
        """Set the addons associated with the profile, without sending
        notifications.  This is used when the profile is being loaded.

        @param addonIdentifiers An array of identifiers.
        """
        self.addons = addonIdentifiers

    def addAddon(self, identifier):
        """Attaches an addon to the profile.  If this is the Defaults
        profile, the addon is detached from all the other profiles.
        This is necessary because of the exclusion rule (see
        getUsedAddons()).

        @param identifier Addon identifier.
        """
        if identifier not in self.addons:
            self.addons.append(identifier)
            # Also send a notification.
            events.send(events.AttachNotify(identifier, self, True))

            if self is profdb.defaults:
                # Detach from others.  The appropriate detach events
                # will be transmitted.
                for p in profdb.getProfiles():
                    if p is not self:
                        p.removeAddon(identifier)

    def removeAddon(self, identifier):
        if identifier in self.addons:
            self.addons.remove(identifier)
            # Also send a notification.
            events.send(events.AttachNotify(identifier, self, False))

    def useAddon(self, identifier):
        """Higher-level addon attaching.  Takes care of the proper
        handling of defaults."""

        if aodb.get(identifier).isOptIn(self):
            self.addAddon(identifier)
        else:
            self.removeAddon(identifier)

    def dontUseAddon(self, identifier):
        """Higher-level addon detaching.  Takes care of the proper
        handling of defaults."""
        
        if aodb.get(identifier).isOptIn(self):
            self.removeAddon(identifier)
        else:
            self.addAddon(identifier)

    def getComponents(self):
        """Match the components with the values of the settings to
        figure out which components are being used.

        @return An array with the identifiers of the used components.
        """
        # Get all the settings that are used to select components.
        compSettings = st.getComponentSettings()

        # Collect the identifiers of the components we use into this
        # array.
        used = []

        for s in compSettings:
            value = self.getValue(s)
            if not value:
                # This component is not selected at all.
                continue
            used.append(value.getValue())

        return used

    def getAddons(self):
        """Returns the list of addons attached to the profile."""
        return [a for a in self.addons if aodb.exists(a)]

    def getUsedAddons(self):
        """Returns the list of all addons that will be used with the
        profile.  This includes the addons that have been attached to
        the Defaults profile.  If a addon is attached to both Defaults
        and this profile, it won't be used (exclusion).

        @return An array of addon identifiers.
        """
        if self is profdb.getDefaults():
            usedAddons = self.getAddons()
        else:
            usedAddons = []

            # Decide which addons are actually used.  The ones that are
            # both in defaultAddons and profileAddons are NOT used.
            # ((P union D) - (P intersection D))
            for a in self.getAddons():
                usedAddons.append(a)

            for d in profdb.getDefaults().getAddons():
                if d in self.getAddons():
                    # This addon is in both the Defaults and this profile,
                    # don't use it.
                    usedAddons.remove(d)
                else:
                    usedAddons.append(d)

        # Any inversed addons that should be used?
        for addon in aodb.getAddons():
            if addon.isInversed():
                if addon.getId() not in usedAddons:
                    usedAddons.append(addon.getId())
                else:
                    usedAddons.remove(addon.getId())
            
        # Filter out the incompatible ones.
        return [a for a in usedAddons if aodb.get(a).isCompatibleWith(self)]

    def getFinalAddons(self):
        """The final addons of a profile are the ones that will be
        used when a game is launched.  The returned list is sorted
        based on the load order that applies to the profile.

        For the Defaults profile, the final addons include ALL the
        existing addons.

        @return A list of addon identifiers, in the actual load order.
        """
        if self is profdb.defaults:
            # Return all existing addons.
            finalAddons = map(lambda a: a.getId(), aodb.getAddons())

        else:
            usedAddons = self.getUsedAddons()

            finalAddons = []

            # Any boxes?
            for ident in usedAddons:
                addon = aodb.get(ident)

                if addon.getBox():
                    # If the box hasn't been selected by the user, forget it.
                    if addon.getBox().getId() not in usedAddons:
                        continue
                
                if addon.isBox():
                    # Also include the required parts of the addons.
                    finalAddons += addon.getRequiredParts()

                finalAddons.append(ident)

        # Apply load order.
        self.sortByLoadOrder(finalAddons)

        return finalAddons                

    def getKeywords(self):
        """The values in the profile provide keywords that can be
        matched against addon dependencies."""

        keys = []

        for setting in st.getSettings():
            # Must be either a toggle or a choice.
            if setting.getType() != 'toggle' and setting.getType() != 'choice':
                continue

            # Must have a value defined.
            v = self.getValue(setting.getId())
            if not v:
                continue

            # Active toggles define a keyword.
            if setting.getType() == 'toggle' and v.getValue() == 'yes':
                if v.getId() not in keys:
                    keys.append(v.getId())

            # Choices define a keyword.
            if setting.getType() == 'choice':
                if v.getValue() not in keys:
                    keys.append(v.getValue())

            # Other setting types don't create keywords.

        return keys

    def sortByLoadOrder(self, addons):
        """Sort a list of addon identifiers based on the load order
        that applies to this profile.  The order is the result of
        multiple factors:
        -# Each profile can define its own load order.
        -# The default profile is consulted is a certain addon's proper
        order is not defined.
        -# If the default profile does not define an order, the
        priority class of the addon is used.

        @param addons The list of addon identifiers.  Will be
        modified.
        """
        def orderCompare(a, b):
            """Compare two addon identifiers.

            @param a Addon identifier.

            @param b Addon identifier.

            @return -1, if a comes before b.  1, if b comes before a.
            """
            # First check if order of these two addons is defined in
            # this profile.
            order = self.getLoadOrder()
            if a in order and b in order:
                # Both of them are in this order.
                return cmp(order.index(a), order.index(b))

            # Check the Defaults profile.
            if self is not profdb.defaults:
                order = profdb.defaults.getLoadOrder()
                if a in order and b in order:
                    # Both of them are in this order.
                    return cmp(order.index(a), order.index(b))

            # Compare the priority classes, then.
            return cmp(aodb.get(a).getPriority(), aodb.get(b).getPriority())

        # Sort them.
        addons.sort(orderCompare)
