/**\file bitmapfont.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <string.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_system.h"
#include "de_filesys.h"

#include "m_misc.h"
#include "fonts.h"

#include "bitmapfont.h"

void Font_Init(font_t* font, fonttype_t type, fontid_t bindId)
{
    assert(font && VALID_FONTTYPE(type));

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

fonttype_t Font_Type(const font_t* font)
{
    assert(font);
    return font->_type;
}

fontid_t Font_PrimaryBind(const font_t* font)
{
    assert(font);
    return font->_primaryBind;
}

void Font_SetPrimaryBind(font_t* font, fontid_t bindId)
{
    assert(font);
    font->_primaryBind = bindId;
}

boolean Font_IsPrepared(font_t* font)
{
    assert(font);
    return !font->_isDirty;
}

int Font_Flags(const font_t* font)
{
    assert(font);
    return font->_flags;
}

int Font_Ascent(font_t* font)
{
    assert(font);
    return font->_ascent;
}

int Font_Descent(font_t* font)
{
    assert(font);
    return font->_descent;
}

int Font_Leading(font_t* font)
{
    assert(font);
    return font->_leading;
}

static byte inByte(DFile* file)
{
    byte b;
    DFile_Read(file, (uint8_t*)&b, sizeof(b));
    return b;
}

static unsigned short inShort(DFile* file)
{
    unsigned short s;
    DFile_Read(file, (uint8_t*)&s, sizeof(s));
    return USHORT(s);
}

static void* readFormat0(font_t* font, DFile* file)
{
    int i, c, bitmapFormat, numPels, glyphCount = 0;
    bitmapfont_t* bf = (bitmapfont_t*)font;
    Size2Raw avgSize;
    uint32_t* image;
    assert(font && font->_type == FT_BITMAP && file);

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
        bitmapfont_char_t* ch = &bf->_chars[i < MAX_CHARS ? i : MAX_CHARS - 1];

        ch->geometry.origin.x = inShort(file);
        ch->geometry.origin.y = inShort(file);
        ch->geometry.size.width  = inByte(file);
        ch->geometry.size.height = inByte(file);

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
        Uri* uri = Fonts_ComposeUri(Fonts_Id(font));
        ddstring_t* uriStr = Uri_ToString(uri);
        Uri_Delete(uri);
        dd_snprintf(buf, 256, "%s", Str_Text(uriStr));
        Str_Delete(uriStr);
        Con_Error("readFormat: Font \"%s\" uses unknown bitmap bitmapFormat %i.\n", buf, bitmapFormat);
    }

    numPels = bf->_texSize.width * bf->_texSize.height;
    image = calloc(1, numPels * sizeof(int));
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

static void* readFormat2(font_t* font, DFile* file)
{
    int i, numPels, dataHeight, glyphCount = 0;
    bitmapfont_t* bf = (bitmapfont_t*)font;
    byte bitmapFormat = 0;
    uint32_t* image, *ptr;
    Size2Raw avgSize;
    assert(font && font->_type == FT_BITMAP && file);

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

        if(code < MAX_CHARS)
        {
            bf->_chars[code].geometry.origin.x = x;
            bf->_chars[code].geometry.origin.y = y;
            bf->_chars[code].geometry.size.width  = w;
            bf->_chars[code].geometry.size.height = h;
        }


        avgSize.width  += bf->_chars[code].geometry.size.width;
        avgSize.height += bf->_chars[code].geometry.size.height;
    }

    font->_noCharSize.width  = avgSize.width  / glyphCount;
    font->_noCharSize.height = avgSize.height / glyphCount;

    // Read the bitmap.
    numPels = bf->_texSize.width * bf->_texSize.height;
    image = ptr = (uint32_t*)calloc(1, numPels * 4);
    if(!image)
        Con_Error("FR_ReadFormat2: Failed on allocation of %lu bytes for font bitmap buffer.\n", (unsigned long) (numPels*4));

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

font_t* BitmapFont_New(fontid_t bindId)
{
    bitmapfont_t* bf = (bitmapfont_t*)malloc(sizeof *bf);
    if(!bf)
        Con_Error("BitmapFont::Construct: Failed on allocation of %lu bytes.", (unsigned long) sizeof *bf);

    bf->_tex = 0;
    bf->_texSize.width = 0;
    bf->_texSize.height = 0;
    Str_Init(&bf->_filePath);
    memset(bf->_chars, 0, sizeof(bf->_chars));
    Font_Init((font_t*)bf, FT_BITMAP, bindId);
    return (font_t*)bf;
}

void BitmapFont_Delete(font_t* font)
{
    BitmapFont_DeleteGLTexture(font);
    free(font);
}

int BitmapFont_CharWidth(font_t* font, unsigned char ch)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);
    if(bf->_chars[ch].geometry.size.width == 0) return font->_noCharSize.width;
    return bf->_chars[ch].geometry.size.width - font->_marginWidth * 2;
}

int BitmapFont_CharHeight(font_t* font, unsigned char ch)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);
    BitmapFont_Prepare(font);
    if(bf->_chars[ch].geometry.size.height == 0) return font->_noCharSize.height;
    return bf->_chars[ch].geometry.size.height - font->_marginHeight * 2;
}

void BitmapFont_Prepare(font_t* font)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    DFile* file;
    void* image = 0;
    int version;
    assert(font && font->_type == FT_BITMAP);

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
                Uri* uri = Fonts_ComposeUri(Fonts_Id(font));
                ddstring_t* path = Uri_ToString(uri);
                Con_Printf("Uploading GL texture for font \"%s\"...\n", Str_Text(path));
                Str_Delete(path);
                Uri_Delete(uri)
            )

            bf->_tex = GL_NewTextureWithParams2(DGL_RGBA, bf->_texSize.width,
                bf->_texSize.height, image, 0, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }

        free(image);
        F_Delete(file);
    }
}

void BitmapFont_DeleteGLTexture(font_t* font)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);

    if(novideo || isDedicated) return;

    font->_isDirty = true;
    if(Con_IsBusy()) return;
    if(bf->_tex)
        glDeleteTextures(1, (const GLuint*) &bf->_tex);
    bf->_tex = 0;
}

void BitmapFont_SetFilePath(font_t* font, const char* filePath)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);

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

DGLuint BitmapFont_GLTextureName(const font_t* font)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);
    return bf->_tex;
}

const Size2Raw* BitmapFont_TextureSize(const font_t* font)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);
    return &bf->_texSize;
}

int BitmapFont_TextureWidth(const font_t* font)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);
    return bf->_texSize.width;
}

int BitmapFont_TextureHeight(const font_t* font)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);
    return bf->_texSize.height;
}

void BitmapFont_CharCoords(font_t* font, int* s0, int* s1,
    int* t0, int* t1, unsigned char ch)
{
    bitmapfont_t* bf = (bitmapfont_t*)font;
    assert(font && font->_type == FT_BITMAP);
    if(!s0 && !s1 && !t0 && !t1) return;
    BitmapFont_Prepare(font);
    if(s0) *s0 = bf->_chars[ch].geometry.origin.x;
    if(s0) *s1 = bf->_chars[ch].geometry.origin.x + bf->_chars[ch].geometry.size.width;
    if(t0) *t0 = bf->_chars[ch].geometry.origin.y;
    if(t1) *t1 = bf->_chars[ch].geometry.origin.y + bf->_chars[ch].geometry.size.height;
}

font_t* BitmapCompositeFont_New(fontid_t bindId)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)malloc(sizeof *cf);
    font_t* font = (font_t*)cf;

    if(!cf)
        Con_Error("BitmapCompositeFont::Construct: Failed on allocation of %lu bytes.", (unsigned long) sizeof *cf);

    cf->_def = 0;
    memset(cf->_chars, 0, sizeof(cf->_chars));

    Font_Init(font, FT_BITMAPCOMPOSITE, bindId);
    font->_flags |= FF_COLORIZE;

    return font;
}

void BitmapCompositeFont_Delete(font_t* font)
{
    BitmapCompositeFont_DeleteGLTextures(font);
    free(font);
}

int BitmapCompositeFont_CharWidth(font_t* font, unsigned char ch)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    assert(font && font->_type == FT_BITMAPCOMPOSITE);
    if(cf->_chars[ch].geometry.size.width == 0) return font->_noCharSize.width;
    return cf->_chars[ch].geometry.size.width - font->_marginWidth * 2 - 2;
}

int BitmapCompositeFont_CharHeight(font_t* font, unsigned char ch)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    assert(font && font->_type == FT_BITMAPCOMPOSITE);
    if(cf->_chars[ch].geometry.size.height == 0) return font->_noCharSize.height;
    return cf->_chars[ch].geometry.size.height - font->_marginHeight * 2 - 2;
}

static __inline texturevariantspecification_t* BitmapCompositeFont_CharSpec(void)
{
    return GL_TextureVariantSpecificationForContext(
                TC_UI, TSF_MONOCHROME | TSF_UPSCALE_AND_SHARPEN, 0, 0, 0,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0, 1, 0, false, false, false, false);
}

void BitmapCompositeFont_Prepare(font_t* font)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    Size2Raw avgSize;
    int numPatches;
    uint i;
    assert(font && font->_type == FT_BITMAPCOMPOSITE);

    if(!font->_isDirty) return;
    if(novideo || isDedicated || Con_IsBusy()) return;

    BitmapCompositeFont_DeleteGLTextures(font);

    avgSize.width = avgSize.height = 0;
    numPatches = 0;

    for(i = 0; i < MAX_CHARS; ++i)
    {
        bitmapcompositefont_char_t* ch = &cf->_chars[i];
        patchid_t patch = ch->patch;
        patchinfo_t info;
        texture_t* tex;

        if(0 == patch) continue;

        ++numPatches;

        R_GetPatchInfo(patch, &info);
        memcpy(&ch->geometry, &info.geometry, sizeof ch->geometry);

        ch->geometry.origin.x += -1 + font->_marginWidth;
        ch->geometry.origin.y += -1 + font->_marginHeight;
        ch->geometry.size.width  += 2;
        ch->geometry.size.height += 2;

        avgSize.width  += ch->geometry.size.width;
        avgSize.height += ch->geometry.size.height;

        tex = Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, patch));
        ch->tex = GL_PrepareTexture(tex, BitmapCompositeFont_CharSpec());
    }

    font->_noCharSize.width  = avgSize.width  / numPatches;
    font->_noCharSize.height = avgSize.height / numPatches;

    // We have prepared all patches.
    font->_isDirty = false;
}

void BitmapCompositeFont_DeleteGLTextures(font_t* font)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    int i;
    assert(font && font->_type == FT_BITMAPCOMPOSITE);

    if(novideo || isDedicated) return;

    font->_isDirty = true;
    if(Con_IsBusy()) return;

    for(i = 0; i < 256; ++i)
    {
        bitmapcompositefont_char_t* ch = &cf->_chars[i];
        texture_t* tex;

        if(!ch->patch) continue;
        tex = Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, ch->patch));
        assert(tex);

        GL_ReleaseVariantTexturesBySpec(tex, BitmapCompositeFont_CharSpec());
        ch->tex = 0;
    }
}

struct ded_compositefont_s* BitmapCompositeFont_Definition(const font_t* font)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    assert(font && font->_type == FT_BITMAPCOMPOSITE);
    return cf->_def;
}

void BitmapCompositeFont_SetDefinition(font_t* font, struct ded_compositefont_s* def)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    assert(font && font->_type == FT_BITMAPCOMPOSITE);
    cf->_def = def;
}

DGLuint BitmapCompositeFont_CharGLTexture(font_t* font, unsigned char ch)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    assert(font->_type == FT_BITMAPCOMPOSITE);

    BitmapCompositeFont_Prepare(font);
    return cf->_chars[ch].tex;
}

patchid_t BitmapCompositeFont_CharPatch(font_t* font, unsigned char ch)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    assert(font && font->_type == FT_BITMAPCOMPOSITE);
    BitmapCompositeFont_Prepare(font);
    return cf->_chars[ch].patch;
}

void BitmapCompositeFont_CharSetPatch(font_t* font, unsigned char chr, const char* patchName)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    bitmapcompositefont_char_t* ch = &cf->_chars[chr];
    assert(font && font->_type == FT_BITMAPCOMPOSITE);

    ch->patch = R_DeclarePatch(patchName);
    font->_isDirty = true;
}

void BitmapCompositeFont_CharCoords(font_t* font, int* s0, int* s1,
    int* t0, int* t1, unsigned char ch)
{
    if(!s0 && !s1 && !t0 && !t1) return;
    BitmapCompositeFont_Prepare(font);
    if(s0) *s0 = 0;
    if(s0) *s1 = 1;
    if(t0) *t0 = 0;
    if(t1) *t1 = 1;
}
