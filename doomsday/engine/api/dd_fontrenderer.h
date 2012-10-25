/**
 * @file dd_fontrenderer.h
 * Font renderer.
 *
 * @ingroup gl
 *
 * @authors Copyright © 1999-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_API_FONT_RENDERER_H
#define LIBDENG_API_FONT_RENDERER_H

/**
 * Font attributes are managed as a finite stack of attribute sets.
 * This value defines the maximum allowed depth of the attribute stack.
 */
#define FR_MAX_ATTRIB_STACK_DEPTH       (8)

/**
 * Font attribute defaults. Used with FR_LoadDefaultAttrib.
 */
#define FR_DEF_ATTRIB_LEADING           (.5)
#define FR_DEF_ATTRIB_TRACKING          (0)
#define FR_DEF_ATTRIB_COLOR_RED         (1)
#define FR_DEF_ATTRIB_COLOR_GREEN       (1)
#define FR_DEF_ATTRIB_COLOR_BLUE        (1)
#define FR_DEF_ATTRIB_ALPHA             (1)
#define FR_DEF_ATTRIB_GLITTER_STRENGTH  (.5)
#define FR_DEF_ATTRIB_SHADOW_STRENGTH   (.5)
#define FR_DEF_ATTRIB_SHADOW_XOFFSET    (2)
#define FR_DEF_ATTRIB_SHADOW_YOFFSET    (2)
#define FR_DEF_ATTRIB_CASE_SCALE        (false)

/**
 * @defgroup drawTextFlags  Draw Text Flags
 * @ingroup apiFlags
 */
/*@{*/
#define DTF_NO_TYPEIN                   (0x0001)
#define DTF_NO_SHADOW                   (0x0002)
#define DTF_NO_GLITTER                  (0x0004)

#define DTF_NO_EFFECTS                  (DTF_NO_TYPEIN|DTF_NO_SHADOW|DTF_NO_GLITTER)
#define DTF_ONLY_SHADOW                 (DTF_NO_TYPEIN|DTF_NO_GLITTER)
/*@}*/

/// @return  Unique identifier associated with the current font.
fontid_t FR_Font(void);

/// Change the current font.
void FR_SetFont(fontid_t font);

/// Push the attribute stack.
void FR_PushAttrib(void);

/// Pop the attribute stack.
void FR_PopAttrib(void);

/// Load the default attributes at the current stack depth.
void FR_LoadDefaultAttrib(void);

/// @return  Current leading (attribute).
float FR_Leading(void);

void FR_SetLeading(float value);

/// @return  Current tracking (attribute).
int FR_Tracking(void);

void FR_SetTracking(int value);

/// Retrieve the current color and alpha factors.
void FR_ColorAndAlpha(float rgba[4]);

void FR_SetColor(float red, float green, float blue);
void FR_SetColorv(const float rgb[3]);

void FR_SetColorAndAlpha(float red, float green, float blue, float alpha);
void FR_SetColorAndAlphav(const float rgba[4]);

/// @return  Current red color factor.
float FR_ColorRed(void);

void FR_SetColorRed(float value);

/// @return  Current green color factor.
float FR_ColorGreen(void);

void FR_SetColorGreen(float value);

/// @return  Current blue color factor.
float FR_ColorBlue(void);

void FR_SetColorBlue(float value);

/// @return  Current alpha factor.
float FR_Alpha(void);

void FR_SetAlpha(float value);

/// Retrieve the current shadow offset (attribute).
void FR_ShadowOffset(int* offsetX, int* offsetY);

void FR_SetShadowOffset(int offsetX, int offsetY);

/// @return  Current shadow strength (attribute).
float FR_ShadowStrength(void);

void FR_SetShadowStrength(float value);

/// @return  Current glitter strength (attribute).
float FR_GlitterStrength(void);

void FR_SetGlitterStrength(float value);

/// @return  Current case scale (attribute).
boolean FR_CaseScale(void);

void FR_SetCaseScale(boolean value);

/**
 * Text blocks (possibly formatted and/or multi-line text):
 *
 * Formatting of text blocks is initially determined by the current font
 * renderer state at draw time (i.e., the attribute stack and draw paramaters).
 *
 ** Paramater blocks:
 *
 * A single text block may also embed attribute and draw paramater changes
 * within the text string itself. Paramater blocks are defined within the curly
 * bracketed escape sequence {&nbsp;} A single paramater block may
 * contain any number of attribute and draw paramaters delimited by semicolons.
 *
 * A text block may contain any number of paramater blocks. The scope for which
 * extends until the last character has been drawn or another block overrides
 * the same attribute.
 *
 * Examples:
 *
 *   "{r = 1.0; g = 0.0; b = 0.0; case}This is red text with a case-scaled first character"
 *   "This is text with an {y = -14}offset{y = 0} internal fragment."
 *   "{fontb; r=0.5; g=1; b=0; x=2; y=-2}This is good!"
 */

/**
 * Draw a text block.
 * @note Member of the Doomsday public API.
 *
 * @param text    Block of text to be drawn.
 * @param origin  Orient drawing about this offset (topleft:[0,0]).
 */
void FR_DrawText(const char* text, const Point2Raw* origin);

/**
 * @copydoc FR_DrawText()
 * @param alignFlags  @ref alignmentFlags
 */
void FR_DrawText2(const char* text, const Point2Raw* origin, int alignFlags);

/**
 * Draw a text block.
 * @note Member of the Doomsday public API.
 *
 * @param text        Block of text to be drawn.
 * @param _origin     Orient drawing about this offset (topleft:[0,0]).
 * @param alignFlags  @ref alignmentFlags
 * @param _textFlags  @ref drawTextFlags
 */
void FR_DrawText3(const char* text, const Point2Raw* _origin, int alignFlags, short _textFlags);

void FR_DrawTextXY3(const char* text, int x, int y, int alignFlags, short flags);
void FR_DrawTextXY2(const char* text, int x, int y, int alignFlags);
void FR_DrawTextXY(const char* text, int x, int y);

// Utility routines:
void FR_TextSize(Size2Raw* size, const char* text);
/// @return  Visible width of the text.
int FR_TextWidth(const char* text);
/// @return  Visible height of the text.
int FR_TextHeight(const char* text);

/*
 * Single characters:
 */

/**
 * Draw a character.
 *
 * @param ch  Character to be drawn.
 * @param origin  Origin/offset at which to begin drawing.
 * @param alignFlags  @ref alignmentFlags
 * @param textFlags  @ref drawTextFlags
 */
void FR_DrawChar3(unsigned char ch, const Point2Raw* origin, int alignFlags, short textFlags);
void FR_DrawChar2(unsigned char ch, const Point2Raw* origin, int alignFlags);
void FR_DrawChar(unsigned char ch, const Point2Raw* origin);

void FR_DrawCharXY3(unsigned char ch, int x, int y, int alignFlags, short textFlags);
void FR_DrawCharXY2(unsigned char ch, int x, int y, int alignFlags);
void FR_DrawCharXY(unsigned char ch, int x, int y);

// Utility routines:
void FR_CharSize(Size2Raw* size, unsigned char ch);
/// @return  Visible width of the character.
int FR_CharWidth(unsigned char ch);
/// @return  Visible height of the character.
int FR_CharHeight(unsigned char ch);

/// \deprecated Will be replaced with per-text-object animations.
void FR_ResetTypeinTimer(void);

#endif /* LIBDENG_API_FONT_RENDERER_H */
