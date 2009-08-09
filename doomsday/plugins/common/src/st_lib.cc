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

/**
 * st_lib.c: The status bar widget code.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#if __JDOOM__
#  include "jdoom.h"
#  include "hu_stuff.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#  include "hu_stuff.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "hu_stuff.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "st_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void STlib_initNum(st_number_t* n, int x, int y, dpatch_t* pl, int* num,
                   boolean* on, int width, float* alpha)
{
    n->x = x;
    n->y = y;
    n->oldnum = 0;
    n->width = width;
    n->alpha = alpha;
    n->num = num;
    n->on = on;
    n->p = pl;
}

void STlib_drawNum(st_number_t* n, boolean refresh)
{
    int                 numdigits = n->width;
    int                 num = *n->num;
    int                 w = n->p[0].width;
    int                 x = n->x;
    int                 neg;

    n->oldnum = *n->num;

    neg = num < 0;

    if(neg)
    {
        if(numdigits == 2 && num < -9)
            num = -9;
        else if(numdigits == 3 && num < -99)
            num = -99;
        num = -num;
    }

    x = n->x - numdigits * w;

    // If non-number, do not draw it.
    if(num == 1994)
        return;

    x = n->x;

    // In the special case of 0, you draw 0.
    if(!num)
        WI_DrawPatch(x - w, n->y, 1, 1, 1, *n->alpha, &n->p[0],
                     NULL, false, ALIGN_LEFT);

    // Draw the number.
    while(num && numdigits--)
    {
        x -= w;
        WI_DrawPatch(x, n->y, 1, 1, 1, *n->alpha, &n->p[num % 10],
                     NULL, false, ALIGN_LEFT);
        num /= 10;
    }

    // Draw a minus sign if necessary.
    if(neg)
        WI_DrawPatch(x - 8, n->y, 1, 1, 1, *n->alpha, &huMinus,
                     NULL, false, ALIGN_LEFT);
}

void STlib_updateNum(st_number_t* n, boolean refresh)
{
    if(*n->on)
        STlib_drawNum(n, refresh);
}

void STlib_initPercent(st_percent_t* p, int x, int y, dpatch_t* pl, int* num,
                       boolean* on, dpatch_t* percent, float* alpha)
{
    STlib_initNum(&p->n, x, y, pl, num, on, 3, alpha);
    p->p = percent;
}

void STlib_updatePercent(st_percent_t* per, int refresh)
{
    if(refresh && *per->n.on)
        WI_DrawPatch(per->n.x, per->n.y, 1, 1, 1, *per->n.alpha, per->p,
                     NULL, false, ALIGN_LEFT);

    STlib_updateNum(&per->n, refresh);
}

void STlib_initMultIcon(st_multicon_t* i, int x, int y, dpatch_t* il,
                        int* iconNum, boolean* on, float* alpha)
{
    i->x = x;
    i->y = y;
    i->oldIconNum = -1;
    i->alpha = alpha;
    i->iconNum = iconNum;
    i->on = on;
    i->p = il;
}

void STlib_updateMultIcon(st_multicon_t* mi, boolean refresh)
{
    if(*mi->on && (mi->oldIconNum != *mi->iconNum || refresh) &&
       *mi->iconNum != -1)
    {
        WI_DrawPatch(mi->x, mi->y, 1, 1, 1, *mi->alpha,
                     &mi->p[*mi->iconNum], NULL, false, ALIGN_LEFT);
        mi->oldIconNum = *mi->iconNum;
    }
}

void STlib_initBinIcon(st_binicon_t* b, int x, int y, dpatch_t* i,
                       boolean* val, boolean* on, int d, float* alpha)
{
    b->x = x;
    b->y = y;
    b->val = val;
    b->alpha = alpha;
    b->oldval = 0;
    b->on = on;
    b->p = i;
    b->data =d;
}

void STlib_updateBinIcon(st_binicon_t* bi, boolean refresh)
{
    if(*bi->on && (bi->oldval != *bi->val || refresh))
    {
        WI_DrawPatch(bi->x, bi->y, 1, 1, 1, *bi->alpha, bi->p,
                     NULL, false, ALIGN_LEFT);
        bi->oldval = *bi->val;
    }
}
