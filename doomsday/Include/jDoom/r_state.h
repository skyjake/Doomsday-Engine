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
//	Refresh/render internal state variables (global).
//
//-----------------------------------------------------------------------------


#ifndef __R_STATE__
#define __R_STATE__

// Need data structure definitions.
#include "d_player.h"
#include "r_data.h"



#ifdef __GNUG__
#pragma interface
#endif



//
// Refresh internal data structures,
//  for rendering.
//

// needed for texture pegging
//extern fixed_t*		textureheight;

// needed for pre rendering (fracs)
extern fixed_t*		spritewidth;

extern fixed_t*		spriteoffset;
extern fixed_t*		spritetopoffset;


#define numvertexes		(*gi.numvertexes)
#define numsegs			(*gi.numsegs)
#define	numsectors		(*gi.numsectors)
#define numsubsectors	(*gi.numsubsectors)
#define numnodes		(*gi.numnodes)
#define numlines		(*gi.numlines)
#define numsides		(*gi.numsides)

#define vertexes		((vertex_t*)(*gi.vertexes))
#define segs			((seg_t*)(*gi.segs))
#define	sectors			((sector_t*)(*gi.sectors))
#define subsectors		((subsector_t*)(*gi.subsectors))
#define nodes			((node_t*)(*gi.nodes))
#define lines			((line_t*)(*gi.lines))
#define sides			((side_t*)(*gi.sides))

/*


//
// POV data.
//
extern fixed_t		viewx;
extern fixed_t		viewy;
extern fixed_t		viewz;

extern angle_t		viewangle;*/
extern player_t*	viewplayer;

#define viewx		gi.Get(DD_VIEWX)
#define viewy		gi.Get(DD_VIEWY)
//#define viewz		gi.Get(DD_VIEWZ)

#define viewangle	gi.Get(DD_VIEWANGLE)

// ?
extern angle_t		clipangle;

extern int		viewangletox[FINEANGLES/2];
extern angle_t		xtoviewangle[SCREENWIDTH+1];
//extern fixed_t		finetangent[FINEANGLES/2];

//extern fixed_t		rw_distance;
//extern angle_t		rw_normalangle;



// angle to line origin
extern int		rw_angle1;

// Segs count?
extern int		sscount;

/*extern visplane_t*	floorplane;
extern visplane_t*	ceilingplane;
*/

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.2  2004/01/07 20:44:40  skyjake
// Merged from branch-nix
//
// Revision 1.1.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:18:44  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------

