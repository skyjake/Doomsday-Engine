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

font_t::font_t(fonttype_t type, fontid_t bindId)
{
    DENG_ASSERT(VALID_FONTTYPE(type));

    _type = type;
    _marginWidth  = 0;
    _marginHeight = 0;
    _leading = 0;
    _ascent = 0;
    _descent = 0;
    _noCharSize.width  = 0;
    _noCharSize.height = 0;
    _primaryBind = bindId;
    _isDirty = true;
}

void font_t::glInit()
{}

void font_t::glDeinit()
{}

void font_t::charSize(Size2Raw *size, unsigned char ch)
{
    if(!size) return;
    size->width  = charWidth(ch);
    size->height = charHeight(ch);
}

fonttype_t font_t::type() const
{
    return _type;
}

fontid_t font_t::primaryBind() const
{
    return _primaryBind;
}

void font_t::setPrimaryBind(fontid_t bindId)
{
    _primaryBind = bindId;
}

int font_t::flags() const
{
    return _flags;
}

int font_t::ascent()
{
    glInit();
    return _ascent;
}

int font_t::descent()
{
    glInit();
    return _descent;
}

int font_t::lineSpacing()
{
    glInit();
    return _leading;
}

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

static void *readFormat0(font_t *font, de::FileHandle *file)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP && file);

    int i, c, bitmapFormat, numPels, glyphCount = 0;
    bitmapfont_t *bf = (bitmapfont_t *)font;
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
        bitmapfont_char_t *ch = &bf->_chars[i < MAX_CHARS ? i : MAX_CHARS - 1];
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

static void *readFormat2(font_t *font, de::FileHandle *file)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP && file);

    int i, numPels, dataHeight, glyphCount = 0;
    bitmapfont_t *bf = (bitmapfont_t *)font;
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
        bitmapfont_char_t *ch = &bf->_chars[code];

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

void Font_RebuildFromFile(font_t *font, char const *resourcePath)
{
    if(font->type() != FT_BITMAP)
    {
        Con_Error("Fonts::RebuildFromFile: Font is of invalid type %i.", int(font->type()));
        exit(1); // Unreachable.
    }
    BitmapFont_SetFilePath(font, resourcePath);
}

font_t *Font_FromFile(fontid_t bindId, char const *resourcePath)
{
    DENG2_ASSERT(resourcePath != 0);

    bitmapfont_t *font = new bitmapfont_t(bindId);
    BitmapFont_SetFilePath(font, resourcePath);

    // Lets try and prepare it right away.
    font->glInit();
    return font;
}

bitmapfont_t::bitmapfont_t(fontid_t bindId )
    : font_t(FT_BITMAP, bindId)
{
    _tex = 0;
    _texSize.width = 0;
    _texSize.height = 0;
    Str_Init(&_filePath);
    std::memset(_chars, 0, sizeof(_chars));
}

bitmapfont_t::~bitmapfont_t()
{
    glDeinit();
}

RectRaw const *bitmapfont_t::charGeometry(unsigned char chr)
{
    glInit();
    bitmapfont_char_t *ch = &_chars[chr];
    return &ch->geometry;
}

int bitmapfont_t::charWidth(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.width == 0) return _noCharSize.width;
    return _chars[ch].geometry.size.width;
}

int bitmapfont_t::charHeight(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.height == 0) return _noCharSize.height;
    return _chars[ch].geometry.size.height;
}

void bitmapfont_t::glInit()
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

void bitmapfont_t::glDeinit()
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

void BitmapFont_SetFilePath(font_t *font, char const *filePath)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP);

    bitmapfont_t *bf = (bitmapfont_t *)font;

    if(!filePath || !filePath[0])
    {
        Str_Free(&bf->_filePath);
        font->_isDirty = true;
        return;
    }

    if(bf->_filePath.size > 0)
    {
        if(!Str_CompareIgnoreCase(&bf->_filePath, filePath))
            return;
    }
    else
    {
        Str_Init(&bf->_filePath);
    }
    Str_Set(&bf->_filePath, filePath);
    font->_isDirty = true;
}

DGLuint BitmapFont_GLTextureName(font_t const *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP);
    bitmapfont_t *bf = (bitmapfont_t *)font;
    return bf->_tex;
}

Size2Raw const *BitmapFont_TextureSize(font_t const *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP);
    bitmapfont_t *bf = (bitmapfont_t *)font;
    return &bf->_texSize;
}

int BitmapFont_TextureWidth(font_t const *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP);
    bitmapfont_t *bf = (bitmapfont_t *)font;
    return bf->_texSize.width;
}

int BitmapFont_TextureHeight(font_t const *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP);
    bitmapfont_t *bf = (bitmapfont_t *)font;
    return bf->_texSize.height;
}

void BitmapFont_CharCoords(font_t *font, unsigned char chr, Point2Raw coords[4])
{
    DENG_ASSERT(font->_type == FT_BITMAP);
    bitmapfont_t *bf = (bitmapfont_t *)font;
    bitmapfont_char_t *ch = &bf->_chars[chr];
    if(!coords) return;
    bf->glInit();
    std::memcpy(coords, ch->coords, sizeof(Point2Raw) * 4);
}

bitmapcompositefont_t::bitmapcompositefont_t(fontid_t bindId)
    : font_t(FT_BITMAPCOMPOSITE, bindId)
{
    _def = 0;
    std::memset(_chars, 0, sizeof(_chars));
    _flags |= FF_COLORIZE;
}

bitmapcompositefont_t::~bitmapcompositefont_t()
{
    glDeinit();
}

RectRaw const *bitmapcompositefont_t::charGeometry(unsigned char chr)
{
    glInit();
    bitmapcompositefont_char_t *ch = &_chars[chr];
    return &ch->geometry;
}

int bitmapcompositefont_t::charWidth(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.width == 0) return _noCharSize.width;
    return _chars[ch].geometry.size.width;
}

int bitmapcompositefont_t::charHeight(unsigned char ch)
{
    glInit();
    if(_chars[ch].geometry.size.height == 0) return _noCharSize.height;
    return _chars[ch].geometry.size.height;
}

static inline texturevariantspecification_t &BitmapCompositeFont_CharSpec()
{
    return GL_TextureVariantSpec(TC_UI, TSF_MONOCHROME | TSF_UPSCALE_AND_SHARPEN,
                                 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 0, -3, 0, false, false, false, false);
}

void bitmapcompositefont_t::glInit()
{
    Size2Raw avgSize;
    int numPatches;
    uint i;

    if(!_isDirty) return;
    if(novideo || isDedicated || BusyMode_Active()) return;

    glDeinit();

    avgSize.width = avgSize.height = 0;
    numPatches = 0;

    for(i = 0; i < MAX_CHARS; ++i)
    {
        bitmapcompositefont_char_t *ch = &_chars[i];
        patchid_t patch = ch->patch;
        patchinfo_t info;

        if(0 == patch) continue;

        R_GetPatchInfo(patch, &info);
        std::memcpy(&ch->geometry, &info.geometry, sizeof ch->geometry);

        ch->geometry.origin.x -= _marginWidth;
        ch->geometry.origin.y -= _marginHeight;
        ch->geometry.size.width  += _marginWidth  * 2;
        ch->geometry.size.height += _marginHeight * 2;
        ch->border = 0;

        ch->tex = App_Textures().scheme("Patches").findByUniqueId(patch)
                      .texture().prepareVariant(BitmapCompositeFont_CharSpec());
        if(ch->tex && ch->tex->source() == TEXS_ORIGINAL)
        {
            // Upscale & Sharpen will have been applied.
            ch->border = 1;
        }

        avgSize.width  += ch->geometry.size.width;
        avgSize.height += ch->geometry.size.height;
        ++numPatches;
    }

    if(numPatches)
    {
        avgSize.width  /= numPatches;
        avgSize.height /= numPatches;
    }

    _noCharSize.width  = avgSize.width;
    _noCharSize.height = avgSize.height;

    // We have prepared all patches.
    _isDirty = false;
}

void bitmapcompositefont_t::glDeinit()
{
    if(novideo || isDedicated) return;

    _isDirty = true;
    if(BusyMode_Active()) return;

    for(int i = 0; i < 256; ++i)
    {
        bitmapcompositefont_char_t *ch = &_chars[i];
        if(!ch->tex) continue;
        GL_ReleaseVariantTexture(*ch->tex);
        ch->tex = 0;
    }
}

bitmapcompositefont_t *bitmapcompositefont_t::fromDef(fontid_t bindId, ded_compositefont_t *def) // static
{
    DENG2_ASSERT(def != 0);

    LOG_AS("bitmapcompositefont_t::fromDef");

    bitmapcompositefont_t *font = new bitmapcompositefont_t(bindId);
    font->setDefinition(def);

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        if(!def->charMap[i].path) continue;
        try
        {
            QByteArray path = reinterpret_cast<de::Uri &>(*def->charMap[i].path).resolved().toUtf8();
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, path.constData());
        }
        catch(de::Uri::ResolveError const &er)
        {
            LOG_WARNING(er.asText());
        }
    }

    // Lets try to prepare it right away.
    font->glInit();
    return font;
}

struct ded_compositefont_s *bitmapcompositefont_t::definition() const
{
    return _def;
}

void bitmapcompositefont_t::setDefinition(struct ded_compositefont_s *newDef)
{
    _def = newDef;
}

void bitmapcompositefont_t::rebuildFromDef(ded_compositefont_t *def)
{
    LOG_AS("bitmapcompositefont_t::rebuildFromDef");

    setDefinition(def);
    if(!def) return;

    for(int i = 0; i < def->charMapCount.num; ++i)
    {
        if(!def->charMap[i].path) continue;

        try
        {
            QByteArray path = reinterpret_cast<de::Uri &>(*def->charMap[i].path).resolved().toUtf8();
            BitmapCompositeFont_CharSetPatch(this, def->charMap[i].ch, path.constData());
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_WARNING(er.asText());
        }
    }
}

Texture::Variant *BitmapCompositeFont_CharTexture(font_t *font, unsigned char ch)
{
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    cf->glInit();
    return cf->_chars[ch].tex;
}

patchid_t BitmapCompositeFont_CharPatch(font_t *font, unsigned char ch)
{
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    cf->glInit();
    return cf->_chars[ch].patch;
}

void BitmapCompositeFont_CharSetPatch(font_t *font, unsigned char chr, char const *encodedPatchName)
{
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    bitmapcompositefont_t::bitmapcompositefont_char_t *ch = &cf->_chars[chr];
    ch->patch = R_DeclarePatch(encodedPatchName);
    font->_isDirty = true;
}

uint8_t BitmapCompositeFont_CharBorder(font_t *font, unsigned char chr)
{
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    bitmapcompositefont_t::bitmapcompositefont_char_t *ch = &cf->_chars[chr];
    cf->glInit();
    return ch->border;
}

void BitmapCompositeFont_CharCoords(font_t *font, unsigned char /*chr*/, Point2Raw coords[4])
{
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    if(!coords) return;

    cf->glInit();

    // Top left.
    coords[0].x = 0;
    coords[0].y = 0;

    // Bottom right.
    coords[2].x = 1;
    coords[2].y = 1;

    // Top right.
    coords[1].x = 1;
    coords[1].y = 0;

    // Bottom left.
    coords[3].x = 0;
    coords[3].y = 1;
}
