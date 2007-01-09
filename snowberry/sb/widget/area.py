# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2006
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

## @file area.py Area Class
##
## Each area can be divided into further sub-areas, which are treated
## the same way as the top-level areas.  The Area class can represent
## either a top-level area or a sub-area.

import sys, os, wx, string
import wx.wizard as wiz
import host, events, widgets, language, paths, ui
import base
import sb.widget.bar
import sb.widget.base
import sb.widget.button
import sb.widget.field
import sb.widget.list
import sb.widget.tab
import sb.widget.text
import sb.widget.tree
import sb.profdb as pr
import logger
from widgets import uniConv

# TODO: Make these .conf'able.
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

# wxWidget ID counters.
WIDGET_ID = 100


class Area (base.Widget):
    """Each Area instance governs the contents of a wxPanel widget.
    Together with the classes in widgets.py, the Area class
    encapsulates all management of the user interface.  Modules that
    use ui.py or widgets.py need not access wxWidgets directly at all.

    The Area class implements the base.Widget interface from
    widgets.py.  getWxWidget() and clear() are the important
    Widget-inherited methods that Area uses.

    Area alignment constants:
    - ALIGN_HORIZONTAL: Widgets are placed next to each other
      inside the area.
    - ALIGN_VERTICAL: New widgets are placed below the existing
      widgets.
    """

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
        if alignment == ui.ALIGN_HORIZONTAL:
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

    def setId(self, ident):
        self.id = ident

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
        global WIDGET_ID
        id = WIDGET_ID
        WIDGET_ID += 1
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
        if self.expanding:
            self.containerSizer.AddStretchSpacer(self.weight)
        else:
            self.containerSizer.Add((0, 0), self.weight, self._getLayoutFlags(),
                                    self.border)
        pass

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

    def createButton(self, name, style=sb.widget.button.Button.STYLE_NORMAL):
        """Create a button widget inside the area.

        @param name Identifier of the button.  The text label that
        appears in the button will be the translation of this
        identifier.

        @return A sb.widget.button.Button object.
        """
        w = sb.widget.button.Button(self.panel, self.__getNewId(), name, style)
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
                   align=sb.widget.text.Text.LEFT):
        """Create a text label widget inside the area.

        @param name Identifier of the text label widget.  The actual
        text that appears in the widget is the translation of this
        identifier.

        @return A widgets.Text object.
        """
        widget = sb.widget.text.Text(self.panel, -1, name, suffix,
                                     maxLineLength, align)
        self.__addWidget(widget)
        return widget

    def createTextField(self, name=''):
        """Create a text field widget inside the area.

        @param name Identifier of the text field widget.

        @return A widgets.TextField object.
        """
        widget = sb.widget.field.TextField(self.panel, -1, name)
        self.__addWidget(widget)
        return widget

    def createFormattedText(self):
        """Create a formatted text label widget inside the area.
        Initially the widget contains no text.

        @return A widgets.FormattedText object.
        """
        widget = sb.widget.text.FormattedText(self.panel, -1)
        self.__addWidget(widget)
        return widget

    def createRichText(self, text):
        """Create a static rich text label widget inside the area.
        Initially the widget contains no text.

        @return A widgets.FormattedText object.
        """
        widget = sb.widget.text.FormattedText(self.panel, -1, False, text)
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

    def createTree(self, name):
        """Create a tree widget inside the area.  Initially the tree
        is empty.  The Tree class provides methods for
        populating the tree with items.

        @param name Identifier of the tree widget.  Events
        sent by the widget are formed based on this identifier.
        For example, selection of an item causes the notification
        '(name)-selected'.

        @return A widgets.Tree object.

        @see widgets.Tree
        """
        widget = sb.widget.tree.Tree(self.panel, self.__getNewId(), name)
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
            sub = self.createArea(alignment=ui.ALIGN_HORIZONTAL, border=0)

            # Create the formatted list (with extended functionality).
            widget = sb.widget.tab.FormattedTabArea(self.panel, self.__getNewId(),
                                                    name)
            sub.setWeight(5)
            sub.setBorder(8, ui.BORDER_RIGHT)
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
        widget = sb.widget.button.CheckBox(self.panel, self.__getNewId(),
                                           name, isChecked)
        self.__addWidget(widget)
        return widget

    def createRadioButton(self, name, isChecked, isFirst=False):
        widget = sb.widget.button.RadioButton(self.panel, self.__getNewId(), name,
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
        widget = sb.widget.field.NumberField(self.panel, self.__getNewId(), name)
        self.__addWidget(widget)
        return widget

    def createSlider(self, name):
        widget = sb.widget.bar.Slider(self.panel, self.__getNewId(), name)
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

        @return  Tuple (area, widget). The area which contains all the widgets
                 of the setting, and the main widget of the setting.
        """
        if setting.getType() == 'implicit':
            # No widgets for implicit settings.
            return (None, None)

        # This is the returned main widget of the setting.
        mainWidget = None

        # Create a subarea for all the widgets of the setting.
        area = self.createArea(alignment=ui.ALIGN_HORIZONTAL)
        area.setExpanding(False)

        # All settings have a similarly positioned label on the left
        # side.
        area.setWeight(leftWeight)

        if setting.getType() != 'toggle':
            area.createText(setting.getId(), ':', 16, sb.widget.text.Text.RIGHT)
        else:
            # Check boxes use a secondary indicator label.
            label = area.createText('toggle-use-default-value',
                                    align=sb.widget.text.Text.RIGHT)

        area.setWeight(0)
        area.setBorder(2) # Space between label and setting.
        area.addSpacer()
        area.setBorder(0)

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
            mainWidget = check

        elif setting.getType() == 'range':
            nf = area.createNumberField(setting.getId())
            nf.setRange(setting.getMinimum(), setting.getMaximum())
            mainWidget = nf

        elif setting.getType() == 'slider':
            slider = area.createSlider(setting.getId())
            slider.setRange(setting.getMinimum(),
                            setting.getMaximum(),
                            setting.getStep())
            mainWidget = slider

        elif setting.getType() == 'choice':
            # Create a label and a drop-down list.
            drop = area.createDropList(setting.getId())
            mainWidget = drop

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
            text = area.createTextField(setting.getId())
            mainWidget = text

        elif setting.getType() == 'file':
            # Create a text field and a button.
            sub = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=0)
            sub.setWeight(1)
            text = sub.createTextField(setting.getId())
            mainWidget = text
            sub.setWeight(0)
            sub.setBorder(4, ui.BORDER_LEFT)
            browseButton = sub.createButton('browse-button',
                                            style=sb.widget.button.Button.STYLE_MINI)

            def browseAction():
                # Open a file browser for selecting the value for a file
                # setting.
                settingId = setting.getId()
                value = pr.getActive().getValue(settingId)
                if not value:
                    currentValue = ''
                else:
                    currentValue = value.getValue()

                selection = sb.util.dialog.chooseFile(
                    settingId + '-selection-title',
                    currentValue,
                    setting.hasToExist(),
                    setting.getAllowedTypes())

                if len(selection) > 0:
                    pr.getActive().setValue(settingId, selection)
                    if events.isMuted():
                        # Update manually.
                        text.getFromProfile(pr.getActive())

            browseButton.addReaction(browseAction)

            # If the file must exist, don't accept nonexistent files.
            if setting.hasToExist():
                text.setValidator(lambda fileName: os.path.exists(fileName))

        elif setting.getType() == 'folder':
            # Create a text field and a button, like with a file setting.
            sub = area.createArea(alignment=ui.ALIGN_HORIZONTAL, border=0)
            sub.setWeight(1)
            text = sub.createTextField(setting.getId())
            mainWidget = text
            sub.setWeight(0)
            browseButton = sub.createButton('browse-button',
                                            style=sb.widget.button.Button.STYLE_MINI)

            def folderBrowseAction():
                # Open a folder browser for selecting the value for a file
                # setting.
                settingId = setting.getId()
                value = pr.getActive().getValue(settingId)
                if not value:
                    currentValue = ''
                else:
                    currentValue = value.getValue()

                selection = sb.util.dialog.chooseFolder(settingId + '-selection-title',
                                                        currentValue)

                if len(selection) > 0:
                    pr.getActive().setValue(settingId, selection)

            browseButton.addReaction(folderBrowseAction)

            # If the file must exist, don't accept nonexistent files.
            if setting.hasToExist():
                text.setValidator(lambda fileName: os.path.exists(fileName))

        return (area, mainWidget)


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

    def createPage(self, identifier, align=ui.ALIGN_VERTICAL, border=3):
        """Create a subpage in the multiarea.  This does not make the
        page visible, however.

        @param identifier Identifier of the new page.
        """
        # Create a new panel for the page.
        panel = wx.Panel(self.panel, -1, style=wx.CLIP_CHILDREN)
        if host.isWindows():
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
            area.panel.Destroy()

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
        return self.doCreateSetting(setting,
                                    SETTING_WEIGHT_LEFT * 20 - 1,
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
