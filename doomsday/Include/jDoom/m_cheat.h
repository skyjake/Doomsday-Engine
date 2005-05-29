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
//  Cheat code checking.
//
//-----------------------------------------------------------------------------

#ifndef __M_CHEAT__
#define __M_CHEAT__

#include "doomstat.h"

//
// CHEAT SEQUENCE PACKAGE
//

#define SCRAMBLE(a) \
((((a)&1)<<7) + (((a)&2)<<5) + ((a)&4) + (((a)&8)<<1) \
 + (((a)&16)>>1) + ((a)&32) + (((a)&64)>>5) + (((a)&128)>>7))

typedef struct {
	unsigned char  *sequence;
	unsigned char  *p;

} cheatseq_t;

int             cht_CheckCheat(cheatseq_t * cht, char key);

void            cht_GetParam(cheatseq_t * cht, char *buffer);

void            cht_GodFunc(player_t *plyr);
void            cht_GiveFunc(player_t *plyr, boolean weapons, boolean ammo,
							 boolean armor, boolean cards);
void            cht_MusicFunc(player_t *plyr, char *buf);
void            cht_NoClipFunc(player_t *plyr);
boolean         cht_WarpFunc(player_t *plyr, char *buf);
void            cht_PowerUpFunc(player_t *plyr, int i);
void            cht_ChoppersFunc(player_t *plyr);
void            cht_PosFunc(player_t *plyr);

boolean 	cht_Responder(event_t *ev);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.6  2005/05/29 05:47:13  danij
// Added cht_Responder() for responding to cheats. Moved all cheat related code to m_cheat.c
//
// Revision 1.5  2004/06/16 18:28:46  skyjake
// Updated style (typenames)
//
// Revision 1.4  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.3  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.1.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.1.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:18:31  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
