/**\file doomdef.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

#ifndef LIBHERETIC_DEFS_H
#define LIBHERETIC_DEFS_H

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
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
    heretic_shareware,
    heretic,
    heretic_extended,
    NUM_GAME_MODES
} gamemode_t;

// Game mode bits for the above.
#define GM_HERETIC_SHAREWARE 0x1
#define GM_HERETIC          0x2
#define GM_HERETIC_EXTENDED 0x4

#define GM_ANY              (GM_HERETIC_SHAREWARE|GM_HERETIC|GM_HERETIC_EXTENDED)
#define GM_NOT_SHAREWARE    (GM_HERETIC|GM_HERETIC_EXTENDED)

#define SCREENWIDTH         320
#define SCREENHEIGHT        200
#define SCREEN_MUL          1

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS          16
#define NUMPLAYERCOLORS     4

#define NUMTEAMS            4 // Color = team.

// Playsim, core timing rate in cycles per second.
#define TICRATE             35
#define TICSPERSEC          35

/**
 * Player Classes
 */
typedef enum {
    PCLASS_PLAYER,
    PCLASS_CHICKEN,
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
    fixed_t     forwardMove[2]; // walk, run
    fixed_t     sideMove[2]; // walk, run
    int         moveMul; // multiplier for above
    int         turnSpeed[3]; // [normal, speed, initial]
    int         jumpTics; // wait inbetween jumps
    int         failUseSound; // sound played when a use fails.
} classinfo_t;

DE_EXTERN_C classinfo_t classInfo[NUM_PLAYER_CLASSES];

/**
 * Game state (hi-level).
 *
 * The current state of the game: whether we are playing, gazing at the
 * intermission screen, the game final animation, or a demo.
 */
typedef enum {
    GS_STARTUP,
    GS_MAP,
    GS_INTERMISSION,
    GS_FINALE,
    GS_WAITING,
    GS_INFINE,
    NUM_GAME_STATES
} gamestate_t;

/**
 * Difficulty/skill settings/filters.
 */
typedef enum {
    SM_NOTHINGS = -1,
    SM_BABY = 0,
    SM_EASY,
    SM_MEDIUM,
    SM_HARD,
    SM_NIGHTMARE,
    NUM_SKILL_MODES
} skillmode_t;

/**
 * Keys (as in, keys to lockables).
 */
typedef enum {
    KT_FIRST,
    KT_YELLOW = KT_FIRST,
    KT_GREEN,
    KT_BLUE,
    NUM_KEY_TYPES
} keytype_t;

/**
 * Weapon ids.
 *
 * The defined weapons, including a marker indicating user has not changed
 * weapon.
 */
typedef enum {
    WT_FIRST, // staff / beak
    WT_SECOND, // goldwand / beak
    WT_THIRD, // crossbow / beak
    WT_FOURTH, // blaster / beak
    WT_FIFTH, // skullrod / beak
    WT_SIXTH, // phoenixrod / beak
    WT_SEVENTH, // mace / beak
    WT_EIGHTH, // gauntlets / beak
    NUM_WEAPON_TYPES,

    WT_NOCHANGE // No pending weapon change.
} weapontype_t;

#define VALID_WEAPONTYPE(val) ((val) >= WT_FIRST && (val) < WT_FIRST + NUM_WEAPON_TYPES)

#define NUMWEAPLEVELS       2 // Number of weapon power levels.

/**
 * Ammunition type identifier.
 */
typedef enum {
    AT_FIRST,
    AT_CRYSTAL = AT_FIRST,
    AT_ARROW,
    AT_ORB,
    AT_RUNE,
    AT_FIREORB,
    AT_MSPHERE,
    NUM_AMMO_TYPES,

    AT_NOAMMO // Takes no ammo, used for staff, gauntlets.
} ammotype_t;

/**
 * Ammunition type definition.
 */
typedef struct {
    int gameModeBits;    ///< Game modes the ammo type is available in.
    const char *hudIcon; ///< Name of the Patch to use in headup displays.
} AmmoDef;

/**
 * Powers, bestowable upon players only.
 */
typedef enum {
    PT_NONE, /// @todo Remove me (index from zero).
    PT_FIRST,
    PT_INVULNERABILITY = PT_FIRST,
    PT_INVISIBILITY,
    PT_ALLMAP,
    PT_INFRARED,
    PT_WEAPONLEVEL2, // Temporarily boost all owned weapons to level 2.
    PT_FLIGHT,
    PT_SHIELD,
    PT_HEALTH2,
    NUM_POWER_TYPES
} powertype_t;

#define INVULNTICS          (30*TICRATE)
#define INVISTICS           (60*TICRATE)
#define INFRATICS           (120*TICRATE)
#define IRONTICS            (60*TICRATE)
#define WPNLEV2TICS         (40*TICRATE)
#define FLIGHTTICS          (60*TICRATE)
#define CHICKENTICS         (40*TICRATE)

/**
 * Inventory Item Types:
 */
typedef enum {
    IIT_NONE = 0,
    IIT_FIRST = 1,
    IIT_INVULNERABILITY = IIT_FIRST,
    IIT_INVISIBILITY,
    IIT_HEALTH,
    IIT_SUPERHEALTH,
    IIT_TOMBOFPOWER,
    IIT_TORCH,
    IIT_FIREBOMB,
    IIT_EGG,
    IIT_FLY,
    IIT_TELEPORT,
    NUM_INVENTORYITEM_TYPES
} inventoryitemtype_t;

#define MAXINVITEMCOUNT        16

#define BLINKTHRESHOLD      (4*TICRATE)

int         G_GetInteger(int id);
void       *G_GetVariable(int id);

// Most damage defined using HITDICE
#define HITDICE(a)          ((1+(P_Random()&7))*a)

#define SBARHEIGHT          (42) // status bar height at bottom of screen

#define TELEFOGHEIGHT       (32)

#define MAXEVENTS           (64)

#define DEFAULT_PLAYER_VIEWHEIGHT (41)

DE_EXTERN_C int localQuakeHappening[MAXPLAYERS];
DE_EXTERN_C int localQuakeTimeout[MAXPLAYERS];

#endif /* LIBHERETIC_DEFS_H */
