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

## @file Configuration Wizard

import ui, paths, events, language
import widgets as wg
import settings as st
import profiles as pr
import addons as ao


HAS_BEEN_RUN = 'setup-wizard-shown'


def init():
    # Create the Rerun button in the Preferences Command area.
    #area = ui.getArea(ui.Area.PREFCOMMAND)
    #area.createButton('run-setup-wizard')
    
    # Register a listener for detecting the completion of Snowberry
    # startup.
    events.addNotifyListener(handleNotify, ['init-done', 'wizard-selected'])

    # Listen for wizard commands.
    events.addCommandListener(handleCommand, ['run-setup-wizard'])
    
    # Commands for the popup menu.
    ui.addPopupMenuCommand(1, 'run-setup-wizard')    


def handleNotify(event):
    """Handle notifications."""

    if event.hasId('init-done'):
        # If this is the first launch ever, we'll display the wizard.
        if not pr.getDefaults().getValue(HAS_BEEN_RUN):
            runWizard()

    elif event.hasId('wizard-selected'):
        prof = pr.get(event.getSelection())
        if prof:
            pr.setActive(prof)
        else:
            pr.setActive(pr.getDefaults())
        

def handleCommand(event):
    """Handle commands."""
    
    if event.hasId('run-setup-wizard'):
        runWizard()


class ProfileWizardPage (ui.WizardPage):
    """A profile page is skipped if it's not checked in the game list."""

    def __init__(self, parentWizard, identifier, gameList):
        ui.WizardPage.__init__(self, parentWizard, identifier)
        self.gameList = gameList

    def setGameList(self, list):
        self.gameList = list

    def getAdjacent(self, theNext):
        next = self
        # Loop until a good page is found.
        while True:
            # Which direction are we going?
            if theNext:
                next = ui.WizardPage.GetNext(next)
            else:
                next = ui.WizardPage.GetPrev(next)

            # Should we skip or stop?
            if (next != None and self.gameList != None and
                next.getId() in self.gameList.getItems()):
                if next.getId() in self.gameList.getSelectedItems():
                    # Activate the correct profile.
                    break
            else:
                # Not a profile page.
                break
        return next

    def GetNext(self):
        return self.getAdjacent(True)
    
    def GetPrev(self):
        return self.getAdjacent(False)


def runWizard():
    """Run the wizard dialog."""

    # Make sure the help panel isn't updated during the wizard.
    events.send(events.Command('freeze'))

    suggested = {
        'doom1': 'DOOM.WAD',
        'doom1-share': 'DOOM1.WAD',
        'doom1-ultimate': 'DOOM.WAD',
        'doom2': 'DOOM2.WAD',
        'doom2-tnt': 'TNT.WAD',
        'doom2-plut': 'PLUTONIA.WAD',
        'heretic-ext': 'HERETIC.WAD',
        'heretic-share': 'HERETIC.WAD',
        'heretic': 'HERETIC.WAD',
        'hexen': 'HEXEN.WAD',
        'hexen-demo': 'HEXEN.WAD',
        'hexen-dk': 'HEXEN.WAD'
    }
    
    # Make the Defaults profile active.
    pr.setActive(pr.getDefaults())
    
    wiz = ui.WizardDialog(language.translate('setup-wizard'),
                          paths.findBitmap('wizard'))

    # Language selection page.
    langPage = ui.WizardPage(wiz, 'wizard-language')
    area = langPage.getArea()
    area.createText('wizard-language-explanation').resizeToBestSize()
    area.createSetting(st.getSetting('language'))

    # Game selection.
    gamePage = ProfileWizardPage(wiz, 'wizard-games', None)
    gamePage.follows(langPage)
    area = gamePage.getArea()
    area.createText('wizard-select-games').resizeToBestSize()
    games = area.createList('', style=wg.List.STYLE_CHECKBOX)
    games.setMinSize(50, 200)
    gamePage.setGameList(games)

    def allGames():
        # Check the entire list.
        for item in games.getItems():
            games.checkItem(item, True)

    def clearGames():
        # Uncheck the entire list.
        for item in games.getItems():
            games.checkItem(item, False)

    controls = area.createArea(alignment=ui.Area.ALIGN_HORIZONTAL, border=2)
    controls.setWeight(0)
    controls.createButton('wizard-games-all').addReaction(allGames)
    controls.createButton('wizard-games-clear').addReaction(clearGames)

    # The pages will be linked together.
    previousPage = gamePage

    deathKingsWad = None

    # We'll do this dynamically.
    checkedProfiles = ['doom1', 'doom2', 'heretic', 'hexen']
    # Only display the system profiles in the wizard (not any user
    # profiles).
    profiles = pr.getProfiles(lambda p: p.isSystemProfile())
    for p in profiles:
        if p is not pr.getDefaults():
            games.addItem(p.getId(), p.getId() in checkedProfiles)

            # Create a page for the profile.
            page = ProfileWizardPage(wiz, p.getId(), games)

            # Link the pages together.
            page.follows(previousPage)
            previousPage = page
            
            area = page.getArea()

            area.createText('wizard-locate-iwad')

            # The suggestion.
            if suggested.has_key(p.getId()):
                sug = area.createText('wizard-suggested-iwad',
                                      ' ' + suggested[p.getId()])

            area.createSetting(st.getSetting('iwad'))

            if p.getId() == 'hexen-dk':
                area.createLine()
                area.createText('deathkings-selection-title').setHeadingStyle()
                
                # Death Kings is an extension to Hexen.  It uses the
                # same Hexen IWAD, but also an addon IWAD.
                area.createText('wizard-locate-iwad-deathkings')

                deathKingsWad = area.createTextField('')

                area.setExpanding(False)
                area.setWeight(0)
                browseButton = area.createButton('browse')

                def browseDeathKings():
                    # Open a file browser.
                    selection = ui.chooseFile('deathkings-selection-title',
                                              '', True,
                                              [('file-type-wad', 'wad')])
                
                    if len(selection) > 0:
                        deathKingsWad.setText(selection)

                browseButton.addReaction(browseDeathKings)

    # Launch options.
    quitPage = ProfileWizardPage(wiz, 'wizard-launching', games)
    quitPage.follows(previousPage)
    area = quitPage.getArea()
    area.createText('wizard-launching-explanation').resizeToBestSize()
    area.createSetting(st.getSetting('quit-on-launch'))

    if wiz.run(langPage) == 'ok':
        # Show the profiles that now have an IWAD.
        for prof in profiles:
            if prof.getValue('iwad', False) != None:
                pr.show(prof.getId())
            else:
                pr.hide(prof.getId())

        # Install the Death Kings WAD?
        if deathKingsWad:
            try:
                ident = ao.install(deathKingsWad.getText())

                # Attach the WAD as an addon.
                kingsProfile = pr.get('hexen-dk')
                kingsProfile.useAddon(ident)
            except:
                # TODO: Handle error.
                pass

        # The wizard will only be shown once automatically.
        pr.getDefaults().setValue(HAS_BEEN_RUN, 'yes', False)
    else:
        # Anything to do if the wizard is canceled?
        pass

    # This'll destroy all the pages of the wizard as well.
    wiz.destroy()

    # Enable help panel updates again.
    events.send(events.Command('unfreeze'))
