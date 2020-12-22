/**\file doomdef.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOM_DEFS_H
#define LIBDOOM_DEFS_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#  ifdef MSVC
#    pragma warning(disable:4244)
#  endif
#  define stricmp _stricmp
#  define strnicmp _strnicmp
#  define strlwr _strlwr
#  define strupr _strupr
#endif

#include <de/c_wrapper.h>
#include <de/legacy/fixedpoint.h>
#include <de/ddkey.h>
#include <doomsday/gamefw/defs.h>
#include <doomsday/world/mobj.h>
#include <doomsday/tab_tables.h>
#include "doomsday.h"
#include "version.h"
#include "info.h"

//#define Set                 DD_SetInteger
#define Get                 DD_GetInteger

//
// Global parameters/defines.
//

#define MOBJINFO            (*_api_InternalData.mobjInfo)
#define STATES              (*_api_InternalData.states)
#define VALIDCOUNT          (*_api_InternalData.validCount)

typedef enum {
    doom_shareware,
    doom,
    doom_ultimate,
    doom_chex,
    doom2,
    doom2_plut,
    doom2_tnt,
    doom2_hacx,
    doom2_freedm,
    doom2_freedoom,
    doom_freedoom,
    doom_bfg,
    doom2_bfg,
    doom2_nerve,
    NUM_GAME_MODES
} gamemode_t;

// Game mode bits for the above (must match the enum order).
#define GM_DOOM_SHAREWARE   0x1
#define GM_DOOM             0x2
#define GM_DOOM_ULTIMATE    0x4
#define GM_DOOM_CHEX        0x8
#define GM_DOOM2            0x10
#define GM_DOOM2_PLUT       0x20
#define GM_DOOM2_TNT        0x40
#define GM_DOOM2_HACX       0x80
#define GM_DOOM2_FREEDM     0x100
#define GM_DOOM2_FREEDOOM   0x200
#define GM_DOOM_FREEDOOM    0x400
#define GM_DOOM_BFG         0x800
#define GM_DOOM2_BFG        0x1000
#define GM_DOOM2_NERVE      0x2000

#define GM_ANY_DOOM         (GM_DOOM|GM_DOOM_BFG|GM_DOOM_SHAREWARE|GM_DOOM_ULTIMATE|GM_DOOM_CHEX|GM_DOOM_FREEDOOM)
#define GM_ANY_DOOM2        (GM_DOOM2|GM_DOOM2_BFG|GM_DOOM2_PLUT|GM_DOOM2_TNT|GM_DOOM2_NERVE|GM_DOOM2_HACX|GM_DOOM2_FREEDM|GM_DOOM2_FREEDOOM)
#define GM_ANY              (GM_ANY_DOOM|GM_ANY_DOOM2)

#define SCREENWIDTH         320
#define SCREENHEIGHT        200
#define SCREEN_MUL          1

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS          16
#define NUMPLAYERCOLORS     4

#define NUMTEAMS            4 // Color = team.

// State updates, number of tics / second.
#define TICRATE             35

/**
 * The current (high-level) state of the game: whether we are playing,
 * gazing at the intermission screen, the game final animation, or a demo.
 */
typedef enum gamestate_e {
    GS_MAP,
    GS_INTERMISSION,
    GS_FINALE,
    GS_STARTUP,
    GS_WAITING,
    GS_INFINE,
    NUM_GAME_STATES
} gamestate_t;

//
// Player Classes
//
typedef enum {
    PCLASS_PLAYER,
    NUM_PLAYER_CLASSES
} playerclass_t;

#define PCLASS_INFO(plrClass)  (&classInfo[plrClass])

typedef struct classinfo_s{
    playerclass_t plrClass;
    char*       niceName;
    dd_bool     userSelectable;
    mobjtype_t  mobjType;
    int         normalState;
    int         runState;
    int         attackState;
    int         attackEndState;
    int         maxArmor;
    fixed_t     maxMove;
    fixed_t     forwardMove[2]; // [walk, run].
    fixed_t     sideMove[2]; // [walk, run].
    int         moveMul; // Multiplier for above.
    int         turnSpeed[2]; // [normal, speed]
    int         jumpTics; // Wait in between jumps.
    int         failUseSound; // Sound played when a use fails.
} classinfo_t;

DE_EXTERN_C classinfo_t classInfo[NUM_PLAYER_CLASSES];

typedef enum {
    SM_NOTHINGS = -1,
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
    KT_FIRST,
    KT_BLUECARD = KT_FIRST,
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

#define VALID_WEAPONTYPE(val) ((val) >= WT_FIRST && (val) < WT_FIRST + NUM_WEAPON_TYPES)

#define NUMWEAPLEVELS       2 // DOOM weapons have 1 power level.

// Ammunition types defined.
typedef enum {
    AT_FIRST,
    AT_CLIP = AT_FIRST, // Pistol / chaingun ammo.
    AT_SHELL, // Shotgun / double barreled shotgun.
    AT_CELL, // Plasma rifle, BFG.
    AT_MISSILE, // Missile launcher.
    NUM_AMMO_TYPES,
    AT_NOAMMO // Unlimited for chainsaw / fist.
} ammotype_t;

// Power ups.
typedef enum {
    PT_FIRST,
    PT_INVULNERABILITY = PT_FIRST,
    PT_STRENGTH,
    PT_INVISIBILITY,
    PT_IRONFEET,
    PT_ALLMAP,
    PT_INFRARED,
    PT_FLIGHT,
    NUM_POWER_TYPES
} powertype_t;

/**
 * Power up durations, how many seconds till expiration, assuming TICRATE
 * is 35 ticks/second.
 */
typedef enum {
    INVULNTICS = (30 * TICRATE),
    INVISTICS = (60 * TICRATE),
    INFRATICS = (120 * TICRATE),
    IRONTICS = (60 * TICRATE)
} powerduration_t;

#define DEFAULT_PLAYER_VIEWHEIGHT (41)

#endif /* LIBDOOM_DEFS_H */
