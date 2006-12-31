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

## @file bar.py Bar Widgets

import sys, wx
import host, events, language, paths
import sb.profdb as pr
import base


class Slider (base.Widget):
    def __init__(self, parent, wxId, id):
        """A slider widget for constrained selection of values."""
        base.Widget.__init__(self, wx.Slider(parent, wxId, 50, 0, 100,
                                             style=(wx.SL_AUTOTICKS |
                                                    wx.SL_LABELS |
                                                    wx.SL_HORIZONTAL)))

        self.widgetId = id
        self.step = 1

        # Listen for changes.  Some platform-specific tweaks are needed:
        if host.isUnix():
            wx.EVT_COMMAND_SCROLL_THUMBTRACK(parent, wxId, self.onThumbTrack)
        else:
            wx.EVT_COMMAND_SCROLL_THUMBRELEASE(parent, wxId, self.onThumbTrack)

        # We want focus notifications.
        self.setFocusId(id)

        # Add a command for reseting the slider.
        self.resetCommand = self.widgetId + '-reset-to-default'
        self.setPopupMenu([('reset-slider', self.resetCommand)])
        
        # Listen to our reset command.
        events.addCommandListener(self.onCommand, [self.resetCommand])
        self.addValueChangeListener()
        self.addProfileChangeListener()

        self.oldValue = None

    def setValue(self, value):
        self.getWxWidget().SetValue(value)

    def getValue(self):
        self.snap()
        return self.getWxWidget().GetValue()

    def snap(self):
        w = self.getWxWidget()
        value = w.GetValue()
        w.SetValue(self.step * ((value + self.step/2) / self.step))

    def setRange(self, minimum, maximum, step):
        self.getWxWidget().SetRange(minimum, maximum)

        self.step = step
        if self.step < 1:
            self.step = 1

        self.getWxWidget().SetTickFreq(self.step, 1)
        self.getWxWidget().SetLineSize(self.step)
        self.getWxWidget().SetPageSize(self.step * 2)

    def onThumbTrack(self, ev):
        if self.widgetId:
            newValue = self.getValue()

            if newValue != self.oldValue:
                pr.getActive().setValue(self.widgetId, str(newValue))

                # Send a notification.
                events.send(events.EditNotify(self.widgetId, newValue))
                self.oldValue = newValue

    def __getValueFromProfile(self):
        # Get the value for the setting as it has been defined
        # in the currently active profile.
        value = pr.getActive().getValue(self.widgetId)
        if value:
            self.setValue(int(value.getValue()))

    def onNotify(self, event):
        base.Widget.onNotify(self, event)

        if self.widgetId:
            if (event.hasId('active-profile-changed') or
                event.hasId(self.widgetId + '-value-changed')):
                self.__getValueFromProfile()

    def onCommand(self, event):
        """Handle the slider reset command."""
        
        base.Widget.onCommand(self, event)

        if self.widgetId and event.hasId(self.resetCommand):
            pr.getActive().removeValue(self.widgetId)
