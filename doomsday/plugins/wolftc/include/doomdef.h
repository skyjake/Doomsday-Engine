/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * Internally used data structures for virtually everything,
 * key definitions, lots of other stuff.
 */

#ifndef __DOOMDEF__
#define __DOOMDEF__

#ifndef __WOLFTC__
#  error "Using WolfTC headers without __WOLFTC__"
#endif

#ifdef WIN32
#pragma warning(disable:4244)
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
#define STARTUPPK3    "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".pk3"

// Misc macros.
#define CLAMP(v, min, max) (v < min? v=min : v > max? v=max : v)

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
    indetermined,                  // Well, no IWAD found.
    NUM_GAME_MODES
} gamemode_t;

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
    GM_DOOM,                          // DOOM 1
    GM_DOOM2,                         // DOOM 2
    GM_TNT,                      // TNT mission pack
    GM_PLUT,                     // Plutonia pack
    GM_NONE,
    NUM_GAME_MISSIONS
} gamemission_t;

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
    NUM_PLAYER_CLASSES
} playerclass_t;

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
    int         movemul;            // multiplier for above
    int         jumptics;           // wait inbetween jumps
    int         failUseSound;       // sound played when a use fails.
} classinfo_t;

extern classinfo_t classInfo[NUM_PLAYER_CLASSES];

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
    SM_NOITEMS = -1, // skill mode 0
    SM_BABY = 0,
    SM_EASY,
    SM_MEDIUM,
    SM_HARD,
    SM_NIGHTMARE,
    NUM_SKILL_MODES
} skillmode_t;

//
// Key cards.
//
typedef enum {
    KT_BLUECARD,
    KT_YELLOWCARD,
    KT_REDCARD,
    KT_BLUESKULL,
    KT_YELLOWSKULL,
    KT_REDSKULL,
    NUM_KEY_TYPES
} keytype_t;

// The defined weapons,
//  including a marker indicating
//  user has not changed weapon.
typedef enum {
    WT_FIRST, // fist
    WT_SECOND, // pistol
    WT_THIRD, // shotgun
    WT_FOURTH, // chaingun
    WT_FIFTH, // missile launcher
    WT_SIXTH, // plasma rifle
    WT_SEVENTH, // bfg
    WT_EIGHTH, // chainsaw
    WT_NINETH, // supershotgun
    NUM_WEAPON_TYPES,

    // No pending weapon change.
    WT_NOCHANGE
} weapontype_t;

#define NUMWEAPLEVELS       1       // DOOM weapons have 1 power level.

// Ammunition types defined.
typedef enum {
    AT_CLIP,                       // Pistol / chaingun ammo.
    AT_SHELL,                      // Shotgun / double barreled shotgun.
    AT_CELL,                       // Plasma rifle, BFG.
    AT_MISSILE,                       // Missile launcher.
    NUM_AMMO_TYPES,
    AT_NOAMMO                      // Unlimited for chainsaw / fist.
} ammotype_t;

// Power up artifacts.
typedef enum {
    PT_INVULNERABILITY,
    PT_STRENGTH,
    PT_INVISIBILITY,
    PT_IRONFEET,
    PT_ALLMAP,
    PT_INFRARED,
    PT_FLIGHT,
    NUM_POWER_TYPES
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

//returns a number from 0 to 255
int             P_Random(void);
void            M_ClearRandom(void);

void            G_IdentifyVersion(void);
int             G_GetInteger(int id);
void           *G_GetVariable(int id);

void            R_SetViewSize(int blocks, int detail);

#endif                          // __DOOMDEF__
