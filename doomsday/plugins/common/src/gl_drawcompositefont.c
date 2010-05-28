/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <string.h>
#include <ctype.h>

// \todo Only included for the config. Refactor away.
#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "gl_drawpatch.h"
#include "gl_drawcompositefont.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_FONTID              (GF_FONTA)
#define DEFAULT_GLITTER             (0)
#define DEFAULT_LEADING             (.25)
#define DEFAULT_TRACKING            (0)
#define DEFAULT_DRAWFLAGS           (DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS)
#define DEFAULT_INITIALCOUNT        (0)

// TYPES -------------------------------------------------------------------

typedef struct {
    struct compositefont_char_s {
        char            lumpname[9];
        patchinfo_t     pInfo;
    } chars[256];
} compositefont_t;

typedef struct {
    gamefontid_t font;
    float scaleX, scaleY;
    float offX, offY;
    float angle;
    float color[4];
    float glitter;
    int tracking;
    float leading;
    boolean typeIn;
    boolean caseScale;
    struct {
        float scale, offset;
    } caseMod[2]; // 1=upper, 0=lower

    int numBreaks;
} drawtextstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void drawFlash(int x, int y, int w, int h, int bright);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static compositefont_t fonts[NUM_GAME_FONTS];

static int typeInTime = 0;

// CODE --------------------------------------------------------------------

static __inline boolean isValidFontId(fontId)
{
    return (fontId >= GF_FIRST && fontId < NUM_GAME_FONTS);
}

static __inline compositefont_t* fontForId(gamefontid_t fontId)
{
    assert(isValidFontId(fontId));
    return &fonts[fontId];
}

static __inline patchid_t patchForFontChar(gamefontid_t id, unsigned char ch)
{
    return fontForId(id)->chars[ch].pInfo.id;
}

static __inline short translateTextToPatchDrawFlags(short in)
{
    short out = 0;
    if(in & DTF_ALIGN_LEFT)
        out |= DPF_ALIGN_LEFT;
    if(in & DTF_ALIGN_RIGHT)
        out |= DPF_ALIGN_RIGHT;
    if(in & DTF_ALIGN_BOTTOM)
        out |= DPF_ALIGN_BOTTOM;
    if(in & DTF_ALIGN_TOP)
        out |= DPF_ALIGN_TOP;
    return out;
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

static void parseParamaterBlock(char** strPtr, drawtextstate_t* state)
{
    (*strPtr)++;
    while(*(*strPtr) && *(*strPtr) != '}')
    {
        (*strPtr) = M_SkipWhite((*strPtr));

        // What do we have here?
        if(!strnicmp((*strPtr), "fonta", 5))
        {
            state->font = GF_FONTA;
            (*strPtr) += 5;
        }
        else if(!strnicmp((*strPtr), "fontb", 5))
        {
            state->font = GF_FONTB;
            (*strPtr) += 5;
        }
        else if(!strnicmp((*strPtr), "flash", 5))
        {
            (*strPtr) += 5;
            state->typeIn = true;
        }
        else if(!strnicmp((*strPtr), "noflash", 7))
        {
            (*strPtr) += 7;
            state->typeIn = false;
        }
        else if(!strnicmp((*strPtr), "glitter", 7))
        {
            (*strPtr) += 7;
            state->glitter = parseFloat(&(*strPtr));
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
            state->numBreaks++;
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
        else
        {
            // Unknown, skip it.
            if(*(*strPtr) != '}')
                (*strPtr)++;
        }
    }

    // Skip over the closing brace.
    if(*(*strPtr))
        (*strPtr)++;
}

static void setFontCharacterPatch(compositefont_t* font, unsigned char ch, const char* lumpname)
{
    memset(font->chars[ch].lumpname, 0, sizeof(font->chars[ch].lumpname));
    strncpy(font->chars[ch].lumpname, lumpname, 8);
}

static void precacheFontCharacterPatch(compositefont_t* font, unsigned char ch)
{
    // Instruct Doomsday to load the patch in monochrome mode.
    // (2 = weighted average).
    DD_SetInteger(DD_MONOCHROME_PATCHES, 2);
    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, true);

    R_PrecachePatch(font->chars[ch].lumpname, &font->chars[ch].pInfo);

    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, false);
    DD_SetInteger(DD_MONOCHROME_PATCHES, 0);
}

void R_SetFontCharacter(gamefontid_t fontId, unsigned char ch, const char* lumpname)
{
    compositefont_t* font;

    if(!isValidFontId(fontId))
    {
        Con_Message("R_SetFontCharacter: Warning, unknown font id %i.\n", (int) fontId);
        return;
    }

    font = fontForId(fontId);
    setFontCharacterPatch(font, ch, lumpname);
    precacheFontCharacterPatch(font, ch);
}

void R_InitFont(gamefontid_t fontId, const fontpatch_t* patches, size_t num)
{
    compositefont_t* font;
    size_t i;

    if(!isValidFontId(fontId))
    {
        Con_Message("R_InitFont: Warning, unknown font id %i.\n", (int) fontId);
        return;
    }

    font = fontForId(fontId);
    memset(font, 0, sizeof(*font));
    for(i = 0; i < 256; ++i)
        font->chars[i].pInfo.id = 0;

    for(i = 0; i < num; ++i)
    {
        const fontpatch_t* p = &patches[i];
        setFontCharacterPatch(font, p->ch, p->lumpName);
        precacheFontCharacterPatch(font, p->ch);
    }
}

void R_TextTicker(timespan_t ticLength)
{
    static trigger_t fixed = { 1 / 35.0 };

    // Restricted to fixed 35 Hz ticks.
    if(!M_RunTrigger(&fixed, ticLength))
        return; // It's too soon.

    typeInTime++;
}

void R_ResetTextTypeInTimer(void)
{
    typeInTime = 0;
}

static void initDrawTextState(drawtextstate_t* state)
{
    state->font = DEFAULT_FONTID;
    state->scaleX = state->scaleY = 1;
    state->offX = state->offY = 0;
    state->angle = 0;
    state->color[CR] = state->color[CG] = state->color[CB] = state->color[CA] = 1;
    state->glitter = DEFAULT_GLITTER;
    state->tracking = DEFAULT_TRACKING;
    state->leading = DEFAULT_LEADING;
    state->typeIn = true;
    state->caseScale = false;
    state->caseMod[0].scale = 1;
    state->caseMod[0].offset = 3;
    state->caseMod[1].scale = 1.25f;
    state->caseMod[1].offset = 0;
    state->numBreaks = 0;
}

/**
 * Draw a string of text controlled by parameter blocks.
 */
void GL_DrawText(const char* inString, int x, int y, gamefontid_t defFont,
    short flags, int defTracking, float defRed, float defGreen, float defBlue, float defAlpha,
    float defGlitter, boolean defCase)
{
#define SMALLBUFF_SIZE  80

    char smallBuff[SMALLBUFF_SIZE+1], *bigBuff = NULL;
    char temp[256], *str, *string, *end;
    int charCount = 0, curCase = -1;
    float cx = (float) x, cy = (float) y, width = 0, extraScale;
    drawtextstate_t state;
    size_t len;

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
    state.glitter = defGlitter;
    state.tracking = defTracking;
    state.caseScale = defCase;

    string = str;
    while(*string)
    {
        // Parse and draw the replacement string.
        if(*string == '{') // Parameters included?
        {
            gamefontid_t oldFont = state.font;
            float oldScaleY = state.scaleY;
            float oldLeading = state.leading;

            parseParamaterBlock(&string, &state);

            if(state.numBreaks > 0)
            {
                do
                {
                    cx = (float) x;
                    cy += GL_CharHeight(0, oldFont) * (1+oldLeading) * oldScaleY;
                } while(--state.numBreaks > 0);
            }
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

            strncpy(temp, string, end - string);
            temp[end - string] = 0;

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
                    alignx = -GL_TextFragmentWidth2(temp, state.font, state.tracking) * state.scaleX;
            }

            // Setup the scaling.
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();

            // Rotate.
            if(state.angle != 0)
            {
                // The origin is the specified (x,y) for the patch.
                // We'll undo the VGA aspect ratio (otherwise the result would
                // be skewed).
                DGL_Translatef((float)x, (float)y, 0);
                DGL_Scalef(1, 200.0f / 240.0f, 1);
                DGL_Rotatef(state.angle, 0, 0, 1);
                DGL_Scalef(1, 240.0f / 200.0f, 1);
                DGL_Translatef(-(float)x, -(float)y, 0);
            }

            DGL_Translatef(cx + state.offX + alignx, cy + state.offY + (state.caseScale ? state.caseMod[curCase].offset : 0), 0);
            extraScale = (state.caseScale ? state.caseMod[curCase].scale : 1);
            DGL_Scalef(state.scaleX, state.scaleY * extraScale, 1);

            // Draw it.
            DGL_Color4fv(state.color);
            GL_DrawTextFragment6(temp, 0, 0, state.font, fragmentFlags, state.tracking, state.typeIn ? charCount : 0, state.glitter);
            charCount += strlen(temp);

            // Advance the current position?
            if(!newline)
            {
                cx += (float) GL_TextFragmentWidth2(temp, state.font, state.tracking) * state.scaleX;
            }
            else
            {
                cx = (float) x;
                cy += (float) GL_TextFragmentHeight(temp, state.font) * (1+state.leading) * state.scaleY;
            }

            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();
        }
    }

    // Free temporary storage.
    if(bigBuff)
        free(bigBuff);

#undef SMALLBUFF_SIZE
}

void GL_CharDimensions(int* width, int* height, unsigned char ch, gamefontid_t font)
{
    if(!width && !height)
        return;
    if(width)
        *width = GL_CharWidth(ch, font);
    if(height)
        *height = GL_CharHeight(ch, font);
}

int GL_CharWidth(unsigned char ch, gamefontid_t fontId)
{
    return fontForId(fontId)->chars[ch].pInfo.width;
}

int GL_CharHeight(unsigned char ch, gamefontid_t fontId)
{
    return fontForId(fontId)->chars[ch].pInfo.height;
}

void GL_TextFragmentDimensions2(int* width, int* height, const char* string,
    gamefontid_t fontId, int tracking)
{
    if(!width && !height)
        return;
    if(width)
        *width = GL_TextFragmentWidth2(string, fontId, tracking);
    if(height)
        *height = GL_TextFragmentHeight(string, fontId);
}

void GL_TextFragmentDimensions(int* width, int* height, const char* string,
    gamefontid_t fontId)
{
    GL_TextFragmentDimensions2(width, height, string, fontId, DEFAULT_TRACKING);
}

/**
 * Find the visible width of the text string fragment.
 * \note Does NOT skip paramater blocks - assumes the caller has dealt
 * with them and not passed them.
 */
int GL_TextFragmentWidth2(const char* string, gamefontid_t fontId, int tracking)
{
    assert(string);
    assert(isValidFontId(fontId));
    {
    const char* ch;
    unsigned char c;
    size_t len;
    int width;
    uint i;

    if(!string[0])
        return 0;

    i = 0;
    width = 0;
    len = strlen(string);
    ch = string;
    while(i++ < len && (c = *ch++) != 0 && c != '\n')
        width += GL_CharWidth(c, fontId);

    return width + tracking * (len-1);
    }
}

int GL_TextFragmentWidth(const char* string, gamefontid_t fontId)
{
    return GL_TextFragmentWidth2(string, fontId, DEFAULT_TRACKING);
}

/**
 * Find the visible height of the text string fragment.
 * \note Does NOT skip paramater blocks - assumes the caller has dealt
 * with them and not passed them.
 */
int GL_TextFragmentHeight(const char* string, gamefontid_t fontId)
{
    assert(string);
    assert(isValidFontId(fontId));
    {
    const char* ch;
    unsigned char c;
    size_t len;
    int height;
    uint i;

    if(!string[0])
        return 0;

    i = 0;
    height = 0;
    len = strlen(string);
    ch = string;
    while(i++ < len && (c = *ch++) != 0 && c != '\n')
    {
        int charHeight = GL_CharHeight(c, fontId);
        if(charHeight > height)
            height = charHeight;
    }

    return height;
    }
}

void GL_TextDimensions(int* width, int* height, const char* string, gamefontid_t fontId)
{
    if(!width && !height)
        return;
    if(width)
        *width = GL_TextWidth(string, fontId);
    if(height)
        *height = GL_TextHeight(string, fontId);
}

/**
 * Find the visible width of the whole formatted text string.
 * Skips parameter blocks eg "{param}Text" = 4 chars
 */
int GL_TextWidth(const char* string, gamefontid_t fontId)
{
    assert(string);
    assert(isValidFontId(fontId));
    {
    int w, maxWidth = -1;
    boolean skip = false;
    const char* ch;
    size_t i, len;

    if(!string[0])
        return 0;

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

        w += GL_CharWidth(c, fontId);

        if(i == len - 1 && maxWidth == -1)
        {
            maxWidth = w;
        }
    }

    return maxWidth;
    }
}

/**
 * Find the visible height of the whole formatted text string.
 * Skips parameter blocks eg "{param}Text" = 4 chars
 */
int GL_TextHeight(const char* string, gamefontid_t fontId)
{
    assert(string);
    assert(isValidFontId(fontId));
    {
    int h, currentLineHeight;
    boolean skip = false;
    const char* ch;
    size_t i, len;

    if(!string[0])
        return 0;
    
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
            h += currentLineHeight == 0? GL_CharHeight('A', fontId) : currentLineHeight;
            currentLineHeight = 0;
            continue;
        }

        charHeight = GL_CharHeight(c, fontId);
        if(charHeight > currentLineHeight)
            currentLineHeight = charHeight;
    }
    h += currentLineHeight;

    return h;
    }
}

/**
 * Write a string using a colored, custom font and do a type-in effect.
 */
void GL_DrawTextFragment6(const char* string, int x, int y, gamefontid_t fontId, short flags,
    int tracking, int initialCount, float glitterStrength)
{
    assert(string);
    assert(isValidFontId(fontId));
    {
    boolean noTypein = (flags & DTF_NO_TYPEIN) != 0;
    boolean noShadow = (flags & DTF_NO_SHADOW) != 0;
    boolean noGlitter = (glitterStrength <= 0? true : false);
    int i, pass, w, h, cx, cy, count, yoff;
    float flash, flashMul, origColor[4], flashColor[3];
    unsigned char c;
    const char* ch;

    flash = (noGlitter? 0 : glitterStrength);

    if(!string[0])
        return;

    if(flags & DTF_ALIGN_RIGHT)
        x -= GL_TextFragmentWidth2(string, fontId, tracking);
    else if(!(flags & DTF_ALIGN_LEFT))
        x -= GL_TextFragmentWidth2(string, fontId, tracking)/2;

    if(flags & DTF_ALIGN_BOTTOM)
        y -= GL_TextFragmentHeight(string, fontId);
    else if(!(flags & DTF_ALIGN_TOP))
        y -= GL_TextFragmentHeight(string, fontId)/2;

    if(!(noTypein && noShadow && noGlitter))
        DGL_GetFloatv(DGL_CURRENT_COLOR_RGBA, origColor);

    if(!(noTypein && noGlitter))
    {
        for(i = 0; i < 3; ++i)
            flashColor[i] = (1 + 2 * origColor[i]) / 3;
    }

    for(pass = (noShadow? 1 : 0); pass < 2; ++pass)
    {
        count = initialCount;
        ch = string;
        cx = x;
        cy = y;

        for(;;)
        {
            c = *ch++;
            yoff = 0;
            flash = noGlitter? 0 : glitterStrength;
            flashMul = 0;

            if(!(noTypein && noShadow && noGlitter))
                DGL_Color4fv(origColor);

            // Do the type-in effect?
            if(!(noTypein && noGlitter) && pass)
            {
                int maxCount = (typeInTime > 0? typeInTime * 2 : 0);

                if(count == maxCount)
                {
                    flashMul = 1;
                    flashColor[CR] = origColor[CR];
                    flashColor[CG] = origColor[CG];
                    flashColor[CB] = origColor[CB];
                }
                else if(count + 1 == maxCount)
                {
                    flashMul = 0.88f;
                    flashColor[CR] = (1 + origColor[CR]) / 2;
                    flashColor[CG] = (1 + origColor[CG]) / 2;
                    flashColor[CB] = (1 + origColor[CB]) / 2;
                }
                else if(count + 2 == maxCount)
                {
                    flashMul = 0.75f;
                    flashColor[CR] = origColor[CR];
                    flashColor[CG] = origColor[CG];
                    flashColor[CB] = origColor[CB];
                }
                else if(count + 3 == maxCount)
                {
                    flashMul = 0.5f;
                    flashColor[CR] = origColor[CR];
                    flashColor[CG] = origColor[CG];
                    flashColor[CB] = origColor[CB];
                }
                else if(count > maxCount)
                {
                    break;
                }
            }
            count++;

            if(!c || c == '\n')
                break;

            w = GL_CharWidth(c, fontId);
            h = GL_CharHeight(c, fontId);

            if(patchForFontChar(fontId, c) != 0 && c != ' ')
            {
                // A character we have a patch for that is not white space.
                if(pass)
                {
                    // The character itself.
                    GL_DrawChar2(c, cx, cy + yoff, fontId);

                    if(flash > 0)
                    {   // Do something flashy.
                        DGL_Color4f(flashColor[CR], flashColor[CG], flashColor[CB], flash * flashMul);
                        drawFlash(cx, cy + yoff, w, h, true);
                    }
                }
                else if(!noShadow)
                {   // Shadow.
                    DGL_Color4f(1, 1, 1, origColor[CA] * cfg.menuShadow);
                    drawFlash(cx, cy + yoff, w, h, false);
                }
            }

            cx += w + tracking;
        }
    }

    if(!noTypein || !noShadow)
    {   // Ensure we restore the original color.
        DGL_Color4fv(origColor);
    }
    }
}

void GL_DrawTextFragment5(const char* string, int x, int y, gamefontid_t fontId, short flags,
    int tracking, int initialCount)
{
    GL_DrawTextFragment6(string, x, y, fontId, flags, tracking, initialCount, DEFAULT_GLITTER);
}

void GL_DrawTextFragment4(const char* string, int x, int y, gamefontid_t fontId, short flags, int tracking)
{
    GL_DrawTextFragment5(string, x, y, fontId, flags, tracking, DEFAULT_INITIALCOUNT);
}

void GL_DrawTextFragment3(const char* string, int x, int y, gamefontid_t fontId, short flags)
{
    GL_DrawTextFragment4(string, x, y, fontId, flags, DEFAULT_TRACKING);
}

void GL_DrawTextFragment2(const char* string, int x, int y, gamefontid_t fontId)
{
    GL_DrawTextFragment3(string, x, y, fontId, DEFAULT_DRAWFLAGS);
}

void GL_DrawTextFragment(const char* string, int x, int y)
{
    GL_DrawTextFragment2(string, x, y, DEFAULT_FONTID);
}

void GL_DrawChar3(unsigned char ch, int x, int y, gamefontid_t fontId, short flags)
{
    GL_DrawPatch2(patchForFontChar(fontId, ch), x, y, translateTextToPatchDrawFlags(flags));
}

void GL_DrawChar2(unsigned char ch, int x, int y, gamefontid_t fontId)
{
    GL_DrawChar3(ch, x, y, fontId, DEFAULT_DRAWFLAGS);
}

void GL_DrawChar(unsigned char ch, int x, int y)
{
    GL_DrawChar2(ch, x, y, DEFAULT_FONTID);
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

    DGL_Bind(DD_GetInteger(DD_DYNLIGHT_TEXTURE));

    if(bright)
        DGL_BlendMode(BM_ADD);
    else
        DGL_BlendFunc(DGL_ZERO, DGL_ONE_MINUS_SRC_ALPHA);

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

    DGL_BlendMode(BM_NORMAL);
}
