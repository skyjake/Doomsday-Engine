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
// DESCRIPTION:
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_MAIN__
#define __R_MAIN__

#include "d_player.h"
#include "r_data.h"
#include "r_sky.h"


#ifdef __GNUG__
#pragma interface
#endif


//
// POV related.
//
extern fixed_t		viewcos;
extern fixed_t		viewsin;

extern int		centerx;
extern int		centery;

extern fixed_t		centerxfrac;
extern fixed_t		centeryfrac;
extern fixed_t		projection;

//extern int		validcount;
#define		validcount		(*gi.validcount)

extern int		linecount;
extern int		loopcount;


//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

extern int extralight;

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS		32


// Blocky/low detail mode.
//B remove this?
//  0 = high, 1 = low
extern	int		detailshift;	



fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y );


fixed_t R_ScaleFromGlobalAngle (angle_t visangle);

void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box );


void R_SetViewSize (int blocks, int detail);
void R_DrawPlayerSprites(ddplayer_t *viewplr);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.2  2003/02/27 23:14:31  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:18:42  skyjake
// Initial checkin
//
//
//-----------------------------------------------------------------------------
