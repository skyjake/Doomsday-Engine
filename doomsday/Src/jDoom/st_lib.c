// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log$
// Revision 1.3  2003/08/19 16:33:44  skyjake
// Use WI_DrawPatch instead of GL_DrawPatch
//
// Revision 1.2  2003/02/27 23:14:33  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:22:09  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:48  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	The status bar widget code.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id$";

#include <ctype.h>

#include "doomdef.h"

#include "v_video.h"

#include "m_swap.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "wi_stuff.h"
#include "r_local.h"


// in AM_map.c
extern boolean		automapactive; 




//
// Hack display negative frags.
//  Loads and store the stminus lump.
//
//patch_t*		sttminus;
int				sttminus_i;

void STlib_init(void)
{
    //sttminus = (patch_t *) W_CacheLumpName("STTMINUS", PU_STATIC);
	sttminus_i = W_GetNumForName("STTMINUS");
}


// ?
void
STlib_initNum
( st_number_t*		n,
  int			x,
  int			y,
  dpatch_t*		pl,
  int*			num,
  boolean*		on,
  int			width )
{
    n->x	= x;
    n->y	= y;
    n->oldnum	= 0;
    n->width	= width;
    n->num	= num;
    n->on	= on;
    n->p	= pl;
}


// 
// A fairly efficient way to draw a number
//  based on differences from the old number.
// Note: worth the trouble?
//
void
STlib_drawNum
( st_number_t*	n,
  boolean	refresh )
{
    int		numdigits = n->width;
    int		num = *n->num;
    
    int		w = SHORT(n->p[0].width);
    int		h = SHORT(n->p[0].height);
    int		x = n->x;
    
    int		neg;

    n->oldnum = *n->num;

    neg = num < 0;

    if (neg)
    {
		if (numdigits == 2 && num < -9)
			num = -9;
		else if (numdigits == 3 && num < -99)
			num = -99;
		num = -num;
    }

    // clear the area
    x = n->x - numdigits*w;

/*    if (n->y - ST_Y < 0)
	I_Error("drawNum: n->y - ST_Y < 0");*/

//    V_CopyRect(x, n->y - ST_Y, BG, w*numdigits, h, x, n->y, FG);

    // if non-number, do not draw it
    if (num == 1994)
	return;

    x = n->x;

    // in the special case of 0, you draw 0
    if (!num) WI_DrawPatch(x - w, n->y, n->p[ 0 ].lump);

    // draw the new number
    while (num && numdigits--)
    {
		x -= w;
		WI_DrawPatch(x, n->y, n->p[ num % 10 ].lump);
		num /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
		WI_DrawPatch(x - 8, n->y, sttminus_i);
}


//
void
STlib_updateNum
( st_number_t*		n,
  boolean		refresh )
{
    if (*n->on) STlib_drawNum(n, refresh);
}


//
void
STlib_initPercent
( st_percent_t*		p,
  int			x,
  int			y,
  dpatch_t*		pl,
  int*			num,
  boolean*		on,
  dpatch_t*		percent )
{
    STlib_initNum(&p->n, x, y, pl, num, on, 3);
    p->p = percent;
}




void
STlib_updatePercent
( st_percent_t*		per,
  int			refresh )
{
    if (refresh && *per->n.on)
		WI_DrawPatch(per->n.x, per->n.y, per->p->lump);
    
    STlib_updateNum(&per->n, refresh);
}



void
STlib_initMultIcon
( st_multicon_t*	i,
  int			x,
  int			y,
  dpatch_t*		il,
  int*			inum,
  boolean*		on )
{
    i->x	= x;
    i->y	= y;
    i->oldinum 	= -1;
    i->inum	= inum;
    i->on	= on;
    i->p	= il;
}



void
STlib_updateMultIcon
( st_multicon_t*	mi,
  boolean		refresh )
{
    /*int			w;
    int			h;
    int			x;
    int			y;*/

    if (*mi->on	&& (mi->oldinum != *mi->inum || refresh) && (*mi->inum!=-1))
    {
		/*if (mi->oldinum != -1)
		{
			x = mi->x - SHORT(mi->p[mi->oldinum].patch->leftoffset);
			y = mi->y - SHORT(mi->p[mi->oldinum].patch->topoffset);
			w = SHORT(mi->p[mi->oldinum].patch->width);
			h = SHORT(mi->p[mi->oldinum].patch->height);

			if (y - ST_Y < 0)
				I_Error("updateMultIcon: y - ST_Y < 0");

//			V_CopyRect(x, y-ST_Y, BG, w, h, x, y, FG);
		}*/
		WI_DrawPatch(mi->x, mi->y, mi->p[*mi->inum].lump);
		mi->oldinum = *mi->inum;
    }
}



void
STlib_initBinIcon
( st_binicon_t*		b,
  int			x,
  int			y,
  dpatch_t*		i,
  boolean*		val,
  boolean*		on )
{
    b->x	= x;
    b->y	= y;
    b->oldval	= 0;
    b->val	= val;
    b->on	= on;
    b->p	= i;
}



void
STlib_updateBinIcon
( st_binicon_t*		bi,
  boolean		refresh )
{
    /*int			x;
    int			y;
    int			w;
    int			h;*/

    if (*bi->on && (bi->oldval != *bi->val || refresh))
    {
		/*x = bi->x - SHORT(bi->p->patch->leftoffset);
		y = bi->y - SHORT(bi->p->patch->topoffset);
		w = SHORT(bi->p->patch->width);
		h = SHORT(bi->p->patch->height);

		if (y - ST_Y < 0)
			I_Error("updateBinIcon: y - ST_Y < 0");*/

//	if (*bi->val)
	    WI_DrawPatch(bi->x, bi->y, /*FG,*/ bi->p->lump);
	/*else
	    V_CopyRect(x, y-ST_Y, BG, w, h, x, y, FG);*/

		bi->oldval = *bi->val;
    }

}

