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

## @file preferences.py Preferences dialog

import ui, events, language
import widgets as wg


def init():
    # Register a listener for detecting the completion of Snowberry
    # startup.
    events.addNotifyListener(handleNotify)

    # Listen for the About button.
    events.addCommandListener(handleCommand)


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
        box.setWeight(1)
        box.createList('addon-paths-list')

        box.setWeight(0)
        commands = box.createArea(alignment=ui.Area.ALIGN_HORIZONTAL, border=2)
        commands.setWeight(0)
        commands.createButton('new-addon-path', wg.Button.STYLE_MINI)
        commands.createButton('delete-addon-path', wg.Button.STYLE_MINI)
        #commands.setWeight(1)
        #commands.addSpacer()
        
        #area.setExpanding(False)
        #area.setBorder(10)
        #area.createButton('preferences')
        
        #area.setExpanding(False)
        #entry = area.createArea(alignment=ui.Area.ALIGN_HORIZONTAL)
        #entry.setBorder(10)
        #entry.setExpanding(False)

        # The name of the profile.
        #entry.setWeight(0)
        #entry.createText('about-button-help')
        #entry.setWeight(0)
        #entry.createButton('about')
