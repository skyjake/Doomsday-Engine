# -*- coding: iso-8859-1 -*-
# $Id: main.py 4303 2007-01-01 19:06:40Z skyjake $
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

## @file loadorder.py Addon Load Order Dialog
##
## This module implements the load order dialog.

import os, time, string
import paths, events, ui, language
import sb.util.dialog
import sb.widget.button as wg
import sb.widget.list
import sb.profdb as pr
import sb.confdb as st
import sb.aodb as ao
import sb.addon


def run(profile):
    """Open the load order dialog.

    @param profile The profile whose order is being edited.
    """

    dialog, area = sb.util.dialog.createButtonDialog(
        'load-order-dialog',
        ['reset', '', 'cancel', 'ok'], 'ok')

    dialog.focusWidget('cancel')
    area.setWeight(0)
    area.createText('load-order-message')

    # Create the widgets that will be used to edit the order.
    area.setWeight(1)
    top = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=0)

    top.setWeight(3)
    listArea = top.createArea(border=3)
    listArea.setWeight(1)
    orderList = listArea.createList('')
    orderList.setMinSize(200, 200)

    top.setWeight(1)
    controlArea = top.createArea(border=3)
    controlArea.setWeight(0)
    controlArea.setExpanding(False)
    upButton = controlArea.createButton('load-order-move-up')
    downButton = controlArea.createButton('load-order-move-down')
    controlArea.addSpacer()
    firstButton = controlArea.createButton('load-order-move-top')
    lastButton = controlArea.createButton('load-order-move-bottom')

    def moveSelFirst():
        orderList.moveItem(orderList.getSelectedItem(), -65535)

    def moveSelLast():
        orderList.moveItem(orderList.getSelectedItem(), +65535)

    def moveSelUp():
        orderList.moveItem(orderList.getSelectedItem(), -1)

    def moveSelDown():
        orderList.moveItem(orderList.getSelectedItem(), +1)

    firstButton.addReaction(moveSelFirst)
    upButton.addReaction(moveSelUp)
    downButton.addReaction(moveSelDown)
    lastButton.addReaction(moveSelLast)

    # The final addons are automatically sorted according to the
    # profile's load order.
    finalAddons = profile.getFinalAddons()
    
    # Put all the finalized list of loaded addons into the list.
    for ident in finalAddons:
        orderList.addItem(ident)

    if len(finalAddons):
        orderList.selectItem(finalAddons[0])

    # We'll accept 'reset' as a dialog end command as well.
    dialog.addEndCommand('reset')

    # Run the dialog and handle the closing command.
    result = dialog.run()

    if result == 'ok':
        # This is the new load order for the profile.
        profile.setLoadOrder(orderList.getItems())

    elif result == 'reset':
        # Clear the profile's load order.
        profile.setLoadOrder([])

    else:
        # Nothing is done on cancel.
        pass

