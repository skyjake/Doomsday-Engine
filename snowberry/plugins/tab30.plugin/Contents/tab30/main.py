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

## @file main.py Addons Tab
##
## This plugin handles commands and notifications from the addons
## tree.  It also implements the load order dialog, the addon
## inspector, and the addon installation dialog.

import os, time, string
import paths, events, ui, language
import sb.util.dialog
import sb.widget.button as wg
import sb.widget.list
import sb.profdb as pr
import sb.confdb as st
import sb.aodb as ao
import sb.addon
import tab30.installer
import tab30.loadorder
import tab30.inspector

ADDONS = 'tab-addons'


# Popup menu items.
addonItems = ['addon-info',
              'addon-settings',
              'uninstall-addon']

categoryItems = ['check-category',
                 'uncheck-category']

commonItems = ['-',
               'install-addon',
               'load-order',
               '-',
               'expand-all-categories',
               'collapse-all-categories']


def init():
    # Create the Addons page.
    area = ui.createTab(ADDONS)
    area.setBorderDirs(ui.BORDER_NOT_BOTTOM)
    
    # Let's create the addons tree...
    area.setWeight(1)

    global tree
    tree = area.createTree()

    # Add all the categories into the tree.
    tree.createCategories()

    # The addon control buttons.
    area.setWeight(0)
    area.setBorderDirs(ui.BORDER_NOT_TOP)
    buttonArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=4)

    buttonArea.setWeight(0)

    # Install (+) button for installing new addons.
    buttonArea.setBorderDirs(ui.BORDER_TOP | ui.BORDER_RIGHT)
    buttonArea.createButton('install-addon', wg.Button.STYLE_MINI)

    global uninstallButton
    uninstallButton = buttonArea.createButton('uninstall-addon',
                                              wg.Button.STYLE_MINI)
    uninstallButton.disable()

    # The Refresh button reloads all addons.
    button = buttonArea.createButton('refresh-addon-database', wg.Button.STYLE_MINI)
    button.resizeToBestSize()   

    buttonArea.setWeight(3)
    buttonArea.addSpacer()
    buttonArea.setWeight(0)

    global settingsButton
    settingsButton = buttonArea.createButton('addon-settings')
    settingsButton.disable()
    
    buttonArea.setBorderDirs(ui.BORDER_TOP)
    buttonArea.createButton('show-addon-paths')

    ui.addMenuCommand(ui.MENU_TOOLS, 'load-order')

    # Registering a notification listener.
    events.addNotifyListener(handleNotification, ['active-profile-changed',
                                                  'active-profile-refreshed',
                                                  'addon-popup-request',
                                                  'addon-tree-selected',
                                                  'addon-installed',
                                                  'addon-uninstalled',
                                                  'addon-database-reloaded'])

    # Registering a command listener to handle the Command events sent
    # by the buttons created above.
    events.addCommandListener(handleCommand, ['install-addon',
                                              'uninstall-addon',
                                              'addon-info',
                                              'addon-settings',
                                              'load-order',
                                              'expand-all-categories',
                                              'collapse-all-categories',
                                              'check-category',
                                              'uncheck-category',
                                              'refresh-addon-database'])
                                              
    # Changing the current addon in the tree is done using commands with
    # the name of the addon as an identifier.
    events.addCommandListener(handleAnyCommand)                                            


def handleNotification(event):
    """This is called when someone sends a notification event.

    @param event An events.Notify object.
    """
    if event.hasId('active-profile-changed') or \
           event.hasId('active-profile-refreshed'):
        # Fill the tree with an updated listing of addons.
        tree.populateWithAddons(pr.getActive())
        settingsButton.disable()

    elif event.hasId('addon-popup-request'):
        # The addon tree is requesting an updated popup menu.
        menu = []
        if event.addon:
            for m in addonItems:
                menu.append(m)
        elif event.category:
            for m in categoryItems:
                menu.append(m)
        menu += commonItems
        tree.setPopupMenu(menu)

    elif event.hasId('addon-tree-selected'):
        # Enable the settings button if there are settings for this
        # addon.
        if st.haveSettingsForAddon(event.getSelection()):
            settingsButton.enable()
        else:
            settingsButton.disable()

        # Can this be uninstalled?
        uninstallButton.enable(ao.exists(event.getSelection()) and
                               ao.get(event.getSelection()).getBox() == None)

    elif event.hasId('addon-installed'):
        tree.removeAll()
        tree.createCategories()
        tree.populateWithAddons(pr.getActive())
        tree.selectAddon(event.addon)

    elif event.hasId('addon-database-reloaded'):
        tree.removeAll()
        tree.createCategories()
        tree.populateWithAddons(pr.getActive())

    elif event.hasId('addon-uninstalled'):
        tree.populateWithAddons(pr.getActive())


def handleAnyCommand(event):
    """Handle any command. This is called whenever a command is broadcasted.
    Must not do anything slow."""
    
    # Is the command an addon identifier?
    if ao.exists(event.getId()):
        # Select the addon in the tree.
        tree.selectAddon(event.getId())
        

def handleCommand(event):
    """This is called when someone sends a command event.

    @param event An events.Command object."""

    # Figure out which addon has been currently selected in the addons
    # tree.  The action will target this addon.
    selected = ''

    if event.hasId('refresh-addon-database'):
        ao.refresh()
        return

    elif event.hasId('install-addon'):
        tab30.installer.run()

    elif event.hasId('uninstall-addon'):
        addon = tree.getSelectedAddon()
        if not ao.exists(addon):
            return
        
        # Make sure the user want to uninstall.
        dialog, area = sb.util.dialog.createButtonDialog(
            'uninstall-addon-dialog',
            ['no', 'yes'], 'no')

        #message = area.createFormattedText()
        area.setExpanding(True)
        text = language.translate('uninstall-addon-query')
        area.createRichText(language.expand(text, language.translate(addon)))

        if dialog.run() == 'yes':
            ao.uninstall(tree.getSelectedAddon())

    elif event.hasId('addon-info'):
        addon = tree.getSelectedAddon()
        # Show the Addon Inspector.
        tab30.inspector.run(ao.get(addon))

    elif event.hasId('addon-settings'):
        if tree.getSelectedAddon():
            command = events.Command('show-addon-settings')
            command.addon = tree.getSelectedAddon()
            events.send(command)        

    elif event.hasId('load-order'):
        # Open the Load Order dialog.
        tab30.loadorder.run(pr.getActive())

    elif event.hasId('expand-all-categories'):
        tree.expandAllCategories(True)

    elif event.hasId('collapse-all-categories'):
        tree.expandAllCategories(False)

    elif event.hasId('check-category'):
        tree.checkSelectedCategory(True)

    elif event.hasId('uncheck-category'):
        tree.checkSelectedCategory(False)

