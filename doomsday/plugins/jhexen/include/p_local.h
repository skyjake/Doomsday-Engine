/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 1999-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
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

#define MAXHEALTH           100
#define MAXMORPHHEALTH      30
#define VIEWHEIGHT          48

#define FLOATBOBRES         64
#define FLOATBOBOFFSET(n)   (FloatBobOffset[(n) < 0? 0 : (n) > FLOATBOBRES - 1? FLOATBOBRES - 1 : (n)])

// Player radius for movement checking.
#define PLAYERRADIUS        16

// MAXRADIUS is for precalculated sector bounding boxes.
#define MAXRADIUS           32
#define MAXMOVE             30

#define USERANGE            64
#define MELEERANGE          64
#define MISSILERANGE        (32*64)

#define LOOKDIR2DEG(x)      ((x) * 85.0/110.0)
#define LOOKDIR2RAD(x)      (LOOKDIR2DEG(x)/180*PI)

#define BASETHRESHOLD       100 // Follow a player exlusively for 3 seconds.

extern int TimerGame; // Tic countdown for deathmatch.

#define thinkerCap          (*gi.thinkerCap)

#define USE_MANA1           1
#define USE_MANA2           1

void        P_SetPsprite(player_t *plr, int position, statenum_t stnum);
void        P_SetPspriteNF(player_t *plr, int position, statenum_t stnum);
void        P_SetupPsprites(player_t *plr);
void        P_MovePsprites(player_t *plr);
void        P_DropWeapon(player_t *plr);
void        P_ActivateMorphWeapon(player_t *plr);
void        P_PostMorphWeapon(player_t *plr, weapontype_t weapon);

void        P_TeleportOther(mobj_t *victim);
void        P_ArtiTeleportOther(player_t *plr);
void        ResetBlasted(mobj_t *mo);
boolean     P_UndoPlayerMorph(player_t *plr);

#define FRICTION_NORMAL     (0.90625f)
#define FRICTION_FLY        (0.91796875f)
#define FRICTION_HIGH       (0.5f)
#define FRICTION_LOW        (0.97265625f)

#define ONFLOORZ            DDMINFLOAT
#define ONCEILINGZ          DDMAXFLOAT
#define FLOATRANDZ          (DDMAXFLOAT-1)
#define FROMCEILINGZ128     (DDMAXFLOAT-2)

// Time interval for item respawning.
#define ITEMQUESIZE         128

extern int iquehead;
extern int iquetail;

extern mobjtype_t PuffType;
extern mobj_t *MissileMobj;
extern float *FloatBobOffset;

mobj_t     *P_SpawnMobj3f(mobjtype_t type, float x, float y, float z);
mobj_t     *P_SpawnMobj3fv(mobjtype_t type, float pos[3]);

void        P_SpawnPuff(float x, float y, float z);
void        P_SpawnBlood(float x, float y, float z, int damage);
mobj_t     *P_SpawnMissile(mobjtype_t type, mobj_t *source, mobj_t *dest);
mobj_t     *P_SpawnMissileXYZ(mobjtype_t type, float x, float y, float z,
                               mobj_t *source, mobj_t *dest);
mobj_t     *P_SpawnMissileAngle(mobjtype_t type, mobj_t *source,
                                 angle_t angle, float momZ);
mobj_t     *P_SpawnMissileAngleSpeed(mobjtype_t type, mobj_t *source,
                                     angle_t angle, float momZ, float speed);
mobj_t     *P_SpawnPlayerMissile(mobjtype_t type, mobj_t *source);
mobj_t     *P_SPMAngle(mobjtype_t type, mobj_t *source, angle_t angle);
mobj_t     *P_SPMAngleXYZ(mobjtype_t type, float x, float y, float z,
                            mobj_t *source, angle_t angle);
mobj_t     *P_SpawnTeleFog(float x, float y);
mobj_t     *P_SpawnKoraxMissile(mobjtype_t type, float x, float y, float z,
                                 mobj_t *source, mobj_t *dest);
void        P_SpawnDirt(mobj_t *actor, float radius);

void        P_Thrust(player_t *plr, angle_t angle, float move);
void        P_ThrustMobj(mobj_t *mo, angle_t angle, float move);
int         P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta);
boolean     P_SeekerMissile(mobj_t *mo, angle_t thresh, angle_t turnMax);
void        P_MobjThinker(mobj_t *mo);
void        P_BlasterMobjThinker(mobj_t *mo);
boolean     P_HealRadius(player_t *plr);
void        P_BlastRadius(player_t *plr);
void        P_SpawnBloodSplatter(float x, float y, float z, mobj_t *origin);
void        P_SpawnBloodSplatter2(float x, float y, float z, mobj_t *origin);

void        P_CreateTIDList(void);

boolean     P_MobjChangeState(mobj_t *mo, statenum_t state);
boolean     P_SetMobjStateNF(mobj_t *mo, statenum_t state);

void        P_MobjRemoveFromTIDList(mobj_t *mo);
void        P_MobjInsertIntoTIDList(mobj_t *mo, int tid);
mobj_t     *P_FindMobjFromTID(int tid, int *searchPosition);

boolean     P_CheckMissileSpawn(mobj_t *mo);
float       P_MobjGetFriction(mobj_t *mo);
void        P_RipperBlood(mobj_t *mo);
terraintype_t P_MobjGetFloorTerrainType(mobj_t* mo);
int         P_HitFloor(mobj_t *mo);

// Spawn spots.
void        P_SpawnMapThing(spawnspot_t *spot);
void        P_SpawnPlayer(spawnspot_t *spot, int playernum);

void        P_NoiseAlert(mobj_t *target, mobj_t *emmiter);
int         P_Massacre(void);
boolean     P_LookForMonsters(mobj_t *mo);
void        P_InitCreatureCorpseQueue(boolean corpseScan);

#define MAXINTERCEPTS       128
extern intercept_t intercepts[MAXINTERCEPTS], *intercept_p;

mobj_t     *P_RoughMonsterSearch(mobj_t *mo, int distance);

void        P_Validate();

#define OPENRANGE           (*(float*) DD_GetVariable(DD_OPENRANGE))
#define OPENTOP             (*(float*) DD_GetVariable(DD_OPENTOP))
#define OPENBOTTOM          (*(float*) DD_GetVariable(DD_OPENBOTTOM))
#define LOWFLOOR            (*(float*) DD_GetVariable(DD_LOWFLOOR))

extern int clipmana[NUM_AMMO_TYPES];

void        HUMsg_ClearMessages(player_t *plr);
void        P_TouchSpecialMobj(mobj_t *special, mobj_t *toucher);
void        P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                         int damage);
void        P_DamageMobj2(mobj_t *target, mobj_t *inflictor,
                          mobj_t *source, int damage, boolean stomping);
void        P_FallingDamage(player_t *plr);
void        P_PoisonPlayer(player_t *plr, mobj_t *poisoner, int poison);
void        P_PoisonDamage(player_t *plr, mobj_t *source, int damage,
                           boolean playPainSound);
boolean     P_GiveMana(player_t *plr, ammotype_t mana, int count);
boolean     P_GiveArmor(player_t *plr, armortype_t armortype, int amount);
boolean     P_GiveBody(player_t *plr, int num);
boolean     P_GivePower(player_t *plr, powertype_t power);
boolean     P_MorphPlayer(player_t *plr);

boolean     A_LocalQuake(byte *args, mobj_t *victim);
void C_DECL A_BridgeRemove(mobj_t *actor);
boolean     A_RaiseMobj(mobj_t *mo);
boolean     A_SinkMobj(mobj_t *mo);
void C_DECL A_NoBlocking(mobj_t *mo);
void C_DECL A_DeQueueCorpse(mobj_t *mo);

#endif
