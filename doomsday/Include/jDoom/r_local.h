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
//	Refresh (R_*) module, global header.
//	All the rendering/drawing stuff is here.
//
//-----------------------------------------------------------------------------

#ifndef __R_LOCAL__
#define __R_LOCAL__

// Binary Angles, sine/cosine/atan lookups.
#include "tables.h"

// Screen size related parameters.
#include "doomdef.h"

// Include the refresh/render data structs.
#include "r_data.h"

void R_CachePatch(dpatch_t *dp, char *name);

//
// Separate header file for each module.
//
//#include "r_main.h"
//#include "r_bsp.h"
//#include "r_segs.h"
//#include "r_plane.h"
//#include "r_data.h"
//#include "r_things.h"
//#include "r_draw.h"

#endif		// __R_LOCAL__
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.2  2003/02/27 23:14:31  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:18:42  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
