/** @file bitmapfont.cpp  Bitmap font resource.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "dd_main.h" // isDedicated
#include "filesys/fs_main.h"
#include "gl/gl_texmanager.h" // GL_NewTextureWithParams
#include "sys_system.h" // novideo
#include "FontManifest"
#include "uri.hh"
#include <de/Log>
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

struct Glyph
{
    Rectanglei posCoords;
    Rectanglei texCoords;
};

DENG2_PIMPL(BitmapFont)
{
    String filePath;        ///< The "archived" version of this font (if any).
    GLuint texGLName;       ///< GL-texture name.
    Vector2ui texMargin;    ///< Margin in pixels.
    Vector2i texDimensions; ///< Texture dimensions in pixels.
    bool needGLInit;

    /// Font metrics.
    int leading;
    int ascent;
    int descent;

    Glyph glyphs[MAX_CHARS];
    Glyph missingGlyph;

    Instance(Public *i)
        : Base(i)
        , texGLName(0)
        , needGLInit(true)
        , leading(0)
        , ascent(0)
        , descent(0)
    {}

    ~Instance()
    {
        self.glDeinit();
    }

    /**
     * Lookup the glyph for the specified character @a ch. If no glyph is defined
     * for this character then the special "missing glyph" is returned instead.
     */
    Glyph &glyph(uchar ch)
    {
        if(ch >= MAX_CHARS) return missingGlyph;
        return glyphs[ch];
    }

    /**
     * Determine the dimensions of the "missing glyph" by averaging the dimensions
     * of the available/defined glyphs.
     *
     * @todo Could be smarter. Presently this treats @em all glyphs equally.
     */
    Vector2ui findMissingGlyphSize()
    {
        Vector2ui accumSize;
        int glyphCount = 0;
        for(int i = 0; i < MAX_CHARS; ++i)
        {
            Glyph &glyph = glyphs[i];
            if(glyph.posCoords.isNull()) continue;

            accumSize += glyph.posCoords.size();
            glyphCount += 1;
        }
        return accumSize / glyphCount;
    }

    uint8_t *readFormat0(de::FileHandle *file)
    {
        DENG2_ASSERT(file != 0);

        self._flags |= AbstractFont::Colorize;
        self._flags &= ~AbstractFont::Shadowed;
        texMargin = Vector2ui(0, 0);

        // Load in the data.
        texDimensions.x = inShort(file);
        texDimensions.y = inShort(file);
        int glyphCount  = inShort(file);

        for(int i = 0; i < glyphCount; ++i)
        {
            Glyph *ch = &glyphs[i < MAX_CHARS ? i : MAX_CHARS - 1];
            ushort x = inShort(file);
            ushort y = inShort(file);
            ushort w = inByte(file);
            ushort h = inByte(file);

            ch->posCoords = Rectanglei::fromSize(Vector2i(0, 0), Vector2ui(w, h));
            ch->texCoords = Rectanglei::fromSize(Vector2i(x, y), Vector2ui(w, h));
        }

        missingGlyph.posCoords.setSize(findMissingGlyphSize());

        int bitmapFormat = inByte(file);
        if(bitmapFormat > 0)
        {
            de::Uri uri = self.manifest().composeUri();
            throw Error("BitmapFont::readFormat0", QString("Font \"%1\" uses unknown format '%2'").arg(uri).arg(bitmapFormat));
        }

        // Read the glyph atlas texture.
        int const numPels = texDimensions.x * texDimensions.y;
        uint32_t *image = (uint32_t *) M_Calloc(numPels * 4);
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

        return (uint8_t *)image;
    }

    uint8_t *readFormat2(de::FileHandle *file)
    {
        DENG2_ASSERT(file != 0);

        self._flags |= AbstractFont::Colorize | AbstractFont::Shadowed;

        int bitmapFormat = inByte(file);
        if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
        {
            de::Uri uri = self.manifest().composeUri();
            throw Error("BitmapFont::readFormat2", QString("Font \"%1\" uses unknown format '%2'").arg(uri).arg(bitmapFormat));
        }

        // Load in the data.
        texDimensions.x = inShort(file);
        texDimensions.y = M_CeilPow2(inShort(file));
        int glyphCount  = inShort(file);
        texMargin.x     = texMargin.y = inShort(file);

        leading = inShort(file);
        /*glyphHeight =*/ inShort(file); // Unused.
        ascent  = inShort(file);
        descent = inShort(file);

        for(int i = 0; i < glyphCount; ++i)
        {
            ushort code = inShort(file);
            DENG2_ASSERT(code < MAX_CHARS);
            Glyph *ch = &glyphs[code];

            ushort x = inShort(file);
            ushort y = inShort(file);
            ushort w = inShort(file);
            ushort h = inShort(file);

            ch->posCoords = Rectanglei::fromSize(Vector2i(0, 0), Vector2ui(w, h) - texMargin * 2);
            ch->texCoords = Rectanglei::fromSize(Vector2i(x, y), Vector2ui(w, h));
        }

        missingGlyph.posCoords.setSize(findMissingGlyphSize());

        // Read the glyph atlas texture.
        int const numPels = texDimensions.x * texDimensions.y;
        uint32_t *image = (uint32_t *) M_Calloc(numPels * 4);
        if(bitmapFormat == 0)
        {
            uint32_t *ptr = image;
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
            uint32_t *ptr = image;
            for(int i = 0; i < numPels; ++i)
            {
                byte luminance = inByte(file);
                byte alpha     = inByte(file);
                *ptr++ = ULONG(luminance | (luminance << 8) | (luminance << 16) | (alpha << 24));
            }
        }

        return (uint8_t *)image;
    }
};

BitmapFont::BitmapFont(FontManifest &manifest)
    : AbstractFont(manifest), d(new Instance(this))
{}

BitmapFont *BitmapFont::fromFile(FontManifest &manifest, String resourcePath) // static
{
    BitmapFont *font = new BitmapFont(manifest);

    font->setFilePath(resourcePath);

    // Lets try and prepare it right away.
    font->glInit();

    return font;
}

int BitmapFont::ascent()
{
    glInit();
    return d->ascent;
}

int BitmapFont::descent()
{
    glInit();
    return d->descent;
}

int BitmapFont::lineSpacing()
{
    glInit();
    return d->leading;
}

Rectanglei const &BitmapFont::glyphPosCoords(uchar ch)
{
    glInit();
    return d->glyph(ch).posCoords;
}

Rectanglei const &BitmapFont::glyphTexCoords(uchar ch)
{
    glInit();
    return d->glyph(ch).texCoords;
}

void BitmapFont::glInit()
{
    LOG_AS("BitmapFont");

    if(!d->needGLInit) return;
    if(novideo || isDedicated || BusyMode_Active()) return;

    glDeinit();

    try
    {
        // Relative paths are relative to the native working directory.
        String path = (NativePath::workPath() / NativePath(d->filePath).expand()).withSeparators('/');
        de::FileHandle *hndl = &App_FileSystem().openFile(path, "rb");

        int format = inByte(hndl);

        uint8_t *pixels = 0;
        switch(format)
        {
        // Original format.
        case 0: pixels = d->readFormat0(hndl); break;
        // Enhanced format.
        case 2: pixels = d->readFormat2(hndl); break;

        default:
            DENG2_ASSERT(!"BitmapFont: Format not implemented");
        }
        if(!pixels) return;

        // Upload the texture.
        if(!novideo && !isDedicated)
        {
            LOG_DEBUG("Uploading atlas texture for \"%s\"...")
                << manifest().composeUri();

            d->texGLName = GL_NewTextureWithParams(DGL_RGBA, d->texDimensions.x, d->texDimensions.y,
                pixels, 0, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }

        M_Free(pixels);
        App_FileSystem().releaseFile(hndl->file());
        delete hndl;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.

    d->needGLInit = false;
}

void BitmapFont::glDeinit()
{
    LOG_AS("BitmapFont");

    if(novideo || isDedicated) return;

    d->needGLInit = true;
    if(BusyMode_Active()) return;

    if(d->texGLName)
    {
        glDeleteTextures(1, (GLuint const *) &d->texGLName);
    }
    d->texGLName = 0;
}

void BitmapFont::setFilePath(String newFilePath)
{
    if(d->filePath != newFilePath)
    {
        d->filePath = newFilePath;
        d->needGLInit = true;
    }
}

GLuint BitmapFont::textureGLName() const
{
    return d->texGLName;
}

Vector2i const &BitmapFont::textureDimensions() const
{
    return d->texDimensions;
}

Vector2ui const &BitmapFont::textureMargin() const
{
    return d->texMargin;
}
