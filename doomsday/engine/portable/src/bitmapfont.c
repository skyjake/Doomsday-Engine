/**\file bitmapfont.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
#include "de_refresh.h"
#include "de_render.h"
#include "de_system.h"
#include "de_filesys.h"

#include "m_misc.h"
#include "fonts.h"

#include "bitmapfont.h"

void Font_Init(font_t* font, fonttype_t type)
{
    assert(NULL != font && VALID_FONTTYPE(type));

    font->_type = type;

    font->_marginWidth  = 0;
    font->_marginHeight = 0;
    font->_leading = 0;
    font->_ascent = 0;
    font->_descent = 0;
    font->_noCharWidth  = 0;
    font->_noCharHeight = 0;
    font->_bindId = 0;
    font->_isDirty = true;
}

boolean Font_IsPrepared(font_t* font)
{
    assert(NULL != font);
    return !font->_isDirty;
}

fonttype_t Font_Type(const font_t* font)
{
    assert(NULL != font);
    return font->_type;
}

int Font_Flags(const font_t* font)
{
    assert(NULL != font);
    return font->_flags;
}

uint Font_BindId(const font_t* font)
{
    assert(NULL != font);
    return font->_bindId;
}

void Font_SetBindId(font_t* font, uint bindId)
{
    assert(NULL != font);
    font->_bindId = bindId;
}

int Font_Ascent(font_t* font)
{
    assert(NULL != font);
    return font->_ascent;
}

int Font_Descent(font_t* font)
{
    assert(NULL != font);
    return font->_descent;
}

int Font_Leading(font_t* font)
{
    assert(NULL != font);
    return font->_leading;
}

static void drawCharacter(unsigned char ch, font_t* font)
{
    int s[2], t[2], x, y, w, h;

    switch(Font_Type(font))
    {
    case FT_BITMAP:  {
        bitmapfont_t* bf = (bitmapfont_t*)font;

        s[0] = bf->_chars[ch].x;
        s[1] = bf->_chars[ch].x + bf->_chars[ch].w;
        t[0] = bf->_chars[ch].y;
        t[1] = bf->_chars[ch].y + bf->_chars[ch].h;

        x = y = 0;
        w = s[1] - s[0];
        h = t[1] - t[0];
        break;
      }
    case FT_BITMAPCOMPOSITE:  {
        bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;

        if(ch != 0)
        {
            x = cf->_chars[ch].x;
            y = cf->_chars[ch].y;
        }
        else
        {
            x = y = 0;
        }
        w = BitmapCompositeFont_CharWidth(font, ch);
        h = BitmapCompositeFont_CharHeight(font, ch);
        if(cf->_chars[ch].patch != 0)
        {
            w += 2;
            h += 2;
        }

        s[0] = 0;
        s[1] = 1;
        t[0] = 0;
        t[1] = 1;
        break;
      }
    default:
        Con_Error("FR::DrawCharacter: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }

    x -= font->_marginWidth;
    y -= font->_marginHeight;

    glBegin(GL_QUADS);
        // Upper left.
        glTexCoord2i(s[0], t[0]);
        glVertex2f(x, y);

        // Upper Right.
        glTexCoord2i(s[1], t[0]);
        glVertex2f(x + w, y);

        // Lower right.
        glTexCoord2i(s[1], t[1]);
        glVertex2f(x + w, y + h);

        // Lower left.
        glTexCoord2i(s[0], t[1]);
        glVertex2f(x, y + h);
    glEnd();
}

static byte inByte(streamfile_t* sf)
{
    assert(sf);
    {
    byte b;
    F_Read(sf, (uint8_t*)&b, sizeof(b));
    return b;
    }
}

static unsigned short inShort(streamfile_t* sf)
{
    assert(sf);
    {
    unsigned short s;
    F_Read(sf, (uint8_t*)&s, sizeof(s));
    return USHORT(s);
    }
}

static void* readFormat0(font_t* font, streamfile_t* sf)
{
    assert(font != NULL && font->_type == FT_BITMAP && NULL != sf);
    {
    int i, c, bitmapFormat, numPels, avgWidth, avgHeight, glyphCount = 0;
    bitmapfont_t* bf = (bitmapfont_t*)font;
    uint32_t* image;

    font->_flags |= FF_COLORIZE;
    font->_flags &= ~FF_SHADOWED;
    font->_marginWidth = font->_marginHeight = 0;

    // Load in the data.
    bf->_texWidth  = inShort(sf);
    bf->_texHeight = inShort(sf);
    glyphCount = inShort(sf);
    VERBOSE2( Con_Printf("readFormat: Dimensions %i x %i, with %i chars.\n",
                         bf->_texWidth, bf->_texHeight, glyphCount) );

    avgWidth = avgHeight = 0;
    for(i = 0; i < glyphCount; ++i)
    {
        bitmapfont_char_t* ch = &bf->_chars[i < MAX_CHARS ? i : MAX_CHARS - 1];

        ch->x = inShort(sf);
        ch->y = inShort(sf);
        ch->w = inByte(sf);
        ch->h = inByte(sf);

        avgWidth  += ch->w;
        avgHeight += ch->h;
    }

    font->_noCharWidth  = avgWidth  / glyphCount;
    font->_noCharHeight = avgHeight / glyphCount;

    // The bitmap.
    bitmapFormat = inByte(sf);
    if(bitmapFormat > 0)
    {
        Con_Error("readFormat: Font %s uses unknown bitmap bitmapFormat %i.\n",
                  Str_Text(Fonts_GetSymbolicName(font)), bitmapFormat);
    }

    numPels = bf->_texWidth * bf->_texHeight;
    image = calloc(1, numPels * sizeof(int));
    for(c = i = 0; i < (numPels + 7) / 8; ++i)
    {
        int bit, mask = inByte(sf);

        for(bit = 7; bit >= 0; bit--, ++c)
        {
            if(c >= numPels)
                break;
            if(mask & (1 << bit))
                image[c] = ~0;
        }
    }

    return image;
    }
}

static void* readFormat2(font_t* font, streamfile_t* sf)
{
    assert(font != NULL && font->_type == FT_BITMAP && NULL != sf);
    {
    int i, numPels, dataHeight, avgWidth, avgHeight, glyphCount = 0;
    bitmapfont_t* bf = (bitmapfont_t*)font;
    byte bitmapFormat = 0;
    uint32_t* image, *ptr;

    bitmapFormat = inByte(sf);
    if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
    {
        Con_Error("FR_ReadFormat2: Bitmap format %i not implemented.\n", bitmapFormat);
    }

    font->_flags |= FF_COLORIZE|FF_SHADOWED;

    // Load in the data.
    bf->_texWidth = inShort(sf);
    dataHeight = inShort(sf);
    bf->_texHeight = M_CeilPow2(dataHeight);
    glyphCount = inShort(sf);
    font->_marginWidth = font->_marginHeight = inShort(sf);

    font->_leading = inShort(sf);
    /*font->_glyphHeight =*/ inShort(sf); // Unused.
    font->_ascent = inShort(sf);
    font->_descent = inShort(sf);

    avgWidth = avgHeight = 0;
    for(i = 0; i < glyphCount; ++i)
    {
        ushort code = inShort(sf);
        ushort x = inShort(sf);
        ushort y = inShort(sf);
        ushort w = inShort(sf);
        ushort h = inShort(sf);

        if(code < MAX_CHARS)
        {
            bf->_chars[code].x = x;
            bf->_chars[code].y = y;
            bf->_chars[code].w = w;
            bf->_chars[code].h = h;
        }


        avgWidth  += bf->_chars[code].w;
        avgHeight += bf->_chars[code].h;
    }

    font->_noCharWidth  = avgWidth  / glyphCount;
    font->_noCharHeight = avgHeight / glyphCount;

    // Read the bitmap.
    numPels = bf->_texWidth * bf->_texHeight;
    image = ptr = (uint32_t*)calloc(1, numPels * 4);
    if(!image)
        Con_Error("FR_ReadFormat2: Failed on allocation of %lu bytes for font bitmap buffer.\n", (unsigned long) (numPels*4));

    if(bitmapFormat == 0)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte red   = inByte(sf);
            byte green = inByte(sf);
            byte blue  = inByte(sf);
            byte alpha = inByte(sf);

            *ptr++ = ULONG(red | (green << 8) | (blue << 16) | (alpha << 24));
        }
    }
    else if(bitmapFormat == 1)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte luminance = inByte(sf);
            byte alpha = inByte(sf);
            *ptr++ = ULONG(luminance | (luminance << 8) | (luminance << 16) |
                      (alpha << 24));
        }
    }

    return image;
    }
}

font_t* BitmapFont_New(void)
{
    bitmapfont_t* bf = (bitmapfont_t*)malloc(sizeof(*bf));
    if(NULL == bf)
        Con_Error("BitmapFont::Construct: Failed on allocation of %lu bytes.", (unsigned long) sizeof(*bf));

    bf->_tex = 0;
    bf->_texWidth = 0;
    bf->_texHeight = 0;
    Str_Init(&bf->_filePath);
    memset(bf->_chars, 0, sizeof(bf->_chars));
    Font_Init((font_t*)bf, FT_BITMAP);
    return (font_t*)bf;
}

void BitmapFont_Delete(font_t* font)
{
    BitmapFont_DeleteGLTexture(font);
    BitmapFont_DeleteGLDisplayLists(font);
    free(font);
}

int BitmapFont_CharWidth(font_t* font, unsigned char ch)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    if(bf->_chars[ch].w == 0) return font->_noCharWidth;
    return bf->_chars[ch].w - font->_marginWidth * 2;
    }
}

int BitmapFont_CharHeight(font_t* font, unsigned char ch)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    BitmapFont_Prepare(font);
    if(bf->_chars[ch].h == 0) return font->_noCharHeight;
    return bf->_chars[ch].h - font->_marginHeight * 2;
    }
}

void BitmapFont_Prepare(font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    abstractfile_t* file;
    void* image = 0;
    int version;

    if(bf->_tex)
        return; // Already prepared.

    file = F_Open(Str_Text(&bf->_filePath), "rb");
    if(NULL != file)
    {
        streamfile_t* sf = AbstractFile_Handle(file);
        assert(sf);

        BitmapFont_DeleteGLDisplayLists(font);
        BitmapFont_DeleteGLTexture(font);

        // Load the font glyph map from the file.
        version = inByte(sf);
        switch(version)
        {
        // Original format.
        case 0: image = readFormat0(font, sf); break;
        // Enhanced format.
        case 2: image = readFormat2(font, sf); break;
        default: break;
        }
        if(!image)
            return;

        // Upload the texture.
        if(!novideo && !isDedicated)
        {
            VERBOSE2(
                Uri* uri = Fonts_GetUri(font);
                ddstring_t* path = Uri_ToString(uri);
                Con_Printf("Uploading GL texture for font \"%s\"...\n", Str_Text(path));
                Str_Delete(path);
                Uri_Delete(uri)
            )

            bf->_tex = GL_NewTextureWithParams2(DGL_RGBA, bf->_texWidth,
                bf->_texHeight, image, 0, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            if(!Con_IsBusy()) // Cannot do this while in busy mode.
            {
                GLuint base = glGenLists(MAX_CHARS);
                glListBase(base);
                { uint i;
                for(i = 0; i < MAX_CHARS; ++i)
                {
                    glNewList(base+i, GL_COMPILE);
                    drawCharacter((unsigned char)i, font);
                    glEndList();
                    bf->_chars[i].dlist = base+i;
                }}

                // All preparation complete.
                font->_isDirty = false;
            }
        }

        free(image);
        F_Delete(file);
    }
    }
}

void BitmapFont_DeleteGLDisplayLists(font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    int i;

    if(novideo || isDedicated)
        return;

    font->_isDirty = true;
    if(Con_IsBusy())
        return;

    for(i = 0; i < 256; ++i)
    {
        bitmapfont_char_t* ch = &bf->_chars[i];
        if(ch->dlist)
            GL_DeleteLists(ch->dlist, 1);
        ch->dlist = 0;
    }
    }
}

void BitmapFont_DeleteGLTexture(font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;

    if(novideo || isDedicated)
        return;

    font->_isDirty = true;
    if(Con_IsBusy())
        return;
    if(bf->_tex)
        glDeleteTextures(1, (const GLuint*) &bf->_tex);
    bf->_tex = 0;
    }
}

void BitmapFont_SetFilePath(font_t* font, const char* filePath)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
 
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
}

DGLuint BitmapFont_GLTextureName(const font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    return bf->_tex;
    }
}

int BitmapFont_TextureWidth(const font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    return bf->_texWidth;
    }
}

int BitmapFont_TextureHeight(const font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    return bf->_texHeight;
    }
}

void BitmapFont_CharCoords(font_t* font, int* s0, int* s1,
    int* t0, int* t1, unsigned char ch)
{
    assert(NULL != font && font->_type == FT_BITMAP);
    {
    bitmapfont_t* bf = (bitmapfont_t*)font;
    if(!s0 && !s1 && !t0 && !t1)
        return;
    BitmapFont_Prepare(font);
    if(s0) *s0 = bf->_chars[ch].x;
    if(s0) *s1 = bf->_chars[ch].x + bf->_chars[ch].w;
    if(t0) *t0 = bf->_chars[ch].y;
    if(t1) *t1 = bf->_chars[ch].y + bf->_chars[ch].h;
    }
}

font_t* BitmapCompositeFont_New(void)
{
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)malloc(sizeof(*cf));
    font_t* font = (font_t*)cf;

    if(NULL == cf)
        Con_Error("BitmapCompositeFont::Construct: Failed on allocation of %lu bytes.", (unsigned long) sizeof(*cf));

    cf->_def = 0;
    memset(cf->_chars, 0, sizeof(cf->_chars));

    Font_Init(font, FT_BITMAPCOMPOSITE);
    font->_flags |= FF_COLORIZE;

    return font;
}

void BitmapCompositeFont_Delete(font_t* font)
{
    BitmapCompositeFont_DeleteGLTextures(font);
    BitmapCompositeFont_DeleteGLDisplayLists(font);
    free(font);
}

int BitmapCompositeFont_CharWidth(font_t* font, unsigned char ch)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    if(cf->_chars[ch].w == 0) return font->_noCharWidth;
    return cf->_chars[ch].w - font->_marginWidth * 2 - 2;
    }
}

int BitmapCompositeFont_CharHeight(font_t* font, unsigned char ch)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    if(cf->_chars[ch].h == 0) return font->_noCharHeight;
    return cf->_chars[ch].h - font->_marginHeight * 2 - 2;
    }
}

void BitmapCompositeFont_Prepare(font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    int avgWidth, avgHeight, numPatches;

    if(!font->_isDirty)
        return;

    BitmapCompositeFont_DeleteGLDisplayLists(font);
    BitmapCompositeFont_DeleteGLTextures(font);

    avgWidth = avgHeight = 0;
    numPatches = 0;

    { uint i;
    for(i = 0; i < MAX_CHARS; ++i)
    {
        patchid_t patch = cf->_chars[i].patch;
        patchinfo_t info;

        if(0 == patch)
            continue;

        ++numPatches;

        R_GetPatchInfo(patch, &info);
        cf->_chars[i].x = info.offset    + info.extraOffset[0] + font->_marginWidth;
        cf->_chars[i].y = info.topOffset + info.extraOffset[1] + font->_marginHeight;
        cf->_chars[i].w = info.width  + 2;
        cf->_chars[i].h = info.height + 2;

        avgWidth  += cf->_chars[i].w;
        avgHeight += cf->_chars[i].h;

        if(!(novideo || isDedicated) && cf->_chars[i].tex == 0)
            cf->_chars[i].tex = GL_PreparePatch(R_PatchTextureByIndex(patch));
    }}

    font->_noCharWidth  = avgWidth  / numPatches; 
    font->_noCharHeight = avgHeight / numPatches; 

    if(!(novideo || isDedicated || Con_IsBusy())) // Cannot do this while in busy mode.
    {
        GLuint base = glGenLists(numPatches);
        glListBase(base);

        { uint i;
        for(i = 0; i < MAX_CHARS; ++i)
        {
            if(0 != cf->_chars[i].patch)
            {
                glNewList(base+i, GL_COMPILE);
                drawCharacter((unsigned char)i, font);
                glEndList();
                cf->_chars[i].dlist = base+i;
            }
            else
            {
                cf->_chars[i].dlist = 0;
            }
        }}

        font->_isDirty = false;
    }
    }
}

void BitmapCompositeFont_DeleteGLDisplayLists(font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    int i;

    if(novideo || isDedicated)
        return;

    font->_isDirty = true;
    if(Con_IsBusy())
        return;

    for(i = 0; i < 256; ++i)
    {
        bitmapcompositefont_char_t* ch = &cf->_chars[i];
        if(ch->dlist)
            GL_DeleteLists(ch->dlist, 1);
        ch->dlist = 0;
    }
    }
}

void BitmapCompositeFont_DeleteGLTextures(font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    int i;

    if(novideo || isDedicated)
        return;

    font->_isDirty = true;
    if(Con_IsBusy())
        return;

    for(i = 0; i < 256; ++i)
    {
        bitmapcompositefont_char_t* ch = &cf->_chars[i];
        if(ch->tex)
            glDeleteTextures(1, (const GLuint*) &ch->tex);
        ch->tex = 0;
    }
    }
}

struct ded_compositefont_s* BitmapCompositeFont_Definition(const font_t* font)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    return cf->_def;
    }
}

void BitmapCompositeFont_SetDefinition(font_t* font, struct ded_compositefont_s* def)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    cf->_def = def;
    }
}

patchid_t BitmapCompositeFont_CharPatch(font_t* font, unsigned char ch)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
    BitmapCompositeFont_Prepare(font);
    return cf->_chars[ch].patch;
    }
}

void BitmapCompositeFont_CharSetPatch(font_t* font, unsigned char ch, const char* patchName)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;

    // Load the patches in monochrome mode. (2 = weighted average).
    monochrome = 2;
    upscaleAndSharpenPatches = true;

    if(!novideo && !isDedicated && !Con_IsBusy())
    {
        if(cf->_chars[ch].tex)
            glDeleteTextures(1, (const GLuint*)&cf->_chars[ch].tex);
        cf->_chars[ch].tex = 0;

        if(cf->_chars[ch].dlist)
            glDeleteLists((GLuint)cf->_chars[ch].dlist, 1);
        cf->_chars[ch].dlist = 0;
    }

    cf->_chars[ch].patch = R_RegisterPatch(patchName);
    { patchinfo_t info;
    R_GetPatchInfo(cf->_chars[ch].patch, &info);
    cf->_chars[ch].x = info.offset    + info.extraOffset[0] + font->_marginWidth;
    cf->_chars[ch].y = info.topOffset + info.extraOffset[1] + font->_marginHeight;
    cf->_chars[ch].w = info.width  + 2;
    cf->_chars[ch].h = info.height + 2;
    }

    font->_isDirty = true;

    upscaleAndSharpenPatches = false;
    monochrome = 0;
    }
}

void BitmapCompositeFont_CharCoords(font_t* font, int* s0, int* s1,
    int* t0, int* t1, unsigned char ch)
{
    assert(NULL != font && font->_type == FT_BITMAPCOMPOSITE);
    {
    if(!s0 && !s1 && !t0 && !t1)
        return;
    BitmapCompositeFont_Prepare(font);
    if(s0) *s0 = 0;
    if(s0) *s1 = 1;
    if(t0) *t0 = 0;
    if(t1) *t1 = 1;
    }
}
