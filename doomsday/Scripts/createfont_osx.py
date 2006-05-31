#!/usr/local/bin/python
"""
EXPERIMENTAL CODE!

Here we load a .TTF font file, and display it in
a basic pygame window. It demonstrates several of the
Font object attributes. Nothing exciting in here, but
it makes a great example for basic window, event, and
font management.
"""

import sys, struct
import pygame
import math
from pygame.locals import *
from pygame import Surface
from pygame.surfarray import blit_array, make_surface, pixels3d, pixels2d
import Numeric

from Foundation import *
from AppKit import *


# Version 1 is the monochrome one.
DFN_VERSION = 2


def _getColor(color=None):
    if color is None:
        return NSColor.clearColor()
    return NSColor.colorWithDeviceRed_green_blue_alpha_(*color)
    #div255 = (0.00390625).__mul__
    #if len(color) == 3:
    #    color = tuple(color) + (255.0,)
    #return NSColor.colorWithDeviceRed_green_blue_alpha_(*color) #*map(div255, color))


def _getBitmapImageRep(size, hasAlpha=True):
    width, height = map(int, size)
    return NSBitmapImageRep.alloc().initWithBitmapDataPlanes_pixelsWide_pixelsHigh_bitsPerSample_samplesPerPixel_hasAlpha_isPlanar_colorSpaceName_bytesPerRow_bitsPerPixel_(None, width, height, 8, 4, hasAlpha, False, NSDeviceRGBColorSpace, width*4, 32)


def alpha_blend(src, dest):
    a = src[3]/255.0
    inv = 1.0  - a
    #return src
    #if src[3] == 0:
    #return src
    #print src[3], a, inv, int(a * src[3] + inv * dest[3])
    return ( min(255, int(a * 1 * src[0] + inv * dest[0])),
             min(255, int(a * 1 * src[1] + inv * dest[1])),
             min(255, int(a * 1 * src[2] + inv * dest[2])),
             min(255, int(a * src[3] + inv * dest[3])) )


def filter_range(n):
    offset = 1 # - int(n/5)
    return range(-n/2 + offset, n/2 + offset)


class SysFont(object):
    def __init__(self, name, size, variant=None):
        self._font = NSFont.fontWithName_size_(name, size)
        self._isBold = False
        self._isOblique = False
        self._isUnderline = False
        self._variant = variant
        self._family = name
        self._size = size
        self._setupFont()

    def _setupFont(self):
        name = self._family
        if self._variant:
            name = '%s-%s' % (name, self._variant)
        elif self._isBold or self._isOblique:
            name = '%s-%s%s' % (
                name,
                self._isBold and 'Bold' or '',
                self._isOblique and 'Oblique' or '')
        self._font = NSFont.fontWithName_size_(name, self._size)
        print name, self._font
        if self._font is None:
            if self._isBold:
                self._font = NSFont.boldSystemFontOfSize(self._size)
            else:
                self._font = NSFont.systemFontOfSize_(self._size)
        
    def get_ascent(self):
        return self._font.ascender()

    def get_descent(self):
        return -self._font.descender()

    def get_bold(self):
        return self._isBold

    def get_height(self):
        return self._font.defaultLineHeightForFont()

    def get_italic(self):
        return self._isOblique

    def get_linesize(self):
        pass

    def get_underline(self):
        return self._isUnderline

    def set_bold(self, isBold):
        if isBold != self._isBold:
            self._isBold = isBold
            self._setupFont()

    def set_italic(self, isOblique):
        if isOblique != self._isOblique:
            self._isOblique = isOblique
            self._setupFont()

    def set_underline(self, isUnderline):
        self._isUnderline = isUnderline
        
    def size(self, text):
        return tuple(map(int,map(math.ceil, NSString.sizeWithAttributes_(text, {
            NSFontAttributeName: self._font,
            NSUnderlineStyleAttributeName: self._isUnderline and 1.0 or 0,
        }))))
        
    def advancement(self, glyph):
        return tuple(self._font.advancementForGlyph_(ord(glyph)))

    def shadow_filter_size(self):
        filter_size = int(self.get_height() / 5)
        if filter_size % 2 == 0:
            filter_size += 1
        return int(max(filter_size, 3))

    def border_size(self):
        return self.shadow_filter_size()/2
    
    def render(self, text, antialias = True, 
               forecolor=(255, 255, 255, 255), 
               backcolor=(255, 255, 255, 0)):
        size = self.size(text)
        img = NSImage.alloc().initWithSize_(size)
        img.lockFocus()

        NSString.drawAtPoint_withAttributes_(text, (0.0, 0.0), {
            NSFontAttributeName: self._font,
            NSUnderlineStyleAttributeName: self._isUnderline and 1.0 or 0,
            NSBackgroundColorAttributeName: _getColor(backcolor),# or None,
            NSForegroundColorAttributeName: _getColor(forecolor),
        })

        rep = NSBitmapImageRep.alloc().initWithFocusedViewRect_(((0.0, 0.0), size))
        img.unlockFocus()
        if rep.samplesPerPixel() == 4:
            s = Surface(size, SRCALPHA|SWSURFACE, 32, [-1<<24,0xff<<16,0xff<<8,0xff])
            
            a = Numeric.reshape(Numeric.fromstring(rep.bitmapData(), typecode=Numeric.Int32), (size[1], size[0]))
            blit_array(s, Numeric.swapaxes(a, 0, 1))
            #print "surface size is ", size

            filter_size = self.shadow_filter_size()
            border = self.border_size()
            
            result = []
            result_size = (size[0] + border*2, size[1] + border*2)
            #print "result size is ", result_size
            for y in range(result_size[1]):
                result.append([(0, 0, 0, 0)] * result_size[0])
            
            # Filter the character appropriately.
            s.lock()
            for y in range(size[1]):
                for x in range(size[0]):
                    color = s.get_at((x, y))
                    s.set_at((x, y), (255, 255, 255, color[3]))

            if filter_size == 3:
                factors = [[1, 2, 1],
                           [2, 4, 2],
                           [1, 2, 1]]
            elif filter_size == 5:
                factors = [[1,  4,  6,  4,  1],
                           [4,  16, 24, 16, 4],
                           [6,  24, 36, 24, 6],
                           [4,  16, 24, 16, 4],
                           [1,  4,  6,  4,  1]]
            elif filter_size == 7:
                factors = [[1,  6,   15,  20,  15,  6,   1],
                           [6,  36,  90,  120, 90,  36,  6],
                           [15, 90,  225, 300, 225, 90,  15],
                           [20, 120, 300, 400, 300, 120, 20],
                           [15, 90,  225, 300, 225, 90,  15],
                           [6,  36,  90,  120, 90,  36,  6],
                           [1,  6,   15,  20,  15,  6,   1]]
            else:
                print "factors for filter size", filter_size, "not defined!"
                factors = 0

            # Filter a shadow.
            for ry in range(result_size[1]):
                for rx in range(result_size[0]):
                    inpos = (rx - int(border), ry - int(border))
                    count = 0
                    colorsum = 0
                    u = 0
                    v = 0
                    for a in filter_range(filter_size):
                        v = 0
                        for b in filter_range(filter_size):
                            x, y = (inpos[0] + a, inpos[1] + b)
                            factor = factors[u][v]
                            if x >= 0 and y >= 0 and x < size[0] and y < size[1]:
                                colorsum += factor * s.get_at((x, y))[3]
                            count += factor
                            v += 1
                        u += 1
                    if count > 0:
                        result_color = (int(colorsum) + count/2) / count
                    else:
                        result_color = 0
                    result[ry][rx] = (0, 0, 0, min(255, result_color*1.4))

            # Blend the glyph itself to the result.
            for y in range(size[1]):
                for x in range(size[0]):
                    color = s.get_at((x, y))
                    result[border + y][border + x] = \
                        alpha_blend(color, result[border + y][border + x])
                    result[border + y][border + x] = \
                        alpha_blend(color, result[border + y][border + x])
                    #result[border + y][border + x] = \
                    #    alpha_blend(color, result[border + y][border + x])

            s.unlock()
                    
            glyph_rgba = Numeric.array(result, typecode=Numeric.UnsignedInt8)
            return glyph_rgba, result_size
                
            #return s.convert_alpha()


class Glyph:
    def __init__(self, code):
        self.code = code
        self.bitmap = None
        self.pos = (0, 0)
        self.dims = (0, 0)


def pow2(n):
    i = 1
    while i < n: 
        i *= 2
    return i
    
    
def find_pixel(glyphs, x, y):
    for g in glyphs:
        lx = x - g.pos[0]
        ly = y - g.pos[1]
        if lx >= 0 and ly >= 0 and lx < g.dims[0] and ly < g.dims[1]:
            # This is the glyph.
            return g.bitmap[ly][lx]
    return (0, 0, 0, 0)


if __name__=='__main__':
    if len(sys.argv) < 5:
        print "usage: " + sys.argv[0] + " (font-name) (font-size) (texture-width) (output-file)"
        sys.exit(0)

    out_filename = sys.argv[4]
   
    pygame.init()
    #screen = pygame.display.set_mode((600, 600))
    point_size = int(sys.argv[2])
    s = SysFont(sys.argv[1], point_size)
    #s = SysFont('Andale Mono', 24)
    #done = False

    # Collect all the glyphs here.
    glyphs = []
    
    texture_width = int(sys.argv[3])
    font_point_height = s.get_height()
    print "Font texture width =", texture_width
    print "Line height =", font_point_height, "pt"
    print "Font size =", point_size, "pt"
    
    ascent = s.get_ascent()
    descent = s.get_descent()
    print "Ascent =", ascent, "pt  Descent =", descent, "pt"
    
    print "Shadow filter is sized %i for this font." % s.shadow_filter_size()
    border_size = s.border_size()
    print "Glyph borders are %i pixels wide." % border_size
    
    rover = (0, 0)
    line_height = 0
    texture_height = 0
    used_pixels = 0
    
    # Only the basic ASCII set, and some specific characters.
    char_range = range(0x20, 0x7F) + [0x96, 0x97, 0xA3, 0xA4, 0xA9, 0xB0, 0xB1]
    for code in char_range: 
        glyph = Glyph(code)
        try:
            glyph.bitmap, glyph.dims = s.render(chr(code))
        
            used_pixels += glyph.dims[0] * glyph.dims[1]
        
            line_height = max(line_height, glyph.dims[1])
            texture_height = max(line_height, texture_height)
        
            # Does if fit here?
            if rover[0] + glyph.dims[0] > texture_width:
                # No, move to the new row.
                rover = (0, rover[1] + line_height)
                texture_height += line_height
                print "Finished a %i pixels tall line." % line_height
                line_height = 0
            
            glyph.pos = rover
        
            # Move the rover.
            rover = (rover[0] + glyph.dims[0], rover[1])
        
            #print "Code %i: Placement =" % code, glyph.pos, " dims =", glyph.dims
            glyphs.append(glyph)
        #except Exception, x:
        except KeyboardInterrupt:
            print "Code %i: Failed to render." % code
            print x
        
    print "Finished rendering %i glyphs." % len(glyphs)
    actual_line_height = glyphs[0].dims[1] - border_size*2
    print "Actual line height is %i pixels." % actual_line_height
    
    print "Size of the finished texture is %i x %i." % (texture_width, texture_height)
        
    actual_height = pow2(texture_height)
    total_pixels = texture_width * actual_height
    
    print "Unused texture area: %.2f %%" % (100*(1.0 - used_pixels/float(total_pixels)))
    print "Writing output to file: %s" % out_filename
    
    out = file(out_filename, 'wb')
    
    # The DFN format begins with a single byte that declares the version 
    # of the format.
    out.write(struct.pack('B', DFN_VERSION))
    
    # The header follows:
    # Pixel format (0 = RGBA, 1 = LA).
    out.write(struct.pack('B', 0)) 
    # Glyph count, texture width and height (unsigned shorts).
    out.write(struct.pack('<HHH', texture_width, texture_height, len(glyphs)))
    # Border width and line height.
    out.write(struct.pack('<HH', border_size, actual_line_height))

    # Scaling factor for converting points to pixels.
    scale = float(actual_line_height) / font_point_height
    pixel_ascent = int(math.ceil(scale * ascent))
    pixel_descent = int(math.ceil(scale * descent))
    print "    Pixel ascent/descent:", pixel_ascent, '/', pixel_descent
    
    pixel_line_height = int(scale * font_point_height)
    print "    Pixel line height:", pixel_line_height
    
    # Ascent and descent.
    out.write(struct.pack('<HHH', pixel_line_height, pixel_ascent, pixel_descent))
    
    # Glyph metrics.
    for g in glyphs:
        out.write(struct.pack('<HHHHH', g.code, g.pos[0], g.pos[1], 
                              g.dims[0], g.dims[1]))

    # Glyph map.
    print "    Writing glyph map..."
    for y in range(texture_height):
        for x in range(texture_width):
            pixel = find_pixel(glyphs, x, y)
            # Use the LA format.
            #out.write(struct.pack('BB', pixel[0], pixel[3]))
            # Use the RGBA format.
            out.write(struct.pack('BBBB', pixel[0], pixel[1], pixel[2], pixel[3]))
    
    out.close()
    