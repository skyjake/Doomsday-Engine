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
#include "de_ui.h"

#include "font.h"
#include "bitmapfont.h"
#include "m_misc.h"

/**
 * @ingroup drawTextFlags
 * @{
 */
#define DTF_INTERNAL_MASK               0xff00
#define DTF_NO_CHARACTER                0x8000 /// Only draw text decorations.
/**@}*/

typedef struct fr_state_attributes_s {
    int tracking;
    float leading;
    float rgba[4];
    int shadowOffsetX, shadowOffsetY;
    float shadowStrength;
    float glitterStrength;
    boolean caseScale;
} fr_state_attributes_t;

// Used with FR_LoadDefaultAttribs
static fr_state_attributes_t defaultAttribs = {
    FR_DEF_ATTRIB_TRACKING,
    FR_DEF_ATTRIB_LEADING,
    { FR_DEF_ATTRIB_COLOR_RED, FR_DEF_ATTRIB_COLOR_GREEN, FR_DEF_ATTRIB_COLOR_BLUE, FR_DEF_ATTRIB_ALPHA },
    FR_DEF_ATTRIB_SHADOW_XOFFSET,
    FR_DEF_ATTRIB_SHADOW_YOFFSET,
    FR_DEF_ATTRIB_SHADOW_STRENGTH,
    FR_DEF_ATTRIB_GLITTER_STRENGTH,
    FR_DEF_ATTRIB_CASE_SCALE
};

typedef struct {
    fontid_t fontNum;
    int attribStackDepth;
    fr_state_attributes_t attribStack[FR_MAX_ATTRIB_STACK_DEPTH];
} fr_state_t;
static fr_state_t fr;
fr_state_t* frState = &fr;

typedef struct {
    fontid_t fontNum;
    float scaleX, scaleY;
    float offX, offY;
    float angle;
    float rgba[4];
    float glitterStrength, shadowStrength;
    int shadowOffsetX, shadowOffsetY;
    int tracking;
    float leading;
    int lastLineHeight;
    boolean typeIn;
    boolean caseScale;
    struct {
        float scale, offset;
    } caseMod[2]; // 1=upper, 0=lower
} drawtextstate_t;

static __inline fr_state_attributes_t* currentAttribs(void);
static int topToAscent(font_t* font);
static int lineHeight(font_t* font, unsigned char ch);
static void drawChar(unsigned char ch, int posX, int posY, font_t* font, int alignFlags, short textFlags);
static void drawFlash(int x, int y, int w, int h, int bright);

static int inited = false;

static char smallTextBuffer[FR_SMALL_TEXT_BUFFER_SIZE+1];
static char* largeTextBuffer = NULL;
static size_t largeTextBufferSize = 0;

static int typeInTime;

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: font renderer module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static int topToAscent(font_t* font)
{
    int lineHeight = Fonts_Leading(font);
    if(lineHeight == 0)
        return 0;
    return lineHeight - Fonts_Ascent(font);
}

static int lineHeight(font_t* font, unsigned char ch)
{
    int ascent = Fonts_Ascent(font);
    if(ascent != 0)
        return ascent;
    return Fonts_CharHeight(font, ch);
}

static __inline fr_state_attributes_t* currentAttribs(void)
{
    return fr.attribStack + fr.attribStackDepth;
}

void FR_Init(void)
{
    // No reinitializations...
    if(inited) return;
    if(isDedicated) return;

    inited = true;
    fr.fontNum = 0;
    FR_LoadDefaultAttrib();
    typeInTime = 0;
}

void FR_Shutdown(void)
{
    if(!inited) return;
    inited = false;
}

boolean FR_Available(void)
{
    return inited;
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

/// \note Member of the Doomsday public API.
void FR_ResetTypeinTimer(void)
{
    errorIfNotInited("FR_ResetTypeinTimer");
    typeInTime = 0;
}

/// \note Member of the Doomsday public API.
void FR_SetFont(fontid_t num)
{
    errorIfNotInited("FR_SetFont");
    if(!Fonts_ToFont(num))
        return; // No such font.
    fr.fontNum = num;
}

void FR_SetNoFont(void)
{
    errorIfNotInited("FR_SetNoFont");
    fr.fontNum = 0;
}

/// \note Member of the Doomsday public API.
fontid_t FR_Font(void)
{
    errorIfNotInited("FR_Font");
    return fr.fontNum;
}

/// \note Member of the Doomsday public API.
void FR_LoadDefaultAttrib(void)
{
    errorIfNotInited("FR_LoadDefaultAttrib");
    memcpy(currentAttribs(), &defaultAttribs, sizeof(defaultAttribs));
}

/// \note Member of the Doomsday public API.
void FR_PushAttrib(void)
{
    errorIfNotInited("FR_PushAttrib");
    if(fr.attribStackDepth == FR_MAX_ATTRIB_STACK_DEPTH)
    {
        Con_Error("FR_PushAttrib: STACK_OVERFLOW.");
        exit(1); // Unreachable.
    }
    memcpy(fr.attribStack + fr.attribStackDepth + 1, fr.attribStack + fr.attribStackDepth, sizeof(fr.attribStack[0]));
    ++fr.attribStackDepth;
}

/// \note Member of the Doomsday public API.
void FR_PopAttrib(void)
{
    errorIfNotInited("FR_PopAttrib");
    if(fr.attribStackDepth == 0)
    {
        Con_Error("FR_PopAttrib: STACK_UNDERFLOW.");
        exit(1); // Unreachable.
    }
    --fr.attribStackDepth;
}

/// \note Member of the Doomsday public API.
float FR_Leading(void)
{
    errorIfNotInited("FR_Leading");
    return currentAttribs()->leading;
}

/// \note Member of the Doomsday public API.
void FR_SetLeading(float value)
{
    errorIfNotInited("FR_SetLeading");
    currentAttribs()->leading = value;
}

/// \note Member of the Doomsday public API.
int FR_Tracking(void)
{
    errorIfNotInited("FR_Tracking");
    return currentAttribs()->tracking;
}

/// \note Member of the Doomsday public API.
void FR_SetTracking(int value)
{
    errorIfNotInited("FR_SetTracking");
    currentAttribs()->tracking = value;
}

/// \note Member of the Doomsday public API.
void FR_ColorAndAlpha(float rgba[4])
{
    errorIfNotInited("FR_ColorAndAlpha");
    memcpy(rgba, currentAttribs()->rgba, sizeof(rgba));
}

/// \note Member of the Doomsday public API.
void FR_SetColor(float red, float green, float blue)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColor");
    sat->rgba[CR] = red;
    sat->rgba[CG] = green;
    sat->rgba[CB] = blue;
}

/// \note Member of the Doomsday public API.
void FR_SetColorv(const float rgba[3])
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColorv");
    sat->rgba[CR] = rgba[CR];
    sat->rgba[CG] = rgba[CG];
    sat->rgba[CB] = rgba[CB];
}

/// \note Member of the Doomsday public API.
void FR_SetColorAndAlpha(float red, float green, float blue, float alpha)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColorAndAlpha");
    sat->rgba[CR] = red;
    sat->rgba[CG] = green;
    sat->rgba[CB] = blue;
    sat->rgba[CA] = alpha;
}

/// \note Member of the Doomsday public API.
void FR_SetColorAndAlphav(const float rgba[4])
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColorAndAlphav");
    sat->rgba[CR] = rgba[CR];
    sat->rgba[CG] = rgba[CG];
    sat->rgba[CB] = rgba[CB];
    sat->rgba[CA] = rgba[CA];
}

/// \note Member of the Doomsday public API.
float FR_ColorRed(void)
{
    errorIfNotInited("FR_ColorRed");
    return currentAttribs()->rgba[CR];
}

/// \note Member of the Doomsday public API.
void FR_SetColorRed(float value)
{
    errorIfNotInited("FR_SetColorRed");
    currentAttribs()->rgba[CR] = value;
}

/// \note Member of the Doomsday public API.
float FR_ColorGreen(void)
{
    errorIfNotInited("FR_ColorGreen");
    return currentAttribs()->rgba[CG];
}

/// \note Member of the Doomsday public API.
void FR_SetColorGreen(float value)
{
    errorIfNotInited("FR_SetColorGreen");
    currentAttribs()->rgba[CG] = value;
}

/// \note Member of the Doomsday public API.
float FR_ColorBlue(void)
{
    errorIfNotInited("FR_ColorBlue");
    return currentAttribs()->rgba[CB];
}

/// \note Member of the Doomsday public API.
void FR_SetColorBlue(float value)
{
    errorIfNotInited("FR_SetColorBlue");
    currentAttribs()->rgba[CB] = value;
}

/// \note Member of the Doomsday public API.
float FR_Alpha(void)
{
    errorIfNotInited("FR_Alpha");
    return currentAttribs()->rgba[CA];
}

/// \note Member of the Doomsday public API.
void FR_SetAlpha(float value)
{
    errorIfNotInited("FR_SetAlpha");
    currentAttribs()->rgba[CA] = value;
}

/// \note Member of the Doomsday public API.
void FR_ShadowOffset(int* offsetX, int* offsetY)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_ShadowOffset");
    if(NULL != offsetX) *offsetX = sat->shadowOffsetX;
    if(NULL != offsetY) *offsetY = sat->shadowOffsetY;
}

/// \note Member of the Doomsday public API.
void FR_SetShadowOffset(int offsetX, int offsetY)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetShadowOffset");
    sat->shadowOffsetX = offsetX;
    sat->shadowOffsetY = offsetY;
}

/// \note Member of the Doomsday public API.
float FR_ShadowStrength(void)
{
    errorIfNotInited("FR_ShadowStrength");
    return currentAttribs()->shadowStrength;
}

/// \note Member of the Doomsday public API.
void FR_SetShadowStrength(float value)
{
    errorIfNotInited("FR_SetShadowStrength");
    currentAttribs()->shadowStrength = MINMAX_OF(0, value, 1);
}

/// \note Member of the Doomsday public API.
float FR_GlitterStrength(void)
{
    errorIfNotInited("FR_GlitterStrength");
    return currentAttribs()->glitterStrength;
}

/// \note Member of the Doomsday public API.
void FR_SetGlitterStrength(float value)
{
    errorIfNotInited("FR_SetGlitterStrength");
    currentAttribs()->glitterStrength = MINMAX_OF(0, value, 1);
}

/// \note Member of the Doomsday public API.
boolean FR_CaseScale(void)
{
    errorIfNotInited("FR_CaseScale");
    return currentAttribs()->caseScale;
}

/// \note Member of the Doomsday public API.
void FR_SetCaseScale(boolean value)
{
    errorIfNotInited("FR_SetCaseScale");
    currentAttribs()->caseScale = value;
}

/// \note Member of the Doomsday public API.
void FR_CharDimensions(int* width, int* height, unsigned char ch)
{
    errorIfNotInited("FR_CharDimensions");
    Fonts_CharDimensions(Fonts_ToFont(fr.fontNum), width, height, ch);
}

/// \note Member of the Doomsday public API.
int FR_CharWidth(unsigned char ch)
{
    errorIfNotInited("FR_CharWidth");
    if(fr.fontNum != 0)
        return Fonts_CharWidth(Fonts_ToFont(fr.fontNum), ch);
    return 0;
}

/// \note Member of the Doomsday public API.
int FR_CharHeight(unsigned char ch)
{
    errorIfNotInited("FR_CharHeight");
    if(fr.fontNum != 0)
        return Fonts_CharHeight(Fonts_ToFont(fr.fontNum), ch);
    return 0;
}

int FR_SingleLineHeight(const char* text)
{
    errorIfNotInited("FR_SingleLineHeight");
    if(fr.fontNum == 0 || !text)
        return 0;
    { int ascent = Fonts_Ascent(Fonts_ToFont(fr.fontNum));
    if(ascent != 0)
        return ascent;
    }
    return Fonts_CharHeight(Fonts_ToFont(fr.fontNum), (unsigned char)text[0]);
}

int FR_GlyphTopToAscent(const char* text)
{
    int lineHeight;
    errorIfNotInited("FR_GlyphTopToAscent");
    if(fr.fontNum == 0 || !text)
        return 0;
    lineHeight = Fonts_Leading(Fonts_ToFont(fr.fontNum));
    if(lineHeight == 0)
        return 0;
    return lineHeight - Fonts_Ascent(Fonts_ToFont(fr.fontNum));
}

static int textFragmentWidth(const char* fragment)
{
    assert(NULL != fragment);
    {
    size_t i, len;
    int width = 0;
    const char* ch;
    unsigned char c;

    if(fr.fontNum == 0)
    {
        Con_Error("textFragmentHeight: Cannot determine height without a current font.");
        exit(1);
    }

    // Just add them together.
    len = strlen(fragment);
    i = 0;
    ch = fragment;
    while(i++ < len && (c = *ch++) != 0 && c != '\n')
        width += FR_CharWidth(c);

    return (int) (width + currentAttribs()->tracking * (len-1));
    }
}

static int textFragmentHeight(const char* fragment)
{
    assert(NULL != fragment);
    {
    const char* ch;
    unsigned char c;
    int height = 0;
    size_t len;
    uint i;
 
    if(fr.fontNum == 0)
    {
        Con_Error("textFragmentHeight: Cannot determine height without a current font.");
        exit(1);
    }

    // Find the greatest height.
    i = 0;
    height = 0;
    len = strlen(fragment);
    ch = fragment;
    while(i++ < len && (c = *ch++) != 0 && c != '\n')
    {
        height = MAX_OF(height, FR_CharHeight(c));
    }

    return topToAscent(Fonts_ToFont(fr.fontNum)) + height;
    }
}

static void textFragmentDimensions(int* width, int* height, const char* fragment)
{
    if(NULL != width)  *width  = textFragmentWidth(fragment);
    if(NULL != height) *height = textFragmentHeight(fragment);
}

static void textFragmentDrawer(const char* fragment, int x, int y, int alignFlags,
    short textFlags, int initialCount)
{
    assert(fragment && fragment[0] && fr.fontNum != 0);
    {
    font_t* font = Fonts_ToFont(fr.fontNum);
    fr_state_attributes_t* sat = currentAttribs();
    boolean noTypein = (textFlags & DTF_NO_TYPEIN) != 0;
    boolean noGlitter = (sat->glitterStrength <= 0 || (textFlags & DTF_NO_GLITTER) != 0);
    boolean noShadow  = (sat->shadowStrength  <= 0 || (textFlags & DTF_NO_SHADOW)  != 0 ||
                         (Font_Flags(font) & FF_SHADOWED) != 0);
    boolean noCharacter = (textFlags & DTF_NO_CHARACTER) != 0;
    float glitter = (noGlitter? 0 : sat->glitterStrength), glitterMul;
    float shadow  = (noShadow ? 0 : sat->shadowStrength), shadowMul;
    float flashColor[3] = { 0, 0, 0 };
    int w, h, cx, cy, count, yoff;
    unsigned char c;
    const char* ch;

    if(alignFlags & ALIGN_RIGHT)
        x -= textFragmentWidth(fragment);
    else if(!(alignFlags & ALIGN_LEFT))
        x -= textFragmentWidth(fragment)/2;

    if(alignFlags & ALIGN_BOTTOM)
        y -= textFragmentHeight(fragment);
    else if(!(alignFlags & ALIGN_TOP))
        y -= textFragmentHeight(fragment)/2;

    if(!(noTypein && noGlitter))
    {
        flashColor[CR] = (1 + 2 * sat->rgba[CR]) / 3;
        flashColor[CG] = (1 + 2 * sat->rgba[CG]) / 3;
        flashColor[CB] = (1 + 2 * sat->rgba[CB]) / 3;
    }

    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_TEXTURE_2D);
    }
    if(Font_Type(font) == FT_BITMAP && 0 != BitmapFont_GLTextureName(font))
    {
        glBindTexture(GL_TEXTURE_2D, BitmapFont_GLTextureName(font));
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glScalef(1.f / BitmapFont_TextureWidth(font),
                 1.f / BitmapFont_TextureHeight(font), 1.f);
    }

    { int pass;
    for(pass = (noShadow? 1 : 0); pass < (noCharacter && noGlitter? 1 : 2); ++pass)
    {
        count = initialCount;
        ch = fragment;
        cx = x + (pass == 0? sat->shadowOffsetX : 0);
        cy = y + (pass == 0? sat->shadowOffsetY : 0);

        for(;;)
        {
            c = *ch++;
            yoff = 0;

            glitter = (noGlitter? 0 : sat->glitterStrength);
            glitterMul = 0;

            shadow = (noShadow? 0 : sat->shadowStrength);
            shadowMul = (noShadow? 0 : sat->rgba[CA]);

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
                            flashColor[CR] = sat->rgba[CR];
                            flashColor[CG] = sat->rgba[CG];
                            flashColor[CB] = sat->rgba[CB];
                        }
                        else if(count + 1 == maxCount)
                        {
                            glitterMul = 0.88f;
                            flashColor[CR] = (1 + sat->rgba[CR]) / 2;
                            flashColor[CG] = (1 + sat->rgba[CG]) / 2;
                            flashColor[CB] = (1 + sat->rgba[CB]) / 2;
                        }
                        else if(count + 2 == maxCount)
                        {
                            glitterMul = 0.75f;
                            flashColor[CR] = sat->rgba[CR];
                            flashColor[CG] = sat->rgba[CG];
                            flashColor[CB] = sat->rgba[CB];
                        }
                        else if(count + 3 == maxCount)
                        {
                            glitterMul = 0.5f;
                            flashColor[CR] = sat->rgba[CR];
                            flashColor[CG] = sat->rgba[CG];
                            flashColor[CB] = sat->rgba[CB];
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

            if(' ' != c)
            {
                // A non-white-space character we have a glyph for.
                if(pass)
                {
                    if(!noCharacter)
                    {
                        // The character itself.
                        glColor4fv(sat->rgba);
                        drawChar(c, cx, cy + yoff, font, ALIGN_TOPLEFT, DTF_NO_EFFECTS);
                    }

                    if(!noGlitter && glitter > 0)
                    {
                        // Do something flashy.
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
    if(renderWireframe)
    {
        /// \fixme do not assume previous state.
        glEnable(GL_TEXTURE_2D);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if(Font_Type(font) == FT_BITMAP && 0 != BitmapFont_GLTextureName(font))
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }
    }
}

/// \note Member of the Doomsday public API.
void FR_DrawChar3(unsigned char ch, int x, int y, int alignFlags, short textFlags)
{
    char str[2];
    str[0] = ch;
    str[1] = '\0';
    FR_DrawText3(str, x, y, alignFlags, textFlags);
}

/// \note Member of the Doomsday public API.
void FR_DrawChar2(unsigned char ch, int x, int y, int alignFlags)
{
    FR_DrawChar3(ch, x, y, alignFlags, DEFAULT_DRAWFLAGS);
}

/// \note Member of the Doomsday public API.
void FR_DrawChar(unsigned char ch, int x, int y)
{
    FR_DrawChar2(ch, x, y, DEFAULT_ALIGNFLAGS);
}

static void drawChar(unsigned char ch, int posX, int posY, font_t* font,
    int alignFlags, short textFlags)
{
    float x = (float) posX, y = (float) posY;

    if(alignFlags & ALIGN_RIGHT)
        x -= Fonts_CharWidth(font, ch);
    else if(!(alignFlags & ALIGN_LEFT))
        x -= Fonts_CharWidth(font, ch) / 2;

    if(alignFlags & ALIGN_BOTTOM)
        y -= topToAscent(font) + lineHeight(font, ch);
    else if(!(alignFlags & ALIGN_TOP))
        y -= (topToAscent(font) + lineHeight(font, ch))/2;

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(x, y, 0);

    switch(Font_Type(font))
    {
    case FT_BITMAP: {
        bitmapfont_t* bf = (bitmapfont_t*)font;

        if(0 != BitmapFont_GLTextureName(font))
            GL_BindTexture(BitmapFont_GLTextureName(font), GL_NEAREST);

        if(bf->_chars[ch].dlist)
        {
            GL_CallList(bf->_chars[ch].dlist);
        }
        else
        {
            int s[2], t[2], x = 0, y = 0, w, h;

            BitmapFont_CharCoords(font, &s[0], &s[1], &t[0], &t[1], ch);
            w = s[1] - s[0];
            h = t[1] - t[0];

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
        break;
      }
    case FT_BITMAPCOMPOSITE: {
        bitmapcompositefont_t* cf = (bitmapcompositefont_t*)font;
        patchid_t patch = BitmapCompositeFont_CharPatch(font, ch);

        if(patch)
        {
            glBindTexture(GL_TEXTURE_2D, GL_PreparePatchTexture(Textures_ToTexture(Textures_TextureForUniqueId(TN_PATCHES, patch))));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else
        {
            GL_SetNoTexture();
        }

        if(cf->_chars[ch].dlist)
        {
            GL_CallList(cf->_chars[ch].dlist);
        }
        else
        {
            int s[2], t[2], x = 0, y = 0, w, h;

            BitmapCompositeFont_CharCoords(font, &s[0], &s[1], &t[0], &t[1], ch);
         
            x = cf->_chars[ch].x;
            y = cf->_chars[ch].y;
            w = BitmapCompositeFont_CharWidth(font, ch);
            h = BitmapCompositeFont_CharHeight(font, ch);
            if(patch != 0)
            {
                w += 2;
                h += 2;
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
        break;
      }
    default:
        Con_Error("FR_DrawChar: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(-x, -y, 0);
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

static void parseParamaterBlock(char** strPtr, drawtextstate_t* state, int* numBreaks)
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
            state->rgba[CR] = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "g", 1))
        {
            (*strPtr)++;
            state->rgba[CG] = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "b", 1))
        {
            (*strPtr)++;
            state->rgba[CB] = parseFloat(&(*strPtr));
        }
        else if(!strnicmp((*strPtr), "a", 1))
        {
            (*strPtr)++;
            state->rgba[CA] = parseFloat(&(*strPtr));
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
                Uri* uri;
                (*strPtr) += 4;
                uri = Uri_NewWithPath2(*strPtr, RC_NULL);
                fontId = Fonts_ResolveUri2(uri, true/*quiet please*/);
                Uri_Delete(uri);
                if(fontId != NOFONTID)
                {
                    (*strPtr) += strlen(*strPtr);
                    state->fontNum = fontId;
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

static void initDrawTextState(drawtextstate_t* state, short textFlags)
{
    fr_state_attributes_t* sat = currentAttribs();

    state->typeIn = (textFlags & DTF_NO_TYPEIN) == 0;
    state->fontNum = fr.fontNum;
    memcpy(state->rgba, sat->rgba, sizeof(state->rgba));
    state->tracking = sat->tracking;
    state->glitterStrength = sat->glitterStrength;
    state->shadowStrength = sat->shadowStrength;
    state->shadowOffsetX = sat->shadowOffsetX;
    state->shadowOffsetY = sat->shadowOffsetY;
    state->leading = sat->leading;
    state->caseScale = sat->caseScale;

    state->scaleX = state->scaleY = 1;
    state->offX = state->offY = 0;
    state->angle = 0;
    state->typeIn = true;
    state->caseMod[0].scale = 1;
    state->caseMod[0].offset = 3;
    state->caseMod[1].scale = 1.25f;
    state->caseMod[1].offset = 0;
    state->lastLineHeight = FR_CharHeight('A') * state->scaleY * (1+state->leading);

    FR_PushAttrib();
}

static char* enlargeTextBuffer(size_t lengthMinusTerminator)
{
    if(lengthMinusTerminator <= FR_SMALL_TEXT_BUFFER_SIZE)
    {
        return smallTextBuffer;
    }
    if(largeTextBuffer == NULL || lengthMinusTerminator > largeTextBufferSize)
    {
        largeTextBufferSize = lengthMinusTerminator;
        largeTextBuffer = (char*)realloc(largeTextBuffer, largeTextBufferSize+1);
        if(largeTextBuffer == NULL)
            Con_Error("FR_EnlargeTextBuffer: Failed on reallocation of %lu bytes.",
                (unsigned long)(lengthMinusTerminator+1));
    }
    return largeTextBuffer;
}

static void freeTextBuffer(void)
{
    if(largeTextBuffer == NULL)
        return;
    free(largeTextBuffer); largeTextBuffer = 0;
    largeTextBufferSize = 0;
}

/// \note Member of the Doomsday public API.
void FR_DrawText3(const char* text, int x, int y, int alignFlags, short origTextFlags)
{
    fontid_t origFont = FR_Font();
    float cx, cy, extraScale;
    drawtextstate_t state;
    const char* fragment;
    int pass, curCase;
    size_t charCount;
    float origColor[4];
    short textFlags;
    char* str, *end;

    errorIfNotInited("FR_DrawText");

    if(!text || !text[0])
        return;

    origTextFlags &= ~(DTF_INTERNAL_MASK);

    // We need to change the current color, so remember for restore.
    glGetFloatv(GL_CURRENT_COLOR, origColor);

    for(pass = ((origTextFlags & DTF_NO_SHADOW)  != 0? 1 : 0);
        pass < ((origTextFlags & DTF_NO_GLITTER) != 0? 2 : 3); ++pass)
    {
        // Configure the next pass.
        cx = (float) x;
        cy = (float) y;
        curCase = -1;
        charCount = 0;
        switch(pass)
        {
        case 0: textFlags = origTextFlags | (DTF_NO_GLITTER|DTF_NO_CHARACTER); break;
        case 1: textFlags = origTextFlags | (DTF_NO_SHADOW |DTF_NO_GLITTER);   break;
        case 2: textFlags = origTextFlags | (DTF_NO_SHADOW |DTF_NO_CHARACTER); break;
        }

        // Apply defaults.
        initDrawTextState(&state, textFlags);

        str = (char*)text;
        while(*str)
        {
            if(*str == '{') // Paramaters included?
            {
                fontid_t lastFont = state.fontNum;
                int lastTracking = state.tracking;
                float lastLeading = state.leading;
                float lastShadowStrength = state.shadowStrength;
                float lastGlitterStrength = state.glitterStrength;
                boolean lastCaseScale = state.caseScale;
                float lastRGBA[4];
                int numBreaks = 0;
                
                lastRGBA[CR] = state.rgba[CR];
                lastRGBA[CG] = state.rgba[CG];
                lastRGBA[CB] = state.rgba[CB];
                lastRGBA[CA] = state.rgba[CA];

                parseParamaterBlock(&str, &state, &numBreaks);

                if(numBreaks != 0)
                {
                    do
                    {
                        cx = (float) x;
                        cy += state.lastLineHeight * (1+lastLeading);
                    } while(--numBreaks > 0);
                }

                if(state.fontNum != lastFont)
                    FR_SetFont(state.fontNum);
                if(state.tracking != lastTracking)
                    FR_SetTracking(state.tracking);
                if(state.leading != lastLeading)
                    FR_SetLeading(state.leading);
                if(state.rgba[CR] != lastRGBA[CR] || state.rgba[CG] != lastRGBA[CG] || state.rgba[CB] != lastRGBA[CB] || state.rgba[CA] != lastRGBA[CA])
                    FR_SetColorAndAlphav(state.rgba);
                if(state.shadowStrength != lastShadowStrength)
                    FR_SetShadowStrength(state.shadowStrength);
                if(state.glitterStrength != lastGlitterStrength)
                    FR_SetGlitterStrength(state.glitterStrength);
                if(state.caseScale != lastCaseScale)
                    FR_SetCaseScale(state.caseScale);
            }

            for(end = str; *end && *end != '{';)
            {
                int newlines = 0, fragmentAlignFlags;
                float alignx = 0;

                // Find the end of the next fragment.
                if(FR_CaseScale())
                {
                    curCase = -1;
                    // Select a substring with characters of the same case (or whitespace).
                    for(; *end && *end != '{' && *end != '\n'; end++)
                    {
                        // We can skip whitespace.
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

                { char* buffer = enlargeTextBuffer(end - str);
                memcpy(buffer, str, end - str);
                buffer[end - str] = '\0';
                fragment = buffer;
                }

                while(*end == '\n')
                {
                    newlines++;
                    end++;
                }

                // Continue from here.
                str = end;

                if(!(alignFlags & (ALIGN_LEFT|ALIGN_RIGHT)))
                {
                    fragmentAlignFlags = alignFlags;
                }
                else
                {
                    // We'll take care of horizontal positioning of the fragment so align left.
                    fragmentAlignFlags = (alignFlags & ~(ALIGN_RIGHT)) | ALIGN_LEFT;
                    if(alignFlags & ALIGN_RIGHT)
                        alignx = -textFragmentWidth(fragment) * state.scaleX;
                }

                // Setup the scaling.
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();

                // Rotate.
                if(state.angle != 0)
                {
                    // The origin is the specified (x,y) for the patch.
                    // We'll undo the aspect ratio (otherwise the result would be skewed).
                    /// \fixme Do not assume the aspect ratio and therefore whether
                    // correction is even needed.
                    glTranslatef((float)x, (float)y, 0);
                    glScalef(1, 200.0f / 240.0f, 1);
                    glRotatef(state.angle, 0, 0, 1);
                    glScalef(1, 240.0f / 200.0f, 1);
                    glTranslatef(-(float)x, -(float)y, 0);
                }

                glTranslatef(cx + state.offX + alignx, cy + state.offY + (FR_CaseScale() ? state.caseMod[curCase].offset : 0), 0);
                extraScale = (FR_CaseScale() ? state.caseMod[curCase].scale : 1);
                glScalef(state.scaleX, state.scaleY * extraScale, 1);

                // Draw it.
                textFragmentDrawer(fragment, 0, 0, fragmentAlignFlags, textFlags, state.typeIn ? (int) charCount : DEFAULT_INITIALCOUNT);
                charCount += strlen(fragment);

                // Advance the current position?
                if(newlines == 0)
                {
                    cx += ((float) textFragmentWidth(fragment) + currentAttribs()->tracking) * state.scaleX;
                }
                else
                {
                    if(strlen(fragment) > 0)
                        state.lastLineHeight = textFragmentHeight(fragment);

                    cx = (float) x;
                    cy += newlines * (float) state.lastLineHeight * (1+FR_Leading());
                }

                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
            }
        }

        FR_PopAttrib();
    }

    freeTextBuffer();

    FR_SetFont(origFont);
    glColor4fv(origColor);
}

/// \note Member of the Doomsday public API.
void FR_DrawText2(const char* text, int x, int y, int alignFlags)
{
    FR_DrawText3(text, x, y, alignFlags, DEFAULT_DRAWFLAGS);
}

/// \note Member of the Doomsday public API.
void FR_DrawText(const char* text, int x, int y)
{
    FR_DrawText2(text, x, y, DEFAULT_ALIGNFLAGS);
}

/// \note Member of the Doomsday public API.
void FR_TextDimensions(int* width, int* height, const char* text)
{
    if(NULL != width)  *width  = FR_TextWidth(text);
    if(NULL != height) *height = FR_TextHeight(text);
}

/// \note Member of the Doomsday public API.
int FR_TextWidth(const char* string)
{
    int w, maxWidth = -1;
    boolean skip = false;
    const char* ch;
    size_t i, len;

    errorIfNotInited("FR_TextWidth");

    if(!string || !string[0])
        return 0;

    w = 0;
    len = strlen(string);
    ch = string;
    for(i = 0; i < len; ++i, ch++)
    {
        unsigned char c = *ch;

        if(c == '{')
        {
            skip = true;
        }
        else if(c == '}')
        {
            skip = false;
            continue;
        }

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

        if(i != len - 1)
        {
            w += FR_Tracking();
        }
        else if(maxWidth == -1)
        {
            maxWidth = w;
        }
    }

    return maxWidth;
}

/// \note Member of the Doomsday public API.
int FR_TextHeight(const char* string)
{
    int h, currentLineHeight;
    boolean skip = false;
    const char* ch;
    size_t i, len;

    if(!string || !string[0])
        return 0;

    errorIfNotInited("FR_TextHeight");

    currentLineHeight = 0;
    len = strlen(string);
    h = 0;
    ch = string;
    for(i = 0; i < len; ++i, ch++)
    {
        unsigned char c = *ch;
        int charHeight;

        if(c == '{')
        {
            skip = true;
        }
        else if(c == '}')
        {
            skip = false;
            continue;
        }

        if(skip)
            continue;

        if(c == '\n')
        {
            h += currentLineHeight == 0? (FR_CharHeight('A') * (1+FR_Leading())) : currentLineHeight;
            currentLineHeight = 0;
            continue;
        }

        charHeight = FR_CharHeight(c) * (1+FR_Leading());
        if(charHeight > currentLineHeight)
            currentLineHeight = charHeight;
    }
    h += currentLineHeight;

    return h;
}
