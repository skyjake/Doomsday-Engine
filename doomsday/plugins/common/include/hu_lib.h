/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

#ifndef __HU_LIB_H__
#define __HU_LIB_H__

#include "hu_stuff.h"

#define HU_CHARERASE        (KEY_BACKSPACE)

#define HU_MAXLINES         (4)
#define HU_MAXLINELENGTH    (160)

// Text Line widget, (parent of Scrolling Text and Input Text widgets).
typedef struct {
    int             x, y; // Left-justified position of scrolling text window.
    gamefontid_t	font;
    char            l[HU_MAXLINELENGTH + 1]; // Line of text.
    int             len; // Current line length.
    int             needsupdate; // Whether this line needs to be udpated.
} hu_textline_t;

// Scrolling Text window widget (child of Text Line widget).
typedef struct {
    hu_textline_t   l[HU_MAXLINES]; // Text lines to draw.
    int             h; // Height in lines.
    int             cl; // Current line number.
    boolean*        on; // Whether to update window.
    boolean         laston; // Last value of *->on.
} hu_stext_t;

// Input Text Line widget (child of Text Line widget).
typedef struct {
    hu_textline_t   l; // Text line to input on.
    int             lm; // Left margin past which I am not to delete characters.
    boolean*        on; // Whether to update window.
    boolean         laston; // Last value of *->on.
} hu_itext_t;

void            HUlib_init(void);

void            HUlib_clearTextLine(hu_textline_t* t);
void            HUlib_initTextLine(hu_textline_t* t, int x, int y,
                                   gamefontid_t font);
boolean         HUlib_addCharToTextLine(hu_textline_t* t, char ch);
boolean         HUlib_delCharFromTextLine(hu_textline_t* t);
void            HUlib_eraseTextLine(hu_textline_t* l);

void            HUlib_initSText(hu_stext_t* s, int x, int y, int h,
                                gamefontid_t font,boolean* on);

void            HUlib_addLineToSText(hu_stext_t* s);
void            HUlib_addMessageToSText(hu_stext_t* s, char* prefix,
                                        char* msg);
void            HUlib_drawSText(hu_stext_t* s);
void            HUlib_eraseSText(hu_stext_t* s);

void            HUlib_initIText(hu_itext_t* it, int x, int y,
                                gamefontid_t font, boolean* on);
void            HUlib_delCharFromIText(hu_itext_t* it);
void            HUlib_eraseLineFromIText(hu_itext_t* it);
void            HUlib_resetIText(hu_itext_t* it);
void            HUlib_addPrefixToIText(hu_itext_t* it, char* str);
boolean         HUlib_keyInIText(hu_itext_t* it, unsigned char ch);
void            HUlib_drawIText(hu_itext_t* it);
void            HUlib_eraseIText(hu_itext_t* it);

#endif
