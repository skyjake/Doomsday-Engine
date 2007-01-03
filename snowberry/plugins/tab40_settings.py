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

## @file tab40_settings.py Settings Tab

import ui, events
import sb.confdb as st
import sb.profdb as pr
import sb.aodb as ao
import widgets as wg
import sb.widget.tab

# Identifiers:
SETTINGS = 'tab-settings'
CATEGORY_AREA = 'settab'

shownSettings = []


def init():
    """Create the Settings page.  No setting widgets are created until
    profiles have been loaded.
    """
    # The setting categories tab area.
    global categoryArea

    area = ui.createTab(SETTINGS)

    # Create the area where all the setting categories are placed.
    categoryArea = area.createTabArea(CATEGORY_AREA,
                                      sb.widget.tab.TabArea.STYLE_FORMATTED)

    createWidgets()

    # Register a notification listener.
    events.addNotifyListener(handleNotify, ['init-done',
                                            'value-changed',
                                            'active-profile-changed',
                                            'addon-database-reloaded'])

    # The addon-settings command is handled here.
    events.addCommandListener(handleCommand, ['show-addon-settings',
                                              'show-snowberry-settings'])


def createWidgets():
    """Create all the setting widgets and tabs.  This is done at
    startup and when addons are installed or uninstalled."""

    # Start with a fresh category tab area.
    categoryArea.removeAllTabs()

    # An array that contains all the existing setting groups.
    global categoryTabs
    categoryTabs = []

    # Create all the category icons.
    allSettingGroups = st.getSettingGroups()

    for group in allSettingGroups:
        area = categoryArea.addTab(group)
        categoryTabs.append((group, area))

    # Addon settings all go inside one tab.
    area = categoryArea.addTab('addons-options')
    categoryTabs.append(('addons-options', area))

    # The Addons tab contains another tab area, where we'll have one
    # tab for each configurable addon that will be loaded.
    global addonArea
    addonArea = area.createTabArea('addontab', sb.widget.tab.TabArea.STYLE_DROP_LIST)

    # Get all addons that exist, get their identifiers, and sort them
    # by name.
    allAddons = map(lambda a: a.getId(), ao.getAddons())
    ao.sortIdentifiersByName(allAddons)

    for addon in allAddons:
        # Only create tabs for the addons that actually have settings
        # defined for them.
        if st.haveSettingsForAddon(addon):
            area = addonArea.addTab(addon)
            categoryTabs.append((addon, area))

            # Each addon may have multiple pages inside its area.
            # These are determined by the groups.
            addonGroups = st.getSettingGroups(addon)

            if len(addonGroups) > 0:
                # Create tabs for each group.
                area = area.createTabArea(addon + '-tab',
                                          sb.widget.tab.TabArea.STYLE_HORIZ_ICON)
                for group in addonGroups:
                    groupId = addon + '-' + group
                    categoryTabs.append((groupId, area.addTab(groupId)))

    # Populate all the tabs with the setting widgets.  When a profile
    # becomes active, the settings that are not applicable will be
    # disabled.  Certain tab pages may be hidden if they are not
    # applicable.
    populateTabs()

    # Request others to create additional widgets in the tabs.
    requestPopulation()

    # Redo the layout.
    ui.getArea(SETTINGS).updateLayout()


def handleCommand(event):
    """Handle commands."""
    if event.hasId('show-addon-settings'):
        # Switch to the Settings tab.
        ui.selectTab(SETTINGS)

        # Switch to the Addons category.
        categoryArea.selectTab('addons-options')

        # Switch to the correct tab.
        addonArea.selectTab(event.addon)
        
    elif event.hasId('show-snowberry-settings'):
        # Change to the Defaults profile.
        pr.setActive(pr.getDefaults())
    
        # Switch to the Settings tab.
        ui.selectTab(SETTINGS)
        
        # Switch to the Snowberry category.
        categoryArea.selectTab('general-options')        


def requestPopulation():
    # Request others to create widgets in the setting tabs, if
    # necessary.
    for areaId, area in categoryTabs:
        events.send(events.PopulateNotify(areaId, area))
        area.updateLayout()


def handleNotify(event):
    """This is called when someone sends a notification event."""

    #if event.hasId('init-done'):

    if (event.hasId('value-changed') or
          event.hasId('active-profile-changed')):
        # Update any settings with value dependencies.
        enableByRequirements(pr.getActive())
        showCompatibleAddons(pr.getActive())

        # Only the defaults profile has the General category.
        if pr.getActive() is pr.getDefaults():
            wasIt = (categoryArea.getSelectedTab() == 'game-options')
            
            categoryArea.showTab('general-options')
            categoryArea.showTab('game-options', False)

            if wasIt:
                categoryArea.selectTab('general-options')
        else:
            wasIt = (categoryArea.getSelectedTab() == 'general-options')

            categoryArea.showTab('game-options')
            categoryArea.showTab('general-options', False)

            if wasIt:
                categoryArea.selectTab('game-options')

            categoryArea.updateIcon('game-options')

    elif event.hasId('addon-database-reloaded'):
        # Since the new addon will most likely introduce new settings,
        # we'll need to recreate the all the settings tabs.
        createWidgets()

        # The newly-created widgets don't have any data in them.  Make
        # them refresh their values.
        pr.refresh()


def populateWithSettings(areaId, area, settings):
    """Create widgets for the given settings in the specified area.

    @param area The area into which the setting widgets will be
    created.

    @param settings An array of Setting objects.

    @param profile A profiles.Profile object, where the values of the
    settings are taken from.
    """
    global shownSettings

    # If no settings were given, we don't need to do anything.
    if len(settings) == 0:
        return

    # Determine if any subgroups need to be formed.
    grouping = {}

    for s in settings:
        name = s.getSubGroup()
        if not name:
            name = ''
        try:
            grouping[name].append(s)
        except KeyError:
            # name wasn't in the array yet.
            grouping[name] = [s]

    orderedGroupKeys = grouping.keys()

    # Sort the subgroups based on the earliest setting order
    # number in each group.
    def getEarliestOrder(settings):
        earliest = 0
        for s in settings:
            if earliest == 0 or s.getOrder() < earliest:
                earliest = s.getOrder()
        return earliest

    orderedGroupKeys.sort(lambda a,b: cmp(getEarliestOrder(grouping[a]),
                                          getEarliestOrder(grouping[b])))

    # Create widgets for all the settings.
    for i in orderedGroupKeys:
        subGroupSettings = grouping[i]

        # Sort the settings based on the order.
        subGroupSettings.sort(lambda a, b: cmp(a.getOrder(), b.getOrder()))

        if i == '':
            # No grouping.
            where = area
        else:
            # Create a subgroup.
            where = area.createArea(border=2, boxedWithTitle=i)
            where.setWeight(0)

        for s in subGroupSettings:
            settingArea, settingWidget = where.createSetting(s)
            if settingArea:
                # Add it into the array of shown settings.
                shownSettings.append((s, settingArea))

    area.updateLayout()


def populateTabs():
    """Create setting widgets in all the tabs."""

    global shownSettings
    shownSettings = []

    for areaId, area in categoryTabs:
        # Use the appropriate layout parameters.
        area.setWeight(0)
        #area.setBorder(2)

        # Create a title inside the tab.
        #if areaId != 'addons-options' and areaId[-8:] == '-options':
        #    area.createText(areaId).setHeadingStyle()
        #    area.createLine()

        def settingFilter(s):
            if len(s.getRequiredAddons()) > 0:
                ident = s.getRequiredAddons()[0]
                properTab = ident

                # The addon does not exist?
                if not ao.exists(ident) or ao.get(ident).isUninstalled():
                    return False

                if s.getGroup():
                    # Addon setting with a group.
                    properTab += '-' + s.getGroup()
            elif not s.getGroup():
                properTab = 'general-options'
            else:
                properTab = s.getGroup()
            return properTab == areaId

        # Collect a list of all the settings for this tab (group).
        tabSettings = st.getSettings(settingFilter)

        populateWithSettings(areaId, area, tabSettings)


def enableByRequirements(profile):
    """Disable settings that can't be used at the moment.  The
    requirements are resolved based on the values in the specified
    profile.

    @param profile A profiles.Profile object.
    """
    for setting, area in shownSettings:
        area.enable(setting.isEnabled(profile))


def showCompatibleAddons(profile):
    """Show only the addons (tabs) that are compatible with the
    current profile.

    @param profile Profile to check against.
    """
    compatibleAddonIds = map(lambda a: a.getId(),
                              ao.getAvailableAddons(profile))

    for identifier in addonArea.getTabs():
        addonArea.showTab(identifier, identifier in compatibleAddonIds)
