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
import widgets as wg
import settings as st


def init():
    # Register a listener for detecting the completion of Snowberry
    # startup.
    events.addNotifyListener(handleNotify)

    # Listen for the About button.
    #events.addCommandListener(handleCommand)
    
    # Commands for the popup menu.
    ui.addPopupMenuCommand(1, 'show-snowberry-settings')


def handleNotify(event):
    """Handle notifications."""

    if (event.hasId('populating-area') and 
        event.getAreaId() == 'general-options'):

        # Create the addon paths list.
        area = event.getArea()
        area.setExpanding(True)
        area.setBorder(1)
        area.setWeight(0)

        box = area.createArea(boxedWithTitle='addon-paths', border=3)
        box.setWeight(0)
        box.createText('addon-paths-info')
        pathList = box.createList('addon-paths-list')
        pathList.setMinSize(100, 90)

        # Insert the current custom paths into the list.
        for p in paths.getAddonPaths():
            pathList.addItem(p)

        def addAddonPath():
            selection = ui.chooseFolder('addon-paths-add-prompt', '')
            if selection:
                pathList.addItem(selection)
                pathList.selectItem(selection)
                paths.addAddonPath(selection)

        def removeAddonPath():
            selection = pathList.getSelectedItem()
            if selection:
                pathList.removeItem(selection)
                paths.removeAddonPath(selection)
        
        box.setWeight(0)
        commands = box.createArea(alignment=ui.Area.ALIGN_HORIZONTAL, border=2)
        commands.setWeight(0)

        # Button for adding new paths.
        button = commands.createButton('new-addon-path', wg.Button.STYLE_MINI)
        button.addReaction(addAddonPath)
        
        # Button for removing a path.
        button = commands.createButton('delete-addon-path',
                                       wg.Button.STYLE_MINI)
        button.addReaction(removeAddonPath)
        
        commands.setExpanding(False)
        commands.setWeight(1)
        commands.createText('restart-required', align=wg.Text.RIGHT
                            ).setSmallStyle()
        
        # Checkboxes for hiding parts of the UI.
        box = area.createArea(boxedWithTitle='ui-parts', border=3)
        box.setWeight(0)

        # TODO: Create widgets for all system settings?
        box.createSetting(st.getSystemSetting('main-hide-title'))
        box.createSetting(st.getSystemSetting('main-hide-help'))
        box.createSetting(st.getSystemSetting(
            'summary-profile-change-autoselect'))
        
        box.createText('restart-required', align=wg.Text.RIGHT).setSmallStyle()
