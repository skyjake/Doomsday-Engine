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
#include "m_misc.h"

#include "bitmapfont.h"

static void drawCharacter(unsigned char ch, bitmapfont_t* font)
{
    int s[2], t[2], x = 0, y = 0, w, h;

    s[0] = font->_chars[ch].x;
    s[1] = font->_chars[ch].x + font->_chars[ch].w;
    t[0] = font->_chars[ch].y;
    t[1] = font->_chars[ch].y + font->_chars[ch].h;

    w = s[1] - s[0];
    h = t[1] - t[0];

    if(!BitmapFont_GLTexture(font))
    {
        x = font->_chars[ch].x;
        y = font->_chars[ch].y;
        w = font->_chars[ch].w;
        h = font->_chars[ch].h;

        s[0] = 0;
        s[1] = 1;
        t[0] = 0;
        t[1] = 1;
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

static byte inByte(DFILE* f)
{
    assert(f);
    {
    byte b;
    F_Read(&b, sizeof(b), f);
    return b;
    }
}

static unsigned short inShort(DFILE* f)
{
    assert(f);
    {
    unsigned short s;
    F_Read(&s, sizeof(s), f);
    return USHORT(s);
    }
}

static void* readFormat0(bitmapfont_t* font, DFILE* file)
{
    assert(font && file);
    {
    int i, c, glyphCount = 0, bitmapFormat, numPels;
    uint32_t* image;

    font->_flags |= BFF_IS_MONOCHROME;
    font->_flags &= ~BFF_HAS_EMBEDDEDSHADOW;
    font->_marginWidth = font->_marginHeight = 0;

    // Load in the data.
    font->_texWidth = inShort(file);
    font->_texHeight = inShort(file);
    glyphCount = inShort(file);
    VERBOSE2( Con_Printf("readFormat0: Dimensions %i x %i, with %i chars.\n",
                         font->_texWidth, font->_texHeight, glyphCount) );

    for(i = 0; i < glyphCount; ++i)
    {
        bitmapfont_char_t* ch = &font->_chars[i < MAX_CHARS ? i : MAX_CHARS - 1];

        ch->x = inShort(file);
        ch->y = inShort(file);
        ch->w = inByte(file);
        ch->h = inByte(file);
    }

    // The bitmap.
    bitmapFormat = inByte(file);
    if(bitmapFormat > 0)
    {
        Con_Error("readFormat0: Font %s uses unknown bitmap bitmapFormat %i.\n",
                  font->_name, bitmapFormat);
    }

    numPels = font->_texWidth * font->_texHeight;
    image = calloc(1, numPels * sizeof(int));
    for(c = i = 0; i < (numPels + 7) / 8; ++i)
    {
        int bit, mask = inByte(file);

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

static void* readFormat2(bitmapfont_t* font, DFILE* file)
{
    assert(font && file);
    {
    int i, numPels, dataHeight, glyphCount = 0;
    byte bitmapFormat = 0;
    uint32_t* image, *ptr;

    bitmapFormat = inByte(file);
    if(bitmapFormat != 1 && bitmapFormat != 0) // Luminance + Alpha.
    {
        Con_Error("FR_ReadFormat1: Bitmap format %i not implemented.\n", bitmapFormat);
    }

    font->_flags |= BFF_IS_MONOCHROME|BFF_HAS_EMBEDDEDSHADOW;

    // Load in the data.
    font->_texWidth = inShort(file);
    dataHeight = inShort(file);
    font->_texHeight = M_CeilPow2(dataHeight);
    glyphCount = inShort(file);
    font->_marginWidth = font->_marginHeight = inShort(file);

    font->_leading = inShort(file);
    /*font->_glyphHeight =*/ inShort(file); // Unused.
    font->_ascent = inShort(file);
    font->_descent = inShort(file);

    for(i = 0; i < glyphCount; ++i)
    {
        ushort code = inShort(file);
        ushort x = inShort(file);
        ushort y = inShort(file);
        ushort w = inShort(file);
        ushort h = inShort(file);

        if(code < MAX_CHARS)
        {
            font->_chars[code].x = x;
            font->_chars[code].y = y;
            font->_chars[code].w = w;
            font->_chars[code].h = h;
        }
    }

    // Read the bitmap.
    numPels = font->_texWidth * font->_texHeight;
    image = ptr = calloc(1, numPels * 4);
    if(bitmapFormat == 0)
    {
        for(i = 0; i < numPels; ++i)
        {
            byte red = inByte(file);
            byte green = inByte(file);
            byte blue = inByte(file);
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
            *ptr++ = ULONG(luminance | (luminance << 8) | (luminance << 16) |
                      (alpha << 24));
        }
    }

    return image;
    }
}

static void prepareFont(bitmapfont_t* font)
{
    assert(font);

    // Is this an archived font?
    if(!Str_IsEmpty(&font->_filePath))
    {
        DFILE* file;
        if(0 == font->_tex &&
           0 != (file = F_Open(Str_Text(&font->_filePath), "rb")))
        {
            void* image = 0;
            int version;

            BitmapFont_DeleteGLDisplayLists(font);
            BitmapFont_DeleteGLTextures(font);

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
            if(!image)
                return;

            // Upload the texture.
            if(!novideo && !isDedicated)
            {
                VERBOSE2( Con_Printf("Uploading GL texture for font '%s' ...\n", Str_Text(&font->_name)) );

                font->_tex = GL_NewTextureWithParams2(DGL_RGBA, font->_texWidth,
                    font->_texHeight, image, 0, GL_LINEAR, GL_NEAREST, 0 /* no AF */,
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
                        font->_chars[i].dlist = base+i;
                    }}

                    // All preparation complete.
                    font->_flags &= ~BFF_IS_DIRTY;
                }
            }

            free(image);
            F_Close(file);
        }
        return;
    }

    // No, its an aggregate-patch font.
    if(!(font->_flags & BFF_IS_DIRTY))
        return;

    font->_flags |= BFF_IS_MONOCHROME;
    font->_marginWidth = font->_marginHeight = 1;

    BitmapFont_DeleteGLDisplayLists(font);
    BitmapFont_DeleteGLTextures(font);

    { uint i;
    for(i = 0; i < MAX_CHARS; ++i)
    {
        patchid_t patch = font->_chars[i].patch;
        patchinfo_t info;
        if(0 == patch)
            continue;
        R_GetPatchInfo(patch, &info);
        font->_chars[i].x = info.offset    + info.extraOffset[0] + font->_marginWidth;
        font->_chars[i].y = info.topOffset + info.extraOffset[1] + font->_marginHeight;
        font->_chars[i].w = info.width + 2;
        font->_chars[i].h = info.height + 2;

        if(!(novideo || isDedicated) && font->_chars[i].tex == 0)
            font->_chars[i].tex = GL_PreparePatch(R_FindPatchTex(patch));
    }}

    if(!(novideo || isDedicated || Con_IsBusy())) // Cannot do this while in busy mode.
    {
        GLuint base = glGenLists(MAX_CHARS);
        glListBase(base);
        { uint i;
        for(i = 0; i < MAX_CHARS; ++i)
        {
            if(0 != font->_chars[i].patch)
            {
                glNewList(base+i, GL_COMPILE);
                drawCharacter((unsigned char)i, font);
                glEndList();
            }
            font->_chars[i].dlist = base+i;
        }}

        font->_flags &= ~BFF_IS_DIRTY;
    }
}

bitmapfont_t* BitmapFont_Construct(fontid_t id, const char* name,
    const char* filePath)
{
    assert(name && name[0]);
    {
    bitmapfont_t* font = malloc(sizeof(*font));

    font->_id = id;
    Str_Init(&font->_name); Str_Set(&font->_name, name);
    Str_Init(&font->_filePath);
    if(filePath)
        Str_Set(&font->_filePath, filePath);
    font->_flags = BFF_IS_DIRTY;
    font->_marginWidth = 0;
    font->_marginHeight = 0;
    font->_leading = 0;
    font->_ascent = 0;
    font->_descent = 0;
    memset(font->_chars, 0, sizeof(font->_chars));
    font->_tex = 0;
    font->_texWidth = 0;
    font->_texHeight = 0;
    // Lets try to prepare this right away if we can.
    prepareFont(font);

    return font;
    }
}

void BitmapFont_Destruct(bitmapfont_t* font)
{
    assert(font);
    BitmapFont_DeleteGLTextures(font);
    BitmapFont_DeleteGLDisplayLists(font);
    Str_Free(&font->_filePath);
    Str_Free(&font->_name);
    free(font);
}

void BitmapFont_DeleteGLDisplayLists(bitmapfont_t* font)
{
    assert(font);
    if(novideo || isDedicated)
        return;
    font->_flags |= BFF_IS_DIRTY;
    if(Con_IsBusy())
        return;
    { int i;
    for(i = 0; i < 256; ++i)
    {
        bitmapfont_char_t* ch = &font->_chars[i];
        if(ch->dlist)
            GL_DeleteLists(ch->dlist, 1);
        ch->dlist = 0;
    }}
}

void BitmapFont_DeleteGLTextures(bitmapfont_t* font)
{
    assert(font);
    if(novideo || isDedicated)
        return;
    font->_flags |= BFF_IS_DIRTY;
    if(Con_IsBusy())
        return;
    if(font->_tex)
        glDeleteTextures(1, (const GLuint*) &font->_tex);
    font->_tex = 0;
    { int i;
    for(i = 0; i < 256; ++i)
    {
        bitmapfont_char_t* ch = &font->_chars[i];
        if(ch->tex)
            glDeleteTextures(1, (const GLuint*) &ch->tex);
        ch->tex = 0;
    }}
}

int BitmapFont_CharWidth(bitmapfont_t* font, unsigned char ch)
{
    assert(font);
    prepareFont(font);
    return font->_chars[ch].w - font->_marginWidth * 2;
}

int BitmapFont_CharHeight(bitmapfont_t* font, unsigned char ch)
{
    assert(font);
    prepareFont(font);
    return font->_chars[ch].h - font->_marginHeight * 2;
}

void BitmapFont_CharDimensions(bitmapfont_t* font, int* width,
    int* height, unsigned char ch)
{
    assert(font);
    if(!width && !height)
        return;
    if(width)
        *width = BitmapFont_CharWidth(font, ch);
    if(height)
        *height = BitmapFont_CharHeight(font, ch);
}

void BitmapFont_CharCoords(bitmapfont_t* font, int* s0, int* s1,
    int* t0, int* t1, unsigned char ch)
{
    assert(font);
    if(!s0 && !s1 && !t0 && !t1)
        return;
    prepareFont(font);
    if(s0)
        *s0 = !BitmapFont_GLTexture(font)? 0 : font->_chars[ch].x;
    if(s0)
        *s1 = !BitmapFont_GLTexture(font)? 1 : font->_chars[ch].x + font->_chars[ch].w;
    if(t0)
        *t0 = !BitmapFont_GLTexture(font)? 0 : font->_chars[ch].y;
    if(t1)
        *t1 = !BitmapFont_GLTexture(font)? 1 : font->_chars[ch].y + font->_chars[ch].h;
}

patchid_t BitmapFont_CharPatch(bitmapfont_t* font, unsigned char ch)
{
    assert(font);
    prepareFont(font);
    return font->_chars[ch].patch;
}

void BitmapFont_CharSetPatch(bitmapfont_t* font, unsigned char ch, const char* patchName)
{
    assert(font);

    // Load the patches in monochrome mode. (2 = weighted average).
    monochrome = 2;
    upscaleAndSharpenPatches = true;

    if(!novideo && !isDedicated && !Con_IsBusy())
    {
        if(font->_chars[ch].tex)
            glDeleteTextures(1, (const GLuint*)&font->_chars[ch].tex);
        font->_chars[ch].tex = 0;

        if(font->_chars[ch].dlist)
            glDeleteLists((GLuint)font->_chars[ch].dlist, 1);
        font->_chars[ch].dlist = 0;
    }

    font->_chars[ch].patch = R_RegisterAsPatch(patchName);
    { patchinfo_t info;
    R_GetPatchInfo(font->_chars[ch].patch, &info);
    font->_chars[ch].x = info.offset    + info.extraOffset[0] + font->_marginWidth;
    font->_chars[ch].y = info.topOffset + info.extraOffset[1] + font->_marginHeight;
    font->_chars[ch].w = info.width  + 2;
    font->_chars[ch].h = info.height + 2;
    }

    font->_flags |= BFF_IS_DIRTY;

    upscaleAndSharpenPatches = false;
    monochrome = 0;
}

int BitmapFont_Flags(const bitmapfont_t* font)
{
    assert(font);
    return font->_flags;
}

fontid_t BitmapFont_Id(const bitmapfont_t* font)
{
    assert(font);
    return font->_id;
}

const ddstring_t* BitmapFont_Name(const bitmapfont_t* font)
{
    assert(font);
    return &font->_name;
}

int BitmapFont_Ascent(bitmapfont_t* font)
{
    assert(font);
    prepareFont(font);
    return font->_ascent;
}

int BitmapFont_Descent(bitmapfont_t* font)
{
    assert(font);
    prepareFont(font);
    return font->_descent;
}

int BitmapFont_Leading(bitmapfont_t* font)
{
    assert(font);
    prepareFont(font);
    return font->_leading;
}

DGLuint BitmapFont_GLTexture(bitmapfont_t* font)
{
    assert(font);
    return font->_tex;
}

int BitmapFont_GLTextureWidth(bitmapfont_t* font)
{
    assert(font);
    return font->_texWidth;
}

int BitmapFont_GLTextureHeight(bitmapfont_t* font)
{
    assert(font);
    return font->_texHeight;
}
