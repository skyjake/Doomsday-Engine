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

DENG2_PIMPL(BitmapFont)
{
    ddstring_t filePath; ///< The "archived" version of this font (if any).
    DGLuint tex;         ///< GL-texture name.
    Size2Raw texSize;    ///< Texture dimensions in pixels.

    /// Character map.
    bitmapfont_char_t chars[MAX_CHARS];

    Instance(Public *i)
        : Base(i)
        , tex(0)
        , texSize(0, 0)
    {
        Str_Init(&filePath);
        zap(chars);
    }

    ~Instance()
    {
        self.glDeinit();
    }

    void *readFormat0(de::FileHandle *file)
    {
        DENG2_ASSERT(file != 0);

        self._flags |= FF_COLORIZE;
        self._flags &= ~FF_SHADOWED;
        self._marginWidth = self._marginHeight = 0;

        // Load in the data.
        texSize.width  = inShort(file);
        texSize.height = inShort(file);
        int glyphCount = inShort(file);
        VERBOSE2( Con_Printf("readFormat0: Size: %i x %i, with %i chars.\n",
                             texSize.width, texSize.height, glyphCount) )

        Size2Raw avgSize;
        for(int i = 0; i < glyphCount; ++i)
        {
            bitmapfont_char_t *ch = &chars[i < MAX_CHARS ? i : MAX_CHARS - 1];
            ushort x = inShort(file);
            ushort y = inShort(file);
            ushort w = inByte(file);
            ushort h = inByte(file);

            ch->geometry.origin.x = 0;
            ch->geometry.origin.y = 0;
            ch->geometry.size.width  = w - self._marginWidth *2;
            ch->geometry.size.height = h - self._marginHeight*2;

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

        self._noCharSize.width  = avgSize.width  / glyphCount;
        self._noCharSize.height = avgSize.height / glyphCount;

        // Load the bitmap.
        int bitmapFormat = inByte(file);
        if(bitmapFormat > 0)
        {
            de::Uri uri = App_Fonts().composeUri(App_Fonts().id(thisPublic));
            throw Error("readFormat0", QString("Font \"%1\" uses unknown bitmap bitmapFormat %2").arg(uri).arg(bitmapFormat));
        }

        int numPels = texSize.width * texSize.height;
        uint32_t *image = (uint32_t *) M_Calloc(numPels * sizeof(int));
        int c, i;
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

    void *readFormat2(de::FileHandle *file)
    {
        DENG2_ASSERT(file != 0);

        byte bitmapFormat = inByte(file);
        if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
        {
            Con_Error("FR_ReadFormat2: Bitmap format %i not implemented.\n", bitmapFormat);
        }

        self._flags |= FF_COLORIZE|FF_SHADOWED;

        // Load in the data.
        texSize.width  = inShort(file);
        texSize.height = M_CeilPow2(inShort(file));
        int glyphCount = inShort(file);
        self._marginWidth = self._marginHeight = inShort(file);

        self._leading = inShort(file);
        /*glyphHeight =*/ inShort(file); // Unused.
        self._ascent  = inShort(file);
        self._descent = inShort(file);

        Size2Raw avgSize;
        for(int i = 0; i < glyphCount; ++i)
        {
            ushort code = inShort(file);
            ushort x    = inShort(file);
            ushort y    = inShort(file);
            ushort w    = inShort(file);
            ushort h    = inShort(file);
            bitmapfont_char_t *ch = &chars[code];

            ch->geometry.origin.x = 0;
            ch->geometry.origin.y = 0;
            ch->geometry.size.width  = w - self._marginWidth *2;
            ch->geometry.size.height = h - self._marginHeight*2;

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

        self._noCharSize.width  = avgSize.width  / glyphCount;
        self._noCharSize.height = avgSize.height / glyphCount;

        // Read the bitmap.
        int numPels = texSize.width * texSize.height;
        uint32_t *ptr = (uint32_t *) M_Calloc(numPels * 4);
        uint32_t *image = ptr;

        if(bitmapFormat == 0)
        {
            for(int i = 0; i < numPels; ++i)
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
            for(int i = 0; i < numPels; ++i)
            {
                byte luminance = inByte(file);
                byte alpha = inByte(file);
                *ptr++ = ULONG(luminance | (luminance << 8) | (luminance << 16) | (alpha << 24));
            }
        }

        return image;
    }
};

BitmapFont::BitmapFont(fontid_t bindId )
    : AbstractFont(FT_BITMAP, bindId), d(new Instance(this))
{}

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
    return &d->chars[chr].geometry;
}

int BitmapFont::charWidth(unsigned char ch)
{
    glInit();
    if(d->chars[ch].geometry.size.width == 0) return _noCharSize.width;
    return d->chars[ch].geometry.size.width;
}

int BitmapFont::charHeight(unsigned char ch)
{
    glInit();
    if(d->chars[ch].geometry.size.height == 0) return _noCharSize.height;
    return d->chars[ch].geometry.size.height;
}

void BitmapFont::glInit()
{
    if(d->tex) return; // Already prepared.

    de::FileHandle *file = reinterpret_cast<de::FileHandle *>(F_Open(Str_Text(&d->filePath), "rb"));
    if(file)
    {
        glDeinit();

        // Load the font glyph map from the file.
        int version = inByte(file);

        void *image;
        switch(version)
        {
        // Original format.
        case 0: image = d->readFormat0(file); break;
        // Enhanced format.
        case 2: image = d->readFormat2(file); break;

        default:
            DENG2_ASSERT(false);
        }
        if(!image) return;

        // Upload the texture.
        if(!novideo && !isDedicated)
        {
            LOG_VERBOSE("Uploading GL texture for font \"%s\"...")
                << App_Fonts().composeUri(App_Fonts().id(this));

            d->tex = GL_NewTextureWithParams(DGL_RGBA, d->texSize.width,
                d->texSize.height, (uint8_t const *)image, 0, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
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
    if(d->tex)
    {
        glDeleteTextures(1, (GLuint const *) &d->tex);
    }
    d->tex = 0;
}

void BitmapFont::setFilePath(char const *newFilePath)
{
    if(!newFilePath || !newFilePath[0])
    {
        Str_Free(&d->filePath);
        _isDirty = true;
        return;
    }

    if(d->filePath.size > 0)
    {
        if(!Str_CompareIgnoreCase(&d->filePath, newFilePath))
            return;
    }
    else
    {
        Str_Init(&d->filePath);
    }
    Str_Set(&d->filePath, newFilePath);
    _isDirty = true;
}

DGLuint BitmapFont::textureGLName() const
{
    return d->tex;
}

Size2Raw const *BitmapFont::textureSize() const
{
    return &d->texSize;
}

int BitmapFont::textureWidth() const
{
    return d->texSize.width;
}

int BitmapFont::textureHeight() const
{
    return d->texSize.height;
}

void BitmapFont::charCoords(unsigned char chr, Point2Raw coords[4])
{
    bitmapfont_char_t *ch = &d->chars[chr];
    if(!coords) return;
    glInit();
    std::memcpy(coords, ch->coords, sizeof(Point2Raw) * 4);
}
