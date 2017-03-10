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

## @file wizard.py Configuration Wizard

import ui, paths, events, language, sb.util.dialog
from sb.util.dialog import WizardPage, WizardDialog
import sb.widget.button as wb
import sb.widget.text as wt
import sb.widget.list
import sb.confdb as st
import sb.profdb as pr
import sb.aodb as ao


HAS_BEEN_RUN = 'setup-wizard-shown' # see profdb.reset()


def init():
    # Register a listener for detecting the completion of Snowberry
    # startup.
    events.addNotifyListener(handleNotify, ['init-done', 'wizard-selected'])

    # Listen for wizard commands.
    events.addCommandListener(handleCommand, ['run-setup-wizard'])
    
    # Commands for the popup menu.
    ui.addMenuCommand(ui.MENU_TOOLS, 'run-setup-wizard', group=ui.MENU_GROUP_APP)    


def handleNotify(event):
    """Handle notifications."""

    if event.hasId('init-done'):
        # If this is the first launch ever, we'll display the wizard.
        if not pr.getDefaults().getValue(HAS_BEEN_RUN):
            runWizard()
        

def handleCommand(event):
    """Handle commands."""
    
    if event.hasId('run-setup-wizard'):
        runWizard()


class ProfileWizardPage (WizardPage):
    """A profile page is skipped if it's not checked in the game list."""

    def __init__(self, parentWizard, identifier, gameList):
        WizardPage.__init__(self, parentWizard, identifier)
        self.gameList = gameList
        self.iwadText = None

    def setGameList(self, list):
        self.gameList = list

    def update(self):
        if self.iwadText:
            self.iwadText.getFromProfile(pr.getActive())
            self.iwadText.select()

    def getAdjacent(self, theNext):
        next = self
        # Loop until a good page is found.
        while True:
            # Which direction are we going?
            if theNext:
                next = WizardPage.GetNext(next)
            else:
                next = WizardPage.GetPrev(next)

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
    #events.send(events.Command('freeze'))

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
        'hexen-dk': 'HEXEN.WAD',
        'hacx': 'HACX.WAD',
        'chex': 'CHEX.WAD'
    }
    
    events.mute()
    
    # Make the Defaults profile active.
    pr.setActive(pr.getDefaults())
    
    wiz = WizardDialog(language.translate('setup-wizard'),
                       paths.findBitmap('wizard'))

    # Language selection page.
    langPage = WizardPage(wiz, 'wizard-language')
    area = langPage.getArea()
    area.createText('wizard-language-explanation', maxLineLength=65).resizeToBestSize()
    sar, languageCheckBox = area.createSetting(st.getSetting('language'))
    languageCheckBox.getFromProfile(pr.getActive())

    # Game selection.
    gamePage = ProfileWizardPage(wiz, 'wizard-games', None)
    gamePage.follows(langPage)
    area = gamePage.getArea()
    area.createText('wizard-select-games', maxLineLength=65).resizeToBestSize()
    area.setBorderDirs(ui.BORDER_NOT_BOTTOM)
    games = area.createList('', style=sb.widget.list.List.STYLE_CHECKBOX)
    area.setBorderDirs(ui.BORDER_NOT_TOP)
    games.setMinSize(50, 180)
    gamePage.setGameList(games)

    def allGames():
        # Check the entire list.
        for item in games.getItems():
            games.checkItem(item, True)

    def clearGames():
        # Uncheck the entire list.
        for item in games.getItems():
            games.checkItem(item, False)

    
    controls = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=2)
    controls.setWeight(0)
    button = controls.createButton('wizard-games-all', wb.Button.STYLE_MINI)
    button.addReaction(allGames)
    button.resizeToBestSize()
    button = controls.createButton('wizard-games-clear', wb.Button.STYLE_MINI)
    button.addReaction(clearGames)
    button.resizeToBestSize()

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

            area.createText('wizard-locate-iwad', maxLineLength=65).resizeToBestSize()

            # The suggestion.
            if suggested.has_key(p.getId()):
                sugArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=2)
                sugArea.setExpanding(False)
                sugArea.setWeight(1)
                sugArea.createText('wizard-suggested-iwad', ':', align=wt.Text.RIGHT)
                sugArea.setWeight(2)
                sug = sugArea.createText('')
                sug.setText(suggested[p.getId()])

            sar, page.iwadText = area.createSetting(st.getSetting('iwad'))

            if p.getId() == 'hexen-dk':
                area.setBorder(12, ui.BORDER_ALL)
                area.createLine()
                area.setBorder(6, ui.BORDER_ALL)
                area.createText('deathkings-selection-title').setHeadingStyle()
                
                # Death Kings is an extension to Hexen.  It uses the
                # same Hexen IWAD, but also an addon IWAD.
                area.createText('wizard-locate-iwad-deathkings', maxLineLength=65).resizeToBestSize()

                entry = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=4)
                entry.setExpanding(False)
                entry.setWeight(1)
                entry.setBorderDirs(ui.BORDER_NOT_LEFT)
                deathKingsWad = entry.createTextField('')
                entry.setWeight(0)
                entry.setBorderDirs(ui.BORDER_TOP_BOTTOM)
                browseButton = entry.createButton('browse-button', wb.Button.STYLE_MINI)

                def browseDeathKings():
                    # Open a file browser.
                    selection = sb.util.dialog.chooseFile('deathkings-selection-title',
                                                          '', True,
                                                          [('file-type-wad', 'wad')])
                
                    if len(selection) > 0:
                        deathKingsWad.setText(selection)

                browseButton.addReaction(browseDeathKings)

	# Addon paths.
	pathsPage = ProfileWizardPage(wiz, 'wizard-addon-paths', games)
	pathsPage.follows(previousPage)
	area = pathsPage.getArea()
	area.createText('wizard-addon-paths-explanation',
                    maxLineLength=65).resizeToBestSize()

    area.setBorderDirs(ui.BORDER_NOT_BOTTOM)
    pathList = area.createList('addon-paths-list')
    pathList.setMinSize(100, 120)

    # Insert the current custom paths into the list.
    for p in paths.getAddonPaths():
        pathList.addItem(p)

    def addAddonPath():
        selection = sb.util.dialog.chooseFolder('addon-paths-add-prompt', '')
        if selection:
            pathList.addItem(selection)
            pathList.selectItem(selection)

    def removeAddonPath():
        selection = pathList.getSelectedItem()
        if selection:
            pathList.removeItem(selection)

    area.setWeight(0)
    area.setBorderDirs(ui.BORDER_NOT_TOP)
    commands = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=2)
    commands.setWeight(0)

    # Button for adding new paths.
    button = commands.createButton('new-addon-path', wb.Button.STYLE_MINI)
    button.addReaction(addAddonPath)

    # Button for removing a path.
    button = commands.createButton('delete-addon-path', wb.Button.STYLE_MINI)
    button.addReaction(removeAddonPath)

    # Launch options.
    quitPage = WizardPage(wiz, 'wizard-launching') 
    quitPage.follows(pathsPage)
    area = quitPage.getArea()
    area.createText('wizard-launching-explanation').resizeToBestSize()
    sar, quitCheckBox = area.createSetting(st.getSetting('quit-on-launch'))
    quitCheckBox.getFromProfile(pr.getActive())

    # List of unusable profiles, due to a missing IWAD.
    unusableProfiles = []
    
    # When the page changes in the wizard, change the active profile.
    def changeActiveProfile(page):
        prof = pr.get(page.getId())
        if prof:
            pr.setActive(prof)
        else:
            pr.setActive(pr.getDefaults())
        # Update page with values from the current profile.
        page.update()
                       
    wiz.setPageReaction(changeActiveProfile)
    
    if wiz.run(langPage) == 'ok':
        events.unmute()
        
        # Show the profiles that now have an IWAD.
        for prof in profiles:
            if prof.getValue('iwad', False) != None:
                pr.show(prof.getId())
                if prof.getId() in unusableProfiles:
                    unusableProfiles.remove(prof.getId())
            else:
                pr.hide(prof.getId())
                if prof.getId() not in unusableProfiles and \
                   prof.getId() in games.getSelectedItems():
                    unusableProfiles.append(prof.getId())

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

        # Update custom addon paths.
        currentAddonPaths = paths.getAddonPaths()
        wasModified = False
        for path in pathList.getItems():
            if path not in currentAddonPaths:
                paths.addAddonPath(path)
                wasModified = True
        for path in currentAddonPaths:
            if path not in pathList.getItems():
                paths.removeAddonPath(path)
                wasModified = True

        if wasModified:
            # Load addons from new paths.
            ao.refresh()

        events.send(events.Notify('addon-paths-changed'))

        # The wizard will only be shown once automatically.
        pr.getDefaults().setValue(HAS_BEEN_RUN, 'yes', False)

    else:
        # Wizard was canceled.
        events.unmute()

    pr.refresh()

    # This'll destroy all the pages of the wizard as well.
    wiz.destroy()

    # Enable help panel updates again.
    #events.send(events.Command('unfreeze'))
    
    # Tell the user about unusable profiles.
    if len(unusableProfiles) > 0:
        dialog, area = sb.util.dialog.createButtonDialog(
            'wizard-unlaunchable-profiles',
            ['ok'], 'ok', resizable=False)
        # Compose a list of the unlaunchable profiles.
        profList = ''
        unusableProfiles.sort(lambda a, b: cmp(language.translate(a),
                                               language.translate(b)))
        for p in unusableProfiles:
            profList += "\n" + language.translate(p)
        msg = area.createText()
        msg.setText(language.translate('wizard-unlaunchable-profiles-listed') + "\n" + 
                    profList)
        msg.resizeToBestSize()
        dialog.run()
        
