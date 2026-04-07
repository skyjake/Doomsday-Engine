/** @file ui_main.h  Graphical User Interface (obsolete).
 *
 * Has ties to the console routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_UI_MAIN_H
#define DE_UI_MAIN_H

#include <de/legacy/rect.h>
#include "ddevent.h"
#ifdef __CLIENT__
#  include "resource/materialvariantspec.h"
#endif

/*enum fontstyle_t {
    FS_NORMAL,
    FS_BOLD,
    FS_LIGHT,
    FONTSTYLE_COUNT
};*/

/// Numeric identifiers of predefined colors.
/// @ingroup infine
enum {
    UIC_TEXT,
    UIC_TITLE,
    /*
    UIC_SHADOW,
    UIC_BG_LIGHT,
    UIC_BG_MEDIUM,
    UIC_BG_DARK,
    UIC_BRD_HI,
    UIC_BRD_MED,
    UIC_BRD_LOW,
    UIC_HELP,
    */
    NUM_UI_COLORS
};

/// Standard dimensions.
#define UI_WIDTH            (1000.0f)
#define UI_HEIGHT           (1000.0f)
//#define UI_BORDER           (UI_WIDTH/120) /// All borders are this wide.
#define UI_SHADOW_OFFSET    (MIN_OF(3, UI_WIDTH/320))
#define UI_SHADOW_STRENGTH  (.6f)
//#define UI_BUTTON_BORDER    (UI_BORDER)
//#define UI_BAR_WDH          (UI_BORDER * 3)
//#define UI_BAR_BORDER       (UI_BORDER / 2)
//#define UI_BAR_BUTTON_BORDER (3 * UI_BAR_BORDER / 2)
#define UI_MAX_COLUMNS      10 /// Maximum columns for list box.

typedef struct {
    float red, green, blue;
} ui_color_t;

DE_EXTERN_C fontid_t fontFixed; //, fontVariable[FONTSTYLE_COUNT];

//void UI_Register(void);

void UI_LoadFonts(void);

const char *UI_ChooseFixedFont(void);
//const char *UI_ChooseVariableFont(fontstyle_t style);

/// @param id  Id number of the color to return e.g. "UIC_TEXT".
ui_color_t* UI_Color(uint id);

/// @return  Height of the current UI font.
//int UI_FontHeight(void);

/**
 * Background with the "The Doomsday Engine" text superimposed.
 *
 * @param origin  Screen-space coordinate origin (top left).
 * @param size  Screen-space dimensions of the cursor in pixels.
 * @param alpha  Alpha level to use when drawing the background.
 */
void UI_DrawDDBackground(const Point2Raw &origin, const Size2Raw &size, float alpha);

void UI_MixColors(ui_color_t* a, ui_color_t* b, ui_color_t* dest, float amount);
void UI_SetColorA(ui_color_t* color, float alpha);
void UI_SetColor(ui_color_t* color);
//void UI_Gradient(const Point2Raw* origin, const Size2Raw* size, ui_color_t* top, ui_color_t* bottom, float topAlpha, float bottomAlpha);
//void UI_GradientEx(const Point2Raw* origin, const Size2Raw* size, int border, ui_color_t* top, ui_color_t* bottom, float topAlpha, float bottomAlpha);
//void UI_DrawRectEx(const Point2Raw* origin, const Size2Raw* size, int brd, dd_bool filled, ui_color_t* top, ui_color_t* bottom, float alpha, float bottomAlpha);

/// Draw shadowed text.
void UI_TextOutEx(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha);
void UI_TextOutEx2(const char* text, const Point2Raw* origin, ui_color_t* color, float alpha, int alignFlags, short textFlags);

//const de::MaterialVariantSpec &UI_MaterialSpec(int texSpecFlags = 0);

#endif
