
//**************************************************************************
//**
//** D_MAIN.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
//#include <io.h>
#include <ctype.h>
#include <math.h>

/*
   // Oh, gross hack! But io.h clashes with vldoor_e::open/close...
   #define __P_SPEC__
 */

#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "s_sound.h"
#include "p_local.h"
#include "d_console.h"
#include "D_Action.h"
#include "d_config.h"
#include "r_sky.h"

  //#include "v_video.h"

#include "m_argv.h"
#include "m_menu.h"
#include "jDoom/m_ctrl.h"

#include "g_game.h"
#include "g_common.h"
#include "g_update.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"

#include "Common/am_map.h"
#include "Common/hu_stuff.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "d_main.h"
#include "d_items.h"
#include "m_bams.h"
#include "d_netJD.h"
#include "AcFnLink.h"
#include "g_update.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define	BGCOLOR		7
#define	FGCOLOR		8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    R_InitTranslation(void);
void    D_Display(void);
int     D_PrivilegedResponder(event_t *event);
fixed_t P_GetMobjFriction(mobj_t *mo);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The interface to the Doomsday engine.
game_import_t gi;
game_export_t gx;

boolean devparm;				// started game with -devparm
boolean nomonsters;				// checkparm of -nomonsters
boolean respawnparm;			// checkparm of -respawn
boolean fastparm;				// checkparm of -fast

skill_t startskill;
int     startepisode;
int     startmap;
boolean autostart;
FILE   *debugfile;

// Demo loop.
int     demosequence;
int     pagetic;
char   *pagename;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
static char gameModeString[17];

// CODE --------------------------------------------------------------------

void D_GetDemoLump(int num, char *out)
{
	sprintf(out, "%cDEMO%i",
			gamemode == shareware ? 'S' : gamemode ==
			registered ? 'R' : gamemode == retail ? 'U' : gamemission ==
			pack_plut ? 'P' : gamemission == pack_tnt ? 'T' : '2', num);
}

// print title for every printed line
char    title[128];

//===========================================================================
// DetectIWADs
//  Check which known IWADs are found. The purpose of this routine is to
//  find out which IWADs the user lets us to know about, but we don't 
//  decide which one gets loaded or even see if the WADs are actually 
//  there. The default location for IWADs is Data\jDoom\.
//===========================================================================
void DetectIWADs(void)
{
	typedef struct {
		char   *file;
		char   *override;
	} fspec_t;

	// The '>' means the paths are affected by the base path.
	char   *paths[] = {
		"}Data\\jDoom\\",
		"}Data\\",
		"}",
		"}Iwads\\",
		"",
		0
	};
	fspec_t iwads[] = {
		{"TNT.wad", "-tnt"},
		{"Plutonia.wad", "-plutonia"},
		{"Doom2.wad", "-doom2"},
		{"Doom1.wad", "-sdoom"},
		{"Doom.wad", "-doom"},
		{"Doom.wad", "-ultimate"},
		{0, 0}
	};
	int     i, k;
	boolean overridden = false;
	char    fn[256];

	// First check if an overriding command line option is being used.
	for(i = 0; iwads[i].file; i++)
		if(ArgExists(iwads[i].override))
		{
			overridden = true;
			break;
		}

	// Tell the engine about all the possible IWADs.    
	for(k = 0; paths[k]; k++)
		for(i = 0; iwads[i].file; i++)
		{
			// Are we allowed to use this?
			if(overridden && !ArgExists(iwads[i].override))
				continue;
			sprintf(fn, "%s%s", paths[k], iwads[i].file);
			DD_AddIWAD(fn);
		}
}

boolean LumpsFound(char **list)
{
	for(; *list; list++)
		if(W_CheckNumForName(*list) == -1)
			return false;
	return true;
}

/*
 * Checks availability of IWAD files by name, to determine whether 
 * registered/commercial features  should be executed (notably loading 
 * PWAD's).
 */
void D_IdentifyFromData(void)
{
	typedef struct {
		char  **lumps;
		GameMode_t mode;
	} identify_t;

	char   *shareware_lumps[] = {
		// List of lumps to detect shareware with.
		"e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6",
		"e1m7", "e1m8", "e1m9",
		"d_e1m1", "floor4_8", "floor7_2", NULL
	};
	char   *registered_lumps[] = {
		// List of lumps to detect registered with.
		"e2m1", "e2m2", "e2m3", "e2m4", "e2m5", "e2m6",
		"e2m7", "e2m8", "e2m9",
		"e3m1", "e3m2", "e3m3", "e3m4", "e3m5", "e3m6",
		"e3m7", "e3m8", "e3m9",
		"cybre1", "cybrd8", "floor7_2", NULL
	};
	char   *retail_lumps[] = {
		// List of lumps to detect Ultimate Doom with.
		"e4m1", "e4m2", "e4m3", "e4m4", "e4m5", "e4m6",
		"e4m7", "e4m8", "e4m9",
		"m_epi4", NULL
	};
	char   *commercial_lumps[] = {
		// List of lumps to detect Doom II with.
		"map01", "map02", "map03", "map04", "map10", "map20",
		"map25", "map30",
		"vilen1", "vileo1", "vileq1", "grnrock", NULL
	};
	char   *plutonia_lumps[] = {
		"_deutex_", "mc5", "mc11", "mc16", "mc20", NULL
	};
	char   *tnt_lumps[] = {
		"cavern5", "cavern7", "stonew1", NULL
	};
	identify_t list[] = {
		{commercial_lumps, commercial},	// Doom2 is easiest to detect.
		{retail_lumps, retail},	// Ultimate Doom is obvious.
		{registered_lumps, registered},
		{shareware_lumps, shareware}
	};
	int     i, num = sizeof(list) / sizeof(identify_t);

	// First check the command line.
	if(ArgCheck("-sdoom"))
	{
		// Shareware DOOM.
		gamemode = shareware;
		return;
	}
	if(ArgCheck("-doom"))
	{
		// Registered DOOM.
		gamemode = registered;
		return;
	}
	if(ArgCheck("-doom2") || ArgCheck("-plutonia") || ArgCheck("-tnt"))
	{
		// DOOM 2.
		gamemode = commercial;
		gamemission = doom2;
		if(ArgCheck("-plutonia"))
			gamemission = pack_plut;
		if(ArgCheck("-tnt"))
			gamemission = pack_tnt;
		return;
	}
	if(ArgCheck("-ultimate"))
	{
		// Retail DOOM 1: Ultimate DOOM.
		gamemode = retail;
		return;
	}

	// Now we must look at the lumps.
	for(i = 0; i < num; i++)
	{
		// If all the listed lumps are found, selection is made.
		// All found?
		if(LumpsFound(list[i].lumps))
		{
			gamemode = list[i].mode;
			// Check the mission packs.
			if(LumpsFound(plutonia_lumps))
				gamemission = pack_plut;
			else if(LumpsFound(tnt_lumps))
				gamemission = pack_tnt;
			else if(gamemode == commercial)
				gamemission = doom2;
			else
				gamemission = doom;
			return;
		}
	}

	// A detection couldn't be made.
	gamemode = shareware;		// Assume the minimum.
	Con_Message("\nIdentifyVersion: DOOM version unknown.\n"
				"** Important data might be missing! **\n\n");
}

/*
 * gamemode, gamemission and the gameModeString are set.
 */
void D_IdentifyVersion(void)
{
	D_IdentifyFromData();

	// The game mode string is returned in DD_Get(DD_GAME_MODE).
	// It is sent out in netgames, and the pcl_hello2 packet contains it.
	// A client can't connect unless the same game mode is used.
	memset(gameModeString, 0, sizeof(gameModeString));

	strcpy(gameModeString,
		   gamemode == shareware ? "doom1-share" : gamemode ==
		   registered ? "doom1" : gamemode ==
		   retail ? "doom1-ultimate" : gamemode == commercial ? (gamemission ==
																 pack_plut ?
																 "doom2-plut" :
																 gamemission ==
																 pack_tnt ?
																 "doom2-tnt" :
																 "doom2") :
		   "-");
}

void D_SetPlayerPtrs(void)
{
	int     i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].plr = DD_GetPlayer(i);
		players[i].plr->extradata = (void *) &players[i];
	}
}

//===========================================================================
// D_PreInit
//===========================================================================
void D_PreInit(void)
{
	int     i;

	if(gi.version < DOOMSDAY_VERSION)
		Con_Error("jDoom requires at least " "Doomsday " DOOMSDAY_VERSION_TEXT
				  "!\n");

	// Setup the DGL interface.
	G_InitDGL();

	// Config defaults. The real settings are read from the .cfg files
	// but these will be used no such files are found.
	memset(&cfg, 0, sizeof(cfg));
	cfg.playerMoveSpeed = 1;
	cfg.dclickuse = false;
	cfg.mouseSensiX = cfg.mouseSensiY = 8;
	cfg.povLookAround = true;
	cfg.joyaxis[0] = JOYAXIS_TURN;
	cfg.joyaxis[1] = JOYAXIS_MOVE;
	cfg.sbarscale = 20;			// Full size.
	cfg.screenblocks = cfg.setblocks = 10;
	cfg.echoMsg = true;
	cfg.lookSpeed = 3;
	cfg.usePatchReplacement = true;
	cfg.menuScale = .9f;
	cfg.menuGlitter = .5f;
	cfg.menuShadow = 0.33f;
	cfg.menuQuitSound = true;
	cfg.flashcolor[0] = .7f;
	cfg.flashcolor[1] = .9f;
	cfg.flashcolor[2] = 1;
	cfg.flashspeed = 4;
	cfg.turningSkull = true;
	cfg.hudShown[HUD_HEALTH] = true;
	cfg.hudShown[HUD_ARMOR] = true;
	cfg.hudShown[HUD_AMMO] = true;
	cfg.hudShown[HUD_KEYS] = true;
	cfg.hudShown[HUD_FRAGS] = true;
	cfg.hudShown[HUD_FACE] = false;
	cfg.hudScale = .6f;
	cfg.hudColor[0] = 1;
	cfg.hudColor[1] = cfg.hudColor[2] = 0;
	cfg.hudColor[3] = 1;
	cfg.hudIconAlpha = 1;
	cfg.xhairSize = 1;
	for(i = 0; i < 4; i++)
		cfg.xhairColor[i] = 255;
	cfg.snd_3D = false;
	cfg.snd_ReverbFactor = 100;
	cfg.moveCheckZ = true;
	cfg.jumpPower = 9;
	cfg.airborneMovement = 1;
	cfg.weaponAutoSwitch = true;
	cfg.secretMsg = true;
	cfg.netJumping = true;
	cfg.netEpisode = 1;
	cfg.netMap = 1;
	cfg.netSkill = sk_medium;
	cfg.netColor = 4; 
	cfg.plrViewHeight = 41;
	cfg.levelTitle = true;
	cfg.hideAuthorIdSoft = true;
	cfg.menuColor[0] = 1;
	cfg.menuColor2[0] = 1;
	cfg.menuSlam = false;

	cfg.maxskulls = true;
	cfg.allowskullsinwalls = false;

	cfg.statusbarAlpha = 1;
	cfg.statusbarCounterAlpha = 1;

	cfg.automapPos = 5;
	cfg.automapWidth = 1.0f;
	cfg.automapHeight = 1.0f;

	cfg.automapL0[0] = 0.4f;	// Unseen areas
	cfg.automapL0[1] = 0.4f;
	cfg.automapL0[2] = 0.4f;

	cfg.automapL1[0] = 1.0f;	// onesided lines 
	cfg.automapL1[1] = 0.0f;
	cfg.automapL1[2] = 0.0f;

	cfg.automapL2[0] = 0.77f;	// floor height change lines
	cfg.automapL2[1] = 0.6f;
	cfg.automapL2[2] = 0.325f;

	cfg.automapL3[0] = 1.0f;	// ceiling change lines
	cfg.automapL3[1] = 0.95f;
	cfg.automapL3[2] = 0.0f;

	cfg.automapBack[0] = 0.0f;
	cfg.automapBack[1] = 0.0f;
	cfg.automapBack[2] = 0.0f;
	cfg.automapBack[3] = 0.7f;
	cfg.automapLineAlpha = .7f;
	cfg.automapShowDoors = true;
	cfg.automapDoorGlow = 8;
	cfg.automapHudDisplay = 2;
	cfg.automapRotate = true;
	cfg.automapBabyKeys = false;
	cfg.counterCheatScale = .7f; //From jHeretic

	cfg.msgShow = true;
	cfg.msgCount = 4;
	cfg.msgScale = .8f;
	cfg.msgUptime = 5 * TICSPERSEC;
	cfg.msgAlign = ALIGN_LEFT;
	cfg.msgBlink = true;

	cfg.msgColor[0] = 1;
	cfg.msgColor[1] = cfg.msgColor[2] = 0;

	cfg.customMusic = true;
	cfg.killMessages = true;
	cfg.bobWeapon = 1;
	cfg.bobView = 1;
	cfg.bobWeaponLower = true;
	cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = true;

	D_SetPlayerPtrs();
	DD_SetConfigFile("jDoom.cfg");
	DD_SetDefsFile("jDoom\\jDoom.ded");
	R_SetDataPath("}Data\\jDoom\\");
	Con_DefineActions(actions);
	Set(DD_SKYFLAT_NAME, (int) SKYFLATNAME);

	// Add the cvars and ccmds to the console databases

	D_ConsoleRegistration();	// main command list
	G_Register();			// read-only game status cvars (for playsim)
	AM_Register();			// for the automap
	MN_Register();			// for the menu

	DD_AddStartupWAD("}Data\\jDoom\\jDoom.wad");	// FONTA and FONTB, M_THERM2
	DetectIWADs();

	/*p = ArgCheck ("-playdemo");
	   if(!p) p = ArgCheck ("-timedemo");
	   if(p && p < myargc-1)
	   {
	   sprintf(file, "%s.lmp", gi.Argv(p+1));
	   gi.AddStartupWAD(file);
	   Con_Message("Playing demo %s.lmp.\n", gi.Argv(p+1));
	   } */

	modifiedgame = false;
}

char *borderLumps[] = {
	"FLOOR7_2",
	"brdr_t",
	"brdr_r",
	"brdr_b",
	"brdr_l",
	"brdr_tl",
	"brdr_tr",
	"brdr_br",
	"brdr_bl"
};

//===========================================================================
// D_PostInit
//  Called after engine init has been completed.
//===========================================================================
void D_PostInit(void)
{
	int     p;
	char    file[256];

	Con_Message("jDoom " VERSIONTEXT "\n");

	SV_Init();
	XG_ReadTypes();
	XG_Register();			// register XG classnames

	G_DefaultBindings();
	R_SetViewSize(cfg.screenblocks, 0);
	G_SetGlowing();

	// Initialize weapon info using definitions.
	P_InitWeaponInfo();

	// Game parameters.
	monsterinfight = GetDefInt("AI|Infight", 0);
	nomonsters = ArgCheck("-nomonsters");
	respawnparm = ArgCheck("-respawn");
	fastparm = ArgCheck("-fast");
	devparm = ArgCheck("-devparm");
	if(ArgCheck("-altdeath"))
		cfg.netDeathmatch = 2;
	else if(ArgCheck("-deathmatch"))
		cfg.netDeathmatch = 1;

	// Print a game mode banner with rulers.
	Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
				gamemode ==
				retail ? "The Ultimate DOOM Startup\n" : gamemode ==
				shareware ? "DOOM Shareware Startup\n" : gamemode ==
				registered ? "DOOM Registered Startup\n" : gamemode ==
				commercial ? (gamemission ==
							  pack_plut ?
							  "Final DOOM: The Plutonia Experiment\n" :
							  gamemission ==
							  pack_tnt ? "Final DOOM: TNT: Evilution\n" :
							  "DOOM 2: Hell on Earth\n") : "Public DOOM\n");
	Con_FPrintf(CBLF_RULER, "");

	// Plutonia and TNT automatically turn on the full sky.
	if(gamemode == commercial &&
	   (gamemission == pack_plut || gamemission == pack_tnt))
	{
		Con_SetInteger("rend-sky-full", 1);
	}

	if(gamemode == commercial)	// Doom2 has a different background.
		borderLumps[0] = "GRNROCK";
	R_SetBorderGfx(borderLumps);

	// get skill / episode / map from parms
	gameskill = startskill = sk_medium;
	startepisode = 1;
	startmap = 1;
	autostart = false;

	p = ArgCheck("-skill");
	if(p && p < myargc - 1)
	{
		startskill = Argv(p + 1)[0] - '1';
		autostart = true;
	}

	p = ArgCheck("-episode");
	if(p && p < myargc - 1)
	{
		startepisode = Argv(p + 1)[0] - '0';
		startmap = 1;
		autostart = true;
	}

	p = ArgCheck("-timer");
	if(p && p < myargc - 1 && deathmatch)
	{
		int     time;

		time = atoi(Argv(p + 1));
		Con_Message("Levels will end after %d minute", time);
		if(time > 1)
			Con_Message("s");
		Con_Message(".\n");
	}

	/*
	   p = ArgCheck ("-avg");
	   if(p && p < myargc-1 && deathmatch)
	   Con_Message("Austin Virtual Gaming: Levels will end after "
	   "20 minutes\n");
	 */

	p = ArgCheck("-warp");
	if(p && p < myargc - 1)
	{
		if(gamemode == commercial)
		{
			startmap = atoi(Argv(p + 1));
			autostart = true;
		}
		else if(p < myargc - 2)
		{
			startepisode = Argv(p + 1)[0] - '0';
			startmap = Argv(p + 2)[0] - '0';
			autostart = true;
		}
	}

	// turbo option
	if((p = ArgCheck("-turbo")))
	{
		int     scale = 200;
		extern int forwardmove[2];
		extern int sidemove[2];

		if(p < myargc - 1)
			scale = atoi(Argv(p + 1));
		if(scale < 10)
			scale = 10;
		if(scale > 400)
			scale = 400;
		Con_Message("turbo scale: %i%%\n", scale);
		forwardmove[0] = forwardmove[0] * scale / 100;
		forwardmove[1] = forwardmove[1] * scale / 100;
		sidemove[0] = sidemove[0] * scale / 100;
		sidemove[1] = sidemove[1] * scale / 100;
	}

	/*
	   // Check for -file in shareware
	   if(modifiedgame)
	   {
	   // These are the lumps that will be checked in IWAD,
	   // if any one is not present, execution will be aborted.
	   char name[23][8]=
	   {
	   "e2m1","e2m2","e2m3","e2m4","e2m5","e2m6","e2m7","e2m8","e2m9",
	   "e3m1","e3m3","e3m3","e3m4","e3m5","e3m6","e3m7","e3m8","e3m9",
	   "dphoof","bfgga0","heada1","cybra1","spida1d1"
	   };
	   int i;

	   if(gamemode == shareware)
	   I_Error("\nYou cannot -file with the shareware version. Register!");

	   // Check for fake IWAD with right name,
	   // but w/o all the lumps of the registered version. 
	   if (gamemode == registered)
	   for (i = 0;i < 23; i++)
	   if (W_CheckNumForName(name[i])<0)
	   I_Error("\nThis is not the registered version.");
	   }
	 */

	Con_Message("P_Init: Init Playloop state.\n");
	P_Init();

	Con_Message("HU_Init: Setting up heads up display.\n");
	HU_Init();

	Con_Message("ST_Init: Init status bar.\n");
	ST_Init();

	Con_Message("MN_Init: Init miscellaneous info.\n");
	MN_Init();

	p = ArgCheck("-loadgame");
	if(p && p < myargc - 1)
	{
		SV_SaveGameFile(Argv(p + 1)[0] - '0', file);
		G_LoadGame(file);
	}

	if(gameaction != ga_loadgame)
	{
		if(autostart || IS_NETGAME)
		{
			G_DeferedInitNew(startskill, startepisode, startmap);
		}
		else
		{
			G_StartTitle();		// start up intro loop
		}
	}
}

void D_Shutdown(void)
{
}

void D_Ticker(void)
{
	MN_Ticker();
	G_Ticker();
}

void D_EndFrame(void)
{
}

char *G_Get(int id)
{
	switch (id)
	{
	case DD_GAME_ID:
		return "jDoom " VERSION_TEXT;

	case DD_GAME_MODE:
		return gameModeString;

	case DD_GAME_CONFIG:
		return gameConfigString;

	case DD_VERSION_SHORT:
		return VERSION_TEXT;

	case DD_VERSION_LONG:
		return VERSIONTEXT "\njDoom is based on linuxdoom-1.10.";

	case DD_ACTION_LINK:
		return (char *) actionlinks;

	case DD_PSPRITE_BOB_X:
		return (char *) (FRACUNIT +
						 FixedMul(FixedMul
								  (FRACUNIT * cfg.bobWeapon,
								   players[consoleplayer].bob),
								  finecosine[(128 * leveltime) & FINEMASK]));

	case DD_PSPRITE_BOB_Y:
		return (char *) (32 * FRACUNIT +
						 FixedMul(FixedMul
								  (FRACUNIT * cfg.bobWeapon,
								   players[consoleplayer].bob),
								  finesine[(128 *
											leveltime) & FINEMASK & (FINEANGLES
																	 / 2 -
																	 1)]));

	default:
		break;
	}
	// ID not recognized, return NULL.
	return 0;
}

//===========================================================================
// GetGameAPI
//  Takes a copy of the engine's entry points and exported data. Returns
//  a pointer to the structure that contains our entry points and exports.
//===========================================================================
game_export_t *GetGameAPI(game_import_t * imports)
{
	// Take a copy of the imports, but only copy as much data as is 
	// allowed and legal.
	memset(&gi, 0, sizeof(gi));
	memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

	// Clear all of our exports.
	memset(&gx, 0, sizeof(gx));

	// Fill in the data for the exports.
	gx.apiSize = sizeof(gx);
	gx.PreInit = D_PreInit;
	gx.PostInit = D_PostInit;
	gx.Shutdown = D_Shutdown;
	gx.BuildTicCmd = (void (*)(void*, float)) G_BuildTiccmd;
	gx.MergeTicCmd = (void (*)(void*, void*)) G_MergeTiccmd;
	gx.Ticker = D_Ticker;
	gx.G_Drawer = D_Display;
	gx.MN_Drawer = M_Drawer;
	gx.PrivilegedResponder = (boolean (*)(event_t *)) D_PrivilegedResponder;
	gx.MN_Responder = M_Responder;
	gx.G_Responder = G_Responder;
	gx.MobjThinker = P_MobjThinker;
	gx.MobjFriction = (fixed_t (*)(void *)) P_GetMobjFriction;
	gx.EndFrame = D_EndFrame;
	gx.ConsoleBackground = D_ConsoleBg;
	gx.UpdateState = G_UpdateState;
#undef Get
	gx.Get = G_Get;
	gx.R_Init = R_InitTranslation;

	gx.NetServerStart = D_NetServerStarted;
	gx.NetServerStop = D_NetServerClose;
	gx.NetConnect = D_NetConnect;
	gx.NetDisconnect = D_NetDisconnect;
	gx.NetPlayerEvent = D_NetPlayerEvent;
	gx.HandlePacket = D_HandlePacket;
	gx.NetWorldEvent = D_NetWorldEvent;

	// Data structure sizes.
	gx.ticcmd_size = sizeof(ticcmd_t);
	gx.vertex_size = sizeof(vertex_t);
	gx.seg_size = sizeof(seg_t);
	gx.sector_size = sizeof(sector_t);
	gx.subsector_size = sizeof(subsector_t);
	gx.node_size = sizeof(node_t);
	gx.line_size = sizeof(line_t);
	gx.side_size = sizeof(side_t);

	return &gx;
}
