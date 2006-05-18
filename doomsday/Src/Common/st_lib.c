/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * The status bar widget code.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#ifdef __JDOOM__
#include "jDoom/doomdef.h"
#include "jDoom/d_config.h"
#include "jDoom/st_stuff.h"
#include "jDoom/r_local.h"
#include "Common/hu_stuff.h"
#include "Common/m_swap.h"
#include "Common/st_lib.h"

#elif __JHERETIC__
#include "jHeretic/Doomdef.h"
#include "jHeretic/h_config.h"
#include "jHeretic/st_stuff.h"
#include "jHeretic/R_local.h"
#include "Common/st_lib.h"

#elif __JHEXEN__
#include "jHexen/h2def.h"
#include "jHexen/x_config.h"
#include "jHexen/st_stuff.h"
#include "jHexen/r_local.h"
#include "Common/st_lib.h"

#elif __JSTRIFE__
#include "jStrife/h2def.h"
#include "jStrife/d_config.h"
#include "jStrife/st_stuff.h"
#include "jStrife/r_local.h"
#include "Common/st_lib.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean automapactive; // in AM_map.c

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     sttminus_i;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void STlib_init(void)
{
    sttminus_i = W_GetNumForName(MINUSPATCH);
}

void STlib_initNum(st_number_t * n, int x, int y, dpatch_t * pl, int *num,
                   boolean *on, int width, float *alpha)
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

/*
 * A fairly efficient way to draw a number
 *  based on differences from the old number.
 * Note: worth the trouble?
 */
void STlib_drawNum(st_number_t * n, boolean refresh)
{
    int     numdigits = n->width;
    int     num = *n->num;

    int     w = SHORT(n->p[0].width);
    int     x = n->x;

    int     neg;

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

    // clear the area
    x = n->x - numdigits * w;

    // if non-number, do not draw it
    if(num == 1994)
        return;

    x = n->x;

    // in the special case of 0, you draw 0
    if(!num)
        WI_DrawPatch(x - w, n->y, 1, 1, 1, *n->alpha, n->p[0].lump,
                     NULL, false, ALIGN_LEFT);

    // draw the new number
    while(num && numdigits--)
    {
        x -= w;
        WI_DrawPatch(x, n->y, 1, 1, 1, *n->alpha, n->p[num % 10].lump,
                     NULL, false, ALIGN_LEFT);
        num /= 10;
    }

    // draw a minus sign if necessary
    if(neg)
        WI_DrawPatch(x - 8, n->y, 1, 1, 1, *n->alpha, sttminus_i,
                     NULL, false, ALIGN_LEFT);
}

void STlib_updateNum(st_number_t * n, boolean refresh)
{
    if(*n->on)
        STlib_drawNum(n, refresh);
}

void STlib_initPercent(st_percent_t * p, int x, int y, dpatch_t * pl, int *num,
                       boolean *on, dpatch_t * percent, float *alpha)
{
    STlib_initNum(&p->n, x, y, pl, num, on, 3, alpha);
    p->p = percent;
}

void STlib_updatePercent(st_percent_t * per, int refresh)
{
    if(refresh && *per->n.on)
        WI_DrawPatch(per->n.x, per->n.y, 1, 1, 1, *per->n.alpha, per->p->lump,
                     NULL, false, ALIGN_LEFT);

    STlib_updateNum(&per->n, refresh);
}

void STlib_initMultIcon(st_multicon_t * i, int x, int y, dpatch_t * il,
                        int *inum, boolean *on, float *alpha)
{
    i->x = x;
    i->y = y;
    i->oldinum = -1;
    i->alpha = alpha;
    i->inum = inum;
    i->on = on;
    i->p = il;
}

void STlib_updateMultIcon(st_multicon_t * mi, boolean refresh)
{
    if(*mi->on && (mi->oldinum != *mi->inum || refresh) && (*mi->inum != -1))
    {
        WI_DrawPatch(mi->x, mi->y, 1, 1, 1, *mi->alpha, mi->p[*mi->inum].lump,
                     NULL, false, ALIGN_LEFT);
        mi->oldinum = *mi->inum;
    }
}

void STlib_initBinIcon(st_binicon_t * b, int x, int y, dpatch_t * i,
                       boolean *val, boolean *on, int d, float *alpha)
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

void STlib_updateBinIcon(st_binicon_t * bi, boolean refresh)
{
    if(*bi->on && (bi->oldval != *bi->val || refresh))
    {

        WI_DrawPatch(bi->x, bi->y, 1, 1, 1, *bi->alpha, bi->p->lump,
                     NULL, false, ALIGN_LEFT);
        bi->oldval = *bi->val;
    }
}
