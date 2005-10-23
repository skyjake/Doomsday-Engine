# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
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

## @file profiles.py Profile Management
##
## @todo The Defaults profile should have its own class.  A
## DefaultsProfile class could then cleanly handle such special cases
## as changing of the user interface language.

import os, re, string, shutil
import events, paths, cfparser, language, logger
import settings as st
import addons as ao
import ui

# The list of available profiles.  All of these will be displayed in the
# Profile List Box.
profiles = []

# The currently active profile.
active = None

# The Defaults profile.
defaults = None


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
            p.values.append(a)
        for o in self.loadOrder:
            p.values.append(o)
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
        if self is defaults:
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
        """Checks if this profile is a system profile.  System
        profiles can only be hidden, not removed completely.

        @return True, if this is a system profile.
        """
        sysProfiles = paths.getSystemPath(paths.PROFILES)
        return os.path.exists(os.path.join(sysProfiles, self.getFileName()))

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
        global defaults

        # If this is a system setting, let's consult the system
        # configuration.
        if st.isDefined(id):
            return Value(id, st.getSystemString(id))

        # First look in the Values in this profile.
        for v in self.values:
            if v.getId() == id:
                return v

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
        if self is defaults and settingId == 'language':
            language.change(newValue)
                
        # Send a notification of this.
        if doNotify:
            events.send(events.ValueNotify(settingId, newValue, self))

    def removeValue(self, settingId):
        """Remove this value from the profile entirely.

        @param settingId Identifier of the setting.
        """
        # Kludge for detecting the change of the language.
        if self is defaults and settingId == 'language':
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

            if self is defaults:
                # Detach from others.  The appropriate detach events
                # will be transmitted.
                for p in profiles:
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

        if ao.get(identifier).isOptIn(self):
            self.addAddon(identifier)
        else:
            self.removeAddon(identifier)

    def dontUseAddon(self, identifier):
        """Higher-level addon detaching.  Takes care of the proper
        handling of defaults."""
        
        if ao.get(identifier).isOptIn(self):
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
        return [a for a in self.addons if ao.exists(a)]

    def getUsedAddons(self):
        """Returns the list of all addons that will be used with the
        profile.  This includes the addons that have been attached to
        the Defaults profile.  If a addon is attached to both Defaults
        and this profile, it won't be used (exclusion).

        @return An array of addon identifiers.
        """
        if self is getDefaults():
            usedAddons = self.getAddons()
        else:
            usedAddons = []

            # Decide which addons are actually used.  The ones that are
            # both in defaultAddons and profileAddons are NOT used.
            # ((P union D) - (P intersection D))
            for a in self.getAddons():
                usedAddons.append(a)

            for d in getDefaults().getAddons():
                if d in self.getAddons():
                    # This addon is in both the Defaults and this profile,
                    # don't use it.
                    usedAddons.remove(d)
                else:
                    usedAddons.append(d)

        # Any inversed addons that should be used?
        for addon in ao.getAddons():
            if addon.isInversed():
                if addon.getId() not in usedAddons:
                    usedAddons.append(addon.getId())
                else:
                    usedAddons.remove(addon.getId())
            
        # Filter out the incompatible ones.
        return [a for a in usedAddons if ao.get(a).isCompatibleWith(self)]

    def getFinalAddons(self):
        """The final addons of a profile are the ones that will be
        used when a game is launched.  The returned list is sorted
        based on the load order that applies to the profile.

        For the Defaults profile, the final addons include ALL the
        existing addons.

        @return A list of addon identifiers, in the actual load order.
        """
        if self is defaults:
            # Return all existing addons.
            finalAddons = map(lambda a: a.getId(), ao.getAddons())

        else:
            usedAddons = self.getUsedAddons()

            finalAddons = []

            # Any boxes?
            for ident in usedAddons:
                addon = ao.get(ident)

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
            if self is not defaults:
                order = defaults.getLoadOrder()
                if a in order and b in order:
                    # Both of them are in this order.
                    return cmp(order.index(a), order.index(b))

            # Compare the priority classes, then.
            return cmp(ao.get(a).getPriority(), ao.get(b).getPriority())

        # Sort them.
        addons.sort(orderCompare)
        

def get(identifier):
    """Returns the profile with the specified identifier.

    @param identifier The identifier of the profile.

    @return A Profile object, or None if the identifier isn't in use.
    """
    for p in profiles:
        if p.getId() == identifier:
            return p
    return None


def getProfiles(func=None):
    return [p for p in profiles if not func or func(p)]


def refresh(hasChanged=False):
    """Send a notification that will cause everybody to refresh their
    state with regards to the active profile.
    """
    ui.freeze()
    
    if hasChanged:
        evName = 'active-profile-changed'
    else:
        evName = 'active-profile-refreshed'
    events.send(events.Notify(evName))

    ui.unfreeze() 


def setActive(profile):
    """Set a new active profile.  If the specified profile is not the
    same as the currently active profile, a notification is sent.

    @param profile A Profile object.
    """
    global active

    if profile is not active:
        active = profile
        refresh(True)


def getActive():
    return active


def getDefaults():
    return defaults


def save():
    """Write all the profiles to disk.  Also saves which of the profiles
    is currently active.
    """
    path = paths.getUserPath(paths.PROFILES)

    # First we'll remove all the ".prof"-files in the user's profile directory.
    #for name in os.listdir(path):
    #    fileName = os.path.join(path, name)
    #    if paths.hasExtension('prof', fileName):
    #        os.remove(fileName)

    # Then we'll create files for Profiles and fill them with information.
    for p in profiles:
        fileName = os.path.join(path, p.getFileName())

        f = file(fileName, 'w')

        # Name has to be written even in user's ".prof"-files.
        f.write('name: ' + p.getName() + "\n")

        # Hidden profiles don't show up in the profiles list.
        if p.isHidden():
            f.write("hidden: yes\n")

        # If the Profile happens to be Defaults, we'll write there
        # information concerning the active Profile.
        if p is defaults:
            f.write('active: ' + active.getId() + "\n")

        # The banner image.
        f.write('banner: ' + p.getBanner() + "\n")

        # The list of attached Addons is saved.
        addons = p.getAddons()
        if len(addons) > 0:
            f.write("addons <" + string.join(p.getAddons(), ', ') + ">\n")

        # Load order is saved.
        if len(p.getLoadOrder()):
            f.write("load-order <" + string.join(p.getLoadOrder(), ', ') +
                    ">\n")                    

        # Values are saved.
        for value in p.getAllValues():
            f.write(value.getId() + ': ' + value.getValue() + "\n")

        f.close()


def _listProfilesIn(path):
    """Compose a list of profile file names in the specified location.

    @param path A directory path.

    @return An array of absolute file names.
    """
    names = []

    for name in os.listdir(path):
        if paths.hasExtension('prof', name):
            names.append(os.path.join(path, name))

    return names


def restore():
    """Reads all the profiles from disk.  Restores the currently active
    profile as well.  This function has to be called after starting
    the program before any profiles are accessed.

    System profiles that aren't present in the user's profile
    directory are copied to the user's profile directory.
    """
    global defaults
    global profiles
    global restoredActiveId

    # By default, restore selection to the Defaults profile.
    restoredActiveId = 'defaults'

    # Clear the current profile list.
    profiles = []

    systemProfilePath = paths.getSystemPath(paths.PROFILES)
    userProfilePath = paths.getUserPath(paths.PROFILES)

    # List all the system and user profiles.
    availSystem = _listProfilesIn(systemProfilePath)
    availUser = _listProfilesIn(userProfilePath)

    # We are going to load only the profiles in the user's direcory,
    # but before that make sure that the user has an instance of each
    # system profile.
    for sysFile in availSystem:
        identifier = paths.getBase(sysFile)

        # Does this exist in the user's directory?
        gotIt = False
        for userFile in availUser:
            if paths.getBase(userFile) == identifier:
                gotIt = True
                break

        if not gotIt:
            # Since the system profile does not exist on the user,
            # copy it to the user profile directory.
            shutil.copyfile(sysFile,
                            os.path.join(userProfilePath,
                                         os.path.basename(sysFile)))

    # Find every profile in system's and user's profile directories.
    # Files in the user's directory augment the files in the system
    # directory.
    for name in _listProfilesIn(userProfilePath):
        load(os.path.join(userProfilePath, name), False)

    defaults = get('defaults')

    # Set the default language.
    lang = defaults.getValue('language')
    if lang:
        language.change(lang.getValue())

    # Send profile-loaded notifications.
    for p in profiles:
        if not p.isHidden():
            events.send(events.ProfileNotify(p))

    # Restore the previously active profile.
    prof = get(restoredActiveId)
    if prof:
        setActive(prof)
    else:
        setActive(defaults)


def load(path, notify=True):
    """Loads a single profile from disk.  After the profile has been
    loaded, sends a "profile-updated" notification so that the plugins
    may react to the new profile.  If a profile with the

    @param path The file name of the profile to load.
    """

    # Parameter "path" stands for profile's file name. Every time a
    # profile is being loaded, we'll have to check if the profile
    # allready exists. In that case, old profile will be updated by
    # new profile's information. Finally when the loading is complete,

    # The identifier of the profile (the base name of the file).
    identifier = paths.getBase(path)

    # Parse the file.
    elements = []
    p = cfparser.FileParser(path)
    try:
        while True:
            elements.append(p.get())

    except cfparser.OutOfElements:
        # The file ended normally.
        pass

    except cfparser.ParseFailed, ex:
        # Show the error dialog.
        logger.add(logger.HIGH, 'error-parsing-profile', path, str(ex))
        return
                
    except Exception:
        # All other errors also cause the error dialog to appear.
        logger.add(logger.HIGH, 'error-read-profile', path,
                   logger.formatTraceback())
        return

    # Should be use an existing profile?
    profile = get(identifier)

    if not profile:
        # Create a new blank profile.
        profile = Profile(identifier)
        # Add it to the main list of profiles.
        global profiles
        profiles.append(profile)

    # Process all the elements.
    for e in elements:
        if e.isKey() and e.getName() == 'name':
            # The user-provided name.
            profile.setName(e.getValue())
            language.define('english', identifier, profile.getName())

        elif e.isKey() and e.getName() == 'active':
            # The identifier of the active profile.  This is stored in
            # the Defaults profile and restore() uses it to return to
            # the active profile that was used when save() was called.
            global restoredActiveId
            restoredActiveId = e.getValue()

        elif e.isKey() and e.getName() == 'hidden':
            profile.setHidden(e.getValue() == 'yes')

        elif e.isList() and e.getName() == 'load-order':
            profile.setLoadOrder(e.getContents())

        elif e.isKey() and e.getName() == 'story':
            # A story associated with the profile.  An HTML-formatted
            # description of the profile, really.
            profile.setStory(e.getValue())

        elif e.isKey() and e.getName() == 'banner':
            profile.setBanner(e.getValue())

        elif e.isList() and e.getName() == 'addons':
            # The addons attached to the profile.
            profile.setAddons(e.getContents())

        elif e.isKey():
            # A value for a setting?
            try:
                # No notifications will be sent.
                profile.setValue(e.getName(), e.getValue(), False)
            except KeyError:
                # Setting does not exist, ignore it.
                pass

    if notify:
        # The profile has been loaded.
        events.send(events.ProfileNotify(profile))


def _getNewProfileId():
    # Choose the new identifier by comparing against the profiles
    # already loaded in memory.
    for i in range(10000):
        identifier = 'userprofile%i' % i
        if not get(identifier):
            # This is unused.
            break
    return identifier


def create(name, gameComponent):
    """Create a new profile with the specified name.  The identifier
    of the profile is generated automatically.  The identifier is not
    visible to the user.

    @param name The visible name of the new profile.

    @param gameComponent The game that the profile will be using.

    @return The Profile object that was created.
    """

    # Generate an unused identifier.
    identifier = _getNewProfileId()

    p = Profile(identifier)

    # Set the basic information of the profile.
    p.setName(name)
    p.setValue('game', gameComponent)

    # Add it to the list of profiles and send a notification.
    profiles.append(p)
    events.send(events.ProfileNotify(p))
    setActive(p)
    return p


def duplicate(identifier, name):
    if identifier == 'defaults':
        return

    # Generate an unused identifier.
    p = get(identifier).duplicate(_getNewProfileId())

    p.setName(name)

    # Add it to the list of profiles and send a notification.
    profiles.append(p)
    events.send(events.ProfileNotify(p))
    setActive(p)
    return p


def hide(identifier):
    """Mark a profile hidden."""

    prof = get(identifier)

    if prof is defaults:
        # Hiding the defaults is not possible.
        return

    prof.setHidden(True)
    events.send(events.ProfileNotify(prof))


def show(identifier):
    """Clear the hide flag and show a profile."""

    prof = get(identifier)
    prof.setHidden(False)
    events.send(events.ProfileNotify(prof))


def remove(identifier):
    """Removes one profile from profiles.

    @param identifier Indentifier of the profile to remove.
    """
    if identifier == 'defaults':
        # The Defaults profile can't be removed.
        return

    prof = get(identifier)

    hide(identifier)

    # System profiles can't be deleted, only hidden.
    if not prof.isSystemProfile():
        fileName = os.path.join(paths.getUserPath(paths.PROFILES),
                               prof.getFileName())
        if os.path.exists(fileName):
            os.remove(fileName)
        profiles.remove(prof)


def reset(identifier):
    """Resetting a profile removes all values except the 'iwad' value.

    @param identifier Profile identifier.
    """
    prof = get(identifier)

    valueIds = map(lambda v: v.getId(), prof.getAllValues())

    for id in valueIds:
        if id != 'iwad' and id != 'game':
            prof.removeValue(id)

    # Also detach all addons, except the Deathkings IWAD.
    for addon in prof.getAddons():

        # Huge kludge!  Reset must not render Deathkings unlaunchable.
        # There should be a better way to declare which addons are not
        # resetable.
        if identifier == 'hexen-dk' and addon == 'hexdd-wad':
            continue
        
        prof.removeAddon(addon)
