# -*- coding: iso-8859-1 -*-
# $Id: ui.py 3529 2006-08-09 21:31:38Z skyjake $
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

## @file dialog.py Dialogs

import sys, os, wx, string
import wx.wizard as wiz
import ui, host, events, widgets, language
import sb.profdb as pr
import settings as st
import logger
from widgets import uniConv
import sb.widget.area


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
    dialog = AreaDialog(ui.getMainPanel(), -1, id, size, alignment)
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
    dialog = createDialog(id, ui.ALIGN_HORIZONTAL, size)
    area = dialog.getArea()

    area.setMinSize(400, 20)

    # The Snowberry logo is on the left.
    #area.setWeight(0)
    #imageArea = area.createArea(alignment=ui.ALIGN_VERTICAL, border=0,
    #                            backgroundColor=wx.Colour(255, 255, 255))
    #imageArea.setWeight(1)
    #imageArea.setExpanding(True)
    #imageArea.addSpacer()
    #imageArea.setWeight(0)
    #imageArea.createImage('snowberry')

    area.setWeight(1)
    area.setBorderDirs(ui.BORDER_NOT_BOTTOM)
    contentArea = area.createArea(alignment=ui.ALIGN_VERTICAL, border=6)
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
    contentArea.setBorderDirs(ui.BORDER_NOT_TOP)
    buttonArea = contentArea.createArea(alignment=ui.ALIGN_HORIZONTAL,
                                        border=0)

    # If no explicit spacing is defined, use the default right
    # alignment.
    if '' not in buttons:
        # The implied spacer.
        buttonArea.addSpacer()

    buttonArea.setBorder(6, ui.BORDER_LEFT_RIGHT)
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


class AreaDialog (wx.Dialog):
    """AreaDialog implements a wxDialog that has an Area inside it."""

    def __init__(self, parent, wxId, id, size, align=ui.ALIGN_VERTICAL):
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
        self.area = sb.widget.area.Area('', self, alignment=align, border=0)

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
        wiz.Wizard.__init__(self, ui.getMainPanel(), self.wxId, title, bmp)

        self.SetBorder(0)

        # List of all the WizardPage objects associated with this
        # wizard.  This list does NOT define the order of the pages,
        # just that they're owned by this wizard.
        self.ownedPages = []

        wiz.EVT_WIZARD_PAGE_CHANGED(ui.getMainPanel(), self.wxId, self.onPageChange)

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
        self.area = sb.widget.area.Area('', self, alignment=ui.ALIGN_VERTICAL, border=6)
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
