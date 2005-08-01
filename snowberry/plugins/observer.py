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

## @file observer.py Event Observer Debugging Plugin
##
## This module prints all the notify and command events to stdout.

import events


def init():
    events.addNotifyListener(handleNotify)
    events.addCommandListener(handleCommand)


def handleNotify(event):
    print
    print "Notify: " + event.getId()

    if 'selection' in dir(event):
        print "  Selection = " + str(event.getSelection())

    if 'deselection' in dir(event):
        print "  Deselection = " + str(event.getDeselection())

    if event.hasId('focus-changed'):
        print "  New focus on " + event.getFocus()

    if event.hasId('value-changed'):
        print "  New value for " + event.getSetting() + " is " + \
              str(event.getValue())

    if event.hasId('profile-updated'):
        print "  Profile =", event.getProfile().getId()
        print "  Hidden =", event.getProfile().isHidden()

    if event.hasId('widget-edited'):
        print "  New value for " + event.getWidget() + " is " + \
              str(event.getValue())

    if event.hasId('populating-area'):
        print "  Area id =", event.getAreaId()


def handleCommand(event):
    print
    print "Command: " + event.getId()


