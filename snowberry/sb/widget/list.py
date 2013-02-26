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

## @file list.py List Widgets

import wx
import host, events, language, paths
import sb.profdb as pr
import sb.confdb
from widgets import uniConv
import base


class List (base.Widget):
    """List control widget.  Supports several styles."""

    # List styles.
    STYLE_SINGLE = 0
    STYLE_MULTIPLE = 1
    STYLE_CHECKBOX = 2
    STYLE_COLUMNS = 3
    
    def __init__(self, parent, wxId, id, style=0):
        self.style = style
        
        if style == List.STYLE_CHECKBOX:
            # Listbox with checkable items.
            base.Widget.__init__(self, wx.CheckListBox(parent, wxId))
        elif style == List.STYLE_COLUMNS:
            base.Widget.__init__(self, wx.ListCtrl(parent, wxId,
                                                   style=wx.LC_REPORT))
                                                          #wx.SIMPLE_BORDER)))
            wx.EVT_LEFT_DOWN(self.getWxWidget(), self.onLeftUp)
            wx.EVT_LIST_ITEM_SELECTED(parent, wxId, self.onItemSelected)
            wx.EVT_LIST_ITEM_DESELECTED(parent, wxId, self.onItemDeselected)
            wx.EVT_LIST_COL_CLICK(parent, wxId, self.onColumnClick)
        else:
            # Normal listbox.
            if style == List.STYLE_MULTIPLE:
                styleFlags = wx.LB_EXTENDED
            elif style == List.STYLE_SINGLE:
                styleFlags = wx.LB_SINGLE
            base.Widget.__init__(self, wx.ListBox(parent, wxId, style=styleFlags))

        # Will be used when sending notifications.
        self.widgetId = id

        self.expandingColumnIndex = -1

        self.items = []
        self.columns = []
        
        wx.EVT_SIZE(self.getWxWidget(), self.onSize)

    def setImageList(self, imageList):
        self.getWxWidget().SetImageList(imageList, wx.IMAGE_LIST_SMALL)

    def onLeftUp(self, ev):
        """Handle the wxWidgets event when the user clicks on the list widget.

        @param ev A wxWidgets event object.
        """
        w = self.getWxWidget()

        item, flags = w.HitTest(ev.GetPosition())
        if item < 0:
            ev.Skip()
            return

        if flags & wx.LIST_HITTEST_ONITEMICON:
            e = events.Notify(self.widgetId + '-icon-click')
            e.item = self.items[w.GetItemData(item)]
            events.send(e)
           
        ev.Skip()
        
    def onColumnClick(self, ev):
        events.send(events.Notify(self.columns[ev.GetColumn()] + '-column-click'))
        ev.Skip()
        
    def onSize(self, event):
        # Resize the expanding column.
        if self.expandingColumnIndex >= 0:
            # This is the new width.
            widget = self.getWxWidget()
            width = widget.GetClientSizeTuple()[0]
            # How much left from the other columns?
            total = 0
            for i in range(widget.GetColumnCount()):
                if i != self.expandingColumnIndex: 
                    total += widget.GetColumnWidth(i)
            widget.SetColumnWidth(self.expandingColumnIndex, width - total - 
                wx.SystemSettings_GetMetric(wx.SYS_VSCROLL_X))

        event.Skip()    

    def addItem(self, identifier, isChecked=False):
        """Append a new item into the listbox.

        @param identifier Identifier of the item.
        """
        w = self.getWxWidget()
        if language.isDefined(identifier):
            visibleText = language.translate(identifier)
        else:
            visibleText = identifier
        w.Append(uniConv(visibleText))
        self.items.append(identifier)

        # In a checklistbox, the items can be checked.
        if self.style == List.STYLE_CHECKBOX:
            if isChecked:
                w.Check(w.GetCount() - 1)

    def removeItem(self, identifier):
        """Remove an item from the listbox.

        @param identifier  Identifier of the item to remove.
        """
        w = self.getWxWidget()
        for i in range(len(self.items)):
            if identifier == self.items[i]:
                w.Delete(i)
                self.items.remove(identifier)
                break

    def addColumn(self, identifier, width=None):
        """Append a column into a list that uses the column style.
        
        @param identifier  Column identifier.
        @param width  Width of the column in pixels. One of the columns
                      can use None for width, which means it is automatically
                      expanded to fill all available space.
        """
        w = self.getWxWidget()
        numCols = w.GetColumnCount()
        if width is None:
            colWidth = 1
            # This column will expand to fill all remaining space.
            self.expandingColumnIndex = len(self.columns)
        else:
            colWidth = width
            
        self.getWxWidget().InsertColumn(numCols,
                                        language.translate(identifier),
                                        width=colWidth)
        self.columns.append(identifier)
        
    def retranslateColumns(self):
        w = self.getWxWidget()
        numCols = w.GetColumnCount()
        for i in range(numCols):
            col = w.GetColumn(i)
            col.SetMask(wx.LIST_MASK_TEXT)
            col.SetText(language.translate(self.columns[i]))
            w.SetColumn(i, col)
        
    def addItemWithColumns(self, identifier, *columns):
        w = self.getWxWidget()

        if type(columns[0]) is int:
            # This is an image item.
            index = w.InsertImageItem(w.GetItemCount(), columns[0])
            #info = wx.ListItem()
            #info.m_itemId = index
            #info.m_col = 0
            #info.m_mask = wx.LIST_MASK_FORMAT
            #info.m_format = wx.LIST_FORMAT_CENTRE
            #w.SetItem(info)
        else:
            index = w.InsertStringItem(w.GetItemCount(), uniConv(columns[0]))
            
        for i in range(1, len(columns)):
            w.SetStringItem(index, i, uniConv(columns[i]))

        # Put the identifier into the data.
        w.SetItemData(index, index)

        self.items.append(identifier)

    def setItemImage(self, identifier, image):
        """Sets the image of an image item.
        
        @param identifier  Item identifier.
        @param image  Image index. Needs an image list.
        """
        try:
            index = self.getItemIndex(identifier)
            self.getWxWidget().SetItemImage(index, image)
        except:
            pass

    def sortItems(self, callback):
        self.getWxWidget().SortItems(callback)

    def clear(self):
        """Remove all the items in the list."""

        if self.style in [List.STYLE_MULTIPLE, List.STYLE_SINGLE, List.STYLE_CHECKBOX]:
            self.getWxWidget().Clear()
        else:
            self.getWxWidget().DeleteAllItems()
        self.items = []

    def getItems(self):
        """Returns a list of all the item identifiers in the list."""
        return self.items
        
    def getItemImages(self):
        """Returns a list of all the item iamges in the list. They use the same order
        as the list returned by self.items. Only supported in the Columns style."""
        
        if self.style != List.STYLE_COLUMNS:
            return None
        
        images = [0] * len(self.items)
        
        w = self.getWxWidget()
        for i in range(w.GetItemCount()):
            info = w.GetItem(i, 0)
            images[w.GetItemData(i)] = info.GetImage()
        
        return images
        
    def getSelectedItems(self):
        """Get the identifiers of all the selected items in the list.
        If the list was created with the List.STYLE_CHECKBOX style,
        this returns all the checked items.

        @return An array of identifiers.
        """
        w = self.getWxWidget()
        selected = []

        if self.style == List.STYLE_CHECKBOX:
            # Return a list of checked items.
            for i in range(w.GetCount()):
                if w.IsChecked(i):
                    selected.append(self.items[i])
            return selected

        if self.style == List.STYLE_COLUMNS:
            # Check the state of each item.
            for i in range(w.GetItemCount()):
                state = w.GetItemState(i, wx.LIST_STATE_SELECTED)
                if state & wx.LIST_STATE_SELECTED:
                    selected.append(self.items[w.GetItemData(i)])
            return selected

        # Compose a list of selected items.
        for sel in w.GetSelections():
            selected.append(self.items[sel])
        return selected

    def getSelectedItem(self):
        """Get the identifier of the currently selected item.  Only
        for single-selection lists.

        @return The identifier.
        """
        index = self.getWxWidget().GetSelection()
        if index == wx.NOT_FOUND:
            return ''
        else:
            return self.items[index]

    def getItemIndex(self, identifier):
        """Does the necessary translation for sorted lists."""

        w = self.getWxWidget()
        
        # Unsorted index.
        index = self.items.index(identifier)

        if self.style == List.STYLE_COLUMNS:
            # Column-style lists may be sorted.
            for i in range(w.GetItemCount()):
                if w.GetItemData(i) == index:
                    index = i
                    break
                
        return index

    def selectItem(self, identifier, doSelect=True):
        """Select one item from the list.  If no identifier is
        provided, the selection is cleared."""

        w = self.getWxWidget()

        if identifier == '' or identifier == None:
            # Clear selection.
            if self.style == List.STYLE_COLUMNS:
                # Check the state of each item.
                for i in range(w.GetItemCount()):
                    state = w.GetItemState(i, wx.LIST_STATE_SELECTED)
                    if state & wx.LIST_STATE_SELECTED:
                        w.SetItemState(i, 0, wx.LIST_STATE_SELECTED)
            return
        
        try:
            index = self.getItemIndex(identifier)

            if self.style == List.STYLE_COLUMNS:
                # Check the one item.
                if doSelect:
                    w.SetItemState(index, wx.LIST_STATE_SELECTED,
                                   wx.LIST_STATE_SELECTED)
                else:
                    w.SetItemState(index, 0, wx.LIST_STATE_SELECTED)
            else:
                w.SetSelection(index)
        except:
            pass

    def deselectItem(self, identifier):
        self.selectItem(identifier, False)
        
    def deselectAllItems(self):
        w = self.getWxWidget()
        if self.style == List.STYLE_COLUMNS:
            self.selectItem(None)
        else:
            for i in range(w.GetCount()):
                w.Deselect(i)

    def ensureVisible(self, identifier):
        """Makes sure an item is visible.

        @param identifier  Identifier of the item to make visible.
        """
        w = self.getWxWidget()
        try:
            index = self.getItemIndex(identifier)
            w.EnsureVisible(index)
        except:
            pass
                
    def checkItem(self, identifier, doCheck=True):
        """Check or uncheck an item in a list that was created with
        the Check style.

        @param identifier Identifier of the item to check.

        @param doCheck True, if the item should be checked.  False, if
        unchecked.
        """
        try:
            index = self.getItemIndex(identifier)
            self.getWxWidget().Check(index, doCheck)
        except:
            # Identifier not found?
            pass

    def moveItem(self, identifier, steps):
        """Move an item up or down in the list.

        @param identifier Item to move.

        @param steps Number of indices to move.  Negative numbers move
        the item upwards, positive numbers move it down.
        """
        w = self.getWxWidget()
        
        for index in range(len(self.items)):
            if self.items[index] == identifier:
                # This is the item to move.
                w.Delete(index)
                del self.items[index]

                # Insert it to another location.
                index += steps
                if index < 0:
                    index = 0
                elif index > len(self.items):
                    index = len(self.items)

                w.Insert(language.translate(identifier), index)
                w.SetSelection(index)

                # Also update the items list.
                self.items = self.items[:index] + [identifier] + \
                             self.items[index:]
                break

    def onItemSelected(self, event):
        """Handle the wxWidgets item selection event.

        @param event A wxWidgets event.
        """
        if self.widgetId and self.style == List.STYLE_COLUMNS:
            # Identifier of the selected item is sent with the
            # notification.
            item = self.items[self.getWxWidget().GetItemData(event.GetIndex())]
            
            # Send a notification about the selected item.
            events.send(events.SelectNotify(self.widgetId, item))

        event.Skip()
        self.react()

    def onItemDeselected(self, event):
        """Handle the wxWidgets item deselection event.

        @param event  A wxWidgets event.
        """
        if self.widgetId:
            item = self.items[self.getWxWidget().GetItemData(event.GetIndex())]
            events.send(events.DeselectNotify(self.widgetId, item))

        event.Skip()
        

class DropList (base.Widget):
    """A drop-down list widget."""

    def __init__(self, parent, wxId, id):
        base.Widget.__init__(self, wx.Choice(parent, wxId))

        # Will be used when sending notifications.
        self.widgetId = id

        self.items = []            

        self.addSorted = False

        # We want focus notifications.
        self.setFocusId(id)

        # Listen to profile changes so we can update the selection.
        self.addProfileChangeListener()

        # Handle item selection events.
        wx.EVT_CHOICE(parent, wxId, self.onItemSelected)
        wx.EVT_LEFT_DOWN(self.getWxWidget(), self.onLeftClick)

    def setSorted(self, doSort=True):
        self.addSorted = doSort

    def onLeftClick(self, event):
        if self.widgetId:
            events.send(events.FocusNotify(self.widgetId))
            self.getWxWidget().SetFocus()
        event.Skip()

    def onItemSelected(self, ev):
        """Handle the wxWidgets event that is sent when the current
        selection changes in the list box.

        @param ev A wxWidgets event.
        """
        if self.widgetId:
            newSelection = self.items[ev.GetSelection()]

            if sb.confdb.isSettingDefined(self.widgetId):
                # This is a setting.
                if newSelection == 'default':
                    pr.getActive().removeValue(self.widgetId)
                else:
                    pr.getActive().setValue(self.widgetId, newSelection)

                # Notify everyone of the change.
                events.send(events.EditNotify(self.widgetId, newSelection))
            else:
                # Normal list.
                events.send(events.SelectNotify(self.widgetId, newSelection))

    def onNotify(self, event):
        base.Widget.onNotify(self, event)

        if self.widgetId and sb.confdb.isSettingDefined(self.widgetId):
            if event.hasId('active-profile-changed'):
                # Get the value for the setting as it has been defined
                # in the currently active profile.
                self.getFromProfile(pr.getActive())

    def getFromProfile(self, profile):
        if sb.confdb.isSettingDefined(self.widgetId):
            value = profile.getValue(self.widgetId, False)
            if value:
                self.selectItem(value.getValue())
            else:
                self.selectItem('default')

    def clear(self):
        """Delete all the items."""
        self.items = []
        self.getWxWidget().Clear()

    def __filter(self, itemText):
        """Filter the item text so it can be displayed on screen."""

        # Set the maximum length.
        if len(itemText) > 30:
            return '...' + uniConv(itemText[-30:])

        return uniConv(itemText)

    def retranslate(self):
        """Update the text of the items in the drop list. Preserve
        current selection."""
        
        drop = self.getWxWidget()
        sel = drop.GetSelection()

        # We will replace all the items in the list.
        drop.Clear()        
        for i in range(len(self.items)):
            drop.Append(self.__filter(language.translate(self.items[i])))

        drop.SetSelection(sel)

    def addItem(self, item, toIndex=None):
        """Append a list of items into the drop-down list.

        @param items An array of strings.
        """
        w = self.getWxWidget()
        
        try:
            # Does this already exist?
            index = self.items.index(item)
            # Update label.
            w.SetString(index, language.translate(item))
            return
        except:
            pass
            
        if toIndex is not None:
            # A specific index has been given.
            self.items.insert(toIndex, item)
            w.Insert(language.translate(item), toIndex)
        else:
            if self.addSorted:
                # Need to locate the right index.
                toIndex = 0
                # Sort alphabetically based on identifier.
                for i in range(len(self.items)):
                    if cmp(item, self.items[i]) < 0:
                        break
                    toIndex = i + 1
                self.items.insert(toIndex, item)
                w.Insert(language.translate(item), toIndex)
            else:
                # Just append.
                self.items.append(item)
                w.Append(self.__filter(language.translate(item)))

    def removeItem(self, item):
        try:
            index = self.items.index(item)
            self.getWxWidget().Delete(index)
            del self.items[index]
        except:
            pass

    def hasItem(self, item):
        return item in self.items

    def selectItem(self, item):
        """Change the current selection.  This does not send any
        notifications.

        @param item Identifier of the item to select.
        """
        for i in range(len(self.items)):
            if self.items[i] == item:
                self.getWxWidget().SetSelection(i)
                break

    def getSelectedItem(self):
        """Returns the identifier of the selected item.

        @return An identifier string.
        """
        return self.items[self.getWxWidget().GetSelection()]

    def sortItems(self):
        """Sort all the items in the list."""
        w = self.getWxWidget()
        w.Clear()
        def sorter(a, b):
            if a[0] == 'default':
                return -1
            elif b[0] == 'default':
                return 1
            return cmp(a[1], b[1])
        translated = map(lambda i: (i, language.translate(i)), self.items)
        translated.sort(sorter)
        self.items = []
        for id, item in translated:
            self.items.append(id)
            w.Append(self.__filter(item))

    def getItems(self):
        """Returns the identifiers of the items in the list.

        @return An array of identifiers."""
        return self.items


class HtmlListBox (wx.HtmlListBox):
    """An implementation of the abstract wxWidgets HTML list box."""

    def __init__(self, parent, wxId, itemsListOwner, style=0):
        wx.HtmlListBox.__init__(self, parent, wxId, style=style)

        # A reference to the owner of the array where the items are
        # stored.  Items is a list of tuples: (id, presentation)
        self.itemsListOwner = itemsListOwner

        # Force a resize event at this point so that GTK doesn't
        # select the wrong first visible item.
        #self.SetSize((100, 100))

    def OnGetItem(self, n):
        """Retrieve the representation of an item.  This method must
        be implemented in the derived class and should return the body
        (i.e. without &lt;html&gt; nor &lt;body&gt; tags) of the HTML
        fragment for the given item.

        @param n Item index number.

        @return HTML-formatted contents of the item.
        """
        return self.itemsListOwner.items[n][1]


class FormattedList (base.Widget):
    """A list widget with HTML-formatted items."""

    def __init__(self, parent, wxId, id, style=0):
        """Construct a FormattedList object.

        @param parent Parent wxWidget.
        @param wxId wxWidgets ID of the new widget (integer).
        @param id Identifier of the new widget (string).
        """
        # This item array is also used by the HtmlListBox instance.
        # This is a list of tuples: (id, presentation)
        self.items = []

        base.Widget.__init__(self, HtmlListBox(parent, wxId, self, style=style))

        self.widgetId = id

        # By default, items are added in alphabetical order.
        self.addSorted = True

        # Listen for selection changes.
        wx.EVT_RIGHT_DOWN(self.getWxWidget(), self.onRightDown)      
        wx.EVT_LISTBOX(parent, wxId, self.onItemSelected)
        wx.EVT_LISTBOX_DCLICK(parent, wxId, self.onItemDoubleClick)

    def onRightDown(self, event):
        """Right button down."""

        w = self.getWxWidget()

        #print "right down in list " + str(event.GetPosition())
        item = w.HitTest(event.GetPosition())
        #print item
        if item >= 0:
            w.SetSelection(item)
            if self.widgetId:
                events.send(events.SelectNotify(
                    self.widgetId, self.items[item][0]))

        event.Skip()

    def setSorted(self, doSort):
        self.addSorted = doSort

    def clear(self):
        """Delete all the items."""
        self.items = []
        self.__updateItemCount()

    def addItem(self, itemId, rawItemText, toIndex=None):
        """Add a new item into the formatted list box.  No translation
        is done.  Instead, the provided item text is displayed using
        HTML formatting.

        @param itemId Identifier of the item.  This is sent in
        notification events.

        @param itemText Formatted text for the item.

        @param toIndex Optional index where the item should be placed.
        If None, the item is added to the end of the list.
        """
        itemText = uniConv(rawItemText)
        
        # If this item identifier already exists, update its
        # description without making any further changes.
        for i in range(len(self.items)):
            if self.items[i][0] == itemId:
                self.items[i] = (itemId, itemText)

                # Let the widget know that a change has occured.
                # Although it may be that this does nothing...
                self.__updateItemCount()
                return

        newItem = (itemId, itemText)
        if toIndex == None:
            if self.addSorted:
                toIndex = 0
                # Sort alphabetically based on identifier.
                for i in range(len(self.items)):
                    if cmp(itemId, self.items[i][0]) < 0:
                        break
                    toIndex = i + 1
            else:
                toIndex = len(self.items)

        self.items = self.items[:toIndex] + [newItem] + \
                     self.items[toIndex:]
        self.__updateItemCount()

    def removeItem(self, itemId):
        """Remove an item from the list.

        @param itemId Identifier of the item to remove.
        """
        # Filter out the unwanted item.
        self.items = [item for item in self.items
                      if item[0] != itemId]
        self.__updateItemCount()

    def __updateItemCount(self):
        w = self.getWxWidget()
        w.Hide()
        w.SetItemCount(len(self.items))
        w.SetSelection(self.getSelectedIndex())
        w.Show()

        #w.Freeze()
        #w.SetItemCount(len(self.items))
        #w.SetSelection(self.getSelectedIndex())
        #w.Thaw()
        #w.Refresh()

    def getItemCount(self):
        """Returns the number of items in the list."""
        
        return len(self.items)

    def selectItem(self, itemId):
        """Select an item in the list."""
        for i in range(len(self.items)):
            if self.items[i][0] == itemId:
                self.getWxWidget().SetSelection(i)
                break

    def getSelectedIndex(self):
        count = len(self.items)
        sel = self.getWxWidget().GetSelection()
        if sel >= count:
            sel = count - 1
        return sel

    def getSelectedItem(self):
        """Return the selected item.

        @return Identifier of the selected item.
        """
        return self.items[self.getSelectedIndex()][0]

    def hasItem(self, itemId):
        """Checks if the list contains an item with the specified
        identifier.

        @param itemId Identifier of the item to check.

        @return True, if the item exists.  Otherwise False.
        """
        for item in self.items:
            if item[0] == itemId:
                return True
        return False

    def onItemSelected(self, ev):
        """Handle the wxWidgets event that is sent when the current
        selection changes in the list box.

        @param ev A wxWidgets event.
        """
        ev.Skip()
        events.send(events.SelectNotify(self.widgetId,
                                        self.items[ev.GetSelection()][0]))

    def onItemDoubleClick(self, ev):
        ev.Skip()
        events.send(events.SelectNotify(self.widgetId,
                                        self.items[ev.GetSelection()][0],
                                        True))
