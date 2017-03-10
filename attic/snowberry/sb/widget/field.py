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

## @file field.py Editor Fields

import sys
import wx
import wx.lib.intctrl as intctrl
import host, events, language, paths
import sb.profdb as pr
import base


def uniDeconv(str):
    """Encode Unicode back to local character encoding.

    @param str  Unicode string.
    
    @return String.
    """
    if host.isWindows():
        # Don't do Unicode on Windows.
        return str
    elif type(str) != unicode:
        return str
    else:
        return str.encode(host.getEncoding())


class TextField (base.Widget):
    """A text field for entering a single-line string of text."""

    def __init__(self, parent, wxId, id):
        base.Widget.__init__(self, wx.TextCtrl(parent, wxId))
        self.widgetId = id

        # The default font looks the best.
        self.getWxWidget().SetFont(wx.NORMAL_FONT)

        # We want focus notifications.
        self.setFocusId(id)

        # Listen for content changes.
        wx.EVT_TEXT(parent, wxId, self.onTextChange)

        # This must be set to True when notifications need to be sent.
        self.willNotify = True

        # If True, a value-changed notification will be immediately
        # reflected in the text field.
        self.reactToNotify = True

        # The default validator accepts anything.
        self.validator = lambda text: True
        
        # Listen to value changes.
        self.addValueChangeListener()
        self.addProfileChangeListener()

    def getText(self):
        """Return the text in the text field."""
        return uniDeconv(self.getWxWidget().GetValue())

    def setText(self, text):
        self.willNotify = False
        if text != None:
            self.getWxWidget().SetValue(text)
        else:
            self.getWxWidget().SetValue('')
        self.willNotify = True

        # This does cause a reaction, though.
        self.react()

    def select(self, fromPos=-1, toPos=-1):
        """Selects a range in the text field. By default, selects everything.
        @param fromPos  Start position for the selection.
        @param endPos  End position for the selection.
        """
        self.getWxWidget().SetSelection(fromPos, toPos)

    def setValidator(self, validatorFunc):
        """Set the validator function.  The validator returns True if
        the text given to it is valid.  By default anything is valid.

        @param validatorFunc A function that takes the text as a
        parameter.
        """
        self.validator = validatorFunc

    def onTextChange(self, ev):
        """Handle the wxWidgets event that occurs when the contents of
        the field are updated (for any reason).

        @param ev wxWidgets event.
        """
        if self.willNotify and self.widgetId:
            newText = self.getWxWidget().GetValue()

            if not self.validator(newText):
                # The new text is not valid.  Nothing will be done.
                return

            self.reactToNotify = False
            if not len(newText):
                pr.getActive().removeValue(self.widgetId)
            else:
                pr.getActive().setValue(self.widgetId, newText)
            self.reactToNotify = True

            # Send a notification.
            events.sendAfter(events.EditNotify(self.widgetId, newText))

        self.react()
        
    def getFromProfile(self, profile):
        """Gets the text from a profile. Needs a widget identifier."""
        # Get the value for the setting as it has been defined
        # in the currently active profile.
        value = profile.getValue(self.widgetId, False)
        if value:
            self.setText(value.getValue())
        else:
            self.setText('')
        
    def onNotify(self, event):
        """Notification listener.

        @param event An events.Notify object.
        """
        base.Widget.onNotify(self, event)

        if self.reactToNotify and event.hasId(self.widgetId + '-value-changed'):
            # This is our value.
            self.setText(event.getValue())

        if self.widgetId and event.hasId('active-profile-changed'):
            self.getFromProfile(pr.getActive())
        

class NumberField (base.Widget):
    def __init__(self, parent, wxId, id):
        base.Widget.__init__(self, intctrl.IntCtrl(parent, wxId,
                                                   limited=False,
                                                   allow_none=True))
        self.widgetId = id

        # Clear the widget by default.
        self.getWxWidget().SetValue(None)

        # The default font looks the best.
        self.getWxWidget().SetFont(wx.NORMAL_FONT)

        # If True, a value-changed notification will cause the field
        # to change.
        self.reactToNotify = True

        # We want focus notifications.
        self.setFocusId(id)

        # Listen for changes.
        intctrl.EVT_INT(parent, wxId, self.onChange)
        
        # Listen to value changes.
        self.addValueChangeListener()
        self.addProfileChangeListener()

    def setRange(self, min, max):
        """Set the allowed range for the field.  Setting a limit to
        None will remove that limit entirely.

        @param min Minimum value for the range.
        @param max Maximum value for the range.
        """
        self.getWxWidget().SetBounds(min, max)

    def setValue(self, value):
        """Set a new value for the number field.

        @param value The new value.  Set to None to clear the field.
        """
        # Clear the widget by default.
        self.getWxWidget().SetValue(value)

    def onChange(self, ev):
        """Handle the wxWidgets event that is sent when the contents
        of the field changes."""
        if self.widgetId:
            if self.getWxWidget().IsInBounds():
                v = ev.GetValue()
                if v is not None:
                    newValue = str(v)
                else:
                    newValue = ''
            else:
                newValue = None

            self.reactToNotify = False
            if newValue == None:
                pr.getActive().removeValue(self.widgetId)
            else:
                pr.getActive().setValue(self.widgetId, newValue)
            self.reactToNotify = True

            # Notification about the change.
            events.sendAfter(events.EditNotify(self.widgetId, newValue))

    def onNotify(self, event):
        base.Widget.onNotify(self, event)
        
        if self.widgetId:
            if self.reactToNotify and event.hasId(self.widgetId + '-value-changed'):
                # This is our value.
                if event.getValue() != None:
                    self.setValue(int(event.getValue()))
                else:
                    self.setValue(None)

            elif event.hasId('active-profile-changed'):
                # Get the value for the setting as it has been defined
                # in the currently active profile.
                value = pr.getActive().getValue(self.widgetId, False)
                if value and value.getValue() != '':
                    self.setValue(int(value.getValue()))
                else:
                    self.setValue(None)
