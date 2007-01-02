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

## @file base.py Widget Base Class

import sys, wx
import host, events, language, paths
import sb.confdb as st
import widgets


# Font styles.
NORMAL = 0
BOLD = 1
ITALIC = 2
HEADING = 3
TITLE = 4
SMALL = 5


class Widget:
    """Widget is the abstract base class for all user interface
    widgets.  Each subclass acts as a wrapper around the real
    wxWidget, converting wxWidgets events to commands and
    notifications."""

    def __init__(self, wxWidget):
        """Constructor.
        @param wxWidget The wxWidget this Widget manages.
        """
        self.wxWidget = wxWidget
        self.menuItems = None
        self.focusId = ''
        self.menuId = None

        # Callback functions for listening directly to changes.
        self.reactors = []

        self.setNormalStyle()

        # Listen for right mouse clicks in case a popup menu should be
        # displayed.
        wx.EVT_RIGHT_DOWN(wxWidget, self.onRightClick)
        wx.EVT_RIGHT_UP(wxWidget, self.onRightClick)

        # A notification is sent when the widget is entered.
        wx.EVT_ENTER_WINDOW(wxWidget, self.onEnterWidget)

        # Listen for focus changes.
        wx.EVT_SET_FOCUS(wxWidget, self.onFocus)

        # Register a notification listener. These notifications are expected
        # to be handled by all widgets.
        events.addNotifyListener(self.onNotify, ['language-changed',
                                                 'focus-request'])
        # Widgets are not automatically listening to commands.
        #events.addCommandListener(self.onCommand)

    def getWxWidget(self):
        """Returns the real wxWidget of this widget."""
        return self.wxWidget

    def addProfileChangeListener(self):
        """Adds a listener callback that listens to changes of the active
        profile. Only those widgets that manage a setting should need this."""
        
        events.addNotifyListener(self.onNotify, ['active-profile-changed'])                                 

    def addValueChangeListener(self):
        """Adds a listener callback that listens to value change notifications
        sent with this widget's identifier."""

        events.addNotifyListener(self.onNotify, 
                                 [self.widgetId + '-value-changed'])

    def clear(self):
        """This is called when the widget is removed from the area by
        calling Area.clear().  This is not called when the area gets
        destroyed."""
        pass

    def destroy(self):
        """Destroy the widget."""

        # Remove the notification listener of this widget.
        events.removeNotifyListener(self.onNotify)
        events.removeCommandListener(self.onCommand)

        w = self.getWxWidget()

        # Detach from the containing sizer.
        sizer = w.GetContainingSizer()
        if sizer:
            sizer.Detach(w)

        w.Destroy()

    def retranslate(self):
        """Update the label(s) of the widget by retranslating them.
        This is called when the language string database changes."""
        pass

    def setFocusId(self, identifier):
        """Setting the focus identifier causes the widget to send
        focus notifications using the identifier.

        @param identifier A string.
        """
        self.focusId = identifier

    def focus(self):
        try:
            self.getWxWidget().SetFocus()
        except:
            pass

    def enable(self, doEnable=True):
        """Enable or disable the widget for user input.

        @param doEnable Boolean value.  True if the widget should be
        enabled, fFalse if not.
        """
        self.getWxWidget().Enable(doEnable)

    def disable(self):
        """Update the label(s) of the widget by retranslating them.
        This is called when the language string database changes."""
        self.enable(False)

    def setMinSize(self, width, height):
        """Set a minimum size for the widget."""
        self.getWxWidget().SetMinSize((width, height))

    def setPopupMenuId(self, menuId):
        """Define a popup menu identifier. When the identifier has been defined, 
        a notification is sent just prior to showing a popup menu, allowing 
        adapting the contents of the menu to the current situation."""
        
        self.menuId = menuId

    def setPopupMenu(self, menuItems):
        """Set a popup menu that is displayed when the widget is
        right-clicked (Ctrl-Clicked).

        @param menuItems An array of identifiers.  These will be sent
        as Command events when items are clicked.  Set to None if the
        menu should be disabled.  The array may also contain tuples:
        (identifier, command).
        """
        self.menuItems = menuItems

    def onRightClick(self, ev):
        # Let wxWidgets handle the event, too.
        widget = self.getWxWidget()
        if not widget.IsEnabled():
            return
            
        # Going up or down?
        if ev.ButtonDown():
            # Mouse right down doesn't do anything.
            return

        # Request an update to the popup menu.
        if self.menuId:
            events.send(events.Notify(self.menuId + '-update-request'))

        # Display the popup menu.
        if self.menuItems:
            menu = wx.Menu()
            self.commandMap = {}

            idCounter = 10000

            # Create the menu items.
            for item in self.menuItems:
                if type(item) == tuple:
                    itemId = item[0]
                    itemCommand = item[1]
                else:
                    itemId = item
                    itemCommand = item

                if itemId == '-':
                    # This is just a separator.
                    menu.AppendSeparator()
                    continue
                
                # Generate a new ID for the item.
                wxId = idCounter
                idCounter += 1
                self.commandMap[wxId] = itemCommand
                menu.Append(wxId,
                            widgets.uniConv(language.translate('menu-' + itemId)))
                wx.EVT_MENU(widget, wxId, self.onPopupCommand)

            # Display the menu.  The callback gets called during this.
            widget.PopupMenu(menu, ev.GetPosition())
            menu.Destroy()            

    def onPopupCommand(self, ev):
        events.send(events.Command(self.commandMap[ev.GetId()]))

    def onFocus(self, event):
        """Handle the wxWidgets event that's sent when the widget
        received input focus.

        @param event A wxWidgets event.
        """
        # Send a focus event if a focus identifier has been specified.
        if self.focusId:
            events.send(events.FocusNotify(self.focusId))
        event.Skip()

    def onEnterWidget(self, event):
        """Handle the wxWidget event of entering the window."""

        if self.focusId:
            events.send(events.FocusNotify(self.focusId))
        event.Skip()

    def onNotify(self, event):
        """Handle a notification.  All widgets are automatically
        registered for listening to notifications.

        @param event An event.Notify object.
        """
        # Has someone requested this widget to receive focus?
        if (event.hasId('focus-request') and 
            event.getFocus() == self.focusId):
            self.focus()

        elif event.hasId('language-changed'):
            self.retranslate()
        
    def onCommand(self, event):
        """Handle a command.  All widgets are automatically registered
        for listening to commands.

        @param event An event.Command object.
        """
        # By default we do nothing.
        pass

    def addReaction(self, func):
        """Add a new callback function that will be called at suitable
        times when a reaction is needed (depends on widget type).

        @param func Callback function.
        """

        self.reactors.append(func)

    def react(self):
        """Call all the reaction callback functions."""

        for func in self.reactors:
            func()

    def setStyle(self, textStyle):
        """Set a new style for the text label.
        @param style One of the constants:
        - Widget.NORMAL: default style
        - Widget.BOLD: bold typeface
        - Widget.ITALIC: italic typeface
        - Widget.HEADING: large bold typeface
        - Widget.TITLE: largest bold typeface
        """
        weight = wx.NORMAL
        style = wx.NORMAL
        styleName = 'style-' + \
                    ['normal', 'bold', 'italic', 'heading', 'title', 
                     'small'][textStyle] + '-'

        try:
            size = st.getSystemInteger(styleName + 'size')
        except:
            size = 11

        try:
            font = st.getSystemString(styleName + 'font')
        except:
            font = ''

        try:
            w = st.getSystemString(styleName + 'weight')
            if w == 'bold':
                weight = wx.BOLD
        except:
            pass

        try:
            s = st.getSystemString(styleName + 'slant')
            if s == 'italic':
                style = wx.ITALIC
        except:
            pass

        newFont = wx.Font(size, wx.DEFAULT, style, weight, faceName=font)
        w = self.getWxWidget()
        w.SetFont(newFont)

        #w.SetSize(w.GetBestSize())
        #bestSize = w.GetBestSize()
        #w.SetMinSize((0,bestSize[1]))

        self.updateDefaultSize()

    def updateDefaultSize(self):
        """By default widgets can be scaled horizontally to zero width."""
        w = self.getWxWidget()
        bestSize = w.GetBestSize()
        w.SetMinSize((0, bestSize[1]))

    def resizeToBestSize(self):
        w = self.getWxWidget()
        bestSize = w.GetBestSize()
        w.SetMinSize(bestSize)
        w.SetSize(bestSize)      
        
    def setNormalStyle(self):
        """Set the style of the text label to Widget.NORMAL."""
        self.setStyle(NORMAL)

    def setSmallStyle(self):
        """Set the style of the text label to Widget.SMALL."""
        self.setStyle(SMALL)

    def setBoldStyle(self):
        """Set the style of the text label to Widget.BOLD."""
        self.setStyle(BOLD)

    def setItalicStyle(self):
        """Set the style of the text label to Widget.ITALIC."""
        self.setStyle(ITALIC)

    def setHeadingStyle(self):
        """Set the style of the text label to Widget.HEADING."""
        self.setStyle(HEADING)

    def setTitleStyle(self):
        """Set the style of the text label to Widget.TITLE."""
        self.setStyle(TITLE)

    def freeze(self):
        """Prevent updates from occurring on the screen."""
        self.getWxWidget().Freeze()

    def unfreeze(self):
        """Allow updates to occur."""
        self.getWxWidget().Thaw()
        self.getWxWidget().Refresh()
        
