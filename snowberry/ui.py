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
import wx.wizard as wiz
import host, events, widgets, language, paths
import sb.widget.list
import sb.widget.tab
import sb.widget.tree
import profiles as pr
import settings as st
import logger
from widgets import uniConv


SETTING_WEIGHT_LEFT = 1
SETTING_WEIGHT_RIGHT = 2

# Border widths in areas.
# TODO: Move these to a .conf.
if host.isWindows():
    AREA_BORDER = 2
    AREA_BORDER_BOXED = 2
elif host.isUnix():
    AREA_BORDER = 2
    AREA_BORDER_BOXED = 1
else:
    AREA_BORDER = 3
    AREA_BORDER_BOXED = 2

# Optional areas.
USE_TITLE_AREA = not st.getSystemBoolean('main-hide-title')
USE_HELP_AREA = not st.getSystemBoolean('main-hide-help')

# An array of UI areas.
uiAreas = {}
mainPanel = None

# Items for the popup menu. Three separate levels of priority.
popupMenuItems = [[], [], []]


def addPopupMenuCommand(level, identifier, label=None):
    """Insert a new popup menu item for the Menu button.

    @param level       Priority level (0, 1, 2).
    @param identifier  Identifier of the command.
    @param label       Label for the command. If not specified, the same
                       as the identifier.
    """
    global popupMenuItems

    if(label == None):
        popupMenuItems[level].append(identifier)
    else:
        popupMenuItems[level].append((identifier, label))


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


def chooseFile(prompt, default, mustExist, fileTypes, defExt=None):
    """Show a file chooser dialog.

    @param prompt Identifier of the dialog title string.

    @param default The default selection.

    @param mustExist The selected file must exist.

    @param fileTypes An array of tuples (name, extension).

    @return The full pathname of the selected file.
    """
    # Compile the string of file types.
    types = []
    for ident, ext in fileTypes:
        types.append(language.translate(ident))
        types.append('*.' + ext.lower() + ';*.' + ext.upper())

    # Always append the "all files" filter.
    types.append(language.translate('file-type-anything'))
    types.append('*.*')

    if not defExt:
        if len(fileTypes) > 0:
            # By default use the first file type.
            defExt = fileTypes[0][1]
        else:
            defExt = ''

    if mustExist:
        flags = wx.FILE_MUST_EXIST
    else:
        flags = 0

    selection = wx.FileSelector(language.translate(prompt),
                                os.path.dirname(default),
                                os.path.basename(default),
                                defExt,
                                string.join(types, '|'),
                                flags)
    return selection


def chooseFolder(prompt, default):
    """Show a directory chooser dialog.

    @param prompt Identifier of the dialog title string.

    @param default The default path.

    @return The selected path.
    """
    return wx.DirSelector(language.translate(prompt), default)


def createDialog(id, alignment=1, size=None):
    """Create an empty modal dialog box.

    @param id Identifier of the dialog area.  The title of the dialog
    is the translation of this identifier.

    @return A widgets.DialogArea object.
    """
    dialog = AreaDialog(mainPanel, -1, id, size, alignment)
    dialog.center()
    return dialog


def createButtonDialog(id, titleText, buttons, defaultButton=None, size=None):
    """Returns the area where the caller may add the contents of the
    dialog.

    @param id Identifier of the dialog.
    @param buttons An array of button commands.
    @param defaultButton The identifier of the default button.

    @return Tuple (dialog, user-area)
    """
    dialog = createDialog(id, Area.ALIGN_HORIZONTAL, size)
    area = dialog.getArea()

    area.setMinSize(400, 20)

    # The Snowberry logo is on the left.
    #area.setWeight(0)
    #imageArea = area.createArea(alignment=Area.ALIGN_VERTICAL, border=0,
    #                            backgroundColor=wx.Colour(255, 255, 255))
    #imageArea.setWeight(1)
    #imageArea.setExpanding(True)
    #imageArea.addSpacer()
    #imageArea.setWeight(0)
    #imageArea.createImage('snowberry')

    area.setWeight(1)
    area.setBorderDirs(Area.BORDER_NOT_BOTTOM)
    contentArea = area.createArea(alignment=Area.ALIGN_VERTICAL,
                                  border=6)
    contentArea.setWeight(0)
    #if titleText != None:
    #    contentArea.setBorder(0)
    #    titleArea = contentArea.createArea(border=6,
    #        backgroundColor=wx.Colour(255, 255, 255))
    #    contentArea.setBorder(6)
    #    title = titleArea.createText('')
    #    title.setTitleStyle()
    #    title.setText(titleText)
    #    title.setMinSize(400, 20)

    #if host.isMac():
    # Generous borders on the Mac.
    contentArea.setBorder(16)

    contentArea.setWeight(1)
    userArea = contentArea.createArea(border=6)

    # Create the buttons.
    contentArea.setWeight(0)
    contentArea.setBorderDirs(Area.BORDER_NOT_TOP)
    buttonArea = contentArea.createArea(alignment=Area.ALIGN_HORIZONTAL,
                                        border=0)

    # If no explicit spacing is defined, use the default right
    # alignment.
    if '' not in buttons:
        # The implied spacer.
        buttonArea.addSpacer()

    buttonArea.setBorder(6, Area.BORDER_LEFT_RIGHT)
    buttonArea.setWeight(0)

    if not host.isMac():
        # Follow the general guidelines of the platform.
        if '' in buttons:
            # Only reverse the portion after the ''.
            index = buttons.index('') + 1
            sub = buttons[index:]
            sub.reverse()
            buttons = buttons[:index] + sub
        else:
            buttons.reverse()

    for button in buttons:
        # If an empty identifier is given, insert a space here.
        if button == '':
            buttonArea.setWeight(1)
            buttonArea.addSpacer()
            buttonArea.setWeight(0)
            continue

        if button == defaultButton:
            style = widgets.Button.STYLE_DEFAULT
        else:
            style = widgets.Button.STYLE_NORMAL

        widget = buttonArea.createButton(button, style=style)
        dialog.identifyWidget(button, widget)

    dialog.center()
    return dialog, userArea


class Area (widgets.Widget):
    """Each Area instance governs the contents of a wxPanel widget.
    Together with the classes in widgets.py, the Area class
    encapsulates all management of the user interface.  Modules that
    use ui.py or widgets.py need not access wxWidgets directly at all.

    The Area class implements the widgets.Widget interface from
    widgets.py.  getWxWidget() and clear() are the important
    Widget-inherited methods that Area uses.

    Area alignment constants:
    - ALIGN_HORIZONTAL: Widgets are placed next to each other
      inside the area.
    - ALIGN_VERTICAL: New widgets are placed below the existing
      widgets.
    """

    # Default areas.
    TITLE = 'main-title'
    PROFILES = 'main-profiles'
    COMMAND = 'main-command'
    PREFCOMMAND = 'main-pref-command'
    TABS = 'main-tabs'
    HELP = 'main-help'

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
    BORDER_NOT_BOTTOM = (1 | 2 | 8)
    BORDER_ALL = (1 | 2 | 4 | 8)

    # wxWidget ID counters.
    WIDGET_ID = 100

    def __init__(self, id, panel, alignment=0, border=0, parentArea=None,
                 borderDirs=15):
        """The constructor initializes the Area instance so that it
        can be used immediately afterwards.

        @param panel The wxWidget panel inside which the new Area will
        be.

        @param alignment Alignment for widgets inside the area.  This
        can be either ALIGN_VERTICAL or ALIGN_HORIZONTAL.

        @param border Width of the border around the new Area.

        @param parentArea Area instance that will act as the parent of
        the new area.  If this is None, it is assumed that the new
        Area is directly inside the main panel of the dialog.
        """
        self.id = id
        self.panel = panel
        self.border = border
        self.borderDirs = borderDirs

        # Widget layout parameters.
        # Weight for sizer when adding widgets.
        self.weight = 1
        self.expanding = True

        # Create a Sizer that maintains the proper layout of widgets
        # inside the Area.
        if alignment == Area.ALIGN_HORIZONTAL:
            self.sizer = wx.BoxSizer(wx.HORIZONTAL)
        else:
            self.sizer = wx.BoxSizer(wx.VERTICAL)

        # Top-level areas have a wx.Panel, with which the sizer needs
        # to be associated.  Subareas use the parent's area.
        if not parentArea:
            self.panel.SetSizer(self.sizer)
            self.parentArea = None
        else:
            self.parentArea = parentArea

        # The contents of the area will be added inside this sizer.
        self.containerSizer = self.sizer

        # The arrays that contain information about the contents of
        # the Area.
        self.widgets = []

    def getId(self):
        return self.id

    def getPanel(self):
        return self.panel

    def getWxWidget(self):
        """An Area instance can be used like a Widget."""
        return self.sizer

    def __getNewId(self):
        """Generate a new widget ID number.  These are given to
        wxWidgets so the events they send can be handled.
        """
        id = Area.WIDGET_ID
        Area.WIDGET_ID += 1
        return id

    def setWeight(self, weight):
        """Set the sizer weight that will be used for the next widget
        that is created."""
        self.weight = weight

    def setBorder(self, border, dirs=None):
        """Set the width of empty space around widgets.

        @param border Number of pixels for new borders.
        """
        self.border = border
        if dirs is not None:
            self.borderDirs = dirs

    def setBorderDirs(self, dirs):
        self.borderDirs = dirs

    def setBackgroundColor(self, red, green, blue):
        self.panel.SetBackgroundColour(wx.Colour(red, green, blue))
        self.panel.SetBackgroundStyle(wx.BG_STYLE_COLOUR)

    def setExpanding(self, expanding):
        self.expanding = expanding

    def updateLayout(self, redoLayout=True):
        """Recalculate the layout of all the widgets inside the area.
        This must be called when widgets are created or destroyed at
        runtime.  The layout of all areas is updated automatically
        right before the main window is displayed during startup.  If
        a plugin modifies the layout of its widgets during event
        handling, it must call updateLayout() manually.
        """
        # Tell the main sizer of our panel's current minimum size.
        m = self.sizer.GetMinSize()
        topSizer = self.panel.GetContainingSizer()
        if topSizer:
            topSizer.SetItemMinSize(self.panel, m)
        # Redo the layout.
        if redoLayout:
            self.sizer.Layout()

    def addSpacer(self):
        """Adds empty space into the area.  The size of the empty
        space is determined by the current weight.
        """
        ## @todo There must be a better way to do this.
        self.createArea('')

    def __addWidget(self, widget):
        """Insert a new widget into the area.
        @param wxWidget Widget object to add to the area.
        """
        # The Area keeps track of its contents.
        self.widgets.append(widget)

        # Use the low-level helper to do the adding.
        self._addWxWidget(widget.getWxWidget())

    def _getLayoutFlags(self):
        """Determines the appropriate layout flags for child widgets."""
        flags = 0
        if self.expanding: flags |= wx.EXPAND
        if self.border > 0:
            if self.borderDirs & 0x1:
                flags |= wx.TOP
            if self.borderDirs & 0x2:
                flags |= wx.RIGHT
            if self.borderDirs & 0x4:
                flags |= wx.BOTTOM
            if self.borderDirs & 0x8:
                flags |= wx.LEFT
        flags |= wx.ALIGN_CENTER_VERTICAL
        flags |= wx.ALIGN_CENTER
        return flags

    def _addWxWidget(self, wxWidget):
        """Insert a wxWidget into the area's sizer.  This protected
        low-level helper method takes care of selecting the correct
        layout parameters for the widget.

        @param wxWidget A wxWidget object.
        """
        # Insert the widget into the Sizer with appropriate layout
        # parameters.
        self.containerSizer.Add(wxWidget, self.weight,
                                self._getLayoutFlags(), self.border)

    def destroyWidget(self, widget):
        """Destroy a particular widget inside the area.

        @param widget The widget to destroy.
        """
        if widget in self.widgets:
            widget.clear()
            widget.destroy()
            self.widgets.remove(widget)

    def destroy(self):
        """Destroy the area and its contents."""

        self.clear()

        if not self.parentArea:
            # Standalone areas are never added to a sizer.
            return

        # Detach and destroy the sizer of this area.
        self.parentArea.getWxWidget().Detach(self.getWxWidget())
        self.getWxWidget().Destroy()

    def clear(self):
        """Remove all the widgets inside the area."""

        # Remove each widget individually.
        for widget in self.widgets:

            # Tell the widget it is about to the removed.  Subareas
            # will process their contents recursively.
            widget.clear()

            # The widget must be explicitly destroyed.
            widget.destroy()

        # No widgets remain.
        self.widgets = []

    def enable(self, doEnable):
        """Enabling an area means enabling all the widgets in it."""
        for widget in self.widgets:
            widget.enable(doEnable)

    def createChildPanel(self):
        """Create a new wxPanel inside this area's panel.  The new
        panel can be given to a subarea, whose background color can
        then be set."""

        return wx.Panel(self.panel, -1)

    def createArea(self, alignment=1, border=0, boxedWithTitle=None,
                   backgroundColor=None):
        """Create a sub-area that is a part of the parent area and can
        contain widgets.

        @param alignment  Widget alignment inside the sub-area.  Can be
        either ALIGN_VERTICAL or ALIGN_HORIZONTAL.

        @param border  Border width for the sub-area (pixels).

        @param boxedWithTitle  If you want to enclose the area inside a
        box frame, give the identifier of the box title here.  The
        actual label text is the translation of the identifier.

        @param backgroundColor  If specified, the new area will have
        its own wxPanel with the color as a background color.

        @return An Area object that represents the contents of the
        area.
        """
        # Subareas don't have identifiers.
        if boxedWithTitle:
            subArea = BoxedArea(boxedWithTitle, self, self.panel,
                                alignment, border)
        elif backgroundColor:
            subArea = ColoredArea(backgroundColor, self, self.panel,
                                  alignment, border)
        else:
            subArea = Area('', self.panel, alignment, border, self)

        self.__addWidget(subArea)
        return subArea

    def createMultiArea(self):
        multi = MultiArea('', self)
        self.__addWidget(multi)
        return multi

    def createButton(self, name, style=widgets.Button.STYLE_NORMAL):
        """Create a button widget inside the area.

        @param name Identifier of the button.  The text label that
        appears in the button will be the translation of this
        identifier.

        @return A widgets.Button object.
        """
        w = widgets.Button(self.panel, self.__getNewId(), name, style)
        self.__addWidget(w)
        return w

    def createLine(self):
        """Create a static horizontal divider line."""

        widget = widgets.Line(self.panel)
        self.__addWidget(widget)
        return widget

    def createImage(self, imageFile):
        """Create an image widget inside the area.

        @param imageFile Base name of the image file.
        """
        widget = widgets.Image(self.panel, imageFile)
        self.__addWidget(widget)
        return widget

    def createText(self, name='', suffix='', maxLineLength=0,
                   align=widgets.Text.LEFT):
        """Create a text label widget inside the area.

        @param name Identifier of the text label widget.  The actual
        text that appears in the widget is the translation of this
        identifier.

        @return A widgets.Text object.
        """
        widget = widgets.Text(self.panel, -1, name, suffix, maxLineLength,
                              align)
        self.__addWidget(widget)
        return widget

    def createTextField(self, name=''):
        """Create a text field widget inside the area.

        @param name Identifier of the text field widget.

        @return A widgets.TextField object.
        """
        widget = widgets.TextField(self.panel, -1, name)
        self.__addWidget(widget)
        return widget

    def createFormattedText(self):
        """Create a formatted text label widget inside the area.
        Initially the widget contains no text.

        @return A widgets.FormattedText object.
        """
        widget = widgets.FormattedText(self.panel, -1)
        self.__addWidget(widget)
        return widget

    def createRichText(self, text):
        """Create a static rich text label widget inside the area.
        Initially the widget contains no text.

        @return A widgets.FormattedText object.
        """
        widget = widgets.FormattedText(self.panel, -1, False, text)
        self.__addWidget(widget)
        return widget

    def createList(self, name, style=sb.widget.list.List.STYLE_SINGLE):
        """Create a list widget inside the area.  Initially the list
        is empty.

        @param name Identifier of the list widget.  Events sent by the
        widget are formed based on this identifier.  For example,
        selection of an item causes the notification
        '(name)-selected'.

        @return A sb.widget.list.List object.
        """
        widget = sb.widget.list.List(self.panel, self.__getNewId(), name, style)
        self.__addWidget(widget)
        return widget

    def createFormattedList(self, name):
        """Create a list widget that displays HTML-formatted items.
        Initially the list is empty.

        @param name Identifier of the list widget.  This is used when
        the widget sends notifications.

        @param A sb.widget.list.FormattedList widget.
        """
        widget = sb.widget.list.FormattedList(self.panel, self.__getNewId(), name)
        self.__addWidget(widget)
        return widget

    def createDropList(self, name):
        """Create a drop-down list widget inside the area.  Initially
        the list is empty.

        @param name Identifier of the drop-down list widget.  Events
        sent by the widget are formed based on this identifier.
        For example, selection of an item causes the notification
        '(name)-selected'.

        @return A sb.widget.list.DropList object.
        """
        widget = sb.widget.list.DropList(self.panel, self.__getNewId(), name)
        self.__addWidget(widget)
        return widget

    def createTree(self):
        """Create a tree widget inside the area.  Initially the tree
        is empty.  The Tree class provides methods for
        populating the tree with items.

        @return A widgets.Tree object.

        @see widgets.Tree
        """
        widget = sb.widget.tree.Tree(self.panel, self.__getNewId())
        self.__addWidget(widget)
        return widget

    def createTabArea(self, name, style=sb.widget.tab.TabArea.STYLE_ICON):
        """Create a TabArea widget inside the area.  The widget is
        composed of an icon list and a notebook-like tabbing area.

        @param name The identifier of the new TabArea widget.  This
        will be used as the base name of events sent by the widget.

        @return A widgets.TabArea widget.
        """
        if style == sb.widget.tab.TabArea.STYLE_FORMATTED:
            # Create a subarea and a separate MultiArea in there.
            sub = self.createArea(alignment=Area.ALIGN_HORIZONTAL,
                                  border=0)

            # Create the formatted list (with extended functionality).
            widget = sb.widget.tab.FormattedTabArea(self.panel, self.__getNewId(),
                                                    name)
            sub.setWeight(5)
            sub.setBorder(8, Area.BORDER_RIGHT)
            sub.__addWidget(widget)
            sub.setBorder(0)

            # Create the multiarea and pair it up.
            sub.setWeight(14)
            multi = sub.createMultiArea()
            widget.setMultiArea(multi)
        else:
            widget = sb.widget.tab.TabArea(self.panel, self.__getNewId(),
                                           name, style)
            self.__addWidget(widget)
        return widget

    def createCheckBox(self, name, isChecked):
        """Create a check box widget inside the area.

        @param name Identifier of the widget.  The text label of the
        widget is the translation of this identifier.

        @param isChecked The initial state of the widget.

        @return A widgets.CheckBox obejct.
        """
        widget = widgets.CheckBox(self.panel, self.__getNewId(),
                                  name, isChecked)
        self.__addWidget(widget)
        return widget

    def createRadioButton(self, name, isChecked, isFirst=False):
        widget = widgets.RadioButton(self.panel, self.__getNewId(), name,
                                     isChecked, isFirst)
        self.__addWidget(widget)
        return widget

    def createNumberField(self, name):
        """Create an integer editing widget inside the area.
        Initially the editing field is empty.

        @param name Identifier of the widget.  The text label of the
        widget is the translation of this identifier.  Notification
        events sent by the widget are based on this identifier.

        @return A widgets.NumberField object.
        """
        widget = widgets.NumberField(self.panel, self.__getNewId(), name)
        self.__addWidget(widget)
        return widget

    def createSlider(self, name):
        widget = widgets.Slider(self.panel, self.__getNewId(), name)
        self.__addWidget(widget)
        return widget

    def createSetting(self, setting):
        self.setBorder(AREA_BORDER)
        return self.doCreateSetting(setting)

    def doCreateSetting(self, setting,
                        leftWeight=SETTING_WEIGHT_LEFT,
                        rightWeight=SETTING_WEIGHT_RIGHT):
        """Create one or more widgets that can be used to edit the
        value of the specified setting.  Automatically creates a
        subarea that contains all the widgets of the setting.

        @param setting A Setting object.

        @return The area which contains all the widgets of the
        setting.
        """
        if setting.getType() == 'implicit':
            # No widgets for implicit settings.
            return

        # Create a subarea for all the widgets of the setting.
        area = self.createArea(alignment = Area.ALIGN_HORIZONTAL)
        area.setExpanding(False)

        # All settings have a similarly positioned label on the left
        # side.
        area.setWeight(leftWeight)

        if setting.getType() != 'toggle':
            area.createText(setting.getId(), ':', 16, widgets.Text.RIGHT)
        else:
            # Check boxes use a secondary indicator label.
            label = area.createText('toggle-use-default-value',
                                    align=widgets.Text.RIGHT)

        area.setWeight(0)
        area.addSpacer()

        # Create the actual widget(s) for the setting.
        area.setWeight(rightWeight)

        if setting.getType() == 'toggle':
            # Toggle settings have just a checkbox.
            isChecked = False
            if pr.getActive():
                isChecked = (pr.getActive().getValue(setting.getId()).
                             getValue() == 'yes')
            check = area.createCheckBox(setting.getId(), isChecked)
            check.setDefaultIndicator(label)

            #drop = area.createDropList(setting.getId())

            # Add all the possible choices into the list.
            #drop.addItem('default')
            #drop.addItem('yes')
            #drop.addItem('no')

            # By default, select the default choice.  This will get
            # updated shortly, though.
            #drop.selectItem('default')

        elif setting.getType() == 'range':
            # Create a label and the integer edit field.
            #area.setWeight(5)
            #area.createText(setting.getId())
            #area.setWeight(2)
            nf = area.createNumberField(setting.getId())
            nf.setRange(setting.getMinimum(), setting.getMaximum())

        elif setting.getType() == 'slider':
            # Create a label and a slider.
            #area.setWeight(2)
            #area.createText(setting.getId())
            #area.setWeight(2)
            slider = area.createSlider(setting.getId())
            slider.setRange(setting.getMinimum(),
                            setting.getMaximum(),
                            setting.getStep())

        elif setting.getType() == 'choice':
            # Create a label and a drop-down list.
            drop = area.createDropList(setting.getId())

            # Insert the choices into the list.
            drop.addItem('default')
            for a in setting.getAlternatives():
                drop.addItem(a)

            # Sort if necessary.
            if setting.isSorted():
                drop.sortItems()

            # The selection will be updated soon afterwards.
            drop.selectItem('default')

        elif setting.getType() == 'text':
            # Create a text field.
            #area.setWeight(1)
            #area.createText(setting.getId())
            #area.setWeight(2)
            text = area.createTextField(setting.getId())

        elif setting.getType() == 'file':
            # Create a text field and a button.
            #area.setWeight(1)
            #area.createText(setting.getId())
            #area.setWeight(2)
            sub = area.createArea(alignment=Area.ALIGN_HORIZONTAL, border=0)
            sub.setWeight(1)
            text = sub.createTextField(setting.getId())
            sub.setWeight(0)
            browseButton = sub.createButton('browse-button',
                                            style=widgets.Button.STYLE_MINI)

            def browseAction():
                # Open a file browser for selecting the value for a file
                # setting.
                settingId = setting.getId()
                value = pr.getActive().getValue(settingId)
                if not value:
                    currentValue = ''
                else:
                    currentValue = value.getValue()

                selection = chooseFile(settingId + '-selection-title',
                                       currentValue,
                                       setting.hasToExist(),
                                       setting.getAllowedTypes())

                if len(selection) > 0:
                    pr.getActive().setValue(settingId, selection)

            browseButton.addReaction(browseAction)

            # If the file must exist, don't accept nonexistent files.
            if setting.hasToExist():
                text.setValidator(lambda fileName: os.path.exists(fileName))

        elif setting.getType() == 'folder':
            # Create a text field and a button, like with a file setting.
            sub = area.createArea(alignment=Area.ALIGN_HORIZONTAL, border=0)
            sub.setWeight(1)
            text = sub.createTextField(setting.getId())
            sub.setWeight(0)
            browseButton = sub.createButton('browse-button',
                                            style=widgets.Button.STYLE_MINI)

            def folderBrowseAction():
                # Open a folder browser for selecting the value for a file
                # setting.
                settingId = setting.getId()
                value = pr.getActive().getValue(settingId)
                if not value:
                    currentValue = ''
                else:
                    currentValue = value.getValue()

                selection = chooseFolder(settingId + '-selection-title',
                                         currentValue)

                if len(selection) > 0:
                    pr.getActive().setValue(settingId, selection)

            browseButton.addReaction(folderBrowseAction)

            # If the file must exist, don't accept nonexistent files.
            if setting.hasToExist():
                text.setValidator(lambda fileName: os.path.exists(fileName))

        return area


class BoxedArea (Area):
    """A BoxedArea is an Area with a frame around it."""

    def __init__(self, boxTitle, parentArea, panel, alignment=0, border=0):
        """Construct a new BoxedArea.  Instances of BoxedArea are
        always sub-areas.

        @param boxTitle Identifier of the box title.

        @param parentArea The Area object inside which the BoxedArea
        will be added.

        @param panel The wxPanel into which all the widgets in the
        BoxedArea will be placed.

        @param alignment Alignment of widgets inside the area.

        @param border Border around widgets.
        """
        Area.__init__(self, '', panel, alignment, border, parentArea)

        self.label = boxTitle

        # Create a wxStaticBox and place it inside the area.
        self.box = wx.StaticBox(panel, -1, language.translate(boxTitle))

        self.containerSizer = wx.StaticBoxSizer(self.box,
                                                self.sizer.GetOrientation())

        # Insert the static box sizer into the area's sizer.  All the
        # widgets of the area will go inside the container sizer,
        # which is the static box sizer.
        self.sizer.Add(self.containerSizer, self.weight,
                       self._getLayoutFlags(), self.border)

    def destroy(self):
        """Destroy the area and its contents."""

        # First destroy the contents of the area.
        self.clear()

        self.sizer.Detach(self.containerSizer)
        ## @todo GTK crashes here if Destroy is called?
        #self.containerSizer.Destroy()

        self.box.Destroy()

        # Proceed with the normal Area destruction.
        Area.destroy(self)

    def retranslate(self):
        """Update the title of the box."""
        pass

    def createSetting(self, setting):
        self.setBorder(AREA_BORDER_BOXED)
        #return self.doCreateSetting(setting, 19, 40)
        return self.doCreateSetting(setting, SETTING_WEIGHT_LEFT * 20 - 1,
                                    SETTING_WEIGHT_RIGHT * 20)


class ColoredArea (Area):
    """ColoredArea uses its own wxPanel so it can have a background
    color.  In other aspects it works like a regular Area."""

    def __init__(self, color, parentArea, panel, alignment=0, border=0):
        """Construct a new colored area.  Instances of ColoredArea are
        always sub-areas."""

        # The ColoredArea uses its own panel.
        colorPanel = wx.Panel(panel, -1)
        colorPanel.SetBackgroundColour(color)
        colorPanel.SetBackgroundStyle(wx.BG_STYLE_COLOUR)

        Area.__init__(self, '', colorPanel, alignment, border, parentArea)

        colorPanel.SetSizer(self.sizer)

    def getWxWidget(self):
        return self.panel


class MultiArea (Area):
    """MultiArea contains multiple subareas.  Only one of them will
    be visible at a time.  All the subareas are visible in the same
    space."""

    def __init__(self, id, parentArea):
        Area.__init__(self, id, parentArea.panel, parentArea=parentArea)

        self.currentPage = None
        self.pages = {}

        events.addNotifyListener(self.onNotify, ['preparing-windows'])

    def destroy(self):
        """Destroy all the pages."""

        events.removeNotifyListener(self.onNotify)

        # Hide (detach) all the pages.
        self.showPage(None)

        for area in self.pages.values():
            area.destroy()

        Area.destroy()

    def getPages(self):
        """Return a list of all the pages of the area.

        @return An array of identifier strings.
        """
        return self.pages.keys()

    def createPage(self, identifier, align=Area.ALIGN_VERTICAL, border=3):
        """Create a subpage in the multiarea.  This does not make the
        page visible, however.

        @param identifier Identifier of the new page.
        """
        # Create a new panel for the page.
        panel = wx.Panel(self.panel, -1, style=wx.CLIP_CHILDREN)
        if host.isWindows():
            panel.SetBackgroundColour(widgets.tabBgColour)
            panel.SetBackgroundStyle(wx.BG_STYLE_SYSTEM)

        panel.Hide()

        # Create a new Area for the page.
        area = Area('', panel, alignment=align, border=border)
        area.setExpanding(True)
        area.setWeight(1)

        self.pages[identifier] = area

        return area

    def removePage(self, identifier):
        """Remove a page from the multiarea.

        @param identifier Page to remove.
        """
        try:
            area = self.pages[identifier]

            # Detach (if shown) and destroy the page area.
            sizer = area.panel.GetContainingSizer()
            if sizer:
                sizer.Detach(area.panel)
            area.destroy()

            # Remove the page from the dictionary.
            del self.pages[identifier]
        except KeyError:
            # Ignore unknown identifiers.
            pass

    def showPage(self, identifier):
        """Show a specific page.  The shown page is added to this
        area's containing sizer.

        @param identifier Identifier of the page to show.  If None,
        all pages are hidden.
        """
        changed = False

        for pageId in self.pages.keys():
            area = self.pages[pageId]
            sizer = area.panel.GetContainingSizer()

            if pageId == identifier:
                # Show this page.
                if not sizer:
                    self.containerSizer.Add(area.panel, 1,
                                            wx.EXPAND | wx.ALL, 3)
                    area.panel.Show()
                    changed = True
            else:
                # Hide this page, if not already hidden.
                if sizer:
                    sizer.Detach(area.panel)
                    area.panel.Hide()
                    changed = True

        if changed:
            self.updateLayout()

    def onNotify(self, event):
        if event.hasId('preparing-windows'):
            self.__updateMinSize()
            Area.updateLayout(self)

    def __updateMinSize(self):
        """Calculate min size based on all pages."""

        minWidth = 0
        minHeight = 0

        for area in self.pages.values():
            minSize = area.panel.GetBestSize()
            minWidth = max(minWidth, minSize[0])
            minHeight = max(minHeight, minSize[1])

        self.sizer.SetMinSize((minWidth + 6, minHeight + 6))
        self.sizer.Layout()

    def refresh(self):
        """Redraw the contents of the multiarea."""

        self.panel.Refresh()


class AreaDialog (wx.Dialog):
    """AreaDialog implements a wxDialog that has an Area inside it."""

    def __init__(self, parent, wxId, id, size, align=Area.ALIGN_VERTICAL):
        wx.Dialog.__init__(self, parent, wxId, uniConv(language.translate(id)),
                           style=(wx.RESIZE_BORDER | wx.CLOSE_BOX |
                                  wx.CAPTION))
        self.dialogId = id
        self.widgetMap = {}

        if size:
            self.recommendedSize = size
        else:
            self.recommendedSize = None

        # All the commands that will cause the dialog to be closed.
        self.dialogEndCommands = ['ok', 'cancel', 'yes', 'no']

        # Create an area inside the dialog.
        self.area = Area('', self, alignment=align, border=0)

    def getArea(self):
        """Return the Area which contains the dialog's widgets.

        @return Area object.
        """
        return self.area

    def addEndCommand(self, command):
        """Add a new command that will close the dialog.

        @param command A command identifier.
        """
        self.dialogEndCommands.append(command)

    def center(self):
        """Position the dialog in the center of the screen."""

        self.Centre()

    def run(self):
        """Show the dialog as modal.  Won't return until the dialog
        has been closed.

        @return The command that closed the dialog.
        """
        self.area.getWxWidget().Fit(self)

        # We'll listen to the common dialog commands (ok, cancel)
        # ourselves.
        events.addCommandListener(self.handleCommand)

        # The command that closes the dialog.  We'll assume the dialog
        # gets closed.
        self.closingCommand = 'cancel'

        if self.recommendedSize is not None:
            self.SetSize(self.recommendedSize)

        # Won't return until the dialog closes.
        self.ShowModal()

        # Stop listening.
        events.removeCommandListener(self.handleCommand)

        return self.closingCommand

    def close(self, closingCommand):
        """Close a modal dialog box that is currently open.

        @param closingCommand The command that is reported as being
        the one that caused the dialog to close.
        """
        self.closingCommand = closingCommand
        self.EndModal(0)

    def handleCommand(self, event):
        """The dialog listens to commands while it is modal.

        @param event A events.Command object.
        """
        if event.getId() in self.dialogEndCommands:
            self.close(event.getId())

    def identifyWidget(self, identifier, widget):
        """Tell the dialog of a widget inside dialog.

        @param identifier Identifier of the widget.

        @param widget The widget object.
        """
        self.widgetMap[identifier] = widget

    def enableWidget(self, identifier, doEnable=True):
        try:
            self.widgetMap[identifier].enable(doEnable)
        except:
            pass

    def disableWidget(self, identifier):
        self.enableWidget(identifier, False)


class WizardDialog (wiz.Wizard):
    """A wizard dialog.  Automatically manages the Next and Prev
    buttons, and opening and closing of the dialog.  The pages of the
    dialog must be created separately, using WizardPages."""

    def __init__(self, title, imageFileName):
        bmp = wx.Bitmap(imageFileName)
        self.wxId = wx.NewId()
        wiz.Wizard.__init__(self, mainPanel, self.wxId, title, bmp)

        self.SetBorder(0)

        # List of all the WizardPage objects associated with this
        # wizard.  This list does NOT define the order of the pages,
        # just that they're owned by this wizard.
        self.ownedPages = []

        wiz.EVT_WIZARD_PAGE_CHANGED(mainPanel, self.wxId, self.onPageChange)

    def addOwnedPage(self, page):
        """Add a new page into the wizard.  This is done automatically
        when a new WizardPage is constructed.  Does not affect the
        order of the pages."""

        self.ownedPages.append(page)

    def onPageChange(self, event):
        events.send(events.SelectNotify('wizard', event.GetPage().getId()))

    def run(self, firstPage):
        """Open the wizard dialog starting with the given page.

        @param firstPage A WizardPage object.
        """
        # Make sure the wizard window is large enough.
        self.FitToPage(firstPage)

        if self.RunWizard(firstPage):
            # The wizard was successful.
            return 'ok'
        else:
            return 'cancel'

    def destroy(self):
        # Destroy all the owned pages.
        for page in self.ownedPages:
            page.destroy()
        self.Destroy()


class WizardPage (wiz.PyWizardPage):
    """A wizard page that contains an Area for the widgets."""

    def __init__(self, wizard, title):
        wiz.PyWizardPage.__init__(self, wizard)

        # The title identifies the page.
        self.identifier = title

        # Links to other pages.
        self.prev = None
        self.next = None

        # Create an Area inside the page.
        self.area = Area('', self, alignment=Area.ALIGN_VERTICAL, border=6)
        self.area.setWeight(0)

        # Create the title widget.
        self.area.createText(title).setTitleStyle()
        self.area.createLine()

        # Tell the wizard of us.
        wizard.addOwnedPage(self)

    def destroy(self):
        self.area.destroy()

    def getId(self):
        return self.identifier

    def getArea(self):
        return self.area

    def setNext(self, page):
        self.next = page

    def setPrev(self, page):
        self.prev = page

    def follows(self, page):
        """This page follows another page.  Sets the next and prev
        links appropriately so that the given page is immediately
        before this one.

        @param page A WizardPage object.
        """
        self.setPrev(page)
        page.setNext(self)

    def GetNext(self):
        """Returns the next page of the wizard.  Called by
        WizardDialog."""

        return self.next

    def GetPrev(self):
        """Returns the previous page of the wizard.  Called by
        WizardDialog."""

        return self.prev


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

        #topSizer = wx.BoxSizer(wx.HORIZONTAL)

        aSizer = wx.BoxSizer(wx.HORIZONTAL)
        bSizer = wx.BoxSizer(wx.VERTICAL)
        cSizer = wx.BoxSizer(wx.HORIZONTAL)
        dSizer = wx.BoxSizer(wx.VERTICAL)
        eSizer = wx.BoxSizer(wx.HORIZONTAL)

        self.verticalSizer = bSizer

        # Title area.
        #sizer = wx.BoxSizer(wx.VERTICAL)
        if USE_TITLE_AREA:
            titlePanel = wx.Panel(self, -1,
                                  style=wx.NO_BORDER | wx.CLIP_CHILDREN)
            _newArea( Area(Area.TITLE, titlePanel, Area.ALIGN_HORIZONTAL) )
            bSizer.Add(titlePanel, 0, wx.EXPAND)

        # Profile area.
        #profilePanel = wx.Panel(self, -1, style=wx.NO_BORDER
        #                         | wx.CLIP_CHILDREN)
        #area = Area(Area.PROFILES, profilePanel, Area.ALIGN_VERTICAL, 10)
        #_newArea(area)
        #cSizer.Add(profilePanel, 0, wx.EXPAND)

        tabPanel = wx.Panel(self, -1,
                            style=wx.NO_BORDER | wx.CLIP_CHILDREN)
        tabsArea = Area(Area.TABS, tabPanel, Area.ALIGN_VERTICAL, 16)
        _newArea(tabsArea)

        dSizer.Add(tabPanel, 5, wx.EXPAND | wx.ALL, 3)

        # Command area.
        commandPanel = wx.Panel(self, -1, style=wx.NO_BORDER
                                 | wx.CLIP_CHILDREN)
        area = Area(Area.COMMAND, commandPanel, Area.ALIGN_HORIZONTAL, 18,
                    borderDirs=Area.BORDER_LEFT | Area.BORDER_RIGHT |
                               Area.BORDER_BOTTOM)
        # The command area is aligned to the right.
        area.addSpacer()
        area.setExpanding(False)
        area.setWeight(0)
        _newArea(area)

        # Preferences command area.
        prefCommandPanel = wx.Panel(self, -1, style=wx.NO_BORDER |
                                    wx.CLIP_CHILDREN)
        area = Area(Area.PREFCOMMAND, prefCommandPanel,
                    Area.ALIGN_HORIZONTAL, 18,
                    borderDirs=Area.BORDER_LEFT | Area.BORDER_RIGHT |
                               Area.BORDER_BOTTOM)
        area.setExpanding(False)
        area.setWeight(0)
        _newArea(area)

        # Create the Menu button in the Preferences Command area.
        self.menuButton = area.createButton('popup-menu')

        eSizer.Add(prefCommandPanel, 0, wx.EXPAND)
        eSizer.Add(commandPanel, 1, wx.EXPAND)
        dSizer.Add(eSizer, 0, wx.EXPAND)
        cSizer.Add(dSizer, 5, wx.EXPAND) # notebook
        bSizer.Add(cSizer, 1, wx.EXPAND)
        aSizer.Add(bSizer, 7, wx.EXPAND)

        # Help area.
        #helpPanel = wx.Panel(self, -1, style = wx.NO_BORDER)
        #helpPanel.SetBackgroundColour(wx.WHITE)
        #_newArea( Area(Area.HELP, helpPanel, Area.ALIGN_VERTICAL, border=4) )

        #aSizer.Add(helpPanel, 0, wx.EXPAND)
        self.SetSizer(aSizer)
        self.SetAutoLayout(True)

        # This maps panel objects to identifier strings.  Used when a
        # notification event needs to be sent.
        #self.tabMap = {}

        # Create a TabArea into the TABS area.
        self.tabs = tabsArea.createTabArea(
            'tab', style=sb.widget.tab.TabArea.STYLE_BASIC)

    def getTabs(self):
        """Returns the TabArea widget that contains the main tabbing
        area."""
        return self.tabs

    def updateLayout(self):
        self.verticalSizer.Layout()


INITIAL_SASH_POS = 180
INITIAL_PROFILE_SASH_POS = 170

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

        wx.Frame.__init__(self, None, -1, title, size=initialSize)
        #self.SetExtraStyle(wx.FRAME_EX_METAL)
        #self.Create(None, -1, title, size=initialSize)

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
                                              style=wx.SP_3DSASH)# |
                                              #wx.SP_NO_XP_THEME)
            self.splitter.SetMinimumPaneSize(10)
            parentWin = self.splitter

        self.profSplitter = wx.SplitterWindow(parentWin, PROF_SPLITTER_ID,
                                              style=wx.SP_3DSASH)# |
                                              #wx.SP_NO_XP_THEME)
        self.profSplitter.SetMinimumPaneSize(100)

        # Profile panel.
        profilePanel = wx.Panel(self.profSplitter, -1, style=wx.NO_BORDER
                                | wx.CLIP_CHILDREN)
        area = Area(Area.PROFILES, profilePanel, Area.ALIGN_VERTICAL, 10)
        _newArea(area)

        # Create panels inside the profile splitter.
        self.mainPanel = MainPanel(self.profSplitter)

        self.splitPos = None
        self.profSplitPos = INITIAL_PROFILE_SASH_POS

        # Create the help area.
        if self.splitter:
            self.helpPanel = wx.Panel(self.splitter, -1, style = wx.NO_BORDER
                                      | wx.CLIP_CHILDREN)
            self.helpPanel.SetBackgroundColour(wx.WHITE)
            _newArea( Area(Area.HELP, self.helpPanel, Area.ALIGN_VERTICAL,
                           border=4) )
            # Init the splitter.
            self.splitter.SplitVertically(self.profSplitter, self.helpPanel,
                                          -INITIAL_SASH_POS)
        else:
            self.helpPanel = None

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
        events.addCommandListener(self.handleCommand, ['quit', 'popup-menu'])

        #self.mainPanel.Hide()

        #if self.splitter:
        #    self.splitter.Freeze()


        #if self.helpPanel:
        #    self.helpPanel.Freeze()
        #    self.helpPanel.Hide()
        #self.Hide()

        # Create a menu bar.
        #menuBar = wx.MenuBar()

        #menu = wx.Menu()
        #menu.Append(wx.ID_ABOUT, language.translate('menu-about'))

        #menuBar.Append(menu, language.translate('menu-game'))

        # Show the menu.
        #self.SetMenuBar(menuBar)

        # Handle menu commands.
        #wx.EVT_MENU(self, wx.ID_ABOUT, self.onAbout)

    def show(self):
#<<<<<<< ui.py
        if self.splitter:
            self.splitter.Thaw()
            self.splitter.Refresh()
        #self.mainPanel.Show()
        #self.Show()
        #if self.helpPanel:
        #    self.helpPanel.Thaw()
        #    self.helpPanel.Show()
        pass
#=======
#        """Show the main window."""
#
#        self.mainPanel.Thaw()
#        self.Show()
#        if self.helpPanel:
#            self.helpPanel.Thaw()
#            self.helpPanel.Show()
#>>>>>>> 1.14

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

        elif event.hasId('popup-menu'):
            # TODO: This would be more logical as a part of MainPanel.
            # Show the popup commands menu.
            menu = wx.Menu()
            self.menuCommandMap = {}

            global popupMenuItems

            menuItems = []

            # Three levels of priority.
            for prio in range(3):
                # Sort the items based on translated labels.
                popupMenuItems[prio].sort(
                    lambda x, y: cmp(language.translate('menu-' + x),
                                     language.translate('menu-' + y)))

                if len(popupMenuItems[prio]) > 0 and len(menuItems) > 0:
                    # Separate the priority levels.
                    menuItems.append('-')
                menuItems += popupMenuItems[prio]

            # Create the menu items.
            for item in menuItems:
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
                wxId = wx.NewId()
                self.menuCommandMap[wxId] = itemCommand
                menu.Append(wxId,
                            uniConv(language.translate('menu-' + itemId)))
                wx.EVT_MENU(self, wxId, self.onPopupCommand)

            # Display the menu.  The callback gets called during this.
            button = self.mainPanel.menuButton.getWxWidget()
            button.PopupMenu(menu, (0, button.GetSizeTuple()[1]))
            menu.Destroy()

    def onPopupCommand(self, ev):
        """Called when a selection is made in the popup menu."""
        events.send(events.Command(self.menuCommandMap[ev.GetId()]))


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
        #self.mainFrame.Show()
        self.mainFrame.Thaw()
        self.mainFrame.Refresh()
        #self.mainFrame.show()


def prepareWindows():
    """Layout windows and make everything ready for action."""

    events.send(events.Notify('preparing-windows'))

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
