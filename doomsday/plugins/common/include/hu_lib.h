/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBCOMMON_UI_LIBRARY_H
#define LIBCOMMON_UI_LIBRARY_H

#include "hu_stuff.h"

typedef struct {
    int id;
    float scale;
    void (*draw) (int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight);
    float* textAlpha, *iconAlpha; /// \todo refactor away.
} uiwidget_t;

/**
 * @defgroup uiWidgetFlags UI Widget Flags
 */
/*@{*/
#define UWF_LEFT2RIGHT      0x0001
#define UWF_RIGHT2LEFT      0x0002
#define UWF_TOP2BOTTOM      0x0004
#define UWF_BOTTOM2TOP      0x0008
/*@}*/

void            UI_DrawWidgets(const uiwidget_t* widgets, size_t numWidgets, short flags, int padding, int x, int y, int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight);

#define HU_MAXLINELENGTH    (160)

// Text Line widget, (parent of Scrolling Text and Input Text widgets).
typedef struct {
    int             x, y; // Left-justified position of scrolling text window.
    char            l[HU_MAXLINELENGTH + 1]; // Line of text.
    int             len; // Current line length.
    int             needsupdate; // Whether this line needs to be udpated.
} hu_textline_t;

// Input Text Line widget (child of Text Line widget).
typedef struct {
    hu_textline_t   l; // Text line to input on.
    int             lm; // Left margin past which I am not to delete characters.
    boolean*        on; // Whether to update window.
    boolean         laston; // Last value of *->on.
} hu_text_t;

void            HUlib_init(void);

void            HUlib_clearTextLine(hu_textline_t* t);
void            HUlib_initTextLine(hu_textline_t* t, int x, int y);
boolean         HUlib_addCharToTextLine(hu_textline_t* t, char ch);
boolean         HUlib_delCharFromTextLine(hu_textline_t* t);
void            HUlib_eraseTextLine(hu_textline_t* l);

void            HUlib_initText(hu_text_t* it, int x, int y, boolean* on);
void            HUlib_delCharFromText(hu_text_t* it);
void            HUlib_eraseLineFromText(hu_text_t* it);
void            HUlib_resetText(hu_text_t* it);
void            HUlib_addPrefixToText(hu_text_t* it, char* str);
boolean         HUlib_keyInText(hu_text_t* it, unsigned char ch);
void            HUlib_drawText(hu_text_t* it, gamefontid_t font);
void            HUlib_eraseText(hu_text_t* it);

#endif /* LIBCOMMON_UI_LIBRARY_H */
