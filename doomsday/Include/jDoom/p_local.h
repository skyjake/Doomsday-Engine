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
//  Play functions, animation, global header.
//
//-----------------------------------------------------------------------------

#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include "p_start.h"
#include "p_actor.h"
#include "p_xg.h"

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS		1
#define STARTBONUSPALS		9
#define NUMREDPALS			8
#define NUMBONUSPALS		4

#define FLOATSPEED		(FRACUNIT*4)

#define DELTAMUL		6.324555320	// Used when calculating ticcmd_t.lookdirdelta

#define bmapwidth		(*gi.bmapwidth)
#define bmapheight		(*gi.bmapheight)
#define bmaporgx		(*gi.bmaporgx)
#define bmaporgy		(*gi.bmaporgy)

#define MAXHEALTH		maxhealth  //100
#define VIEWHEIGHT		(41*FRACUNIT)

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS	128
#define MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT	(FRACBITS+7)
#define MAPBMASK		(MAPBLOCKSIZE-1)
#define MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)

#define TOCENTER		-8

// player radius for movement checking
#define PLAYERRADIUS	16*FRACUNIT

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS		32*FRACUNIT

#define GRAVITY		Get(DD_GRAVITY)	//FRACUNIT
#define MAXMOVE		(30*FRACUNIT)

#define USERANGE		(64*FRACUNIT)
#define MELEERANGE		(64*FRACUNIT)
#define MISSILERANGE	(32*64*FRACUNIT)

// follow a player exlusively for 3 seconds
#define	BASETHRESHOLD	 	100

#define MAXSPECIALCROSS		64

// GMJ 02/02/02
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seestate)

//
// P_TICK
//

// both the head and tail of the thinker list
//extern    thinker_t   thinkercap; 

/*void P_InitThinkers (void);
   void P_AddThinker (thinker_t* thinker);
   void P_RemoveThinker (thinker_t* thinker); */

#define thinkercap		(*gi.thinkercap)

/*#define   P_InitThinkers  gi.InitThinkers
   #define P_AddThinker gi.AddThinker
   #define P_RemoveThinker  gi.RemoveThinker
   #define P_RunThinkers    gi.RunThinkers */

//
// P_PSPR
//
void            P_SetupPsprites(player_t * curplayer);
void            P_MovePsprites(player_t * curplayer);
void            P_DropWeapon(player_t * player);
void            P_SetPsprite(player_t * player, int position,
							 statenum_t stnum);

//
// P_USER
//
void            P_PlayerThink(player_t * player);
void            P_SetMessage(player_t * pl, char *msg);

extern int      maxhealth, healthlimit;
extern int      armorpoints[2];	   // Green and blue points.

//
// P_MOBJ
//
#define ONFLOORZ		MININT
#define ONCEILINGZ		MAXINT

// Time interval for item respawning.
#define ITEMQUESIZE		128

extern mapthing_t itemrespawnque[ITEMQUESIZE];
extern int      itemrespawntime[ITEMQUESIZE];
extern int      iquehead;
extern int      iquetail;

void            P_RespawnSpecials(void);

mobj_t         *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);

void            P_RemoveMobj(mobj_t *th);
boolean         P_SetMobjState(mobj_t *mobj, statenum_t state);
void            P_MobjThinker(mobj_t *mobj);

void            P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z);
mobj_t         *P_SpawnCustomPuff(fixed_t x, fixed_t y, fixed_t z,
								  mobjtype_t type);
void            P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
mobj_t         *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type);
void            P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type);
void            P_SpawnPlayer(mapthing_t * mthing, int pnum);
mobj_t         *P_SpawnTeleFog(int x, int y);

void            P_SetDoomsdayFlags(mobj_t *mo);

//
// P_ENEMY
//
void            P_NoiseAlert(mobj_t *target, mobj_t *emmiter);

extern mobj_t  *braintargets[64];
extern int      numbraintargets;
extern int      braintargeton;

//
// P_MAPUTL
//
/*typedef struct
   {
   fixed_t  x;
   fixed_t  y;
   fixed_t  dx;
   fixed_t  dy;

   } divline_t;

   typedef struct
   {
   fixed_t  frac;       // along trace line
   boolean  isaline;
   union {
   mobj_t*  thing;
   line_t*  line;
   }            d;
   } intercept_t; */

/*#define MAXINTERCEPTS 128

   extern intercept_t   intercepts[MAXINTERCEPTS];
   extern intercept_t*  intercept_p;

   typedef boolean (*traverser_t) (intercept_t *in); */

/*fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
   int  P_PointOnLineSide (fixed_t x, fixed_t y, line_t* line);
   int  P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t* line);
   void     P_MakeDivline (line_t* li, divline_t* dl);
   fixed_t P_InterceptVector (divline_t* v2, divline_t* v1);
   int  P_BoxOnLineSide (fixed_t* tmbox, line_t* ld);

   extern fixed_t       opentop;
   extern fixed_t       openbottom;
   extern fixed_t       openrange;
   extern fixed_t       lowfloor;

   void     P_LineOpening (line_t* linedef);

   boolean P_BlockLinesIterator (int x, int y, boolean(*func)(line_t*) );
   boolean P_BlockThingsIterator (int x, int y, boolean(*func)(mobj_t*) );

   #define PT_ADDLINES      1
   #define PT_ADDTHINGS 2
   #define PT_EARLYOUT      4

   extern divline_t trace;

   boolean
   P_PathTraverse
   ( fixed_t    x1,
   fixed_t  y1,
   fixed_t  x2,
   fixed_t  y2,
   int      flags,
   boolean  (*trav) (intercept_t *));
 */

/*#define P_AproxDistance       gi.ApproxDistance
   #define P_PointOnLineSide    gi.PointOnLineSide
   #define P_BoxOnLineSide      gi.BoxOnLineSide */

#define openrange			Get(DD_OPENRANGE)
#define opentop				Get(DD_OPENTOP)
#define openbottom			Get(DD_OPENBOTTOM)
#define lowfloor			Get(DD_LOWFLOOR)
//#define P_LineOpening     gi.LineOpening

/*#define P_BlockLinesIterator  gi.BlockLinesIterator
   #define P_BlockThingsIterator    gi.BlockThingsIterator

   #define P_PathTraverse       gi.PathTraverse */

void            P_UnsetThingPosition(mobj_t *thing);
void            P_SetThingPosition(mobj_t *thing);

int             P_Massacre(void);

//
// P_MAP
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern boolean  floatok;
extern fixed_t  tmfloorz;
extern fixed_t  tmceilingz;

extern line_t  *ceilingline;

boolean         P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
boolean         P_CheckPosition2(mobj_t *thing, fixed_t x, fixed_t y,
								 fixed_t z);
boolean         P_TryMove(mobj_t *thing, fixed_t x, fixed_t y,
						  boolean dropoff);
boolean         P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y);
void            P_SlideMove(mobj_t *mo);

//boolean P_CheckSight (mobj_t* t1, mobj_t* t2);
void            P_UseLines(player_t * player);

//#define P_CheckSight  gi.CheckSight

boolean         P_ChangeSector(sector_t *sector, boolean crunch);

extern mobj_t  *linetarget;		   // who got hit (or NULL)

fixed_t         P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance);

void            P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
							 fixed_t slope, int damage);

void            P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage);

//
// P_SETUP
//
/*extern byte*      rejectmatrix;   // for fast sight rejection
   extern short*        blockmaplump;   // offsets in blockmap are from here
   extern short*        blockmap;
   extern int       bmapwidth;
   extern int       bmapheight; // in mapblocks
   extern fixed_t       bmaporgx;
   extern fixed_t       bmaporgy;   // origin of block map
   extern mobj_t**      blocklinks; // for thing chains */

//
// P_INTER
//
extern int      maxammo[NUMAMMO];
extern int      clipammo[NUMAMMO];

void            P_TouchSpecialThing(mobj_t *special, mobj_t *toucher);

void            P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
							 int damage);

void            P_ExplodeMissile(mobj_t *mo);

//
// P_SPEC
//
#include "p_spec.h"

#endif							// __P_LOCAL__
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.6  2004/05/30 08:42:35  skyjake
// Tweaked indentation style
//
// Revision 1.5  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.4  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.2.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/04/29 13:10:56  skyjake
// Missile puff ptcgen issue fixed
//
// Revision 1.1  2003/02/26 19:18:32  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
