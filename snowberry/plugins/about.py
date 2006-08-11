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

## @file about.py About dialog

import ui, events, language
import sb.util.dialog
import widgets as wg
import sb.confdb as st


def init():
    # Listen for the About button.
    events.addCommandListener(handleCommand, ['about'])
    
    # Commands for the popup menu.
    ui.addPopupMenuCommand(0, 'about')


def handleCommand(event):
    """Handle the About command and display the About Snowberry
    dialog.

    @param event  An events.Command event.
    """

    if event.hasId('about'):
        # Create the About dialog and show it.
        dialog, area = sb.util.dialog.createButtonDialog(
            'about-dialog', language.translate('about-title'), ['ok'], 'ok')

        content = area.createArea(alignment=ui.ALIGN_VERTICAL, border=0)
        content.setWeight(0)

        content.createText('').setText(
            language.translate('about-version') + ' ' +
            st.getSystemString('snowberry-version'))

        content.createText('about-subtitle')

        content.setBorder(6)
        content.setWeight(1)
        box = content.createArea(boxedWithTitle='about-credits')
        info = box.createFormattedText()
        info.setMinSize(300, 280)
        info.setText(language.translate('about-info'))
        
        dialog.run()
