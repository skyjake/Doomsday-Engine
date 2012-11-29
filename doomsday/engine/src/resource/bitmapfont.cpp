/**
 * @file bitmapfont.cpp Bitmap Font
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <string.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_resource.h"

#include "m_misc.h" // For M_CeilPow2()

#include <de/memory.h>

void Font_Init(font_t *font, fonttype_t type, fontid_t bindId)
{
    DENG_ASSERT(font && VALID_FONTTYPE(type));

    font->_type = type;
    font->_marginWidth  = 0;
    font->_marginHeight = 0;
    font->_leading = 0;
    font->_ascent = 0;
    font->_descent = 0;
    font->_noCharSize.width  = 0;
    font->_noCharSize.height = 0;
    font->_primaryBind = bindId;
    font->_isDirty = true;
}

fonttype_t Font_Type(font_t const *font)
{
    DENG_ASSERT(font);
    return font->_type;
}

fontid_t Font_PrimaryBind(font_t const *font)
{
    DENG_ASSERT(font);
    return font->_primaryBind;
}

void Font_SetPrimaryBind(font_t *font, fontid_t bindId)
{
    DENG_ASSERT(font);
    font->_primaryBind = bindId;
}

boolean Font_IsPrepared(font_t *font)
{
    DENG_ASSERT(font);
    return !font->_isDirty;
}

int Font_Flags(font_t const *font)
{
    DENG_ASSERT(font);
    return font->_flags;
}

int Font_Ascent(font_t *font)
{
    DENG_ASSERT(font);
    return font->_ascent;
}

int Font_Descent(font_t *font)
{
    DENG_ASSERT(font);
    return font->_descent;
}

int Font_Leading(font_t *font)
{
    DENG_ASSERT(font);
    return font->_leading;
}

static byte inByte(FileHandle *file)
{
    byte b;
    FileHandle_Read(file, (uint8_t *)&b, sizeof(b));
    return b;
}

static unsigned short inShort(FileHandle *file)
{
    unsigned short s;
    FileHandle_Read(file, (uint8_t *)&s, sizeof(s));
    return USHORT(s);
}

static void *readFormat0(font_t *font, FileHandle *file)
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
        char buf[256];
        Uri *uri = Fonts_ComposeUri(Fonts_Id(font));
        AutoStr *uriStr = Uri_ToString(uri);
        Uri_Delete(uri);
        dd_snprintf(buf, 256, "%s", Str_Text(uriStr));
        Con_Error("readFormat: Font \"%s\" uses unknown bitmap bitmapFormat %i.\n", buf, bitmapFormat);
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

static void *readFormat2(font_t *font, FileHandle *file)
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

font_t *BitmapFont_New(fontid_t bindId)
{
    bitmapfont_t *bf = (bitmapfont_t*) M_Malloc(sizeof(*bf));
    if(!bf) Con_Error("BitmapFont::Construct: Failed on allocation of %lu bytes.", (unsigned long) sizeof(*bf));

    bf->_tex = 0;
    bf->_texSize.width = 0;
    bf->_texSize.height = 0;
    Str_Init(&bf->_filePath);
    memset(bf->_chars, 0, sizeof(bf->_chars));
    Font_Init((font_t *)bf, FT_BITMAP, bindId);
    return (font_t *)bf;
}

void BitmapFont_Delete(font_t *font)
{
    BitmapFont_DeleteGLTexture(font);
    M_Free(font);
}

RectRaw const *BitmapFont_CharGeometry(font_t *font, unsigned char chr)
{
    bitmapfont_t* bf = (bitmapfont_t *)font;
    bitmapfont_char_t *ch = &bf->_chars[chr];
    DENG_ASSERT(font->_type == FT_BITMAP);
    return &ch->geometry;
}

int BitmapFont_CharWidth(font_t *font, unsigned char ch)
{
    bitmapfont_t *bf = (bitmapfont_t *)font;
    DENG_ASSERT(font && font->_type == FT_BITMAP);
    if(bf->_chars[ch].geometry.size.width == 0) return font->_noCharSize.width;
    return bf->_chars[ch].geometry.size.width;
}

int BitmapFont_CharHeight(font_t *font, unsigned char ch)
{
    bitmapfont_t *bf = (bitmapfont_t *)font;
    DENG_ASSERT(font && font->_type == FT_BITMAP);
    BitmapFont_Prepare(font);
    if(bf->_chars[ch].geometry.size.height == 0) return font->_noCharSize.height;
    return bf->_chars[ch].geometry.size.height;
}

void BitmapFont_Prepare(font_t *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP);

    bitmapfont_t *bf = (bitmapfont_t *)font;
    FileHandle *file;
    void *image = 0;
    int version;

    if(bf->_tex) return; // Already prepared.

    file = F_Open(Str_Text(&bf->_filePath), "rb");
    if(file)
    {
        BitmapFont_DeleteGLTexture(font);

        // Load the font glyph map from the file.
        version = inByte(file);
        switch(version)
        {
        // Original format.
        case 0: image = readFormat0(font, file); break;
        // Enhanced format.
        case 2: image = readFormat2(font, file); break;
        default: break;
        }
        if(!image) return;

        // Upload the texture.
        if(!novideo && !isDedicated)
        {
            VERBOSE2(
                Uri *uri = Fonts_ComposeUri(Fonts_Id(font));
                AutoStr *path = Uri_ToString(uri);
                Con_Printf("Uploading GL texture for font \"%s\"...\n", Str_Text(path));
                Uri_Delete(uri)
            )

            bf->_tex = GL_NewTextureWithParams2(DGL_RGBA, bf->_texSize.width,
                bf->_texSize.height, (uint8_t const *)image, 0, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }

        M_Free(image);
        F_Delete(file);
    }
}

void BitmapFont_DeleteGLTexture(font_t *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAP);

    bitmapfont_t *bf = (bitmapfont_t *)font;

    if(novideo || isDedicated) return;

    font->_isDirty = true;
    if(BusyMode_Active()) return;
    if(bf->_tex)
    {
        glDeleteTextures(1, (GLuint const *) &bf->_tex);
    }
    bf->_tex = 0;
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
    BitmapFont_Prepare(font);
    memcpy(coords, ch->coords, sizeof(Point2Raw) * 4);
}

font_t *BitmapCompositeFont_New(fontid_t bindId)
{
    bitmapcompositefont_t *cf = (bitmapcompositefont_t*) M_Malloc(sizeof(*cf));
    font_t* font = (font_t *)cf;

    if(!cf) Con_Error("BitmapCompositeFont::Construct: Failed on allocation of %lu bytes.", (unsigned long) sizeof(*cf));

    cf->_def = 0;
    memset(cf->_chars, 0, sizeof(cf->_chars));

    Font_Init(font, FT_BITMAPCOMPOSITE, bindId);
    font->_flags |= FF_COLORIZE;

    return font;
}

void BitmapCompositeFont_Delete(font_t *font)
{
    BitmapCompositeFont_ReleaseTextures(font);
    M_Free(font);
}

RectRaw const *BitmapCompositeFont_CharGeometry(font_t *font, unsigned char chr)
{
    DENG_ASSERT(font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    bitmapcompositefont_char_t *ch = &cf->_chars[chr];
    return &ch->geometry;
}

int BitmapCompositeFont_CharWidth(font_t *font, unsigned char ch)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    if(cf->_chars[ch].geometry.size.width == 0) return font->_noCharSize.width;
    return cf->_chars[ch].geometry.size.width;
}

int BitmapCompositeFont_CharHeight(font_t *font, unsigned char ch)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    if(cf->_chars[ch].geometry.size.height == 0) return font->_noCharSize.height;
    return cf->_chars[ch].geometry.size.height;
}

static inline texturevariantspecification_t *BitmapCompositeFont_CharSpec()
{
    return GL_TextureVariantSpecificationForContext(
                TC_UI, TSF_MONOCHROME | TSF_UPSCALE_AND_SHARPEN, 0, 0, 0,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0, -3, 0, false, false, false, false);
}

void BitmapCompositeFont_Prepare(font_t *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);

    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    Size2Raw avgSize;
    int numPatches;
    uint i;

    if(!font->_isDirty) return;
    if(novideo || isDedicated || BusyMode_Active()) return;

    BitmapCompositeFont_ReleaseTextures(font);

    avgSize.width = avgSize.height = 0;
    numPatches = 0;

    for(i = 0; i < MAX_CHARS; ++i)
    {
        bitmapcompositefont_char_t *ch = &cf->_chars[i];
        patchid_t patch = ch->patch;
        patchinfo_t info;

        if(0 == patch) continue;

        R_GetPatchInfo(patch, &info);
        memcpy(&ch->geometry, &info.geometry, sizeof ch->geometry);

        ch->geometry.origin.x -= font->_marginWidth;
        ch->geometry.origin.y -= font->_marginHeight;
        ch->geometry.size.width  += font->_marginWidth  * 2;
        ch->geometry.size.height += font->_marginHeight * 2;
        ch->border = 0;

        de::Texture *tex = App_Textures()->scheme("Patches").findByUniqueId(patch).texture();
        ch->tex = GL_PrepareTextureVariant(reinterpret_cast<texture_s *>(tex), BitmapCompositeFont_CharSpec());
        if(ch->tex && TextureVariant_Source(ch->tex) == TEXS_ORIGINAL)
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

    font->_noCharSize.width  = avgSize.width;
    font->_noCharSize.height = avgSize.height;

    // We have prepared all patches.
    font->_isDirty = false;
}

void BitmapCompositeFont_ReleaseTextures(font_t *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);

    if(novideo || isDedicated) return;

    font->_isDirty = true;
    if(BusyMode_Active()) return;

    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    for(int i = 0; i < 256; ++i)
    {
        bitmapcompositefont_char_t *ch = &cf->_chars[i];
        if(!ch->tex) continue;
        GL_ReleaseVariantTexture(ch->tex);
        ch->tex = 0;
    }
}

struct ded_compositefont_s *BitmapCompositeFont_Definition(font_t const *font)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    return cf->_def;
}

void BitmapCompositeFont_SetDefinition(font_t *font, struct ded_compositefont_s *def)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    cf->_def = def;
}

TextureVariant *BitmapCompositeFont_CharTexture(font_t *font, unsigned char ch)
{
    DENG_ASSERT(font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    BitmapCompositeFont_Prepare(font);
    return cf->_chars[ch].tex;
}

patchid_t BitmapCompositeFont_CharPatch(font_t *font, unsigned char ch)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    BitmapCompositeFont_Prepare(font);
    return cf->_chars[ch].patch;
}

void BitmapCompositeFont_CharSetPatch(font_t *font, unsigned char chr, char const *patchName)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    bitmapcompositefont_char_t *ch = &cf->_chars[chr];
    ch->patch = R_DeclarePatch(patchName);
    font->_isDirty = true;
}

uint8_t BitmapCompositeFont_CharBorder(font_t *font, unsigned char chr)
{
    DENG_ASSERT(font && font->_type == FT_BITMAPCOMPOSITE);
    bitmapcompositefont_t *cf = (bitmapcompositefont_t *)font;
    bitmapcompositefont_char_t *ch = &cf->_chars[chr];
    BitmapCompositeFont_Prepare(font);
    return ch->border;
}

void BitmapCompositeFont_CharCoords(font_t *font, unsigned char /*chr*/, Point2Raw coords[4])
{
    DENG_ASSERT(font);
    if(!coords) return;

    BitmapCompositeFont_Prepare(font);

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
