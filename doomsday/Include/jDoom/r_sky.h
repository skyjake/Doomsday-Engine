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
//	Sky rendering.
//
//-----------------------------------------------------------------------------


#ifndef __R_SKY__
#define __R_SKY__


#ifdef __GNUG__
#pragma interface
#endif

// SKY, store the number for name.
#define SKYFLATNAME  "F_SKY1"

// Called whenever the view size changes.
void R_InitSkyMap (void);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.4  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.2.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.4.2  2004/01/07 13:12:14  skyjake
// Cleanup
//
// Revision 1.2.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/02/27 23:14:31  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:18:43  skyjake
// Initial checkin
//
//
//-----------------------------------------------------------------------------

