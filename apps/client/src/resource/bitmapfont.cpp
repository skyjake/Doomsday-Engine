/** @file bitmapfont.cpp  Bitmap font resource.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "gl/gl_main.h" // GL_NewTextureWithParams
#include "sys_system.h" // novideo
#include "resource/fontmanifest.h"
#include <doomsday/uri.h>
#include <de/logbuffer.h>
#include <de/legacy/mathutil.h> // M_CeilPow2()
#include <de/legacy/memory.h>
#include <doomsday/busymode.h>
#include <doomsday/filesys/fs_main.h>

using namespace de;
using namespace res;

static byte inByte(FileHandle *file)
{
    byte b;
    file->read((uint8_t *)&b, sizeof(b));
    return b;
}

static ushort inShort(FileHandle *file)
{
    ushort s;
    file->read((uint8_t *)&s, sizeof(s));
    return DD_USHORT(s);
}

struct Glyph
{
    Rectanglei posCoords;
    Rectanglei texCoords;
};

DE_PIMPL(BitmapFont)
{
    String filePath;        ///< The "archived" version of this font (if any).
    GLuint texGLName;       ///< GL-texture name.
    Vec2ui texMargin;    ///< Margin in pixels.
    Vec2i texDimensions; ///< Texture dimensions in pixels.
    bool needGLInit;

    /// Font metrics.
    int leading;
    int ascent;
    int descent;

    Glyph glyphs[MAX_CHARS];
    Glyph missingGlyph;

    Impl(Public *i)
        : Base(i)
        , texGLName(0)
        , needGLInit(true)
        , leading(0)
        , ascent(0)
        , descent(0)
    {}

    ~Impl()
    {
        self().glDeinit();
    }

    /**
     * Lookup the glyph for the specified character @a ch. If no glyph is defined
     * for this character then the special "missing glyph" is returned instead.
     */
    Glyph &glyph(dbyte ch)
    {
        //if(ch >= MAX_CHARS) return missingGlyph;
        return glyphs[ch];
    }

    /**
     * Determine the dimensions of the "missing glyph" by averaging the dimensions
     * of the available/defined glyphs.
     *
     * @todo Could be smarter. Presently this treats @em all glyphs equally.
     */
    Vec2ui findMissingGlyphSize()
    {
        Vec2ui accumSize;
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

    uint8_t *readFormat0(FileHandle *file)
    {
        DE_ASSERT(file != 0);

        self()._flags |= AbstractFont::Colorize;
        self()._flags &= ~AbstractFont::Shadowed;
        texMargin = Vec2ui(0, 0);

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

            ch->posCoords = Rectanglei::fromSize(Vec2i(0, 0), Vec2ui(w, h));
            ch->texCoords = Rectanglei::fromSize(Vec2i(x, y), Vec2ui(w, h));
        }

        missingGlyph.posCoords.setSize(findMissingGlyphSize());

        int bitmapFormat = inByte(file);
        if(bitmapFormat > 0)
        {
            res::Uri uri = self().manifest().composeUri();
            throw Error(
                "BitmapFont::readFormat0",
                stringf("Font \"%s\" uses unknown format '%i'", uri.pathCStr(), bitmapFormat));
        }

        // Read the glyph atlas texture.
        const int numPels = texDimensions.x * texDimensions.y;
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

    uint8_t *readFormat2(FileHandle *file)
    {
        DE_ASSERT(file != 0);

        self()._flags |= AbstractFont::Colorize | AbstractFont::Shadowed;

        int bitmapFormat = inByte(file);
        if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
        {
            res::Uri uri = self().manifest().composeUri();
            throw Error(
                "BitmapFont::readFormat2",
                stringf("Font \"%s\" uses unknown format '%i'", uri.pathCStr(), bitmapFormat));
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
            DE_ASSERT(code < MAX_CHARS);
            Glyph *ch = &glyphs[code];

            ushort x = inShort(file);
            ushort y = inShort(file);
            ushort w = inShort(file);
            ushort h = inShort(file);

            ch->posCoords = Rectanglei::fromSize(Vec2i(0, 0), Vec2ui(w, h) - texMargin * 2);
            ch->texCoords = Rectanglei::fromSize(Vec2i(x, y), Vec2ui(w, h));
        }

        missingGlyph.posCoords.setSize(findMissingGlyphSize());

        // Read the glyph atlas texture.
        const int numPels = texDimensions.x * texDimensions.y;
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

                *ptr++ = DD_ULONG(red | (green << 8) | (blue << 16) | (alpha << 24));
            }
        }
        else if(bitmapFormat == 1)
        {
            uint32_t *ptr = image;
            for(int i = 0; i < numPels; ++i)
            {
                byte luminance = inByte(file);
                byte alpha     = inByte(file);
                *ptr++ = DD_ULONG(luminance | (luminance << 8) | (luminance << 16) | (alpha << 24));
            }
        }

        return (uint8_t *)image;
    }
};

BitmapFont::BitmapFont(FontManifest &manifest)
    : AbstractFont(manifest), d(new Impl(this))
{}

BitmapFont *BitmapFont::fromFile(FontManifest &manifest, String resourcePath) // static
{
    BitmapFont *font = new BitmapFont(manifest);

    font->setFilePath(resourcePath);

    // Lets try and prepare it right away.
    font->glInit();

    return font;
}

int BitmapFont::ascent() const
{
    glInit();
    return d->ascent;
}

int BitmapFont::descent() const
{
    glInit();
    return d->descent;
}

int BitmapFont::lineSpacing() const
{
    glInit();
    return d->leading;
}

const Rectanglei &BitmapFont::glyphPosCoords(dbyte ch) const
{
    glInit();
    return d->glyph(ch).posCoords;
}

const Rectanglei &BitmapFont::glyphTexCoords(dbyte ch) const
{
    glInit();
    return d->glyph(ch).texCoords;
}

void BitmapFont::glInit() const
{
    LOG_AS("BitmapFont");

    if(!d->needGLInit) return;
    if(novideo || BusyMode_Active()) return;

    glDeinit();

    try
    {
        // Relative paths are relative to the native working directory.
        String path = (NativePath::workPath() / NativePath(d->filePath).expand()).withSeparators('/');
        FileHandle *hndl = &App_FileSystem().openFile(path, "rb");

        int format = inByte(hndl);

        uint8_t *pixels = 0;
        switch(format)
        {
        // Original format.
        case 0: pixels = d->readFormat0(hndl); break;
        // Enhanced format.
        case 2: pixels = d->readFormat2(hndl); break;

        default:
            DE_ASSERT_FAIL("BitmapFont: Format not implemented");
        }
        if(!pixels)
        {
            App_FileSystem().releaseFile(hndl->file());
            delete hndl;
            return;
        }

        // Upload the texture.
        if(!novideo && !isDedicated)
        {
            LOG_GL_XVERBOSE("Uploading atlas texture for \"%s\"...",
                            manifest().composeUri());

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

void BitmapFont::glDeinit() const
{
    LOG_AS("BitmapFont");

    if(novideo || isDedicated) return;

    d->needGLInit = true;
    if(BusyMode_Active()) return;

    if(d->texGLName)
    {
        Deferred_glDeleteTextures(1, (const GLuint *) &d->texGLName);
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

uint BitmapFont::textureGLName() const
{
    return d->texGLName;
}

const Vec2i &BitmapFont::textureDimensions() const
{
    return d->texDimensions;
}

const Vec2ui &BitmapFont::textureMargin() const
{
    return d->texMargin;
}
