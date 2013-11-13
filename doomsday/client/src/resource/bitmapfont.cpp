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

#include "de_platform.h"
#include "resource/bitmapfont.h"

#include "dd_main.h"
#include "filesys/fs_main.h"
#include "gl/gl_texmanager.h"
#include "sys_system.h" // novideo
#include "uri.hh"
#include <de/mathutil.h> // M_CeilPow2()
#include <de/memory.h>

using namespace de;

static byte inByte(de::FileHandle *file)
{
    byte b;
    file->read((uint8_t *)&b, sizeof(b));
    return b;
}

static ushort inShort(de::FileHandle *file)
{
    ushort s;
    file->read((uint8_t *)&s, sizeof(s));
    return USHORT(s);
}

DENG2_PIMPL(BitmapFont)
{
    ddstring_t filePath;    ///< The "archived" version of this font (if any).
    GLuint texGLName;       ///< GL-texture name.
    Vector2i texDimensions; ///< Texture dimensions in pixels.
    bool needGLInit;

    bitmapfont_char_t glyphs[MAX_CHARS];
    bitmapfont_char_t missingGlyph;

    Instance(Public *i)
        : Base(i)
        , texGLName(0)
        , needGLInit(true)
    {
        Str_Init(&filePath);
        zap(glyphs);
        zap(missingGlyph);
    }

    ~Instance()
    {
        self.glDeinit();
    }

    bitmapfont_char_t &glyph(uchar ch)
    {
        if(ch >= MAX_CHARS) return missingGlyph;
        return glyphs[ch];
    }

    void *readFormat0(de::FileHandle *file)
    {
        DENG2_ASSERT(file != 0);

        self._flags |= FF_COLORIZE;
        self._flags &= ~FF_SHADOWED;
        self._margin = Vector2ui(0, 0);

        // Load in the data.
        texDimensions.x = inShort(file);
        texDimensions.y = inShort(file);
        int glyphCount  = inShort(file);

        Vector2ui avgSize;
        for(int i = 0; i < glyphCount; ++i)
        {
            bitmapfont_char_t *ch = &glyphs[i < MAX_CHARS ? i : MAX_CHARS - 1];
            ushort x = inShort(file);
            ushort y = inShort(file);
            ushort w = inByte(file);
            ushort h = inByte(file);

            ch->geometry = Rectanglei::fromSize(Vector2i(0, 0), Vector2ui(w, h) - self._margin * 2);
            ch->coords   = Rectanglei::fromSize(Vector2i(x, y), Vector2ui(w, h));

            avgSize += ch->geometry.size();
        }

        missingGlyph.geometry.setSize(avgSize / glyphCount);

        // Load the bitmap.
        int bitmapFormat = inByte(file);
        if(bitmapFormat > 0)
        {
            de::Uri uri = App_Fonts().composeUri(App_Fonts().id(thisPublic));
            throw Error("BitmapFont::readFormat0", QString("Font \"%1\" uses unknown format '%2'").arg(uri).arg(bitmapFormat));
        }

        int numPels = texDimensions.x * texDimensions.y;
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

        int bitmapFormat = inByte(file);
        if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
        {
            de::Uri uri = App_Fonts().composeUri(App_Fonts().id(thisPublic));
            throw Error("BitmapFont::readFormat2", QString("Font \"%1\" uses unknown format '%2'").arg(uri).arg(bitmapFormat));
        }

        self._flags |= FF_COLORIZE|FF_SHADOWED;

        // Load in the data.
        texDimensions.x = inShort(file);
        texDimensions.y = M_CeilPow2(inShort(file));
        int glyphCount  = inShort(file);
        self._margin.x  = self._margin.y = inShort(file);

        self._leading = inShort(file);
        /*glyphHeight =*/ inShort(file); // Unused.
        self._ascent  = inShort(file);
        self._descent = inShort(file);

        Vector2ui avgSize;
        for(int i = 0; i < glyphCount; ++i)
        {
            ushort code = inShort(file);
            ushort x    = inShort(file);
            ushort y    = inShort(file);
            ushort w    = inShort(file);
            ushort h    = inShort(file);
            bitmapfont_char_t *ch = &glyphs[code];

            ch->geometry = Rectanglei::fromSize(Vector2i(0, 0), self._margin * 2 - Vector2ui(w, h));
            ch->coords   = Rectanglei::fromSize(Vector2i(x, y), Vector2ui(w, h));

            avgSize += ch->geometry.size();
        }

        missingGlyph.geometry.setSize(avgSize / glyphCount);

        // Read the bitmap.
        int numPels = texDimensions.x * texDimensions.y;
        uint32_t *image = (uint32_t *) M_Calloc(numPels * 4);
        uint32_t *ptr = image;

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
                byte alpha     = inByte(file);
                *ptr++ = ULONG(luminance | (luminance << 8) | (luminance << 16) | (alpha << 24));
            }
        }

        return image;
    }
};

BitmapFont::BitmapFont(fontid_t bindId) : AbstractFont(), d(new Instance(this))
{
    setPrimaryBind(bindId);
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

Rectanglei const &BitmapFont::charPosCoords(uchar ch)
{
    glInit();
    return d->glyph(ch).geometry;
}

Rectanglei const &BitmapFont::charTexCoords(uchar ch)
{
    glInit();
    return d->glyph(ch).coords;
}

void BitmapFont::glInit()
{
    if(d->texGLName) return; // Already prepared.

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

            d->texGLName = GL_NewTextureWithParams(DGL_RGBA, d->texDimensions.x, d->texDimensions.y,
                (uint8_t const *)image, 0, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
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

    d->needGLInit = true;
    if(BusyMode_Active()) return;
    if(d->texGLName)
    {
        glDeleteTextures(1, (GLuint const *) &d->texGLName);
    }
    d->texGLName = 0;
}

void BitmapFont::setFilePath(char const *newFilePath)
{
    if(!newFilePath || !newFilePath[0])
    {
        Str_Free(&d->filePath);
        d->needGLInit = true;
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
    d->needGLInit = true;
}

GLuint BitmapFont::textureGLName() const
{
    return d->texGLName;
}

Vector2i const &BitmapFont::textureDimensions() const
{
    return d->texDimensions;
}
