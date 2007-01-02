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

## @file installer.py Addon Installer

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


def run():
    # Open the file selection dialog.
    for selection in chooseAddons('install-addon-dialog',
                                  language.translate('install-addon-title'),
                                  'install'):
        try:
            ao.install(selection)
        except Exception, ex:
            logger.add(logger.HIGH, 'error-addon-installation-failed',
                       selection, str(ex))

    logger.show()


def chooseAddons(dialogId, title, actionButton):
    """Opens an addon selection dialog.

    @param title Title of the dialog.

    @param actionButton The button that will perform the affirmative
    action of the dialog.
    """
    dialog, area = sb.util.dialog.createButtonDialog(dialogId, 
        ['cancel', actionButton], 
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
    pathField.select()
    pathField.focus()
    folderArea.setWeight(0)
    folderArea.setBorderDirs(ui.BORDER_NOT_RIGHT)
    browseButton = folderArea.createButton('addon-dialog-folder-browse',
                                           style=wg.Button.STYLE_MINI)
    #folderArea.addSpacer()
    #folderArea.setBorderDirs(ui.BORDER_NOT_RIGHT)
    
    folderCmdArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL)
    folderCmdArea.setExpanding(False)
    folderCmdArea.setBorder(6, ui.BORDER_NOT_TOP)
    folderCmdArea.setWeight(1)
    folderCmdArea.addSpacer()
    folderCmdArea.setWeight(0)
        
    uninstButton = folderCmdArea.createButton('addon-dialog-folder-uninstalled')
   
    # Add to Custom Folders button.
    folderCmdArea.setBorder(6, ui.BORDER_LEFT | ui.BORDER_BOTTOM)
    addToMyButton = folderCmdArea.createButton('addon-dialog-add-to-custom-folders')

    def goToUninstalled():
        pathField.setText(paths.getUserPath(paths.UNINSTALLED))
        pathField.select()
        addToMyButton.disable()

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

    for col, width in [('name', None), ('type', 180)]:
        foundList.addColumn('addon-dialog-' + col, width)

    def updateList():
        # Update the found addons list.
        foundList.clear()
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
                addToMyButton.enable()
        else:
            addToMyButton.disable()

    def browseAction():
        # Show a directory browser.
        selection = sb.util.dialog.chooseFolder('addon-dialog-browse-prompt',
                                                pathField.getText())
        if len(selection):
            pathField.setText(selection)
            pathField.select()

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
        events.send(events.Notify('addon-paths-changed'))
        ao.refresh()
        return []

    # The dialog was canceled.
    return []
