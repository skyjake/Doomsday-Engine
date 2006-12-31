# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2006
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

## @file tree.py Tree Widget

import wx
import host, events, language, paths
import sb.aodb as ao
import sb.profdb as pr
import base
from widgets import uniConv


class MyTreeCtrl (wx.TreeCtrl):
    """A wxTreeCtrl that sorts items alphabetically, but places all
    branches after the leaves.  That is, all items with children are
    placed below the items that don't have children."""

    def __init__(self, parent, wxId):
        wx.TreeCtrl.__init__(self, parent, wxId,
                             style=(wx.TR_NO_LINES |
                                    wx.TR_HAS_BUTTONS |
                                    wx.TR_TWIST_BUTTONS |
                                    wx.TR_HIDE_ROOT))

    def OnCompareItems(self, item1, item2):
        """Override this function in the derived class to change the
        sort order of the items in the tree control.  The function
        should return a negative, zero or positive value if the first
        item is less than, equal to or greater than the second one.
        """
        # Count the number of children (non-recursively).
        n1 = self.GetChildrenCount(item1, False)
        n2 = self.GetChildrenCount(item2, False)

        if (n1 == 0 and n2 == 0) or (n1 > 0 and n2 > 0):
            # Normal comparison based on case-insensitive alphabet.
            return cmp(self.GetItemText(item1).lower(),
                       self.GetItemText(item2).lower())

        # The one with no children must come first.
        if n1 == 0 and n2 > 0:
            return -1
        else:
            return 1


class Tree (base.Widget):
    """The addon browser tree widget."""

    class Hierarchy:
        """A subclass for storing information about a category
        hierarchy."""
        
        def __init__(self):
            self.root = None
            self.rootId = ''
            self.categories = {}
            self.items = {}

        def hasAddon(self, addon):
            """Checks if the addon is represented in the hierarchy.

            @param addon The addon identifier.
            """
            return self.items.has_key(addon)

        def getCategory(self, item):
            """Finds the category that matches the item.

            @param item A Tree item object.

            @return Category object.
            """
            for category in self.categories.keys():
                if self.categories[category] == item:
                    return category
            return None

    def __init__(self, parent, wxId):
        base.Widget.__init__(self, MyTreeCtrl(parent, wxId))

        # The addon tree uses a fixed identifier.
        self.widgetId = 'addon-tree'

        # Load the tree icons.
        images = wx.ImageList(16, 16)
        images.Add(wx.Bitmap(paths.findBitmap('unchecked')))
        images.Add(wx.Bitmap(paths.findBitmap('checked')))
        images.Add(wx.Bitmap(paths.findBitmap('defcheck')))
        if host.isWindows():
            images.Add(wx.Bitmap(paths.findBitmap('closedfolder')))
            images.Add(wx.Bitmap(paths.findBitmap('openfolder')))
        self.getWxWidget().AssignImageList(images)

        ## @todo Should this be LEFT_UP?
        wx.EVT_LEFT_DOWN(self.getWxWidget(), self.onItemClick)

        # Right-clicking in the tree must change the selection in
        # addition to displaying the context menu.
        wx.EVT_RIGHT_DOWN(self.getWxWidget(), self.onRightClick)

        # We will not allow expanding totally empty categories.
        wx.EVT_TREE_ITEM_EXPANDING(parent, wxId, self.onItemExpanding)

        self.grayColour = wx.Colour(192, 192, 192)

        # The root items.
        self.removeAll()

        # Clear the list of item ancestors (addon name -> [parent items]).
        self.itemAncestors = {}
        
        self.listenToNotifications = True
        
        # In addition to the normal notifications, listen to addon attachment
        # and detachment notifications.
        events.addNotifyListener(self.onNotify, ['addon-attached', 
                                                 'addon-detached'])

    def createCategories(self):
        """Create menu items for all the categories.  The appearance
        of the category title reflects its contents.  If a category
        contains no addons, it is displayed using a gray colour.
        Categories with only unchecked addons are displayed in bold
        black.  Categories with checked addons are also bold black,
        but they have a number after the title displaying the number
        of checked addons in the category.  The numbers are inherited
        by the parent categories of a category with checked addons.
        """
        tree = self.getWxWidget()
        tree.Freeze()
      
        # Create the root items.
        self.available.rootId = 'available-addons-root'
        self.available.root = tree.AppendItem(self.root,
                                              language.translate\
                                              (self.available.rootId))
        self.defaults.rootId = 'default-addons-root'
        self.defaults.root = tree.AppendItem(self.root,
                                             language.translate\
                                             (self.defaults.rootId))

        self.createCategoryTree(self.available)
        self.createCategoryTree(self.defaults)

        tree.Thaw()

    def createCategoryTree(self, hierarchy):
        """Create a category tree under the specified root item.

        @param rootItem The tree item under which the categories are
        to be created.

        @param categoryMap A dictionary that will hold the mapping
        between Category objects and tree items.
        """
        tree = self.getWxWidget()
        
        # Start with no categories.
        hierarchy.categories = { ao.getRootCategory(): hierarchy.root }

        # Start from the root level.
        levelCategories = ao.getSubCategories(ao.getRootCategory())

        while len(levelCategories) > 0:
            # Sort the categories by name.
            levelCategories.sort(lambda a, b: cmp(a.getName(), b.getName()))

            # The categories of the next level.
            nextLevel = []

            for category in levelCategories:
                # Create an item for this category.
                parentItem = hierarchy.categories[category.getParent()]

                # The labels will be updated separately.
                item = tree.AppendItem(parentItem, '')

                if host.isWindows():
                    # Windows requires that all items have an image.
                    tree.SetItemImage(item, 3)
                                
                hierarchy.categories[category] = item

                # Append this category's children for the next level
                # of processing.
                nextLevel += ao.getSubCategories(category)

            levelCategories = nextLevel

    def removeAll(self):
        """Remove all the items in the tree, including categories and
        addons.
        """
        # Delete all children of the hidden root.
        tree = self.getWxWidget()

        tree.Freeze()
        tree.DeleteAllItems()
        tree.Thaw()

        self.defaults = Tree.Hierarchy()
        self.available = Tree.Hierarchy()
      
        # Create the hidden root item of the tree.
        # The real root item won't be visible.
        self.root = tree.AddRoot('Addons')

    def removeAddons(self):
        """Remove all the addons from the tree.  Does not affect the
        categories.
        """
        pass

    def isAddonUnder(self, addon, parentItem):
        """Returns True if the addon is a descendent of the parentItem in the
        Tree.
        
        @param addon  Addon identifier.
        @param parentItem  wxTreeItemId, under which the addon is to be under.
        
        @return True, if the addon resides under the parentItem.
        """
        try:
            for ancestor in self.itemAncestors[addon]:
                if ancestor == parentItem:
                    return True
        except:
            # Not defined.
            pass
        return False        

    def insertSorted(self, parentItem, label):
        """Insert a tree item under the parent item, so that it's in
        the proper alphabetical order.
        """
        tree = self.getWxWidget()
        index = 0

        # Find the item before which the new item will be inserted.
        child, cookie = tree.GetFirstChild(parentItem)
        while child:
            # If we hit a Category item, we must stop.
            if self.getItemData(child) == None:
                break
            if cmp(label.lower(), tree.GetItemText(child).lower()) < 0:
                # Insert it before this one.
                break
            child = tree.GetNextSibling(child)
            index += 1
            
        item = tree.InsertItemBefore(parentItem, index, label)
        return item

    def setItemData(self, item, data):
        self.getWxWidget().SetPyData(item, data)

    def getItemData(self, item):
        try:
            data = self.getWxWidget().GetPyData(item)
            return data
        except:
            return None

    def getItemAncestors(self, item):
        """Builds an array of the ancestor items of the item.
        
        @return Array of wxTreeItemIds.
        """
        tree = self.getWxWidget()
        ancestors = []
        parent = tree.GetItemParent(item)
        while parent:
            ancestors.append(parent)
            parent = tree.GetItemParent(parent)
        return ancestors

    def showAddons(self, addons, hierarchy):
        """Make sure that only the specified addons are visible in the
        tree.  All other items are removed from the tree.

        @param addon An array of addon identifiers.

        @param categoryMap Dictionary of Categories to tree items.

        @param itemMap Dictionary of addon identifiers to tree items.
        This will be updated as the method progresses.
        """
        tree = self.getWxWidget()
        tree.Freeze()
        
        # Insert all addons that are listed in addons but are not in
        # the item map.
        for addon in addons:

            # Hidden addons are not listed.
            if not ao.get(addon).isVisible():
                continue
            
            if not hierarchy.items.has_key(addon):
                # Create this item.
                category = ao.get(addon).getCategory()
                parentItem = hierarchy.categories[category]
                item = self.insertSorted(parentItem,
                                         language.translate(addon))
                hierarchy.items[addon] = item
                
                # Determine the ancestors of the item.
                self.itemAncestors[addon] = self.getItemAncestors(item)

                # The extra data contains information about the addon.
                self.setItemData(item, addon)
        
        # Remove all addons that are in the map but are not in the
        # addons list.
        for addon in hierarchy.items.keys():
            if addon not in addons:
                # Remove the item from the tree.
                tree.Delete(hierarchy.items[addon])
                # Remove the entry from the dictionaries.
                del hierarchy.items[addon]
                try:
                    del self.itemAncestors[addon]
                except:
                    pass

        tree.Thaw()

    def populateWithAddons(self, forProfile):
        """Calling this populates the tree with addons.  The addon
        categories must already be created by calling
        createCategories.

        @param forProfile If this is not None, only the addons
        compatible with this profile are added to the tree.
        """
        tree = self.getWxWidget()

        # The visible root items.
        defaultAddons = pr.getDefaults().getAddons()

        # All available addons for the profile.
        idents = map(lambda a: a.getId(),
                     ao.getAvailableAddons(forProfile))

        if forProfile is pr.getDefaults():
            self.showAddons(idents, self.defaults)
            self.showAddons([], self.available)

        else:
            defaultAddons = [a for a in defaultAddons
                              if ao.get(a).isCompatibleWith(forProfile)]

            self.showAddons(defaultAddons, self.defaults)
            self.showAddons(idents, self.available)

        self.refreshCategoryLabels(forProfile)
        self.refreshItems(forProfile)

    def refreshCategoryLabels(self, profile):
        """Set the text and the font used in all the category labels.
        The labels reflect the contents of the category.
        """
        tree = self.getWxWidget()

        # The addons that will actually be used when launching.
        usedAddons = profile.getUsedAddons()

        for hier in [self.defaults, self.available]:
            rootLabel = language.translate(hier.rootId)
            totalCount = len(hier.items)

            if host.isWindows():
                # The closed folder icon.
                tree.SetItemImage(hier.root, 3)

            if totalCount == 0:
                tree.SetItemBold(hier.root, False)
                tree.SetItemTextColour(hier.root, self.grayColour)
                tree.SetItemText(hier.root, uniConv(rootLabel))
                tree.Collapse(hier.root)
                # Expansion of an empty tree should be vetoed.
            else:
                # Expand all categories that have checked items in
                # them.  Collapse the others.
                for category in hier.categories.keys():
                    # Count the number of addons that will be used.
                    checkCount = 0
                    count = 0

                    # Are all the items in the category box parts?
                    justBoxParts = True
                    
                    for addon in hier.items.keys():
                        if self.isAddonUnder(addon, hier.categories[category]):
                            count += 1
                            if addon in usedAddons:
                                checkCount += 1

                            # Box parts?
                            if not ao.get(addon).getBox():
                                justBoxParts = False

                    item = hier.categories[category]
                    tree.SetItemBold(item, checkCount > 0)
                    
                    if category is ao.getRootCategory():
                        label = rootLabel
                    else:
                        label = category.getName()

                    # The appearance of the label depends on the
                    # number of addons in the children.
                    if count > 0:
                        if checkCount > 0:
                            label += ' (' + str(checkCount) + ')'
                            # Don't force-open the Defaults root.
                            #if item is not hier.root and not justBoxParts:
                            #    tree.Expand(item)
                        tree.SetItemTextColour(item, wx.BLACK)
                    elif count == 0:
                        tree.Collapse(item)
                        tree.SetItemTextColour(item, self.grayColour)
                    tree.SetItemText(item, uniConv(label))

        # Expand and collapse the roots based on which profile is
        # selected.
        if profile is pr.getDefaults():
            tree.Expand(self.defaults.root)
            tree.Collapse(self.available.root)
        else:
            tree.Collapse(self.defaults.root)
            tree.Expand(self.available.root)
            
    def refreshItems(self, forProfile, justAddon=None):
        """Refresh the appearance of items."""

        tree = self.getWxWidget()
        if forProfile is not pr.getDefaults():
            defaultAddons = pr.getDefaults().getAddons()
        else:
            defaultAddons = []

        usedAddons = forProfile.getUsedAddons()

        for hier in [self.available, self.defaults]:
            for addon in hier.items.keys():
                if justAddon == None or addon == justAddon:
                    item = hier.items[addon]
                    if addon in usedAddons:
                        tree.SetItemTextColour(item, wx.BLACK)
                        if addon in defaultAddons:
                            # The 'D' icon.
                            imageNum = 2
                        else:
                            # The normal check icon.
                            imageNum = 1
                    else:
                        # The unchecked icon.
                        #tree.SetItemTextColour(item, wx.Colour(100,100,100))
                        imageNum = 0
                    tree.SetItemImage(item, imageNum)

    def onNotify(self, event):
        """Handle a notification event.  Refreshes the contents of the
        tree when an addon is attached to or detached from a profile.

        @param event An events.Notify object.
        """
        if not self.listenToNotifications:
            return
        
        base.Widget.onNotify(self, event)
        
        if event.hasId('addon-attached') or event.hasId('addon-detached'):
            if event.getProfile() is pr.getActive():
                #print "categ labels"
                self.refreshCategoryLabels(event.getProfile())
                #print "items"
                self.refreshItems(event.getProfile())
                #print " done!"                

    def retranslate(self):
        """Update all the items in the tree."""

        if pr.getActive():
            self.refreshCategoryLabels(pr.getActive())
            self.refreshItems(pr.getActive())

    def getSelectedAddon(self):
        """Returns the currently selected addon.

        @return Identifier of the selected addon.
        """
        tree = self.getWxWidget()
        return self.getItemData(tree.GetSelection())

    def lookupCategory(self, item):
        """Returns the Category object that is associated with the
        tree item, if any.

        @return None, or a addons.Category object.
        """
        for hier in [self.available, self.defaults]:
            for category in hier.categories.keys():
                if hier.categories[category] == item:
                    return category
        return None

    def refreshPopupMenu(self):
        """Refresh the popup menu depending on the currently selected
        tree icon.
        """
        tree = self.getWxWidget()
        selectedItem = tree.GetSelection()
        addon = self.getItemData(selectedItem)

        # Send a request to update the popup menu.
        request = events.Notify('addon-popup-request')
        if addon:
            # These shouldn't be hardcoded menus...
            request.addon = ao.get(addon)
            request.category = None
        else:
            request.addon = None
            # Is there a category here?
            request.category = self.lookupCategory(selectedItem)

        # The menu will get updated during send().
        events.send(request)

    def onItemClick(self, ev):
        """Handle the wxWidgets event when the user clicks on the tree
        widget.

        @param ev A wxWidgets event object.
        """
        tree = self.getWxWidget()

        item, flags = tree.HitTest(ev.GetPosition())
        addonName = self.getItemData(item)

        if flags & wx.TREE_HITTEST_ONITEMBUTTON:
            tree.Toggle(item)

        if flags & wx.TREE_HITTEST_ONITEMICON and addonName:
            profile = pr.getActive()
            usedAddons = profile.getUsedAddons()

            if addonName in usedAddons:
                profile.dontUseAddon(addonName)
            else:
                profile.useAddon(addonName)

            # Also change the selection.
            self.selectItem(item)

        elif (flags & wx.TREE_HITTEST_ONITEMLABEL or
              flags & wx.TREE_HITTEST_ONITEMRIGHT):
            # Change the selection.
            self.selectItem(item)

    def selectItem(self, item):
        """Select a particular item in the tree.

        @param item The tree item to select.
        """
        tree = self.getWxWidget()
        tree.SelectItem(item)

        # If an addon is associated with the item, send a notification.
        addon = self.getItemData(item)
        if addon:
            events.sendAfter(events.SelectNotify(self.widgetId, addon))
        else:
            # Perhaps a category was selected?
            cat = self.available.getCategory(tree.GetSelection())
            if cat == None:
                cat = self.defaults.getCategory(tree.GetSelection())
            if cat != None:
                events.sendAfter(events.SelectNotify(self.widgetId,
                                                     cat.getLongId()))

        self.refreshPopupMenu()

    def selectAddon(self, addon):
        """Select the item that has been associated with a given addon.

        @param addon The addon to select.
        """
        for hier in [self.available, self.defaults]:
            if hier.items.has_key(addon):
                self.selectItem(hier.items[addon])
                break

    def onRightClick(self, ev):
        """Handle the wxWidgets even when the user clicks on the tree
        using the right mouse button.

        @param ev A wxWidgets event object.
        """
        tree = self.getWxWidget()
        item, flags = tree.HitTest(ev.GetPosition())

        if (flags & wx.TREE_HITTEST_ONITEMLABEL or
            flags & wx.TREE_HITTEST_ONITEMRIGHT):
            # Change the selection.
            self.selectItem(item)

        # Continue with the normal event handling.
        base.Widget.onRightClick(self, ev)

    def getHierarchy(self, item):
        """Finds the hierarchy where the item is in."""

        for hier in [self.available, self.defaults]:
            if item in hier.categories.values():
                return hier

            if item in hier.items.values():
                return hier

        return None                

    def onItemExpanding(self, ev):
        """Handle the wxWidgets event that is sent when an item is
        about to be expanded.  We will not allow expanding empty
        categories.

        @param ev A wxWidgets event.
        """
        hier = self.getHierarchy(ev.GetItem())

        for addon in hier.items.keys():
            if self.isAddonUnder(addon, ev.GetItem()):
                # There is something under the category.
                ev.Skip()
                return

        ev.Veto()        

    def expandAllCategories(self, doExpand):
        """Expand or collapse all categories in the tree."""

        tree = self.getWxWidget()

        # Affects both hierarchy.
        for hier in [self.available, self.defaults]:
            for category in hier.categories.keys():
                item = hier.categories[category]

                if(doExpand):
                    tree.Expand(item)
                else:
                    tree.Collapse(item)

    def checkSelectedCategory(self, doCheck):
        """Check or uncheck all the addons under the currently
        selected category.  Affects all subcategories.

        @param doCheck True if the addons should be checked.  False if
        unchecked.
        """
        tree = self.getWxWidget()
        profile = pr.getActive()

        # This is probably a category item.  We'll need to be careful.
        categoryItem = tree.GetSelection()

        self.listenToNotifications = False

        for hier in [self.available, self.defaults]:
            for category in hier.categories.keys():
                if hier.categories[category] == categoryItem:
                    # This is the category we are looking for.  We'll
                    # have to iterate through all the addons.
                    for addon in hier.items.keys():
                        if self.isAddonUnder(addon, categoryItem):
                            #print addon
                            if doCheck:
                                profile.useAddon(addon)
                            else:
                                profile.dontUseAddon(addon)
                    break

        # Enable notifications and update with the current profile.
        self.listenToNotifications = True
        self.refreshCategoryLabels(profile)
        self.refreshItems(profile)
