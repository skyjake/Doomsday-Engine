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
// Revision 1.4  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.2.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/07/12 22:10:05  skyjake
// Removed obsolete stuff
//
// Revision 1.1  2003/02/26 19:18:26  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:12  Jaakko
// Added all headers
//
//
// DESCRIPTION:
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __D_MAIN__
#define __D_MAIN__

#include "d_event.h"

#ifdef __GNUG__
#pragma interface
#endif



#define MAXWADFILES             20
extern char*		wadfiles[MAXWADFILES];

void D_AddFile (char *file);



//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
// If not overrided by user input, calls N_AdvanceDemo.
//
void D_DoomMain (void);

// Called by IO functions when input is detected.
void D_PostEvent (event_t* ev);

#endif

