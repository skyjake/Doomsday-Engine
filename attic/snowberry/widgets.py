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

import sys, os.path
import wx
import wx.html
import wx.lib.intctrl as intctrl
import wx.lib.scrolledpanel as scrolled
import wx.lib.fancytext as fancy
import host, events, ui, language, paths
import sb.profdb as pr
#import sb.confdb as st
import sb.widget.base


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


# The image list for all the icons used in the UI.
iconManager = None


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
            if language.isDefined(identifier):
                imageName = language.translate(identifier)
            else:
                imageName = identifier
            fileName = paths.findBitmap(imageName)
            if len(fileName) == 0:
                # Fallback icon.
                fileName = paths.findBitmap('deng')
            bmp = wx.Bitmap(fileName)
            self.imageList.Add(bmp)
            self.bitmaps.append(identifier)
            return len(self.bitmaps) - 1
            
    def getBitmapPath(self, identifier, scaledSize=None):
        """Locates a bitmap and scales it, if necessary.
        
        @param identifier  Bitmap identifier.
        @param scaledSize  Tuple (width, height). If specified, determines 
                           the size of the image. If this does not match the
                           size of the existing image on disk, a new image 
                           file will be created.
        
        @return  Path of the image file.
        """
        if scaledSize is None:
            return paths.findBitmap(identifier)
        
        # A scaled size has been specified.
        # Let's generate the scaled name of the bitmap.
        ident = 'scaled%ix%i-' % scaledSize + identifier

        # Try to locate an existing copy of the image.
        imagePath = paths.findBitmap(ident)
        
        if imagePath != '':
            return imagePath

        # We have to generate it. First try loading the original image.
        originalPath = paths.findBitmap(identifier)
        if originalPath == '':
            # No such image.
            return ''

        # Scale and save. PNG should work well with all source images.
        image = wx.Image(originalPath)
        originalSize = (image.GetWidth(), image.GetHeight())
        if originalSize[0] == scaledSize[0] and originalSize[1] == scaledSize[1]:
            # No need to scale.
            return originalPath
        if scaledSize[0] < originalSize[0] or scaledSize[1] < originalSize[1]:
            # Upscale first to cause some extra blurring.
            image.Rescale(originalSize[0]*2, originalSize[1]*2, wx.IMAGE_QUALITY_HIGH)
        image.Rescale(scaledSize[0], scaledSize[1], wx.IMAGE_QUALITY_HIGH)
        imagePath = os.path.join(paths.getUserPath(paths.GRAPHICS), ident + '.png')
        image.SaveFile(imagePath, wx.BITMAP_TYPE_PNG)
                              
        return imagePath


class Line (sb.widget.base.Widget):
    """A static horizontal divider line."""

    def __init__(self, parent):
        """Construct a new divider line.

        @param parent The parent panel.
        """
        sb.widget.base.Widget.__init__(self, 
            wx.StaticLine(parent, -1, style=wx.LI_HORIZONTAL))


class Image (sb.widget.base.Widget):
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
        sb.widget.base.Widget.__init__(self, self.panel)

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
