/**\file gl_font.c
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

/**
 * Font Renderer.
 */

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_render.h"

#include "bitmapfont.h"
#include "m_args.h"
#include "m_misc.h"

typedef struct fr_state_attributes_s {
    int tracking;
    int shadowOffsetX, shadowOffsetY;
    float shadowStrength;
    float glitterStrength;
} fr_state_attributes_t;

typedef struct {
    int fontIdx;
    int attribStackDepth;
    fr_state_attributes_t attribStack[FR_MAX_ATTRIB_STACK_DEPTH];
} fr_state_t;
static fr_state_t fr;
fr_state_t* frState = &fr;

typedef struct {
    fontid_t font;
    float scaleX, scaleY;
    float offX, offY;
    float angle;
    float color[4];
    float glitterStrength, shadowStrength;
    int shadowOffsetX, shadowOffsetY;
    int tracking;
    float leading;
    boolean typeIn;
    boolean caseScale;
    struct {
        float scale, offset;
    } caseMod[2]; // 1=upper, 0=lower
} drawtextstate_t;

static __inline fr_state_attributes_t* currentAttributes(void);
static int topToAscent(bitmapfont_t* font);
static int lineHeight(bitmapfont_t* font, unsigned char ch);
static void drawChar(unsigned char ch, int posX, int posY, bitmapfont_t* font, short flags);
static void drawFlash(int x, int y, int w, int h, int bright);

static int inited = false;

static int numFonts = 0;
static bitmapfont_t** fonts = 0; // The list of fonts.

static int typeInTime;

static fontid_t getNewFontId(void)
{
    assert(inited);
    {
    int i;
    fontid_t max = 0;
    for(i = 0; i < numFonts; ++i)
    {
        const bitmapfont_t* font = fonts[i];
        if(BitmapFont_Id(font) > max)
            max = BitmapFont_Id(font);
    }
    return max + 1;
    }
}

static fontid_t findFontIdForName(const char* name)
{
    assert(inited);
    if(name && name[0])
    {
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            const bitmapfont_t* font = fonts[i];
            if(!Str_CompareIgnoreCase(BitmapFont_Name(font), name))
                return BitmapFont_Id(font);
        }
    }
    return 0; // Not found.
}

static int findFontIdxForName(const char* name)
{
    assert(inited);
    if(name && name[0])
    {
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            const bitmapfont_t* font = fonts[i];
            if(!Str_CompareIgnoreCase(BitmapFont_Name(font), name))
                return i;
        }
    }
    return -1;
}

static int findFontIdxForId(fontid_t id)
{
    assert(inited);
    if(id != 0)
    {
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            const bitmapfont_t* font = fonts[i];
            if(BitmapFont_Id(font) == id)
                return i;
        }
    }
    return -1;
}

static void destroyFontIdx(int idx)
{
    assert(inited);

    BitmapFont_Destruct(fonts[idx]);

    memmove(&fonts[idx], &fonts[idx + 1], sizeof(*fonts) * (numFonts - idx - 1));
    --numFonts;
    fonts = realloc(fonts, sizeof(*fonts) * numFonts);

    if(fr.fontIdx == idx)
        fr.fontIdx = -1;
}

static void destroyFonts(void)
{
    assert(inited);
    {
    while(numFonts)
        destroyFontIdx(0);
    fonts = 0;
    fr.fontIdx = -1;
    }
}

static bitmapfont_t* addFont(bitmapfont_t* font)
{
    assert(inited && font);
    fonts = realloc(fonts, sizeof(*fonts) * ++numFonts);
    fonts[numFonts-1] = font;
    return font;
}

/**
 * Creates a new font record for a file and attempts to prepare it.
 */
static bitmapfont_t* loadFont(const char* name, const char* path)
{
    assert(inited);
    {
    bitmapfont_t* font;   
    if(0 != (font = addFont(BitmapFont_Construct(getNewFontId(), name, path))))
    {
        VERBOSE( Con_Message("New font '%s'.\n", Str_Text(BitmapFont_Name(font))) );
    }
    return font;
    }
}

static int topToAscent(bitmapfont_t* font)
{
    assert(font);
    {
    int lineHeight = BitmapFont_Leading(font);
    if(lineHeight == 0)
        return 0;
    return lineHeight - BitmapFont_Ascent(font);
    }
}

static int lineHeight(bitmapfont_t* font, unsigned char ch)
{
    int ascent = BitmapFont_Ascent(font);
    if(ascent != 0)
        return ascent;
    return BitmapFont_CharHeight(font, ch);
}

static __inline fr_state_attributes_t* currentAttributes(void)
{
    return fr.attribStack + fr.attribStackDepth;
}

int FR_Init(void)
{
    if(inited)
        return -1; // No reinitializations...

    inited = true;
    numFonts = 0;
    fonts = 0; // No fonts!
    fr.fontIdx = -1;
    FR_LoadDefaultAttrib();
    typeInTime = 0;

    return 0;
}

void FR_Shutdown(void)
{
    if(!inited)
        return;
    destroyFonts();
    inited = false;
}

fontid_t FR_CreateFontFromDef(ded_compositefont_t* def)
{
    bitmapfont_t* font;
    fontid_t id;

    // Valid name?
    if(!def->id[0])
        Con_Error("FR_CreateFontFromDef: A valid name is required (not NULL or zero-length).");

    // Already a font by this name?
    if(0 != (id = FR_SafeFontIdForName(def->id)))
    {
        Con_Error("FR_CreateFontFromDef: Failed to create font '%s', name already in use.");
        return 0; // Unreachable.
    }

    // A new font.
    font = addFont(BitmapFont_Construct(getNewFontId(), def->id, 0));
    { int i;
    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path)
            continue;
        if(0 != (path = Uri_Resolved(def->charMap[i].path)))
        {
            BitmapFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }}
    return BitmapFont_Id(font);
}

fontid_t FR_LoadSystemFont(const char* name, const char* searchPath)
{
    assert(name && searchPath && searchPath[0]);
    {
    bitmapfont_t* font;

    if(!inited)
    {
#if _DEBUG
        Con_Message("Warning: Font renderer has not yet been initialized.");
#endif
        return 0;
    }

    if(0 != findFontIdForName(name) || 0 == F_Access(searchPath))
    {   
        return 0; // Error.
    }

    VERBOSE2( Con_Message("Preparing font \"%s\"...\n", F_PrettyPath(searchPath)) );
    if(0 == (font = loadFont(name, searchPath)))
    {   // Error.
        Con_Message("Warning: Unknown format for %s\n", searchPath);
        return 0;
    }

    // Make this the current font.
    fr.fontIdx = numFonts-1;

    return BitmapFont_Id(font);
    }
}

void FR_DestroyFont(fontid_t id)
{
    int idx;
    if(!inited)
        Con_Error("FR_DestroyFont: Font renderer has not yet been initialized.");
    if(-1 != (idx = findFontIdxForId(id)))
    {
        destroyFontIdx(idx);
        if(fr.fontIdx == idx)
            fr.fontIdx = -1;
    }
}

void FR_Update(void)
{
    assert(inited);
    if(novideo || isDedicated)
        return;
    { int i;
    for(i = 0; i < numFonts; ++i)
    {
        BitmapFont_DeleteGLTextures(fonts[i]);
        BitmapFont_DeleteGLDisplayLists(fonts[i]);
    }}
}

void FR_Ticker(timespan_t ticLength)
{
    static trigger_t fixed = { 1 / 35.0 };

    if(!inited)
        return;
    // Restricted to fixed 35 Hz ticks.
    if(!M_RunTrigger(&fixed, ticLength))
        return; // It's too soon.

    ++typeInTime;
}

void FR_ResetTypeInTimer(void)
{
    if(!inited)
        return;
    typeInTime = 0;
}

bitmapfont_t* FR_Font(fontid_t id)
{
    assert(inited);
    { int idx;
    if(-1 != (idx = findFontIdxForId(id)))
        return fonts[idx];
    }
    return 0;
}

fontid_t FR_SafeFontIdForName(const char* name)
{
    if(!inited)
    {
#if _DEBUG
        Con_Message("Warning: Font renderer has not yet been initialized.");
#endif
        return 0;
    }
    return findFontIdForName(name);
}

fontid_t FR_FontIdForName(const char* name)
{
    fontid_t id = FR_SafeFontIdForName(name);
    if(id == 0)
        Con_Error("Failed loading font \"%s\".", name);
    return id;
}

int FR_GetFontIdx(fontid_t id)
{
    int idx;
    if(!inited)
        Con_Error("FR_GetFontIdx: Font renderer has not yet been initialized.");
    if(-1 == (idx = findFontIdxForId(id)))
        Con_Message("Warning:FR_GetFontIdx: Unknown ID %u.\n", id);
    return idx;
}

void FR_SetFont(fontid_t id)
{
    int idx;
    if(!inited)
        Con_Error("FR_SetFont: Font renderer has not yet been initialized.");
    idx = FR_GetFontIdx(id);
    if(idx == -1)
        return; // No such font.
    fr.fontIdx = idx;
}

fontid_t FR_GetCurrentId(void)
{
    if(!inited)
        Con_Error("FR_GetCurrentId: Font renderer has not yet been initialized.");
    if(fr.fontIdx != -1)
        return BitmapFont_Id(fonts[fr.fontIdx]);
    return 0;
}

void FR_LoadDefaultAttrib(void)
{
    fr_state_attributes_t* sat = currentAttributes();
    if(!inited)
        Con_Error("FR_LoadDefaultAttrib: Font renderer has not yet been initialized.");
    sat->tracking = DEFAULT_TRACKING;
    sat->shadowStrength = DEFAULT_SHADOW_STRENGTH;
    sat->shadowOffsetX = DEFAULT_SHADOW_XOFFSET;
    sat->shadowOffsetY = DEFAULT_SHADOW_YOFFSET;
    sat->glitterStrength = DEFAULT_GLITTER_STRENGTH;
}

void FR_PushAttrib(void)
{
    if(!inited)
        Con_Error("FR_PushAttrib: Font renderer has not yet been initialized.");
    if(fr.attribStackDepth == FR_MAX_ATTRIB_STACK_DEPTH)
    {
        Con_Error("FR_PushAttrib: STACK_OVERFLOW.");
        exit(1); // Unreachable.
    }
    memcpy(fr.attribStack + fr.attribStackDepth + 1, fr.attribStack + fr.attribStackDepth, sizeof(fr.attribStack[0]));
    ++fr.attribStackDepth;
}

void FR_PopAttrib(void)
{
    if(!inited)
        Con_Error("FR_PopAttrib: Font renderer has not yet been initialized.");
    if(fr.attribStackDepth == 0)
    {
        Con_Error("FR_PopAttrib: STACK_UNDERFLOW.");
        exit(1); // Unreachable.
    }
    --fr.attribStackDepth;
}

int FR_Tracking(void)
{
    fr_state_attributes_t* sat = currentAttributes();
    if(!inited)
        Con_Error("FR_Tracking: Font renderer has not yet been initialized.");
    return sat->tracking;
}

void FR_SetTracking(int tracking)
{
    fr_state_attributes_t* sat = currentAttributes();
    if(!inited)
        Con_Error("FR_SetTracking: Font renderer has not yet been initialized.");
    sat->tracking = tracking;
}

void FR_SetShadowOffset(int offsetX, int offsetY)
{
    fr_state_attributes_t* sat = currentAttributes();
    if(!inited)
        Con_Error("FR_SetShadowOffset: Font renderer has not yet been initialized.");
    sat->shadowOffsetX = offsetX;
    sat->shadowOffsetY = offsetY;
}

void FR_SetShadowStrength(float value)
{
    fr_state_attributes_t* sat = currentAttributes();
    if(!inited)
        Con_Error("FR_SetShadowStrength: Font renderer has not yet been initialized.");
    sat->shadowStrength = MINMAX_OF(0, value, 1);
}

void FR_SetGlitterStrength(float value)
{
    fr_state_attributes_t* sat = currentAttributes();
    if(!inited)
        Con_Error("FR_SetGlitterStrength: Font renderer has not yet been initialized.");
    sat->glitterStrength = MINMAX_OF(0, value, 1);
}

void FR_CharDimensions(int* width, int* height, unsigned char ch)
{
    if(!inited)
        Con_Error("FR_CharDimensions: Font renderer has not yet been initialized.");
    BitmapFont_CharDimensions(fonts[fr.fontIdx], width, height, ch);
}

int FR_CharWidth(unsigned char ch)
{
    if(!inited)
        Con_Error("FR_CharWidth: Font renderer has not yet been initialized.");
    if(fr.fontIdx != -1)
        return BitmapFont_CharWidth(fonts[fr.fontIdx],ch);
    return 0;
}

int FR_CharHeight(unsigned char ch)
{
    if(!inited)
        Con_Error("FR_CharHeight: Font renderer has not yet been initialized.");
    if(fr.fontIdx != -1)
        return BitmapFont_CharHeight(fonts[fr.fontIdx], ch);
    return 0;
}

void FR_TextFragmentDimensions(int* width, int* height, const char* text)
{
    if(!inited)
        Con_Error("FR_TextFragmentDimensions: Font renderer has not yet been initialized.");
    if(!width && !height)
        return;
    if(width)
        *width = FR_TextFragmentWidth(text);
    if(height)
        *height = FR_TextFragmentHeight(text);
}

int FR_TextFragmentWidth(const char* text)
{
    size_t i, len;
    int width = 0;
    const char* ch;
    unsigned char c;

    if(!inited)
        Con_Error("FR_TextFragmentWidth: Font renderer has not yet been initialized.");

    if(fr.fontIdx == -1 || !text)
        return 0;

    // Just add them together.
    len = strlen(text);
    i = 0;
    ch = text;
    while(i++ < len && (c = *ch++) != 0 && c != '\n')
        width += FR_CharWidth(c);

    return (int) (width + currentAttributes()->tracking * (len-1));
}

int FR_TextFragmentHeight(const char* text)
{
    const char* ch;
    unsigned char c;
    int height = 0;
    size_t len;
    uint i;

    if(!inited)
        Con_Error("FR_TextFragmentHeight: Font renderer has not yet been initialized.");
 
    if(fr.fontIdx == -1 || !text)
        return 0;

    // Find the greatest height.
    i = 0;
    height = 0;
    len = strlen(text);
    ch = text;
    while(i++ < len && (c = *ch++) != 0 && c != '\n')
    {
        height = MAX_OF(height, FR_CharHeight(c));
    }

    return topToAscent(fonts[fr.fontIdx]) + height;
}

int FR_SingleLineHeight(const char* text)
{
    if(!inited)
        Con_Error("FR_SingleLineHeight: Font renderer has not yet been initialized.");
    if(fr.fontIdx == -1 || !text)
        return 0;
    { int ascent = BitmapFont_Ascent(fonts[fr.fontIdx]);
    if(ascent != 0)
        return ascent;
    }
    return BitmapFont_CharHeight(fonts[fr.fontIdx], (unsigned char)text[0]);
}

int FR_GlyphTopToAscent(const char* text)
{
    int lineHeight;
    if(!inited)
        Con_Error("FR_GlyphTopToAscent: Font renderer has not yet been initialized.");
    if(fr.fontIdx == -1 || !text)
        return 0;
    lineHeight = BitmapFont_Leading(fonts[fr.fontIdx]);
    if(lineHeight == 0)
        return 0;
    return lineHeight - BitmapFont_Ascent(fonts[fr.fontIdx]);
}

static void drawTextFragment(const char* string, int x, int y, short flags,
    int initialCount)
{
    assert(inited && string && string[0] && fr.fontIdx != -1);
    {
    bitmapfont_t* cf = fonts[fr.fontIdx];
    fr_state_attributes_t* sat = currentAttributes();
    boolean noTypein = (flags & DTF_NO_TYPEIN) != 0;
    boolean noGlitter = (sat->glitterStrength <= 0 || (flags & DTF_NO_GLITTER) != 0);
    boolean noShadow  = (sat->shadowStrength  <= 0 || (flags & DTF_NO_SHADOW)  != 0 ||
                         (BitmapFont_Flags(cf) & BFF_HAS_EMBEDDEDSHADOW)  != 0);
    float glitter = (noGlitter? 0 : sat->glitterStrength), glitterMul;
    float shadow  = (noShadow ? 0 : sat->shadowStrength), shadowMul;
    int w, h, cx, cy, count, yoff;
    float origColor[4], flashColor[3];
    unsigned char c;
    const char* ch;

    if(flags & DTF_ALIGN_RIGHT)
        x -= FR_TextFragmentWidth(string);
    else if(!(flags & DTF_ALIGN_LEFT))
        x -= FR_TextFragmentWidth(string)/2;

    if(flags & DTF_ALIGN_BOTTOM)
        y -= FR_TextFragmentHeight(string);
    else if(!(flags & DTF_ALIGN_TOP))
        y -= FR_TextFragmentHeight(string)/2;

    if(!(noTypein && noShadow && noGlitter))
        glGetFloatv(GL_CURRENT_COLOR, origColor);

    if(!(noTypein && noGlitter))
    {
        flashColor[CR] = (1 + 2 * origColor[0]) / 3;
        flashColor[CG] = (1 + 2 * origColor[1]) / 3;
        flashColor[CB] = (1 + 2 * origColor[2]) / 3;
    }

    if(0 != BitmapFont_GLTextureName(cf))
    {
        glBindTexture(GL_TEXTURE_2D, BitmapFont_GLTextureName(cf));
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glScalef(1.f / BitmapFont_TextureWidth(cf),
                 1.f / BitmapFont_TextureHeight(cf), 1.f);
    }

    { int pass;
    for(pass = (noShadow? 1 : 0); pass < 2; ++pass)
    {
        count = initialCount;
        ch = string;
        cx = x + (pass == 0? sat->shadowOffsetX : 0);
        cy = y + (pass == 0? sat->shadowOffsetY : 0);

        for(;;)
        {
            c = *ch++;
            yoff = 0;

            glitter = (noGlitter? 0 : sat->glitterStrength);
            glitterMul = 0;

            shadow = (noShadow? 0 : sat->shadowStrength);
            shadowMul = (noShadow? 0 : origColor[3]);

            if(!(noTypein && noShadow && noGlitter))
                glColor4fv(origColor);

            // Do the type-in effect?
            if(!noTypein && (pass || (!noShadow && !pass)))
            {
                int maxCount = (typeInTime > 0? typeInTime * 2 : 0);

                if(pass)
                {
                    if(!noGlitter)
                    {
                        if(count == maxCount)
                        {
                            glitterMul = 1;
                            flashColor[CR] = origColor[0];
                            flashColor[CG] = origColor[1];
                            flashColor[CB] = origColor[2];
                        }
                        else if(count + 1 == maxCount)
                        {
                            glitterMul = 0.88f;
                            flashColor[CR] = (1 + origColor[0]) / 2;
                            flashColor[CG] = (1 + origColor[1]) / 2;
                            flashColor[CB] = (1 + origColor[2]) / 2;
                        }
                        else if(count + 2 == maxCount)
                        {
                            glitterMul = 0.75f;
                            flashColor[CR] = origColor[0];
                            flashColor[CG] = origColor[1];
                            flashColor[CB] = origColor[2];
                        }
                        else if(count + 3 == maxCount)
                        {
                            glitterMul = 0.5f;
                            flashColor[CR] = origColor[0];
                            flashColor[CG] = origColor[1];
                            flashColor[CB] = origColor[2];
                        }
                        else if(count > maxCount)
                        {
                            break;
                        }
                    }
                    else if(count > maxCount)
                    {
                        break;
                    }
                }
                else
                {
                    if(count == maxCount)
                    {
                        shadowMul = 0;
                    }
                    else if(count + 1 == maxCount)
                    {
                        shadowMul *= .25f;
                    }
                    else if(count + 2 == maxCount)
                    {
                        shadowMul *= .5f;
                    }
                    else if(count + 3 == maxCount)
                    {
                        shadowMul *= .75f;
                    }
                    else if(count > maxCount)
                    {
                        break;
                    }
                }
            }
            count++;

            if(!c || c == '\n')
                break;

            w = FR_CharWidth(c);
            h = FR_CharHeight(c);

            if(' ' != c &&
               (0 != BitmapFont_GLTextureName(cf) || 0 != BitmapFont_CharPatch(cf, c)))
            {
                // A non-white-space character we have a glyph for.
                if(pass)
                {
                    // The character itself.
                    drawChar(c, cx, cy + yoff, cf, DEFAULT_DRAWFLAGS);

                    if(!noGlitter && glitter > 0)
                    {   // Do something flashy.
                        glColor4f(flashColor[CR], flashColor[CG], flashColor[CB], glitter * glitterMul);
                        drawFlash(cx, cy + yoff, w, h, true);
                    }
                }
                else if(!noShadow)
                {
                    glColor4f(1, 1, 1, shadow * shadowMul);
                    drawFlash(cx, cy + yoff, w, h, false);
                }
            }

            cx += w + sat->tracking;
        }
    }}

    // Restore previous GL-state.
    if(0 != BitmapFont_GLTextureName(cf))
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }
    if(!(noTypein && noGlitter && noShadow))
        glColor4fv(origColor);
    }
}

void FR_DrawTextFragment3(const char* string, int x, int y, short flags,
    int initialCount)
{
    if(!inited)
        Con_Error("Bitmap font system not yet initialized.");
    if(!string || !string[0])
    {
/*#if _DEBUG
        Con_Message("Warning: Attempt drawTextFragment with invalid/empty string.\n");
#endif*/
        return;
    }
    if(fr.fontIdx == -1)
    {
#if _DEBUG
        Con_Message("Warning: Attempt drawTextFragment without a current font.\n");
#endif
        return;
    }
    drawTextFragment(string, x, y, flags, initialCount);
}

void FR_DrawTextFragment2(const char* string, int x, int y, short flags)
{
    FR_DrawTextFragment3(string, x, y, flags, DEFAULT_INITIALCOUNT);
}

void FR_DrawTextFragment(const char* string, int x, int y)
{
    FR_DrawTextFragment2(string, x, y, DEFAULT_DRAWFLAGS);
}

void FR_DrawChar2(unsigned char ch, int x, int y, short flags)
{
    bitmapfont_t* cf;
    if(fr.fontIdx == -1)
        return;
    cf = fonts[fr.fontIdx];
    if(0 != BitmapFont_GLTextureName(cf))
    {
        glBindTexture(GL_TEXTURE_2D, BitmapFont_GLTextureName(cf));
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glScalef(1.f / BitmapFont_TextureWidth(cf),
                 1.f / BitmapFont_TextureHeight(cf), 1.f);
    }
    drawChar(ch, x, y, cf, flags);
    if(0 != BitmapFont_GLTextureName(cf))
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }
}

void FR_DrawChar(unsigned char ch, int x, int y)
{
    FR_DrawChar2(ch, x, y, DEFAULT_DRAWFLAGS);
}

static void drawChar(unsigned char ch, int posX, int posY, bitmapfont_t* font, short flags)
{
    float x = (float) posX, y = (float) posY;
    DGLuint tex;

    if(flags & DTF_ALIGN_RIGHT)
        x -= BitmapFont_CharWidth(font, ch);
    else if(!(flags & DTF_ALIGN_LEFT))
        x -= BitmapFont_CharWidth(font, ch) / 2;

    if(flags & DTF_ALIGN_BOTTOM)
        y -= topToAscent(font) + lineHeight(font, ch);
    else if(!(flags & DTF_ALIGN_TOP))
        y -= (topToAscent(font) + lineHeight(font, ch))/2;

    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_TEXTURE_2D);
    }
    else if(0 != (tex = BitmapFont_GLTextureName(font)))
    {
        GL_BindTexture(tex, GL_NEAREST);
    }
    else
    {
        patchid_t patch = BitmapFont_CharPatch(font, ch);
        if(patch == 0)
            return;
        GL_BindTexture(font->_chars[ch].tex, (filterUI ? GL_LINEAR : GL_NEAREST));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(x, y, 0);

    if(font->_chars[ch].dlist)
    {
        GL_CallList(font->_chars[ch].dlist);
    }
    else
    {
        int s[2], t[2], x = 0, y = 0, w, h;

        BitmapFont_CharCoords(font, &s[0], &s[1], &t[0], &t[1], ch);
        w = s[1] - s[0];
        h = t[1] - t[0];

        if(!BitmapFont_GLTextureName(font))
        {           
            x = font->_chars[ch].x;
            y = font->_chars[ch].y;
            w = font->_chars[ch].w;
            h = font->_chars[ch].h;
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

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(-x, -y, 0);

    if(renderWireframe)
    {
        /// \fixme do not assume previous state.
        glEnable(GL_TEXTURE_2D);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

static void drawFlash(int x, int y, int w, int h, int bright)
{
    float fsize = 4.f + bright, fw = fsize * w / 2.0f, fh = fsize * h / 2.0f;

    // Don't draw anything for very small letters.
    if(h <= 4)
        return;

    x += (int) (w / 2.0f - fw / 2);
    y += (int) (h / 2.0f - fh / 2);
    w = (int) fw;
    h = (int) fh;

    glBindTexture(GL_TEXTURE_2D, GL_PrepareLSTexture(LST_DYNAMIC));

    if(bright)
        GL_BlendMode(BM_ADD);
    else
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x + w, y);
        glTexCoord2f(1, 1);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1);
        glVertex2f(x, y + h);
    glEnd();

    GL_BlendMode(BM_NORMAL);
}

static int C_DECL compareFontName(const void* a, const void* b)
{
    return Str_CompareIgnoreCase(*((const ddstring_t**)a), Str_Text(*((const ddstring_t**)b)));
}

ddstring_t** FR_CollectFontNames(int* count)
{
    assert(inited);
    if(numFonts != 0)
    {
        ddstring_t** list = malloc(sizeof(*list) * (numFonts+1));
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            list[i] = Str_New();
            Str_Copy(list[i], BitmapFont_Name(fonts[i]));
        }
        list[i] = 0; // Terminate.
        qsort(list, numFonts, sizeof(*list), compareFontName);
        if(count)
            *count = numFonts;
        return list;
    }
    if(count)
        *count = 0;
    return 0;
}

/**
 * Expected: <whitespace> = <whitespace> <float>
 */
static float parseFloat(char** str)
{
    float value;
    char* end;

    *str = M_SkipWhite(*str);
    if(**str != '=')
        return 0; // Now I'm confused!

    *str = M_SkipWhite(*str + 1);
    value = (float) strtod(*str, &end);
    *str = end;
    return value;
}

static void parseParamaterBlock(char** strPtr, drawtextstate_t* state,
    int* numBreaks)
{
    (*strPtr)++;
    while(*(*strPtr) && *(*strPtr) != '}')
    {
        (*strPtr) = M_SkipWhite((*strPtr));

        // What do we have here?
        if(!strnicmp((*strPtr), "flash", 5))
        {
            (*strPtr) += 5;
            state->typeIn = true;
        }
        else if(!strnicmp((*strPtr), "noflash", 7))
        {
            (*strPtr) += 7;
            state->typeIn = false;
        }
        else if(!strnicmp((*strPtr), "case", 4))
        {
            (*strPtr) += 4;
            state->caseScale = true;
        }
        else if(!strnicmp((*strPtr), "nocase", 6))
        {
            (*strPtr) += 6;
            state->caseScale = false;
        }
        else if(!strnicmp((*strPtr), "ups", 3))
        {
            (*strPtr) += 3;
            state->caseMod[1].scale = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "upo", 3))
        {
            (*strPtr) += 3;
            state->caseMod[1].offset = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "los", 3))
        {
            (*strPtr) += 3;
            state->caseMod[0].scale = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "loo", 3))
        {
            (*strPtr) += 3;
            state->caseMod[0].offset = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "break", 5))
        {
            (*strPtr) += 5;
            ++(*numBreaks);
        }
        else if(!strnicmp((*strPtr), "r", 1))
        {
            (*strPtr)++;
            state->color[CR] = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "g", 1))
        {
            (*strPtr)++;
            state->color[CG] = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "b", 1))
        {
            (*strPtr)++;
            state->color[CB] = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "x", 1))
        {
            (*strPtr)++;
            state->offX = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "y", 1))
        {
            (*strPtr)++;
            state->offY = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "scalex", 6))
        {
            (*strPtr) += 6;
            state->scaleX = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "scaley", 6))
        {
            (*strPtr) += 6;
            state->scaleY = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "scale", 5))
        {
            (*strPtr) += 5;
            state->scaleX = state->scaleY = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "angle", 5))
        {
            (*strPtr) += 5;
            state->angle = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "glitter", 7))
        {
            (*strPtr) += 7;
            state->glitterStrength = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "shadow", 6))
        {
            (*strPtr) += 6;
            state->shadowStrength = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "tracking", 8))
        {
            (*strPtr) += 8;
            state->tracking = parseFloat(&(*strPtr));
        }
        else
        {
            // Perhaps a font name?
            fontid_t fontId;
            if(!strnicmp((*strPtr), "font", 4))
            {
                (*strPtr) += 4;
                if((fontId = FR_SafeFontIdForName(*strPtr)))
                {
                    (*strPtr) += Str_Length(BitmapFont_Name(FR_Font(fontId)));
                    state->font = fontId;
                    continue;
                }

                Con_Message("Warning:parseParamaterBlock: Unknown font '%s'.\n", (*strPtr));
                (*strPtr) = M_FindWhite((*strPtr));
                continue;
            }

            // Unknown, skip it.
            if(*(*strPtr) != '}')
                (*strPtr)++;
        }
    }

    // Skip over the closing brace.
    if(*(*strPtr))
        (*strPtr)++;
}

static void initDrawTextState(drawtextstate_t* state)
{
    fr_state_attributes_t* sat = currentAttributes();
    state->font = BitmapFont_Id(fonts[fr.fontIdx]);
    state->tracking = DEFAULT_TRACKING;
    state->scaleX = state->scaleY = 1;
    state->offX = state->offY = 0;
    state->angle = 0;
    state->color[CR] = state->color[CG] = state->color[CB] = state->color[CA] = 1;
    state->glitterStrength = DEFAULT_GLITTER_STRENGTH;
    state->shadowStrength = DEFAULT_SHADOW_STRENGTH;
    state->shadowOffsetX = DEFAULT_SHADOW_XOFFSET;
    state->shadowOffsetY = DEFAULT_SHADOW_YOFFSET;
    state->leading = DEFAULT_LEADING;
    state->typeIn = true;
    state->caseScale = false;
    state->caseMod[0].scale = 1;
    state->caseMod[0].offset = 3;
    state->caseMod[1].scale = 1.25f;
    state->caseMod[1].offset = 0;
}

/**
 * Draw a string of text controlled by parameter blocks.
 */
void FR_DrawText(const char* inString, int x, int y, fontid_t defFont,
    short flags, float defLeading, int defTracking, float defRed, float defGreen, float defBlue, float defAlpha,
    float defGlitter, float defShadow, boolean defCase)
{
#define SMALLBUFF_SIZE          (80)
#define MAX_FRAGMENTLENGTH      (256)

    float cx = (float) x, cy = (float) y, width = 0, extraScale;
    char smallBuff[SMALLBUFF_SIZE+1], *bigBuff = NULL;
    char temp[MAX_FRAGMENTLENGTH+1], *str, *string, *end;
    drawtextstate_t state;
    size_t charCount = 0;
    int curCase = -1, lastLineHeight;
    size_t len;
    fontid_t oldFont = FR_GetCurrentId();

    if(!inString || !inString[0])
        return;

    len = strlen(inString);
    if(len < SMALLBUFF_SIZE)
    {
        memcpy(smallBuff, inString, len);
        smallBuff[len] = '\0';

        str = smallBuff;
    }
    else
    {
        bigBuff = malloc(len+1);
        memcpy(bigBuff, inString, len);
        bigBuff[len] = '\0';

        str = bigBuff;
    }

    initDrawTextState(&state);
    // Apply defaults:
    state.font = defFont;
    state.typeIn = (flags & DTF_NO_TYPEIN) == 0;
    state.color[CR] = defRed;
    state.color[CG] = defGreen;
    state.color[CB] = defBlue;
    state.color[CA] = defAlpha;
    state.glitterStrength = defGlitter;
    state.shadowStrength = defShadow;
    state.tracking = defTracking;
    state.leading = defLeading;
    state.caseScale = defCase;

    FR_SetFont(state.font);
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetTracking(state.tracking);
    FR_SetShadowOffset(state.shadowOffsetX, state.shadowOffsetY);
    FR_SetShadowStrength(state.shadowStrength);
    FR_SetGlitterStrength(state.glitterStrength);
    lastLineHeight = FR_CharHeight('A') * state.scaleY;

    string = str;
    while(*string)
    {
        // Parse and draw the replacement string.
        if(*string == '{') // Parameters included?
        {
            fontid_t lastFont = state.font;
            int lastTracking = state.tracking;
            float lastLeading = state.leading;
            float lastShadowStrength = state.shadowStrength;
            float lastGlitterStrength = state.glitterStrength;
            int numBreaks = 0;

            parseParamaterBlock(&string, &state, &numBreaks);

            if(numBreaks != 0)
            {
                do
                {
                    cx = (float) x;
                    cy += lastLineHeight * (1+lastLeading);
                } while(--numBreaks > 0);
            }

            if(state.font != lastFont)
                FR_SetFont(state.font);
            if(state.tracking != lastTracking)
                FR_SetTracking(state.tracking);
            if(state.shadowStrength != lastShadowStrength)
                FR_SetShadowStrength(state.shadowStrength);
            if(state.glitterStrength != lastGlitterStrength)
                FR_SetGlitterStrength(state.glitterStrength);
        }

        for(end = string; *end && *end != '{';)
        {
            boolean newline = false;
            short fragmentFlags;
            float alignx = 0;

            // Find the end of the next fragment.
            if(state.caseScale)
            {
                curCase = -1;
                // Select a substring with characters of the same case (or whitespace).
                for(; *end && *end != '{' && *end != '\n'; end++)
                {
                    // We can other skip whitespace.
                    if(isspace(*end))
                        continue;

                    if(curCase < 0)
                        curCase = (isupper(*end) != 0);
                    else if(curCase != (isupper(*end) != 0))
                        break;
                }
            }
            else
            {
                curCase = 0;
                for(; *end && *end != '{' && *end != '\n'; end++);
            }

            strncpy(temp, string, MIN_OF(MAX_FRAGMENTLENGTH, end - string));
            temp[MIN_OF(MAX_FRAGMENTLENGTH, end - string)] = '\0';

            if(end && *end == '\n')
            {
                newline = true;
                ++end;
            }

            // Continue from here.
            string = end;

            if(!(flags & (DTF_ALIGN_LEFT|DTF_ALIGN_RIGHT)))
            {
                fragmentFlags = flags;
            }
            else
            {
                // We'll take care of horizontal positioning of the fragment so align left.
                fragmentFlags = (flags & ~(DTF_ALIGN_RIGHT)) | DTF_ALIGN_LEFT;
                if(flags & DTF_ALIGN_RIGHT)
                    alignx = -FR_TextFragmentWidth(temp) * state.scaleX;
            }

            // Setup the scaling.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            // Rotate.
            if(state.angle != 0)
            {
                // The origin is the specified (x,y) for the patch.
                // We'll undo the VGA aspect ratio (otherwise the result would
                // be skewed).
                glTranslatef((float)x, (float)y, 0);
                glScalef(1, 200.0f / 240.0f, 1);
                glRotatef(state.angle, 0, 0, 1);
                glScalef(1, 240.0f / 200.0f, 1);
                glTranslatef(-(float)x, -(float)y, 0);
            }

            glTranslatef(cx + state.offX + alignx, cy + state.offY + (state.caseScale ? state.caseMod[curCase].offset : 0), 0);
            extraScale = (state.caseScale ? state.caseMod[curCase].scale : 1);
            glScalef(state.scaleX, state.scaleY * extraScale, 1);

            // Draw it.
            glColor4fv(state.color);
            FR_DrawTextFragment3(temp, 0, 0, fragmentFlags, state.typeIn ? (int) charCount : 0);
            charCount += strlen(temp);

            // Advance the current position?
            if(!newline)
            {
                cx += (float) FR_TextFragmentWidth(temp) * state.scaleX;
            }
            else
            {
                if(strlen(temp) > 0)
                    lastLineHeight = FR_TextFragmentHeight(temp);

                cx = (float) x;
                cy += (float) lastLineHeight * (1+state.leading);
            }

            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }
    }

    // Free temporary storage.
    if(bigBuff)
        free(bigBuff);

    FR_PopAttrib();

#undef MAX_FRAGMENTLENGTH
#undef SMALLBUFF_SIZE
}

void FR_TextDimensions(int* width, int* height, const char* string, fontid_t fontId)
{
    if(!width && !height)
        return;
    if(width)
        *width = FR_TextWidth(string, fontId);
    if(height)
        *height = FR_TextHeight(string, fontId);
}

/**
 * Find the visible width of the whole formatted text block.
 * Skips parameter blocks eg "{param}Text" = 4 chars
 */
int FR_TextWidth(const char* string, fontid_t fontId)
{
    fontid_t oldFontId = FR_GetCurrentId();
    int w, maxWidth = -1;
    boolean skip = false;
    const char* ch;
    size_t i, len;

    if(!string || !string[0])
        return 0;

    FR_SetFont(fontId);
    w = 0;
    len = strlen(string);
    ch = string;
    for(i = 0; i < len; ++i, ch++)
    {
        unsigned char c = *ch;

        if(c == '{')
            skip = true;
        else if(c == '}')
            skip = false;
        if(skip)
            continue;

        if(c == '\n')
        {
            if(w > maxWidth)
                maxWidth = w;
            w = 0;
            continue;
        }

        w += FR_CharWidth(c);

        if(i == len - 1 && maxWidth == -1)
        {
            maxWidth = w;
        }
    }

    FR_SetFont(oldFontId);
    return maxWidth;
}

/**
 * Find the visible height of the whole formatted text block.
 * Skips parameter blocks eg "{param}Text" = 4 chars
 */
int FR_TextHeight(const char* string, fontid_t fontId)
{
    fontid_t oldFontId = FR_GetCurrentId();
    int h, currentLineHeight;
    boolean skip = false;
    const char* ch;
    size_t i, len;

    if(!string || !string[0])
        return 0;
    
    FR_SetFont(fontId);
    currentLineHeight = 0;
    len = strlen(string);
    h = 0;
    ch = string;
    for(i = 0; i < len; ++i, ch++)
    {
        unsigned char c = *ch;
        int charHeight;

        if(c == '{')
            skip = true;
        else if(c == '}')
            skip = false;
        if(skip)
            continue;

        if(c == '\n')
        {
            h += currentLineHeight == 0? FR_CharHeight('A') : currentLineHeight;
            currentLineHeight = 0;
            continue;
        }

        charHeight = FR_CharHeight(c);
        if(charHeight > currentLineHeight)
            currentLineHeight = charHeight;
    }
    h += currentLineHeight;

    FR_SetFont(oldFontId);
    return h;
}
