/** @file rend_font.cpp  Font Renderer.
 *
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DE_NO_API_MACROS_FONT_RENDER

#include "de_base.h"
#include "render/rend_font.h"
#include "sys_system.h" // novideo

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <de/legacy/concurrency.h>
#include <de/glstate.h>
#include <de/glinfo.h>

#include "resource/bitmapfont.h"
#include "resource/compositebitmapfont.h"

#include "gl/gl_main.h"
#include "gl/gl_draw.h"
#include "gl/gl_texmanager.h"

#include "api_fontrender.h"
#include "render/rend_main.h"

using namespace de;

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
    dd_bool caseScale;
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
    dd_bool typeIn;
    dd_bool caseScale;
    struct {
        float scale, offset;
    } caseMod[2]; // 1=upper, 0=lower
} drawtextstate_t;

static void drawChar(dbyte ch, float posX, float posY, const AbstractFont &font, int alignFlags); //, short textFlags);
static void drawFlash(const Point2Raw *origin, const Size2Raw *size, bool bright);

static int initedFont = false;

static char smallTextBuffer[FR_SMALL_TEXT_BUFFER_SIZE+1];
static char *largeTextBuffer = NULL;
static size_t largeTextBufferSize = 0;

static int typeInTime;

static void errorIfNotInited(const char* callerName)
{
    if (initedFont) return;
    App_Error("%s: font renderer module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static int topToAscent(const AbstractFont &font)
{
    int lineHeight = font.lineSpacing();
    if (lineHeight == 0)
        return 0;
    return lineHeight - font.ascent();
}

static inline fr_state_attributes_t *currentAttribs(void)
{
    return fr.attribStack + fr.attribStackDepth;
}

void FR_Shutdown(void)
{
    if (!initedFont) return;
    initedFont = false;
}

dd_bool FR_Available(void)
{
    return initedFont;
}

void FR_Ticker(timespan_t /*ticLength*/)
{
    if (!initedFont)
        return;

    // Restricted to fixed 35 Hz ticks.
    /// @todo We should not be synced to the games' fixed "sharp" timing.
    ///        This font renderer is used by the engine's UI also.
    if (!DD_IsSharpTick())
        return; // It's too soon.

    ++typeInTime;
}

#undef FR_ResetTypeinTimer
void FR_ResetTypeinTimer(void)
{
    errorIfNotInited("FR_ResetTypeinTimer");
    typeInTime = 0;
}

#undef FR_SetFont
void FR_SetFont(fontid_t num)
{
    errorIfNotInited("FR_SetFont");
    if (num != NOFONTID)
    {
        try
        {
            App_Resources().toFontManifest(num);
            fr.fontNum = num;
            return;
        }
        catch (const ClientResources::UnknownFontIdError &)
        {}
    }
    else
    {
        fr.fontNum = num;
    }
}

void FR_SetNoFont(void)
{
    errorIfNotInited("FR_SetNoFont");
    fr.fontNum = 0;
}

#undef FR_Font
fontid_t FR_Font(void)
{
    errorIfNotInited("FR_Font");
    return fr.fontNum;
}

#undef FR_LoadDefaultAttrib
void FR_LoadDefaultAttrib(void)
{
    errorIfNotInited("FR_LoadDefaultAttrib");
    memcpy(currentAttribs(), &defaultAttribs, sizeof(defaultAttribs));
}

#undef FR_PushAttrib
void FR_PushAttrib(void)
{
    errorIfNotInited("FR_PushAttrib");
    if (fr.attribStackDepth+1 == FR_MAX_ATTRIB_STACK_DEPTH)
    {
        App_Error("FR_PushAttrib: STACK_OVERFLOW.");
        exit(1); // Unreachable.
    }
    memcpy(fr.attribStack + fr.attribStackDepth + 1, fr.attribStack + fr.attribStackDepth, sizeof(fr.attribStack[0]));
    ++fr.attribStackDepth;
}

#undef FR_PopAttrib
void FR_PopAttrib(void)
{
    errorIfNotInited("FR_PopAttrib");
    if (fr.attribStackDepth == 0)
    {
        App_Error("FR_PopAttrib: STACK_UNDERFLOW.");
        exit(1); // Unreachable.
    }
    --fr.attribStackDepth;
}

#undef FR_Leading
float FR_Leading(void)
{
    errorIfNotInited("FR_Leading");
    return currentAttribs()->leading;
}

#undef FR_SetLeading
void FR_SetLeading(float value)
{
    errorIfNotInited("FR_SetLeading");
    currentAttribs()->leading = value;
}

#undef FR_Tracking
int FR_Tracking(void)
{
    errorIfNotInited("FR_Tracking");
    return currentAttribs()->tracking;
}

#undef FR_SetTracking
void FR_SetTracking(int value)
{
    errorIfNotInited("FR_SetTracking");
    currentAttribs()->tracking = value;
}

#undef FR_ColorAndAlpha
void FR_ColorAndAlpha(float rgba[4])
{
    errorIfNotInited("FR_ColorAndAlpha");
    memcpy(rgba, currentAttribs()->rgba, sizeof(rgba[0]) * 4);
}

#undef FR_SetColor
void FR_SetColor(float red, float green, float blue)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColor");
    sat->rgba[CR] = red;
    sat->rgba[CG] = green;
    sat->rgba[CB] = blue;
}

#undef FR_SetColorv
void FR_SetColorv(const float rgba[3])
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColorv");
    sat->rgba[CR] = rgba[CR];
    sat->rgba[CG] = rgba[CG];
    sat->rgba[CB] = rgba[CB];
}

#undef FR_SetColorAndAlpha
void FR_SetColorAndAlpha(float red, float green, float blue, float alpha)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColorAndAlpha");
    sat->rgba[CR] = red;
    sat->rgba[CG] = green;
    sat->rgba[CB] = blue;
    sat->rgba[CA] = alpha;
}

#undef FR_SetColorAndAlphav
void FR_SetColorAndAlphav(const float rgba[4])
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetColorAndAlphav");
    sat->rgba[CR] = rgba[CR];
    sat->rgba[CG] = rgba[CG];
    sat->rgba[CB] = rgba[CB];
    sat->rgba[CA] = rgba[CA];
}

#undef FR_ColorRed
float FR_ColorRed(void)
{
    errorIfNotInited("FR_ColorRed");
    return currentAttribs()->rgba[CR];
}

#undef FR_SetColorRed
void FR_SetColorRed(float value)
{
    errorIfNotInited("FR_SetColorRed");
    currentAttribs()->rgba[CR] = value;
}

#undef FR_ColorGreen
float FR_ColorGreen(void)
{
    errorIfNotInited("FR_ColorGreen");
    return currentAttribs()->rgba[CG];
}

#undef FR_SetColorGreen
void FR_SetColorGreen(float value)
{
    errorIfNotInited("FR_SetColorGreen");
    currentAttribs()->rgba[CG] = value;
}

#undef FR_ColorBlue
float FR_ColorBlue(void)
{
    errorIfNotInited("FR_ColorBlue");
    return currentAttribs()->rgba[CB];
}

#undef FR_SetColorBlue
void FR_SetColorBlue(float value)
{
    errorIfNotInited("FR_SetColorBlue");
    currentAttribs()->rgba[CB] = value;
}

#undef FR_Alpha
float FR_Alpha(void)
{
    errorIfNotInited("FR_Alpha");
    return currentAttribs()->rgba[CA];
}

#undef FR_SetAlpha
void FR_SetAlpha(float value)
{
    errorIfNotInited("FR_SetAlpha");
    currentAttribs()->rgba[CA] = value;
}

#undef FR_ShadowOffset
void FR_ShadowOffset(int* offsetX, int* offsetY)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_ShadowOffset");
    if (NULL != offsetX) *offsetX = sat->shadowOffsetX;
    if (NULL != offsetY) *offsetY = sat->shadowOffsetY;
}

#undef FR_SetShadowOffset
void FR_SetShadowOffset(int offsetX, int offsetY)
{
    fr_state_attributes_t* sat = currentAttribs();
    errorIfNotInited("FR_SetShadowOffset");
    sat->shadowOffsetX = offsetX;
    sat->shadowOffsetY = offsetY;
}

#undef FR_ShadowStrength
float FR_ShadowStrength(void)
{
    errorIfNotInited("FR_ShadowStrength");
    return currentAttribs()->shadowStrength;
}

#undef FR_SetShadowStrength
void FR_SetShadowStrength(float value)
{
    errorIfNotInited("FR_SetShadowStrength");
    currentAttribs()->shadowStrength = MINMAX_OF(0, value, 1);
}

#undef FR_GlitterStrength
float FR_GlitterStrength(void)
{
    errorIfNotInited("FR_GlitterStrength");
    return currentAttribs()->glitterStrength;
}

#undef FR_SetGlitterStrength
void FR_SetGlitterStrength(float value)
{
    errorIfNotInited("FR_SetGlitterStrength");
    currentAttribs()->glitterStrength = MINMAX_OF(0, value, 1);
}

#undef FR_CaseScale
dd_bool FR_CaseScale(void)
{
    errorIfNotInited("FR_CaseScale");
    return currentAttribs()->caseScale;
}

#undef FR_SetCaseScale
void FR_SetCaseScale(dd_bool value)
{
    errorIfNotInited("FR_SetCaseScale");
    currentAttribs()->caseScale = value;
}

#undef FR_CharSize
void FR_CharSize(Size2Raw *size, dbyte ch)
{
    errorIfNotInited("FR_CharSize");
    if (size)
    {
        Vec2ui dimensions = App_Resources().font(fr.fontNum).glyphPosCoords(ch).size();
        size->width  = dimensions.x;
        size->height = dimensions.y;
    }
}

#undef FR_CharWidth
int FR_CharWidth(dbyte ch)
{
    errorIfNotInited("FR_CharWidth");
    if (fr.fontNum != 0)
        return App_Resources().font(fr.fontNum).glyphPosCoords(ch).width();
    return 0;
}

#undef FR_CharHeight
int FR_CharHeight(dbyte ch)
{
    errorIfNotInited("FR_CharHeight");
    if (fr.fontNum != 0)
        return App_Resources().font(fr.fontNum).glyphPosCoords(ch).height();
    return 0;
}

int FR_SingleLineHeight(const char *text)
{
    errorIfNotInited("FR_SingleLineHeight");
    if (fr.fontNum == 0 || !text)
        return 0;
    AbstractFont &font = App_Resources().font(fr.fontNum);
    int ascent = font.ascent();
    if (ascent != 0)
        return ascent;
    return font.glyphPosCoords((dbyte)text[0]).height();
}

int FR_GlyphTopToAscent(const char *text)
{
    errorIfNotInited("FR_GlyphTopToAscent");
    if (fr.fontNum == 0 || !text)
        return 0;
    AbstractFont &font = App_Resources().font(fr.fontNum);
    int lineHeight = font.lineSpacing();
    if (lineHeight == 0)
        return 0;
    return lineHeight - font.ascent();
}

enum class TextFragmentPass
{
    Shadow,
    Character,
    Glitter,
};

struct TextFragment
{
    TextFragmentPass pass;
    const AbstractFont &font;
    const char *text;
    int x;
    int y;
    int alignFlags;
    uint16_t textFlags;
    int initialCount;

    int length;
    int width;
    int height;

    void updateDimensions()
    {
        length = int(strlen(text));

        // Width.
        {
            width = 0;

            // Just add them together.
            int i = 0;
            const char *ch = text;
            dbyte c;
            while (i++ < length && (c = *ch++) != 0 && c != '\n')
            {
                width += FR_CharWidth(c);
            }
        }

        // Height.
        {
            height = 0;

            // Find the greatest height.
            int i = 0;
            const char *ch = text;
            dbyte c;
            while (i++ < length && (c = *ch++) != 0 && c != '\n')
            {
                height = de::max(height, FR_CharHeight(c));
            }

            height += topToAscent(font);
        }
    }

    void applyBlendMode() const
    {
        switch (pass)
        {
        case TextFragmentPass::Shadow:
            DGL_BlendFunc(DGL_ZERO, DGL_ONE_MINUS_SRC_ALPHA);
            break;

        case TextFragmentPass::Character:
            DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
            break;

        case TextFragmentPass::Glitter:
            DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE);
            break;
        }
    }
};

static void drawTextFragment(const TextFragment &fragment)
{
    DE_ASSERT(fragment.text != 0 && fragment.text[0]);

    fr_state_attributes_t* sat = currentAttribs();
    bool noTypein = (fragment.textFlags & DTF_NO_TYPEIN) != 0;

    float glitterMul;
    float shadowMul;
    float flashColor[3] = { 0, 0, 0 };
    int x = fragment.x;
    int y = fragment.y;
    int w, h, cx, cy, count, yoff;
    unsigned char c;
    const char* ch;

    // We may be able to skip a fragment completely.
    {
        if (fragment.pass == TextFragmentPass::Shadow &&
            (sat->shadowStrength <= 0 || fragment.font.flags().testFlag(AbstractFont::Shadowed)))
        {
            return;
        }

        if (fragment.pass == TextFragmentPass::Glitter && sat->glitterStrength <= 0)
        {
            return;
        }
    }

    if (fragment.pass == TextFragmentPass::Glitter)
    {
        flashColor[CR] = (1 + 2 * sat->rgba[CR]) / 3;
        flashColor[CG] = (1 + 2 * sat->rgba[CG]) / 3;
        flashColor[CB] = (1 + 2 * sat->rgba[CB]) / 3;
    }

    if (fragment.alignFlags & ALIGN_RIGHT)
    {
        x -= fragment.width;
    }
    else if (!(fragment.alignFlags & ALIGN_LEFT))
    {
        x -= fragment.width / 2;
    }

    if (fragment.alignFlags & ALIGN_BOTTOM)
    {
        y -= fragment.height;
    }
    else if (!(fragment.alignFlags & ALIGN_TOP))
    {
        y -= fragment.height / 2;
    }

    if (const BitmapFont *bmapFont = maybeAs<BitmapFont>(fragment.font))
    {
        if (bmapFont->textureGLName())
        {
            GL_BindTextureUnmanaged(bmapFont->textureGLName(),
                                    gfx::ClampToEdge,
                                    gfx::ClampToEdge,
                                    filterUI ? gfx::Linear : gfx::Nearest);

            DGL_MatrixMode(DGL_TEXTURE);
            DGL_PushMatrix();
            DGL_LoadIdentity();
            DGL_Scalef(1.f / bmapFont->textureDimensions().x,
                       1.f / bmapFont->textureDimensions().y, 1.f);
        }
    }

    bool isBlendModeSet = false;

    //for (int pass = (noShadow? 1 : 0); pass < (noCharacter && noGlitter? 1 : 2); ++pass)
    {
        count = fragment.initialCount;
        ch = fragment.text;
        cx = x + (fragment.pass == TextFragmentPass::Shadow? sat->shadowOffsetX : 0);
        cy = y + (fragment.pass == TextFragmentPass::Shadow? sat->shadowOffsetY : 0);

        for (;;)
        {
            c = *ch++;
            yoff = 0;

            glitterMul = 0;
            shadowMul = sat->rgba[CA];

            // Do the type-in effect?
            if (!noTypein) // && (pass || (!noShadow && !pass)))
            {
                int maxCount = (typeInTime > 0? typeInTime * 2 : 0);

                if (fragment.pass == TextFragmentPass::Shadow)
                {
                    if (count == maxCount)
                    {
                        shadowMul = 0;
                    }
                    else if (count + 1 == maxCount)
                    {
                        shadowMul *= .25f;
                    }
                    else if (count + 2 == maxCount)
                    {
                        shadowMul *= .5f;
                    }
                    else if (count + 3 == maxCount)
                    {
                        shadowMul *= .75f;
                    }
                    else if (count > maxCount)
                    {
                        break;
                    }
                }
                else
                {
                    //if (!noGlitter)
                    if (fragment.pass == TextFragmentPass::Glitter)
                    {
                        if (count == maxCount)
                        {
                            glitterMul = 1;
                            flashColor[CR] = sat->rgba[CR];
                            flashColor[CG] = sat->rgba[CG];
                            flashColor[CB] = sat->rgba[CB];
                        }
                        else if (count + 1 == maxCount)
                        {
                            glitterMul = 0.88f;
                            flashColor[CR] = (1 + sat->rgba[CR]) / 2;
                            flashColor[CG] = (1 + sat->rgba[CG]) / 2;
                            flashColor[CB] = (1 + sat->rgba[CB]) / 2;
                        }
                        else if (count + 2 == maxCount)
                        {
                            glitterMul = 0.75f;
                            flashColor[CR] = sat->rgba[CR];
                            flashColor[CG] = sat->rgba[CG];
                            flashColor[CB] = sat->rgba[CB];
                        }
                        else if (count + 3 == maxCount)
                        {
                            glitterMul = 0.5f;
                            flashColor[CR] = sat->rgba[CR];
                            flashColor[CG] = sat->rgba[CG];
                            flashColor[CB] = sat->rgba[CB];
                        }
                        else if (count > maxCount)
                        {
                            break;
                        }
                    }
                    else if (count > maxCount)
                    {
                        break;
                    }
                }
            }
            count++;

            if (!c || c == '\n')
                break;

            w = FR_CharWidth(c);
            h = FR_CharHeight(c);

            if (c != ' ')
            {
                // A non-white-space character we have a glyph for.
                switch (fragment.pass)
                {
                    case TextFragmentPass::Character:
                        // The character itself.
                        if (sat->rgba[CA] > 0.001f)
                        {
                            DGL_Color4fv(sat->rgba);
                            if (!isBlendModeSet)
                            {
                                fragment.applyBlendMode();
                                isBlendModeSet = true;
                            }
                            drawChar(c, cx, cy + yoff, fragment.font, ALIGN_TOPLEFT);
                        }
                        break;

                    case TextFragmentPass::Glitter:
                        if (/*!noGlitter &&*/ sat->glitterStrength > 0)
                        {
                            // Do something flashy.
                            Point2Raw origin;
                            Size2Raw  size;
                            origin.x    = cx;
                            origin.y    = cy + yoff;
                            size.width  = w;
                            size.height = h;
                            const float alpha = sat->glitterStrength * glitterMul;
                            if (alpha > 0)
                            {
                                if (!isBlendModeSet)
                                {
                                    fragment.applyBlendMode();
                                    isBlendModeSet = true;
                                }
                                DGL_Color4f(flashColor[CR], flashColor[CG], flashColor[CB], alpha);
                                drawFlash(&origin, &size, true);
                            }
                        }
                        break;

                    case TextFragmentPass::Shadow:
                    {
                        Point2Raw origin;
                        Size2Raw  size;
                        origin.x    = cx;
                        origin.y    = cy + yoff;
                        size.width  = w;
                        size.height = h;
                        const float alpha = sat->shadowStrength * shadowMul;
                        if (alpha > 0)
                        {
                            if (!isBlendModeSet)
                            {
                                fragment.applyBlendMode();
                                isBlendModeSet = true;
                            }
                            DGL_Color4f(1, 1, 1, alpha);
                            drawFlash(&origin, &size, false);
                        }
                        break;
                    }
                }
            }

            cx += w + sat->tracking;
        }
    }

    // Restore previous GL-state.
    if (const BitmapFont *bmapFont = maybeAs<BitmapFont>(fragment.font))
    {
        if (bmapFont->textureGLName())
        {
            DGL_MatrixMode(DGL_TEXTURE);
            DGL_PopMatrix();
        }
    }
}

static void drawChar(dbyte ch, float x, float y, const AbstractFont &font, int alignFlags)
{
    if (alignFlags & ALIGN_RIGHT)
    {
        x -= font.glyphPosCoords(ch).width();
    }
    else if (!(alignFlags & ALIGN_LEFT))
    {
        x -= font.glyphPosCoords(ch).width() / 2;
    }

    const int ascent = font.ascent();
    const int lineHeight = ascent ? ascent : font.glyphPosCoords(ch).height();
    if (alignFlags & ALIGN_BOTTOM)
    {
        y -= topToAscent(font) + lineHeight;
    }
    else if (!(alignFlags & ALIGN_TOP))
    {
        y -= (topToAscent(font) + lineHeight) / 2;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(x, y, 0);

    Rectanglei geometry = font.glyphPosCoords(ch);

    if (const BitmapFont *bmapFont = maybeAs<BitmapFont>(font))
    {
        /// @todo Filtering should be determined at a higher level.
        /// @todo We should not need to re-bind this texture here.
        GL_BindTextureUnmanaged(bmapFont->textureGLName(), gfx::ClampToEdge,
                                gfx::ClampToEdge, filterUI? gfx::Linear : gfx::Nearest);

        geometry = geometry.expanded(bmapFont->textureMargin().toVec2i());
    }
    else if (const CompositeBitmapFont *compFont = maybeAs<CompositeBitmapFont>(font))
    {
        GL_BindTexture(compFont->glyphTexture(ch));
        geometry = geometry.expanded(compFont->glyphTextureBorder(ch));
    }

    Vec2i coords[4] = { font.glyphTexCoords(ch).topLeft,
                        font.glyphTexCoords(ch).topRight(),
                        font.glyphTexCoords(ch).bottomRight,
                        font.glyphTexCoords(ch).bottomLeft() };

    GL_DrawRectWithCoords(geometry, coords);

    if (is<CompositeBitmapFont>(font))
    {
        GL_SetNoTexture();
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-x, -y, 0);
}

static void drawFlash(const Point2Raw *origin, const Size2Raw *size, bool bright)
{
    float fsize = 4.f + bright;
    float fw = fsize * size->width  / 2.0f;
    float fh = fsize * size->height / 2.0f;
    int x, y, w, h;

    // Don't draw anything for very small letters.
    if (size->height <= 4) return;

    x = origin->x + (int) (size->width  / 2.0f - fw / 2);
    y = origin->y + (int) (size->height / 2.0f - fh / 2);
    w = (int) fw;
    h = (int) fh;

    GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC),
                            gfx::ClampToEdge, gfx::ClampToEdge);

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);
    DGL_End();
}

/**
 * Expected:
 * <pre>
 *     "<whitespace> = <whitespace> <float>"
 * </pre>
 */
static float parseFloat(const char **str)
{
    float value;
    char *end;

    *str = M_SkipWhite(*str);
    if (**str != '=') return 0; // Now I'm confused!

    *str = M_SkipWhite(*str + 1);
    value = (float) strtod(*str, &end);
    *str = end;
    return value;
}

/**
 * Expected:
 * <pre>
 *      "<whitespace> = <whitespace> [|"]<string>[|"]"
 * </pre>
 */
 static dd_bool parseString(const char **str, char* buf, size_t bufLen)
{
    size_t len;
    const char* end;

    if (!buf || bufLen == 0) return false;

    *str = M_SkipWhite(*str);
    if (**str != '=') return false; // Now I'm confused!

    // Skip over any leading whitespace.
    *str = M_SkipWhite(*str + 1);

    // Skip over any opening '"' character.
    if (**str == '"') (*str)++;

    // Find the end of the string.
    end = *str;
    while (*end && *end != '}' && *end != ',' && *end !='"') { end++; }

    len = end - *str;
    if (len != 0)
    {
        dd_snprintf(buf, MIN_OF(len+1, bufLen), "%s", *str);
        *str = end;
    }

    // Skip over any closing '"' character.
    if (**str == '"')
        (*str)++;

    return true;
}

static void parseParamaterBlock(const char **strPtr, drawtextstate_t* state, int *numBreaks)
{
    LOG_AS("parseParamaterBlock");

    (*strPtr)++;
    while (*(*strPtr) && *(*strPtr) != '}')
    {
        (*strPtr) = M_SkipWhite(*strPtr);

        // What do we have here?
        if (!strnicmp((*strPtr), "flash", 5))
        {
            (*strPtr) += 5;
            state->typeIn = true;
        }
        else if (!strnicmp((*strPtr), "noflash", 7))
        {
            (*strPtr) += 7;
            state->typeIn = false;
        }
        else if (!strnicmp((*strPtr), "case", 4))
        {
            (*strPtr) += 4;
            state->caseScale = true;
        }
        else if (!strnicmp((*strPtr), "nocase", 6))
        {
            (*strPtr) += 6;
            state->caseScale = false;
        }
        else if (!strnicmp((*strPtr), "ups", 3))
        {
            (*strPtr) += 3;
            state->caseMod[1].scale = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "upo", 3))
        {
            (*strPtr) += 3;
            state->caseMod[1].offset = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "los", 3))
        {
            (*strPtr) += 3;
            state->caseMod[0].scale = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "loo", 3))
        {
            (*strPtr) += 3;
            state->caseMod[0].offset = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "break", 5))
        {
            (*strPtr) += 5;
            ++(*numBreaks);
        }
        else if (!strnicmp((*strPtr), "r", 1))
        {
            (*strPtr)++;
            state->rgba[CR] = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "g", 1))
        {
            (*strPtr)++;
            state->rgba[CG] = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "b", 1))
        {
            (*strPtr)++;
            state->rgba[CB] = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "a", 1))
        {
            (*strPtr)++;
            state->rgba[CA] = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "x", 1))
        {
            (*strPtr)++;
            state->offX = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "y", 1))
        {
            (*strPtr)++;
            state->offY = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "scalex", 6))
        {
            (*strPtr) += 6;
            state->scaleX = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "scaley", 6))
        {
            (*strPtr) += 6;
            state->scaleY = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "scale", 5))
        {
            (*strPtr) += 5;
            state->scaleX = state->scaleY = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "angle", 5))
        {
            (*strPtr) += 5;
            state->angle = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "glitter", 7))
        {
            (*strPtr) += 7;
            state->glitterStrength = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "shadow", 6))
        {
            (*strPtr) += 6;
            state->shadowStrength = parseFloat(&(*strPtr));
        }
        else if (!strnicmp((*strPtr), "tracking", 8))
        {
            (*strPtr) += 8;
            state->tracking = parseFloat(&(*strPtr));
        }
        else
        {
            // Perhaps a font name?
            if (!strnicmp((*strPtr), "font", 4))
            {
                char buf[80];

                (*strPtr) += 4;
                if (parseString(&(*strPtr), buf, 80))
                {
                    try
                    {
                        state->fontNum = App_Resources().fontManifest(res::makeUri(buf)).uniqueId();
                        continue;
                    }
                    catch (const Resources::MissingResourceManifestError &)
                    {}
                }

                LOG_GL_WARNING("Unknown font '%s'") << *strPtr;
                continue;
            }

            // Unknown, skip it.
            if (*(*strPtr) != '}')
            {
                (*strPtr)++;
            }
        }
    }

    // Skip over the closing brace.
    if (*(*strPtr))
        (*strPtr)++;
}

static void initDrawTextState(drawtextstate_t* state, uint16_t textFlags)
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
    if (lengthMinusTerminator <= FR_SMALL_TEXT_BUFFER_SIZE)
    {
        return smallTextBuffer;
    }
    if (largeTextBuffer == NULL || lengthMinusTerminator > largeTextBufferSize)
    {
        largeTextBufferSize = lengthMinusTerminator;
        largeTextBuffer = (char*)realloc(largeTextBuffer, largeTextBufferSize+1);
        if (largeTextBuffer == NULL)
            App_Error("FR_EnlargeTextBuffer: Failed on reallocation of %lu bytes.",
                (unsigned long)(lengthMinusTerminator+1));
    }
    return largeTextBuffer;
}

static void freeTextBuffer(void)
{
    if (largeTextBuffer == NULL)
        return;
    free(largeTextBuffer); largeTextBuffer = 0;
    largeTextBufferSize = 0;
}

#undef FR_TextWidth
int FR_TextWidth(const char* string)
{
    int w, maxWidth = -1;
    dd_bool skipping = false, escaped = false;
    const char* ch;
    size_t i, len;

    errorIfNotInited("FR_TextWidth");

    if (!string || !string[0])
        return 0;

    /// @todo All visual format parsing should be done in one place.

    w = 0;
    len = strlen(string);
    ch = string;
    for (i = 0; i < len; ++i, ch++)
    {
        unsigned char c = *ch;

        if (c == FR_FORMAT_ESCAPE_CHAR)
        {
            escaped = true;
            continue;
        }
        if (!escaped && c == '{')
        {
            skipping = true;
        }
        else if (skipping && c == '}')
        {
            skipping = false;
            continue;
        }

        if (skipping)
            continue;

        escaped = false;

        if (c == '\n')
        {
            if (w > maxWidth)
                maxWidth = w;
            w = 0;
            continue;
        }

        w += FR_CharWidth(c);

        if (i != len - 1)
        {
            w += FR_Tracking();
        }
        else if (maxWidth == -1)
        {
            maxWidth = w;
        }
    }

    return maxWidth;
}

#undef FR_TextHeight
int FR_TextHeight(const char* string)
{
    int h, currentLineHeight;
    dd_bool skip = false;
    const char* ch;
    size_t i, len;

    if (!string || !string[0])
        return 0;

    errorIfNotInited("FR_TextHeight");

    currentLineHeight = 0;
    len = strlen(string);
    h = 0;
    ch = string;
    for (i = 0; i < len; ++i, ch++)
    {
        unsigned char c = *ch;
        int charHeight;

        if (c == '{')
        {
            skip = true;
        }
        else if (c == '}')
        {
            skip = false;
            continue;
        }

        if (skip)
            continue;

        if (c == '\n')
        {
            h += currentLineHeight == 0? (FR_CharHeight('A') * (1+FR_Leading())) : currentLineHeight;
            currentLineHeight = 0;
            continue;
        }

        charHeight = FR_CharHeight(c) * (1+FR_Leading());
        if (charHeight > currentLineHeight)
            currentLineHeight = charHeight;
    }
    h += currentLineHeight;

    return h;
}

#undef FR_TextSize
void FR_TextSize(Size2Raw* size, const char* text)
{
    if (!size) return;
    size->width  = FR_TextWidth(text);
    size->height = FR_TextHeight(text);
}

#undef FR_DrawText3
void FR_DrawText3(const char* text, const Point2Raw* _origin, int alignFlags, uint16_t textFlags)
{
    fontid_t origFont = FR_Font();
    float cx, cy, extraScale;
    drawtextstate_t state;
    const char* fragment;
    int curCase;
    Point2Raw origin;
    Size2Raw textSize;
    int charCount;
    float origColor[4];
    const char *str, *end;
    dd_bool escaped = false;

    errorIfNotInited("FR_DrawText");

    if (!text || !text[0]) return;

    origin.x = _origin? _origin->x : 0;
    origin.y = _origin? _origin->y : 0;

    textFlags &= ~(DTF_INTERNAL_MASK);

    // If we aren't aligning to top-left we need to know the dimensions.
    if (alignFlags & ALIGN_RIGHT)
    {
        FR_TextSize(&textSize, text);
    }

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    // We need to change the current color, so remember for restore.
    DGL_CurrentColor(origColor);

    for (TextFragmentPass pass :
         {TextFragmentPass::Shadow, TextFragmentPass::Character, TextFragmentPass::Glitter})
    {
        switch (pass)
        {
            case TextFragmentPass::Shadow:
                if (textFlags & DTF_NO_SHADOW) continue;
                break;

            case TextFragmentPass::Character:
                if (textFlags & DTF_NO_CHARACTER) continue;
                break;

            case TextFragmentPass::Glitter:
                if (textFlags & DTF_NO_GLITTER) continue;
                break;
        }

        // Configure the next pass.
        cx = (float) origin.x;
        cy = (float) origin.y;
        curCase = -1;
        charCount = 0;

        // Apply defaults.
        initDrawTextState(&state, textFlags);

        str = text;
        while (*str)
        {
            if (*str == FR_FORMAT_ESCAPE_CHAR)
            {
                escaped = true;
                ++str;
                continue;
            }
            if (!escaped && *str == '{') // Paramaters included?
            {
                fontid_t lastFont = state.fontNum;
                int lastTracking = state.tracking;
                float lastLeading = state.leading;
                float lastShadowStrength = state.shadowStrength;
                float lastGlitterStrength = state.glitterStrength;
                dd_bool lastCaseScale = state.caseScale;
                float lastRGBA[4];
                int numBreaks = 0;

                lastRGBA[CR] = state.rgba[CR];
                lastRGBA[CG] = state.rgba[CG];
                lastRGBA[CB] = state.rgba[CB];
                lastRGBA[CA] = state.rgba[CA];

                parseParamaterBlock(&str, &state, &numBreaks);

                if (numBreaks != 0)
                {
                    do
                    {
                        cx = (float) origin.x;
                        cy += state.lastLineHeight * (1+lastLeading);
                    } while (--numBreaks > 0);
                }

                if (state.fontNum != lastFont)
                    FR_SetFont(state.fontNum);
                if (state.tracking != lastTracking)
                    FR_SetTracking(state.tracking);
                if (state.leading != lastLeading)
                    FR_SetLeading(state.leading);
                if (state.rgba[CR] != lastRGBA[CR] || state.rgba[CG] != lastRGBA[CG] || state.rgba[CB] != lastRGBA[CB] || state.rgba[CA] != lastRGBA[CA])
                    FR_SetColorAndAlphav(state.rgba);
                if (state.shadowStrength != lastShadowStrength)
                    FR_SetShadowStrength(state.shadowStrength);
                if (state.glitterStrength != lastGlitterStrength)
                    FR_SetGlitterStrength(state.glitterStrength);
                if (state.caseScale != lastCaseScale)
                    FR_SetCaseScale(state.caseScale);
            }

            for (end = str; *end && *end != FR_FORMAT_ESCAPE_CHAR && (escaped || *end != '{');)
            {
                int newlines = 0, fragmentAlignFlags;
                float alignx = 0;

                // Find the end of the next fragment.
                if (FR_CaseScale())
                {
                    curCase = -1;
                    // Select a substring with characters of the same case (or whitespace).
                    for (; *end && *end != FR_FORMAT_ESCAPE_CHAR && (escaped || *end != '{') &&
                        *end != '\n'; end++)
                    {
                        escaped = false;

                        // We can skip whitespace.
                        if (isspace(*end))
                            continue;

                        if (curCase < 0)
                            curCase = (isupper(*end) != 0);
                        else if (curCase != (isupper(*end) != 0))
                            break;
                    }
                }
                else
                {
                    curCase = 0;
                    for (; *end && *end != FR_FORMAT_ESCAPE_CHAR && (escaped || *end != '{') &&
                        *end != '\n'; end++) { escaped = false; }
                }

                // No longer escaped.
                escaped = false;

                { char* buffer = enlargeTextBuffer(end - str);
                memcpy(buffer, str, end - str);
                buffer[end - str] = '\0';
                fragment = buffer;
                }

                while (*end == '\n')
                {
                    newlines++;
                    end++;
                }

                // Continue from here.
                str = end;

                if (!(alignFlags & (ALIGN_LEFT|ALIGN_RIGHT)))
                {
                    fragmentAlignFlags = alignFlags;
                }
                else
                {
                    // We'll take care of horizontal positioning of the fragment so align left.
                    fragmentAlignFlags = (alignFlags & ~(ALIGN_RIGHT)) | ALIGN_LEFT;
                    if (alignFlags & ALIGN_RIGHT)
                        alignx = -textSize.width * state.scaleX;
                }

                // Setup the scaling.
                DGL_MatrixMode(DGL_MODELVIEW);
                DGL_PushMatrix();

                // Rotate.
                if (!fequal(state.angle, 0))
                {
                    // The origin is the specified (x,y) for the patch.
                    // We'll undo the aspect ratio (otherwise the result would be skewed).
                    /// @todo Do not assume the aspect ratio and therefore whether
                    // correction is even needed.
                    DGL_Translatef((float)origin.x, (float)origin.y, 0);
                    DGL_Scalef(1, 200.0f / 240.0f, 1);
                    DGL_Rotatef(state.angle, 0, 0, 1);
                    DGL_Scalef(1, 240.0f / 200.0f, 1);
                    DGL_Translatef(-(float)origin.x, -(float)origin.y, 0);
                }

                DGL_Translatef(cx + state.offX + alignx, cy + state.offY + (FR_CaseScale() ? state.caseMod[curCase].offset : 0), 0);
                extraScale = (FR_CaseScale() ? state.caseMod[curCase].scale : 1);
                DGL_Scalef(state.scaleX, state.scaleY * extraScale, 1);

                // Draw it.
                DE_ASSERT(fr.fontNum);

                TextFragment frag{pass,
                                  App_Resources().font(fr.fontNum),
                                  fragment,
                                  0,
                                  0,
                                  fragmentAlignFlags,
                                  textFlags,
                                  state.typeIn ? charCount : DEFAULT_INITIALCOUNT,
                                  0,
                                  0,
                                  0};
                frag.updateDimensions();

                drawTextFragment(frag);

                charCount += frag.length;

                // Advance the current position?
                if (newlines == 0)
                {
                    cx += (float(frag.width) + currentAttribs()->tracking) * state.scaleX;
                }
                else
                {
                    if (frag.length > 0)
                    {
                        state.lastLineHeight = frag.height;
                    }

                    cx = (float) origin.x;
                    cy += newlines * state.lastLineHeight * (1.f + FR_Leading());
                }

                DGL_MatrixMode(DGL_MODELVIEW);
                DGL_PopMatrix();
            }
        }

        FR_PopAttrib();
    }

    freeTextBuffer();

    FR_SetFont(origFont);
    DGL_Color4fv(origColor);
    DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
}

#undef FR_DrawText2
void FR_DrawText2(const char* text, const Point2Raw* origin, int alignFlags)
{
    FR_DrawText3(text, origin, alignFlags, DEFAULT_DRAWFLAGS);
}

#undef FR_DrawText
void FR_DrawText(const char* text, const Point2Raw* origin)
{
    FR_DrawText2(text, origin, DEFAULT_ALIGNFLAGS);
}

#undef FR_DrawTextXY3
void FR_DrawTextXY3(const char* text, int x, int y, int alignFlags, short flags)
{
    Point2Raw origin;
    origin.x = x;
    origin.y = y;
    FR_DrawText3(text, &origin, alignFlags, flags);
}

#undef FR_DrawTextXY2
void FR_DrawTextXY2(const char* text, int x, int y, int alignFlags)
{
    FR_DrawTextXY3(text, x, y, alignFlags, DEFAULT_DRAWFLAGS);
}

#undef FR_DrawTextXY
void FR_DrawTextXY(const char* text, int x, int y)
{
    FR_DrawTextXY2(text, x, y, DEFAULT_ALIGNFLAGS);
}

#undef FR_DrawChar3
void FR_DrawChar3(unsigned char ch, const Point2Raw* origin, int alignFlags, short textFlags)
{
    char str[2];
    str[0] = ch;
    str[1] = '\0';
    FR_DrawText3(str, origin, alignFlags, textFlags);
}

#undef FR_DrawChar2
void FR_DrawChar2(unsigned char ch, const Point2Raw* origin, int alignFlags)
{
    FR_DrawChar3(ch, origin, alignFlags, DEFAULT_DRAWFLAGS);
}

#undef FR_DrawChar
void FR_DrawChar(unsigned char ch, const Point2Raw* origin)
{
    FR_DrawChar2(ch, origin, DEFAULT_ALIGNFLAGS);
}

#undef FR_DrawCharXY3
void FR_DrawCharXY3(unsigned char ch, int x, int y, int alignFlags, short textFlags)
{
    Point2Raw origin;
    origin.x = x;
    origin.y = y;
    FR_DrawChar3(ch, &origin, alignFlags, textFlags);
}

#undef FR_DrawCharXY2
void FR_DrawCharXY2(unsigned char ch, int x, int y, int alignFlags)
{
    FR_DrawCharXY3(ch, x, y, alignFlags, DEFAULT_DRAWFLAGS);
}

#undef FR_DrawCharXY
void FR_DrawCharXY(unsigned char ch, int x, int y)
{
    FR_DrawCharXY2(ch, x, y, DEFAULT_ALIGNFLAGS);
}

void FR_Init(void)
{
    // No reinitializations...
    if (initedFont || novideo) return;

    initedFont = true;
    fr.fontNum = 0;
    FR_LoadDefaultAttrib();
    typeInTime = 0;
}

#undef Fonts_ResolveUri
DE_EXTERN_C fontid_t Fonts_ResolveUri(const uri_s *uri)
{
    if (!uri) return NOFONTID;
    try
    {
        return App_Resources().fontManifest(*reinterpret_cast<const res::Uri *>(uri)).uniqueId();
    }
    catch (const Resources::MissingResourceManifestError &)
    {}
    return NOFONTID;
}

DE_DECLARE_API(FR) =
{
    { DE_API_FONT_RENDER },
    Fonts_ResolveUri,
    FR_Font,
    FR_SetFont,
    FR_PushAttrib,
    FR_PopAttrib,
    FR_LoadDefaultAttrib,
    FR_Leading,
    FR_SetLeading,
    FR_Tracking,
    FR_SetTracking,
    FR_ColorAndAlpha,
    FR_SetColor,
    FR_SetColorv,
    FR_SetColorAndAlpha,
    FR_SetColorAndAlphav,
    FR_ColorRed,
    FR_SetColorRed,
    FR_ColorGreen,
    FR_SetColorGreen,
    FR_ColorBlue,
    FR_SetColorBlue,
    FR_Alpha,
    FR_SetAlpha,
    FR_ShadowOffset,
    FR_SetShadowOffset,
    FR_ShadowStrength,
    FR_SetShadowStrength,
    FR_GlitterStrength,
    FR_SetGlitterStrength,
    FR_CaseScale,
    FR_SetCaseScale,
    FR_DrawText,
    FR_DrawText2,
    FR_DrawText3,
    FR_DrawTextXY3,
    FR_DrawTextXY2,
    FR_DrawTextXY,
    FR_TextSize,
    FR_TextWidth,
    FR_TextHeight,
    FR_DrawChar3,
    FR_DrawChar2,
    FR_DrawChar,
    FR_DrawCharXY3,
    FR_DrawCharXY2,
    FR_DrawCharXY,
    FR_CharSize,
    FR_CharWidth,
    FR_CharHeight,
    FR_ResetTypeinTimer
};
