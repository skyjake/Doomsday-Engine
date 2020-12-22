/** @file api_fontrender.h Font renderer.
 *
 * @ingroup gl
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_API_FONT_RENDERER_H
#define DE_API_FONT_RENDERER_H

#include "api_base.h"

#ifdef __cplusplus
extern "C" {
#endif

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

DE_API_TYPEDEF(FR)
{
    de_api_t api;

    fontid_t (*ResolveUri)(const struct uri_s *uri);

    /// @return  Unique identifier associated with the current font.
    fontid_t (*Font)(void);

    /// Change the current font.
    void (*SetFont)(fontid_t font);

    /// Push the attribute stack.
    void (*PushAttrib)(void);

    /// Pop the attribute stack.
    void (*PopAttrib)(void);

    /// Load the default attributes at the current stack depth.
    void (*LoadDefaultAttrib)(void);

    /// @return  Current leading (attribute).
    float (*Leading)(void);

    void (*SetLeading)(float value);

    /// @return  Current tracking (attribute).
    int (*Tracking)(void);

    void (*SetTracking)(int value);

    /// Retrieve the current color and alpha factors.
    void (*ColorAndAlpha)(float rgba[4]);

    void (*SetColor)(float red, float green, float blue);

    void (*SetColorv)(const float rgb[3]);

    void (*SetColorAndAlpha)(float red, float green, float blue, float alpha);

    void (*SetColorAndAlphav)(const float rgba[4]);

    /// @return  Current red color factor.
    float (*ColorRed)(void);

    void (*SetColorRed)(float value);

    /// @return  Current green color factor.
    float (*ColorGreen)(void);

    void (*SetColorGreen)(float value);

    /// @return  Current blue color factor.
    float (*ColorBlue)(void);

    void (*SetColorBlue)(float value);

    /// @return  Current alpha factor.
    float (*Alpha)(void);

    void (*SetAlpha)(float value);

    /// Retrieve the current shadow offset (attribute).
    void (*ShadowOffset)(int* offsetX, int* offsetY);

    void (*SetShadowOffset)(int offsetX, int offsetY);

    /// @return  Current shadow strength (attribute).
    float (*ShadowStrength)(void);

    void (*SetShadowStrength)(float value);

    /// @return  Current glitter strength (attribute).
    float (*GlitterStrength)(void);

    void (*SetGlitterStrength)(float value);

    /// @return  Current case scale (attribute).
    dd_bool (*CaseScale)(void);

    void (*SetCaseScale)(dd_bool value);

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
     * <pre>
     *   "{r = 1.0; g = 0.0; b = 0.0; case}This is red text with a case-scaled first character"
     *   "This is text with an {y = -14}offset{y = 0} internal fragment."
     *   "{fontb; r=0.5; g=1; b=0; x=2; y=-2}This is good!"
     * </pre>
     */

    /**
     * Draw a text block.
     *
     * @param text    Block of text to be drawn.
     * @param origin  Orient drawing about this offset (topleft:[0,0]).
     */
    void (*DrawText)(const char* text, const Point2Raw* origin);

    /**
     * @copydoc de_api_FR_s::DrawText
     * @param alignFlags  @ref alignmentFlags
     */
    void (*DrawText2)(const char* text, const Point2Raw* origin, int alignFlags);

    /**
     * Draw a text block.
     *
     * @param text        Block of text to be drawn.
     * @param _origin     Orient drawing about this offset (topleft:[0,0]).
     * @param alignFlags  @ref alignmentFlags
     * @param _textFlags  @ref drawTextFlags
     */
    void (*DrawText3)(const char* text, const Point2Raw* _origin, int alignFlags, uint16_t _textFlags);

    void (*DrawTextXY3)(const char* text, int x, int y, int alignFlags, short flags);

    void (*DrawTextXY2)(const char* text, int x, int y, int alignFlags);

    void (*DrawTextXY)(const char* text, int x, int y);

    // Utility routines:
    void (*TextSize)(Size2Raw* size, const char* text);

    /// @return  Visible width of the text.
    int (*TextWidth)(const char* text);

    /// @return  Visible height of the text.
    int (*TextHeight)(const char* text);

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
    void (*DrawChar3)(unsigned char ch, const Point2Raw* origin, int alignFlags, short textFlags);

    void (*DrawChar2)(unsigned char ch, const Point2Raw* origin, int alignFlags);

    void (*DrawChar)(unsigned char ch, const Point2Raw* origin);

    void (*DrawCharXY3)(unsigned char ch, int x, int y, int alignFlags, short textFlags);

    void (*DrawCharXY2)(unsigned char ch, int x, int y, int alignFlags);

    void (*DrawCharXY)(unsigned char ch, int x, int y);

    // Utility routines:
    void (*CharSize)(Size2Raw* size, unsigned char ch);

    /// @return  Visible width of the character.
    int (*CharWidth)(unsigned char ch);

    /// @return  Visible height of the character.
    int (*CharHeight)(unsigned char ch);

    /// @deprecated Will be replaced with per-text-object animations.
    void (*ResetTypeinTimer)(void);

}
DE_API_T(FR);

#ifndef DE_NO_API_MACROS_FONT_RENDER
#define Fonts_ResolveUri        _api_FR.ResolveUri
#define FR_Font                 _api_FR.Font
#define FR_SetFont              _api_FR.SetFont
#define FR_PushAttrib           _api_FR.PushAttrib
#define FR_PopAttrib            _api_FR.PopAttrib
#define FR_LoadDefaultAttrib    _api_FR.LoadDefaultAttrib
#define FR_Leading              _api_FR.Leading
#define FR_SetLeading           _api_FR.SetLeading
#define FR_Tracking             _api_FR.Tracking
#define FR_SetTracking          _api_FR.SetTracking
#define FR_ColorAndAlpha        _api_FR.ColorAndAlpha
#define FR_SetColor             _api_FR.SetColor
#define FR_SetColorv            _api_FR.SetColorv
#define FR_SetColorAndAlpha     _api_FR.SetColorAndAlpha
#define FR_SetColorAndAlphav    _api_FR.SetColorAndAlphav
#define FR_ColorRed             _api_FR.ColorRed
#define FR_SetColorRed          _api_FR.SetColorRed
#define FR_ColorGreen           _api_FR.ColorGreen
#define FR_SetColorGreen        _api_FR.SetColorGreen
#define FR_ColorBlue            _api_FR.ColorBlue
#define FR_SetColorBlue         _api_FR.SetColorBlue
#define FR_Alpha                _api_FR.Alpha
#define FR_SetAlpha             _api_FR.SetAlpha
#define FR_ShadowOffset         _api_FR.ShadowOffset
#define FR_SetShadowOffset      _api_FR.SetShadowOffset
#define FR_ShadowStrength       _api_FR.ShadowStrength
#define FR_SetShadowStrength    _api_FR.SetShadowStrength
#define FR_GlitterStrength      _api_FR.GlitterStrength
#define FR_SetGlitterStrength   _api_FR.SetGlitterStrength
#define FR_CaseScale            _api_FR.CaseScale
#define FR_SetCaseScale         _api_FR.SetCaseScale
#define FR_DrawText             _api_FR.DrawText
#define FR_DrawText2            _api_FR.DrawText2
#define FR_DrawText3            _api_FR.DrawText3
#define FR_DrawTextXY3          _api_FR.DrawTextXY3
#define FR_DrawTextXY2          _api_FR.DrawTextXY2
#define FR_DrawTextXY           _api_FR.DrawTextXY
#define FR_TextSize             _api_FR.TextSize
#define FR_TextWidth            _api_FR.TextWidth
#define FR_TextHeight           _api_FR.TextHeight
#define FR_DrawChar3            _api_FR.DrawChar3
#define FR_DrawChar2            _api_FR.DrawChar2
#define FR_DrawChar             _api_FR.DrawChar
#define FR_DrawCharXY3          _api_FR.DrawCharXY3
#define FR_DrawCharXY2          _api_FR.DrawCharXY2
#define FR_DrawCharXY           _api_FR.DrawCharXY
#define FR_CharSize             _api_FR.CharSize
#define FR_CharWidth            _api_FR.CharWidth
#define FR_CharHeight           _api_FR.CharHeight
#define FR_ResetTypeinTimer     _api_FR.ResetTypeinTimer
#endif

#if defined __DOOMSDAY__ && defined __CLIENT__
DE_USING_API(FR);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_API_FONT_RENDERER_H */
