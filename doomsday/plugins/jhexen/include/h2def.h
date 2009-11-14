/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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

/**
 * h2def.h:
 */

#ifndef __H2DEF_H__
#define __H2DEF_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#ifdef WIN32
#pragma warning(disable:4244)
#endif

#include <stdio.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"
#include "version.h"
#include "info.h"

#define Set                 DD_SetInteger
#define Get                 DD_GetInteger

#define CONFIGFILE          GAMENAMETEXT".cfg"
#define DEFSFILE            GAMENAMETEXT"\\"GAMENAMETEXT".ded"
#define DATAPATH            "}data\\"GAMENAMETEXT"\\"
#define STARTUPWAD          "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".wad"
#define STARTUPPK3          "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".pk3"

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

extern game_import_t gi;
extern game_export_t gx;

//
// Global parameters/defines.
//

#define MOBJINFO            (*gi.mobjInfo)
#define STATES              (*gi.states)
#define VALIDCOUNT          (*gi.validCount)

// Game mode handling - identify IWAD version to handle IWAD dependend
// animations etc.
typedef enum {
    shareware, // 4 map demo
    registered, // HEXEN registered
    extended, // DeathKings
    indetermined, // Well, no IWAD found.
    NUM_GAME_MODES
} gamemode_t;

// Game mode bits for the above.
#define GM_SHAREWARE        0x1 // 4 map demo
#define GM_REGISTERED       0x2 // HEXEN registered
#define GM_EXTENDED         0x4 // DeathKings
#define GM_INDETERMINED     0x8 // Well, no IWAD found.

#define GM_ANY              (GM_SHAREWARE|GM_REGISTERED|GM_EXTENDED)
#define GM_NOTSHAREWARE     (GM_REGISTERED|GM_EXTENDED)

#define SCREENWIDTH         320
#define SCREENHEIGHT        200
#define SCREEN_MUL          1

#define MAXPLAYERS          8

// Playsim, core timing rate in cycles per second.
#define TICRATE             35
#define TICSPERSEC          35

/**
 * Armor types.
 */
typedef enum {
    ARMOR_ARMOR,
    ARMOR_SHIELD,
    ARMOR_HELMET,
    ARMOR_AMULET,
    NUMARMOR
} armortype_t;

/**
 * Player Classes
 */
typedef enum {
    PCLASS_FIGHTER,
    PCLASS_CLERIC,
    PCLASS_MAGE,
    PCLASS_PIG,
    NUM_PLAYER_CLASSES
} playerclass_t;

#define PCLASS_INFO(class)  (&classInfo[class])

typedef struct classinfo_s{
    const char* niceName;
    boolean     userSelectable;
    mobjtype_t  mobjType;
    int         normalState;
    int         runState;
    int         attackState;
    int         attackEndState;
    int         maxArmor;
    int         autoArmorSave;
    fixed_t     maxMove;
    fixed_t     forwardMove[2]; // walk, run.
    fixed_t     sideMove[2]; // walk, run.
    int         moveMul; // multiplier for above.
    int         turnSpeed[3]; // [normal, speed, initial].
    int         jumpTics; // wait inbetween jumps.
    int         failUseSound; // sound played when a use fails.
    int         armorIncrement[NUMARMOR];
    int         pieceX[3]; // temp.
} classinfo_t;

extern classinfo_t classInfo[NUM_PLAYER_CLASSES];

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
    GS_DEMOSCREEN,
    GS_WAITING,
    GS_INFINE,
    NUM_GAME_STATES
} gamestate_t;

/**
 * Difficulty/skill settings/filters.
 */
typedef enum {
    SM_BABY,
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
    KT_KEY1,
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
 * Weapon ids.
 *
 * The defined weapons, including a marker indicating user has not changed
 * weapon.
 */
typedef enum {
    WT_FIRST,
    WT_SECOND,
    WT_THIRD,
    WT_FOURTH,
    NUM_WEAPON_TYPES,

    WT_NOCHANGE // No pending weapon change.
} weapontype_t;

#define WPIECE1             1
#define WPIECE2             2
#define WPIECE3             4

#define NUMWEAPLEVELS       1 // Number of weapon power levels.

/**
 * Ammunition types.
 */
typedef enum {
    AT_BLUEMANA,
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
    PT_INVULNERABILITY,
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

enum { VX, VY, VZ }; // Vertex indices.

enum { CR, CG, CB, CA }; // Color indices.

#define IS_SERVER           (Get(DD_SERVER))
#define IS_CLIENT           (Get(DD_CLIENT))
#define IS_NETGAME          (Get(DD_NETGAME))
#define IS_DEDICATED        (Get(DD_DEDICATED))

#define CVAR(typ, x)        (*(typ*)Con_GetVariable(x)->ptr)

#define SFXVOLUME           (Get(DD_SFX_VOLUME)/17)
#define MUSICVOLUME         (Get(DD_MUSIC_VOLUME)/17)

#define CONSOLEPLAYER       (Get(DD_CONSOLEPLAYER))
#define DISPLAYPLAYER       (Get(DD_DISPLAYPLAYER))

#define GAMETIC             (*((timespan_t*) DD_GetVariable(DD_GAMETIC)))

// Uncomment, to enable all timebomb stuff.
#define TIMEBOMB_YEAR       (95) // years since 1900
#define TIMEBOMB_STARTDATE  (268) // initial date (9/26)
#define TIMEBOMB_ENDDATE    (301) // end date (10/29)

extern int maulatorSeconds;

#define MAULATORTICS        ((unsigned int) maulatorSeconds * TICSPERSEC)

// Most damage defined using HITDICE
#define HITDICE(a)          ((1 + (P_Random() & 7)) * (a))

#define SBARHEIGHT          (39) // status bar height at bottom of screen

#define TELEFOGHEIGHT       (32)

extern fixed_t finesine[5 * FINEANGLES / 4];
extern fixed_t *finecosine;

// Set if homebrew PWAD stuff has been added.
extern boolean  modifiedgame;

#define MAX_PLAYER_STARTS   (8)

void            H2_Main(void);

void            G_IdentifyVersion(void);
void            G_CommonPreInit(void);
void            G_CommonPostInit(void);

int             G_GetInteger(int id);
void*           G_GetVariable(int id);

void            G_DeathMatchSpawnPlayer(int playernum);
int             G_GetMapNumber(int episode, int map);
void            G_InitNew(skillmode_t skill, int episode, int map);
void            G_DeferedInitNew(skillmode_t skill, int episode, int map);
void            G_DeferredNewGame(skillmode_t skill);
void            G_DeferedPlayDemo(char* demo);
void            G_DoPlayDemo(void);
void            G_LoadGame(int slot);
void            G_DoLoadGame(void);
void            G_RecordDemo(skillmode_t skill, int numplayers, int episode,
                             int map, char* name);
void            G_PlayDemo(char* name);
void            G_TimeDemo(char* name);
void            G_LeaveMap(int map, int position, boolean secret);
void            G_StartNewGame(skillmode_t skill);
void            G_StartNewInit(void);
void            G_WorldDone(void);
void            G_ScreenShot(void);
void            G_DoReborn(int playernum);
void            G_StopDemo(void);
void            G_DemoEnds(void);
void            G_DemoAborted(void);

void            G_Ticker(timespan_t ticLength);
boolean         G_Responder(event_t* ev);

void            P_Init(void);

void            P_SetupMap(int episode, int map, int playermask,
                           skillmode_t skill);

extern boolean setsizeneeded;

extern int      localQuakeHappening[MAXPLAYERS];

byte            P_Random(void);
void            M_ResetRandom(void);

extern unsigned char rndtable[256];

void            SC_Open(const char* name);
void            SC_OpenLump(lumpnum_t lump);
void            SC_OpenFile(const char* name);
void            SC_OpenFileCLib(const char* name);
void            SC_Close(void);
boolean         SC_GetString(void);
void            SC_MustGetString(void);
void            SC_MustGetStringName(char* name);
boolean         SC_GetNumber(void);
void            SC_MustGetNumber(void);
void            SC_UnGet(void);

boolean         SC_Compare(char* text);
int             SC_MatchString(char** strings);
int             SC_MustMatchString(char** strings);
void            SC_ScriptError(char* message);

extern char* sc_String;
extern int sc_Number;
extern int sc_Line;
extern boolean sc_End;
extern boolean sc_Crossed;
extern boolean sc_FileScripts;
extern const char* sc_ScriptsDir;

//----------------------
// Chat mode (CT_chat.c)
//----------------------

void            CT_Init(void);
void            CT_Drawer(void);
boolean         CT_Responder(event_t* ev);
void            CT_Ticker(void);
char            CT_dequeueChatChar(void);

extern boolean  chatmodeon;

void        Draw_TeleportIcon(void);

#endif // __H2DEF_H__
