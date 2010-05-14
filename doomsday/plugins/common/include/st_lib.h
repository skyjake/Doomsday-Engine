/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * st_lib.h: HUD, Status Widgets.
 */

#ifndef __COMMON_STLIB__
#define __COMMON_STLIB__

#include "hu_stuff.h"

/**
 * Number widget.
 */
typedef struct {
    int             x, y; // Upper right-hand corner of the number (right-justified).
    int             maxDigits; // Max # of digits in number.
    float           alpha;
    int*            num; // Pointer to current value.
    patchinfo_t*    p; // List of patches for 0-9.
} st_number_t;

void            STlib_InitNum(st_number_t* n, int x, int y, patchinfo_t* pl,
                              int* num, int maxDigits, float alpha);
void            STlib_DrawNum(st_number_t* n, float alpha);

/**
 * Percent widget
 * "child" of number widget, or, more precisely, contains a number widget.
 */
typedef struct {
    st_number_t     n; // Number.
    patchinfo_t*       p; // Percent sign graphic.
} st_percent_t;

void            STlib_InitPercent(st_percent_t* p, int x, int y,
                                  patchinfo_t* pl, int* num,
                                  patchinfo_t* percent, float alpha);
void            STlib_DrawPercent(st_percent_t* per, float alpha);

/**
 * Icon widget.
 */
typedef struct {
    int             x, y; // Center-justified location of icon.
    float           alpha;
    patchinfo_t*       p; // Icon.
} st_icon_t;

void            STlib_InitIcon(st_icon_t* b, int x, int y,
                               patchinfo_t* i, float alpha);
void            STlib_DrawIcon(st_icon_t* bi, float alpha);

/**
 * Multi Icon widget.
 */
typedef struct {
    int             x, y; // Center-justified location of icons.
    float           alpha;
    patchinfo_t*       p; // List of icons.
} st_multiicon_t;

void            STlib_InitMultiIcon(st_multiicon_t* mi, int x, int y,
                                    patchinfo_t* il, float alpha);
void            STlib_DrawMultiIcon(st_multiicon_t* mi, int icon, float alpha);

#endif
