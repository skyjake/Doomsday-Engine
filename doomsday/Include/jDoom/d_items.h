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
//  Items: key cards, artifacts, weapon, ammunition.
//
//-----------------------------------------------------------------------------

#ifndef __D_ITEMS__
#define __D_ITEMS__

#include "doomdef.h"

#ifdef __GNUG__
#pragma interface
#endif

// Weapon info: sprite frames, ammunition use.
typedef struct {
	ammotype_t      ammo;
	int             pershot;	   // Ammo used per shot.
	int             upstate;
	int             downstate;
	int             readystate;
	int             atkstate;
	int             flashstate;
	int             static_switch; // Weapon is not lowered during switch.

} weaponinfo_t;

extern weaponinfo_t weaponinfo[NUMWEAPONS];

void            P_InitWeaponInfo(void);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.5  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.4  2004/05/28 17:16:34  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.2.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/07/01 23:58:37  skyjake
// Static weapon switching (value, weaponinfo)
//
// Revision 1.1  2003/02/26 19:18:26  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:12  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
