/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * Play functions, animation, global header.
 */

#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include "p_spec.h"
#include "p_start.h"
#include "p_actor.h"
#include "p_xg.h"

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS        1
#define STARTBONUSPALS      9
#define NUMREDPALS          8
#define NUMBONUSPALS        4

#define FLOATSPEED      (FRACUNIT*4)

#define DELTAMUL        6.324555320 // Used when calculating ticcmd_t.lookdirdelta


#define MAXHEALTH       maxhealth  //100
#define VIEWHEIGHT      (41*FRACUNIT)

#define TOCENTER        -8

// player radius for movement checking
#define PLAYERRADIUS    16*FRACUNIT

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32*FRACUNIT

#define GRAVITY     ((IS_NETGAME && cfg.netGravity != -1)? \
                     (fixed_t) (((float) cfg.netGravity / 100) * FRACUNIT) : Get(DD_GRAVITY)) //FRACUNIT
#define MAXMOVE     (30*FRACUNIT)

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD       100


// GMJ 02/02/02
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seestate)

//
// P_TICK
//

#define thinkercap      (*gi.thinkercap)

extern int      TimerGame;         // tic countdown for deathmatch

//
// P_PSPR
//
void            P_SetupPsprites(player_t *curplayer);
void            P_MovePsprites(player_t *curplayer);
void            P_DropWeapon(player_t *player);
void            P_SetPsprite(player_t *player, int position, statenum_t stnum);

//
// P_USER
//
extern int      maxhealth, healthlimit, godmodehealth;
extern int      soulspherehealth, soulspherelimit, megaspherehealth;
extern int      armorpoints[4];    // Green, blue, IDFA and IDKFA points.
extern int      armorclass[4];     // Green and blue classes.

//
// P_MOBJ
//
// Any floor type >= FLOOR_LIQUID will floorclip sprites
enum {
    FLOOR_SOLID,
    FLOOR_LIQUID,
    FLOOR_WATER,
    FLOOR_LAVA,
    FLOOR_SLUDGE,
    FLOOR_BLOOD,
    FLOOR_SLIME,
    NUM_TERRAINTYPES
};

#define ONFLOORZ        DDMININT
#define ONCEILINGZ      DDMAXINT
#define FLOATRANDZ      (DDMAXINT-1)

// Time interval for item respawning.
#define ITEMQUESIZE     128

extern int      iquehead;
extern int      iquetail;

void            P_SpawnMapThing(thing_t * th);
mobj_t         *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);

void            P_RemoveMobj(mobj_t *th);
boolean         P_SetMobjState(mobj_t *mobj, statenum_t state);
void            P_MobjThinker(mobj_t *mobj);
int             P_GetThingFloorType(mobj_t *thing);
void            P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z);
mobj_t         *P_SpawnCustomPuff(fixed_t x, fixed_t y, fixed_t z,
                                  mobjtype_t type);
void            P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
void            P_RipperBlood(mobj_t *mo);
mobj_t         *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type);
void            P_SpawnPlayer(thing_t * mthing, int pnum);
mobj_t         *P_SpawnTeleFog(int x, int y);

void            P_SetDoomsdayFlags(mobj_t *mo);

void            P_HitFloor(mobj_t *mo);

//
// P_ENEMY
//
void            P_NoiseAlert(mobj_t *target, mobj_t *emmiter);

/* killough 3/26/98: spawn icon landings */
void P_SpawnBrainTargets(void);

/* killough 3/26/98: global state of boss brain */
extern struct brain_s {
    int easy;
    int targeton;
} brain;

/* Removed fixed limit on number of brain targets */
extern mobj_t  **braintargets;
extern int      numbraintargets;
extern int      numbraintargets_alloc;

//
// P_MAPUTL
//

#define openrange           (*(float*) DD_GetVariable(DD_OPENRANGE))
#define opentop             (*(float*) DD_GetVariable(DD_OPENTOP))
#define openbottom          (*(float*) DD_GetVariable(DD_OPENBOTTOM))
#define lowfloor            (*(float*) DD_GetVariable(DD_LOWFLOOR))

void            P_UnsetThingPosition(mobj_t *thing);
void            P_SetThingPosition(mobj_t *thing);

int             P_Massacre(void);

//
// P_INTER
//
extern int      maxammo[NUM_AMMO_TYPES];
extern int      clipammo[NUM_AMMO_TYPES];

void            P_TouchSpecialThing(mobj_t *special, mobj_t *toucher);

void            P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                             int damage);
void            P_DamageMobj2(mobj_t *target, mobj_t *inflictor,
                              mobj_t *source, int damage, boolean stomping);

void            P_ExplodeMissile(mobj_t *mo);

#endif
