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
import sb.profdb as pr
import settings as st


def uniConv(str):
    """Decode character encoding to Unicode.

    @param str  String in local encoding.
    
    @return Unicode string.
    """
    if host.isWindows():
        # Don't do Unicode on Windows.
        return str
    elif type(str) == unicode:
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
    elif type(str) != unicode:
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

        # A notification is sent when the widget is entered.
        wx.EVT_ENTER_WINDOW(wxWidget, self.onEnterWidget)

        # Listen for focus changes.
        wx.EVT_SET_FOCUS(wxWidget, self.onFocus)

        # Register a notification listener. These notifications are expected
        # to be handled by all widgets.
        events.addNotifyListener(self.onNotify, ['language-changed',
                                                 'focus-request'])
        # Widgets are not automatically listening to commands.
        #events.addCommandListener(self.onCommand)

    def getWxWidget(self):
        """Returns the real wxWidget of this widget."""
        return self.wxWidget

    def addProfileChangeListener(self):
        """Adds a listener callback that listens to changes of the active
        profile. Only those widgets that manage a setting should need this."""
        
        events.addNotifyListener(self.onNotify, ['active-profile-changed'])                                 

    def addValueChangeListener(self):
        """Adds a listener callback that listens to value change notifications
        sent with this widget's identifier."""

        events.addNotifyListener(self.onNotify, 
                                 [self.widgetId + '-value-changed'])

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
        # Send a focus event if a focus identifier has been specified.
        if self.focusId:
            events.send(events.FocusNotify(self.focusId))
        event.Skip()

    def onEnterWidget(self, event):
        """Handle the wxWidget event of entering the window."""

        if self.focusId:
            events.send(events.FocusNotify(self.focusId))
        event.Skip()

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

        #w.SetSize(w.GetBestSize())
        #bestSize = w.GetBestSize()
        #w.SetMinSize((0,bestSize[1]))

        self.updateDefaultSize()

    def updateDefaultSize(self):
        """By default widgets can be scaled horizontally to zero width."""
        w = self.getWxWidget()
        bestSize = w.GetBestSize()
        w.SetMinSize((0, bestSize[1]))

    def resizeToBestSize(self):
        w = self.getWxWidget()
        bestSize = w.GetBestSize()
        w.SetMinSize(bestSize)
        w.SetSize(bestSize)      
        
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

    def freeze(self):
        """Prevent updates from occurring on the screen."""
        self.getWxWidget().Freeze()

    def unfreeze(self):
        """Allow updates to occur."""
        self.getWxWidget().Thaw()
        self.getWxWidget().Refresh()
        

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
            w = self.getWxWidget()
            if event.hasId('active-profile-changed'):
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

            elif event.hasId(self.widgetId + '-value-changed'):
                if event.getValue() == 'yes':
                    w.Set3StateValue(wx.CHK_CHECKED)
                elif event.getValue() == 'no':
                    w.Set3StateValue(wx.CHK_UNCHECKED)

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

        if self.reactToNotify and event.hasId(self.widgetId + '-value-changed'):
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
            if host.isMac():
                # It appears wxWidgets fancy text is broken on the Mac.
                useFancy = False
            else:
                useFancy = True
            
            if useFancy:
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

            else:
                text = initialText.replace('<b>', '')
                text = text.replace('</b>', '')
                text = text.replace('<i>', '')
                text = text.replace('</i>', '')
                text = text.replace('<tt>', '')
                text = text.replace('</tt>', '')

            # Break it up if too long lines detected.
            brokenText = breakLongLines(text, 70)
            
            if useFancy:
                Widget.__init__(self, fancy.StaticFancyText(
                    parent, wxId, uniConv(brokenText)))
            else:
                Widget.__init__(self, wx.StaticText(parent, wxId, 
                                uniConv(brokenText)))

            self.resizeToBestSize()

    def setText(self, formattedText):
        """Set new HTML content into the formatted text widget."""
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
        Widget.onNotify(self, event)

        if self.widgetId:
            if (event.hasId('active-profile-changed') or
                event.hasId(self.widgetId + '-value-changed')):
                self.__getValueFromProfile()

    def onCommand(self, event):
        """Handle the slider reset command."""
        
        Widget.onCommand(self, event)

        if self.widgetId and event.hasId(self.resetCommand):
            pr.getActive().removeValue(self.widgetId)

