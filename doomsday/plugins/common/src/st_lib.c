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

void STlib_InitNum(st_number_t* n, int x, int y, patchinfo_t* pl, int* num,
                   int maxDigits, float alpha)
{
    n->x = x;
    n->y = y;
    n->maxDigits = maxDigits;
    n->alpha = alpha;
    n->num = num;
    n->p = pl;
}

void STlib_DrawNum(st_number_t* n, float alpha)
{
    int                 numdigits = n->maxDigits;
    int                 num = *n->num;
    int                 w = n->p[0].width;
    int                 x = n->x;
    int                 neg;

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
        WI_DrawPatch3(n->p[0].id, x - w, n->y, NULL, false, DPF_ALIGN_LEFT, 1, 1, 1, n->alpha * alpha);

    // Draw the number.
    while(num && numdigits--)
    {
        x -= w;
        WI_DrawPatch3(n->p[num % 10].id, x, n->y, NULL, false, DPF_ALIGN_LEFT, 1, 1, 1, n->alpha * alpha);
        num /= 10;
    }

    // Draw a minus sign if necessary.
    if(neg)
        WI_DrawPatch3(huMinus.id, x - 8, n->y, NULL, false, DPF_ALIGN_LEFT, 1, 1, 1, n->alpha * alpha);
}

void STlib_InitPercent(st_percent_t* p, int x, int y, patchinfo_t* pl, int* num,
    patchinfo_t* percent, float alpha)
{
    STlib_InitNum(&p->n, x, y, pl, num, 3, alpha);
    p->p = percent;
}

void STlib_DrawPercent(st_percent_t* per, float alpha)
{
    WI_DrawPatch3(per->p->id, per->n.x, per->n.y, NULL, false, DPF_ALIGN_LEFT, 1, 1, 1, per->n.alpha * alpha);
    STlib_DrawNum(&per->n, alpha);
}

void STlib_InitIcon(st_icon_t* b, int x, int y, patchinfo_t* i, float alpha)
{
    b->x = x;
    b->y = y;
    b->alpha = alpha;
    b->p = i;
}

void STlib_DrawIcon(st_icon_t* bi, float alpha)
{
    WI_DrawPatch3(bi->p->id, bi->x, bi->y, NULL, false, DPF_ALIGN_LEFT, 1, 1, 1, bi->alpha * alpha);
}

void STlib_InitMultiIcon(st_multiicon_t* i, int x, int y, patchinfo_t* il, float alpha)
{
    i->x = x;
    i->y = y;
    i->alpha = alpha;
    i->p = il;
}

void STlib_DrawMultiIcon(st_multiicon_t* mi, int iconNum, float alpha)
{
    if(iconNum >= 0)
        WI_DrawPatch3(mi->p[iconNum].id, mi->x, mi->y, NULL, false, DPF_ALIGN_LEFT, 1, 1, 1, mi->alpha * alpha);
}
