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
//
//
//-----------------------------------------------------------------------------


#ifndef __P_INTER__
#define __P_INTER__


#ifdef __GNUG__
#pragma interface
#endif


boolean	P_GivePower(player_t*, int);
void P_GiveCard(player_t* player, card_t card);
boolean
P_GiveBody
( player_t*	player,
 int		num );
void P_GiveBackpack(player_t *player);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.3  2004/01/07 20:44:40  skyjake
// Merged from branch-nix
//
// Revision 1.2.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/08/24 00:25:40  skyjake
// Separate function for giving backpack
//
// Revision 1.1  2003/02/26 19:18:32  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------

