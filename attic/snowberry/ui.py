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

## @file ui.py User Interface
##
## This module contains classes and functions for managing the
## application's user interface.  The basic concept is that the
## interface has been divided into a number of areas.  The top-level
## areas (title, profiles, command, help) are created when the main
## window is constructed, and cannot be changed at runtime.  The
## contents of each area can be modified at runtime.
##
## Many of the areas are managed by plugins.  This means the plugin
## creates all the widgets inside the area.
##
## Each area can be divided into further sub-areas, which are treated
## the same way as the top-level areas.  The Area class can represent
## either a top-level area or a sub-area.

import sys, os, wx, string
import host, events, widgets, language, paths
import sb.widget.tab
import sb.profdb as pr
import sb.confdb as st
import logger
from widgets import uniConv

# Default areas.
TITLE = 'main-title'
PROFILES = 'main-profiles'
COMMAND = 'main-command'
PREFCOMMAND = 'main-pref-command'
TABS = 'main-tabs'
HELP = 'main-help'

# Menus.
MENU_APP = 0
MENU_PROFILE = 1
MENU_TOOLS = 2
MENU_HELP = 3

# Menu groups.
MENU_GROUP_LAUNCH = 10
MENU_GROUP_ADDON = 20
MENU_GROUP_AODB = 30
MENU_GROUP_PROFILE = 40
MENU_GROUP_PROFDB = 50
MENU_GROUP_APP = 60

# Layout alignments.
ALIGN_HORIZONTAL = 0
ALIGN_VERTICAL = 1

# Border flags.
BORDER_TOP = 1
BORDER_RIGHT = 2
BORDER_BOTTOM = 4
BORDER_LEFT = 8
BORDER_TOP_BOTTOM = (1 | 4)
BORDER_LEFT_RIGHT = (2 | 8)
BORDER_NOT_TOP = (2 | 4 | 8)
BORDER_NOT_RIGHT = (1 | 4 | 8)
BORDER_NOT_BOTTOM = (1 | 2 | 8)
BORDER_NOT_LEFT = (1 | 2 | 4)
BORDER_ALL = (1 | 2 | 4 | 8)

# Optional areas.
USE_TITLE_AREA = not st.getSystemBoolean('main-hide-title')
USE_HELP_AREA = not st.getSystemBoolean('main-hide-help')
USE_MINIMAL_PROFILE = st.getSystemBoolean('profile-minimal-mode')

# A Windows kludge: background colour for tabs and the controls in them.
#tabBgColour = wx.Colour(250, 250, 250)
#st.getSystemInteger('tab-background-red'),
#                        st.getSystemInteger('tab-background-green'),
#                        st.getSystemInteger('tab-background-blue'))

# An array of UI areas.
uiAreas = {}
mainPanel = None

# Items for the popup menu. Up to seven menus.
menuItems = [[], [], [], [], [], [], []]


class Timer (wx.Timer):
    """A timer class."""

    def __init__(self, notifyId=''):
        """Initialize the timer."""
        wx.Timer.__init__(self)
        self.notifyId = notifyId

    def Notify(self):
        """Called when the timer expires."""
        self.expire()

    def start(self, milliSeconds):
        """Start the timer in one-shot mode.

        @param milliSeconds  Milliseconds after which the timer expires.
        """
        self.Start(milliSeconds, wx.TIMER_ONE_SHOT)

    def expire(self):
        """Send a notification if one has been defined. This method can be
        overridden by subclasses.
        """
        if self.notifyId:
            events.send(events.Notify(self.notifyId))


class MainPanel (wx.Panel):
    def __init__(self, parent):
        wx.Panel.__init__(self, parent, -1, style=wx.NO_BORDER |
                          wx.CLIP_CHILDREN)

        from sb.widget.area import Area

        aSizer = wx.BoxSizer(wx.HORIZONTAL)
        bSizer = wx.BoxSizer(wx.VERTICAL)
        cSizer = wx.BoxSizer(wx.HORIZONTAL)
        dSizer = wx.BoxSizer(wx.VERTICAL)
        eSizer = wx.BoxSizer(wx.HORIZONTAL)

        self.verticalSizer = bSizer

        # Title area.
        if USE_TITLE_AREA:
            titlePanel = wx.Panel(self, -1,
                                  style=wx.NO_BORDER | wx.CLIP_CHILDREN)
            _newArea( Area(TITLE, titlePanel, ALIGN_HORIZONTAL) )
            bSizer.Add(titlePanel, 0, wx.EXPAND)

        # Profile area.
        tabPanel = wx.Panel(self, -1,
                            style=wx.NO_BORDER | wx.CLIP_CHILDREN)
        tabsArea = Area(TABS, tabPanel, ALIGN_VERTICAL, 16)
        _newArea(tabsArea)

        dSizer.Add(tabPanel, 5, wx.EXPAND | wx.ALL, 3)

        # Command area.
        commandPanel = wx.Panel(self, -1, style=wx.NO_BORDER
                                 | wx.CLIP_CHILDREN)
        area = Area(COMMAND, commandPanel, ALIGN_HORIZONTAL, 18,
                    borderDirs=BORDER_LEFT | BORDER_RIGHT | BORDER_BOTTOM)
        # The command area is aligned to the right.
        area.addSpacer()
        area.setExpanding(False)
        area.setWeight(0)
        _newArea(area)

        # Preferences command area.
        prefCommandPanel = wx.Panel(self, -1, style=wx.NO_BORDER |
                                    wx.CLIP_CHILDREN)
        area = Area(PREFCOMMAND, prefCommandPanel,
                    ALIGN_HORIZONTAL, 18,
                    borderDirs=BORDER_LEFT | BORDER_RIGHT | BORDER_BOTTOM)
        area.setExpanding(False)
        area.setWeight(0)
        _newArea(area)

        eSizer.Add(prefCommandPanel, 0, wx.EXPAND)
        eSizer.Add(commandPanel, 1, wx.EXPAND)
        dSizer.Add(eSizer, 0, wx.EXPAND)
        cSizer.Add(dSizer, 5, wx.EXPAND) # notebook
        bSizer.Add(cSizer, 1, wx.EXPAND)
        aSizer.Add(bSizer, 7, wx.EXPAND)

        # Help area.
        self.SetSizer(aSizer)
        self.SetAutoLayout(True)

    def getTabs(self):
        """Returns the TabArea widget that contains the main tabbing
        area."""
        return self.tabs

    def updateLayout(self):
        self.verticalSizer.Layout()


INITIAL_SASH_POS = 180
INITIAL_PROFILE_SASH_POS = 190

if st.isDefined('main-split-position'):
    INITIAL_SASH_POS = st.getSystemInteger('main-split-position')

if st.isDefined('main-profile-split-position'):
    INITIAL_PROFILE_SASH_POS = st.getSystemInteger(
        'main-profile-split-position')


class MainFrame (wx.Frame):
    """The main frame is the main window of Snowberry."""

    def __init__(self, title):
        """Initialize the main window.

        @param title  Title for the main window.
        """
        from sb.widget.area import Area

        # Commands for the popup menu.
        self.menuCommandMap = {}

        # FIXME: Layout is botched. Way too wide.
        if host.isMac():
            initialSize = (769, 559)
        elif host.isUnix():
            initialSize = (900, 610)
        else:
            initialSize = (733, 527)

        # The configuration may define a window size.
        if st.isDefined('main-width'):
            initialSize = (st.getSystemInteger('main-width'), initialSize[1])
        if st.isDefined('main-height'):
            initialSize = (initialSize[0], st.getSystemInteger('main-height'))

        # The configuration can also specify the window position.
        if st.isDefined('main-x') and st.isDefined('main-y'):
            initialPos = (st.getSystemInteger('main-x'), st.getSystemInteger('main-y'))
        else:
            initialPos = None

        wx.Frame.__init__(self, None, -1, title, size=initialSize)
        #self.SetExtraStyle(wx.FRAME_EX_METAL)
        #self.Create(None, -1, title, size=initialSize)

        if initialPos is not None:
            self.MoveXY(*initialPos)
        else:
            self.Center()

        # Set the icon for the frame.
        icon = wx.Icon('graphics/snowberry.ico', wx.BITMAP_TYPE_ICO)
        self.SetIcon(icon)

        #self.Iconize(True)
        #self.Hide()

        SPLITTER_ID = 9501
        PROF_SPLITTER_ID = 9502
        self.splitter = None
        self.profSplitter = None

        # The parentWin is where the profSplitter and the help panel
        # are inside.
        parentWin = self

        if USE_HELP_AREA:
            # The help area is in a splitter.
            self.splitter = wx.SplitterWindow(self, SPLITTER_ID,
                                              style=wx.SP_3DSASH)
            self.splitter.SetMinimumPaneSize(10)
            parentWin = self.splitter

        if not USE_MINIMAL_PROFILE:
            self.profSplitter = wx.SplitterWindow(parentWin, PROF_SPLITTER_ID,
                                                  style=wx.SP_3DSASH)# |
                                                  #wx.SP_NO_XP_THEME)
            self.profSplitter.SetMinimumPaneSize(100)

            # Profile panel.
            profilePanel = wx.Panel(self.profSplitter, -1, style=wx.NO_BORDER
                                    | wx.CLIP_CHILDREN)
            area = Area(PROFILES, profilePanel, ALIGN_VERTICAL, 10)
            _newArea(area)

            # Create panels inside the profile splitter.
            self.mainPanel = MainPanel(self.profSplitter)
        else:
            profilePanel = None

            self.mainPanel = MainPanel(parentWin)

            getArea(TABS).setWeight(0)
            profArea = getArea(TABS).createArea(alignment=ALIGN_HORIZONTAL, border=12)
            profArea.setId(PROFILES)
            _newArea(profArea)
            getArea(TABS).setWeight(1)
            getArea(TABS).setBorderDirs(BORDER_NOT_TOP)

        # Create a TabArea into the TABS area.
        self.mainPanel.tabs = getArea(TABS).createTabArea(
            'tab', style=sb.widget.tab.TabArea.STYLE_BASIC)

        if st.isDefined('main-split-position'):
            self.splitPos = INITIAL_SASH_POS
        else:
            self.splitPos = None
        self.profSplitPos = INITIAL_PROFILE_SASH_POS

        # Create the help area.
        if self.splitter:
            self.helpPanel = wx.Panel(self.splitter, -1, style = wx.NO_BORDER
                                      | wx.CLIP_CHILDREN)
            self.helpPanel.SetBackgroundColour(wx.WHITE)
            _newArea( Area(HELP, self.helpPanel, ALIGN_VERTICAL,
                           border=4) )
            # Init the splitter.
            leftSide = self.profSplitter
            if not leftSide:
                leftSide = self.mainPanel
            self.splitter.SplitVertically(leftSide, self.helpPanel,
                                          -INITIAL_SASH_POS)
        else:
            self.helpPanel = None

        if self.profSplitter:
            self.profSplitter.SplitVertically(profilePanel, self.mainPanel,
                                              self.profSplitPos)

        # Listen for changes in the sash position.
        wx.EVT_SPLITTER_SASH_POS_CHANGED(self, SPLITTER_ID,
                                         self.onSplitChange)
        wx.EVT_SPLITTER_SASH_POS_CHANGED(self, PROF_SPLITTER_ID,
                                         self.onProfSplitChange)

        # The main panel should be globally accessible.
        global mainPanel
        mainPanel = self.mainPanel

        # Intercept the window close event.
        wx.EVT_CLOSE(self, self.onWindowClose)

        # Maintain the splitter position.
        wx.EVT_SIZE(self, self.onWindowSize)

        # Listen to some commands.
        events.addCommandListener(self.handleCommand, ['quit'])

        # Create a menu bar.
        self.menuBar = wx.MenuBar()

    def show(self):
        #if self.splitter:
        #    self.splitter.Thaw()
        #    self.splitter.Refresh()
        pass

    def updateLayout(self):
        """Update the layout of the widgets inside the main window."""

        # Also update all UI areas.
        for key in uiAreas:
            uiAreas[key].updateLayout()

        self.mainPanel.updateLayout()
        #self.Show()

        if not host.isMac():
            self.mainPanel.GetSizer().Fit(self.mainPanel)

        # The main panel's sizer does not account for the help panel.
        #if host.isUnix(): # or host.isWindows():
        #    windowSize = self.GetSizeTuple()
        #    self.SetSize((windowSize[0] + INITIAL_SASH_POS, windowSize[1]))

    def onWindowSize(self, ev):
        """Handle the wxWidgets event that is sent when the main window
        size changes.

        @param ev The wxWidgets event.
        """
        # Allow others to handle this event as well.
        ev.Skip()

        if self.splitter:
            if self.splitPos:
                pos = self.splitPos
            else:
                pos = INITIAL_SASH_POS

            self.splitter.SetSashPosition(ev.GetSize()[0] - pos)

    def onSplitChange(self, ev):
        """Update the splitter anchor position."""

        if self.splitter:
            self.splitPos = self.GetClientSizeTuple()[0] - ev.GetSashPosition()

    def onProfSplitChange(self, ev):
        """Update the profile splitter anchor position."""

        self.profSplitPos = ev.GetSashPosition()

    def onWindowClose(self, ev):
        """Handle the window close event that is sent when the user
        tries to close the main window.  Broadcasts a 'quit'
        events.Notify object so that all the listeners will know that
        the application is closing down.

        @param ev A wxCloseEvent object.
        """
        # Send a 'quit' command so that everyone has a chance to save
        # their current status.
        events.send(events.Notify('quit'))

        # Save window size to UserHome/conf/window.conf.
        winSize = self.GetSizeTuple()
        fileName = os.path.join(paths.getUserPath(paths.CONF), 'window.conf')
        try:
            f = file(fileName, 'w')
            f.write('# This file is generated automatically.\n')
            f.write('appearance main (\n')
            f.write('  width = %i\n  height = %i\n' % winSize)
            f.write('  x = %i\n  y = %i\n' % self.GetPositionTuple())
            if self.splitPos != None:
                f.write('  split-position = %i\n' % self.splitPos)
            f.write('  profile-split-position = %i\n' % self.profSplitPos)
            f.write(')\n')
        except:
            # Window size not saved.
            pass

        # Destroy the main frame.
        self.Destroy()
        return True

    def handleCommand(self, event):
        """This is called whenever someone broadcasts an
        events.Command object.  The main window is only interested in
        the 'quit' command, on the reception of which it closes
        itself.

        @param event An events.Command object.
        """
        if event.hasId('quit'):
            self.Close()

    def updateMenus(self):
        """Create the main frame menus based on menuItems array."""

        global menuItems
        self.menuCommandMap = {}

        # Array of menus. Number to wxMenu.
        self.menus = {}

        # Three levels of priority.
        for level in range(len(menuItems)):
            if len(menuItems[level]) == 0:
                # No menu for this.
                self.menus[level] = None
                continue

            self.menus[level] = wx.Menu()
            self.menuBar.Append(self.menus[level],
                                language.translate("menu-" + str(level)))

            # Sort the items based on groups, and append separators where necessary.
            menuItems[level].sort(lambda x, y: cmp(x[3], y[3]))
            separated = []
            previousItem = None
            for item in menuItems[level]:
                if previousItem and previousItem[3] != item[3]:
                    separated.append(('-', '', False, item[3]))
                separated.append(item)
                previousItem = item

            # Create the menu items.
            for itemId, itemCommand, itemSeparate, itemGroup in separated:
                if itemId == '-':
                    # This is just a separator.
                    self.menus[level].AppendSeparator()
                    continue

                if itemSeparate and self.menus[level].GetMenuItemCount() > 0:
                    self.menus[level].AppendSeparator()

                menuItemId = 'menu-' + itemId

                accel = ''
                if language.isDefined(menuItemId + '-accel'):
                    accel = "\t" + language.translate(menuItemId + '-accel')

                # Generate a new ID for the item.
                wxId = wx.NewId()
                self.menuCommandMap[wxId] = itemCommand
                self.menus[level].Append(wxId,
                                        uniConv(language.translate(menuItemId) + accel))
                wx.EVT_MENU(self, wxId, self.onPopupCommand)

                if host.isMac():
                    # Special menu items on Mac.
                    if itemId == 'about':
                        wx.App_SetMacAboutMenuItemId(wxId)
                    if itemId == 'quit':
                        wx.App_SetMacExitMenuItemId(wxId)
                    if itemId == 'show-snowberry-settings':
                        wx.App_SetMacPreferencesMenuItemId(wxId)

        if host.isMac():
            # Special Help menu on Mac.
            wx.App_SetMacHelpMenuTitleName(language.translate('menu-' + str(MENU_HELP)))

        self.SetMenuBar(self.menuBar)

    def onPopupCommand(self, ev):
        """Called when a selection is made in the popup menu."""
        events.send(events.Command(self.menuCommandMap[ev.GetId()]))

    def getMenuItem(self, identifier):
        """Finds a menu item.
        @param identifier  Identifier of the menu item.
        @return wxMenuItem, or None."""

        for item in self.menuCommandMap.keys():
            if self.menuCommandMap[item] == identifier:
                return self.menuBar.FindItemById(item)
        return None


class SnowberryApp (wx.App):
    def __init__(self):
        wx.App.__init__(self, 0)

    def OnInit(self):
        # Create and show the main window.
        self.mainFrame = MainFrame('Snowberry')

        self.SetTopWindow(self.mainFrame)
        return True

    def updateTitle(self):
        windowTitle = st.getSystemString('snowberry-title') + ' ' + \
                      st.getSystemString('snowberry-version')
        self.mainFrame.SetTitle(windowTitle)

    def showMainWindow(self):
        self.updateTitle()
        self.mainFrame.Thaw()
        self.mainFrame.Refresh()


def getMainPanel():
    """Returns the wxPanel of the main window."""
    return mainPanel


def addMenuCommand(level, identifier, label=None, pos=None, separate=False, group=''):
    """Insert a new popup menu item for the Menu button.

    @param level       Priority level (0, 1, 2).
    @param identifier  Identifier of the command.
    @param label       Label for the command. If not specified, the same as the identifier.
    @param pos         Position in the menu. None will cause the new item to be appended
                       to the end of the menu. The first item in the menu has position 0.
    @param separate    If True and there are existing items already, add a separator.
    @param group       If specified, items are sorted by this before adding to the menu.
                       The items without a group are inserted first. Groups are separated
                       with separators.
    """
    global menuItems

    if host.isMac() and level == 0:
        # On the Mac, the Snowberry menu is integrated into the app menu.
        # There is no need for a level zero menu.
        # Put the items temporarily on level 1, where they will be moved
        # by wxPython to the app menu.
        level = 1

    if label is None:
        item = (identifier, identifier, separate, group)
    else:
        item = (identifier, label, separate, group)

    if pos is None:
        menuItems[level].append(item)
    else:
        menuItems[level].insert(pos, item)


def isMenuEmpty(level):
    """Determines whether a certain menu is currently empty.
    @return True or False."""
    return len(menuItems[level]) == 0


def enableMenuCommand(identifier, enable=True):
    """Enables or disables a command in the menu.

    @param identifier  Identifier of the menu item.
    @param enable  True, if the item should be enabled. False, if disabled.
    """
    item = app.mainFrame.getMenuItem(identifier)
    if item:
        if enable:
            item.Enable(True)
        else:
            item.Enable(False)


def disableMenuCommand(identifier):
    enableMenuCommand(identifier, False)


def getArea(id):
    """Returns the UI area with the specified ID.

    @return An Area object.
    """
    return uiAreas[id]


def _newArea(area):
    """A private helper function for adding a new area to the list of
    identifiable areas.  Sub-areas are not identifiable.

    @param area An Area object.
    """
    global uiAreas
    uiAreas[area.getId()] = area


def selectTab(id):
    """Select a tab in the tab switcher.

    @param id Identifier of the tab.
    """
    mainPanel.getTabs().selectTab(id)


def createTab(id):
    """Create a new tab in the tab switcher.

    @param id The identifier of the new tab.  The label of the tab
    will be the translation of this identifier.

    @return An Area object that represents the new tab.
    """
    area = mainPanel.getTabs().addTab(id)
    area.setBorder(10)

    # Register this area so it can be found with getArea().
    _newArea(area)

    return area


def prepareWindows():
    """Layout windows and make everything ready for action."""

    events.send(events.Notify('preparing-windows'))

    app.mainFrame.updateMenus()
    app.mainFrame.updateLayout()
    app.mainFrame.Show()
    app.mainFrame.Freeze()


def startMainLoop():
    """Start the wxWidgets main loop."""

    # Before we give wxPython all control, update the layout of UI
    # areas.
    app.showMainWindow()

    # Display any issues that came up during init.
    logger.show()

    # Notify everyone that the main loop is about to kick in.
    events.send(events.Notify('init-done'))

    app.MainLoop()


def freeze():
    app.mainFrame.Freeze()


def unfreeze():
    app.mainFrame.Thaw()
    app.mainFrame.Refresh()


# Initialize the UI module by creating the main window of the application.
app = SnowberryApp()

# Construct an icon manager that'll take care of the icons used in the
# user interface.
widgets.initIconManager()
