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
import paths, events, ui, language, widgets
import sb.util.dialog
import sb.widget.button as wg
import sb.widget.list as wl
import sb.widget.text as wt
import sb.profdb as pr
import sb.confdb as st
import sb.aodb as ao
import sb.addon
import tab30.installer
import tab30.loadorder
import tab30.inspector
import wx

ADDONS = 'tab-addons'

# True, if the Addons tab is currently visible.
tabVisible = False

# By default, sort by name.
listSortMode = 'name'

# True, if the addons list should be refreshed.
mustRefreshList = True


def init():
    # Manager for the addon list icons.
    global iconManager, addonIcons
    iconManager = widgets.IconManager(16, 16)
    addonIcons = [iconManager.get('unchecked'),
                  iconManager.get('checked'),
                  iconManager.get('defcheck')]

    # Create the Addons page.
    area = ui.createTab(ADDONS)

    WEIGHTS = [2, 3]
    
    # Top area for the filter controls.
    area.setWeight(0)
    topArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=0)
    topArea.setExpanding(False)

    # Counter text.
    global countText
    topArea.setWeight(WEIGHTS[0])
    countText = topArea.createText('', align=wt.Text.LEFT)   
    
    # Filter selection.
    topArea.setWeight(WEIGHTS[1])
    filterArea = topArea.createArea(alignment=ui.ALIGN_HORIZONTAL, border=0)
    filterArea.setWeight(0)
    filterArea.setExpanding(False)
    global listFilter
    filterArea.createText('addon-list-filter').resizeToBestSize()
    filterArea.setWeight(1)
    filterArea.setBorder(4, ui.BORDER_LEFT)
    listFilter = filterArea.createDropList('addon-list-filter-mode')
    listFilter.addItem('addon-list-filter-mode-compatible')
    listFilter.addItem('addon-list-filter-mode-compatible-pwad')
    listFilter.addItem('addon-list-filter-mode-pwad')
    listFilter.addItem('addon-list-filter-mode-all')
    listFilter.addItem('addon-list-filter-mode-all-with-boxes')
    listFilter.selectItem('addon-list-filter-mode-compatible')

    # Middle area for the category tree and the addon list.
    area.setWeight(1)
    area.setBorderDirs(ui.BORDER_LEFT_RIGHT)
    middleArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=0)
    area.setBorderDirs(ui.BORDER_NOT_TOP)

    # On the left, there is the main addon database controls.
    middleArea.setWeight(WEIGHTS[0])
    # Category tree.
    global tree
    middleArea.setBorder(8, ui.BORDER_RIGHT)
    tree = middleArea.createTree('category-tree')
    middleArea.setBorder(0)
    # Add all the categories into the tree.
    refreshCategories()
    
    # On the right, there is the filtered addon listing.
    middleArea.setWeight(WEIGHTS[1])
    global addonList 
    addonList = middleArea.createList('addon-list', style=wl.List.STYLE_COLUMNS)
    addonList.setImageList(iconManager.getImageList())
    addonList.setPopupMenuId('addon-list-popup')
    
    # Setup the columns.
    addonList.addColumn('addon-list-check', 20)
    addonList.addColumn('addon-list-name')
    addonList.addColumn('addon-list-version', 50)
    
    # Button arae in the bottom.
    area.setWeight(0)
    buttonArea = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=4)

    # Install (+) button for installing new addons.
    buttonArea.setWeight(0)
    buttonArea.setBorderDirs(ui.BORDER_TOP | ui.BORDER_RIGHT)
    buttonArea.createButton('install-addon', wg.Button.STYLE_MINI)

    global uninstallButton
    uninstallButton = buttonArea.createButton('uninstall-addon',
                                              wg.Button.STYLE_MINI)
    uninstallButton.disable()

    # The Refresh button reloads all addons.
    button = buttonArea.createButton('refresh-addon-database', wg.Button.STYLE_MINI)
    button.resizeToBestSize()   
    
    # Create addon listing controls.
    buttonArea.setWeight(1)
    buttonArea.addSpacer()
    buttonArea.setWeight(0)

    global infoButton
    infoButton = buttonArea.createButton('addon-info')
    infoButton.disable()

    global settingsButton
    settingsButton = buttonArea.createButton('addon-settings')
    settingsButton.disable()
    
    buttonArea.setBorderDirs(ui.BORDER_TOP)
    buttonArea.createButton('show-addon-paths')    
   
    ui.addMenuCommand(ui.MENU_TOOLS, 'load-order', group=ui.MENU_GROUP_AODB)

    # Registering a notification listener.
    events.addNotifyListener(handleNotification, ['tab-selected', 
                                                  'addon-list-icon-click',
                                                  'active-profile-changed',
                                                  'addon-list-popup-update-request',
                                                  'addon-attached',
                                                  'addon-detached',
                                                  'category-tree-selected',
                                                  'addon-installed',
                                                  'addon-database-reloaded',
                                                  'addon-list-selected',
                                                  'addon-list-deselected',
                                                  'addon-list-filter-mode-selected',
                                                  'addon-list-check-column-click',
                                                  'addon-list-name-column-click',
                                                  'addon-list-version-column-click'])

    # Registering a command listener to handle the Command events sent
    # by the buttons created above.
    events.addCommandListener(handleCommand, ['install-addon',
                                              'uninstall-addon',
                                              'addon-info',
                                              'addon-settings',
                                              'load-order',
                                              'addon-list-check-selected',
                                              'addon-list-uncheck-selected',
                                              'addon-list-check-all',
                                              'addon-list-uncheck-all',
                                              'addon-show-parent-box',
                                              'addon-show-box-category',
                                              'refresh-addon-database'])
                                              
    # Changing the current addon in the tree is done using commands with
    # the name of the addon as an identifier.
    events.addCommandListener(handleAnyCommand)
    
    # Menu commands.
    ui.addMenuCommand(ui.MENU_TOOLS, 'install-addon', pos=0, group=ui.MENU_GROUP_AODB)
    
         
def refreshCategories():
    """Recreates the categories of the category tree."""
    tree.freeze()
    oldSel = tree.getSelectedItem()
    expanded = tree.getExpandedItems()
    tree.clear()
    # Start populating from the root category.
    buildCategories(tree, 'category-tree-root', ao.getRootCategory())
    tree.expandItem('category-tree-root')
    map(tree.expandItem, expanded)        
    tree.selectItem(oldSel)
    tree.unfreeze()


def buildCategories(tree, parentId, parentCategory):
    """Builds categories recursively."""

    for cat in ao.getSubCategories(parentCategory):
        id = cat.getLongId()
        tree.addItem(id, parentId)
        buildCategories(tree, id, cat)    
        
    tree.sortItemChildren(parentId)
        

def refreshListIfVisible():
    global mustRefreshList
    mustRefreshList = True
    if tabVisible:
        refreshList()
        
        
def determineIcon(addon, profile, defaultAddons, profileAddons):
    """Determines which icon to use for an addon.
    
    @param addon  addon.Addon.
    @param profile  profile.Profile.
    @param defaultAddons  List of addon identifiers from Defaults.
    @param profileAddons  List of addon identifiers from the profile.
    
    @return Icon index.
    """
    ident = addon.getId()
    icon = addonIcons[0]
    if profile is pr.getDefaults():
        # In the defaults profile, addons are either checked or unchecked.
        checked = ident in defaultAddons

        # Invert the check mark on opt-out addons.
        if addon.isInversed():
            checked = not checked

        if checked:
            icon = addonIcons[1]
        else:
            icon = addonIcons[0]
    else:
        # In other profiles, addons may have the defcheck as well.
        checked = ident in profileAddons
        
        # Being in the Defaults inverts the selection.
        if ident in defaultAddons:
            checked = not checked
            inDefaults = True
        else:
            inDefaults = False
            
        if addon.isInversed():
            checked = not checked
            
        if checked:
            if inDefaults:
                icon = addonIcons[2]
            else:
                icon = addonIcons[1]
        else:
            icon = addonIcons[0]
    return icon
        

def refreshList():
    """Refreshes the list of addons."""
    addonList.freeze()

    # Try to keep the old selection, if there is one.
    oldSelections = addonList.getSelectedItems()

    addonList.clear()
    
    # Filtering parameters.
    profile = pr.getActive()
    isDefaults = profile is pr.getDefaults()
    if tree.getSelectedItem():
        filterCategory = ao.getCategory(tree.getSelectedItem())
    else:
        filterCategory = None
        
    # Which addons are currently attached?
    defaultAddons = pr.getDefaults().getAddons()
    if not isDefaults:
        profileAddons = pr.getActive().getAddons()
    else:
        profileAddons = None
    
    # Categories that will be in bold font in category-tree.
    boldCategories = []
    
    # Determine filtering options.
    mode = listFilter.getSelectedItem()
    onlyCompatible = False
    onlyPWAD = False
    alsoBoxes = False
    if 'compatible' in mode:
        onlyCompatible = True
    if 'pwad' in mode:
        onlyPWAD = True
    if 'with-boxes' in mode:
        alsoBoxes = True
        
    for addon in ao.getAddons():
        # Does this addon pass the filter?
        if not isDefaults and onlyCompatible:
            if not addon.isCompatibleWith(pr.getActive()):
                # Cannot be listed.
                continue
        if onlyPWAD and (addon.getType() != 'addon-type-wad' or not addon.isPWAD()):
            continue
        # Has a category been selected?
        if filterCategory and not filterCategory.isAncestorOf(addon.getCategory()):
            # Not in the category.
            continue
        # Addons in boxes are not listed unless the correct category is selected.
        if not alsoBoxes and addon.getBox():
            # Addons in boxes are only visible if the selected category is a box category.
            boxCateg = addon.getBox().getContentCategoryLongId()
            if not filterCategory: continue
            if filterCategory.getLongId().find(boxCateg) != 0:
                # Not the right category.
                continue
        # Passed the filter!
        id = addon.getId()
        versionStr = ''
        icon = determineIcon(addon, profile, defaultAddons, profileAddons)
            
        if language.isDefined(id + '-version'):
            versionStr = language.translate(id + '-version')
            
        name = language.translate(id)
        if addon.getBox() and alsoBoxes:
            name += ' ' + language.translate('addon-list-in-box')
            
        addonList.addItemWithColumns(id,  icon, name, versionStr)
      
    # Reselect old items.
    oldSelections.reverse()
    for sel in oldSelections:
        addonList.selectItem(sel)
        addonList.ensureVisible(sel)

    sortList()
    updateButtons()

    addonList.unfreeze()
    
    # Update the counter text.
    countText.setText(language.expand(language.translate('addon-counter'),
                                      str(len(addonList.getItems())), 
                                      str(ao.getAddonCount())))
    
    # Update boldings in category-tree.
    # --TODO!--   
    
    global mustRefreshList
    mustRefreshList = False
    
    
def refreshItemInList(item):
    addon = ao.get(item)
    defaultAddons = pr.getDefaults().getAddons()
    if pr.getActive() is not pr.getDefaults():
        profileAddons = pr.getActive().getAddons()
    else:
        profileAddons = None
    
    # Update the image of the item.
    addonList.setItemImage(item, determineIcon(addon, pr.getActive(),
                                               defaultAddons, profileAddons))
                                               
                                               
def sortList():
    """Sort the current addon list according to the sort mode."""
    
    # Unsorted items of the list. Sorting will be done based on this information.
    if listSortMode == 'check':
        icons = addonList.getItemImages()
    items = addonList.getItems()
    keys = map(lambda i: (language.translate(i).lower(), 
                          language.translate(i + '-version', '0.0')), items)
    
    def checkSortCmp(a, b):
        # First compare check icon (descending).
        result = cmp(icons[b], icons[a])
        if result != 0:
            return result
        # Then compare as in by name.
        return nameSortCmp(a, b)

    def nameSortCmp(a, b):
        # First by name.
        result = cmp(keys[a][0], keys[b][0])
        if result != 0:
            return result
        # Then by version (descending).
        return cmp(keys[b][1], keys[a][1])

    def versionSortCmp(a, b):
        # First by version (descending).
        result = cmp(keys[b][1], keys[a][1])
        if result != 0:
            return result
        # Then by name.
        return cmp(keys[a][0], keys[b][0])
        
    funcs = {'version': versionSortCmp, 'check': checkSortCmp, 'name': nameSortCmp}
    addonList.sortItems(funcs[listSortMode])
                              

def handleNotification(event):
    """This is called when someone sends a notification event.

    @param event An events.Notify object.
    """
    global tabVisible
    global mustRefreshList

    if event.hasId('addon-list-icon-click'):
        # Clicking on the list icon will attach/detach the addon.
        profile = pr.getActive()
        if event.item in profile.getUsedAddons():
            profile.dontUseAddon(event.item)
        else:
            profile.useAddon(event.item)
            
    elif event.hasId('addon-attached') or event.hasId('addon-detached'):
        refreshItemInList(event.getAddon())

    elif event.hasId('addon-list-filter-mode-selected'):
        refreshListIfVisible()

    elif event.hasId('addon-list-check-column-click') or \
         event.hasId('addon-list-name-column-click') or \
         event.hasId('addon-list-version-column-click'):
        global listSortMode
        if 'check' in event.getId():
            listSortMode = 'check'
        elif 'version' in event.getId():
            listSortMode = 'version'
        else:
            listSortMode = 'name'
        sortList()

    elif event.hasId('tab-selected'):
        # If there is a refresh pending, do it now that the tab has 
        # been selected.
        if event.getSelection() == ADDONS:
            tabVisible = True
            if mustRefreshList:
                refreshList()
        else:
            tabVisible = False
    
    elif event.hasId('active-profile-changed'):
        refreshListIfVisible()
            
        # Fill the tree with an updated listing of addons.
        #tree.populateWithAddons(pr.getActive())
        #settingsButton.disable()

    elif event.hasId('addon-list-popup-update-request'):
        # The addon list is requesting an updated popup menu.
        # Menu items depend on which items are selected.
        items = addonList.getSelectedItems()
        menu = []
        
        if len(items) == 1:
            # One addon is selected.
            if ao.get(items[0]).getBox():
                menu += ['addon-show-parent-box']
            if ao.get(items[0]).getType() == 'addon-type-box':
                menu += ['addon-show-box-category']
            # Info is always available for all addons.
            menu += ['addon-info'] 
            if st.haveSettingsForAddon(items[0]):
                menu += ['addon-settings']
        elif len(items) > 1:
            # Multiple addons selected.
            menu += ['addon-list-check-selected',
                     'addon-list-uncheck-selected']

        if len(items) > 0: menu += ['uninstall-addon']

        # Append the common items.
        if len(menu) > 0: menu.append('-')
        menu += ['install-addon'] 
        menu += ['load-order']

        menu += ['-', 'addon-list-check-all', 'addon-list-uncheck-all']
        
        addonList.setPopupMenu(menu)

    elif event.hasId('category-tree-selected'):
        refreshListIfVisible()
        # Enable the settings button if there are settings for this
        # addon.
        #if st.haveSettingsForAddon(event.getSelection()):
        #    settingsButton.enable()
        #else:
        #    settingsButton.disable()

        # Can this be uninstalled?
        #uninstallButton.enable(ao.exists(event.getSelection()) and
        #                       ao.get(event.getSelection()).getBox() == None)

    elif event.hasId('addon-list-selected') or event.hasId('addon-list-deselected'):
        updateButtons()

    elif event.hasId('addon-installed'):
        refreshCategories()
        refreshListIfVisible()
        #tree.selectAddon(event.addon)

    elif event.hasId('addon-database-reloaded'):
        refreshCategories()
        refreshListIfVisible()
        
        
def updateButtons():
    items = addonList.getSelectedItems()
    if len(items) == 0:
        settingsButton.disable()
        uninstallButton.disable()
        infoButton.disable()
        return
        
    if len(items) > 1:
        # More than one.
        settingsButton.disable() 

        canUninstall = False
        # At least one uninstallable?
        for addon in items:
            if ao.isUninstallable(addon):
                uninstallButton.enable()
                canUninstall = True
                break
        if not canUninstall:
            uninstallButton.disable()

        infoButton.disable()
        return

    sel = items[0]
    settingsButton.enable(st.haveSettingsForAddon(sel))
    infoButton.enable()
        
    # Can this be uninstalled?
    uninstallButton.enable(ao.isUninstallable(sel))


def showInAddonList(addon):
    if ao.exists(addon):
        if addon in addonList.getItems():
            # Select the addon in the tree.
            addonList.deselectAllItems()
            addonList.selectItem(addon)
            addonList.ensureVisible(addon)
        else:
            # The addon was not currently visible, so make sure it is.
            tree.selectItem('category-tree-root')
            addonList.deselectAllItems()
            refreshListIfVisible()
            addonList.selectItem(addon)
            addonList.ensureVisible(addon)


def handleAnyCommand(event):
    """Handle any command. This is called whenever a command is broadcasted.
    Must not do anything slow."""
    # Is the command an addon identifier?
    showInAddonList(event.getId())
        

def handleCommand(event):
    """This is called when someone sends a command event.

    @param event An events.Command object."""

    # Figure out which addon has been currently selected in the addons
    # tree.  The action will target this addon.
    selected = ''

    if event.hasId('refresh-addon-database'):
        ao.refresh()

    elif event.hasId('install-addon'):
        tab30.installer.run()

    elif event.hasId('uninstall-addon'):
        selectedItems = addonList.getSelectedItems()
                
        # Only take the uninstallable addons.
        addons = filter(lambda i: ao.isUninstallable(i), selectedItems)
        if len(addons) == 0:
            return
            
        # Make sure the user wants to uninstall.
        dialog, area = sb.util.dialog.createButtonDialog(
            'uninstall-addon-dialog',
            ['no', 'yes'], 'no', resizable=False)

        if len(addons) == 1:
            text = language.translate('uninstall-addon-query')
            area.setExpanding(True)
            area.createRichText(language.expand(text, language.translate(addons[0])))
        else:
            area.setWeight(0)
            area.createText('uninstall-addon-query-several')
            msg = '<html><ul>'
            for addon in addons:
                msg += '<li>' + language.translate(addon) + '</li>'
            msg += '</ul></html>'
            ft = area.createFormattedText()
            ft.setText(msg)
            ft.setMinSize(400, 150)
    
        if dialog.run() == 'yes':
            for addon in addons:
                ao.uninstall(addon)
            ao.refresh()

    elif event.hasId('addon-info'):
        sel = addonList.getSelectedItems()
        if len(sel) > 0:
            # Show the Addon Inspector.
            tab30.inspector.run(ao.get(sel[0]))

    elif event.hasId('addon-settings'):
        sel = addonList.getSelectedItems()
        if len(sel) > 0:
            command = events.Command('show-addon-settings')
            command.addon = sel[0]
            events.send(command)        

    elif event.hasId('load-order'):
        # Open the Load Order dialog.
        tab30.loadorder.run(pr.getActive())

    elif event.hasId('addon-list-check-selected'):
        for addon in addonList.getSelectedItems():
            pr.getActive().useAddon(addon)

    elif event.hasId('addon-list-uncheck-selected'):
        for addon in addonList.getSelectedItems():
            pr.getActive().dontUseAddon(addon)

    elif event.hasId('addon-list-check-all'):
        for addon in addonList.getItems():
            pr.getActive().useAddon(addon)

    elif event.hasId('addon-list-uncheck-all'):
        for addon in addonList.getItems():
            pr.getActive().dontUseAddon(addon)
            
    elif event.hasId('addon-show-parent-box'):
        for addon in addonList.getSelectedItems():
            a = ao.get(addon)
            if a.getBox():
                showInAddonList(a.getBox().getId())
            break
            
    elif event.hasId('addon-show-box-category'):
        for addon in addonList.getSelectedItems():
            a = ao.get(addon)
            if a.getType() == 'addon-type-box':
                addonList.deselectAllItems()
                tree.selectItem(a.getContentCategoryLongId())
                refreshListIfVisible()
            break

    #elif event.hasId('expand-all-categories'):
    #    tree.expandAllCategories(True)

    #elif event.hasId('collapse-all-categories'):
    #    tree.expandAllCategories(False)

    #elif event.hasId('check-category'):
    #    tree.checkSelectedCategory(True)

    #elif event.hasId('uncheck-category'):
    #    tree.checkSelectedCategory(False)



def oldInit():
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
                                                  'addon-popup-request',
                                                  'addon-installed',
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
