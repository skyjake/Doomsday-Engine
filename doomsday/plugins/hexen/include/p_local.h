/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_local.h
 */

#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include "p_start.h"
#include "p_actor.h"
#include "p_spec.h"
#include "d_net.h"
#include "p_terraintype.h"

#define STARTREDPALS        1
#define STARTBONUSPALS      9
#define STARTPOISONPALS     13
#define STARTICEPAL         21
#define STARTHOLYPAL        22
#define STARTSCOURGEPAL     25
#define NUMREDPALS          8
#define NUMBONUSPALS        4
#define NUMPOISONPALS       8

#define FLOATSPEED          4

#define MAXHEALTH           (maxHealth) //100
#define MAXMORPHHEALTH      30
#define VIEWHEIGHT          48

#define FLOATBOBRES         64
#define FLOATBOBOFFSET(n)   (FloatBobOffset[MIN_OF(((uint8_t)(n)), FLOATBOBRES - 1)])

// Player radius for movement checking.
#define PLAYERRADIUS        16

// MAXRADIUS is for precalculated sector bounding boxes.
#define MAXRADIUS           32

#define USERANGE            64
#define MELEERANGE          64
#define MISSILERANGE        (32*64)

#define LOOKDIR2DEG(x)      ((x) * 85.0/110.0)
#define LOOKDIR2RAD(x)      (LOOKDIR2DEG(x)/180*PI)

#define BASETHRESHOLD       100 // Follow a player exlusively for 3 seconds.

#define sentient(mobj)      ((mobj)->health > 0 && P_GetState((mobj)->type, SN_SEE))

#define thinkerCap          (*gi.thinkerCap)

#define USE_MANA1           1
#define USE_MANA2           1

// Time interval for item respawning.
#define SPAWNQUEUE_MAX         128

DENG_EXTERN_C int iquehead;
DENG_EXTERN_C int iquetail;

DENG_EXTERN_C mobjtype_t PuffType;
DENG_EXTERN_C mobj_t* MissileMobj;
DENG_EXTERN_C coord_t* FloatBobOffset;
DENG_EXTERN_C int clipmana[NUM_AMMO_TYPES];
DENG_EXTERN_C int TimerGame; // Tic countdown for deathmatch.

#ifdef __cplusplus
extern "C" {
#endif

void        P_SetPsprite(player_t* plr, int position, statenum_t stnum);
void        P_SetPspriteNF(player_t* plr, int position, statenum_t stnum);
void        P_SetupPsprites(player_t* plr);
void        P_MovePsprites(player_t* plr);
void        P_DropWeapon(player_t* plr);
void        P_ActivateMorphWeapon(player_t* plr);
void        P_PostMorphWeapon(player_t* plr, weapontype_t weapon);

void        P_TeleportOther(mobj_t* victim);
void        P_ArtiTeleportOther(player_t* plr);
void        ResetBlasted(mobj_t* mo);
boolean     P_UndoPlayerMorph(player_t* plr);

void        P_ThrustMobj(mobj_t* mo, angle_t angle, coord_t move);
int         P_FaceMobj(mobj_t* source, mobj_t* target, angle_t* delta);
boolean     P_SeekerMissile(mobj_t* mo, angle_t thresh, angle_t turnMax);
void        P_MobjThinker(void *mo);
boolean     P_HealRadius(player_t* plr);
void        P_BlastRadius(player_t* plr);

void        P_CreateTIDList(void);

boolean     P_MobjChangeState(mobj_t* mo, statenum_t state);
boolean     P_SetMobjStateNF(mobj_t* mo, statenum_t state);

void        P_MobjRemoveFromTIDList(mobj_t* mo);
void        P_MobjInsertIntoTIDList(mobj_t* mo, int tid);
mobj_t*     P_FindMobjFromTID(int tid, int* searchPosition);

boolean     P_CheckMissileSpawn(mobj_t* mo);
coord_t     P_MobjGetFriction(mobj_t* mo);
void        P_RipperBlood(mobj_t* mo);
const terraintype_t* P_MobjGetFloorTerrainType(mobj_t* mo);
boolean     P_HitFloor(mobj_t* mo);

void        P_NoiseAlert(mobj_t* target, mobj_t* emmiter);
int         P_Massacre(void);
boolean     P_LookForMonsters(mobj_t* mo);

void        P_InitCorpseQueue(void);
void        P_AddCorpseToQueue(mobj_t* mo);
void        P_RemoveCorpseInQueue(mobj_t* mo);

mobj_t*     P_RoughMonsterSearch(mobj_t* mo, int distance);

void        P_Validate();

void        P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher);
void        P_PoisonPlayer(player_t* plr, mobj_t* poisoner, int poison);

int         P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, boolean stomping);
int         P_DamageMobj2(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, boolean stomping, boolean skipNetworkCheck);
int         P_FallingDamage(player_t* plr);
int         P_PoisonDamage(player_t* plr, mobj_t* source, int damage,
                           boolean playPainSound);

boolean     P_GiveMana(player_t* plr, ammotype_t mana, int count);
boolean     P_GiveArmor(player_t* plr, armortype_t armortype, int amount);
boolean     P_GiveArmor2(player_t* plr, armortype_t type, int amount);
boolean     P_GiveBody(player_t* plr, int num);
boolean     P_GivePower(player_t* plr, powertype_t power);
boolean     P_MorphPlayer(player_t* plr);

boolean     A_LocalQuake(byte* args, mobj_t* victim);
void C_DECL A_BridgeRemove(mobj_t* actor);
boolean     A_RaiseMobj(mobj_t* mo);
boolean     A_SinkMobj(mobj_t* mo);
void C_DECL A_NoBlocking(mobj_t* mo);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
