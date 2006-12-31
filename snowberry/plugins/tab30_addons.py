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

## @file tab30_addons.py Addons Tab
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
import logger

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
    area.setBorderDirs(ui.BORDER_ALL)
    buttonArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=2)
    buttonArea.setBorderDirs(ui.BORDER_LEFT_RIGHT)

    buttonArea.setWeight(0)

    # Install (+) button for installing new addons.
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
    
    buttonArea.createButton('load-order')

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
        # Open the file selection dialog.
        for selection in \
                chooseAddons('install-addon-dialog',
                             language.translate('install-addon-title'),
                             'install'):
            try:
                ao.install(selection)
            except Exception, ex:
                logger.add(logger.HIGH, 'error-addon-installation-failed',
                           selection, str(ex))

		logger.show()

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
        showInspector(ao.get(addon))

    elif event.hasId('addon-settings'):
        if tree.getSelectedAddon():
            command = events.Command('show-addon-settings')
            command.addon = tree.getSelectedAddon()
            events.send(command)        

    elif event.hasId('load-order'):
        # Open the Load Order dialog.
        showLoadOrder(pr.getActive())

    elif event.hasId('expand-all-categories'):
        tree.expandAllCategories(True)

    elif event.hasId('collapse-all-categories'):
        tree.expandAllCategories(False)

    elif event.hasId('check-category'):
        tree.checkSelectedCategory(True)

    elif event.hasId('uncheck-category'):
        tree.checkSelectedCategory(False)


def showInspector(addon):
    """Show a dialog box that contains a lot of information (all there
    is to know) about the addon.

    @param An addons.Addon object.
    """
    ident = addon.getId()
    
    dialog, area = sb.util.dialog.createButtonDialog(
        'addon-inspector-dialog',
         ['ok'], 'ok', size=(570, 450))

    msg = ""

    msg += '<h3>' + language.translate(ident) + '</h3>'
    if language.isDefined(ident + '-readme'):
        msg += "<p>" + language.translate(ident + '-readme')

    def makeField(header, content):
        return '<tr><td width="20%" bgcolor="#E8E8E8" align="right"><b>' + header + \
               '</b><td width="80%">' + content

    beginTable = '<p><table width="100%" border=0 cellpadding=6 cellspacing=0>'
    endTable = '</table>'

    #
    # General Information
    #
    msg += beginTable
    msg += makeField('Summary:', language.translate(ident + '-summary', '-'))
    msg += makeField('Version:', language.translate(ident + '-version', '-'))
    msg += makeField('Last Modified:',
                     time.strftime("%a, %d %b %Y %H:%M:%S",
                                   time.localtime(addon.getLastModified())))
    msg += makeField('Author(s):', language.translate(ident + '-author', '-'))
    msg += makeField('Contact:', language.translate(ident + '-contact', '-'))
    msg += makeField('Copyright:',
                     language.translate(ident + '-copyright', '-'))
    msg += makeField('License:', language.translate(ident + '-license', '-'))
    msg += makeField('Category:', addon.getCategory().getPath())
    msg += makeField('Format:', language.translate(addon.getType()))
    msg += makeField('Identifier:', addon.getId())
    msg += endTable

    #
    # Raw Dependencies
    #
    msg += '<h3>Dependencies</h3>' + beginTable
    allDeps = []
    
    # Excluded categories.
    dep = []
    for category in addon.getExcludedCategories():
        dep.append(category.getPath())
    allDeps.append(('Excluded Categories:', dep))

    # Keywords.
    for type, label in [(sb.addon.EXCLUDES, 'Excludes:'),
                        (sb.addon.REQUIRES, 'Requires:'),
                        (sb.addon.PROVIDES, 'Provides:'),
                        (sb.addon.OFFERS, 'Offers:')]:
        # Make a copy of the list so we can modify it.
        dep = []
        for kw in addon.getKeywords(type):
            dep.append(kw)
        allDeps.append((label, dep))

    # Create a table out of each dependency type.
    for heading, listing in allDeps:
        msg += makeField(heading, string.join(listing, '<br>'))

    msg += endTable

    #
    # Content Analysis
    # 
    msg += '<h3>Contents</h3>' + beginTable
    msg += makeField('Content Path:', addon.getContentPath())
    msg += endTable
    
    #msg += "<p>format-specific data"
    #msg += "<br>content analysis, size"
    #msg += "<br>list of files, if a bundle"

    #msg += '<h3>Identifiers</h3>'
    #msg += "<br>all internal identifiers used by the addon"

    #msg += '<h3>Metadata</h3>'
    #msg += "<br>metadata analysis, source"

    text = area.createFormattedText()
    text.setMinSize(500, 200)
    text.setText(msg)
    dialog.run()        


def showLoadOrder(profile):
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


def chooseAddons(dialogId, title, actionButton):
    """Opens an addon selection dialog.

    @param title Title of the dialog.

    @param actionButton The button that will perform the affirmative
    action of the dialog.
    """
    dialog, area = sb.util.dialog.createButtonDialog(dialogId, 
        ['addon-dialog-add-to-custom-folders', '', 'cancel', actionButton], 
        actionButton)

    dialog.addEndCommand('addon-dialog-add-to-custom-folders')

    # The action button is disabled until a valid selection has been
    # made.
    dialog.disableWidget(actionButton)

    area.setWeight(0)
    folderArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL)
    folderArea.setExpanding(False)
    folderArea.setBorder(2, ui.BORDER_NOT_LEFT)
    folderArea.setWeight(0)
    folderArea.createText('addon-dialog-folder').resizeToBestSize()
    folderArea.setBorderDirs(ui.BORDER_ALL)
    folderArea.setWeight(1)
    pathField = folderArea.createTextField('')
    pathField.setText(os.getcwd())
    pathField.focus()
    folderArea.setWeight(0)
    browseButton = folderArea.createButton('addon-dialog-folder-browse',
                                           style=wg.Button.STYLE_MINI)
    folderArea.setWeight(0)
    folderArea.addSpacer()
    folderArea.setBorderDirs(ui.BORDER_NOT_RIGHT)
    uninstButton = folderArea.createButton('addon-dialog-folder-uninstalled')

    def goToUninstalled():
        pathField.setText(paths.getUserPath(paths.UNINSTALLED))
        dialog.disableWidget('addon-dialog-add-to-custom-folders')

    uninstButton.addReaction(goToUninstalled)

    area.createText('addon-dialog-found')
    area.setWeight(1)
    foundList = area.createList('', style=sb.widget.list.List.STYLE_COLUMNS)
    foundList.setMinSize(500, 300)

    area.setWeight(0)
    area.createText('addon-dialog-addons-copied', maxLineLength=70).resizeToBestSize()

    def selectAction():
        dialog.enableWidget(actionButton)

    foundList.addReaction(selectAction)

    for col, width in [('name', 300), ('type', 180)]:
        foundList.addColumn('addon-dialog-' + col, width)

    def updateList():
        # Update the found addons list.
        foundList.removeAllItems()
        dialog.disableWidget(actionButton)
        extensions = ao.getAddonExtensions() + ['manifest']

        # This should be done in addons.py.
        fileNames = os.listdir(pathField.getText())
        for name in fileNames:
            type = ''
            for ext in extensions:
                if paths.hasExtension(ext, name):
                    type = ext
                    break

            if not type:
                # Unknown files are skipped.
                continue

            # Manifests don't appear in the list if the corresponding
            # addon is in the same directory.
            if paths.hasExtension('manifest', name):
                foundSame = False

                # Identifier of the addon the manifest belongs to.
                manifestId = paths.getBase(name)

                # See if the addon is in the list.
                for other in fileNames:
                    if other == name:
                        continue
                    if manifestId == ao.formIdentifier(other)[0]:
                        foundSame = True
                        break                    
                if foundSame:
                    # Don't add it.
                    continue
            
            foundList.addItemWithColumns(
                name, name, language.translate('addon-dialog-type-' + type))

    # Update reactions.
    def pathChanged():
        if os.path.exists(pathField.getText()):
            updateList()
            if pathField.getText() != paths.getUserPath(paths.UNINSTALLED):
                dialog.enableWidget('addon-dialog-add-to-custom-folders')
        else:
            dialog.disableWidget('addon-dialog-add-to-custom-folders')

    def browseAction():
        # Show a directory browser.
        selection = sb.util.dialog.chooseFolder('addon-dialog-browse-prompt',
                                                pathField.getText())
        if len(selection):
            pathField.setText(selection)

    # The initial contents of the list.
    updateList()

    pathField.addReaction(pathChanged)
    browseButton.addReaction(browseAction)

    dialog.addEndCommand(actionButton)
    result = dialog.run()
    if result == actionButton:
        addonFiles = map(lambda name: os.path.join(pathField.getText(), name),
                         foundList.getSelectedItems())

        # Include any associated manifests.
        for name in addonFiles:
            manifest = ao.formIdentifier(name)[0] + '.manifest'
            manifestFile = os.path.join(pathField.getText(), manifest)
            if manifest not in addonFiles and os.path.exists(manifestFile):
                addonFiles.append(manifestFile)
                #print 'including ' + manifestFile + ' due to ' + name

        return addonFiles

    elif result == 'addon-dialog-add-to-custom-folders':
        paths.addAddonPath(pathField.getText())
        ao.refresh()
        return []

    # The dialog was canceled.
    return []
