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
//-----------------------------------------------------------------------------

// We are referring to sprite numbers.
#include "info.h"

#ifdef __GNUG__
#pragma implementation "d_items.h"
#endif
#include "d_items.h"

#include "p_local.h"

//
// PSPRITE ACTIONS for waepons.
// This struct controls the weapon animations.
//
// Each entry is:
//   ammo/amunition type
//  upstate
//  downstate
// readystate
// atkstate, i.e. attack/fire/hit frame
// flashstate, muzzle flash
//

//
// These are used if other definitions are not found.
//

weaponinfo_t	weaponinfo[NUMWEAPONS] =
{
    {
	// fist
	am_noammo,
	0,
	S_PUNCHUP,
	S_PUNCHDOWN,
	S_PUNCH,
	S_PUNCH1,
	S_NULL
    },	
    {
	// pistol
	am_clip,
	1,
	S_PISTOLUP,
	S_PISTOLDOWN,
	S_PISTOL,
	S_PISTOL1,
	S_PISTOLFLASH
    },	
    {
	// shotgun
	am_shell,
	1,
	S_SGUNUP,
	S_SGUNDOWN,
	S_SGUN,
	S_SGUN1,
	S_SGUNFLASH1
    },
    {
	// chaingun
	am_clip,
	1,
	S_CHAINUP,
	S_CHAINDOWN,
	S_CHAIN,
	S_CHAIN1,
	S_CHAINFLASH1
    },
    {
	// missile launcher
	am_misl,
	1,
	S_MISSILEUP,
	S_MISSILEDOWN,
	S_MISSILE,
	S_MISSILE1,
	S_MISSILEFLASH1
    },
    {
	// plasma rifle
	am_cell,
	1,
	S_PLASMAUP,
	S_PLASMADOWN,
	S_PLASMA,
	S_PLASMA1,
	S_PLASMAFLASH1
    },
    {
	// bfg 9000
	am_cell,
	40,
	S_BFGUP,
	S_BFGDOWN,
	S_BFG,
	S_BFG1,
	S_BFGFLASH1
    },
    {
	// chainsaw
	am_noammo,
	0,
	S_SAWUP,
	S_SAWDOWN,
	S_SAW,
	S_SAW1,
	S_NULL
    },
    {
	// super shotgun
	am_shell,
	2,
	S_DSGUNUP,
	S_DSGUNDOWN,
	S_DSGUN,
	S_DSGUN1,
	S_DSGUNFLASH1
    },	
};

//===========================================================================
// GetDefInt
//===========================================================================
int GetDefInt(char *def, int *returned_value)
{
	char *data;
	int val;

	// Get the value.
	if(!Def_Get(DD_DEF_VALUE, def, &data)) return 0; // No such value...
	// Convert to integer.
	val = strtol(data, 0, 0);
	if(returned_value) *returned_value = val;
	return val;
}

void GetDefState(char *def, int *val)
{
	char *data;

	// Get the value.
	if(!Def_Get(DD_DEF_VALUE, def, &data)) return;
	// Get the state number.
	*val = Def_Get(DD_DEF_STATE, data, 0);
	if(*val < 0) *val = 0;
}

// Initialize weapon info, maxammo and clipammo.
void P_InitWeaponInfo()
{
#define PLMAX "Player|Max ammo|"
#define PLCLP "Player|Clip ammo|"
#define WPINF "Weapon Info|"

	int i, k;
	char buf[80];
	char *data;
	char *ammotypes[] = { "clip", "shell", "cell", 
		"misl", "-", "noammo", 0 };

	// Max ammo.
	GetDefInt(PLMAX"Clip", &maxammo[am_clip]);
	GetDefInt(PLMAX"Shell", &maxammo[am_shell]);
	GetDefInt(PLMAX"Cell", &maxammo[am_cell]);
	GetDefInt(PLMAX"Misl", &maxammo[am_misl]);

	// Clip ammo.
	GetDefInt(PLCLP"Clip", &clipammo[am_clip]);
	GetDefInt(PLCLP"Shell", &clipammo[am_shell]);
	GetDefInt(PLCLP"Cell", &clipammo[am_cell]);
	GetDefInt(PLCLP"Misl", &clipammo[am_misl]);

	for(i=0; i<NUMWEAPONS; i++)
	{
		sprintf(buf, WPINF"%i|Type", i);
		if(Def_Get(DD_DEF_VALUE, buf, &data))
		{
			// Set the right type of ammo.
			for(k=0; ammotypes[k]; k++)
				if(!stricmp(data, ammotypes[k]))
				{
					weaponinfo[i].ammo = k;
					break;
				}
		}
		sprintf(buf, WPINF"%i|Per shot", i);
		GetDefInt(buf, &weaponinfo[i].pershot);
		sprintf(buf, WPINF"%i|Up", i);
		GetDefState(buf, &weaponinfo[i].upstate);
		sprintf(buf, WPINF"%i|Down", i);
		GetDefState(buf, &weaponinfo[i].downstate);
		sprintf(buf, WPINF"%i|Ready", i);
		GetDefState(buf, &weaponinfo[i].readystate);
		sprintf(buf, WPINF"%i|Atk", i);
		GetDefState(buf, &weaponinfo[i].atkstate);
		sprintf(buf, WPINF"%i|Flash", i);
		GetDefState(buf, &weaponinfo[i].flashstate);
		sprintf(buf, WPINF"%i|Static", i);
		weaponinfo[i].static_switch = GetDefInt(buf, 0);
	}
}

void P_InitPlayerValues(player_t *p)
{
#define PLINA "Player|Init ammo|"
	int i;
	char buf[20];

	GetDefInt("Player|Health", &p->health);
	GetDefInt("Player|Weapon", (int*) &p->readyweapon);
	p->pendingweapon = p->readyweapon;
	for(i = 0; i < NUMWEAPONS; i++)
	{
		sprintf(buf, "Weapon Info|%i|Owned", i);
		GetDefInt(buf, (int*) &p->weaponowned[i]);
	}
	GetDefInt(PLINA"Clip", &p->ammo[am_clip]);
	GetDefInt(PLINA"Shell", &p->ammo[am_shell]);
	GetDefInt(PLINA"Cell", &p->ammo[am_cell]);
	GetDefInt(PLINA"Misl", &p->ammo[am_misl]);
}

