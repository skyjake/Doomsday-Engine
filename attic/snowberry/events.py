# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#   Antti Kopponen <antti.kopponen@tut.fi>
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

## @file events.py Event Bus

# The event listeners. The dictionaries match event identifiers to lists
# of callbacks. The None identifier applies to all events sent.
commandListeners = {None:[]}
notifyListeners = {None:[]}

# Send counter.  Used to determine if a send() is currently underway.
sendDepth = 0

# Events waiting to be sent (FIFO queue).
queuedEvents = []

# While muted, the sending of events is prevented. All events 
# get discarded.
areEventsMuted = False


class Event:
    def __init__(self, id):
        self.id = id

    def getId(self):
        return self.id

    def hasId(self, id):
        """Test whether the event is of certain type.

        @param id The event identifier to test.

        @return True, if the event's identifier matches the specified
        id.  Otherwise false.
        """        
        return self.id == id
    
        
class Command (Event):
    def __init__(self, id):
        Event.__init__(self, id)
        self.myClass = Command


class Notify (Event):
    def __init__(self, id):
        Event.__init__(self, id)
        self.myClass = Notify


class PopulateNotify (Notify):
    """A PopulateNotify requests any listeners to populate an area
    with widgets."""

    def __init__(self, identifier, area):
        Notify.__init__(self, 'populating-area')
        self.areaId = identifier
        self.area = area

    def getArea(self):
        return self.area

    def getAreaId(self):
        return self.areaId


class SelectNotify (Notify):
    """SelectNotify should be sent when something gets selected.  This
    can be for example an item in a list, or an icon."""
    
    def __init__(self, id, newSelection, doubleClicked=False):
        """Construct a new selection notification.

        @param id Identifies where the selection has occured.  The
        suffix '-selected' will be automatically appended.

        @param newSelection Identifier of the item that is now selected.

        @param doubleClicked True, if the selection was done using a
        double click.
        """
        Notify.__init__(self, id + "-selected")
        self.selection = newSelection
        self.doubleClick = doubleClicked

    def getSelection(self):
        """Returns the identifier of the item that is now selected."""
        return self.selection

    def isDoubleClick(self):
        return self.doubleClick


class DeselectNotify (Notify):
    """DeselectNotify is sent when something is deselected."""

    def __init__(self, id, deselectedItemIdentifier):
        """Construct a new deselection notification.

        @param id  Identifies where the deselection has occured.

        @param deselectedItemIdentifer  Deselected item.
        """
        Notify.__init__(self, id + '-deselected')
        self.deselection = deselectedItemIdentifier

    def getDeselection(self):
        return self.deselection


class FocusNotify (Notify):
    """FocusNotify should be sent when something gains the input
    focus.  Not all widgets send this, though."""

    def __init__(self, id):
        """Construct a new focus change event.
        @param id Identifier of the object that now has the focus.
        """
        Notify.__init__(self, "focus-changed")
        self.focus = id

    def getFocus(self):
        """Return the identifier of whatever now has the focus."""
        return self.focus


class FocusRequestNotify (FocusNotify):
    def __init__(self, id):
        FocusNotify.__init__(self, id)
        self.id = 'focus-request'


class ValueNotify (Notify):
    """ValueNotify is sent by a profile when a profiles.Value in it
    changes its value."""

    def __init__(self, settingId, newValue, profile):
        Notify.__init__(self, "value-changed")
        self.settingId = settingId
        self.newValue = newValue
        self.profile = profile

    def getSetting(self):
        return self.settingId

    def getValue(self):
        return self.newValue

    def getProfile(self):
        return self.profile
        
    def getTargetedEvent(self):
        """Make a new event that is targetable to a specific handler."""
        
        ev = ValueNotify(self.settingId, self.newValue, self.profile)
        ev.id = self.settingId + '-value-changed'
        return ev
    

class EditNotify (Notify):
    """EditNotify must be sent when an editable widget changes its
    value.  The owner of the widget can then react to the change."""

    def __init__(self, id, newValue):
        Notify.__init__(self, "widget-edited")
        self.widget = id
        self.newValue = newValue

    def getWidget(self):
        return self.widget

    def getValue(self):
        return self.newValue


class AttachNotify (Notify):
    """An AttachNotify is sent when an addon is attached or detached
    from a profile."""

    def __init__(self, addonIdentifier, profile, isAttached=True):
        if isAttached:
            eventId = 'addon-attached'
        else:
            eventId = 'addon-detached'
        Notify.__init__(self, eventId)
        self.addon = addonIdentifier
        self.profile = profile

    def getAddon(self):
        return self.addon

    def getProfile(self):
        return self.profile

    
class ProfileNotify (Notify):
    """ProfileNotify is sent when a profile's information has changed
    or a new profile has been loaded."""
    
    def __init__(self, profile):
        Notify.__init__(self, 'profile-updated')
        self.profile = profile
    
    def getProfile(self):
        return self.profile


class ActiveProfileNotify (Notify):
    """ActiveProfileNotify is sent when the active profile is changed,
    or when the active profile is refreshed."""
    
    def __init__(self, profile, wasChanged=True):
        Notify.__init__(self, 'active-profile-changed')
        self.profile = profile
        self.changed = wasChanged
        
    def getProfile(self):
        return self.profile
        
    def wasChanged(self):
        return self.changed


class LaunchNotify (Notify):
    """LaunchNotify is sent when the command line options for
    launching a game has been compiled."""

    def __init__(self, commandLineOptions):
        Notify.__init__(self, 'launching')
        self.options = commandLineOptions

    def getOptions(self):
        """This is the actual list of parameters that will be used for
        launching the game.  Notification listeners are permitted to
        edit the options, but it should be noted that there are no
        guarantees as to which listener gets to handle the options
        first.

        @return An array of command line parameter strings.
        """
        return self.options


class LanguageNotify (Notify):
    """A LanguageNotify is sent when the user interface language is
    changed."""

    def __init__(self, newLanguage):
        Notify.__init__(self, 'language-changed')
        # We'll assume the language identifier begins with 'language-'.
        self.lang = newLanguage[9:]

    def getLanguage(self):
        return self.lang


#class AddonNotify (Notify):
#    """AddonNotify is sent when an addon is installed or uninstalled.
#    Plugins should refresh their widgets to reflect the new
#    configuration."""##
#
#    def __init__(self, 


def addListener(listeners, eventIds, callback):
    if eventIds == None:
        # General purpose callback.
        listeners[None].append(callback)
        #print "General purpose callback " + str(callback)
        
    # No duplicates allowed.
    elif callback not in listeners[None]:
        for id in eventIds:
            if not listeners.has_key(id):
                listeners[id] = [callback]
            else:
                if callback not in listeners[id]:
                    listeners[id].append(callback)
                #else:
                #    print "Duplicate addition of listener " + str(callback) + " into " + id


def addCommandListener(callback, eventIds = None):
    """Register a new Command listener callback function.  When a
    Command event is sent, the callback will get called.

    @param callback Callback function for Commands.
    @param eventIds List of event identifiers.
    """
    addListener(commandListeners, eventIds, callback)


def addNotifyListener(callback, eventIds = None):
    """Register a new Notify listener callback function.  When a
    Notify event is sent, the callback will get called.

    @param callback Callback function for Notifys.
    @param eventIds List of event identifiers.
    """
    addListener(notifyListeners, eventIds, callback)


def removeCommandListener(callback):
    """Remove an existing Command listener callback function.

    @param callback Callback function for Commands.
    """
    for key in commandListeners.keys():
        try:
            commandListeners[key].remove(callback)
        except:
            # Not necessarily in this list.
            pass
        
        
def removeNotifyListener(callback):
    """Remove an existing Notify listener callback function.

    @param callback Callback function for Notifys.
    """
    for key in notifyListeners.keys():
        try:
            notifyListeners[key].remove(callback)
        except:
            # Not necessarily in this list.
            pass


def mute():
    """Mutes the sending of events. While muted, all events are discarded."""
    global areEventsMuted
    areEventsMuted = True


def unmute():
    """Unmutes the sending of events. While muted, all events are discarded."""
    global areEventsMuted
    areEventsMuted = False
    
    
def isMuted():
    return areEventsMuted

        
def send(event):
    """Broadcast an event to all listeners of the appropriate type.
    The event is processed synchronously: all listeners will be
    notified immediately during the execution of this function.

    @param event The event to send.  All listeners will get the same
    event object.
    """
    if areEventsMuted:
        return # Discard.

    global sendDepth, queuedEvents

    sendDepth += 1
    
    # Commands and Notifys go to a different set of listeners.
    if event.myClass == Command:
        listeners = commandListeners
    else:
        listeners = notifyListeners

    # The identifier of the event to be sent.
    sendId = event.getId()
    
    # Always include the unfiltered callbacks.
    callbacks = [c for c in listeners[None]]
    
    if listeners.has_key(sendId):
        # The specialized callbacks.
        callbacks += listeners[sendId]

    #print "Sending " + sendId + ":"
    #print callbacks

    # Send the event to all the appropriate listeners.
    for callback in callbacks:
        try:
            callback(event)
        except Exception, x:
			# Report errors.
            import logger
            logger.add(logger.HIGH, 'error-runtime-exception-during-event',
					   sendId, str(x), logger.formatTraceback())
            logger.show()

    # If event processing is complete but events are queued, send them
    # now.
    if sendDepth == 1:
        while len(queuedEvents) > 0:
            # Listeners may call sendAfter() during this while loop.
            roster = queuedEvents
            queuedEvents = []
            for ev in roster:
                send(ev)

    sendDepth -= 1
    
    # If a value-changed event is sent, also send the specially targeted
    # event meant to be handled by the widget who handles the value.
    if event.hasId('value-changed'):
        # Place it in the beginning of the queue.
        send(event.getTargetedEvent())


def sendAfter(event):
    """Broadcast an event to all listeners of the appropriate type.
    If this is called during the execution of send(), the event will
    be delayed until send() is finished.

    @param event The event to send.
    """
    if areEventsMuted:
        return # Discard.
    
    global sendDepth, queuedEvents

    if sendDepth > 0:
        # A send() is currently underway.  Queue the event.
        queuedEvents.append(event)
    else:
        # We can send immediately.
        send(event)
