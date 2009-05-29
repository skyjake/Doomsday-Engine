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

#endif
