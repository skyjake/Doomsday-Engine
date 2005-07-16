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

## @file widgets.py User Interface Widgets
##
## This module contains a number of wrapper classes that wrap the real
## widget classes of wxWidgets.  This is done for purposes of
## convenience and event translation.  Events are converted into
## notifications that can be broadcasted using events.py.
##
## FormattedTabArea and TabArea could use a common base class.

import sys
import wx
import wx.html
import wx.lib.intctrl as intctrl
import wx.lib.scrolledpanel as scrolled
import wx.lib.fancytext as fancy
import host, events, ui, language, paths
import addons as ao
import profiles as pr
import settings as st


def uniConv(str):
    """Decode character encoding to Unicode.

    @param str  String in local encoding.
    
    @return Unicode string.
    """
    if host.isWindows():
        # Don't do Unicode on Windows.
        return str
    else:
        return str.decode(host.getEncoding())


def uniDeconv(str):
    """Encode Unicode back to local character encoding.

    @param str  Unicode string.
    
    @return String.
    """
    if host.isWindows():
        # Don't do Unicode on Windows.
        return str
    else:
        return str.encode(host.getEncoding())
    

# A Windows kludge: background colour for tabs and the controls in them.
tabBgColour = wx.Colour(250, 250, 250)
#st.getSystemInteger('tab-background-red'),
#                        st.getSystemInteger('tab-background-green'),
#                        st.getSystemInteger('tab-background-blue'))

# The image list for all the icons used in the UI.
iconManager = None


def breakLongLines(text, maxLineLength):
    """Break long lines with newline characters.

    @param maxLineLength  Maximum length of a line, in characters.

    @return The new text with newlines inserted."""
    
    brokenText = ''
    lineLen = 0
    skipping = False
    for i in range(len(text) - 1):
        if text[i] == '<':
            skipping = True
        elif skipping and text[i] == '>':
            skipping = False

        if not skipping:
            lineLen += 1
                    
        if (text[i] in ' -/:;') and lineLen > maxLineLength:
            # Break it up!
            if text[i] != ' ':
                brokenText += text[i]
            if not skipping:
                # Newlines are not inserted if we're skipping.
                brokenText += '\n'
                lineLen = 0
        else:
            brokenText += text[i]

    return brokenText + text[-1]   


def initIconManager():
    """Initialize a manager for all the icons used in the user
    interface.
    """
    wx.InitAllImageHandlers()

    global iconManager
    iconManager = IconManager(48, 48)


class IconManager:
    """The IconManager class provides a simpler identifier-based
    interface to wxWidgets image lists."""

    def __init__(self, width, height):
        """Construct a new icon manager.

        @param width Width of the icons.

        @param height Height of the icons.
        """
        self.imageList = wx.ImageList(width, height)

        # Identifiers of the icons in the list.
        self.bitmaps = []

    def getImageList(self):
        """Returns the wxImageList managed by this manager.

        @return A wxImageList object.
        """
        return self.imageList

    def get(self, identifier):
        """Finds the index number of the specified image.  If the
        image hasn't yet been loaded, it is loaded now.

        @param identifier Identifier of the image.
        """
        try:
            return self.bitmaps.index(identifier)

        except ValueError:
            # Load it now.  Images are affected by the localisation.
            imageName = language.translate(identifier)
            fileName = paths.findBitmap(imageName)
            if len(fileName) == 0:
                # Fallback icon.
                fileName = paths.findBitmap('deng')
            bmp = wx.Bitmap(fileName)
            self.imageList.Add(bmp)
            self.bitmaps.append(identifier)
            return len(self.bitmaps) - 1


class Widget:
    """Widget is the abstract base class for all user interface
    widgets.  Each subclass acts as a wrapper around the real
    wxWidget, converting wxWidgets events to commands and
    notifications."""

    # Font styles.
    NORMAL = 0
    BOLD = 1
    ITALIC = 2
    HEADING = 3
    TITLE = 4
    SMALL = 5

    def __init__(self, wxWidget):
        """Constructor.
        @param wxWidget The wxWidget this Widget manages.
        """
        self.wxWidget = wxWidget
        self.menuItems = None
        self.focusId = ''

        # Callback functions for listening directly to changes.
        self.reactors = []

        self.setNormalStyle()

        # Listen for right mouse clicks in case a popup menu should be
        # displayed.
        wx.EVT_RIGHT_UP(wxWidget, self.onRightClick)

        # Listen for focus changes.
        wx.EVT_SET_FOCUS(wxWidget, self.onFocus)

        # Register a notification listener.
        events.addNotifyListener(self.onNotify)
        events.addCommandListener(self.onCommand)

    def getWxWidget(self):
        """Returns the real wxWidget of this widget."""
        return self.wxWidget

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
        self.getWxWidget().SetFocus()

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
        # Display the popup menu.
        if self.menuItems and self.getWxWidget().IsEnabled():
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
                            uniConv(language.translate('menu-' + itemId)))
                wx.EVT_MENU(self.getWxWidget(), wxId, self.onPopupCommand)

            # Display the menu.  The callback gets called during this.
            self.getWxWidget().PopupMenu(menu, ev.GetPosition())
            menu.Destroy()
            
        # Let wxWidgets handle the event, too.
        ev.Skip()      

    def onPopupCommand(self, ev):
        events.send(events.Command(self.commandMap[ev.GetId()]))

    def onFocus(self, event):
        """Handle the wxWidgets event that's sent when the widget
        received input focus.

        @param event A wxWidgets event.
        """
        event.Skip()
        # Send a focus event if a focus identifier has been specified.
        if self.focusId:
            events.send(events.FocusNotify(self.focusId))

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
        w.SetSize(w.GetBestSize())

    def setNormalStyle(self):
        """Set the style of the text label to Widget.NORMAL."""
        self.setStyle(Widget.NORMAL)

    def setSmallStyle(self):
        """Set the style of the text label to Widget.SMALL."""
        self.setStyle(Widget.SMALL)

    def setBoldStyle(self):
        """Set the style of the text label to Widget.BOLD."""
        self.setStyle(Widget.BOLD)

    def setItalicStyle(self):
        """Set the style of the text label to Widget.ITALIC."""
        self.setStyle(Widget.ITALIC)

    def setHeadingStyle(self):
        """Set the style of the text label to Widget.HEADING."""
        self.setStyle(Widget.HEADING)

    def setTitleStyle(self):
        """Set the style of the text label to Widget.TITLE."""
        self.setStyle(Widget.TITLE)


class Line (Widget):
    """A static horizontal divider line."""

    def __init__(self, parent):
        """Construct a new divider line.

        @param parent The parent panel.
        """
        Widget.__init__(self, wx.StaticLine(parent, -1,
                                            style=wx.LI_HORIZONTAL))


class Image (Widget):
    """A static image widget.  The user will not be able to interact
    with instances of the Image widget."""

    def __init__(self, parent, imageName):
        """Construct a new Image widget.

        @param imageName Base name of the image file.  It must be
        located in one of the graphics directories.
        """
        # Create a new panel so we can control the background color.
        self.panel = wx.Panel(parent, -1)
        self.panel.SetBackgroundColour(wx.WHITE)
        Widget.__init__(self, self.panel)

        # See if we can find the right image.
        bmp = wx.Image(paths.findBitmap(imageName)).ConvertToBitmap()

        self.bitmap = wx.StaticBitmap(self.panel, -1, bmp)
        self.panel.SetMinSize(bmp.GetSize())

    def setImage(self, imageName):
        """Change the image displayed in the widget.

        @param imageName Base name of the image file.  It must be
        located in one of the graphics directories.
        """
        bmp = wx.Image(paths.findBitmap(imageName)).ConvertToBitmap()
        self.bitmap.Freeze()
        self.bitmap.SetBitmap(bmp)
        self.panel.SetMinSize(bmp.GetSize())
        self.bitmap.Thaw()
        self.panel.Refresh()


class Button (Widget):
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

        Widget.__init__(self, widget)
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
        else:
            # Pop back up when toggled down.
            wx.EVT_TOGGLEBUTTON(parent, wxId, self.onToggle)

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


class CheckBox (Widget):
    """A check box widget."""

    def __init__(self, parent, wxId, label, isChecked):
        Widget.__init__(self, wx.CheckBox(parent, wxId,
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
                    ': ' + language.translate(
                    pr.getDefaults().getValue(self.widgetId).getValue()) + ')')
                #self.indicator.getWxWidget().Show()
            else:
                self.indicator.setText('')
                #self.indicator.getWxWidget().Hide()

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

    def onNotify(self, event):
        """Handle notifications.  When the active profile changes, the
        check box's state is updated."""
        
        Widget.onNotify(self, event)

        if self.widgetId:
            if event.hasId('active-profile-changed'):
                w = self.getWxWidget()

                # Get the value for the setting as it has been defined
                # in the currently active profile.
                value = pr.getActive().getValue(self.widgetId, False)
                if value:
                    if value.getValue() == 'yes':
                        w.Set3StateValue(wx.CHK_CHECKED)
                    else:
                        w.Set3StateValue(wx.CHK_UNCHECKED)
                else:
                    w.Set3StateValue(wx.CHK_UNDETERMINED)

                self.updateState()

    def retranslate(self):
        if self.widgetId:
            self.getWxWidget().SetLabel(
                uniConv(language.translate(self.widgetId)))


class RadioButton (Widget):
    """A radio button widget."""

    def __init__(self, parent, wxId, label, isToggled, isFirst=False):
        if isFirst:
            flags = wx.RB_GROUP
        else:
            flags = 0
        Widget.__init__(self, wx.RadioButton(parent, wxId, 
                                             language.translate(label), 
                                             style=flags))
        self.label = label
        
        if isToggled:
            self.getWxWidget().SetValue(True)
        
    def setText(self, text):
        self.getWxWidget().SetLabel(text)
        
    def isChecked(self):
        return self.getWxWidget().GetValue()


class MyStaticText (wx.StaticText):
    def __init__(self, parent, wxId, label, style=0):
        wx.StaticText.__init__(self, parent, wxId, label, style=style)
        if host.isWindows():
            # No background.
            self.SetBackgroundStyle(wx.BG_STYLE_SYSTEM)
        wx.EVT_ERASE_BACKGROUND(self, self.clearBack)

    def clearBack(self, event):
        pass


class Text (Widget):
    """A static text widget that displays a string of text."""

    # Alignments.
    LEFT = 0
    MIDDLE = 1
    RIGHT = 2
    
    def __init__(self, parent, wxId, id, suffix='', maxLineLength=0, align=0):
        """Constructor.

        @param parent
        @param wxId
        @param id      Identifier of the text string.
        @param suffix  Suffix for the translated version of the text.
        @param maxLineLength  Maximum line length for the text (in characters).
        @param alignment      Alignment (LEFT, MIDDLE, RIGHT).
        """
        # Prepare the text string.
        self.maxLineLength = maxLineLength
        self.suffix = suffix
        self.widgetId = id

        # Alignment style flag.
        styleFlags = 0
        if align == Text.LEFT:
            styleFlags |= wx.ALIGN_LEFT
        elif align == Text.MIDDLE:
            styleFlags |= wx.ALIGN_CENTRE
        elif align == Text.RIGHT:
            styleFlags |= wx.ALIGN_RIGHT
            
        Widget.__init__(self, MyStaticText(parent, wxId,
                                           uniConv(self.__prepareText()),
                                           style = styleFlags |
                                           wx.ST_NO_AUTORESIZE))
        self.setNormalStyle()

        # When the text is clicked, send a notification.
        wx.EVT_LEFT_DOWN(self.getWxWidget(), self.onClick)

    def __prepareText(self):
        text = language.translate(self.widgetId) + self.suffix
        if self.maxLineLength > 0:
            text = breakLongLines(text, self.maxLineLength)
        return text

    def retranslate(self):
        if self.widgetId:
            self.setText(self.__prepareText())

    def setText(self, text):
        "Set new untranslated content into the text widget."
        self.getWxWidget().SetLabel(uniConv(text))

    def onClick(self, ev):
        """Clicking a label causes a focus request to be sent."""
        
        focusId = self.focusId
        if not focusId:
            focusId = self.widgetId
        
        if focusId:
            event = events.FocusRequestNotify(focusId)
            events.send(event)

        ev.Skip()


class TextField (Widget):
    """A text field for entering a single-line string of text."""

    def __init__(self, parent, wxId, id):
        Widget.__init__(self, wx.TextCtrl(parent, wxId))
        self.widgetId = id

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

    def onNotify(self, event):
        """Notification listener.

        @param event An events.Notify object.
        """
        Widget.onNotify(self, event)

        if self.reactToNotify and event.hasId('value-changed'):
            if event.getSetting() == self.widgetId:
                # This is our value.
                self.setText(event.getValue())

        if self.widgetId and event.hasId('active-profile-changed'):
            # Get the value for the setting as it has been defined
            # in the currently active profile.
            value = pr.getActive().getValue(self.widgetId, False)
            if value:
                self.setText(value.getValue())
            else:
                self.setText('')


class MyHtmlWindow (wx.html.HtmlWindow):
    def __init__(self, parent, wxId):
        wx.html.HtmlWindow.__init__(self, parent, wxId)
        #self.SetBackgroundStyle(wx.BG_STYLE_COLOUR)
        #self.SetBackgroundColour(wx.SystemSettings_GetColour(
        #    wx.SYS_COLOUR_WINDOW))
        self.SetBorders(3)

    def OnLinkClicked(self, link):
        """Handle a hypertext link click by sending a command event.

        @param link A wxHtmlLinkInfo object.
        """
        events.send(events.Command(link.GetHref()))


class FormattedText (Widget):
    def __init__(self, parent, wxId, useHtmlFormatting=True, initialText=''):
        """Constructor.

        @param parent  Parent wxWidget ID.
        @param wxId    ID of the formatted text widget.
        @param useHtmlFormatting  Use a HTML window widget.
        @param initialText  Initial contents.
        """

        self.useHtml = useHtmlFormatting
        
        if self.useHtml:
            Widget.__init__(self, MyHtmlWindow(parent, wxId))
        else:
            text = initialText.replace('<b>', '<font weight="bold">')
            text = text.replace('</b>', '</font>')
            text = text.replace('<i>', '<font style="italic">')
            text = text.replace('</i>', '</font>')
            text = text.replace('<tt>', '<font family="fixed">')
            text = text.replace('</tt>', '</font>')

            # fancytext doesn't support non-ascii chars?
            text = text.replace('ä', 'a')
            text = text.replace('ö', 'o')
            text = text.replace('ü', 'u')
            text = text.replace('å', 'a')
            text = text.replace('Ä', 'A')
            text = text.replace('Ö', 'O')
            text = text.replace('Ü', 'U')
            text = text.replace('Å', 'A')

            # Break it up if too long lines detected.
            brokenText = breakLongLines(text, 70)
            
            Widget.__init__(self, fancy.StaticFancyText(
                parent, wxId, uniConv(brokenText)))

    def setText(self, formattedText):
        "Set new HTML content into the formatted text widget."
        w = self.getWxWidget()

        if self.useHtml:
            fontName = None
            fontSize = None
            
            if st.isDefined("style-html-font"):
                fontName = st.getSystemString("style-html-font")
            if st.isDefined("style-html-size"):
                fontSize = st.getSystemString("style-html-size")
            
            if fontName != None or fontSize != None:
                fontElem = "<font"
                if fontName != None:
                    fontElem += ' face="%s"' % fontName
                if fontSize != None:
                    fontElem += ' size="%s"' % fontSize
                fontElem += ">"
            else:
                fontElem = None

            #color = wx.SystemSettings_GetColour(wx.SYS_COLOUR_HIGHLIGHT)
            #print color
        
            #formattedText = '<body bgcolor="#%02x%02x%02x">' \
            #                % (color.Red(), color.Green(), color.Blue()) \
            #                + formattedText + '</body>'
        
            if fontElem == None:
                w.SetPage(uniConv(formattedText))
            else:
                w.SetPage(uniConv(fontElem + formattedText + '</font>'))

    def setSystemBackground(self):
        """Use the window background colour as the background colour
        of the formatted text."""

        self.getWxWidget().SetBackgroundStyle(wx.BG_STYLE_SYSTEM)

        #color = wx.SystemSettings_GetColour(wx.SYS_COLOUR_WINDOW)
        #print color


class NumberField (Widget):
    def __init__(self, parent, wxId, id):
        Widget.__init__(self, intctrl.IntCtrl(parent, wxId,
                                              limited=False,
                                              allow_none=True))
        self.widgetId = id

        # Clear the widget by default.
        self.getWxWidget().SetValue(None)

        # If True, a value-changed notification will cause the field
        # to change.
        self.reactToNotify = True

        # We want focus notifications.
        self.setFocusId(id)

        # Listen for changes.
        intctrl.EVT_INT(parent, wxId, self.onChange)

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
                if v:
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
        Widget.onNotify(self, event)
        
        if self.widgetId:
            if self.reactToNotify and event.hasId('value-changed'):
                if event.getSetting() == self.widgetId:
                    # This is our value.
                    if event.getValue() != None:
                        self.setValue(int(event.getValue()))
                    else:
                        self.setValue(None)

            elif event.hasId('active-profile-changed'):
                # Get the value for the setting as it has been defined
                # in the currently active profile.
                value = pr.getActive().getValue(self.widgetId, False)
                if value:
                    self.setValue(int(value.getValue()))
                else:
                    self.setValue(None)


class Slider (Widget):
    def __init__(self, parent, wxId, id):
        """A slider widget for constrained selection of values."""
        Widget.__init__(self, wx.Slider(parent, wxId, 50, 0, 100,
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
        Widget.onNotify(self, event)

        if self.widgetId:
            if (event.hasId('active-profile-changed') or
                (event.hasId('value-changed') and
                 event.getSetting() == self.widgetId)):
                self.__getValueFromProfile()

    def onCommand(self, event):
        Widget.onCommand(self, event)

        if self.widgetId:
            if event.hasId(self.resetCommand):
                pr.getActive().removeValue(self.widgetId)


class List (Widget):

    # List styles.
    STYLE_SINGLE = 0
    STYLE_MULTIPLE = 1
    STYLE_CHECKBOX = 2
    STYLE_COLUMNS = 3
    
    def __init__(self, parent, wxId, id, style=0):
        self.style = style
        
        if style == List.STYLE_CHECKBOX:
            # Listbox with checkable items.
            Widget.__init__(self, wx.CheckListBox(parent, wxId))
        elif style == List.STYLE_COLUMNS:
            Widget.__init__(self, wx.ListCtrl(parent, wxId,
                                              style=(wx.LC_REPORT |
                                                     wx.SIMPLE_BORDER)))
            wx.EVT_LIST_ITEM_SELECTED(parent, wxId, self.onItemSelected)
        else:
            # Normal listbox.
            if style == List.STYLE_MULTIPLE:
                styleFlags = wx.LB_MULTIPLE
            elif style == List.STYLE_SINGLE:
                styleFlags = wx.LB_SINGLE
            Widget.__init__(self, wx.ListBox(parent, wxId, style=styleFlags))

        # Will be used when sending notifications.
        self.widgetId = id

        self.items = []
        self.columns = []

    def addItem(self, identifier, isChecked=False):
        """Append a new item into the listbox.

        @param identifier Identifier of the item.
        """
        w = self.getWxWidget()
        if language.isDefined(identifier):
            visibleText = language.translate(identifier)
        else:
            visibleText = identifier
        w.Append(visibleText)
        self.items.append(identifier)

        # In a checklistbox, the items can be checked.
        if self.style == List.STYLE_CHECKBOX:
            if isChecked:
                w.Check(w.GetCount() - 1)

    def removeItem(self, identifier):
        """Remove an item from the listbox.

        @param identifier  Identifier of the item to remove.
        """
        w = self.getWxWidget()
        for i in range(len(self.items)):
            if identifier == self.items[i]:
                w.Delete(i)
                self.items.remove(identifier)
                break

    def addItemWithColumns(self, identifier, *columns):
        w = self.getWxWidget()

        index = w.InsertStringItem(w.GetItemCount(), columns[0])
        for i in range(1, len(columns)):
            w.SetStringItem(index, i, columns[i])

        self.items.append(identifier)

    def addColumn(self, identifier, width=-1):
        w = self.getWxWidget()
        numCols = w.GetColumnCount()
        self.getWxWidget().InsertColumn(numCols,
                                        language.translate(identifier),
                                        width=width)

        self.columns.append(identifier)

    def removeAllItems(self):
        """Remove all the items in the list."""

        self.getWxWidget().DeleteAllItems()
        self.items = []

    def getItems(self):
        """Returns a list of all the item identifiers in the list."""
        return self.items

    def getSelectedItems(self):
        """Get the identifiers of all the selected items in the list.
        If the list was created with the List.STYLE_CHECKBOX style,
        this returns all the checked items.

        @return An array of identifiers.
        """
        w = self.getWxWidget()
        selected = []

        if self.style == List.STYLE_CHECKBOX:
            # Return a list of checked items.
            for i in range(w.GetCount()):
                if w.IsChecked(i):
                    selected.append(self.items[i])
            return selected

        if self.style == List.STYLE_COLUMNS:
            # Check the state of each item.
            for i in range(w.GetItemCount()):
                state = w.GetItemState(i, wx.LIST_STATE_SELECTED)
                if state & wx.LIST_STATE_SELECTED:
                    selected.append(self.items[i])
            return selected

        # Compose a list of selected items.
        for sel in w.GetSelections():
            selected.append(self.items[sel])
        return selected

    def getSelectedItem(self):
        """Get the identifier of the currently selected item.  Only
        for single-selection lists.

        @return The identifier.
        """
        index = self.getWxWidget().GetSelection()
        if index == wx.NOT_FOUND:
            return ''
        else:
            return self.items[index]

    def selectItem(self, identifier):
        try:
            index = self.items.index(identifier)
            self.getWxWidget().SetSelection(index)
        except:
            pass

    def checkItem(self, identifier, doCheck=True):
        """Check or uncheck an item in a list that was created with
        the Check style.

        @param identifier Identifier of the item to check.

        @param doCheck True, if the item should be checked.  False, if
        unchecked.
        """
        try:
            index = self.items.index(identifier)
            self.getWxWidget().Check(index, doCheck)
        except:
            # Identifier not found?
            pass

    def moveItem(self, identifier, steps):
        """Move an item up or down in the list.

        @param identifier Item to move.

        @param steps Number of indices to move.  Negative numbers move
        the item upwards, positive numbers move it down.
        """
        w = self.getWxWidget()
        
        for index in range(len(self.items)):
            if self.items[index] == identifier:
                # This is the item to move.
                w.Delete(index)
                del self.items[index]

                # Insert it to another location.
                index += steps
                if index < 0:
                    index = 0
                elif index > len(self.items):
                    index = len(self.items)

                w.Insert(language.translate(identifier), index)
                w.SetSelection(index)

                # Also update the items list.
                self.items = self.items[:index] + [identifier] + \
                             self.items[index:]
                break

    def onItemSelected(self, event):
        """Handle the wxWidgets item selection event.

        @param event A wxWidgets event.
        """

        event.Skip()
        self.react()
        

class DropList (Widget):
    """A drop-down list widget."""

    def __init__(self, parent, wxId, id):
        Widget.__init__(self, wx.Choice(parent, wxId))

        # Will be used when sending notifications.
        self.widgetId = id

        self.items = []

        # We want focus notifications.
        self.setFocusId(id)

        # Handle item selection events.
        wx.EVT_CHOICE(parent, wxId, self.onItemSelected)
        wx.EVT_LEFT_DOWN(self.getWxWidget(), self.onLeftClick)

    def onLeftClick(self, event):
        event.Skip()
        if self.widgetId:
            events.send(events.FocusNotify(self.widgetId))

    def onItemSelected(self, ev):
        """Handle the wxWidgets event that is sent when the current
        selection changes in the list box.

        @param ev A wxWidgets event.
        """
        if self.widgetId:
            newSelection = self.items[ev.GetSelection()]

            if newSelection == 'default':
                pr.getActive().removeValue(self.widgetId)
            else:
                pr.getActive().setValue(self.widgetId, newSelection)

            # Notify everyone of the change.
            events.send(events.EditNotify(self.widgetId, newSelection))

    def onNotify(self, event):
        Widget.onNotify(self, event)

        if self.widgetId:
            if event.hasId('active-profile-changed'):
                # Get the value for the setting as it has been defined
                # in the currently active profile.
                value = pr.getActive().getValue(self.widgetId, False)
                if value:
                    self.selectItem(value.getValue())
                else:
                    self.selectItem('default')

    def clear(self):
        """Delete all the items."""
        self.items = []
        self.getWxWidget().Clear()

    def __filter(self, itemText):
        """Filter the item text so it can be displayed on screen."""

        # Set the maximum length.
        if len(itemText) > 30:
            return '...' + itemText[-30:]

        return uniConv(itemText)

    def retranslate(self):
        """Update the text of the items in the drop list. Preserve
        current selection."""
        
        drop = self.getWxWidget()
        sel = drop.GetSelection()

        # We will replace all the items in the list.
        drop.Clear()        
        for i in range(len(self.items)):
            drop.Append(self.__filter(language.translate(self.items[i])))

        drop.SetSelection(sel)

    def addItem(self, item):
        """Append a list of items into the drop-down list.

        @param items An array of strings.
        """
        self.items.append(item)
        self.getWxWidget().Append(self.__filter(language.translate(item)))

    def selectItem(self, item):
        """Change the current selection.  This does not send any
        notifications.

        @param item Identifier of the item to select.
        """
        for i in range(len(self.items)):
            if self.items[i] == item:
                self.getWxWidget().SetSelection(i)
                break

    def getSelectedItem(self):
        """Returns the identifier of the selected item.

        @return An identifier string.
        """
        return self.items[self.getWxWidget().GetSelection()]

    def sortItems(self):
        """Sort all the items in the list."""
        w = self.getWxWidget()
        w.Clear()
        def sorter(a, b):
            if a[0] == 'default':
                return -1
            elif b[0] == 'default':
                return 1
            return cmp(a[1], b[1])
        translated = map(lambda i: (i, language.translate(i)), self.items)
        translated.sort(sorter)
        self.items = []
        for id, item in translated:
            self.items.append(id)
            w.Append(self.__filter(item))

    def getItems(self):
        """Returns the identifiers of the items in the list.

        @return An array of identifiers."""
        return self.items


class HtmlListBox (wx.HtmlListBox):
    """An implementation of the abstract wxWidgets HTML list box."""

    def __init__(self, parent, wxId, itemsListOwner, style=0):
        wx.HtmlListBox.__init__(self, parent, wxId, style=style)

        # A reference to the owner of the array where the items are
        # stored.  Items is a list of tuples: (id, presentation)
        self.itemsListOwner = itemsListOwner

        # Force a resize event at this point so that GTK doesn't
        # select the wrong first visible item.
        #self.SetSize((100, 100))

    def OnGetItem(self, n):
        """Retrieve the representation of an item.  This method must
        be implemented in the derived class and should return the body
        (i.e. without &lt;html&gt; nor &lt;body&gt; tags) of the HTML
        fragment for the given item.

        @param n Item index number.

        @return HTML-formatted contents of the item.
        """
        return self.itemsListOwner.items[n][1]


class FormattedList (Widget):
    """A list widget with HTML-formatted items."""

    def __init__(self, parent, wxId, id, style=0):
        """Construct a FormattedList object.

        @param parent Parent wxWidget.
        @param wxId wxWidgets ID of the new widget (integer).
        @param id Identifier of the new widget (string).
        """
        # This item array is also used by the HtmlListBox instance.
        # This is a list of tuples: (id, presentation)
        self.items = []

        Widget.__init__(self, HtmlListBox(parent, wxId, self, style=style))

        self.widgetId = id

        # By default, items are added in alphabetical order.
        self.addSorted = True

        # Listen for selection changes.
        wx.EVT_LISTBOX(parent, wxId, self.onItemSelected)
        wx.EVT_LISTBOX_DCLICK(parent, wxId, self.onItemDoubleClick)

    def setSorted(self, doSort):
        self.addSorted = doSort

    def clear(self):
        """Delete all the items."""
        self.items = []
        self.__updateItemCount()

    def addItem(self, itemId, rawItemText, toIndex=None):
        """Add a new item into the formatted list box.  No translation
        is done.  Instead, the provided item text is displayed using
        HTML formatting.

        @param itemId Identifier of the item.  This is sent in
        notification events.

        @param itemText Formatted text for the item.

        @param toIndex Optional index where the item should be placed.
        If None, the item is added to the end of the list.
        """
        itemText = uniConv(rawItemText)
        
        # If this item identifier already exists, update its
        # description without making any further changes.
        for i in range(len(self.items)):
            if self.items[i][0] == itemId:
                self.items[i] = (itemId, itemText)

                # Let the widget know that a change has occured.
                # Although it may be that this does nothing...
                self.__updateItemCount()
                return

        newItem = (itemId, itemText)
        if toIndex == None:
            if self.addSorted:
                toIndex = 0
                # Sort alphabetically based on identifier.
                for i in range(len(self.items)):
                    if cmp(itemId, self.items[i][0]) < 0:
                        break
                    toIndex = i + 1
            else:
                toIndex = len(self.items)

        self.items = self.items[:toIndex] + [newItem] + \
                     self.items[toIndex:]
        self.__updateItemCount()

    def removeItem(self, itemId):
        """Remove an item from the list.

        @param itemId Identifier of the item to remove.
        """
        # Filter out the unwanted item.
        self.items = [item for item in self.items
                      if item[0] != itemId]
        self.__updateItemCount()

    def __updateItemCount(self):
        w = self.getWxWidget()
        w.Hide()
        w.SetItemCount(len(self.items))
        w.SetSelection(self.getSelectedIndex())
        w.Show()

        #w.Freeze()
        #w.SetItemCount(len(self.items))
        #w.SetSelection(self.getSelectedIndex())
        #w.Thaw()
        #w.Refresh()

    def getItemCount(self):
        """Returns the number of items in the list."""
        
        return len(self.items)

    def selectItem(self, itemId):
        """Select an item in the list."""
        for i in range(len(self.items)):
            if self.items[i][0] == itemId:
                self.getWxWidget().SetSelection(i)
                break

    def getSelectedIndex(self):
        count = len(self.items)
        sel = self.getWxWidget().GetSelection()
        if sel >= count:
            sel = count - 1
        return sel

    def getSelectedItem(self):
        """Return the selected item.

        @return Identifier of the selected item.
        """
        return self.items[self.getSelectedIndex()][0]

    def hasItem(self, itemId):
        """Checks if the list contains an item with the specified
        identifier.

        @param itemId Identifier of the item to check.

        @return True, if the item exists.  Otherwise False.
        """
        for item in self.items:
            if item[0] == itemId:
                return True
        return False

    def onItemSelected(self, ev):
        """Handle the wxWidgets event that is sent when the current
        selection changes in the list box.

        @param ev A wxWidgets event.
        """
        ev.Skip()
        events.send(events.SelectNotify(self.widgetId,
                                        self.items[ev.GetSelection()][0]))

    def onItemDoubleClick(self, ev):
        ev.Skip()
        events.send(events.SelectNotify(self.widgetId,
                                        self.items[ev.GetSelection()][0],
                                        True))


class FormattedTabArea (FormattedList):
    """FormattedTabArea extends FormattedList by pairing it up with a
    previously created MultiArea.  This class implements the same
    interface as TabArea.

    This widget class is different from the other classes.  The other
    classes encapsulate a single wxWidgets widget.  FormattedTabArea
    combines a FormattedList with a MultiArea object."""

    def __init__(self, parent, wxId, id):
        """Constuct a new formatted tab area.

        @param parent The parent wxPanel.

        @param wxId The ID number of the formatted list.

        @param id The identifier of the formatted list.

        @param ownerArea The Area in which the formatted tab area
        resides.
        """
        FormattedList.__init__(self, parent, wxId, id,
                               style=wx.SUNKEN_BORDER)

        self.setSorted(False)
        self.multi = None

        # List of all currently hidden tabs.
        self.hiddenTabs = []

    def setMultiArea(self, multiArea):
        """Set the MultiArea that will react to the changes in the list.
        
        @param multiArea The ui.MultiArea object to pair up with the
        list.
        """
        self.multi = multiArea

    def __getPresentation(self, tabId):
        """Compose the HTML presentation for a tab identifier.

        @return HTML content as a string.
        """
        # Kludge for the game-options icon.  There should be a way to
        # map the icon more flexibly so this hardcoding wouldn't be
        # needed.
        if tabId == 'game-options':
            game = 'game-jdoom'
            if pr.getActive():
                for c in pr.getActive().getComponents():
                    if c[:5] == 'game-':
                        game = c
                        break
            imageName = language.translate(game + '-icon')
        else:
            imageName = language.translate(tabId + '-icon')

        return ('<table width="100%" border=0 cellspacing=3 cellpadding=1>' + 
                '<tr><td width=35><img width=32 height=32 ' + 
                'src="%s"><td align="left" valign="center">%s</td>' %
                (paths.findBitmap(imageName), language.translate(tabId)) +
                '</table>')

    def retranslate(self):
        """Update the icons and names of the categories."""

        for tab in self.getTabs():
            self.updateIcon(tab)

        self.multi.refresh()

    def getTabs(self):
        """Compose a list of all the areas of the tab area.  Hidden
        areas are included, too.

        @return An array that contains identifiers.
        """
        return self.multi.getPages()

    def getSelectedTab(self):
        """Returns the identifier of the currently selected tab.
        Returns None if no tab is selected."""

        return self.getSelectedItem()

    def addTab(self, id):
        """Add a new tab into the TabArea widget.  The tab is appended
        after previously added tabs.

        @param id Identifier for the tab.  The name and the icon of
        the tab will be determined using this identifier.

        @return The Area object for the contents of the tab.
        """
        area = self.multi.createPage(id)

        # Add it to the end.
        self.addItem(id, self.__getPresentation(id))

        # Automatically show the first page.
        if self.getItemCount() == 1:
            self.selectTab(id)

        return area

    def removeAllTabs(self):
        """Remove all tabs from the tab area."""

        for id in self.getTabs():
            self.removeTab(id)

    def removeTab(self, id):
        """Remove an existing tab from the TabArea widget.

        @param id Identifier of the tab to remove.
        """
        if id in self.hiddenTabs:
            self.hiddenTabs.remove(id)
        
        self.multi.removePage(id)
        self.removeItem(id)

    def selectTab(self, selectedId):
        """Select a specific tab.

        @param selectedId Identifier of the tab to select.
        """
        self.selectItem(selectedId)
        self.multi.showPage(selectedId)

    def hideTab(self, identifier):
        """Hide a tab in the tab area.

        @param identifier Identifier of the tab to hide.
        """
        self.showTab(identifier, False)

    def showTab(self, identifier, doShow=True):
        """Show a hidden tab in the tab area, or hide a tab.  Hiding
        and showing does not alter the order of the tabs.

        @param identifier Identifier of the tab to show or hide.

        @param doShow True, if the tab should be shown; False, if hidden.
        """
        if doShow and identifier in self.hiddenTabs:
            self.hiddenTabs.remove(identifier)
            self.addItem(identifier, self.__getPresentation(identifier), 0)

        elif not doShow and identifier not in self.hiddenTabs:
            # Move the selection to the previous item.
            #if self.getSelectedTab() == identifier:
            #    index = min(self.getItemCount() - 1, self.getSelectedIndex() + 1)
            #    self.selectTab(self.items[index][0])
                
            self.hiddenTabs.append(identifier)
            self.removeItem(identifier)
        
    def onItemSelected(self, ev):
        FormattedList.onItemSelected(self, ev)

        self.multi.showPage(self.getSelectedItem())

    def updateIcon(self, identifier):
        """Update the presentation of the specified tab.

        @param identifier Tab identifier.
        """
        self.addItem(identifier, self.__getPresentation(identifier))

#    def updateIcons(self):
#        """Update the items in the formatted list."""
#
#        #tabs = self.getTabs()
#        #self.addItem(

    


class TabArea (Widget):
    """TabArea is a widget that implements a tab area.  It supports a
    number of different styles: TabArea.STYLE_ICON and
    TabArea.STYLE_CHOICE.  In the icon style, the user can select the
    visible tab by clicking on one of the icons in the list.  In the
    choice style, there is drop-down list for the tab titles.
    """

    # Tab area styles.
    STYLE_ICON = 'icon'
    STYLE_HORIZ_ICON = 'horiz-icon'
    STYLE_DROP_LIST = 'drop'
    STYLE_BASIC = 'basic'
    STYLE_FORMATTED = 'formatted'

    def __init__(self, parent, wxId, id, style):
        # The formatted tab area style uses another widget entirely.
        if style == TabArea.STYLE_FORMATTED:
            raise Exception('not supported here')
        
        # Create the appropriate widget.
        if style == TabArea.STYLE_ICON:
            w = wx.Listbook(parent, wxId, style=wx.LB_LEFT)
            w.GetListView().SetSingleStyle(wx.LC_AUTOARRANGE)
        elif style == TabArea.STYLE_HORIZ_ICON:
            w = wx.Listbook(parent, wxId, style=wx.LB_TOP)
        elif style == TabArea.STYLE_BASIC:
            w = wx.Notebook(parent, wxId)
        else:
            w = wx.Choicebook(parent, wxId, style=wx.CHB_TOP)

        Widget.__init__(self, w)

        self.style = style

        # The horizontal icon style is handled like the normal icon
        # style.
        if self.style == TabArea.STYLE_HORIZ_ICON:
            self.style = TabArea.STYLE_ICON

        # The IconTabs widget remembers its id so it can send
        # notifications events.
        self.widgetId = id

        # The panel map is used when figuring out the IDs of each page
        # when an event occurs.  It contains an ordered array of
        # tuples: (identifier, panel).
        self.panelMap = []
        self.tabAreas = {}

        if self.style == TabArea.STYLE_ICON:
            wx.EVT_LISTBOOK_PAGE_CHANGED(parent, wxId, self.onTabChange)
            # The icon style uses the icons.
            w.SetImageList(iconManager.getImageList())
        elif style == TabArea.STYLE_BASIC:
            wx.EVT_NOTEBOOK_PAGE_CHANGED(parent, wxId, self.onTabChange)
        else:
            wx.EVT_CHOICEBOOK_PAGE_CHANGED(parent, wxId, self.onTabChange)

    def clear(self):
        """When a TabArea is cleared, it must destroy all of its tabs,
        which contain subareas."""
        
        self.removeAllTabs()
        Widget.clear(self)

    def getTabs(self):
        """Compose a list of all the areas of the tab area.  Hidden
        areas are included, too.

        @return An array that contains identifiers.
        """
        return map(lambda mapped: mapped[0], self.panelMap)

    def __lookupPanel(self, panel):
        """Looks up a panel identifier in the panel map.

        @param panel A wxPanel object.

        @return Identifier of the tab.  None, if panel not found.
        """
        for id, mapped in self.panelMap:
            if panel == mapped:
                return id
        return None

    def __getTabIndex(self, id):
        """Determine the index of a specific tab in the tab area."""

        book = self.getWxWidget()
        for i in range(book.GetPageCount()):
            if self.__lookupPanel(book.GetPage(i)) == id:
                return i
        return None

    def __getPanelIndex(self, panel):
        """Determine the index of a specific panel in the tab area."""

        book = self.getWxWidget()
        for i in range(book.GetPageCount()):
            if book.GetPage(i) is panel:
                return i
        return None

    def addTab(self, id):
        """Add a new tab into the TabArea widget.  The tab is appended
        after previously added tabs.

        @param id Identifier for the tab.  The name and the icon of
        the tab will be determined using this identifier.

        @return The Area object for the contents of the tab.
        """
        book = self.getWxWidget()
        #tab = scrolled.ScrolledPanel(book, -1)
        #tab.SetupScrolling()
        tab = wx.Panel(book, -1, style=wx.CLIP_CHILDREN)

        if host.isWindows():
            tab.SetBackgroundColour(tabBgColour)
            tab.SetBackgroundStyle(wx.BG_STYLE_SYSTEM)

        # Put the new tab in the page map so that when an event
        # occurs, we will know the ID of the tab.
        self.panelMap.append((id, tab))

        # Insert the page into the book.
        book.AddPage(tab, language.translate(id))

        area = ui.Area(id, tab, ui.Area.ALIGN_VERTICAL)
        area.setExpanding(True)
        area.setWeight(1)

        # Store the Area object in a dictionary for quick access.
        self.tabAreas[id] = area

        self.updateIcons()

        return area

    def removeTab(self, id):
        """Remove an existing tab from the TabArea widget.

        @param id Identifier of the tab to remove.
        """
        panel = None
        for tabId, mapped in self.panelMap:
            if tabId == id:
                # This is the panel to remove.
                panel = mapped
                break

        if not panel:
            # It doesn't exist.
            return

        # Remove the panel from the map and destroy it.
        for i in range(len(self.panelMap)):
            if self.panelMap[i][1] is panel:
                del self.panelMap[i]
                break

        # Remove the page from the tab area, but this doesn't
        # destroy the panel.
        book = self.getWxWidget()
        for i in range(book.GetPageCount()):
            if book.GetPage(i) == panel:
                book.RemovePage(i)
                break

        self.tabAreas[id].destroy()
        del self.tabAreas[id]

    def removeAllTabs(self):
        """Remove all tabs from the tab area."""

        for id in self.getTabs():
            self.removeTab(id)

    def selectTab(self, selectedId):
        """Select a specific tab.

        @param id Tab identifier.
        """
        index = self.__getTabIndex(selectedId)
        if index != None:
            self.getWxWidget().SetSelection(index)

    def hideTab(self, identifier):
        """Hide a tab in the tab area.

        @param identifier Identifier of the tab to hide.
        """
        self.showTab(identifier, False)

    def showTab(self, identifier, doShow=True):
        """Show a hidden tab in the tab area, or hide a tab.  Hiding
        and showing does not alter the order of the tabs.

        @param identifier Identifier of the tab to show or hide.

        @param doShow True, if the tab should be shown; False, if hidden.
        """
        book = self.getWxWidget()

        if not doShow:
            # Hide the page by removing it from the widget.
            index = self.__getTabIndex(identifier)
            if index != None:
                # Is this currently selected?
                if index == book.GetSelection():
                    # Automatically select another page.
                    if index > 0:
                        book.SetSelection(index - 1)
                    elif index < book.GetPageCount() - 1:
                        book.SetSelection(index + 1)
                
                # This won't destroy the panel or its contents.
                book.RemovePage(index)

        else:
            # Show the page by inserting it to the correct index.
            for tabId, panel in self.panelMap:
                if tabId == identifier:
                    # Determine the location of the shown tab.
                    where = 0
                    for id, mapped in self.panelMap:
                        index = self.__getPanelIndex(mapped)
                        if id == identifier:
                            if index != None:
                                # The panel appears to be visible already.
                                return
                            break
                        if index != None:
                            where = index + 1

                    # Insert this panel back into the book.
                    book.InsertPage(where, panel,
                                    language.translate(identifier))
                    self.updateIcons()
                    return

    def updateIcons(self):
        """Update all the icons used by the tab area labels, if using
        an icon style.
        """
        if self.style == TabArea.STYLE_ICON:
            # Update all the tabs.
            for identifier, panel in self.panelMap:
                pageIndex = self.__getPanelIndex(panel)
                if pageIndex != None:
                    iconName = identifier + '-icon'
                    self.getWxWidget().SetPageImage(pageIndex,
                                                    iconManager.get(iconName))

    def retranslate(self):
        """Retranslate the tab labels."""
    
        for identifier, panel in self.panelMap:
            pageIndex = self.__getPanelIndex(panel)
            if pageIndex != None:
                try:
                    self.getWxWidget().SetPageText(
                        pageIndex, language.translate(identifier))
                except:
                    # wxWidgets hasn't implemented the appropriate method?
                    pass

    def onTabChange(self, ev):
        """Handle the wxWidgets event that occurs when the currently
        selected tab changes."""

        # Figure out which panel is now visible.
        tabPanel = self.getWxWidget().GetPage(ev.GetSelection())

        # Send a notification event.
        notification = events.SelectNotify(self.widgetId,
                                           self.__lookupPanel(tabPanel))
        events.send(notification)


class MyTreeCtrl (wx.TreeCtrl):
    """A wxTreeCtrl that sorts items alphabetically, but places all
    branches after the leaves.  That is, all items with children are
    placed below the items that don't have children."""

    def __init__(self, parent, wxId):
        wx.TreeCtrl.__init__(self, parent, wxId,
                             style=(wx.TR_NO_LINES |
                                    wx.TR_HAS_BUTTONS |
                                    wx.TR_TWIST_BUTTONS |
                                    wx.TR_HIDE_ROOT))

    def OnCompareItems(self, item1, item2):
        """Override this function in the derived class to change the
        sort order of the items in the tree control.  The function
        should return a negative, zero or positive value if the first
        item is less than, equal to or greater than the second one.
        """
        # Count the number of children (non-recursively).
        n1 = self.GetChildrenCount(item1, False)
        n2 = self.GetChildrenCount(item2, False)

        if (n1 == 0 and n2 == 0) or (n1 > 0 and n2 > 0):
            # Normal comparison based on case-insensitive alphabet.
            return cmp(self.GetItemText(item1).lower(),
                       self.GetItemText(item2).lower())

        # The one with no children must come first.
        if n1 == 0 and n2 > 0:
            return -1
        else:
            return 1


class Tree (Widget):
    """The addon browser tree widget."""

    class Hierarchy:
        """A subclass for storing information about a category
        hierarchy."""
        
        def __init__(self):
            self.root = None
            self.rootId = ''
            self.categories = {}
            self.items = {}

        def hasAddon(self, addon):
            """Checks if the addon is represented in the hierarchy.

            @param addon The addon identifier.
            """
            return self.items.has_key(addon)

        def getCategory(self, item):
            """Finds the category that matches the item.

            @param item A Tree item object.

            @return Category object.
            """
            for category in self.categories.keys():
                if self.categories[category] == item:
                    return category
            return None

    def __init__(self, parent, wxId):
        Widget.__init__(self, MyTreeCtrl(parent, wxId))

        # The addon tree uses a fixed identifier.
        self.widgetId = 'addon-tree'

        # Load the tree icons.
        images = wx.ImageList(16, 16)
        images.Add(wx.Bitmap(paths.findBitmap('unchecked')))
        images.Add(wx.Bitmap(paths.findBitmap('checked')))
        images.Add(wx.Bitmap(paths.findBitmap('defcheck')))
        if host.isWindows():
            images.Add(wx.Bitmap(paths.findBitmap('closedfolder')))
            images.Add(wx.Bitmap(paths.findBitmap('openfolder')))
        self.getWxWidget().AssignImageList(images)

        ## @todo Should this be LEFT_UP?
        wx.EVT_LEFT_DOWN(self.getWxWidget(), self.onItemClick)

        # Right-clicking in the tree must change the selection in
        # addition to displaying the context menu.
        wx.EVT_RIGHT_DOWN(self.getWxWidget(), self.onRightClick)

        # We will not allow expanding totally empty categories.
        wx.EVT_TREE_ITEM_EXPANDING(parent, wxId, self.onItemExpanding)

        self.grayColour = wx.Colour(192, 192, 192)

        # The root items.
        self.removeAll()

    def createCategories(self):
        """Create menu items for all the categories.  The appearance
        of the category title reflects its contents.  If a category
        contains no addons, it is displayed using a gray colour.
        Categories with only unchecked addons are displayed in bold
        black.  Categories with checked addons are also bold black,
        but they have a number after the title displaying the number
        of checked addons in the category.  The numbers are inherited
        by the parent categories of a category with checked addons.
        """
        tree = self.getWxWidget()
        tree.Freeze()
      
        # Create the root items.
        self.available.rootId = 'available-addons-root'
        self.available.root = tree.AppendItem(self.root,
                                              language.translate\
                                              (self.available.rootId))
        self.defaults.rootId = 'default-addons-root'
        self.defaults.root = tree.AppendItem(self.root,
                                             language.translate\
                                             (self.defaults.rootId))

        self.createCategoryTree(self.available)
        self.createCategoryTree(self.defaults)

        tree.Thaw()

    def createCategoryTree(self, hierarchy):
        """Create a category tree under the specified root item.

        @param rootItem The tree item under which the categories are
        to be created.

        @param categoryMap A dictionary that will hold the mapping
        between Category objects and tree items.
        """
        tree = self.getWxWidget()
        
        # Start with no categories.
        hierarchy.categories = { ao.getRootCategory(): hierarchy.root }

        # Start from the root level.
        levelCategories = ao.getSubCategories(ao.getRootCategory())

        while len(levelCategories) > 0:
            # Sort the categories by name.
            levelCategories.sort(lambda a, b: cmp(a.getName(), b.getName()))

            # The categories of the next level.
            nextLevel = []

            for category in levelCategories:
                # Create an item for this category.
                parentItem = hierarchy.categories[category.getParent()]

                # The labels will be updated separately.
                item = tree.AppendItem(parentItem, '')

                if host.isWindows():
                    # Windows requires that all items have an image.
                    tree.SetItemImage(item, 3)
                                
                hierarchy.categories[category] = item

                # Append this category's children for the next level
                # of processing.
                nextLevel += ao.getSubCategories(category)

            levelCategories = nextLevel

    def removeAll(self):
        """Remove all the items in the tree, including categories and
        addons.
        """
        # Delete all children of the hidden root.
        tree = self.getWxWidget()

        tree.Freeze()
        tree.DeleteAllItems()
        tree.Thaw()

        self.defaults = Tree.Hierarchy()
        self.available = Tree.Hierarchy()
      
        # Create the hidden root item of the tree.
        # The real root item won't be visible.
        self.root = tree.AddRoot('Addons')

    def removeAddons(self):
        """Remove all the addons from the tree.  Does not affect the
        categories.
        """
        pass

    def isItemUnder(self, child, parent):
        """Returns True if the parent is truly an ancestor of the
        child."""
        tree = self.getWxWidget()
        item = tree.GetItemParent(child)
        while item:
            if item == parent:
                return True
            item = tree.GetItemParent(item)
        return False

    def insertSorted(self, parentItem, label):
        """Insert a tree item under the parent item, so that it's in
        the proper alphabetical order.
        """
        tree = self.getWxWidget()
        index = 0

        # Find the item before which the new item will be inserted.
        child, cookie = tree.GetFirstChild(parentItem)
        while child:
            # If we hit a Category item, we must stop.
            if self.getItemData(child) == None:
                break
            if cmp(label.lower(), tree.GetItemText(child).lower()) < 0:
                # Insert it before this one.
                break
            child = tree.GetNextSibling(child)
            index += 1
            
        item = tree.InsertItemBefore(parentItem, index, label)
        return item

    def setItemData(self, item, data):
        self.getWxWidget().SetPyData(item, data)

    def getItemData(self, item):
        try:
            data = self.getWxWidget().GetPyData(item)
            return data
        except:
            return None

    def showAddons(self, addons, hierarchy):
        """Make sure that only the specified addons are visible in the
        tree.  All other items are removed from the tree.

        @param addon An array of addon identifiers.

        @param categoryMap Dictionary of Categories to tree items.

        @param itemMap Dictionary of addon identifiers to tree items.
        This will be updated as the method progresses.
        """
        tree = self.getWxWidget()
        tree.Freeze()
        
        # Insert all addons that are listed in addons but are not in
        # the item map.
        for addon in addons:

            # Hidden addons are not listed.
            if not ao.get(addon).isVisible():
                continue
            
            if not hierarchy.items.has_key(addon):
                # Create this item.
                category = ao.get(addon).getCategory()
                parentItem = hierarchy.categories[category]
                item = self.insertSorted(parentItem,
                                         language.translate(addon))
                hierarchy.items[addon] = item

                # The extra data contains information about the addon.
                self.setItemData(item, addon)
        
        # Remove all addons that are in the map but are not in the
        # addons list.
        for addon in hierarchy.items.keys():
            if addon not in addons:
                # Remove the item from the tree.
                tree.Delete(hierarchy.items[addon])
                # Remove the entry from the dictionary.
                del hierarchy.items[addon]

        tree.Thaw()

    def populateWithAddons(self, forProfile):
        """Calling this populates the tree with addons.  The addon
        categories must already be created by calling
        createCategories.

        @param forProfile If this is not None, only the addons
        compatible with this profile are added to the tree.
        """
        tree = self.getWxWidget()

        # The visible root items.
        defaultAddons = pr.getDefaults().getAddons()

        # All available addons for the profile.
        idents = map(lambda a: a.getId(),
                     ao.getAvailableAddons(forProfile))

        if forProfile is pr.getDefaults():
            self.showAddons(idents, self.defaults)
            self.showAddons([], self.available)

        else:
            defaultAddons = [a for a in defaultAddons
                              if ao.get(a).isCompatibleWith(forProfile)]

            self.showAddons(defaultAddons, self.defaults)
            self.showAddons(idents, self.available)

        self.refreshCategoryLabels(forProfile)
        self.refreshItems(forProfile)

    def refreshCategoryLabels(self, profile):
        """Set the text and the font used in all the category labels.
        The labels reflect the contents of the category.
        """
        tree = self.getWxWidget()

        # The addons that will actually be used when launching.
        usedAddons = profile.getUsedAddons()
        
        for hier in [self.defaults, self.available]:

            rootLabel = language.translate(hier.rootId)
            totalCount = len(hier.items)

            if host.isWindows():
                # The closed folder icon.
                tree.SetItemImage(hier.root, 3)

            if totalCount == 0:
                tree.SetItemBold(hier.root, False)
                tree.SetItemTextColour(hier.root, self.grayColour)
                tree.SetItemText(hier.root, uniConv(rootLabel))
                tree.Collapse(hier.root)
                # Expansion of an empty tree should be vetoed.
            else:
                # Expand all categories that have checked items in
                # them.  Collapse the others.
                for category in hier.categories.keys():
                    # Count the number of addons that will be used.
                    checkCount = 0
                    count = 0

                    # Are all the items in the category box parts?
                    justBoxParts = True
                    
                    for addon in hier.items.keys():
                        if self.isItemUnder(hier.items[addon],
                                            hier.categories[category]):
                            count += 1
                            if addon in usedAddons:
                                checkCount += 1

                            # Box parts?
                            if not ao.get(addon).getBox():
                                justBoxParts = False

                    item = hier.categories[category]
                    tree.SetItemBold(item, checkCount > 0)
                    
                    if category is ao.getRootCategory():
                        label = rootLabel
                    else:
                        label = category.getName()

                    # The appearance of the label depends on the
                    # number of addons in the children.
                    if count > 0:
                        if checkCount > 0:
                            label += ' (' + str(checkCount) + ')'
                            # Don't force-open the Defaults root.
                            #if item is not hier.root and not justBoxParts:
                            #    tree.Expand(item)
                        tree.SetItemTextColour(item, wx.BLACK)
                    elif count == 0:
                        tree.Collapse(item)
                        tree.SetItemTextColour(item, self.grayColour)
                    tree.SetItemText(item, uniConv(label))

        # Expand and collapse the roots based on which profile is
        # selected.
        if profile is pr.getDefaults():
            tree.Expand(self.defaults.root)
            tree.Collapse(self.available.root)
        else:
            tree.Collapse(self.defaults.root)
            tree.Expand(self.available.root)

    def refreshItems(self, forProfile, justAddon=None):
        """Refresh the appearance of items."""

        tree = self.getWxWidget()
        if forProfile is not pr.getDefaults():
            defaultAddons = pr.getDefaults().getAddons()
        else:
            defaultAddons = []

        usedAddons = forProfile.getUsedAddons()

        for hier in [self.available, self.defaults]:
            for addon in hier.items.keys():
                if justAddon == None or addon == justAddon:
                    item = hier.items[addon]
                    if addon in usedAddons:
                        tree.SetItemTextColour(item, wx.BLACK)
                        if addon in defaultAddons:
                            # The 'D' icon.
                            imageNum = 2
                        else:
                            # The normal check icon.
                            imageNum = 1
                    else:
                        # The unchecked icon.
                        #tree.SetItemTextColour(item, wx.Colour(100,100,100))
                        imageNum = 0
                    tree.SetItemImage(item, imageNum)

    def onNotify(self, event):
        """Handle a notification event.  Refreshes the contents of the
        tree when an addon is attached to or detached from a profile.

        @param event An events.Notify object.
        """
        Widget.onNotify(self, event)
        
        if event.hasId('addon-attached') or event.hasId('addon-detached'):
            if event.getProfile() is pr.getActive():
                self.refreshCategoryLabels(event.getProfile())
                self.refreshItems(event.getProfile())

    def retranslate(self):
        """Update all the items in the tree."""

        if pr.getActive():
            self.refreshCategoryLabels(pr.getActive())
            self.refreshItems(pr.getActive())

    def getSelectedAddon(self):
        """Returns the currently selected addon.

        @return Identifier of the selected addon.
        """
        tree = self.getWxWidget()
        return self.getItemData(tree.GetSelection())

    def lookupCategory(self, item):
        """Returns the Category object that is associated with the
        tree item, if any.

        @return None, or a addons.Category object.
        """
        for hier in [self.available, self.defaults]:
            for category in hier.categories.keys():
                if hier.categories[category] == item:
                    return category
        return None

    def refreshPopupMenu(self):
        """Refresh the popup menu depending on the currently selected
        tree icon.
        """
        tree = self.getWxWidget()
        selectedItem = tree.GetSelection()
        addon = self.getItemData(selectedItem)

        # Send a request to update the popup menu.
        request = events.Notify('addon-popup-request')
        if addon:
            # These shouldn't be hardcoded menus...
            request.addon = ao.get(addon)
            request.category = None
        else:
            request.addon = None
            # Is there a category here?
            request.category = self.lookupCategory(selectedItem)

        # The menu will get updated during send().
        events.send(request)

    def onItemClick(self, ev):
        """Handle the wxWidgets event when the user clicks on the tree
        widget.

        @param ev A wxWidgets event object.
        """
        tree = self.getWxWidget()

        item, flags = tree.HitTest(ev.GetPosition())
        addonName = self.getItemData(item)

        if flags & wx.TREE_HITTEST_ONITEMBUTTON:
            tree.Toggle(item)

        if flags & wx.TREE_HITTEST_ONITEMICON and addonName:
            profile = pr.getActive()
            usedAddons = profile.getUsedAddons()

            if addonName in usedAddons:
                profile.dontUseAddon(addonName)
            else:
                profile.useAddon(addonName)

            # Also change the selection.
            self.selectItem(item)

        elif (flags & wx.TREE_HITTEST_ONITEMLABEL or
              flags & wx.TREE_HITTEST_ONITEMRIGHT):
            # Change the selection.
            self.selectItem(item)

    def selectItem(self, item):
        """Select a particular item in the tree.

        @param item The tree item to select.
        """
        tree = self.getWxWidget()
        tree.SelectItem(item)

        # If an addon is associated with the item, send a notification.
        addon = self.getItemData(item)
        if addon:
            events.sendAfter(events.SelectNotify(self.widgetId, addon))
        else:
            # Perhaps a category was selected?
            cat = self.available.getCategory(tree.GetSelection())
            if cat == None:
                cat = self.defaults.getCategory(tree.GetSelection())
            if cat != None:
                events.sendAfter(events.SelectNotify(self.widgetId,
                                                     cat.getLongId()))

        self.refreshPopupMenu()

    def selectAddon(self, addon):
        """Select the item that has been associated with a given addon.

        @param addon The addon to select.
        """
        for hier in [self.available, self.defaults]:
            if hier.items.has_key(addon):
                self.selectItem(hier.items[addon])
                break

    def onRightClick(self, ev):
        """Handle the wxWidgets even when the user clicks on the tree
        using the right mouse button.

        @param ev A wxWidgets event object.
        """
        tree = self.getWxWidget()
        item, flags = tree.HitTest(ev.GetPosition())

        if (flags & wx.TREE_HITTEST_ONITEMLABEL or
            flags & wx.TREE_HITTEST_ONITEMRIGHT):
            # Change the selection.
            self.selectItem(item)

        # Continue with the normal event handling.
        Widget.onRightClick(self, ev)

    def getHierarchy(self, item):
        """Finds the hierarchy where the item is in."""

        for hier in [self.available, self.defaults]:
            if item in hier.categories.values():
                return hier

            if item in hier.items.values():
                return hier

        return None                

    def onItemExpanding(self, ev):
        """Handle the wxWidgets event that is sent when an item is
        about to be expanded.  We will not allow expanding empty
        categories.

        @param ev A wxWidgets event.
        """
        hier = self.getHierarchy(ev.GetItem())

        for item in hier.items.values():
            if self.isItemUnder(item, ev.GetItem()):
                # There is something under the category.
                ev.Skip()
                return

        ev.Veto()        

    def expandAllCategories(self, doExpand):
        """Expand or collapse all categories in the tree."""

        tree = self.getWxWidget()

        # Affects both hierarchy.
        for hier in [self.available, self.defaults]:
            for category in hier.categories.keys():
                item = hier.categories[category]

                if(doExpand):
                    tree.Expand(item)
                else:
                    tree.Collapse(item)

    def checkSelectedCategory(self, doCheck):
        """Check or uncheck all the addons under the currently
        selected category.  Affects all subcategories.

        @param doCheck True if the addons should be checked.  False if
        unchecked.
        """
        tree = self.getWxWidget()
        profile = pr.getActive()

        # This is probably a category item.  We'll need to be careful.
        categoryItem = tree.GetSelection()

        for hier in [self.available, self.defaults]:
            for category in hier.categories.keys():
                if hier.categories[category] == categoryItem:
                    # This is the category we are looking for.  We'll
                    # have to iterate through all the addons.
                    for addon in hier.items.keys():
                        if self.isItemUnder(hier.items[addon],
                                            categoryItem):
                            print addon
                            if doCheck:
                                profile.useAddon(addon)
                            else:
                                profile.dontUseAddon(addon)
                    break
