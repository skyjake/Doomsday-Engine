# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#   Antti Kopponen <antti.kopponen@tut.fi>
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

## @file profilelist.py Profile List

import ui, events, widgets
import sb.util.dialog
import sb.widget.button as wg
import sb.widget.text as wt
import sb.widget.list
import sb.profdb as pr
import sb.confdb as st
import language
import paths
from ui import ALIGN_HORIZONTAL

USE_MINIMAL_MODE = st.getSystemBoolean('profile-minimal-mode')

profileList = None
deleteButton = None
dupeButton = None

# If set to true, the profile list won't be updated on notifications.
profileListDisabled = False

# Popup menus.
defaultsMenu = ['reset-profile', '-', 'new-profile', 'unhide-profiles']

normalMenu = ['hide-profile',
              'rename-profile',
              'duplicate-profile',
              'reset-profile',
              'delete-profile',
              '-',
              'new-profile',
              'unhide-profiles']

systemMenu = ['hide-profile',
              'rename-profile',
              'duplicate-profile',
              'reset-profile',
              '-',
              'new-profile',
              'unhide-profiles']


def getIconSize():
    """Determines the size of profile icons.
    @return Size of profile icons in pixels, as a string."""
    if st.getSystemBoolean('profile-large-icons'):
        return 50
    else:
        return 27


def makeHTML(name, iconName, gameName=None, boldName=False):
    # The icon for the game component.
    iconSize = getIconSize()
    iconPath = widgets.iconManager.getBitmapPath(iconName, (iconSize, iconSize))
    
    if boldName:
        nameStyle = ('<b>', '</b>')
    else:
        nameStyle = ('', '')

    if st.getSystemBoolean('profile-large-icons'): 
        padding = 5
    else:
        padding = 3
    
    html = '<table width="100%%" border=0 cellspacing=2 cellpadding=%i>' % padding
    html += '<tr><td width="%i"' % iconSize + '><img width="%i"' % iconSize
    html += ' height="%i"' % iconSize + ' src="' + iconPath + '"></td><td>' 
    html += '<font size="+1">' + nameStyle[0] + name + nameStyle[1] + '</font>'
    if gameName:
        html += '<br><font color="#808080" size="-1">' + gameName + '</font>' 
    html += '</td></tr></table>'
    return html 


def makeProfileHTML(profile):
    if profile is pr.getDefaults():
        return makeHTML(profile.getName(), 'defaults', boldName=True)
        #iconPath = paths.findBitmap('defaults')
        #iconSize = iconSizeString()
        #return '<table width="100%" border=0 cellspacing=2 cellpadding=5>' + \
        #   '<tr><td width="' + iconSize + \
        #   '"><img width="%s" height="%s"' % (iconSize, iconSize) + \
        #   ' src="' + iconPath + \
        #   '"><td align=left><font size="+1"><b>' + profile.getName() + \
        #   '</b></font></table>'
    else:
        game = 'game-undefined'
        for c in profile.getComponents():
            if c[:5] == 'game-':
                game = c
                break

        return makeHTML(profile.getName(), language.translate(game + '-icon'),
                        language.translate(game))


def init():
    # Create the profile list and the profile control buttons.
    area = ui.getArea(ui.PROFILES)

    global profileList
    
    area.setBorder(0)
    
    if USE_MINIMAL_MODE:
        area.setWeight(1)
        area.addSpacer()
        area.setWeight(3)
        profileList = area.createDropList('profile-list')
        profileList.setSorted()
        area.setWeight(1)
        area.addSpacer()
        
    else:
        # Normal profile list mode.
        area.setWeight(1)
        profileList = area.createFormattedList("profile-list")

        if not st.getSystemBoolean('profile-hide-buttons'):
            # This should be a small button.
            area.setWeight(0)
            area.setBorder(3)
            controls = area.createArea(alignment=ALIGN_HORIZONTAL, border=2)
            controls.setExpanding(False)
            #area.setExpanding(False)
            controls.setWeight(0)
            controls.createButton('new-profile', wg.Button.STYLE_MINI)

            global deleteButton
            deleteButton = controls.createButton('delete-profile',
                                                 wg.Button.STYLE_MINI)

            global dupeButton
            dupeButton = controls.createButton('duplicate-profile',
                                               wg.Button.STYLE_MINI)

    # Set the title graphics.
    global bannerImage
    try:
        area = ui.getArea(ui.TITLE)
        bannerImage = area.createImage('banner-default')
    except:
        # There is no title area.
        bannerImage = None

    # Register a listener for notifications.
    events.addNotifyListener(notifyHandler, ['quit', 'language-changed',
                                             'profile-updated',
                                             'active-profile-changed',
                                             'profile-list-selected'])
                                             
    # Register a listener for commands.
    events.addCommandListener(commandHandler, ['freeze', 'unfreeze',
                                               'new-profile', 
                                               'rename-profile',
                                               'reset-profile', 
                                               'delete-profile',
                                               'duplicate-profile',
                                               'hide-profile', 
                                               'unhide-profiles'])

    # Commands for the menu.
    ui.addMenuCommand(ui.MENU_PROFILE, 'new-profile', group=ui.MENU_GROUP_PROFDB)
    ui.addMenuCommand(ui.MENU_PROFILE, 'unhide-profiles', group=ui.MENU_GROUP_PROFDB)

    ui.addMenuCommand(ui.MENU_PROFILE, 'reset-profile', group=ui.MENU_GROUP_PROFILE)
    ui.addMenuCommand(ui.MENU_PROFILE, 'rename-profile', group=ui.MENU_GROUP_PROFILE)
    ui.addMenuCommand(ui.MENU_PROFILE, 'duplicate-profile', group=ui.MENU_GROUP_PROFILE)
    ui.addMenuCommand(ui.MENU_PROFILE, 'hide-profile', group=ui.MENU_GROUP_PROFILE)
    ui.addMenuCommand(ui.MENU_PROFILE, 'delete-profile', group=ui.MENU_GROUP_PROFILE)


def addListItem(profile, toIndex=None):
    if not USE_MINIMAL_MODE:
        profileList.addItem(profile.getId(), makeProfileHTML(profile), toIndex)
    else:
        profileList.addItem(profile.getId(), toIndex)


def notifyHandler(event):
    "This is called when a Notify event is broadcasted."

    # We are interested in notifications that concern profiles.
    if event.hasId('quit'):
        # Save the current profile configuration.
        pr.save()

    elif event.hasId('language-changed'):
        # Just update the Defaults, because it's the only profile whose name
        # is translated.
        addListItem(pr.getDefaults())

    elif event.hasId('profile-updated'):
        # A profile has been loaded or updated.  Make sure it's in the
        # list.
        p = event.getProfile()

        # The Defaults profile should be first in the list.
        if p is pr.getDefaults():
            destIndex = 0
        else:
            # The new profile will be added to wherever the widget
            # wants to put it.
            destIndex = None

        if profileList.hasItem(p.getId()):
            # This already exists in the list.
            if p.isHidden():
                # Remove it completely.
                profileList.removeItem(p.getId())

                # Update the selected item since the active one is now
                # hidden.
                pr.setActive(pr.get(profileList.getSelectedItem()))
            else:
                # Just update the item.
                addListItem(p)

        elif not p.isHidden():
            # The item does not yet exist in the list.
            addListItem(p, toIndex=destIndex)

    elif event.hasId('active-profile-changed') and not profileListDisabled:
        # Highlight the correct profile in the list.
        profileList.selectItem(pr.getActive().getId())

        if pr.getActive() is pr.getDefaults():
            if deleteButton: deleteButton.disable()
            if dupeButton: dupeButton.disable()
            ui.disableMenuCommand('rename-profile')
            ui.disableMenuCommand('delete-profile')
            ui.disableMenuCommand('hide-profile')
            ui.disableMenuCommand('duplicate-profile')
            profileList.setPopupMenu(defaultsMenu)
        else:
            isSystem = pr.getActive().isSystemProfile()
            if deleteButton: deleteButton.enable(not isSystem)
            if dupeButton: dupeButton.enable()
            ui.enableMenuCommand('rename-profile')
            ui.enableMenuCommand('delete-profile', not isSystem)
            ui.enableMenuCommand('hide-profile')
            ui.enableMenuCommand('duplicate-profile')
            if isSystem:
                menu = systemMenu
            else:
                menu = normalMenu
            profileList.setPopupMenu(menu)

        # Update the banner image.
        if bannerImage:
            bannerImage.setImage(pr.getActive().getBanner())

    elif event.hasId('profile-list-selected'):
        # Change the currently active profile.
        p = pr.get(event.getSelection())
        pr.setActive(p)

        # Double-clicking causes a Play command.
        if event.isDoubleClick() and p is not pr.getDefaults():
            events.sendAfter(events.Command('play'))


def commandHandler(event):
    """This is called when a Command event is broadcasted."""

    global profileListDisabled

    if event.hasId('freeze'):
        profileListDisabled = True

    elif event.hasId('unfreeze'):
        profileListDisabled = False
        #notifyHandler(events.Notify('active-profile-changed'))
        pr.refresh()

    elif event.hasId('new-profile'):
        dialog, area = sb.util.dialog.createButtonDialog(
            'new-profile-dialog',
            ['cancel', 'ok'], 'ok', resizable=False)
        dialog.disableWidget('ok')

        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        # The name of the profile.
        entry.setWeight(1)
        entry.setBorder(4, ui.BORDER_RIGHT)
        entry.createText('new-profile-name', ':', align=wt.Text.RIGHT)
        entry.setBorder(0)
        entry.setWeight(2)
        nameField = entry.createTextField('')
        nameField.focus()

        # Only enable the OK button if there is something in the name field.
        def buttonEnabler():
            dialog.enableWidget('ok', len(nameField.getText()) > 0)

        nameField.addReaction(buttonEnabler)

        # The game component.
        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        entry.setWeight(1)
        entry.setBorder(4, ui.BORDER_RIGHT)
        entry.createText('new-profile-game', ':', align=wt.Text.RIGHT)
        entry.setBorder(0)
        entry.setWeight(2)
        gameDrop = entry.createDropList('')

        for game in st.getGameComponents():
            gameDrop.addItem(game.getId())

        gameDrop.sortItems()

        # Select the first one.
        gameDrop.selectItem(gameDrop.getItems()[0])

        if dialog.run() == 'ok':
            # Get the values from the controls.
            pr.create(nameField.getText(), gameDrop.getSelectedItem())

    elif event.hasId('rename-profile'):
        dialog, area = sb.util.dialog.createButtonDialog(
            'rename-profile-dialog',
            ['cancel', 'ok'], 'ok', resizable=False)

        prof = pr.getActive()

        # A text field of the new name.
        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        # The name of the profile.
        entry.setWeight(1)
        entry.setBorder(4, ui.BORDER_RIGHT)
        entry.createText('rename-profile-name', ':', align=wt.Text.RIGHT)
        entry.setBorder(0)
        entry.setWeight(2)
        nameField = entry.createTextField('')
        nameField.setText(prof.getName())
        nameField.focus()

        # Only enable the OK button if there is something in the name
        # field.
        def buttonEnabler():
            dialog.enableWidget('ok', len(nameField.getText()) > 0)

        nameField.addReaction(buttonEnabler)

        if dialog.run() == 'ok':
            prof.setName(nameField.getText())
            # The profile list needs to be updated, too.
            events.send(events.ProfileNotify(prof))
            pr.refresh()

    elif event.hasId('reset-profile'):
        dialog, area = sb.util.dialog.createButtonDialog(
            'reset-profile-dialog',
            ['cancel', 'reset-profile-values', 'reset-profile-addons', 
            'reset-profile-everything'], 
            'cancel', resizable=False)

        text = language.translate('reset-profile-query')
        message = area.createRichText(
            language.expand(text, pr.getActive().getName()))

        # Accept these as dialog-closing commands.
        dialog.addEndCommand('reset-profile-values')
        dialog.addEndCommand('reset-profile-addons')
        dialog.addEndCommand('reset-profile-everything')
        
        result = dialog.run()
        if result == 'cancel':
            return

        resetValues = True
        resetAddons = True
        if result == 'reset-profile-values':
            resetAddons = False
        elif result == 'reset-profile-addons':
            resetValues = False
        pr.reset(pr.getActive().getId(), resetValues, resetAddons)
        pr.refresh()

    elif event.hasId('delete-profile'):
        # If this is a system profile, just hide it.
        if pr.getActive().isSystemProfile():
            pr.hide(pr.getActive().getId())
            return
        
        dialog, area = sb.util.dialog.createButtonDialog(
            'delete-profile-dialog',
            ['no', 'yes'], 'no', resizable=False)

        text = language.translate('delete-profile-query')
        area.createRichText(language.expand(text, pr.getActive().getName()))

        if dialog.run() == 'yes':
            # Get the values from the controls.
            pr.remove(pr.getActive().getId())

    elif event.hasId('duplicate-profile'):
        dialog, area = sb.util.dialog.createButtonDialog(
            'duplicate-profile-dialog',
            ['cancel', 'ok'], 'ok', resizable=False)

        text = language.translate('duplicating-profile')
        area.setWeight(1)
        area.createRichText(language.expand(text, pr.getActive().getName()))

        area.setWeight(1)
        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        # The name of the profile.
        entry.setWeight(1)
        entry.setBorder(4, ui.BORDER_RIGHT)
        entry.createText('new-profile-name', ':', align=wt.Text.RIGHT)
        entry.setBorder(0)
        entry.setWeight(3)
        nameField = entry.createTextField('')
        nameField.setText(pr.getActive().getName())
        nameField.focus()

        # Only enable the OK button if there is something in the name field.
        def buttonEnabler():
            dialog.enableWidget('ok', len(nameField.getText()) > 0)

        nameField.addReaction(buttonEnabler)

        if dialog.run() == 'ok':
            pr.duplicate(pr.getActive().getId(), nameField.getText())

    elif event.hasId('hide-profile'):
        pr.hide(pr.getActive().getId())

    elif event.hasId('unhide-profiles'):
        # Display a dialog bog for unhiding hidden profiles.
        hiddenProfiles = pr.getProfiles(lambda p: p.isHidden())

        dialog, area = sb.util.dialog.createButtonDialog(
            'unhide-profile-dialog',
            ['cancel', 'ok'], 'ok', resizable=False)

        area.setWeight(0)
        area.createText('unhiding-profiles')

        area.setWeight(3)
        area.setBorder(0)
        profList = area.createList('', sb.widget.list.List.STYLE_CHECKBOX)
        profList.setMinSize(50, 150)

        def selectAll():
            # Check the entire list.
            for item in profList.getItems():
                profList.checkItem(item, True)

        def clearAll():
            # Uncheck the entire list.
            for item in profList.getItems():
                profList.checkItem(item, False)

        area.setWeight(0)
        controls = area.createArea(alignment=ALIGN_HORIZONTAL,
                                   border=2)
        controls.setWeight(0)
        button = controls.createButton('unhide-select-all', 
            style=sb.widget.button.Button.STYLE_MINI)
        button.addReaction(selectAll)
        button.resizeToBestSize()
        button = controls.createButton('unhide-clear-all', 
            style=sb.widget.button.Button.STYLE_MINI)
        button.addReaction(clearAll)
        button.resizeToBestSize()

        for prof in hiddenProfiles:
            profList.addItem(prof.getId())

        if dialog.run() == 'ok':
            # Unhide all the selected items.
            selected = profList.getSelectedItems()
            for sel in selected:
                pr.show(sel)

            if len(selected):
                # Select the first one that was shown.
                pr.setActive(pr.get(selected[0]))
