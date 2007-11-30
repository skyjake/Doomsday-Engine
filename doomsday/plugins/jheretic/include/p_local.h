/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * p_local.h:
 */

#ifndef __P_LOCAL__
#define __P_LOCAL__

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

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS    1
#define STARTBONUSPALS  9
#define NUMREDPALS      8
#define NUMBONUSPALS    4

#define FLOATSPEED      4

#define DELTAMUL        6.324555320 // Used when calculating ticcmd_t.lookdirdelta

#define MAXHEALTH       100
#define MAXCHICKENHEALTH 30
#define VIEWHEIGHT      41

#define TOCENTER        -8

// player radius for movement checking
#define PLAYERRADIUS    16

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32
#define MAXMOVE         30

#define USERANGE        64
#define MELEERANGE      64
#define MISSILERANGE    (32*64)

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD   100


// GMJ 02/02/02
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seestate)

#define FOOTCLIPSIZEF    (10)

// ***** P_TICK *****

//extern thinker_t thinkercap; // both the head and tail of the thinker list

#define thinkercap      (*gi.thinkercap)

extern int      TimerGame;         // tic countdown for deathmatch

//void P_InitThinkers(void);
//void P_AddThinker(thinker_t *thinker);
//void P_RemoveThinker(thinker_t *thinker);

/*#define P_InitThinkers        gi.InitThinkers
   #define P_AddThinker     gi.AddThinker
   #define P_RemoveThinker      gi.RemoveThinker */

// ***** P_PSPR *****

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

void            P_AddMaceSpot(spawnspot_t *mthing);
void            P_RepositionMace(mobj_t *mo);
void            P_SetPsprite(player_t *player, int position, statenum_t stnum);
void            P_SetupPsprites(player_t *curplayer);
void            P_MovePsprites(player_t *curplayer);
void            P_DropWeapon(player_t *player);
void            P_ActivateMorphWeapon(player_t *player);
void            P_PostMorphWeapon(player_t *player, weapontype_t weapon);
void            P_UpdateBeak(player_t *player, pspdef_t *psp);
void            P_FireWeapon(player_t *player);

// ***** P_USER *****

void            P_ClientSideThink(void);
void            P_Thrust(player_t *player, angle_t angle, float move);
boolean         P_UndoPlayerMorph(player_t *player);

// ***** P_MOBJ *****

// Any floor type >= FLOOR_LIQUID will floorclip sprites
enum {
    FLOOR_SOLID,
    FLOOR_LIQUID,
    FLOOR_WATER,
    FLOOR_LAVA,
    FLOOR_SLUDGE
};

#define FRICTION_NORMAL     (0.90625f)
#define FRICTION_FLY        (0.91796875f)
#define FRICTION_HIGH       (0.5f)
#define FRICTION_LOW        (0.97265625f)

#define ONFLOORZ            DDMININT
#define ONCEILINGZ          DDMAXINT
#define FLOATRANDZ          (DDMAXINT-1)

// Time interval for item respawning.
#define ITEMQUESIZE         128

extern int      iquehead;
extern int      iquetail;

extern mobjtype_t PuffType;
extern mobj_t  *MissileMobj;

mobj_t     *P_SpawnMobj3f(mobjtype_t type, float x, float y, float z);
mobj_t     *P_SpawnMobj3fv(mobjtype_t type, float pos[3]);

void        P_SpawnPuff(float x, float y, float z);
void        P_SpawnBlood(float x, float y, float z, int damage);
mobj_t     *P_SpawnMissile(mobjtype_t type, mobj_t *source, mobj_t *dest);
mobj_t     *P_SpawnMissileAngle(mobjtype_t type, mobj_t *source,
                                angle_t angle, float momz);
mobj_t     *P_SpawnTeleFog(float x, float y);

void        P_RemoveMobj(mobj_t *th);
boolean     P_SetMobjState(mobj_t *mobj, statenum_t state);
boolean     P_SetMobjStateNF(mobj_t *mobj, statenum_t state);
void        P_ThrustMobj(mobj_t *mo, angle_t angle, float move);
void        P_WindThrust(mobj_t *mo);
int         P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta);
boolean     P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnMax);
void        P_MobjThinker(mobj_t *mobj);
void        P_BlasterMobjThinker(mobj_t *mobj);
void        P_SpawnBloodSplatter(float x, float y, float z, mobj_t *originator);
void        P_RipperBlood(mobj_t *mo);
int         P_GetMobjFloorType(mobj_t *thing);
int         P_HitFloor(mobj_t *thing);
boolean     P_CheckMissileSpawn(mobj_t *missile);
void        P_ZMovement(mobj_t *mo);
void        P_ExplodeMissile(mobj_t *mo);

void        P_SpawnMapThing(spawnspot_t *th);
void        P_SpawnPlayer(spawnspot_t *mthing, int plrnum);

// ***** P_ENEMY *****

void        P_NoiseAlert(mobj_t *target, mobj_t *emmiter);
void        P_AddBossSpot(float x, float y, angle_t angle);
void        P_Massacre(void);
void        P_DSparilTeleport(mobj_t *actor);

// ***** P_MAPUTL *****

#define openrange           (*(float*) DD_GetVariable(DD_OPENRANGE))
#define opentop             (*(float*) DD_GetVariable(DD_OPENTOP))
#define openbottom          (*(float*) DD_GetVariable(DD_OPENBOTTOM))
#define lowfloor            (*(float*) DD_GetVariable(DD_LOWFLOOR))

// ***** P_SETUP *****

/*extern byte *rejectmatrix;                // for fast sight rejection
   extern short *blockmaplump;              // offsets in blockmap are from here
   extern short *blockmap;
   extern int bmapwidth, bmapheight;        // in mapblocks
   extern fixed_t bmaporgx, bmaporgy;       // origin of block map
   extern mobj_t **blocklinks;              // for thing chains */

#define bmapwidth   (*gi.bmapwidth)
#define bmapheight  (*gi.bmapheight)
#define bmaporgx    (*gi.bmaporgx)
#define bmaporgy    (*gi.bmaporgy)

char           *P_GetLevelName(int episode, int map);
char           *P_GetShortLevelName(int episode, int map);

// ***** P_INTER *****

extern int      maxammo[NUM_AMMO_TYPES];
extern int      clipammo[NUM_AMMO_TYPES];

void            P_GiveKey(player_t *player, keytype_t key);
void            P_TouchSpecialMobj(mobj_t *special, mobj_t *toucher);
void            P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                             int damage);
void            P_DamageMobj2(mobj_t *target, mobj_t *inflictor,
                              mobj_t *source, int damage, boolean stomping);
boolean         P_GiveAmmo(player_t *player, ammotype_t ammo, int count);
boolean         P_GiveBody(player_t *player, int num);
boolean         P_GivePower(player_t *player, powertype_t power);
boolean         P_MorphPlayer(player_t *player);

// mn_menu, sb_bar
void            Draw_BeginZoom(float s, float originX, float originY);
void            Draw_EndZoom(void);

// ***** ST_STUFF *****

void            ST_Inventory(boolean show);
boolean         ST_IsInventoryVisible(void);

void            ST_InventoryFlashCurrent(player_t *player);

void            ST_doPaletteStuff(void);

#define LOOKDIR2DEG(x) ((x) * 85.0/110.0)
#define LOOKDIR2RAD(x) (LOOKDIR2DEG(x)/180*PI)

void            R_SetFilter(int filter);
int             R_GetFilterColor(int filter);

#endif                          // __P_LOCAL__
