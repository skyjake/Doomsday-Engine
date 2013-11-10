/** @file bitmapfont.cpp Bitmap Font
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_resource.h"

#include <de/mathutil.h> // M_CeilPow2()
#include <de/memory.h>

using namespace de;

static byte inByte(de::FileHandle *file)
{
    byte b;
    file->read((uint8_t *)&b, sizeof(b));
    return b;
}

static unsigned short inShort(de::FileHandle *file)
{
    ushort s;
    file->read((uint8_t *)&s, sizeof(s));
    return USHORT(s);
}

static void *readFormat0(AbstractFont *font, de::FileHandle *file)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP && file);

    int i, c, bitmapFormat, numPels, glyphCount = 0;
    BitmapFont *bf = (BitmapFont *)font;
    Size2Raw avgSize;
    uint32_t *image;

    font->_flags |= FF_COLORIZE;
    font->_flags &= ~FF_SHADOWED;
    font->_marginWidth = font->_marginHeight = 0;

    // Load in the data.
    bf->_texSize.width  = inShort(file);
    bf->_texSize.height = inShort(file);
    glyphCount = inShort(file);
    VERBOSE2( Con_Printf("readFormat0: Size: %i x %i, with %i chars.\n", bf->_texSize.width, bf->_texSize.height, glyphCount) )

    avgSize.width = avgSize.height = 0;
    for(i = 0; i < glyphCount; ++i)
    {
        BitmapFont::bitmapfont_char_t *ch = &bf->_chars[i < MAX_CHARS ? i : MAX_CHARS - 1];
        ushort x = inShort(file);
        ushort y = inShort(file);
        ushort w = inByte(file);
        ushort h = inByte(file);

        ch->geometry.origin.x = 0;
        ch->geometry.origin.y = 0;
        ch->geometry.size.width  = w - font->_marginWidth *2;
        ch->geometry.size.height = h - font->_marginHeight*2;

        // Top left.
        ch->coords[0].x = x;
        ch->coords[0].y = y;

        // Bottom right.
        ch->coords[2].x = x + w;
        ch->coords[2].y = y + h;

        // Top right.
        ch->coords[1].x = ch->coords[2].x;
        ch->coords[1].y = ch->coords[0].y;

        // Bottom left.
        ch->coords[3].x = ch->coords[0].x;
        ch->coords[3].y = ch->coords[2].y;

        avgSize.width  += ch->geometry.size.width;
        avgSize.height += ch->geometry.size.height;
    }

    font->_noCharSize.width  = avgSize.width  / glyphCount;
    font->_noCharSize.height = avgSize.height / glyphCount;

    // The bitmap.
    bitmapFormat = inByte(file);
    if(bitmapFormat > 0)
    {
        de::Uri uri = App_Fonts().composeUri(App_Fonts().id(font));
        throw Error("readFormat0", QString("Font \"%1\" uses unknown bitmap bitmapFormat %2").arg(uri).arg(bitmapFormat));
    }

    numPels = bf->_texSize.width * bf->_texSize.height;
    image = (uint32_t *) M_Calloc(numPels * sizeof(int));
    for(c = i = 0; i < (numPels + 7) / 8; ++i)
    {
        int bit, mask = inByte(file);

        for(bit = 7; bit >= 0; bit--, ++c)
        {
            if(c >= numPels) break;
            if(mask & (1 << bit))
            {
                image[c] = ~0;
            }
        }
    }

    return image;
}

static void *readFormat2(AbstractFont *font, de::FileHandle *file)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP && file);

    int i, numPels, dataHeight, glyphCount = 0;
    BitmapFont *bf = (BitmapFont *)font;
    byte bitmapFormat = 0;
    uint32_t *image, *ptr;
    Size2Raw avgSize;

    bitmapFormat = inByte(file);
    if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
    {
        Con_Error("FR_ReadFormat2: Bitmap format %i not implemented.\n", bitmapFormat);
    }

    font->_flags |= FF_COLORIZE|FF_SHADOWED;

    // Load in the data.
    bf->_texSize.width = inShort(file);
    dataHeight = inShort(file);
    bf->_texSize.height = M_CeilPow2(dataHeight);
    glyphCount = inShort(file);
    font->_marginWidth = font->_marginHeight = inShort(file);

    font->_leading = inShort(file);
    /*font->_glyphHeight =*/ inShort(file); // Unused.
    font->_ascent = inShort(file);
    font->_descent = inShort(file);

    avgSize.width = avgSize.height = 0;
    for(i = 0; i < glyphCount; ++i)
    {
        ushort code = inShort(file);
        ushort x = inShort(file);
        ushort y = inShort(file);
        ushort w = inShort(file);
        ushort h = inShort(file);
        BitmapFont::bitmapfont_char_t *ch = &bf->_chars[code];

        ch->geometry.origin.x = 0;
        ch->geometry.origin.y = 0;
        ch->geometry.size.width  = w - font->_marginWidth *2;
        ch->geometry.size.height = h - font->_marginHeight*2;

        // Top left.
        ch->coords[0].x = x;
        ch->coords[0].y = y;

        // Bottom right.
        ch->coords[2].x = x + w;
        ch->coords[2].y = y + h;

        // Top right.
        ch->coords[1].x = ch->coords[2].x;
        ch->coords[1].y = ch->coords[0].y;

        // Bottom left.
        ch->coords[3].x = ch->coords[0].x;
        ch->coords[3].y = ch->coords[2].y;

        avgSize.width  += ch->geometry.size.width;
        avgSize.height += ch->geometry.size.height;
    }

    font->_noCharSize.width  = avgSize.width  / glyphCount;
    font->_noCharSize.height = avgSize.height / glyphCount;

    // Read the bitmap.
    numPels = bf->_texSize.width * bf->_texSize.height;
    image = ptr = (uint32_t *) M_Calloc(numPels * 4);
    if(!image) Con_Error("FR_ReadFormat2: Failed on allocation of %lu bytes for font bitmap buffer.\n", (unsigned long) (numPels*4));

    if(bitmapFormat == 0)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte red   = inByte(file);
            byte green = inByte(file);
            byte blue  = inByte(file);
            byte alpha = inByte(file);

            *ptr++ = ULONG(red | (green << 8) | (blue << 16) | (alpha << 24));
        }
    }
    else if(bitmapFormat == 1)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte luminance = inByte(file);
            byte alpha = inByte(file);
            *ptr++ = ULONG(luminance | (luminance << 8) | (luminance << 16) | (alpha << 24));
        }
    }

    return image;
}

BitmapFont::BitmapFont(fontid_t bindId )
    : AbstractFont(FT_BITMAP, bindId)
{
    _tex = 0;
    _texSize.width = 0;
    _texSize.height = 0;
    Str_Init(&_filePath);
    std::memset(_chars, 0, sizeof(_chars));
}

BitmapFont::~BitmapFont()
{
    glDeinit();
}

void BitmapFont::rebuildFromFile(char const *resourcePath)
{
    setFilePath(resourcePath);
}

BitmapFont *BitmapFont::fromFile(fontid_t bindId, char const *resourcePath) // static
{
    DENG2_ASSERT(resourcePath != 0);

    BitmapFont *font = new BitmapFont(bindId);
    font->setFilePath(resourcePath);

    // Lets try and prepare it right away.
    font->glInit();

    return font;
}

RectRaw const *BitmapFont::charGeometry(unsigned char chr)
{
    glInit();
    bitmapfont_char_t *ch = &_chars[chr];
    return &ch->geometry;
}

int BitmapFont::charWidth(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.width == 0) return _noCharSize.width;
    return _chars[ch].geometry.size.width;
}

int BitmapFont::charHeight(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.height == 0) return _noCharSize.height;
    return _chars[ch].geometry.size.height;
}

void BitmapFont::glInit()
{
    void *image = 0;
    int version;

    if(_tex) return; // Already prepared.

    de::FileHandle *file = reinterpret_cast<de::FileHandle *>(F_Open(Str_Text(&_filePath), "rb"));
    if(file)
    {
        glDeinit();

        // Load the font glyph map from the file.
        version = inByte(file);
        switch(version)
        {
        // Original format.
        case 0: image = readFormat0(this, file); break;
        // Enhanced format.
        case 2: image = readFormat2(this, file); break;
        default: break;
        }
        if(!image) return;

        // Upload the texture.
        if(!novideo && !isDedicated)
        {
            LOG_VERBOSE("Uploading GL texture for font \"%s\"...")
                << App_Fonts().composeUri(App_Fonts().id(this));

            _tex = GL_NewTextureWithParams(DGL_RGBA, _texSize.width,
                _texSize.height, (uint8_t const *)image, 0, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }

        M_Free(image);
        App_FileSystem().releaseFile(file->file());
        delete file;
    }
}

void BitmapFont::glDeinit()
{
    if(novideo || isDedicated) return;

    _isDirty = true;
    if(BusyMode_Active()) return;
    if(_tex)
    {
        glDeleteTextures(1, (GLuint const *) &_tex);
    }
    _tex = 0;
}

void BitmapFont::setFilePath(char const *filePath)
{
    if(!filePath || !filePath[0])
    {
        Str_Free(&_filePath);
        _isDirty = true;
        return;
    }

    if(_filePath.size > 0)
    {
        if(!Str_CompareIgnoreCase(&_filePath, filePath))
            return;
    }
    else
    {
        Str_Init(&_filePath);
    }
    Str_Set(&_filePath, filePath);
    _isDirty = true;
}

DGLuint BitmapFont::textureGLName() const
{
    return _tex;
}

Size2Raw const *BitmapFont::textureSize() const
{
    return &_texSize;
}

int BitmapFont::textureWidth() const
{
    return _texSize.width;
}

int BitmapFont::textureHeight() const
{
    return _texSize.height;
}

void BitmapFont::charCoords(unsigned char chr, Point2Raw coords[4])
{
    BitmapFont::bitmapfont_char_t *ch = &_chars[chr];
    if(!coords) return;
    glInit();
    std::memcpy(coords, ch->coords, sizeof(Point2Raw) * 4);
}
