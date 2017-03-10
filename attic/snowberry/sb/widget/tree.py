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


class Tree (base.Widget):
    """The tree widget. Item texts are translated from language string identifiers."""
    
    def __init__(self, parent, wxId, id):
        base.Widget.__init__(self, wx.TreeCtrl(parent, wxId))

        # Will be used when sending notifications.
        self.widgetId = id
        self.rootId = id + '-root'

        self.clear()

        # Load the tree icons.
        #images = wx.ImageList(16, 16)
        #images.Add(wx.Bitmap(paths.findBitmap('unchecked')))
        #images.Add(wx.Bitmap(paths.findBitmap('checked')))
        #images.Add(wx.Bitmap(paths.findBitmap('defcheck')))
        #if host.isWindows():
        #    images.Add(wx.Bitmap(paths.findBitmap('closedfolder')))
        #    images.Add(wx.Bitmap(paths.findBitmap('openfolder')))
        #self.getWxWidget().AssignImageList(images)

        ## @todo Should this be LEFT_UP?
        #wx.EVT_LEFT_DOWN(self.getWxWidget(), self.onItemClick)

        # Right-clicking in the tree must change the selection in
        # addition to displaying the context menu.
        #wx.EVT_RIGHT_DOWN(self.getWxWidget(), self.onRightClick)

        # We will not allow expanding totally empty categories.
        #wx.EVT_TREE_ITEM_EXPANDING(parent, wxId, self.onItemExpanding)

        wx.EVT_TREE_SEL_CHANGED(parent, wxId, self.onSelectionChanged)

        #self.grayColour = wx.Colour(192, 192, 192)

        # The root items.
        #self.removeAll()

        # Clear the list of item ancestors (addon name -> [parent items]).
        #self.itemAncestors = {}
        
        #self.listenToNotifications = True
        
        # In addition to the normal notifications, listen to addon attachment
        # and detachment notifications.
        #events.addNotifyListener(self.onNotify, ['addon-attached', 
        #                                         'addon-detached'])

    def clear(self):
        """Delete all the items in the tree."""
        self.getWxWidget().DeleteAllItems()      
        self.items = {}
        self.addItem(self.rootId)
        
    def addItem(self, identifier, parentId=None):
        """Add an item into the tree.
        
        @param identifier  Identifier of the item.
        @param parentIdentifier  Identifier of the parent item. 
                                 If None, the added item is a root-level item.
        """
        # If no parent has been specified, use the root.
        if parentId is None:
            parentId = self.rootId
            
        # Look up the parent wxTreeItemId.
        w = self.getWxWidget()
        try:
            parentItem = self.items[parentId]
            item = w.AppendItem(parentItem, language.translate(identifier))
        except:
            item = w.AddRoot(language.translate(identifier))

        # Store the identifier as the item data.
        #w.SetPyData(item, identifier)
        
        self.items[identifier] = item
                
    #def onSize(self, event):
    #    w, h = self.getWxWidget().GetClientSizeTuple()
    #    self.list.SetDimensions(0, 0, w, h)        
                
    #def insertSorted(self, parentItem, label):
    #    """Insert a tree item under the parent item, so that it's in
    #    the proper alphabetical order.
    #    """
    #    tree = self.getWxWidget()
    #    index = 0

    #    # Find the item before which the new item will be inserted.
    #    child, cookie = tree.GetFirstChild(parentItem)
    #    while child:
    #        # If we hit a Category item, we must stop.
    #        if self.getItemData(child) == None:
    #            break
    #        if cmp(label.lower(), tree.GetItemText(child).lower()) < 0:
    #            # Insert it before this one.
    #            break
    #        child = tree.GetNextSibling(child)
    #        index += 1
    #        
    #    item = tree.InsertItemBefore(parentItem, index, label)
    #    return item

    #def setItemData(self, item, data):
    #    self.getWxWidget().SetPyData(item, data)

    #def getItemData(self, item):
    #    try:
    #        data = self.getWxWidget().GetPyData(item)
    #        return data
    #    except:
    #        return None

    def retranslate(self):
        """Update all the items in the tree."""

        if pr.getActive():
            pass

    def lookupItem(self, wxItemId):
        for i in self.items.keys():
            if self.items[i] == wxItemId:
                return i
        return ''

    def getSelectedItem(self):
        w = self.getWxWidget()
        #id = w.GetPyData(w.GetSelection())
        id = self.lookupItem(w.GetSelection())
        return id
    
    def onSelectionChanged(self, treeEvent):
        """Send a notification that the selection has changed in the tree."""
        
        item = treeEvent.GetItem()
        events.send(events.SelectNotify(self.widgetId, self.lookupItem(item)))

    def onItemClick(self, ev):
        """Handle the wxWidgets event when the user clicks on the tree
        widget.

        @param ev A wxWidgets event object.
        """
        tree = self.getWxWidget()

    def selectItem(self, identifier):
        """Select a particular item in the tree.

        @param identifier  The tree item to select.
        """
        try:
            tree = self.getWxWidget()
            tree.SelectItem(self.items[identifier])
        except:
            # Ignore unknown identifiers.
            pass
        
    def getExpandedItems(self):
        """Returns a list of the identifiers of all expanded items."""
        tree = self.getWxWidget()
        expanded = []
        for identifier in self.items:
            if tree.IsExpanded(self.items[identifier]):
                expanded.append(identifier)
        return expanded
        
    def expandItem(self, identifier, doExpand=True):
        tree = self.getWxWidget()
        if doExpand:
            tree.Expand(self.items[identifier])
        else:
            tree.Collapse(self.items[identifier])

    def collapseItem(self, identifier):
        self.expandItem(identifier, False)

    def sortItemChildren(self, identifier):
        self.getWxWidget().SortChildren(self.items[identifier])
