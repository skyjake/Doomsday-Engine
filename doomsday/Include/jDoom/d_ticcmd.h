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


#ifndef __D_TICCMD__
#define __D_TICCMD__

#include "doomsday.h"

#ifdef __GNUG__
#pragma interface
#endif

// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,
// plus a checksum for internal state consistency.
#pragma pack(1)
typedef struct
{
    char	forwardMove;	// *2048 for move
    char	sideMove;		// *2048 for move
    short	angle;			// <<16 for angle delta
    short	pitch;	        // view pitch
    byte	actions;
} ticcmd_t;

// This'll be used for saveplayer_t, but acts only as padding.
typedef struct
{
    char	forwardmove;	// *2048 for move
    char	sidemove;	// *2048 for move
    short	angleturn;	// <<16 for angle delta
    short	consistancy;	// checks for net game
    byte	chatchar;
    byte	buttons;
} saveticcmd_t;
#pragma pack()


#endif
