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

## @file text.py Text Widgets

import sys
import wx
import wx.html
import wx.lib.fancytext as fancy
import host, events, language, paths
import sb.confdb as st
import base
from widgets import uniConv

# Workaround for a wxWidgets StaticText bug.
if host.isMac():
    from wx.lib.stattext import GenStaticText as StaticText
else:
    from wx import StaticText

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
                brokenText += "\n"
                lineLen = 0
        else:
            brokenText += text[i]

    return brokenText + text[-1]   


class MyStaticText (StaticText):
    def __init__(self, parent, wxId, label, style=0):
        StaticText.__init__(self, parent, wxId, label, style=style)
        if host.isWindows():
            # No background.
            self.SetBackgroundStyle(wx.BG_STYLE_SYSTEM)
        wx.EVT_ERASE_BACKGROUND(self, self.clearBack)

    def clearBack(self, event):
        pass


class Text (base.Widget):
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
            
        base.Widget.__init__(self, MyStaticText(parent, wxId,
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


class FormattedText (base.Widget):
    def __init__(self, parent, wxId, useHtmlFormatting=True, initialText=''):
        """Constructor.

        @param parent  Parent wxWidget ID.
        @param wxId    ID of the formatted text widget.
        @param useHtmlFormatting  Use a HTML window widget.
        @param initialText  Initial contents.
        """

        self.useHtml = useHtmlFormatting
        
        if self.useHtml:
            base.Widget.__init__(self, MyHtmlWindow(parent, wxId))
        else:
            if host.isMac():
                # It appears wxWidgets fancy text is broken on the Mac.
                useFancy = True #False
            else:
                useFancy = True
            
            if useFancy:
                text = initialText.replace('<b>', '<font weight="bold">')
                text = text.replace('</b>', '</font>')
                text = text.replace('<i>', '<font style="italic">')
                text = text.replace('</i>', '</font>')
                text = text.replace('<tt>', '<font family="fixed">')
                text = text.replace('</tt>', '</font>')

                text = '<font family="swiss" size="%s">' \
                       % st.getSystemString('style-normal-size') + text + '</font>'

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
                base.Widget.__init__(self, fancy.StaticFancyText(
                    parent, wxId, uniConv(brokenText)))
            else:
                base.Widget.__init__(self, StaticText(parent, wxId, 
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
