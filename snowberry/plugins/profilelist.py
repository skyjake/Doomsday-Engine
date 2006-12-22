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

import ui, events
import sb.util.dialog
import sb.widget.button as wg
import sb.widget.list
import sb.profdb as pr
import sb.confdb as st
import language
import paths
from ui import ALIGN_HORIZONTAL

profileList = None

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


def makeHTML(name, game):
    # The icon for the game component.
    iconName = language.translate(game + '-icon')
    iconPath = paths.findBitmap(iconName)

    # The name of the game.
    gameName = language.translate(game)

    if st.getSystemBoolean('profile-large-icons'):
        iconSize = '50'
    else:
        iconSize = '24'
    
    return '<table width="100%" border=0 cellspacing=2 cellpadding=5>' + \
           '<tr><td width="' + iconSize + '"><img width="' + iconSize + \
           '" height="' + iconSize + '" src="' + iconPath + '"><td>' + \
           '<font size="+1"><b>' + name + '</b></font><br>' + \
           '<font color="#808080">' + gameName + '</font>' + \
           '</table>'


def makeProfileHTML(profile):
    if profile is pr.getDefaults():
        # There should be a real icon for the Defaults profile.
        iconPath = paths.findBitmap('defaults')
        return '<table width="100%" border=0 cellspacing=2 cellpadding=5>' + \
           '<tr><td align=left >' + \
           '<font size="+1"><b>' + \
           profile.getName() + \
           '</b></font>' + \
           '<td width="48"><img src="' + iconPath + '"></table>'

    else:
        c = 'game-other'
        for c in profile.getComponents():
            if c[:5] == 'game-':
                game = c
                break

        return makeHTML(profile.getName(), game)


def init():
    # Create the profile list and the profile control buttons.
    area = ui.getArea(ui.PROFILES)
    area.setWeight(1)
    area.setBorder(0) #3)

    global profileList
    profileList = area.createFormattedList("profile-list")

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


def notifyHandler(event):
    "This is called when a Notify event is broadcasted."

    # We are interested in notifications that concern profiles.
    if event.hasId('quit'):
        # Save the current profile configuration.
        pr.save()

    elif event.hasId('language-changed'):
        # Just update the Defaults.
        p = pr.getDefaults()
        profileList.addItem(p.getId(), makeProfileHTML(p))

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
                profileList.addItem(p.getId(), makeProfileHTML(p))

        elif not p.isHidden():
            # The item does not yet exist in the list.
            profileList.addItem(p.getId(), makeProfileHTML(p),
                                toIndex=destIndex)

    elif event.hasId('active-profile-changed') and not profileListDisabled:
        # Highlight the correct profile in the list.
        profileList.selectItem(pr.getActive().getId())

        if pr.getActive() is pr.getDefaults():
            deleteButton.disable()
            dupeButton.disable()
            profileList.setPopupMenu(defaultsMenu)
        else:
            deleteButton.enable()
            dupeButton.enable()
            profileList.setPopupMenu(normalMenu)

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
            language.translate('new-profile-title'),
            ['cancel', 'ok'], 'ok')
        dialog.disableWidget('ok')

        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        # The name of the profile.
        entry.setWeight(1)
        entry.createText('new-profile-name')
        entry.setWeight(2)
        nameField = entry.createTextField('')

        # Only enable the OK button if there is something in the name field.
        def buttonEnabler():
            dialog.enableWidget('ok', len(nameField.getText()) > 0)

        nameField.addReaction(buttonEnabler)

        # The game component.
        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        entry.setWeight(1)
        entry.createText('new-profile-game')
        entry.setWeight(1)
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
            language.translate('rename-profile-title'),
            ['cancel', 'ok'], 'ok')

        prof = pr.getActive()

        # A text field of the new name.
        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        # The name of the profile.
        entry.setWeight(1)
        entry.createText('rename-profile-name')
        entry.setWeight(2)
        nameField = entry.createTextField('')
        nameField.setText(prof.getName())

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
            language.translate('reset-profile-title'),
            ['no', 'yes'], 'no')

        text = language.translate('reset-profile-query')
        message = area.createRichText(
            language.expand(text, pr.getActive().getName()))

        if dialog.run() == 'yes':
            pr.reset(pr.getActive().getId())

    elif event.hasId('delete-profile'):
        dialog, area = sb.util.dialog.createButtonDialog(
            'delete-profile-dialog',
            language.translate('delete-profile-title'),
            ['no', 'yes'], 'no')

        text = language.translate('delete-profile-query')
        area.createRichText(language.expand(text, pr.getActive().getName()))

        if dialog.run() == 'yes':
            # Get the values from the controls.
            pr.remove(pr.getActive().getId())

    elif event.hasId('duplicate-profile'):
        dialog, area = sb.util.dialog.createButtonDialog(
            'duplicate-profile-dialog',
            language.translate('duplicate-profile-title'),
            ['cancel', 'ok'], 'ok')

        text = language.translate('duplicating-profile')
        area.setWeight(3)
        area.createRichText(language.expand(text, pr.getActive().getName()))

        area.setWeight(1)
        entry = area.createArea(alignment=ALIGN_HORIZONTAL)
        entry.setExpanding(False)

        # The name of the profile.
        entry.setWeight(1)
        entry.createText('new-profile-name')
        entry.setWeight(3)
        nameField = entry.createTextField('')
        nameField.setText(pr.getActive().getName())

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
            language.translate('unhide-profile-title'),
            ['cancel', 'ok'], 'ok')

        area.setWeight(0)
        area.createText('unhiding-profiles')

        area.setWeight(3)
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
        controls.createButton('unhide-select-all').addReaction(selectAll)
        controls.createButton('unhide-clear-all').addReaction(clearAll)

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
