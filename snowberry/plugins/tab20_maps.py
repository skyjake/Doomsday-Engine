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

import ui
import widgets as wg

MAPS = 'tab-maps'


# The map list box (has columns).
mapListBox = None


def init():
    # Create the Maps page.
    area = ui.createTab(MAPS)
    area.setWeight(1)

    global mapListBox
    mapListBox = area.createList('maps-list', wg.List.STYLE_COLUMNS)

    # The columns.
    mapListBox.addColumn('maps-list-identifier')
    mapListBox.addColumn('maps-list-count')

    # Some buttons in the bottom.
    area.setWeight(0)
    buttonArea = area.createArea(alignment=ui.Area.ALIGN_HORIZONTAL, border=2)

    buttonArea.setWeight(0)
    clearButton = buttonArea.createButton('maps-list-clear')

    # Listen to our commands.
    events.addCommandListener(handleCommand)

    
def handleCommand(event):
    """Handle a command event."""
    
    if events.hasId('maps-list-clear'):
        mapListBox.selectItem(None)
