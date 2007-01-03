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

## @file button.py Button Controls

import sys, wx
import host, events, language, paths
import sb.profdb as pr
import sb.confdb as st
from widgets import uniConv
import base


class Button (base.Widget):
    """The Button class implements a button that emits a Command event
    when pushed.  There are three styles available:
    - STYLE_NORMAL: A normal button.
    - STYLE_DEFAULT: A default button.
    - STYLE_MINI: A small button.
    """

    STYLE_NORMAL = 'normal'
    STYLE_DEFAULT = 'default'
    STYLE_MINI = 'mini'

    def __init__(self, parent, wxId, id, style):
        # Create a wxButton of the appropriate type.
        if style != Button.STYLE_MINI:
            widget = wx.Button(parent, wxId,
                               uniConv(language.translate(id)))
        else:
            widget = wx.ToggleButton(parent, wxId,
                                     uniConv(language.translate(id)))

        # Default buttons are usually a bit different visually.
        if style == Button.STYLE_DEFAULT:
            widget.SetDefault()

        base.Widget.__init__(self, widget)
        self.widgetId = id

        if style == Button.STYLE_MINI:
            # The mini buttons use the small style.
            self.setSmallStyle()
            # The size of the mini buttons is configurable.
            bestSize = widget.GetBestSize()
            width = 30
            height = bestSize[1]
            try:
                width = st.getSystemInteger('button-mini-width')
                if st.getSystemInteger('button-mini-height'):
                    height = st.getSystemInteger('button-mini-height')
            except:
                pass
            widget.SetMinSize((width, height))

        if style != Button.STYLE_MINI:
            # We will handle the click event ourselves.
            wx.EVT_BUTTON(parent, wxId, self.onClick)
            #self.updateDefaultSize()
        else:
            # Pop back up when toggled down.
            wx.EVT_TOGGLEBUTTON(parent, wxId, self.onToggle)

    def updateDefaultSize(self):
        w = self.getWxWidget()
        w.SetMinSize(w.GetBestSize())

    def retranslate(self):
        self.getWxWidget().SetLabel(uniConv(language.translate(self.widgetId)))

    def onClick(self, ev):
        """Handle a wxWidgets button click event."""
        events.send(events.Command(self.widgetId))

        # This causes a reaction.
        self.react()

    def onToggle(self, ev):
        """Handle a wxWidgets toggle button click event."""
        # Since this is supposed to be a normal button and not a
        # toggle button, return to the Off-state automatically.
        self.getWxWidget().SetValue(False)
        events.send(events.Command(self.widgetId))

        # This causes a reaction.
        self.react()


class CheckBox (base.Widget):
    """A check box widget."""

    def __init__(self, parent, wxId, label, isChecked):
        base.Widget.__init__(self, wx.CheckBox(parent, wxId,
                                               uniConv(language.translate(label)),
                                               style=wx.CHK_3STATE |
                                               wx.CHK_ALLOW_3RD_STATE_FOR_USER))
        self.widgetId = label

        # Set the initial value of the checkbox.
        w = self.getWxWidget()
        if isChecked:
            w.Set3StateValue(wx.CHK_CHECKED)
        else:
            w.Set3StateValue(wx.CHK_UNCHECKED)

        # We want focus notifications.
        self.setFocusId(self.widgetId)

        self.indicator = None

        # When the checkbox is focused, send a notification.
        wx.EVT_CHECKBOX(parent, wxId, self.onClick)
        
        # Listen for value changes.
        self.addValueChangeListener()
        self.addProfileChangeListener()

    def setDefaultIndicator(self, indicatorTextWidget):
        """Set the Text widget that displays when the check box is in
        the Default state.

        @param indicatorTextWidget  A Widgets.Text widget.
        """
        self.indicator = indicatorTextWidget
        self.indicator.getWxWidget().SetForegroundColour(
            wx.Colour(170, 170, 170))
        self.indicator.setSmallStyle()
        self.updateState()

        # Clicking on the indicator will move focus to the setting widget.
        self.indicator.setFocusId(self.widgetId)

    def updateState(self):
        """Update the state of the indicator to match one of the three
        states."""

        if self.indicator:
            w = self.getWxWidget()
            if w.Get3StateValue() == wx.CHK_UNDETERMINED:
                self.indicator.setText(
                    '(' + language.translate('toggle-use-default-value') + \
                    language.translate(
                    pr.getDefaults().getValue(self.widgetId).getValue()) + ')')
            else:
                self.indicator.setText('')

    def onClick(self, event):
        """Swap through the three states when the check box is clicked."""

        if self.widgetId:
            w = self.getWxWidget()
            state = w.Get3StateValue()

            if (state == wx.CHK_UNDETERMINED and 
                pr.getActive() is pr.getDefaults()):
                state = wx.CHK_UNCHECKED
                w.Set3StateValue(state)

            if state == wx.CHK_CHECKED:
                pr.getActive().setValue(self.widgetId, 'yes')
                newValue = 'yes'
                
            elif state == wx.CHK_UNCHECKED:
                pr.getActive().setValue(self.widgetId, 'no')
                newValue = 'no'

            else:
                pr.getActive().removeValue(self.widgetId)
                newValue = 'default'

            self.updateState()
            events.send(events.EditNotify(self.widgetId, newValue))
        
        # Let wxWidgets process the event, too.
        event.Skip()

    def getFromProfile(self, profile):
        # Get the value for the setting as it has been defined
        # in the currently active profile.
        value = profile.getValue(self.widgetId, False)
        w = self.getWxWidget()
        if value:
            if value.getValue() == 'yes':
                w.Set3StateValue(wx.CHK_CHECKED)
            else:
                w.Set3StateValue(wx.CHK_UNCHECKED)
        else:
            w.Set3StateValue(wx.CHK_UNDETERMINED)
        self.updateState()

    def onNotify(self, event):
        """Handle notifications.  When the active profile changes, the
        check box's state is updated."""
        
        base.Widget.onNotify(self, event)

        if self.widgetId:
            w = self.getWxWidget()
            if event.hasId('active-profile-changed'):
                self.getFromProfile(pr.getActive())

            elif event.hasId(self.widgetId + '-value-changed'):
                if event.getValue() == 'yes':
                    w.Set3StateValue(wx.CHK_CHECKED)
                elif event.getValue() == 'no':
                    w.Set3StateValue(wx.CHK_UNCHECKED)
                elif event.getValue() is None:
                    w.Set3StateValue(wx.CHK_UNDETERMINED)
                self.updateState()

    def retranslate(self):
        if self.widgetId:
            self.getWxWidget().SetLabel(
                uniConv(language.translate(self.widgetId)))


class RadioButton (base.Widget):
    """A radio button widget."""

    def __init__(self, parent, wxId, label, isToggled, isFirst=False):
        if isFirst:
            flags = wx.RB_GROUP
        else:
            flags = 0
        base.Widget.__init__(self, wx.RadioButton(parent, wxId, 
                                                  language.translate(label), 
                                                  style=flags))
        self.label = label
        
        if isToggled:
            self.getWxWidget().SetValue(True)
        
    def setText(self, text):
        self.getWxWidget().SetLabel(text)
        
    def isChecked(self):
        return self.getWxWidget().GetValue()
