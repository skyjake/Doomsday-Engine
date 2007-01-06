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

## @file preferences.py Preferences in the Defaults/Snowberry Settings

import ui, events, language, paths
import sb.widget.button as wb
import sb.widget.text as wt
import sb.widget.list as wl
import sb.widget.tab as wa
import sb.confdb as st
import sb.util.dialog


# List widget of custom addon paths.
pathList = None

prefTabs = None


def init():
    # Register a listener for detecting the completion of Snowberry
    # startup.
    events.addNotifyListener(handleNotify, ['populating-area',
                                            'addon-paths-changed'])

    # Listen for the About button.
    events.addCommandListener(handleCommand, ['show-addon-paths'])
    
    # Commands for the popup menu.
    ui.addMenuCommand(ui.MENU_TOOLS, 'show-snowberry-settings', group=ui.MENU_GROUP_APP)


def handleNotify(event):
    """Handle notifications."""

    global pathList
    global prefTabs

    if (event.hasId('populating-area') and 
        event.getAreaId() == 'general-options'):

        # Create the addon paths list.
        area = event.getArea()
        area.setExpanding(True)
        area.setBorder(1)
        area.setWeight(1)
        
        # Tabs for different kinds of settings.
        tabs = area.createTabArea('pref-tab', style=wa.TabArea.STYLE_BASIC)        
        prefTabs = tabs
        box = tabs.addTab('addon-paths')
        box.setBorder(10)

        #box = area.createArea(boxedWithTitle='addon-paths', border=3)
        box.setWeight(1)
        #box.createText('addon-paths-info')
        box.setBorderDirs(ui.BORDER_NOT_BOTTOM)
        pathList = box.createList('addon-paths-list', wl.List.STYLE_MULTIPLE)
        box.setBorderDirs(ui.BORDER_NOT_TOP)
        pathList.setMinSize(100, 90)

        # Insert the current custom paths into the list.
        for p in paths.getAddonPaths():
            pathList.addItem(p)

        def addAddonPath():
            selection = sb.util.dialog.chooseFolder('addon-paths-add-prompt', '')
            if selection:
                pathList.deselectAllItems()
                pathList.addItem(selection)
                pathList.selectItem(selection)
                paths.addAddonPath(selection)                

        def removeAddonPath():
            for selection in pathList.getSelectedItems():
                pathList.removeItem(selection)
                paths.removeAddonPath(selection)
        
        box.setWeight(0)
        commands = box.createArea(alignment=ui.ALIGN_HORIZONTAL, border=2)
        commands.setWeight(0)

        # Button for adding new paths.
        button = commands.createButton('new-addon-path', wb.Button.STYLE_MINI)
        button.addReaction(addAddonPath)
        
        # Button for removing a path.
        button = commands.createButton('delete-addon-path',
                                       wb.Button.STYLE_MINI)
        button.addReaction(removeAddonPath)
        
        commands.setExpanding(False)
        commands.setWeight(1)
        commands.createText('refresh-required', align=wt.Text.RIGHT
                            ).setSmallStyle()
        
        # Checkboxes for hiding parts of the UI.
        #box = area.createArea(boxedWithTitle='ui-parts', border=3)
        box = tabs.addTab('ui-parts')
        box.setBorder(10)
        box.setWeight(0)

        # Create widgets for all (boolean) system settings.
        toggles = st.getSystemSettings(lambda s: s.getType() == 'toggle')
        toggles.sort(key=lambda s: s.getId())
        for toggle in toggles:
            box.createSetting(toggle)

        box.setWeight(1)
        box.addSpacer()
        box.setWeight(0)
        box.setBorder(10)
        box.createText('restart-required', align=wt.Text.RIGHT).setSmallStyle()

    elif event.hasId('addon-paths-changed'):
        # Insert the current custom paths into the list.
        pathList.clear()
        for p in paths.getAddonPaths():
            pathList.addItem(p)


def handleCommand(event):
    if event.hasId('show-addon-paths'):
        events.send(events.Command('show-snowberry-settings'))
        prefTabs.selectTab('addon-paths')
        pathList.focus()
