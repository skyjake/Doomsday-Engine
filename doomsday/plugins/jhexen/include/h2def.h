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
 * h2def.h:
 */

#ifndef __H2DEF__
#define __H2DEF__

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
#include "g_dgl.h"
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

// Misc macros.
#define CLAMP(v, min, max)  (v < min? v=min : v > max? v=max : v)

extern game_import_t gi;
extern game_export_t gx;

//
// Global parameters/defines.
//

#define mobjinfo            (*gi.mobjinfo)
#define states              (*gi.states)
#define VALIDCOUNT          (*gi.validCount)

// Game mode handling - identify IWAD version to handle IWAD dependend
// animations etc.
typedef enum {
    shareware, // 4 level demo
    registered, // HEXEN registered
    extended, // DeathKings
    indetermined, // Well, no IWAD found.
    NUM_GAME_MODES
} gamemode_t;

// Game mode bits for the above.
#define GM_SHAREWARE        0x1 // 4 level demo
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
    mobjtype_t  mobjtype;
    int         normalstate;
    int         runstate;
    int         attackstate;
    int         attackendstate;
    int         maxarmor;
    int         autoarmorsave;
    fixed_t     maxmove;
    fixed_t     forwardmove[2]; // walk, run.
    fixed_t     sidemove[2]; // walk, run.
    int         movemul; // multiplier for above.
    fixed_t     turnSpeed[3]; // [normal, speed, initial].
    int         jumptics; // wait inbetween jumps.
    int         failUseSound; // sound played when a use fails.
    int         armorincrement[NUMARMOR];
    int         piecex[3]; // temp.
} classinfo_t;

extern classinfo_t classInfo[NUM_PLAYER_CLASSES];

/**
 * Game state (hi-level).
 *
 * The current state of the game: whether we are playing, gazing at the
 * intermission screen, the game final animation, or a demo.
 */
typedef enum {
    GS_LEVEL,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN,
    GS_WAITING,
    GS_INFINE
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
 * Artifacts (collectable, inventory items).
 */
typedef enum {
    arti_none,
    arti_invulnerability,
    arti_health,
    arti_superhealth,
    arti_healingradius,
    arti_summon,
    arti_torch,
    arti_egg,
    arti_fly,
    arti_blastradius,
    arti_poisonbag,
    arti_teleportother,
    arti_speed,
    arti_boostmana,
    arti_boostarmor,
    arti_teleport,
    // Puzzle artifacts
    arti_firstpuzzitem,
    arti_puzzskull = arti_firstpuzzitem,
    arti_puzzgembig,
    arti_puzzgemred,
    arti_puzzgemgreen1,
    arti_puzzgemgreen2,
    arti_puzzgemblue1,
    arti_puzzgemblue2,
    arti_puzzbook1,
    arti_puzzbook2,
    arti_puzzskull2,
    arti_puzzfweapon,
    arti_puzzcweapon,
    arti_puzzmweapon,
    arti_puzzgear1,
    arti_puzzgear2,
    arti_puzzgear3,
    arti_puzzgear4,
    NUMARTIFACTS
} artitype_e;

#define MAXARTICOUNT        (25)

#define BLINKTHRESHOLD      (4*TICRATE)

enum { VX, VY, VZ }; // Vertex indices.

#define IS_SERVER           Get(DD_SERVER)
#define IS_CLIENT           Get(DD_CLIENT)
#define IS_NETGAME          Get(DD_NETGAME)
#define IS_DEDICATED        Get(DD_DEDICATED)

#define CVAR(typ, x)        (*(typ*)Con_GetVariable(x)->ptr)

#define snd_SfxVolume       (Get(DD_SFX_VOLUME)/17)
#define snd_MusicVolume     (Get(DD_MUSIC_VOLUME)/17)

#define consoleplayer       Get(DD_CONSOLEPLAYER)
#define displayplayer       Get(DD_DISPLAYPLAYER)

// Uncomment, to enable all timebomb stuff.
#define TIMEBOMB_YEAR       (95) // years since 1900
#define TIMEBOMB_STARTDATE  (268) // initial date (9/26)
#define TIMEBOMB_ENDDATE    (301) // end date (10/29)

extern int MaulatorSeconds;

#define MAULATORTICS    ( (unsigned int) /*((netgame || demorecording || demoplayback)? 25*35 :*/ MaulatorSeconds*35 )

// Most damage defined using HITDICE
#define HITDICE(a)          ((1+(P_Random()&7))*a)

#define SBARHEIGHT          (39) // status bar height at bottom of screen

#define TELEFOGHEIGHT       (32)

extern fixed_t finesine[5 * FINEANGLES / 4];
extern fixed_t *finecosine;

extern boolean  DevMaps; // true = map development mode
extern char *DevMapsDir; // development maps directory
extern int DebugSound; // debug flag for displaying sound info

extern boolean cmdfrag; // true if a CMD_FRAG packet should be sent out every

extern int Sky1Texture;
extern int Sky2Texture;
extern int prevmap;

// Set if homebrew PWAD stuff has been added.
extern boolean  modifiedgame;

#define MAX_PLAYER_STARTS   (8)

void            H2_Main(void);

void            G_IdentifyVersion(void);
void            R_SetFilter(int filter);
int             R_GetFilterColor(int filter);
void            R_Init(void);

int             G_GetInteger(int id);
void           *G_GetVariable(int id);

void            G_DeathMatchSpawnPlayer(int playernum);
int             G_GetLevelNumber(int episode, int map);
void            G_InitNew(skillmode_t skill, int episode, int map);
void            G_DeferedInitNew(skillmode_t skill, int episode, int map);
void            G_DeferredNewGame(skillmode_t skill);
void            G_DeferedPlayDemo(char *demo);
void            G_DoPlayDemo(void);
void            G_LoadGame(int slot);
void            G_DoLoadGame(void);
void            G_SaveGame(int slot, char *description);
void            G_RecordDemo(skillmode_t skill, int numplayers, int episode,
                             int map, char *name);
void            G_PlayDemo(char *name);
void            G_TimeDemo(char *name);
void            G_TeleportNewMap(int map, int position);
void            G_LeaveLevel(int map, int position, boolean secret);
void            G_StartNewGame(skillmode_t skill);
void            G_StartNewInit(void);
void            G_WorldDone(void);
void            G_ScreenShot(void);
void            G_DoReborn(int playernum);
void            G_StopDemo(void);
void            G_DemoEnds(void);
void            G_DemoAborted(void);

void            G_Ticker(timespan_t ticLength);
boolean         G_Responder(event_t *ev);

void            P_Init(void);

void            P_SetupLevel(int episode, int map, int playermask,
                             skillmode_t skill);

extern boolean setsizeneeded;

void            R_SetViewSize(int blocks, int detail);

extern int      localQuakeHappening[MAXPLAYERS];

byte            P_Random(void);
void            M_ClearRandom(void);

extern unsigned char rndtable[256];

void            SC_Open(char *name);
void            SC_OpenLump(char *name);
void            SC_OpenFile(char *name);
void            SC_OpenFileCLib(char *name);
void            SC_Close(void);
boolean         SC_GetString(void);
void            SC_MustGetString(void);
void            SC_MustGetStringName(char *name);
boolean         SC_GetNumber(void);
void            SC_MustGetNumber(void);
void            SC_UnGet(void);

boolean         SC_Compare(char *text);
int             SC_MatchString(char **strings);
int             SC_MustMatchString(char **strings);
void            SC_ScriptError(char *message);

extern char *sc_String;
extern int sc_Number;
extern int sc_Line;
extern boolean sc_End;
extern boolean sc_Crossed;
extern boolean sc_FileScripts;
extern char *sc_ScriptsDir;

//----------------------
// Interlude (IN_lude.c)
//----------------------

#define MAX_INTRMSN_MESSAGE_SIZE 1024

extern boolean  intermission;
extern char     ClusterMessage[MAX_INTRMSN_MESSAGE_SIZE];

void            IN_Start(void);
void            IN_Stop(void);
void            IN_Ticker(void);
void            IN_Drawer(void);

//----------------------
// Chat mode (CT_chat.c)
//----------------------

void            CT_Init(void);
void            CT_Drawer(void);
boolean         CT_Responder(event_t *ev);
void            CT_Ticker(void);
char            CT_dequeueChatChar(void);

extern boolean  chatmodeon;

void        Draw_TeleportIcon(void);

#endif // __H2DEF_H__
