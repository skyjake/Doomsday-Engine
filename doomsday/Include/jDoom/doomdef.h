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
//  Internally used data structures for virtually everything,
//   key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDEF__
#define __DOOMDEF__

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#pragma warning(disable:4244 4761)
#define stricmp _stricmp
#define strnicmp _strnicmp
#define strupr _strupr
#define strlwr _strlwr
#endif

#include "../doomsday.h"
#include "../dd_api.h"
#include "g_dgl.h"
#include "version.h"
#include "p_ticcmd.h"

#define Set			DD_SetInteger
#define Get			DD_GetInteger

extern game_import_t gi;
extern game_export_t gx;

//
// Global parameters/defines.
//

#define mobjinfo	(*gi.mobjinfo)
#define states		(*gi.states)
#define validCount	(*gi.validcount)

// Game mode handling - identify IWAD version
//  to handle IWAD dependend animations etc.
typedef enum {
	shareware,					   // DOOM 1 shareware, E1, M9
	registered,					   // DOOM 1 registered, E3, M27
	commercial,					   // DOOM 2 retail, E1 M34
	// DOOM 2 german edition not handled
	retail,						   // DOOM 1 retail, E4, M36
	indetermined				   // Well, no IWAD found.
} GameMode_t;

// Mission packs - might be useful for TC stuff?
typedef enum {
	doom,						   // DOOM 1
	doom2,						   // DOOM 2
	pack_tnt,					   // TNT mission pack
	pack_plut,					   // Plutonia pack
	none
} GameMission_t;

// Identify language to use, software localization.
typedef enum {
	english,
	french,
	german,
	unknown
} Language_t;

// If rangecheck is undefined,
// most parameter validation debugging code will not be compiled
#define RANGECHECK

// Do or do not use external soundserver.
// The sndserver binary to be run separately
//  has been introduced by Dave Taylor.
// The integrated sound support is experimental,
//  and unfinished. Default is synchronous.
// Experimental asynchronous timer based is
//  handled by SNDINTR.
#define SNDSERV  1
//#define SNDINTR  1

// This one switches between MIT SHM (no proper mouse)
// and XFree86 DGA (mickey sampling). The original
// linuxdoom used SHM, which is default.
//#define X11_DGA       1

//
// For resize of screen, at start of game.
// It will not work dynamically, see visplanes.
//
#define	BASE_WIDTH		320

// It is educational but futile to change this
//  scaling e.g. to 2. Drawing of status bar,
//  menues etc. is tied to the scale implied
//  by the graphics.
#define	SCREEN_MUL		1
#define	INV_ASPECT_RATIO	0.625  // 0.75, ideally

// Defines suck. C sucks.
// C++ might sucks for OOP, but it sure is a better C.
// So there.
#define SCREENWIDTH  320
//SCREEN_MUL*BASE_WIDTH //320
#define SCREENHEIGHT 200
//(int)(SCREEN_MUL*BASE_WIDTH*INV_ASPECT_RATIO) //200

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS		16

// State updates, number of tics / second.
#define TICRATE		35

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo.
typedef enum {
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN,
	GS_WAITING,
	GS_INFINE
} gamestate_t;

//
// Difficulty/skill settings/filters.
//

// Skill flags.
#define	MTF_EASY		1
#define	MTF_NORMAL		2
#define	MTF_HARD		4

// Deaf monsters/do not react to sound.
#define	MTF_AMBUSH		8

typedef enum {
	sk_baby,
	sk_easy,
	sk_medium,
	sk_hard,
	sk_nightmare
} skill_t;

//
// Key cards.
//
typedef enum {
	it_bluecard,
	it_yellowcard,
	it_redcard,
	it_blueskull,
	it_yellowskull,
	it_redskull,

	NUMCARDS
} card_t;

// The defined weapons,
//  including a marker indicating
//  user has not changed weapon.
typedef enum {
	wp_fist,
	wp_pistol,
	wp_shotgun,
	wp_chaingun,
	wp_missile,
	wp_plasma,
	wp_bfg,
	wp_chainsaw,
	wp_supershotgun,

	NUMWEAPONS,

	// No pending weapon change.
	wp_nochange
} weapontype_t;

// Ammunition types defined.
typedef enum {
	am_clip,					   // Pistol / chaingun ammo.
	am_shell,					   // Shotgun / double barreled shotgun.
	am_cell,					   // Plasma rifle, BFG.
	am_misl,					   // Missile launcher.
	NUMAMMO,
	am_noammo					   // Unlimited for chainsaw / fist.
} ammotype_t;

// Power up artifacts.
typedef enum {
	pw_invulnerability,
	pw_strength,
	pw_invisibility,
	pw_ironfeet,
	pw_allmap,
	pw_infrared,
	NUMPOWERS
} powertype_t;

//
// Power up durations,
//  how many seconds till expiration,
//  assuming TICRATE is 35 ticks/second.
//
typedef enum {
	INVULNTICS = (30 * TICRATE),
	INVISTICS = (60 * TICRATE),
	INFRATICS = (120 * TICRATE),
	IRONTICS = (60 * TICRATE)
} powerduration_t;

// Header, generated by sound utility.
// The utility was written by Dave Taylor.
//#include "sounds.h"

enum { VX, VY, VZ };			   // Vertex indices.

#define VCOORDS_DEFINED

#define IS_SERVER		Get(DD_SERVER)
#define IS_CLIENT		Get(DD_CLIENT)
#define IS_NETGAME		Get(DD_NETGAME)
#define IS_DEDICATED	Get(DD_DEDICATED)

void            D_IdentifyVersion(void);
void            D_SetPlayerPtrs(void);
char           *G_Get(int id);

void            R_SetViewSize(int blocks, int detail);
void            R_DrawPlayerSprites(ddplayer_t *viewplr);

#endif							// __DOOMDEF__
