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

## @file tab.py Tab Area Widgets

import wx
import ui, host, events, language, paths
import sb.profdb as pr
import widgets as wg
import base
from list import FormattedList


class FormattedTabArea (FormattedList):
    """FormattedTabArea extends FormattedList by pairing it up with a
    previously created MultiArea.  This class implements the same
    interface as TabArea.

    This widget class is different from the other classes.  The other
    classes encapsulate a single wxWidgets widget.  FormattedTabArea
    combines a FormattedList with a MultiArea object."""

    def __init__(self, parent, wxId, id):
        """Constuct a new formatted tab area.

        @param parent The parent wxPanel.

        @param wxId The ID number of the formatted list.

        @param id The identifier of the formatted list.

        @param ownerArea The Area in which the formatted tab area
        resides.
        """
        FormattedList.__init__(self, parent, wxId, id,
                               style=wx.SUNKEN_BORDER)

        self.setSorted(False)
        self.multi = None

        # List of all currently hidden tabs.
        self.hiddenTabs = []

    def setMultiArea(self, multiArea):
        """Set the MultiArea that will react to the changes in the list.

        @param multiArea The ui.MultiArea object to pair up with the
        list.
        """
        self.multi = multiArea

    def __getPresentation(self, tabId):
        """Compose the HTML presentation for a tab identifier.

        @return HTML content as a string.
        """
        # Kludge for the game-options icon.  There should be a way to
        # map the icon more flexibly so this hardcoding wouldn't be
        # needed.
        if tabId == 'game-options':
            game = 'game-jdoom'
            if pr.getActive():
                for c in pr.getActive().getComponents():
                    if c[:5] == 'game-':
                        game = c
                        break
            imageName = language.translate(game + '-icon')
        else:
            imageName = language.translate(tabId + '-icon')

        return ('<table width="100%" border=0 cellspacing=3 cellpadding=1>' +
                '<tr><td width=35><img width=32 height=32 ' +
                'src="%s"></td><td align="left" valign="center">%s</td>' %
                (paths.findBitmap(imageName), language.translate(tabId)) +
                '</tr></table>')

    def retranslate(self):
        """Update the icons and names of the categories."""

        for tab in self.getTabs():
            self.updateIcon(tab)

        self.multi.refresh()

    def getTabs(self):
        """Compose a list of all the areas of the tab area.  Hidden
        areas are included, too.

        @return An array that contains identifiers.
        """
        return self.multi.getPages()

    def getSelectedTab(self):
        """Returns the identifier of the currently selected tab.
        Returns None if no tab is selected."""

        return self.getSelectedItem()

    def addTab(self, id):
        """Add a new tab into the TabArea widget.  The tab is appended
        after previously added tabs.

        @param id Identifier for the tab.  The name and the icon of
        the tab will be determined using this identifier.

        @return The Area object for the contents of the tab.
        """
        area = self.multi.createPage(id)

        # Add it to the end.
        self.addItem(id, self.__getPresentation(id))

        # Automatically show the first page.
        if self.getItemCount() == 1:
            self.selectTab(id)

        return area

    def removeAllTabs(self):
        """Remove all tabs from the tab area."""

        for id in self.getTabs():
            self.removeTab(id)

    def removeTab(self, id):
        """Remove an existing tab from the TabArea widget.

        @param id Identifier of the tab to remove.
        """
        if id in self.hiddenTabs:
            self.hiddenTabs.remove(id)

        self.multi.removePage(id)
        self.removeItem(id)

    def selectTab(self, selectedId):
        """Select a specific tab.

        @param selectedId Identifier of the tab to select.
        """
        self.selectItem(selectedId)
        self.multi.showPage(selectedId)

    def hideTab(self, identifier):
        """Hide a tab in the tab area.

        @param identifier Identifier of the tab to hide.
        """
        self.showTab(identifier, False)

    def showTab(self, identifier, doShow=True):
        """Show a hidden tab in the tab area, or hide a tab.  Hiding
        and showing does not alter the order of the tabs.

        @param identifier Identifier of the tab to show or hide.

        @param doShow True, if the tab should be shown; False, if hidden.
        """
        if doShow and identifier in self.hiddenTabs:
            self.hiddenTabs.remove(identifier)
            self.addItem(identifier, self.__getPresentation(identifier), 0)

        elif not doShow and identifier not in self.hiddenTabs:
            # Move the selection to the previous item.
            #if self.getSelectedTab() == identifier:
            #    index = min(self.getItemCount() - 1, self.getSelectedIndex() + 1)
            #    self.selectTab(self.items[index][0])

            self.hiddenTabs.append(identifier)
            self.removeItem(identifier)

    def onItemSelected(self, ev):
        FormattedList.onItemSelected(self, ev)

        self.multi.showPage(self.getSelectedItem())

    def updateIcon(self, identifier):
        """Update the presentation of the specified tab.

        @param identifier Tab identifier.
        """
        self.addItem(identifier, self.__getPresentation(identifier))

#    def updateIcons(self):
#        """Update the items in the formatted list."""
#
#        #tabs = self.getTabs()
#        #self.addItem(




class TabArea (base.Widget):
    """TabArea is a widget that implements a tab area.  It supports a
    number of different styles: TabArea.STYLE_ICON and
    TabArea.STYLE_CHOICE.  In the icon style, the user can select the
    visible tab by clicking on one of the icons in the list.  In the
    choice style, there is drop-down list for the tab titles.
    """

    # Tab area styles.
    STYLE_ICON = 'icon'
    STYLE_HORIZ_ICON = 'horiz-icon'
    STYLE_DROP_LIST = 'drop'
    STYLE_BASIC = 'basic'
    STYLE_FORMATTED = 'formatted'

    def __init__(self, parent, wxId, id, style):
        # The formatted tab area style uses another widget entirely.
        if style == TabArea.STYLE_FORMATTED:
            raise Exception('not supported here')

        # Create the appropriate widget.
        if style == TabArea.STYLE_ICON:
            w = wx.Listbook(parent, wxId, style=wx.LB_LEFT)
            w.GetListView().SetSingleStyle(wx.LC_AUTOARRANGE)
        elif style == TabArea.STYLE_HORIZ_ICON:
            w = wx.Listbook(parent, wxId, style=wx.LB_TOP)
        elif style == TabArea.STYLE_BASIC:
            w = wx.Notebook(parent, wxId)
        else:
            w = wx.Choicebook(parent, wxId, style=wx.CHB_TOP)

        base.Widget.__init__(self, w)

        self.style = style

        # The horizontal icon style is handled like the normal icon
        # style.
        if self.style == TabArea.STYLE_HORIZ_ICON:
            self.style = TabArea.STYLE_ICON

        # The IconTabs widget remembers its id so it can send
        # notifications events.
        self.widgetId = id

        # The panel map is used when figuring out the IDs of each page
        # when an event occurs.  It contains an ordered array of
        # tuples: (identifier, panel).
        self.panelMap = []
        self.tabAreas = {}

        if self.style == TabArea.STYLE_ICON:
            wx.EVT_LISTBOOK_PAGE_CHANGED(parent, wxId, self.onTabChange)
            # The icon style uses the icons.
            w.SetImageList(wg.iconManager.getImageList())
        elif style == TabArea.STYLE_BASIC:
            wx.EVT_NOTEBOOK_PAGE_CHANGED(parent, wxId, self.onTabChange)
        else:
            wx.EVT_CHOICEBOOK_PAGE_CHANGED(parent, wxId, self.onTabChange)

    def clear(self):
        """When a TabArea is cleared, it must destroy all of its tabs,
        which contain subareas."""

        self.removeAllTabs()
        base.Widget.clear(self)

    def getTabs(self):
        """Compose a list of all the areas of the tab area.  Hidden
        areas are included, too.

        @return An array that contains identifiers.
        """
        return map(lambda mapped: mapped[0], self.panelMap)

    def __lookupPanel(self, panel):
        """Looks up a panel identifier in the panel map.

        @param panel A wxPanel object.

        @return Identifier of the tab.  None, if panel not found.
        """
        for id, mapped in self.panelMap:
            if panel == mapped:
                return id
        return None

    def __getTabIndex(self, id):
        """Determine the index of a specific tab in the tab area."""

        book = self.getWxWidget()
        for i in range(book.GetPageCount()):
            if self.__lookupPanel(book.GetPage(i)) == id:
                return i
        return None

    def __getPanelIndex(self, panel):
        """Determine the index of a specific panel in the tab area."""

        book = self.getWxWidget()
        for i in range(book.GetPageCount()):
            if book.GetPage(i) is panel:
                return i
        return None

    def addTab(self, id):
        """Add a new tab into the TabArea widget.  The tab is appended
        after previously added tabs.

        @param id Identifier for the tab.  The name and the icon of
        the tab will be determined using this identifier.

        @return The Area object for the contents of the tab.
        """
        book = self.getWxWidget()
        #tab = scrolled.ScrolledPanel(book, -1)
        #tab.SetupScrolling()
        tab = wx.Panel(book, -1, style=wx.CLIP_CHILDREN)

        if host.isWindows():
            tab.SetBackgroundStyle(wx.BG_STYLE_SYSTEM)

        # Put the new tab in the page map so that when an event
        # occurs, we will know the ID of the tab.
        self.panelMap.append((id, tab))

        # Insert the page into the book.
        book.AddPage(tab, language.translate(id))

        import sb.widget.area
        area = sb.widget.area.Area(id, tab, ui.ALIGN_VERTICAL)
        area.setExpanding(True)
        area.setWeight(1)

        # Store the Area object in a dictionary for quick access.
        self.tabAreas[id] = area

        self.updateIcons()

        return area

    def removeTab(self, id):
        """Remove an existing tab from the TabArea widget.

        @param id Identifier of the tab to remove.
        """
        panel = None
        for tabId, mapped in self.panelMap:
            if tabId == id:
                # This is the panel to remove.
                panel = mapped
                break

        if not panel:
            # It doesn't exist.
            return

        # Remove the panel from the map and destroy it.
        for i in range(len(self.panelMap)):
            if self.panelMap[i][1] is panel:
                del self.panelMap[i]
                break

        # Remove the page from the tab area, but this doesn't
        # destroy the panel.
        book = self.getWxWidget()
        for i in range(book.GetPageCount()):
            if book.GetPage(i) == panel:
                book.RemovePage(i)
                break

        self.tabAreas[id].destroy()
        del self.tabAreas[id]

    def removeAllTabs(self):
        """Remove all tabs from the tab area."""

        for id in self.getTabs():
            self.removeTab(id)

    def selectTab(self, selectedId):
        """Select a specific tab.

        @param id Tab identifier.
        """
        index = self.__getTabIndex(selectedId)
        if index != None:
            self.getWxWidget().SetSelection(index)

    def hideTab(self, identifier):
        """Hide a tab in the tab area.

        @param identifier Identifier of the tab to hide.
        """
        self.showTab(identifier, False)

    def showTab(self, identifier, doShow=True):
        """Show a hidden tab in the tab area, or hide a tab.  Hiding
        and showing does not alter the order of the tabs.

        @param identifier Identifier of the tab to show or hide.

        @param doShow True, if the tab should be shown; False, if hidden.
        """
        book = self.getWxWidget()

        if not doShow:
            # Hide the page by removing it from the widget.
            index = self.__getTabIndex(identifier)
            if index != None:
                # Is this currently selected?
                if index == book.GetSelection():
                    # Automatically select another page.
                    if index > 0:
                        book.SetSelection(index - 1)
                    elif index < book.GetPageCount() - 1:
                        book.SetSelection(index + 1)

                # This won't destroy the panel or its contents.
                book.RemovePage(index)

        else:
            # Show the page by inserting it to the correct index.
            for tabId, panel in self.panelMap:
                if tabId == identifier:
                    # Determine the location of the shown tab.
                    where = 0
                    for id, mapped in self.panelMap:
                        index = self.__getPanelIndex(mapped)
                        if id == identifier:
                            if index != None:
                                # The panel appears to be visible already.
                                return
                            break
                        if index != None:
                            where = index + 1

                    # Insert this panel back into the book.
                    book.InsertPage(where, panel,
                                    language.translate(identifier))
                    self.updateIcons()
                    return

    def updateIcons(self):
        """Update all the icons used by the tab area labels, if using
        an icon style.
        """
        if self.style == TabArea.STYLE_ICON:
            # Update all the tabs.
            for identifier, panel in self.panelMap:
                pageIndex = self.__getPanelIndex(panel)
                if pageIndex != None:
                    iconName = identifier + '-icon'
                    self.getWxWidget().SetPageImage(pageIndex,
                                                    wg.iconManager.get(iconName))

    def retranslate(self):
        """Retranslate the tab labels."""

        for identifier, panel in self.panelMap:
            pageIndex = self.__getPanelIndex(panel)
            if pageIndex != None:
                try:
                    self.getWxWidget().SetPageText(
                        pageIndex, language.translate(identifier))
                except:
                    # wxWidgets hasn't implemented the appropriate method?
                    pass

    def onTabChange(self, ev):
        """Handle the wxWidgets event that occurs when the currently
        selected tab changes."""

        # Figure out which panel is now visible.
        tabPanel = self.getWxWidget().GetPage(ev.GetSelection())

        # Send a notification event.
        notification = events.SelectNotify(self.widgetId,
                                           self.__lookupPanel(tabPanel))
        events.send(notification)
