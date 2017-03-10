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

## @file profdb.py Profile Management
##
## @todo The Defaults profile should have its own class.  A
## DefaultsProfile class could then cleanly handle such special cases
## as changing of the user interface language.

import os, re, string, shutil
import events, paths, cfparser, language, logger
import sb.profile
import ui


# The list of available profiles.  All of these will be displayed in the
# Profile List Box.
profiles = []

# The currently active profile.
active = None

# The Defaults profile.
defaults = None


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
    
    @todo  Move this out of the module so the import is unnecessary?
    """
    ui.freeze()
    
    event = events.ActiveProfileNotify(active, hasChanged)
    events.send(event)

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


def _listProfilesIn(locations):
    """Compose a list of profile file names by reading from the specified 
    locations.

    @param paths A list of directory paths.

    @return An array of absolute file names.
    """
    names = []

    for path in locations:
        try:
            for name in os.listdir(path):
                if paths.hasExtension('prof', name):
                    names.append(os.path.join(path, name))
        except OSError:
            # Such a location does not exist, ignore it.
            pass

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

    systemProfilePaths = [paths.getSystemPath(paths.PROFILES)] + \
                          paths.getBundlePaths(paths.PROFILES)
    userProfilePath = paths.getUserPath(paths.PROFILES)

    # List all the system and user profiles.
    availSystem = _listProfilesIn(systemProfilePaths)
    availUser = _listProfilesIn([userProfilePath])

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
    for name in _listProfilesIn([userProfilePath]):
        load(os.path.join(userProfilePath, name), False)
    logger.show()

    defaults = get('defaults')

    if defaults is None:
        # Recreate the Defaults profile.
        load(os.path.join(systemProfilePath, "defaults.prof"), False)
        defaults = get('defaults')
        logger.show()

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
        profile = sb.profile.Profile(identifier)
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

    p = sb.profile.Profile(identifier)

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


def reset(identifier, resetValues=True, resetAddons=True):
    """Resetting a profile removes all values except the 'iwad' value.

    @param identifier Profile identifier.
    @param resetValues  True, if the profile's values should be reset.
    @param resetAddons  True, if the profile's addons should be reset.
    """
    prof = get(identifier)

    if resetValues:
        valueIds = map(lambda v: v.getId(), prof.getAllValues())
        for id in valueIds:
            # Some values cannot be reset.
            if id != 'iwad' and id != 'game' and id != 'setup-wizard-shown':
                prof.removeValue(id)

    if resetAddons:
        # Also detach all addons, except the Deathkings IWAD.
        for addon in prof.getAddons():
            # Huge kludge!  Reset must not render Deathkings unlaunchable.
            # There should be a better way to declare which addons are not
            # resetable.
            if identifier == 'hexen-dk' and addon == 'hexdd-wad':
                continue
        
            prof.removeAddon(addon)
