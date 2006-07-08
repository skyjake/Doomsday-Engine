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
 * Internally used data structures for virtually everything,
 * key definitions, lots of other stuff.
 */

#ifndef __DOOMDEF__
#define __DOOMDEF__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifdef WIN32
#pragma warning(disable:4244 4761)
#endif

#include <stdio.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"
#include "g_dgl.h"
#include "version.h"
#include "p_ticcmd.h"

#define Set         DD_SetInteger
#define Get         DD_GetInteger

#define CONFIGFILE    GAMENAMETEXT".cfg"
#define DEFSFILE      GAMENAMETEXT"\\"GAMENAMETEXT".ded"
#define DATAPATH      "}data\\"GAMENAMETEXT"\\"
#define STARTUPWAD    "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".wad"

extern game_import_t gi;
extern game_export_t gx;

//
// Global parameters/defines.
//

#define mobjinfo    (*gi.mobjinfo)
#define states      (*gi.states)
#define validCount  (*gi.validcount)

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

// Game mode handling - identify IWAD version
//  to handle IWAD dependend animations etc.
typedef enum {
    shareware,                     // DOOM 1 shareware, E1, M9
    registered,                    // DOOM 1 registered, E3, M27
    commercial,                    // DOOM 2 retail, E1 M34
    // DOOM 2 german edition not handled
    retail,                        // DOOM 1 retail, E4, M36
    indetermined                   // Well, no IWAD found.
} GameMode_t;

// Game mode bits for the above.
#define GM_SHAREWARE        0x1     // DOOM 1 shareware, E1, M9
#define GM_REGISTERED       0x2     // DOOM 1 registered, E3, M27
#define GM_COMMERCIAL       0x4     // DOOM 2 retail, E1 M34
    // DOOM 2 german edition not handled
#define GM_RETAIL           0x8     // DOOM 1 retail, E4, M36
#define GM_INDETERMINED     0x16    // Well, no IWAD found.

#define GM_ANY              (GM_SHAREWARE|GM_REGISTERED|GM_COMMERCIAL|GM_RETAIL)
#define GM_NOTSHAREWARE     (GM_REGISTERED|GM_COMMERCIAL|GM_RETAIL)


// Mission packs - might be useful for TC stuff?
typedef enum {
    doom,                          // DOOM 1
    doom2,                         // DOOM 2
    pack_tnt,                      // TNT mission pack
    pack_plut,                     // Plutonia pack
    none
} GameMission_t;

// If rangecheck is undefined,
// most parameter validation debugging code will not be compiled
#define RANGECHECK

// Defines suck. C sucks.
// C++ might sucks for OOP, but it sure is a better C.
// So there.
#define SCREENWIDTH  320
//SCREEN_MUL*BASE_WIDTH //320
#define SCREENHEIGHT 200
#define SCREEN_MUL      1

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS      16

// State updates, number of tics / second.
#define TICRATE     35

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
// Player Classes
//
typedef enum {
    PCLASS_PLAYER,
    NUMCLASSES
} pclass_t;

#define PCLASS_INFO(class)  (&classInfo[class])

typedef struct classinfo_s{
    int         normalstate;
    int         runstate;
    int         attackstate;
    int         attackendstate;
    int         maxarmor;
    fixed_t     maxmove;
    fixed_t     forwardmove[2];     // walk, run
    fixed_t     sidemove[2];        // walk, run
} classinfo_t;

extern classinfo_t classInfo[NUMCLASSES];

//
// Difficulty/skill settings/filters.
//

// Skill flags.
#define MTF_EASY        1
#define MTF_NORMAL      2
#define MTF_HARD        4

// Deaf monsters/do not react to sound.
#define MTF_AMBUSH      8

typedef enum {
    sk_noitems = -1, // skill mode 0
    sk_baby = 0,
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

    NUMKEYS
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
    WP_NOCHANGE
} weapontype_t;

#define NUMWEAPLEVELS       1       // DOOM weapons have 1 power level.

// Ammunition types defined.
typedef enum {
    am_clip,                       // Pistol / chaingun ammo.
    am_shell,                      // Shotgun / double barreled shotgun.
    am_cell,                       // Plasma rifle, BFG.
    am_misl,                       // Missile launcher.
    NUMAMMO,
    AM_NOAMMO                      // Unlimited for chainsaw / fist.
} ammotype_t;

// Power up artifacts.
typedef enum {
    pw_invulnerability,
    pw_strength,
    pw_invisibility,
    pw_ironfeet,
    pw_allmap,
    pw_infrared,
    pw_flight,
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

enum { VX, VY, VZ };               // Vertex indices.

#define IS_SERVER       Get(DD_SERVER)
#define IS_CLIENT       Get(DD_CLIENT)
#define IS_NETGAME      Get(DD_NETGAME)
#define IS_DEDICATED    Get(DD_DEDICATED)

void            D_IdentifyVersion(void);
char           *G_Get(int id);

void            R_SetViewSize(int blocks, int detail);

#endif                          // __DOOMDEF__
