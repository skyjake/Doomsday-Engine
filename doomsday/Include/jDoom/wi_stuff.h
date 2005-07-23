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
//  Intermission.
//
//-----------------------------------------------------------------------------

#ifndef __WI_STUFF__
#define __WI_STUFF__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "doomdef.h"
#include "r_defs.h"

// States for the intermission

typedef enum {
	NoState = -1,
	StatCount,
	ShowNextLoc
} stateenum_t;

// Called by main loop, animate the intermission.
void            WI_Ticker(void);

// Called by main loop,
// draws the intermission directly into the screen buffer.
void            WI_Drawer(void);

// Setup for an intermission screen.
void            WI_Start(wbstartstruct_t * wbstartstruct);

void            WI_SetState(stateenum_t st);
void            WI_End(void);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.8  2005/07/23 08:47:08  skyjake
// Merged 1.9.0-beta1 and beta2 into HEAD
//
// Revision 1.7.2.1  2005/06/15 18:22:42  skyjake
// Numerous fixes after compiling with gcc-4.0 on Mac
//
// Revision 1.7  2005/05/29 05:45:09  danij
// Moved text drawing routines to the commonised hu_stuff.c
//
// Revision 1.6  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.5  2004/05/28 17:16:36  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.3.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.3.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.3  2003/08/24 00:26:17  skyjake
// More advanced patch replacements
//
// Revision 1.2  2003/08/17 23:29:36  skyjake
// Implemented Patch Replacement
//
// Revision 1.1  2003/02/26 19:18:52  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:14  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
