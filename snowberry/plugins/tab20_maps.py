# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2005
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

## @file tab_maps.py Maps Tab
##
## This plugin displays a list of all WAD addons that contain maps.
## Selections are equivalent to the Addons tab, but they are presented
## as a list control where it's easier to select one addon instead of
## checking/unchecking a tree.

import ui, events
import widgets as wg
import addons as ao
import profiles as pr


MAPS = 'tab-maps'


# React to selection notifications.
listenSelections = True

# The map list box (has columns).
mapListBox = None


def init():
    # Create the Maps page.
    area = ui.createTab(MAPS)
    area.setWeight(1)

    global mapListBox
    mapListBox = area.createList('maps-list', wg.List.STYLE_COLUMNS)

    # The columns.
    mapListBox.addColumn('maps-list-identifier', 150)
    mapListBox.addColumn('maps-list-count', 100)

    # Some buttons in the bottom.
    area.setWeight(0)
    buttonArea = area.createArea(alignment=ui.Area.ALIGN_HORIZONTAL, border=2)

    buttonArea.setWeight(0)
    clearButton = buttonArea.createButton('maps-list-clear')

    # Listen to our commands.
    events.addCommandListener(handleCommand)

    events.addNotifyListener(handleNotify)

    
def handleCommand(event):
    """Handle a command event."""
    
    if event.hasId('maps-list-clear'):
        mapListBox.selectItem(None)


def handleNotify(event):
    """Handle a notification event."""

    global listenSelections

    if event.hasId('active-profile-changed'):
        refreshList()

    # TODO: Listen to notifications about selected and deselected list
    # items.
    # TODO: Listen to addon-attached and addon-detached.

    if event.hasId('maps-list-selected') and listenSelections:
        pr.getActive().useAddon(event.getSelection())

    if event.hasId('maps-list-deselected'):
        pr.getActive().dontUseAddon(event.getDeselection())

    if event.hasId('addon-attached'):
        listenSelections = False
        mapListBox.selectItem(event.getAddon())
        listenSelections = True

    if event.hasId('addon-detached'):
        listenSelections = False
        mapListBox.deselectItem(event.getAddon())
        listenSelections = True
    

def refreshList():
    """Fill the maps list with addons."""

    # Clear the list.
    mapListBox.removeAllItems()

    wads = [a for a in ao.getAvailableAddons(pr.getActive())
            if a.getType() == 'addon-type-wad']

    # Translate addon names.
    wads.sort(lambda a, b: cmp(a.getId(), b.getId()))

    for wad in wads:
        # TODO: More information titles.
        mapListBox.addItemWithColumns(wad.getId(),
                                      wad.getId(),
                                      '50 maps')

    prof = pr.getActive()
    usedAddons = prof.getUsedAddons()
    theFirst = True

    # Select the addons currently attached to the profile.
    # Also make sure the first one is visible.
    for addonId in usedAddons:
        mapListBox.selectItem(addonId)
        if theFirst:
            # Make sure it's visible.
            mapListBox.ensureVisible(addonId)
            theFirst = False
