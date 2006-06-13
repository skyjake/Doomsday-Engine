/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Items: key cards, artifacts, weapon, ammunition.
 */

#ifndef __H_ITEMS__
#define __H_ITEMS__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"

#ifdef __GNUG__
#pragma interface
#endif

#define WEAPON_INFO(weaponnum, pclass, fmode) (&weaponinfo[weaponnum][pclass].mode[fmode])

typedef struct {
    int             gamemodebits;   // Game modes, weapon is available in.
    int             ammotype[NUMAMMO];  // required ammo types.
    int             pershot[NUMAMMO];   // Ammo used per shot of each type.
    boolean         autofire;           // (True)= fire when raised if fire held.
    int             upstate;
    int             raisesound;         // Sound played when weapon is raised.
    int             downstate;
    int             readystate;
    int             readysound;         // Sound played WHILE weapon is readyied.
    int             atkstate;
    int             holdatkstate;
    int             flashstate;
    int             static_switch;  // Weapon is not lowered during switch.
} weaponmodeinfo_t;

// Weapon info: sprite frames, ammunition use.
typedef struct {
    weaponmodeinfo_t mode[NUMWEAPLEVELS];
} weaponinfo_t;

extern weaponinfo_t weaponinfo[NUMWEAPONS][NUMCLASSES];

void            P_InitWeaponInfo(void);

#define NUMINVENTORYSLOTS   14
typedef struct {
    int             type;
    int             count;
} inventory_t;

#endif
