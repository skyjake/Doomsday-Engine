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
    int             width; // Max # of digits in number.
    int             oldnum; // Last number value.
    float*          alpha; // Pointer to alpha.
    int*            num; // Pointer to current value.
    boolean*        on; // Pointer to boolean stating whether to update number.
    dpatch_t*       p; // List of patches for 0-9.
    int             data; // User data.
} st_number_t;

/**
 * Percent widget
 * "child" of number widget, or, more precisely, contains a number widget.
 */
typedef struct {
    // number information
    st_number_t     n;

    // percent sign graphic
    dpatch_t*       p;
} st_percent_t;

/**
 * Multiple Icon widget.
 */
typedef struct {
    int             x, y; // Center-justified location of icons.
    int             oldIconNum; // Last icon number.
    int*            iconNum; // Pointer to current icon.
    float*          alpha; // Pointer to alpha.
    boolean*        on; // Pointer to boolean stating whether to update icon.
    dpatch_t*       p; // List of icons.
    int             data; // User data.
} st_multicon_t;

/**
 * Binary Icon widget.
 */
typedef struct {
    int             x, y; // Center-justified location of icon.
    int             oldval; // Last icon value.
    boolean*        val; // Pointer to current icon status.
    float*          alpha; // Pointer to alpha.
    boolean*        on; // Pointer to boolean stating whether to update icon.
    dpatch_t*       p; // Icon.
    int             data; // User data.
} st_binicon_t;

void            STlib_initNum(st_number_t* n, int x, int y, dpatch_t* pl,
                              int* num, boolean* on, int width, float* alpha);
void            STlib_updateNum(st_number_t* n, boolean refresh);
void            STlib_initPercent(st_percent_t* p, int x, int y,
                                  dpatch_t* pl, int* num, boolean* on,
                                  dpatch_t* percent, float* alpha);
void            STlib_updatePercent(st_percent_t* per, int refresh);
void            STlib_initMultIcon(st_multicon_t* mi, int x, int y,
                                   dpatch_t* il, int* inum, boolean* on,
                                   float* alpha);
void            STlib_updateMultIcon(st_multicon_t* mi, boolean refresh);
void            STlib_initBinIcon(st_binicon_t* b, int x, int y, dpatch_t* i,
                                  boolean* val, boolean* on, int d, float* alpha);
void            STlib_updateBinIcon(st_binicon_t* bi, boolean refresh);

#endif
