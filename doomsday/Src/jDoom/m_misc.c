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
//
// $Log$
// Revision 1.7  2005/01/01 22:58:52  skyjake
// Resolved a bunch of compiler warnings
//
// Revision 1.6  2004/05/29 09:53:29  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.5  2004/05/28 19:52:58  skyjake
// Finished switch from branch-1-7 to trunk, hopefully everything is fine
//
// Revision 1.2.2.2  2004/05/16 10:01:36  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.2.1.2.1  2003/11/19 17:07:12  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2.2.1  2003/09/21 15:35:21  skyjake
// Fixed screenshot file name selection
// Moved G_DoScreenShot() to Src/Common
//
// Revision 1.2  2003/02/27 23:14:32  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:21:50  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:46  Jaakko
// Added Doomsday sources
//
//-----------------------------------------------------------------------------

#include "doomdef.h"

void strcatQuoted(char *dest, char *src)
{
	int     k = strlen(dest) + 1, i;

	strcat(dest, "\"");
	for(i = 0; src[i]; i++)
	{
		if(src[i] == '"')
		{
			strcat(dest, "\\\"");
			k += 2;
		}
		else
		{
			dest[k++] = src[i];
			dest[k] = 0;
		}
	}
	strcat(dest, "\"");
}
