/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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

/**
 * p_local.h:
 */

#ifndef __P_LOCAL_H__
#define __P_LOCAL_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include "p_spec.h"
#include "p_start.h"
#include "p_actor.h"
#include "p_xg.h"
#include "info.h"

// Palette indices, for damage/bonus red-/gold-shifts.
#define STARTREDPALS    1
#define STARTBONUSPALS  9
#define NUMREDPALS      8
#define NUMBONUSPALS    4

#define FLOATSPEED      4

#define MAXHEALTH       (maxHealth) //100
#define MAXCHICKENHEALTH 30
#define VIEWHEIGHT      41

// player radius for movement checking
#define PLAYERRADIUS    16

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32

#define USERANGE        64
#define MELEERANGE      64
#define MISSILERANGE    (32*64)

// Follow a player exlusively for 3 seconds.
#define BASETHRESHOLD   100

#define sentient(mobj) ((mobj)->health > 0 && P_GetState((mobj)->type, SN_SEE))

#define FOOTCLIPSIZEF    (10)

#define thinkerCap      (*gi.thinkerCap)

#define FLAME_THROWER_TICS  10*35
#define MAGIC_JUNK          1234

#define USE_GWND_AMMO_1 1
#define USE_GWND_AMMO_2 1
#define USE_CBOW_AMMO_1 1
#define USE_CBOW_AMMO_2 1
#define USE_BLSR_AMMO_1 1
#define USE_BLSR_AMMO_2 5
#define USE_SKRD_AMMO_1 1
#define USE_SKRD_AMMO_2 5
#define USE_PHRD_AMMO_1 1
#define USE_PHRD_AMMO_2 1
#define USE_MACE_AMMO_1 1
#define USE_MACE_AMMO_2 5

void            P_RepositionMace(mobj_t* mo);
void            P_SetPsprite(player_t* player, int position,
                             statenum_t stnum);
void            P_SetupPsprites(player_t* curplayer);
void            P_MovePsprites(player_t* curplayer);
void            P_DropWeapon(player_t* player);
void            P_ActivateMorphWeapon(player_t* player);
void            P_PostMorphWeapon(player_t* player, weapontype_t weapon);
void            P_UpdateBeak(player_t* player, pspdef_t* psp);
void            P_FireWeapon(player_t* player);

boolean         P_UndoPlayerMorph(player_t* player);

extern mobjtype_t puffType;
extern mobj_t* missileMobj;

boolean     P_MobjChangeState(mobj_t* mo, statenum_t state);
boolean     P_SetMobjStateNF(mobj_t* mo, statenum_t state);
void        P_ThrustMobj(mobj_t* mo, angle_t angle, coord_t move);
void        P_WindThrust(mobj_t* mo);
int         P_FaceMobj(mobj_t* source, mobj_t* target, angle_t* delta);
boolean     P_SeekerMissile(mobj_t* actor, angle_t thresh, angle_t turnMax);
void        P_MobjThinker(void *mo);
void        P_RipperBlood(mobj_t* mo);
boolean     P_HitFloor(mobj_t* thing);
boolean     P_CheckMissileSpawn(mobj_t* missile);
void        P_MobjMoveZ(mobj_t* mo);
void        P_ExplodeMissile(mobj_t* mo);

const char*     P_GetMapName(uint episode, uint map);
const char*     P_GetShortMapName(uint episode, uint map);

extern float turboMul;
extern int maxAmmo[NUM_AMMO_TYPES];
extern int clipAmmo[NUM_AMMO_TYPES];
extern int maxAmmo[NUM_AMMO_TYPES];

void            P_GiveKey(player_t* player, keytype_t key);
void            P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher);
int             P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, boolean stomping);
int             P_DamageMobj2(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, boolean stomping, boolean skipNetworkCheck);
boolean         P_GiveAmmo(player_t* player, ammotype_t ammo, int count);
boolean         P_GiveBody(player_t* player, int num);
boolean         P_GivePower(player_t* player, powertype_t power);
boolean         P_MorphPlayer(player_t* player);

void            Draw_BeginZoom(float s, float originX, float originY);
void            Draw_EndZoom(void);

#define LOOKDIR2DEG(x) ((x) * 85.0/110.0)
#define LOOKDIR2RAD(x) (LOOKDIR2DEG(x)/180*PI)

#endif
