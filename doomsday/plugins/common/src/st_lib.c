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

void STlib_InitNum(st_number_t* n, int* num)
{
    n->num = num;
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
    WI_DrawPatch4(bi->p->id, bi->x, bi->y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, bi->alpha * alpha);
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
        WI_DrawPatch4(mi->p[iconNum].id, mi->x, mi->y, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, mi->alpha * alpha);
}
