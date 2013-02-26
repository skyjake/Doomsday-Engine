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

## @file tab20_maps.py Maps Tab
##
## This plugin displays a list of all WAD addons that contain maps.
## Selections are equivalent to the Addons tab, but they are presented
## as a list control where it's easier to select one addon instead of
## checking/unchecking a tree.

import os
import ui, events, language
import widgets as wg
import sb.widget.button as wb
import sb.widget.list
import sb.aodb as ao
import sb.profdb as pr
import widgets


MAPS = 'tab-maps'


# React to selection notifications.
listenSelections = True

# The map list box (has columns).
mapListBox = None


def init():
    # Create the Maps page.
    area = ui.createTab(MAPS)
    area.setBorderDirs(ui.BORDER_NOT_BOTTOM)
    area.setWeight(1)

    global mapListBox
    area.setBorderDirs(ui.BORDER_NOT_BOTTOM)
    mapListBox = area.createList('maps-list', sb.widget.list.List.STYLE_COLUMNS)

    # The columns.
    mapListBox.addColumn('maps-list-icon', 20)
    mapListBox.addColumn('maps-list-identifier', 180)
    mapListBox.addColumn('maps-list-count')

    # Prepare icons.
    global iconManager, mapIcons
    iconManager = widgets.IconManager(16, 16)
    mapIcons = [iconManager.get('unchecked'), iconManager.get('checked')]
    mapListBox.setImageList(iconManager.getImageList())

    # Some buttons in the bottom.
    area.setWeight(0)
    area.setBorderDirs(ui.BORDER_NOT_TOP)
    buttonArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=4)
    buttonArea.setBorderDirs(ui.BORDER_TOP | ui.BORDER_RIGHT)
    buttonArea.setWeight(0)
    clearButton = buttonArea.createButton('maps-list-clear', wb.Button.STYLE_MINI)
    clearButton.resizeToBestSize()
    refreshButton = buttonArea.createButton('refresh-addon-database', wb.Button.STYLE_MINI)
    refreshButton.resizeToBestSize()   
    
    buttonArea.setWeight(1)
    buttonArea.addSpacer()
    buttonArea.setWeight(0)
    buttonArea.setBorderDirs(ui.BORDER_TOP | ui.BORDER_LEFT)
    buttonArea.createButton('show-addon-paths')   

    # Listen to our commands.
    events.addCommandListener(handleCommand, ['maps-list-clear'])

    events.addNotifyListener(handleNotify, ['active-profile-changed',
                                            'maps-list-selected',
                                            'maps-list-deselected',
                                            'addon-attached',
                                            'addon-detached',
                                            'addon-database-reloaded',
                                            'language-changed'])

    
def handleCommand(event):
    """Handle a command event."""
    
    if event.hasId('maps-list-clear'):
        mapListBox.selectItem(None)


def handleNotify(event):
    """Handle a notification event."""

    global listenSelections

    if event.hasId('active-profile-changed'):
        refreshList()

    if event.hasId('maps-list-selected') and listenSelections:
        pr.getActive().useAddon(event.getSelection())

    if event.hasId('maps-list-deselected'):
        pr.getActive().dontUseAddon(event.getDeselection())

    if event.hasId('addon-attached'):
        listenSelections = False
        mapListBox.selectItem(event.getAddon())
        mapListBox.setItemImage(event.getAddon(), 1)
        listenSelections = True

    if event.hasId('addon-detached'):
        listenSelections = False
        mapListBox.deselectItem(event.getAddon())
        mapListBox.setItemImage(event.getAddon(), 0)
        listenSelections = True
        
    if event.hasId('addon-database-reloaded'):
        refreshList()
        
    if event.hasId('language-changed'):
        mapListBox.retranslateColumns()
    

def refreshList():
    """Fill the maps list with addons."""

    # Clear the list.
    mapListBox.clear()

    wads = [a for a in ao.getAvailableAddons(pr.getActive())
            if a.getType() == 'addon-type-wad' and a.isPWAD()]

    # Translate addon names.
    wads.sort(lambda a, b: cmp(a.getId(), b.getId()))

    for wad in wads:
        # TODO: More information titles.
        if not language.isDefined(wad.getId()):
            visibleName = language.translate(wad.getId())
        else:
            visibleName = os.path.basename(wad.getContentPath())
        mapListBox.addItemWithColumns(wad.getId(),
                                      0,
                                      visibleName,
                                      wad.getShortContentAnalysis())

    prof = pr.getActive()
    usedAddons = prof.getUsedAddons()
    theFirst = True

    # Select the addons currently attached to the profile.
    # Also make sure the first one is visible.
    for addonId in usedAddons:
        mapListBox.selectItem(addonId)
        mapListBox.setItemImage(addonId, 1)
        if theFirst:
            # Make sure it's visible.
            mapListBox.ensureVisible(addonId)
            theFirst = False
