/** @file h2def.h
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef __H2DEF_H__
#define __H2DEF_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
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

// Predefined with some OS.
#ifdef UNIX

#include <limits.h>

#define MAXCHAR     SCHAR_MAX
#define MAXSHORT    SHRT_MAX
#define MAXINT      INT_MAX
#define MAXLONG     LONG_MAX

#define MINCHAR     SCHAR_MIN
#define MINSHORT    SHRT_MIN
#define MININT      INT_MIN
#define MINLONG     LONG_MIN

#else /* not UNIX */

#define MAXCHAR     ((char)0x7f)
#define MAXSHORT    ((short)0x7fff)

// Max pos 32-bit int.
#define MAXINT      ((int)0x7fffffff)
#define MAXLONG     ((long)0x7fffffff)
#define MINCHAR     ((char)0x80)
#define MINSHORT    ((short)0x8000)

// Max negative 32-bit integer.
#define MININT      ((int)0x80000000)
#define MINLONG     ((long)0x80000000)
#endif

//#define Set                 DD_SetInteger
#define Get                 DD_GetInteger

//
// Global parameters/defines.
//

#define MOBJINFO            (*_api_InternalData.mobjInfo)
#define STATES              (*_api_InternalData.states)
#define VALIDCOUNT          (*_api_InternalData.validCount)

typedef enum {
    hexen_demo,
    hexen,
    hexen_deathkings,
    hexen_betademo, // hexen_demo with some bugs
    hexen_v10,      // Hexen release 1.0
    NUM_GAME_MODES
} gamemode_t;

// Game mode bits for the above.
#define GM_HEXEN_DEMO       0x1
#define GM_HEXEN            0x2
#define GM_HEXEN_DEATHKINGS 0x4
#define GM_HEXEN_BETA       0x8
#define GM_HEXEN_V10        0x10

#define GM_ANY              (GM_HEXEN_DEMO|GM_HEXEN|GM_HEXEN_DEATHKINGS|GM_HEXEN_BETA|GM_HEXEN_V10)

#define SCREENWIDTH         320
#define SCREENHEIGHT        200
#define SCREEN_MUL          1

#define MAXPLAYERS          8
#define NUMPLAYERCOLORS     8

#define NUMTEAMS            8 // Color = team.

// Playsim, core timing rate in cycles per second.
#define TICRATE             35
#define TICSPERSEC          35

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
 * Armor types.
 */
typedef enum {
    ARMOR_FIRST,
    ARMOR_ARMOR = ARMOR_FIRST,
    ARMOR_SHIELD,
    ARMOR_HELMET,
    ARMOR_AMULET,
    NUMARMOR
} armortype_t;

/**
 * Player weapon types:
 */
typedef enum {
    WT_FIRST,
    WT_SECOND,
    WT_THIRD,
    WT_FOURTH,
    NUM_WEAPON_TYPES,

    WT_NOCHANGE  ///< No pending change.
} weapontype_t;

#define VALID_WEAPONTYPE(val)       ((val) >= WT_FIRST && (val) < WT_FIRST + NUM_WEAPON_TYPES)

// Total number of weapon power levels.
#define NUMWEAPLEVELS               ( 1 )

// Total number of pieces for the fourth weapon.
#define WEAPON_FOURTH_PIECE_COUNT   ( 3 )

// All fourth-weapon pieces in bits.
#define WEAPON_FOURTH_COMPLETE      ((WEAPON_FOURTH_PIECE_COUNT << 1) + 1)

/**
 * Player Classes
 */
typedef enum {
    PCLASS_NONE = -1,
    PCLASS_FIRST = 0,
    PCLASS_FIGHTER = PCLASS_FIRST,
    PCLASS_CLERIC,
    PCLASS_MAGE,
    PCLASS_PIG,
    NUM_PLAYER_CLASSES
} playerclass_t;

#define VALID_PLAYER_CLASS(c)       ((c) >= PCLASS_FIRST && (c) < NUM_PLAYER_CLASSES)

#define PCLASS_INFO(plrClass)       (&classInfo[plrClass])

typedef struct classinfo_s
{
    playerclass_t plrClass;
    const char *niceName;
    dd_bool     userSelectable;
    mobjtype_t  mobjType;
    int         normalState;
    int         runState;
    int         attackState;
    int         attackEndState;
    int         maxArmor;
    int         autoArmorSave;
    fixed_t     maxMove;
    fixed_t     forwardMove[2];  ///< walk, run.
    fixed_t     sideMove[2];     ///< walk, run.
    int         moveMul;         ///< multiplier for above.
    int         turnSpeed[3];    ///< [normal, speed, initial].
    int         jumpTics;        ///< wait inbetween jumps.
    int         failUseSound;    ///< sound played when a use fails.
    int         armorIncrement[NUMARMOR];
    textenum_t  skillModeName[NUM_SKILL_MODES];
    struct weaponpiecedata_s
    {
        Point2Raw   offset;
        const char *patchName;
    } fourthWeaponPiece[WEAPON_FOURTH_PIECE_COUNT];
    const char *fourthWeaponCompletePatchName;
} classinfo_t;

DE_EXTERN_C classinfo_t classInfo[NUM_PLAYER_CLASSES];

/**
 * Game state (hi-level).
 *
 * The current state of the game: whether we are playing, gazing at the
 * intermission screen, the game final animation, or a demo.
 */
typedef enum {
    GS_MAP,
    GS_INTERMISSION,
    GS_FINALE,
    GS_STARTUP,
    GS_WAITING,
    GS_INFINE,
    NUM_GAME_STATES
} gamestate_t;

/**
 * Keys (as in, keys to lockables).
 */
typedef enum {
    KT_FIRST,
    KT_KEY1 = KT_FIRST,
    KT_KEY2,
    KT_KEY3,
    KT_KEY4,
    KT_KEY5,
    KT_KEY6,
    KT_KEY7,
    KT_KEY8,
    KT_KEY9,
    KT_KEYA,
    KT_KEYB,
    NUM_KEY_TYPES
} keytype_t;

/**
 * Ammunition types.
 */
typedef enum {
    AT_FIRST,
    AT_BLUEMANA = AT_FIRST,
    AT_GREENMANA,
    NUM_AMMO_TYPES,

    AT_NOAMMO // Takes no ammo, used for staff, gauntlets.
} ammotype_t;

#define MAX_MANA            200

/**
 * Powers, bestowable upon players only.
 */
typedef enum {
    PT_NONE,
    PT_FIRST,
    PT_INVULNERABILITY = PT_FIRST,
    PT_ALLMAP,
    PT_INFRARED,
    PT_FLIGHT,
    PT_SHIELD,
    PT_HEALTH2,
    PT_SPEED,
    PT_MINOTAUR,
    NUM_POWER_TYPES
} powertype_t;

#define INVULNTICS          (30*TICRATE)
#define INVISTICS           (60*TICRATE)
#define INFRATICS           (120*TICRATE)
#define IRONTICS            (60*TICRATE)
#define WPNLEV2TICS         (40*TICRATE)
#define FLIGHTTICS          (60*TICRATE)
#define SPEEDTICS           (45*TICRATE)
#define MORPHTICS           (40*TICRATE)
//#define MAULATORTICS      (25*TICRATE)

/**
 * Inventory Item Types:
 */
typedef enum {
    IIT_NONE = 0,
    IIT_FIRST = 1,
    IIT_INVULNERABILITY = IIT_FIRST,
    IIT_HEALTH,
    IIT_SUPERHEALTH,
    IIT_HEALINGRADIUS,
    IIT_SUMMON,
    IIT_TORCH,
    IIT_EGG,
    IIT_FLY,
    IIT_BLASTRADIUS,
    IIT_POISONBAG,
    IIT_TELEPORTOTHER,
    IIT_SPEED,
    IIT_BOOSTMANA,
    IIT_BOOSTARMOR,
    IIT_TELEPORT,
    // Puzzle items:
    IIT_FIRSTPUZZITEM,
    IIT_PUZZSKULL = IIT_FIRSTPUZZITEM,
    IIT_PUZZGEMBIG,
    IIT_PUZZGEMRED,
    IIT_PUZZGEMGREEN1,
    IIT_PUZZGEMGREEN2,
    IIT_PUZZGEMBLUE1,
    IIT_PUZZGEMBLUE2,
    IIT_PUZZBOOK1,
    IIT_PUZZBOOK2,
    IIT_PUZZSKULL2,
    IIT_PUZZFWEAPON,
    IIT_PUZZCWEAPON,
    IIT_PUZZMWEAPON,
    IIT_PUZZGEAR1,
    IIT_PUZZGEAR2,
    IIT_PUZZGEAR3,
    IIT_PUZZGEAR4,
    NUM_INVENTORYITEM_TYPES
} inventoryitemtype_t;

#define MAXINVITEMCOUNT        (25)

#define BLINKTHRESHOLD      (4*TICRATE)

// Uncomment, to enable all timebomb stuff.
#define TIMEBOMB_YEAR       (95) // years since 1900
#define TIMEBOMB_STARTDATE  (268) // initial date (9/26)
#define TIMEBOMB_ENDDATE    (301) // end date (10/29)

DE_EXTERN_C int maulatorSeconds;

#define MAULATORTICS        ((unsigned int) maulatorSeconds * TICSPERSEC)

// Most damage defined using HITDICE
#define HITDICE(a)          ((1 + (P_Random() & 7)) * (a))

#define SBARHEIGHT          (39) // status bar height at bottom of screen

#define TELEFOGHEIGHT       (32)

#define DEFAULT_PLAYER_VIEWHEIGHT (48)

DE_EXTERN_C fixed_t finesine[5 * FINEANGLES / 4];
DE_EXTERN_C fixed_t *finecosine;

// Set if homebrew PWAD stuff has been added.
DE_EXTERN_C dd_bool  modifiedgame;

#define MAX_PLAYER_STARTS   (8)

DE_EXTERN_C int localQuakeHappening[MAXPLAYERS];
DE_EXTERN_C int localQuakeTimeout[MAXPLAYERS]; // zero for unlimited

#ifdef __cplusplus
extern "C" {
#endif

byte P_Random(void);
void M_ResetRandom(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __H2DEF_H__
