/**
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 *
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999-2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © Raven Software, Corp.
 *\author Copyright © 1993-1996 by id Software, Inc.
 */

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
 * g_game.c : Top-level game routines.
 *
 * Compiles for jDoom, jHeretic and jHexen.
 */

// HEADER FILES ------------------------------------------------------------
#include <ctype.h>
#include <string.h>
#include <math.h>

#if  __DOOM64TC__
#  include <stdlib.h>
#  include "doom64tc.h"
#  include "p_saveg.h"
#elif __WOLFTC__
#  include <stdlib.h>
#  include "wolftc.h"
#  include "p_saveg.h"
#elif __JDOOM__
#  include <stdlib.h>
#  include "jdoom.h"
#  include "p_saveg.h"
#elif __JHERETIC__
#  include <stdio.h>
#  include "jheretic.h"
#  include "p_saveg.h"
#  include "p_inventory.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "p_inventory.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "g_controls.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "am_map.h"
#include "hu_stuff.h"
#include "hu_msg.h"
#include "g_common.h"
#include "g_update.h"
#include "d_net.h"
#include "x_hair.h"
#include "p_player.h"
#include "r_common.h"
#include "p_mapspec.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define BODYQUESIZE         32

#define UNNAMEDMAP          "Unnamed"
#define NOTAMAPNAME         "N/A"
#define READONLYCVAR        CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__
struct missileinfo_s {
    mobjtype_t type;
    int     speed[2];
}
MonsterMissileInfo[] =
{
#if __JDOOM__
    {MT_BRUISERSHOT, {15, 20}},
    {MT_HEADSHOT, {10, 20}},
    {MT_TROOPSHOT, {10, 20}},
# if __DOOM64TC__
    {MT_BRUISERSHOTRED, {15, 20}},
    {MT_NTROSHOT, {20, 40}},
# endif
#elif __JHERETIC__
    {MT_IMPBALL, {10, 20}},
    {MT_MUMMYFX1, {9, 18}},
    {MT_KNIGHTAXE, {9, 18}},
    {MT_REDAXE, {9, 18}},
    {MT_BEASTBALL, {12, 20}},
    {MT_WIZFX1, {18, 24}},
    {MT_SNAKEPRO_A, {14, 20}},
    {MT_SNAKEPRO_B, {14, 20}},
    {MT_HEADFX1, {13, 20}},
    {MT_HEADFX3, {10, 18}},
    {MT_MNTRFX1, {20, 26}},
    {MT_MNTRFX2, {14, 20}},
    {MT_SRCRFX1, {20, 28}},
    {MT_SOR2FX1, {20, 28}},
#endif
    {-1, {-1, -1}}                  // Terminator
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

boolean cht_Responder(event_t *ev);

boolean M_EditResponder(event_t *ev);

void    P_InitPlayerValues(player_t *p);

#if __JHEXEN__
void    P_InitSky(int map);
#endif

void    HU_UpdatePsprites(void);

void    G_ConsoleRegistration(void);
void    DetectIWADs(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    G_PlayerReborn(int player);
void    G_InitNew(skillmode_t skill, int episode, int map);
void    G_DoInitNew(void);
void    G_DoReborn(int playernum);
void    G_DoLoadLevel(void);
void    G_DoNewGame(void);
void    G_DoLoadGame(void);
void    G_DoPlayDemo(void);
void    G_DoCompleted(void);
void    G_DoVictory(void);
void    G_DoWorldDone(void);
void    G_DoSaveGame(void);
void    G_DoScreenShot(void);
boolean G_ValidateMap(int *episode, int *map);

#if __JHEXEN__ || __JSTRIFE__
void    G_DoTeleportNewMap(void);
void    G_DoSingleReborn(void);
void    H2_PageTicker(void);
void    H2_AdvanceDemo(void);
#endif

void    G_StopDemo(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#if __JDOOM__
extern gamemission_t gamemission;
#endif

#if __JHERETIC__
extern int playerkeys;
#endif

extern char *borderLumps[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifdef TIC_DEBUG
FILE   *rndDebugfile;
#endif

game_config_t cfg; // The global cfg.

gameaction_t gameaction;
skillmode_t gameskill;
int     gameepisode;
int     gamemap;
int     nextmap;                // if non zero this will be the next map

#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
boolean respawnmonsters;
#endif

#ifndef __JDOOM__
int     prevmap;
#endif

boolean paused;
boolean sendpause;              // send a pause event next tic
boolean usergame;               // ok to save / end game

boolean viewactive;

boolean deathmatch;             // only if started as net death
player_t players[MAXPLAYERS];

int     levelstarttic;          // gametic at level start
int     totalkills, totalitems, totalsecret;    // for intermission

char    defdemoname[32];
boolean singledemo;             // quit after playing a demo from cmdline

boolean precache = true;        // if true, load all graphics at start

#if __JDOOM__
wbstartstruct_t wminfo;         // parms for world map / intermission
#endif

int     savegameslot;
char    savedescription[32];

#if __JDOOM__
mobj_t *bodyque[BODYQUESIZE];
int     bodyqueslot;
#endif

#if __JHEXEN__ || __JSTRIFE__
// Position indicator for cooperative net-play reborn
int     RebornPosition;
int     LeaveMap;
int     LeavePosition;
#endif

boolean secretexit;
char    savename[256];

#if __JHEXEN__ || __JSTRIFE__
int maphub = 0;
#endif

// vars used with game status cvars
int gsvInLevel = 0;
int gsvCurrentMusic = 0;
int gsvMapMusic = -1;

int gsvArmor = 0;
int gsvHealth = 0;

#if !__JHEXEN__
int gsvKills = 0;
int gsvItems = 0;
int gsvSecrets = 0;
#endif

int gsvCurrentWeapon;
int gsvWeapons[NUM_WEAPON_TYPES];
int gsvKeys[NUM_KEY_TYPES];
int gsvAmmo[NUM_AMMO_TYPES];

char *gsvMapName = NOTAMAPNAME;

#if __JHERETIC__ || __JHEXEN__
int gsvArtifacts[NUMARTIFACTS];
#endif

#if __JHEXEN__
int gsvWPieces[4];
#endif

static gamestate_t gamestate = GS_DEMOSCREEN;

cvar_t gamestatusCVars[] =
{
   {"game-state", READONLYCVAR, CVT_INT, &gamestate, 0, 0},
   {"game-state-level", READONLYCVAR, CVT_INT, &gsvInLevel, 0, 0},
   {"game-paused", READONLYCVAR, CVT_INT, &paused, 0, 0},
   {"game-skill", READONLYCVAR, CVT_INT, &gameskill, 0, 0},

   {"map-id", READONLYCVAR, CVT_INT, &gamemap, 0, 0},
   {"map-name", READONLYCVAR, CVT_CHARPTR, &gsvMapName, 0, 0},
   {"map-episode", READONLYCVAR, CVT_INT, &gameepisode, 0, 0},
#if __JDOOM__
   {"map-mission", READONLYCVAR, CVT_INT, &gamemission, 0, 0},
#endif
#if __JHEXEN__ || __JSTRIFE__
   {"map-hub", READONLYCVAR, CVT_INT, &maphub, 0, 0},
#endif
   {"game-music", READONLYCVAR, CVT_INT, &gsvCurrentMusic, 0, 0},
   {"map-music", READONLYCVAR, CVT_INT, &gsvMapMusic, 0, 0},
#if !__JHEXEN__
   {"game-stats-kills", READONLYCVAR, CVT_INT, &gsvKills, 0, 0},
   {"game-stats-items", READONLYCVAR, CVT_INT, &gsvItems, 0, 0},
   {"game-stats-secrets", READONLYCVAR, CVT_INT, &gsvSecrets, 0, 0},
#endif

   {"player-health", READONLYCVAR, CVT_INT, &gsvHealth, 0, 0},
   {"player-armor", READONLYCVAR, CVT_INT, &gsvArmor, 0, 0},
   {"player-weapon-current", READONLYCVAR, CVT_INT, &gsvCurrentWeapon, 0, 0},

#if __JDOOM__
   // Ammo
   {"player-ammo-bullets", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CLIP], 0, 0},
   {"player-ammo-shells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_SHELL], 0, 0},
   {"player-ammo-cells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CELL], 0, 0},
   {"player-ammo-missiles", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MISSILE], 0, 0},
   // Weapons
   {"player-weapon-fist", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-pistol", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-shotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-chaingun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   {"player-weapon-mlauncher", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0},
   {"player-weapon-plasmarifle", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0},
   {"player-weapon-bfg", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0},
   {"player-weapon-chainsaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0},
   {"player-weapon-sshotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_NINETH], 0, 0},
   // Keys
   {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUECARD], 0, 0},
   {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWCARD], 0, 0},
   {"player-key-red", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDCARD], 0, 0},
   {"player-key-blueskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUESKULL], 0, 0},
   {"player-key-yellowskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWSKULL], 0, 0},
   {"player-key-redskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDSKULL], 0, 0},
#elif __JHERETIC__
   // Ammo
   {"player-ammo-goldwand", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CRYSTAL], 0, 0},
   {"player-ammo-crossbow", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ARROW], 0, 0},
   {"player-ammo-dragonclaw", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ORB], 0, 0},
   {"player-ammo-hellstaff", READONLYCVAR, CVT_INT, &gsvAmmo[AT_RUNE], 0, 0},
   {"player-ammo-phoenixrod", READONLYCVAR, CVT_INT, &gsvAmmo[AT_FIREORB], 0, 0},
   {"player-ammo-mace", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MSPHERE], 0, 0},
    // Weapons
   {"player-weapon-staff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-goldwand", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-crossbow", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-dragonclaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   {"player-weapon-hellstaff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0},
   {"player-weapon-phoenixrod", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0},
   {"player-weapon-mace", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0},
   {"player-weapon-gauntlets", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0},
   // Keys
   {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOW], 0, 0},
   {"player-key-green", READONLYCVAR, CVT_INT, &gsvKeys[KT_GREEN], 0, 0},
   {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUE], 0, 0},
   // Artifacts
   {"player-artifact-ring", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_invulnerability], 0, 0},
   {"player-artifact-shadowsphere", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_invisibility], 0, 0},
   {"player-artifact-crystalvial", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_health], 0, 0},
   {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_superhealth], 0, 0},
   {"player-artifact-tomeofpower", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_tomeofpower], 0, 0},
   {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_torch], 0, 0},
   {"player-artifact-firebomb", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_firebomb], 0, 0},
   {"player-artifact-egg", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_egg], 0, 0},
   {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_fly], 0, 0},
   {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_teleport], 0, 0},
#elif __JHEXEN__
   // Mana
   {"player-mana-blue", READONLYCVAR, CVT_INT, &gsvAmmo[AT_BLUEMANA], 0, 0},
   {"player-mana-green", READONLYCVAR, CVT_INT, &gsvAmmo[AT_GREENMANA], 0, 0},
   // Keys
   {"player-key-steel", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY1], 0, 0},
   {"player-key-cave", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY2], 0, 0},
   {"player-key-axe", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY3], 0, 0},
   {"player-key-fire", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY4], 0, 0},
   {"player-key-emerald", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY5], 0, 0},
   {"player-key-dungeon", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY6], 0, 0},
   {"player-key-silver", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY7], 0, 0},
   {"player-key-rusted", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY8], 0, 0},
   {"player-key-horn", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY9], 0, 0},
   {"player-key-swamp", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYA], 0, 0},
   {"player-key-castle", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYB], 0, 0},
   // Weapons
   {"player-weapon-first", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-second", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-third", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-fourth", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   // Weapon Pieces
   {"player-weapon-piece1", READONLYCVAR, CVT_INT, &gsvWPieces[0], 0, 0},
   {"player-weapon-piece2", READONLYCVAR, CVT_INT, &gsvWPieces[1], 0, 0},
   {"player-weapon-piece3", READONLYCVAR, CVT_INT, &gsvWPieces[2], 0, 0},
   {"player-weapon-allpieces", READONLYCVAR, CVT_INT, &gsvWPieces[3], 0, 0},
   // Artifacts
   {"player-artifact-defender", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_invulnerability], 0, 0},
   {"player-artifact-quartzflask", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_health], 0, 0},
   {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_superhealth], 0, 0},
   {"player-artifact-mysticambit", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_healingradius], 0, 0},
   {"player-artifact-darkservant", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_summon], 0, 0},
   {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_torch], 0, 0},
   {"player-artifact-porkalator", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_egg], 0, 0},
   {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_fly], 0, 0},
   {"player-artifact-repulsion", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_blastradius], 0, 0},
   {"player-artifact-flechette", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_poisonbag], 0, 0},
   {"player-artifact-banishment", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_teleportother], 0, 0},
   {"player-artifact-speed", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_speed], 0, 0},
   {"player-artifact-might", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_boostmana], 0, 0},
   {"player-artifact-bracers", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_boostarmor], 0, 0},
   {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_teleport], 0, 0},
   {"player-artifact-skull", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzskull], 0, 0},
   {"player-artifact-heart", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgembig], 0, 0},
   {"player-artifact-ruby", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemred], 0, 0},
   {"player-artifact-emerald1", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemgreen1], 0, 0},
   {"player-artifact-emerald2", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemgreen2], 0, 0},
   {"player-artifact-sapphire1", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemblue1], 0, 0},
   {"player-artifact-sapphire2", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgemblue2], 0, 0},
   {"player-artifact-daemoncodex", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzbook1], 0, 0},
   {"player-artifact-liberoscura", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzbook2], 0, 0},
   {"player-artifact-flamemask", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzskull2], 0, 0},
   {"player-artifact-glaiveseal", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzfweapon], 0, 0},
   {"player-artifact-holyrelic", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzcweapon], 0, 0},
   {"player-artifact-sigilmagus", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzmweapon], 0, 0},
   {"player-artifact-gear1", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear1], 0, 0},
   {"player-artifact-gear2", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear2], 0, 0},
   {"player-artifact-gear3", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear3], 0, 0},
   {"player-artifact-gear4", READONLYCVAR, CVT_INT, &gsvArtifacts[arti_puzzgear4], 0, 0},
#endif
   {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static skillmode_t d_skill;
static int d_episode;
static int d_map;

#if __JHEXEN__ || __JSTRIFE__
static int GameLoadSlot;
#endif

// CODE --------------------------------------------------------------------

void G_Register(void)
{
    int         i;

    for(i = 0; gamestatusCVars[i].name; ++i)
        Con_AddVariable(gamestatusCVars + i);
}

/**
 * Common Pre Engine Initialization routine.
 * Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_PreInit(void)
{
    int         i;

    // Make sure game.dll isn't newer than Doomsday...
    if(gi.version < DOOMSDAY_VERSION)
        Con_Error(GAMENAMETEXT " requires at least Doomsday " DOOMSDAY_VERSION_TEXT
                  "!\n");
#ifdef TIC_DEBUG
    rndDebugfile = fopen("rndtrace.txt", "wt");
#endif

    verbose = ArgExists("-verbose");

    // Setup the DGL interface.
    G_InitDGL();

    // Setup the players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].plr = DD_GetPlayer(i);
        players[i].plr->extradata = (void *) &players[i];
    }

    DD_SetConfigFile( CONFIGFILE );
    DD_SetDefsFile( DEFSFILE );
    R_SetDataPath( DATAPATH );

    R_SetBorderGfx(borderLumps);

    DD_SetVariable(DD_SKYFLAT_NAME, SKYFLATNAME);
    Con_SetString("map-name", NOTAMAPNAME, 1);

    G_RegisterBindClasses();
    G_RegisterPlayerControls();
    P_RegisterCustomMapProperties();

    // Add the cvars and ccmds to the console databases
    G_ConsoleRegistration();    // main command list
    D_NetConsoleRegistration(); // for network
    G_Register();               // read-only game status cvars (for playsim)
    G_ControlRegister();        // for controls/input
    AM_Register();              // for the automap
    MN_Register();              // for the menu
    HUMsg_Register();           // for the message buffer/chat widget
    ST_Register();              // for the hud/statusbar
    X_Register();               // for the crosshair

    DD_AddStartupWAD( STARTUPPK3 );
    DetectIWADs();
}

/**
 * Common Post Engine Initialization routine.
 * Game-specific post init actions should be placed in eg D_PostInit()
 * (for jDoom) and NOT here.
 */
void G_PostInit(void)
{
    // Init the save system and create the game save directory
    SV_Init();

#ifndef __JHEXEN__
    XG_ReadTypes();
    XG_Register();              // register XG classnames
#endif

    G_DefaultBindings();
    R_SetViewSize(cfg.screenblocks, 0);
    G_SetGlowing();

    Con_Message("P_Init: Init Playloop state.\n");
    P_Init();

    Con_Message("HU_Init: Setting up heads up display.\n");
    HU_Init();

    Con_Message("ST_Init: Init status bar.\n");
    ST_Init();

    cht_Init();

    Con_Message("MN_Init: Init miscellaneous info.\n");
    MN_Init();

    // Create the various line lists (spechits, anims, buttons etc).
    spechit = P_CreateIterList();
    linespecials = P_CreateIterList();
}

/**
 * Retrieve the current game state.
 *
 * @return              The current game state.
 */
gamestate_t G_GetGameState(void)
{
    return gamestate;
}

/**
 * Change the game's state.
 *
 * @param state         The state to change to.
 */
void G_ChangeGameState(gamestate_t state)
{
    gamestate = state;
}

/**
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle(void)
{

    char       *name = "title";
    void       *script;

    G_StopDemo();
    usergame = false;

    // The title script must always be defined.
    if(!Def_Get(DD_DEF_FINALE, name, &script))
    {
        Con_Error("G_StartTitle: Script \"%s\" not defined.\n", name);
    }

    FI_Start(script, FIMODE_LOCAL);
}

void G_DoLoadLevel(void)
{
    int         i;
    char       *lname, *ptr;

#if __JHEXEN__ || __JSTRIFE__
    static int firstFragReset = 1;
#endif

    levelstarttic = gametic;    // for time calculation
    G_ChangeGameState(GS_LEVEL);

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(players[i].plr->ingame && players[i].playerstate == PST_DEAD)
            players[i].playerstate = PST_REBORN;
#if __JHEXEN__ || __JSTRIFE__
        if(!IS_NETGAME || (IS_NETGAME != 0 && deathmatch != 0) ||
            firstFragReset == 1)
        {
            memset(players[i].frags, 0, sizeof(players[i].frags));
            firstFragReset = 0;
        }
#else
        memset(players[i].frags, 0, sizeof(players[i].frags));
#endif
    }

#if __JHEXEN__ || __JSTRIFE__
    SN_StopAllSequences();
#endif

    // Set all player mobjs to NULL, clear control state toggles etc.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].plr->mo = NULL;
        G_ResetLookOffset(i);
    }

    P_SetupLevel(gameepisode, gamemap, 0, gameskill);
    Set(DD_DISPLAYPLAYER, consoleplayer);   // view the guy you are playing
    gameaction = GA_NONE;

    Z_CheckHeap();

    // clear cmd building stuff
    G_ResetMousePos();
    sendpause = paused = false;

    G_ControlReset(-1); // Clear all controls for all local players.

    // set the game status cvar for map name
    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    if(lname)
    {
        ptr = strchr(lname, ':');   // Skip the E#M# or Level #.
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
    }
#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name
    if(!lname)
        lname = P_GetMapName(gamemap);
#endif

    // Set the map name
    // If still no name, call it unnamed.
    if(!lname)
    {
        Con_SetString("map-name", UNNAMEDMAP, 1);
    }
    else
    {
        Con_SetString("map-name", lname, 1);
    }

    // Start a briefing, if there is one.
    FI_Briefing(gameepisode, gamemap);
}

/*
 * Get info needed to make ticcmd_ts for the players.
 * Return false if the event should be checked for bindings.
 */
boolean G_Responder(event_t *ev)
{
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    // DJS - Why is this here??
    player_t *plr = &players[consoleplayer];

    if(!actions[A_USEARTIFACT].on)
    {                           // flag to denote that it's okay to use an artifact
        if(!ST_IsInventoryVisible())
        {
            plr->readyArtifact = plr->inventory[plr->inv_ptr].type;
        }
        usearti = true;
    }

#endif
    // any key/button down pops up menu if in demos
    if(gameaction == GA_NONE && !singledemo && !menuactive &&
       (Get(DD_PLAYBACK) || FI_IsMenuTrigger(ev)))
    {
        if(ev->state == EVS_DOWN &&
           (ev->type == EV_KEY || ev->type == EV_MOUSE_BUTTON ||
            ev->type == EV_JOY_BUTTON))
        {
            M_StartMenu();
            return true;
        }
        return false;
    }

    // With the menu active, none of these should respond to input events.
    if(!menuactive)
    {
        // Try Infine
        if(FI_Responder(ev))
            return true;

        // Try the chatmode responder
        if(HUMsg_Responder(ev))
            return true;

        // Check for cheats
        if(cht_Responder(ev))
            return true;
    }

    // Try the edit responder
    if(M_EditResponder(ev))
        return true;

    // We may wish to eat the event depending on type...
    if(G_AdjustControlState(ev))
        return true;

    // The event wasn't used.
    return false;
}

/**
 * Updates the game status cvars based on game and player data.
 * Called each tick by G_Ticker().
 */
void G_UpdateGSVarsForPlayer(player_t *pl)
{
    int         i;

    if(!pl)
        return;

    gsvHealth = pl->health;
#if !__JHEXEN__
    // Level stats
    gsvKills = pl->killcount;
    gsvItems = pl->itemcount;
    gsvSecrets = pl->secretcount;
#endif
        // armor
#if __JHEXEN__
    gsvArmor = FixedDiv(PCLASS_INFO(pl->class)->autoarmorsave +
                        pl->armorpoints[ARMOR_ARMOR] +
                        pl->armorpoints[ARMOR_SHIELD] +
                        pl->armorpoints[ARMOR_HELMET] +
                        pl->armorpoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
#else
    gsvArmor = pl->armorpoints;
#endif
    // Owned keys
    for(i = 0; i < NUM_KEY_TYPES; ++i)
#if __JHEXEN__
        gsvKeys[i] = (pl->keys & (1 << i))? 1 : 0;
#else
        gsvKeys[i] = pl->keys[i];
#endif
    // current weapon
    gsvCurrentWeapon = pl->readyweapon;

    // owned weapons
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        gsvWeapons[i] = pl->weaponowned[i];

#if __JHEXEN__
    // weapon pieces
    gsvWPieces[0] = (pl->pieces & WPIECE1)? 1 : 0;
    gsvWPieces[1] = (pl->pieces & WPIECE2)? 1 : 0;
    gsvWPieces[2] = (pl->pieces & WPIECE3)? 1 : 0;
    gsvWPieces[3] = (pl->pieces == 7)? 1 : 0;
#endif
    // current ammo amounts
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        gsvAmmo[i] = pl->ammo[i];

#if __JHERETIC__ || __JHEXEN__
    // artifacts
    for(i = 0; i < NUMINVENTORYSLOTS; ++i)
        gsvArtifacts[pl->inventory[i].type] = pl->inventory[i].count;
#endif
}

/**
 * The core of the game timing loop.
 * Game state, game actions etc occur here.
 */
void G_Ticker(void)
{
    int         i;
    player_t   *plyr = &players[consoleplayer];
    static gamestate_t oldgamestate = -1;

    if(IS_CLIENT && !Get(DD_GAME_READY))
        return;

#if _DEBUG
Z_CheckHeap();
#endif
    // do player reborns if needed
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->ingame && players[i].playerstate == PST_REBORN)
            G_DoReborn(i);

        // Player has left?
        if(players[i].playerstate == PST_GONE)
        {
            players[i].playerstate = PST_REBORN;
            if(players[i].plr->mo)
            {
                if(!IS_CLIENT)
                {
                    P_SpawnTeleFog(players[i].plr->mo->pos[VX],
                                   players[i].plr->mo->pos[VY]);
                }
                // Let's get rid of the mobj.
#ifdef _DEBUG
Con_Message("G_Ticker: Removing player %i's mobj.\n", i);
#endif
                P_RemoveMobj(players[i].plr->mo);
                players[i].plr->mo = NULL;
            }
        }
    }

    // do things to change the game state
    while(gameaction != GA_NONE)
    {
        switch(gameaction)
        {
#if __JHEXEN__ || __JSTRIFE__
        case GA_INITNEW:
            G_DoInitNew();
            break;
        case GA_SINGLEREBORN:
            G_DoSingleReborn();
            break;
        case GA_LEAVEMAP:
            Draw_TeleportIcon();
            G_DoTeleportNewMap();
            break;
#endif
        case GA_LOADLEVEL:
            G_DoLoadLevel();
            break;

        case GA_NEWGAME:
            G_DoNewGame();
            break;

        case GA_LOADGAME:
            G_DoLoadGame();
            break;

        case GA_SAVEGAME:
            G_DoSaveGame();
            break;

        case GA_PLAYDEMO:
            G_DoPlayDemo();
            break;

        case GA_COMPLETED:
            G_DoCompleted();
            break;

        case GA_VICTORY:
            gameaction = GA_NONE;
            break;

        case GA_WORLDDONE:
            G_DoWorldDone();
            break;

        case GA_SCREENSHOT:
            G_DoScreenShot();
            gameaction = GA_NONE;
            break;

        case GA_NONE:
        default:
            break;
        }
    }

    // Update the viewer's look angle
    G_LookAround(consoleplayer);

    // Enable/disable sending of frames (delta sets) to clients.
    Set(DD_ALLOW_FRAMES, G_GetGameState() == GS_LEVEL);
    if(!IS_CLIENT)
    {
        // Tell Doomsday when the game is paused (clients can't pause
        // the game.)
        Set(DD_CLIENT_PAUSED, P_IsPaused());
    }

    // Must be called on every tick.
    P_RunPlayers();

    // Do main actions.
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        // update in-level game status cvar
        if(oldgamestate != GS_LEVEL)
            gsvInLevel = 1;

        P_DoTick();

        HU_UpdatePsprites();

        // Active briefings once again (they were disabled when loading
        // a saved game).
        brief_disabled = false;

        if(IS_DEDICATED)
            break;

        ST_Ticker();
        AM_Ticker();
        HU_Ticker();
        break;

    case GS_INTERMISSION:
#if __JDOOM__
        WI_Ticker();
#else
        IN_Ticker();
#endif

    default:
        if(oldgamestate != G_GetGameState())
        {
            // update game status cvars
            gsvInLevel = 0;
            Con_SetString("map-name", NOTAMAPNAME, 1);
            gsvMapMusic = -1;
        }
        break;
    }

    oldgamestate = gamestate;

    // Update the game status cvars for player data
    G_UpdateGSVarsForPlayer(plyr);

    // Update view window size.
    R_ViewWindowTicker();

    // InFine ticks whenever it's active.
    FI_Ticker();

    // Servers will have to update player information and do such stuff.
    if(!IS_CLIENT)
        NetSv_Ticker();
}

/*
 * Called at the start.
 * Called by the game initialization functions.
 */
void G_InitPlayer(int player)
{
    player_t *p;

    // set up the saved info
    p = &players[player];

    // clear everything else to defaults
    G_PlayerReborn(player);
}

/*
 * Called when a player exits a level.
 *
 * Jobs include; striping keys, artifacts and powers from the player
 * and configuring other player-specific properties ready for the next
 * level.
 *
 * @param player        Id of the player to configure.
 */
void G_PlayerExitMap(int player)
{
#if !__JDOOM__
    int     i;
#endif
#if __JHEXEN__ || __JSTRIFE__
    int     flightPower;
#endif
    player_t *p = &players[player];
    boolean newCluster;

#if __JHEXEN__ || __JSTRIFE__
    newCluster = (P_GetMapCluster(gamemap) != P_GetMapCluster(LeaveMap));
#else
    newCluster = true;
#endif

#if __JHERETIC__
    // Empty the player's inventory.
    for(i = 0; i < p->inventorySlotNum; i++)
    {
        p->inventory[i].count = 1;
    }
    p->artifactCount = p->inventorySlotNum;
#endif

    // Remember if flying
#if __JHEXEN__ || __JSTRIFE__
    flightPower = p->powers[PT_FLIGHT];
#endif

#if !__JDOOM__
    // Strip flight artifacts?
    if(!deathmatch && newCluster) // Entering new cluster
    {
        p->powers[PT_FLIGHT] = 0;

        for(i = 0; i < MAXARTICOUNT; i++)
            P_InventoryUseArtifact(p, arti_fly);
    }
#endif
    // Remove their powers.
    p->update |= PSF_POWERS;
    memset(p->powers, 0, sizeof(p->powers));

#if __JHEXEN__ || __JSTRIFE__
    if(!newCluster && !deathmatch)
        p->powers[PT_FLIGHT] = flightPower; // restore flight.
#endif

    // Remove their keys.
#if __JDOOM__ || __JHERETIC__
    p->update |= PSF_KEYS;
    memset(p->keys, 0, sizeof(p->keys));
#else
    if(!deathmatch && newCluster)
        p->keys = 0;
#endif

    // Misc
#if __JHERETIC__
    playerkeys = 0;

    p->rain1 = NULL;
    p->rain2 = NULL;
#endif

    // Un-morph?
#if __JHERETIC__ || __JHEXEN__
    p->update |= PSF_MORPH_TIME;
    if(p->morphTics)
    {
        p->readyweapon = p->plr->mo->special1;    // Restore weapon
        p->morphTics = 0;
    }
#endif

    p->plr->lookdir = 0;
    p->plr->mo->flags &= ~MF_SHADOW;    // cancel invisibility
    p->plr->extralight = 0;     // cancel gun flashes
    p->plr->fixedcolormap = 0;  // cancel ir gogles

    // Clear filter.
    p->plr->filter = 0;
    p->plr->flags |= DDPF_FILTER;
    p->damagecount = 0;         // no palette changes
    p->bonuscount = 0;

#if __JHEXEN__ || __JSTRIFE__
    p->poisoncount = 0;
#endif

    HUMsg_ClearMessages(p);
}

/*
 * Safely clears the player data structures.
 */
void ClearPlayer(player_t *p)
{
    ddplayer_t *ddplayer = p->plr;
    int     playeringame = ddplayer->ingame;
    int     flags = ddplayer->flags;
    int     start = p->startspot;
    fixcounters_t counter, acked;

    // Restore counters.
    counter = ddplayer->fixcounter;
    acked = ddplayer->fixacked;

    memset(p, 0, sizeof(*p));
    // Restore the pointer to ddplayer.
    p->plr = ddplayer;
    // Also clear ddplayer.
    memset(ddplayer, 0, sizeof(*ddplayer));
    // Restore the pointer to this player.
    ddplayer->extradata = p;
    // Restore the playeringame data.
    ddplayer->ingame = playeringame;
    ddplayer->flags = flags & ~(DDPF_INTERYAW | DDPF_INTERPITCH);
    // Don't clear the start spot.
    p->startspot = start;
    // Restore counters.
    ddplayer->fixcounter = counter;
    ddplayer->fixacked = acked;

    ddplayer->fixcounter.angles++;
    ddplayer->fixcounter.pos++;
    ddplayer->fixcounter.mom++;

/*    ddplayer->fixacked.angles =
        ddplayer->fixacked.pos =
        ddplayer->fixacked.mom = -1;
#ifdef _DEBUG
    Con_Message("ClearPlayer: fixacked set to -1 (counts:%i, %i, %i)\n",
                ddplayer->fixcounter.angles,
                ddplayer->fixcounter.pos,
                ddplayer->fixcounter.mom);
#endif*/
}

/*
 * Called after a player dies
 * almost everything is cleared and initialized
 */
void G_PlayerReborn(int player)
{
    player_t *p;
    int     frags[MAXPLAYERS];
    int     killcount;
    int     itemcount;
    int     secretcount;

#if __JDOOM__ || __JHERETIC__
    int     i;
#endif
#if __JHERETIC__
    boolean secret = false;
    int     spot;
#elif __JHEXEN__ || __JSTRIFE__
    uint    worldTimer;
#endif

    memcpy(frags, players[player].frags, sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;
#if __JHEXEN__ || __JSTRIFE__
    worldTimer = players[player].worldTimer;
#endif

    p = &players[player];
#if __JHERETIC__
    if(p->didsecret)
        secret = true;
    spot = p->startspot;
#endif

    // Clears (almost) everything.
    ClearPlayer(p);

#if __JHERETIC__
    p->startspot = spot;
#endif

    memcpy(players[player].frags, frags, sizeof(players[player].frags));
    players[player].killcount = killcount;
    players[player].itemcount = itemcount;
    players[player].secretcount = secretcount;
#if __JHEXEN__ || __JSTRIFE__
    players[player].worldTimer = worldTimer;
    players[player].colormap = cfg.PlayerColor[player];
#endif
#if __JHEXEN__
    players[player].class = cfg.PlayerClass[player];
#endif
    p->usedown = p->attackdown = true;  // don't do anything immediately
    p->playerstate = PST_LIVE;
    p->health = MAXHEALTH;

#if __JDOOM__
    p->readyweapon = p->pendingweapon = WT_SECOND;
    p->weaponowned[WT_FIRST] = true;
    p->weaponowned[WT_SECOND] = true;
    p->ammo[AT_CLIP] = 50;

    // See if the Values specify anything.
    P_InitPlayerValues(p);

#elif __JHERETIC__
    p->readyweapon = p->pendingweapon = WT_SECOND;
    p->weaponowned[WT_FIRST] = true;
    p->weaponowned[WT_SECOND] = true;
    p->ammo[AT_CRYSTAL] = 50;

    if(gamemap == 9 || secret)
    {
        p->didsecret = true;
    }

#else
    p->readyweapon = p->pendingweapon = WT_FIRST;
    p->weaponowned[WT_FIRST] = true;
    localQuakeHappening[player] = false;
#endif

#if __JDOOM__ || __JHERETIC__
    // Reset maxammo.
    for(i = 0; i < NUM_AMMO_TYPES; i++)
        p->maxammo[i] = maxammo[i];
#endif

#if !__JDOOM__
    P_InventoryResetCursor(p);
    if(p == &players[consoleplayer])
    {
#  if __JHEXEN__ || __JSTRIFE__
        SB_state = -1;          // refresh the status bar
#  endif
    }
#endif

    // We'll need to update almost everything.
#if __JHERETIC__
    p->update |=
        PSF_STATE | PSF_HEALTH | PSF_ARMOR_TYPE | PSF_ARMOR_POINTS |
        PSF_INVENTORY | PSF_POWERS | PSF_KEYS | PSF_OWNED_WEAPONS | PSF_AMMO |
        PSF_MAX_AMMO | PSF_PENDING_WEAPON | PSF_READY_WEAPON;
#else
    p->update |= PSF_REBORN;
#endif

    p->plr->flags &= ~DDPF_DEAD;
}

#ifdef __JDOOM__
void G_QueueBody(mobj_t *body)
{
    // flush an old corpse if needed
    if(bodyqueslot >= BODYQUESIZE)
        P_RemoveMobj(bodyque[bodyqueslot % BODYQUESIZE]);
    bodyque[bodyqueslot % BODYQUESIZE] = body;
    bodyqueslot++;
}
#endif

void G_DoReborn(int playernum)
{
#if __JHEXEN__ || __JSTRIFE__
    int     i;
    boolean oldWeaponowned[NUM_WEAPON_TYPES];
    int     oldKeys;
    int     oldPieces;
    int     bestWeapon;
#endif
    boolean foundSpot;
    thing_t *assigned;

    // Clear the currently playing script, if any.
    FI_Reset();

    if(!IS_NETGAME)
    {
        // We've just died, don't do a briefing now.
        brief_disabled = true;

#if __JHEXEN__ || __JSTRIFE__
        if(SV_HxRebornSlotAvailable())
        {   // Use the reborn code if the slot is available
            gameaction = GA_SINGLEREBORN;
        }
        else
        {   // Start a new game if there's no reborn info
            gameaction = GA_NEWGAME;
        }
#else
        // reload the level from scratch
        gameaction = GA_LOADLEVEL;
#endif
    }
    else                        // Netgame
    {
        if(players[playernum].plr->mo)
        {
            // first dissasociate the corpse
            players[playernum].plr->mo->player = NULL;
            players[playernum].plr->mo->dplayer = NULL;
        }

        if(IS_CLIENT)
        {
            if(G_GetGameState() == GS_LEVEL) 
            {
                G_DummySpawnPlayer(playernum);
            }
            return;
        }

        Con_Printf("G_DoReborn for %i.\n", playernum);

        // spawn at random spot if in death match
        if(deathmatch)
        {
            G_DeathMatchSpawnPlayer(playernum);
            return;
        }

#if __JHEXEN__ || __JSTRIFE__
        // Cooperative net-play, retain keys and weapons
        oldKeys = players[playernum].keys;
        oldPieces = players[playernum].pieces;
        for(i = 0; i < NUM_WEAPON_TYPES; i++)
            oldWeaponowned[i] = players[playernum].weaponowned[i];
#endif

        // Try to spawn at the assigned spot.
        foundSpot = false;
        assigned = P_GetPlayerStart(
#if __JHEXEN__ || __JSTRIFE__
                                    RebornPosition,
#else
                                    0,
#endif
                                    playernum);
        if(P_CheckSpot(playernum, assigned, true))
        {
            // Appropriate player start spot is open
            Con_Printf("- spawning at assigned spot\n");
            P_SpawnPlayer(assigned, playernum);
            foundSpot = true;
        }
#if __JDOOM__ || __JHERETIC__
        else
        {
            Con_Printf("- force spawning at %i.\n", players[playernum].startspot);

            // Fuzzy returns false if it needs telefragging.
            if(!P_FuzzySpawn(assigned, playernum, true))
            {
                // Spawn at the assigned spot, telefrag whoever's there.
                P_Telefrag(players[playernum].plr->mo);
            }
        }
#else
        else
        {
            // Try to spawn at one of the other player start spots
            for(i = 0; i < MAXPLAYERS; i++)
            {
                if(P_CheckSpot
                   (playernum, P_GetPlayerStart(RebornPosition, i), true))
                {
                    // Found an open start spot
                    P_SpawnPlayer(P_GetPlayerStart(RebornPosition, i),
                                  playernum);
                    foundSpot = true;
                    break;
                }
            }
        }
        if(!foundSpot)
        {
            // Player's going to be inside something
            P_SpawnPlayer(P_GetPlayerStart(RebornPosition, playernum),
                          playernum);
        }

        // Restore keys and weapons
        players[playernum].keys = oldKeys;
        players[playernum].pieces = oldPieces;
        for(bestWeapon = 0, i = 0; i < NUM_WEAPON_TYPES; i++)
        {
            if(oldWeaponowned[i])
            {
                bestWeapon = i;
                players[playernum].weaponowned[i] = true;
            }
        }

        players[playernum].ammo[AT_BLUEMANA] = 25;
        players[playernum].ammo[AT_GREENMANA] = 25;
        if(bestWeapon)
        {                       // Bring up the best weapon
            players[playernum].pendingweapon = bestWeapon;
        }
#endif
    }
}

#if __JHEXEN__ || __JSTRIFE__
void G_StartNewInit(void)
{
    SV_HxInitBaseSlot();
    SV_HxClearRebornSlot();

# if __JHEXEN__
    P_ACSInitNewGame();
# endif

    // Default the player start spot group to 0
    RebornPosition = 0;
}

void G_StartNewGame(skillmode_t skill)
{
    int     realMap = 1;

    G_StartNewInit();
#   if __JHEXEN__
    realMap = P_TranslateMap(1);
#   elif __JSTRIFE__
    realMap = 1;
#   endif
    if(realMap == -1)
    {
        realMap = 1;
    }
    G_InitNew(d_skill, 1, realMap);
}

/*
 * Only called by the warp cheat code.  Works just like normal map to map
 * teleporting, but doesn't do any interlude stuff.
 */
void G_TeleportNewMap(int map, int position)
{
    gameaction = GA_LEAVEMAP;
    LeaveMap = map;
    LeavePosition = position;
}

void G_DoTeleportNewMap(void)
{
    // Clients trust the server in these things.
    if(IS_CLIENT)
    {
        gameaction = GA_NONE;
        return;
    }

    SV_HxMapTeleport(LeaveMap, LeavePosition);
    G_ChangeGameState(GS_LEVEL);
    gameaction = GA_NONE;
    RebornPosition = LeavePosition;

    // Is there a briefing before this map?
    FI_Briefing(gameepisode, gamemap);
}
#endif

/*
 * Leave the current level and start intermission routine.
 * (if __JHEXEN__ the intermission will only be displayed when exiting a
 * hub and in DeathMatch games)
 *
 * @param map:          Map id of the level we are leaving.
 * @param position:     Position id (maps with multiple entry/exit points).
 * @param secret:       (TRUE) if this is a secret exit point.
 */
void G_LeaveLevel(int map, int position, boolean secret)
{
    if(cyclingMaps && mapCycleNoExit)
        return;

#if __JHEXEN__
    if(shareware && map > 4)
    {
        // Not possible in the 4-level demo.
        P_SetMessage(&players[consoleplayer], "PORTAL INACTIVE -- DEMO", false);
        return;
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
    LeaveMap = map;
    LeavePosition = position;
#else
    secretexit = secret;
  #if __JDOOM__
  # if !__DOOM64TC__
      // IF NO WOLF3D LEVELS, NO SECRET EXIT!
      if(secret && (gamemode == commercial) && (W_CheckNumForName("map31") < 0))
          secretexit = false;
  # endif
  #endif
#endif

    gameaction = GA_COMPLETED;
}

/*
 * @return boolean  (True) If the game has been completed
 */
boolean G_IfVictory(void)
{
#if __DOOM64TC__
    if((gameepisode == 1 && gamemap == 30) ||
       (gameepisode == 2 && gamemap == 7))
    {
        gameaction = GA_VICTORY;
        return true;
    }
#elif __JDOOM__
    if((gamemap == 8) && (gamemode != commercial))
    {
        gameaction = GA_VICTORY;
        return true;
    }

#elif __JHERETIC__
    if(gamemap == 8)
    {
        gameaction = GA_VICTORY;
        return true;
    }

#elif __JHEXEN__ || __JSTRIFE__
    if(LeaveMap == -1 && LeavePosition == -1)
    {
        gameaction = GA_VICTORY;
        return true;
    }
#endif

    return false;
}

void G_DoCompleted(void)
{
    int     i;

#if __JHERETIC__
    static int afterSecret[5] = { 7, 5, 5, 5, 4 };
#endif

    // Clear the currently playing script, if any.
    FI_Reset();

    // Is there a debriefing for this map?
    if(FI_Debriefing(gameepisode, gamemap))
        return;

    gameaction = GA_NONE;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(players[i].plr->ingame)
        {
            G_PlayerExitMap(i); // take away cards and stuff

            // Update this client's stats.
            NetSv_SendPlayerState(i, DDSP_ALL_PLAYERS,
                                  PSF_FRAGS | PSF_COUNTERS, true);
        }
    }

    if(automapactive)
        AM_Stop();

    // Has the player completed the game?
    if(G_IfVictory())
        return; // Victorious!

#if __JHERETIC__
    prevmap = gamemap;
    if(secretexit == true)
    {
        gamemap = 9;
    }
    else if(gamemap == 9)
    {                           // Finished secret level
        gamemap = afterSecret[gameepisode - 1];
    }
    else
    {
        // Is there an overide for nextmap? (eg from an XG line)
        if(nextmap > 0)
            gamemap = nextmap;

        gamemap++;
    }
#endif

#if __JDOOM__
# if !__DOOM64TC__
    if(gamemode != commercial && gamemap == 9)
    {
        for(i = 0; i < MAXPLAYERS; i++)
            players[i].didsecret = true;
    }
# endif

    wminfo.didsecret = players[consoleplayer].didsecret;
    wminfo.last = gamemap - 1;

# if __DOOM64TC__
    if(secretexit)
    {
        if(gameepisode = 1)
        {
            switch(gamemap)
            {
            case 15: wminfo.next = 30; break;
            case 31: wminfo.next = 34; break;
            case 9:  wminfo.next = 33; break;
            case 1:  wminfo.next = 31; break;
            case 20: wminfo.next = 32; break;
            case 38: wminfo.next = 0; break;
            }
        }
        else // episode 2
        {
            if(gamemap == 3)
                wminfo.next = 7;
        }
    }
    else
    {
        if(gameepisode == 1)
        {
            switch(gamemap)
            {
            case 31: wminfo.next = 15; break;
            case 35: wminfo.next = 15; break;
            case 33: wminfo.next = 20; break;
            case 34: wminfo.next = 9; break;
            case 32: wminfo.next = 37; break;
            case 37: wminfo.next = 35; break;
            case 38: wminfo.next = 0; break;
            default: wminfo.next = gamemap;
            }
        }
        else
        {
            if(gamemap == 8)
                wminfo.next = 3;
            else
                wminfo.next = gamemap;
        }
    }
# else
    // wminfo.next is 0 biased, unlike gamemap
    if(gamemode == commercial)
    {
        if(secretexit)
            switch (gamemap)
            {
            case 15:
                wminfo.next = 30;
                break;
            case 31:
                wminfo.next = 31;
                break;
            }
        else
            switch (gamemap)
            {
            case 31:
            case 32:
                wminfo.next = 15;
                break;
            default:
                wminfo.next = gamemap;
            }
    }
    else
    {
        if(secretexit)
            wminfo.next = 8;    // go to secret level
        else if(gamemap == 9)
        {
            // returning from secret level
            switch (gameepisode)
            {
            case 1:
                wminfo.next = 3;
                break;
            case 2:
                wminfo.next = 5;
                break;
            case 3:
                wminfo.next = 6;
                break;
            case 4:
                wminfo.next = 2;
                break;
            }
        }
        else
            wminfo.next = gamemap;  // go to next level
    }
# endif

    // Is there an overide for wminfo.next? (eg from an XG line)
    if(nextmap > 0)
    {
        wminfo.next = nextmap -1;   // wminfo is zero based
        nextmap = 0;
    }

    wminfo.maxkills = totalkills;
    wminfo.maxitems = totalitems;
    wminfo.maxsecret = totalsecret;

    G_PrepareWIData();

    // Tell the clients what's going on.
    NetSv_Intermission(IMF_BEGIN, 0, 0);
    viewactive = false;
    automapactive = false;
#elif __JHERETIC__
    // Let the clients know the next level.
    NetSv_SendGameState(0, DDSP_ALL_PLAYERS);
#elif __JHEXEN__ || __JSTRIFE__
    NetSv_Intermission(IMF_BEGIN, LeaveMap, LeavePosition);
#endif
    G_ChangeGameState(GS_INTERMISSION);

#if __JDOOM__
    WI_Start(&wminfo);
#else
    IN_Start();
#endif
}

#if __JDOOM__
void G_PrepareWIData(void)
{
    int     i;
    ddmapinfo_t minfo;
    char    levid[8];

    wminfo.epsd = gameepisode - 1;
    wminfo.maxfrags = 0;

    P_GetMapLumpName(gameepisode, gamemap, levid);

    // See if there is a par time definition.
    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && minfo.partime > 0)
        wminfo.partime = 35 * (int) minfo.partime;
    else
        wminfo.partime = -1; // unknown

    wminfo.pnum = consoleplayer;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        wminfo.plyr[i].in = players[i].plr->ingame;
        wminfo.plyr[i].skills = players[i].killcount;
        wminfo.plyr[i].sitems = players[i].itemcount;
        wminfo.plyr[i].ssecret = players[i].secretcount;
        wminfo.plyr[i].stime = leveltime;
        memcpy(wminfo.plyr[i].frags, players[i].frags,
               sizeof(wminfo.plyr[i].frags));
    }
}
#endif

void G_WorldDone(void)
{
    gameaction = GA_WORLDDONE;

#if __JDOOM__
    if(secretexit)
        players[consoleplayer].didsecret = true;
#endif
}

void G_DoWorldDone(void)
{
    G_ChangeGameState(GS_LEVEL);
#if __JDOOM__
    gamemap = wminfo.next + 1;
#endif
    G_DoLoadLevel();
    gameaction = GA_NONE;
    viewactive = true;
}

#if __JHEXEN__ || __JSTRIFE__
/*
 * Called by G_Ticker based on gameaction.  Loads a game from the reborn
 * save slot.
 */
void G_DoSingleReborn(void)
{
    gameaction = GA_NONE;
    SV_HxLoadGame(SV_HxGetRebornSlot());
    SB_SetClassData();
}
#endif

/*
 * Can be called by the startup code or the menu task.
 */
#if __JHEXEN__ || __JSTRIFE__
void G_LoadGame(int slot)
{
    GameLoadSlot = slot;
    gameaction = GA_LOADGAME;
}
#else
void G_LoadGame(char *name)
{
    strcpy(savename, name);
    gameaction = GA_LOADGAME;
}
#endif

/*
 * Called by G_Ticker based on gameaction.
 */
void G_DoLoadGame(void)
{
    G_StopDemo();
    FI_Reset();
    gameaction = GA_NONE;

#if __JHEXEN__ || __JSTRIFE__

    Draw_LoadIcon();

    SV_HxLoadGame(GameLoadSlot);
    if(!IS_NETGAME)
    {                           // Copy the base slot to the reborn slot
        SV_HxUpdateRebornSlot();
    }
    SB_SetClassData();
#else
    SV_LoadGame(savename);
#endif
}

/*
 * Called by the menu task.
 * Description is a 24 byte text string
 */
void G_SaveGame(int slot, char *description)
{
    savegameslot = slot;
    strcpy(savedescription, description);
    gameaction = GA_SAVEGAME;
}

/*
 * Called by G_Ticker based on gameaction.
 */
void G_DoSaveGame(void)
{
#if __JHEXEN__ || __JSTRIFE__
    Draw_SaveIcon();

    SV_HxSaveGame(savegameslot, savedescription);
#else
    char    name[100];

    SV_SaveGameFile(savegameslot, name);
    SV_SaveGame(name, savedescription);
#endif

    gameaction = GA_NONE;
    savedescription[0] = 0;

    P_SetMessage(players + consoleplayer, TXT_GAMESAVED, false);
}

#if __JHEXEN__ || __JSTRIFE__
void G_DeferredNewGame(skillmode_t skill)
{
    d_skill = skill;
    gameaction = GA_NEWGAME;
}

void G_DoInitNew(void)
{
    SV_HxInitBaseSlot();
    G_InitNew(d_skill, d_episode, d_map);
    gameaction = GA_NONE;
}
#endif

/*
 * Can be called by the startup code or the menu task,
 * consoleplayer, displayplayer, playeringame[] should be set.
 */
void G_DeferedInitNew(skillmode_t skill, int episode, int map)
{
    d_skill = skill;
    d_episode = episode;
    d_map = map;

#if __JHEXEN__ || __JSTRIFE__
    gameaction = GA_INITNEW;
#else
    gameaction = GA_NEWGAME;
#endif
}

void G_DoNewGame(void)
{
    G_StopDemo();
#if __JDOOM__ || __JHERETIC__
    if(!IS_NETGAME)
    {
        deathmatch = false;
        respawnmonsters = false;
        nomonsters = ArgExists("-nomonsters");  //false;
    }
    G_InitNew(d_skill, d_episode, d_map);
#else
    G_StartNewGame(d_skill);
#endif
    gameaction = GA_NONE;
}

/*
 * Start a new game.
 */
void G_InitNew(skillmode_t skill, int episode, int map)
{
    int     i;

#if __JDOOM__ || __JHERETIC__
    int     speed;
#endif

    // If there are any InFine scripts running, they must be stopped.
    FI_Reset();

    if(paused)
    {
        paused = false;
    }

    if(skill < SM_BABY)
        skill = SM_BABY;
    if(skill > SM_NIGHTMARE)
        skill = SM_NIGHTMARE;

    // Make sure that the episode and map numbers are good.
    G_ValidateMap(&episode, &map);

    M_ClearRandom();

#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
    if(respawnparm)
        respawnmonsters = true;
    else
        respawnmonsters = false;
#endif

#if __JDOOM__ || __JHERETIC__
    // Is respawning enabled at all in nightmare skill?
    if(skill == SM_NIGHTMARE)
        respawnmonsters = cfg.respawnMonstersNightmare;
#endif

// KLUDGE:
#if __JDOOM__
    // Fast monsters?
    if(fastparm || (skill == SM_NIGHTMARE && gameskill != SM_NIGHTMARE))
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; i++)
            states[i].tics = 1;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; i++)
            states[i].tics = 4;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; i++)
            states[i].tics = 1;
    }
    else
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; i++)
            states[i].tics = 2;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; i++)
            states[i].tics = 8;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; i++)
            states[i].tics = 2;
    }
#endif

    // Fast missiles?
#if __JDOOM__ || __JHERETIC__
#   if __JDOOM__
    speed = (fastparm || (skill == SM_NIGHTMARE && gameskill != SM_NIGHTMARE));
#   else
    speed = skill == SM_NIGHTMARE;
#   endif

    for(i = 0; MonsterMissileInfo[i].type != -1; i++)
    {
        mobjinfo[MonsterMissileInfo[i].type].speed =
            MonsterMissileInfo[i].speed[speed] << FRACBITS;
    }
#endif
// <-- KLUDGE

    if(!IS_CLIENT)
    {
        // force players to be initialized upon first level load
        for(i = 0; i < MAXPLAYERS; i++)
        {
            players[i].playerstate = PST_REBORN;
#if __JHEXEN__ || __JSTRIFE__
            players[i].worldTimer = 0;
#else
            players[i].didsecret = false;
#endif
        }
    }

    usergame = true;            // will be set false if a demo
    paused = false;
    automapactive = false;
    viewactive = true;
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;
    GL_Update(DDUF_BORDER);

    NetSv_UpdateGameConfig();

    // Tell the engine if we want that all players know
    // where everybody else is.
    Set(DD_SEND_ALL_PLAYERS, !deathmatch);

    G_DoLoadLevel();

#if __JHEXEN__
    // Initialize the sky.
    P_InitSky(map);
#endif
}

/**
 * Return the index of this level.
 */
int G_GetLevelNumber(int episode, int map)
{
#if __JHEXEN__ || __JSTRIFE__
    return P_TranslateMap(map);
#elif __DOOM64TC__
    return (episode == 2? 39 + map : map); //episode1 has 40 maps
#else
  #if __JDOOM__
    if(gamemode == commercial)
        return map;
    else
  #endif
    {
        return map + episode * 9; // maps per episode.
    }
#endif
}

/**
 * Compose the name of the map lump identifier.
 */
void P_GetMapLumpName(int episode, int map, char *lumpName)
{
#if __DOOM64TC__
    sprintf(lumpName, "E%iM%02i", episode, map);
#elif __JDOOM__
    if(gamemode == commercial)
        sprintf(lumpName, "MAP%02i", map);
    else
        sprintf(lumpName, "E%iM%i", episode, map);
#elif  __JHERETIC__
    sprintf(lumpName, "E%iM%i", episode, map);
#else
    sprintf(lumpName, "MAP%02i", map);
#endif
}

/*
 * Returns true if the specified ep/map exists in a WAD.
 */
boolean P_MapExists(int episode, int map)
{
    char    buf[20];

    P_GetMapLumpName(episode, map, buf);
    return W_CheckNumForName(buf) >= 0;
}

/*
 * Returns true if the specified (episode, map) pair can be used.
 * Otherwise the values are adjusted so they are valid.
 */
boolean G_ValidateMap(int *episode, int *map)
{
    boolean ok = true;

    if(*episode < 1)
    {
        *episode = 1;
        ok = false;
    }
    if(*map < 1)
    {
        *map = 1;
        ok = false;
    }
#if __DOOM64TC__
    if(*episode > 2)
    {
        *episode = 2;
        ok = false;
    }
    if(*episode == 2)
    {
        if(*map > 7)
        {
            *map = 7;
            ok = false;
        }
    }
    else
    {
        if(*map > 40)
        {
            *map = 40;
            ok = false;
        }
    }
#elif __JDOOM__
    if(gamemode == shareware)
    {
        // only start episode 1 on shareware
        if(*episode > 1)
        {
            *episode = 1;
            ok = false;
        }
    }
    else
    {
        // Allow episodes 1-9.
        if(*episode > 9)
        {
            *episode = 9;
            ok = false;
        }
    }
    if(gamemode == commercial)
    {
        if(*map > 99)
        {
            *map = 99;
            ok = false;
        }
    }
    else
    {
        if(*map > 9)
        {
            *map = 9;
            ok = false;
        }
    }

#elif __JHERETIC__
    //  Allow episodes 1-9.
    if(*episode > 9)
    {
        *episode = 9;
        ok = false;
    }
    if(*map > 9)
    {
        *map = 9;
        ok = false;
    }

    if(gamemode == shareware) // Shareware version checks
    {
        if(*episode > 1)
        {
            *episode = 1;
            ok = false;
        }
    }
    else if(gamemode == extended) // Extended version checks
    {
        if(*episode == 6)
        {
            if(*map > 3)
            {
                *map = 3;
                ok = false;
            }
        }
        else if(*episode > 5)
        {
            *episode = 5;
            ok = false;
        }
    }
    else // Registered version checks
    {
        if(*episode == 4)
        {
            if(*map != 1)
            {
                *map = 1;
                ok = false;
            }
        }
        else if(*episode > 3)
        {
            *episode = 3;
            ok = false;
        }
    }
#elif __JHEXEN__ || __JSTRIFE__
    if(*map > 99)
    {
        *map = 99;
        ok = false;
    }
#endif

    // Check that the map truly exists.
    if(!P_MapExists(*episode, *map))
    {
        // (1,1) should exist always?
        *episode = 1;
        *map = 1;
        ok = false;
    }

    return ok;
}

#if __JHERETIC__
char *P_GetShortLevelName(int episode, int map)
{
    char   *name = P_GetLevelName(episode, map);
    char   *ptr;

    // Remove the "ExMx:" from the beginning.
    ptr = strchr(name, ':');
    if(!ptr)
        return name;
    name = ptr + 1;
    while(*name && isspace(*name))
        name++;                 // Skip any number of spaces.
    return name;
}

char   *P_GetLevelName(int episode, int map)
{
    char    id[10];
    ddmapinfo_t info;

    // Compose the level identifier.
    P_GetMapLumpName(episode, map, id);

    // Get the map info definition.
    if(!Def_Get(DD_DEF_MAP_INFO, id, &info))
    {
        // There is no map information for this map...
        return "";
    }
    return info.name;
}
#endif

void G_DeferedPlayDemo(char *name)
{
    strcpy(defdemoname, name);
    gameaction = GA_PLAYDEMO;
}

void G_DoPlayDemo(void)
{
    int     lnum = W_CheckNumForName(defdemoname);
    char   *lump;
    char    buf[128];

    gameaction = GA_NONE;

    // The lump should contain the path of the demo file.
    if(lnum < 0 || W_LumpLength(lnum) != 64)
    {
        Con_Message("G_DoPlayDemo: invalid demo lump \"%s\".\n", defdemoname);
        return;
    }

    lump = W_CacheLumpNum(lnum, PU_CACHE);
    memset(buf, 0, sizeof(buf));
    strcpy(buf, "playdemo ");
    strncat(buf, lump, 64);

    // Start playing the demo.
    if(DD_Execute(buf, false))
        G_ChangeGameState(GS_WAITING); // The demo will begin momentarily.
}

/*
 * Stops both playback and a recording. Called at critical points like
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo(void)
{
    DD_Execute("stopdemo", true);
}

void G_DemoEnds(void)
{
    G_ChangeGameState(GS_WAITING);
    if(singledemo)
        Sys_Quit();
    FI_DemoEnds();
}

void G_DemoAborted(void)
{
    G_ChangeGameState(GS_WAITING);
    FI_DemoEnds();
}

void G_ScreenShot(void)
{
    gameaction = GA_SCREENSHOT;
}

void G_DoScreenShot(void)
{
    int     i;
    filename_t name;
    char   *numPos;

    // Use game mode as the file name base.
    sprintf(name, "%s-", (char *) G_GetVariable(DD_GAME_MODE));
    numPos = name + strlen(name);

    // Find an unused file name.
    for(i = 0; i < 1e6; i++)    // Stop eventually...
    {
        sprintf(numPos, "%03i.tga", i);
        if(!M_FileExists(name))
            break;
    }
    M_ScreenShot(name, 24);
    Con_Message("Wrote %s.\n", name);
}
