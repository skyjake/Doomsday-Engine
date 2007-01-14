/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

// P_local.h

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

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS    1
#define STARTBONUSPALS  9
#define NUMREDPALS      8
#define NUMBONUSPALS    4

#define FLOATSPEED      (FRACUNIT*4)

#define DELTAMUL        6.324555320 // Used when calculating ticcmd_t.lookdirdelta

#define MAXHEALTH       100
#define MAXCHICKENHEALTH 30
#define VIEWHEIGHT      (cfg.eyeHeight*FRACUNIT) // 41*FRACUNIT

#define TOCENTER TICCMD_FALL_DOWN

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
#define BASETHRESHOLD   100


// GMJ 02/02/02
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seestate)

#define FOOTCLIPSIZE    10*FRACUNIT

typedef enum {
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
} dirtype_t;



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

#define LOWERSPEED FRACUNIT*6
#define RAISESPEED FRACUNIT*6
#define WEAPONBOTTOM 128*FRACUNIT
#define WEAPONTOP 32*FRACUNIT
#define FLAME_THROWER_TICS 10*35
#define MAGIC_JUNK 1234
#define MAX_MACE_SPOTS 8

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

void            P_OpenWeapons(void);
void            P_CloseWeapons(void);
void            P_AddMaceSpot(thing_t * mthing);
void            P_RepositionMace(mobj_t *mo);
void            P_SetPsprite(player_t *player, int position, statenum_t stnum);
void            P_SetupPsprites(player_t *curplayer);
void            P_MovePsprites(player_t *curplayer);
void            P_DropWeapon(player_t *player);
void            P_ActivateMorphWeapon(player_t *player);
void            P_PostMorphWeapon(player_t *player, weapontype_t weapon);
void            P_UpdateBeak(player_t *player, pspdef_t * psp);
void            P_FireWeapon(player_t *player);

// ***** P_USER *****

void            P_ClientSideThink(void);
void            P_Thrust(player_t *player, angle_t angle, fixed_t move);
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

#define ONFLOORZ   DDMININT
#define ONCEILINGZ DDMAXINT
#define FLOATRANDZ (DDMAXINT-1)

// Time interval for item respawning.
#define ITEMQUESIZE     128

extern int      iquehead;
extern int      iquetail;

extern mobjtype_t PuffType;
extern mobj_t  *MissileMobj;

void            P_SpawnMapThing(thing_t * th);
mobj_t         *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
void            P_RemoveMobj(mobj_t *th);
boolean         P_SetMobjState(mobj_t *mobj, statenum_t state);
boolean         P_SetMobjStateNF(mobj_t *mobj, statenum_t state);
void            P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move);
void            P_WindThrust(mobj_t *mo);
int             P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta);
boolean         P_SeekerMissile(mobj_t *actor, angle_t thresh,
                                angle_t turnMax);
void            P_MobjThinker(mobj_t *mobj);
void            P_BlasterMobjThinker(mobj_t *mobj);
void            P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z);
void            P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
void            P_BloodSplatter(fixed_t x, fixed_t y, fixed_t z,
                                mobj_t *originator);
void            P_RipperBlood(mobj_t *mo);
int             P_GetThingFloorType(mobj_t *thing);
int             P_HitFloor(mobj_t *thing);
boolean         P_CheckMissileSpawn(mobj_t *missile);
mobj_t         *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type);
mobj_t         *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type,
                                    angle_t angle, fixed_t momz);
void            P_SpawnPlayer(thing_t * mthing, int plrnum);
void            P_ZMovement(mobj_t *mo);
mobj_t         *P_SpawnTeleFog(int x, int y);
void            P_ExplodeMissile(mobj_t *mo);

// ***** P_ENEMY *****

void            P_NoiseAlert(mobj_t *target, mobj_t *emmiter);
void            P_InitMonsters(void);
void            P_AddBossSpot(fixed_t x, fixed_t y, angle_t angle);
void            P_Massacre(void);
void            P_DSparilTeleport(mobj_t *actor);

// ***** P_MAPUTL *****

#define openrange           Get(DD_OPENRANGE)
#define opentop             Get(DD_OPENTOP)
#define openbottom          Get(DD_OPENBOTTOM)
#define lowfloor            Get(DD_LOWFLOOR)

void            P_UnsetThingPosition(mobj_t *thing);
void            P_SetThingPosition(mobj_t *thing);

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
void            P_TouchSpecialThing(mobj_t *special, mobj_t *toucher);
void            P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                             int damage);
void            P_DamageMobj2(mobj_t *target, mobj_t *inflictor,
                              mobj_t *source, int damage, boolean stomping);
boolean         P_GiveAmmo(player_t *player, ammotype_t ammo, int count);
boolean         P_GiveBody(player_t *player, int num);
boolean         P_GivePower(player_t *player, powertype_t power);
boolean         P_MorphPlayer(player_t *player);

// ***** AM_MAP *****

void            AM_Ticker(void);
void            AM_Drawer(void);

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
