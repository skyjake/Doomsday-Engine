
//**************************************************************************
//**
//** h2_main.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
//#include <direct.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "h2def.h"
#include "p_local.h"
#include "soundst.h"
#include "settings.h"
#include "h2_actn.h"
#include "d_net.h"
#include "g_update.h"

#include "AcFnLink.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct
{
	char *name;
	void (*func)(char **args, int tag);
	int requiredArgs;
	int tag;
} execOpt_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_ExecuteSetViewSize(void);
void G_BuildTiccmd(ticcmd_t *cmd);
void F_Drawer(void);
void I_HideMouse (void);
void S_InitScript(void);
void G_Drawer(void);
void H2_ConsoleBg(int *width, int *height);
void H2_EndFrame(void);
int H2_PrivilegedResponder(event_t *event);
void R_DrawPlayerSprites(ddplayer_t *viewplr);
void H2_ConsoleRegistration();
void H2_DefaultBindings();
void SB_HandleCheatNotification(int fromplayer, void *data, int length);
int HU_PSpriteYOffset(player_t *pl);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void H2_ProcessEvents(void);
/*void H2_DoAdvanceDemo(void);
void H2_AdvanceDemo(void);
void H2_PageTicker(void);*/

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void HandleArgs();
static void ExecOptionSCRIPTS(char **args, int tag);
static void ExecOptionDEVMAPS(char **args, int tag);
static void ExecOptionSKILL(char **args, int tag);
static void ExecOptionPLAYDEMO(char **args, int tag);
static void WarpCheck(void);

#ifdef TIMEBOMB
static void DoTimeBomb(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean startupScreen;
extern int demosequence;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

game_export_t	gx;
game_import_t	gi;

jhexen_config_t cfg;

boolean DevMaps;			// true = Map development mode
char *DevMapsDir = "";		// development maps directory
boolean shareware;			// true if only episode 1 present
boolean nomonsters;			// checkparm of -nomonsters
boolean respawnparm;		// checkparm of -respawn
boolean randomclass;		// checkparm of -randclass
boolean debugmode;			// checkparm of -debug
boolean ravpic;				// checkparm of -ravpic
boolean nofullscreen;		// checkparm of -nofullscreen
boolean cdrom;				// true if cd-rom mode active
boolean cmdfrag;			// true if a CMD_FRAG packet should be sent out
boolean singletics;			// debug flag to cancel adaptiveness
boolean artiskip;			// whether shift-enter skips an artifact
boolean netcheat;			// allow cheating in netgames (-netcheat)
boolean dontrender;			// don't render the player view (debug)
skill_t startskill;
int startepisode;
int startmap;

// Network games parameters.

boolean autostart;
//boolean advancedemo;
FILE *debugfile;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int WarpMap;

static execOpt_t ExecOptions[] =
{
	{ "-scripts", ExecOptionSCRIPTS, 1, 0 },
	{ "-devmaps", ExecOptionDEVMAPS, 1, 0 },
	{ "-skill", ExecOptionSKILL, 1, 0 },
	{ "-playdemo", ExecOptionPLAYDEMO, 1, 0 },
	{ "-timedemo", ExecOptionPLAYDEMO, 1, 0 },
	{ NULL, NULL, 0, 0 } // Terminator
};

static char gameModeString[17];

// CODE --------------------------------------------------------------------

//==========================================================================
//
// H2_Main
//
//==========================================================================
void InitMapMusicInfo(void);

char *borderLumps[] =
{
	"F_022",	// background
	"bordt",	// top
	"bordr",	// right
	"bordb",	// bottom
	"bordl",	// left
	"bordtl",	// top left
	"bordtr",	// top right
	"bordbr",	// bottom right
	"bordbl"	// bottom left
};

#ifdef TIC_DEBUG
FILE *rndDebugfile;
#endif

void H2_PreInit(void)
{
	int		i;

#ifdef TIC_DEBUG	
	rndDebugfile = fopen("rndtrace.txt", "wt");
#endif

	if(gi.version < DOOMSDAY_VERSION) 
		Con_Error("jHexen requires at least Doomsday "DOOMSDAY_VERSION_TEXT"!\n");

	G_InitDGL();

	// Setup the players.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		players[i].plr = DD_GetPlayer(i);
		players[i].plr->extradata = (void*) &players[i];
	}
	DD_SetDefsFile("jHexen\\jHexen.ded");
	DD_SetConfigFile("jHexen.cfg");
	R_SetDataPath("}Data\\jHexen\\");
	R_SetBorderGfx(borderLumps);
	Con_DefineActions(actions);

	// Add the JHexen cvars and ccmds to the console databases.
	H2_ConsoleRegistration();

	// The startup WADs.
	DD_AddIWAD("}Data\\jHexen\\Hexen.wad");
	DD_AddIWAD("}Data\\Hexen.wad");
	DD_AddIWAD("}Hexen.wad");
	DD_AddIWAD("Hexen.wad");
	DD_AddStartupWAD("}Data\\jHexen\\jHexen.wad");

	startepisode = 1;
	startskill = sk_medium;
	startmap = 1;
	shareware = false; // Always false for Hexen

	HandleArgs();

	// Set defaults.
	memset(&cfg, 0, sizeof(cfg));
	cfg.sbarscale = 20;
//	cfg.messageson = true;
	cfg.dclickuse = false;
	cfg.mouseSensiX = 8;
	cfg.mouseSensiY = 8;
//	cfg.joydead = 10;
	cfg.joyaxis[0] = JOYAXIS_TURN;
	cfg.joyaxis[1] = JOYAXIS_MOVE;
	cfg.screenblocks = cfg.setblocks = 10;
	cfg.showFullscreenMana = 1;
	cfg.lookSpeed = 3;
	cfg.xhairSize = 1;
	for(i = 0; i < 4; i++) cfg.xhairColor[i] = 255;	
	cfg.jumpEnabled = true;	// Always true in Hexen
	cfg.jumpPower = 9;

	cfg.netMap = 1;
	cfg.netSkill = sk_medium;
	cfg.netColor = 8;	// Use the default color by default.
	cfg.netMobDamageModifier = 1;
	cfg.netMobHealthModifier = 1;
	cfg.mapTitle = true;
	cfg.menuScale = .9f;
	cfg.cameraNoClip = true;
	cfg.bobView = cfg.bobWeapon = 1;
}

/*
 * Set the game mode string.
 */
void H2_IdentifyVersion(void)
{
	// Determine the game mode. Assume demo mode.
	strcpy(gameModeString, "hexen-demo");

	if(W_CheckNumForName("MAP05") >= 0)
	{
		// Normal Hexen.
		strcpy(gameModeString, "hexen");
	}
	
	// This is not a very accurate test...
	if(W_CheckNumForName("MAP59") >= 0 && W_CheckNumForName("MAP60") >= 0)
	{
		// It must be Deathkings!
		strcpy(gameModeString, "hexen-dk");
	}
}

void H2_PostInit(void)
{
	int	pClass, p;

	Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER, "jHexen "
		VERSIONTEXT"\n");
	Con_FPrintf(CBLF_RULER, "");

	// Did we end up in demo mode?
	if(!stricmp(gameModeString, "hexen-demo"))
	{
		//Set(DD_SHAREWARE, true);
		shareware = true;
		Con_Message( "*** Hexen 4-level Beta Demo ***\n");
	}

	// Init savegame directory.
	SV_HxInit();

	H2_DefaultBindings();
	G_SetGlowing();

	// Check the -class argument.
	pClass = PCLASS_FIGHTER;
	if((p = ArgCheck("-class")) != 0)
	{
		pClass = atoi(Argv(p+1));
		if(pClass > PCLASS_MAGE || pClass < PCLASS_FIGHTER)
		{
			Con_Error("Invalid player class: %d\n", pClass);
		}
		Con_Message("\nPlayer Class: %d\n", pClass);
	}
	cfg.PlayerClass[consoleplayer] = pClass;

	// Init the view.
	R_SetViewSize(cfg.screenblocks, 0);

	Con_Message("P_Init: Init Playloop state.\n");
	P_Init();
	
	Con_Message("MN_Init: Init menu system.\n");
	MN_Init();

	Con_Message("CT_Init: Init chat mode data.\n");
	CT_Init();

	InitMapMusicInfo();		// Init music fields in mapinfo

	Con_Message("S_InitScript\n");
	S_InitScript();

	Con_Message("SN_InitSequenceScript: Registering sound sequences.\n");
	SN_InitSequenceScript();

	// Check for command line warping. Follows P_Init() because the
	// MAPINFO.TXT script must be already processed.
	WarpCheck();

	if(autostart)
	{
		Con_Message("Warp to Map %d (\"%s\":%d), Skill %d\n",
			WarpMap, P_GetMapName(startmap), startmap, startskill+1);
	}

	Con_Message("SB_Init: Loading patches.\n");
	SB_Init();

//	if(CheckRecordFrom()) return;

	/*p = ArgCheck("-record");
	if(p && p < gi.Argc()-1)
	{
		singledemo = true;	// Quit after recording.
		G_RecordDemo(startskill, 1, startepisode, startmap, Argv(p+1));
		return;
	}

	p = ArgCheck("-playdemo");
	if(p && p < gi.Argc()-1)
	{
		singledemo = true; // Quit after one demo
		G_DeferedPlayDemo(Argv(p+1));
		return;
	}*/

/*	p = ArgCheck("-timedemo");
	if(p && p < gi.Argc()-1)
	{
		singledemo = true; // Quit after timing
		G_TimeDemo(Argv(p+1));
		return;
	}*/

	if((p = ArgCheckWith("-loadgame", 1)) != 0)
	{
		G_LoadGame(atoi(Argv(p+1)));
	}

	if(gameaction != ga_loadgame)
	{
		GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
		if(autostart || netgame)
		{
			G_StartNewInit();
			G_InitNew(startskill, startepisode, startmap);
		}
		else
		{
			G_StartTitle();
		}
	}
}

//==========================================================================
//
// HandleArgs
//
//==========================================================================

static void HandleArgs()
{
	int p;
	execOpt_t *opt;

	nomonsters = ArgExists("-nomonsters");
	respawnparm = ArgExists("-respawn");
	randomclass = ArgExists("-randclass");
	ravpic = ArgExists("-ravpic");
	artiskip = ArgExists("-artiskip");
	debugmode = ArgExists("-debug");
	cfg.netDeathmatch = ArgExists("-deathmatch");
	cdrom = ArgExists("-cdrom");
	cmdfrag = ArgExists("-cmdfrag");
	nofullscreen = ArgExists("-nofullscreen");
	netcheat = ArgExists("-netcheat");
	dontrender = ArgExists("-noview");
	
	// Process command line options
	for(opt = ExecOptions; opt->name != NULL; opt++)
	{
		p = ArgCheck(opt->name);
		if(p && p < Argc() - opt->requiredArgs)
		{
			opt->func(ArgvPtr(p), opt->tag);
		}
	}
}

//==========================================================================
//
// WarpCheck
//
//==========================================================================

static void WarpCheck(void)
{
	int p;
	int map;

	p = ArgCheck("-warp");
	if(p && p < Argc()-1)
	{
		WarpMap = atoi(Argv(p+1));
		map = P_TranslateMap(WarpMap);
		if(map == -1)
		{ // Couldn't find real map number
			startmap = 1;
			Con_Message("-WARP: Invalid map number.\n");
		}
		else
		{ // Found a valid startmap
			startmap = map;
			autostart = true;
		}
	}
	else
	{
		WarpMap = 1;
		startmap = P_TranslateMap(1);
		if(startmap == -1)
		{
			startmap = 1;
		}
	}
}

//==========================================================================
//
// ExecOptionSKILL
//
//==========================================================================

static void ExecOptionSKILL(char **args, int tag)
{
	startskill = args[1][0]-'1';
	autostart = true;
}


//==========================================================================
//
// ExecOptionPLAYDEMO
//
//==========================================================================

static void ExecOptionPLAYDEMO(char **args, int tag)
{
	char file[256];

	sprintf(file, "%s.lmp", args[1]);
	DD_AddStartupWAD(file);
	Con_Message("Playing demo %s.lmp.\n", args[1]);
}

//==========================================================================
//
// ExecOptionSCRIPTS
//
//==========================================================================

static void ExecOptionSCRIPTS(char **args, int tag)
{
	sc_FileScripts = true;
	sc_ScriptsDir = args[1];
}

//==========================================================================
//
// ExecOptionDEVMAPS
//
//==========================================================================

static void ExecOptionDEVMAPS(char **args, int tag)
{
	DevMaps = true;
	Con_Message("Map development mode enabled:\n");
	Con_Message("[config    ] = %s\n", args[1]);
	SC_OpenFileCLib(args[1]);
	SC_MustGetStringName("mapsdir");
	SC_MustGetString();
	Con_Message("[mapsdir   ] = %s\n", sc_String);
	DevMapsDir = malloc(strlen(sc_String)+1);
	strcpy(DevMapsDir, sc_String);
	SC_MustGetStringName("scriptsdir");
	SC_MustGetString();
	Con_Message("[scriptsdir] = %s\n", sc_String);
	sc_FileScripts = true;
	sc_ScriptsDir = malloc(strlen(sc_String)+1);
	strcpy(sc_ScriptsDir, sc_String);
	while(SC_GetString())
	{
		if(SC_Compare("file"))
		{
			SC_MustGetString();
			DD_AddStartupWAD(sc_String);
		}
		else
		{
			SC_ScriptError(NULL);
		}
	}
	SC_Close();
}


/*long superatol(char *s)
{
	long int n=0, r=10, x, mul=1;
	char *c=s;

	for (; *c; c++)
	{
		x = (*c & 223) - 16;

		if (x == -3)
		{
			mul = -mul;
		}
		else if (x == 72 && r == 10)
		{
			n -= (r=n);
			if (!r) r=16;
			if (r<2 || r>36) return -1;
		}
		else
		{
			if (x>10) x-=39;
			if (x >= r) return -1;
			n = (n*r) + x;
		}
	}
	return(mul*n);
}
*/

/*static void ExecOptionMAXZONE(char **args, int tag)
{
	int size;
	
	size = superatol(args[1]);
	if (size < MINIMUM_HEAP_SIZE) size = MINIMUM_HEAP_SIZE;
	if (size > MAXIMUM_HEAP_SIZE) size = MAXIMUM_HEAP_SIZE;
	maxzone = size;
}*/


//==========================================================================
//
// H2_AdvanceDemo
//
// Called after each demo or intro demosequence finishes.
//
//==========================================================================
/*
void H2_AdvanceDemo(void)
{
	advancedemo = true;
}

//==========================================================================
//
// H2_StartTitle
//
//==========================================================================

void H2_StartTitle(void)
{
	gameaction = ga_nothing;
	demosequence = -1;
	H2_AdvanceDemo();
}
*/
//==========================================================================
//
// CheckRecordFrom
//
// -recordfrom <savegame num> <demoname>
//
//==========================================================================

/*static boolean CheckRecordFrom(void)
{
	int p;

	p = ArgCheck("-recordfrom");
	if(!p || p > gi.Argc()-2)
	{ // Bad args
		return false;
	}
	G_LoadGame(atoi(Argv(p+1)));
	G_DoLoadGame(); // Load the gameskill etc info from savegame
	G_RecordDemo(gameskill, 1, gameepisode, gamemap, Argv(p+2));
	return true;
}*/


//==========================================================================
//
// FixedDiv
//
//==========================================================================

/*fixed_t FixedDiv(fixed_t a, fixed_t b)
{
	if((abs(a)>>14) >= abs(b))
	{
		return((a^b)<0 ? DDMININT : DDMAXINT);
	}
	return(FixedDiv2(a, b));
}*/


void H2_Ticker(void)
{
	//if(advancedemo) H2_DoAdvanceDemo();
	MN_Ticker();
	G_Ticker();
}

/*void G_ModifyDupTiccmd(ticcmd_t *cmd)
{
	if(cmd->buttons & BT_SPECIAL) cmd->buttons = 0;
}*/

/*void H2_UpdateState(int step)
{
	switch(step)
	{
	case DD_PRE:
		// Do a sound reset.
		S_Reset();
		break;

	case DD_POST:
		P_Init();
		SB_Init(); // Updates the status bar patches.
		MN_Init();
		S_InitScript();
		SN_InitSequenceScript();
		H2_SetGlowing();
		break;
	}
}*/

char *G_Get(int id)
{
	switch(id)
	{
	case DD_GAME_ID:
		return "jHexen "VERSION_TEXT;

	case DD_GAME_MODE:
		return gameModeString;

	case DD_GAME_CONFIG:
		return gameConfigString;

	case DD_VERSION_SHORT:
		return VERSION_TEXT;

	case DD_VERSION_LONG:
		return VERSIONTEXT"\njHexen is based on Hexen v1.1 by Raven Software.";
	
	case DD_ACTION_LINK:
		return (char*) actionlinks;

	case DD_PSPRITE_BOB_X:
		if(players[consoleplayer].morphTics > 0) return 0;
		return (char*) (FRACUNIT + FixedMul(
			FixedMul(FRACUNIT*cfg.bobWeapon, players[consoleplayer].bob), 
			finecosine[(128 * leveltime) & FINEMASK]));

	case DD_PSPRITE_BOB_Y:
		if(players[consoleplayer].morphTics > 0) return (char*) (32*FRACUNIT);
		return (char*) (32*FRACUNIT + FixedMul(
			FixedMul(FRACUNIT*cfg.bobWeapon, players[consoleplayer].bob),
			finesine[(128 * leveltime) & FINEMASK & (FINEANGLES/2-1)]));

	case DD_ALT_MOBJ_THINKER:
		return (char*) P_BlasterMobjThinker;

/*	case DD_PSPRITE_OFFSET_Y:
		return (char*) HU_PSpriteYOffset(players + consoleplayer);*/

	default:
		break;
	}
	return 0;
}

//--------------------------------------------------------------------------
// H2_Shutdown
//	Game-specific shutdown routine.
//--------------------------------------------------------------------------
void H2_Shutdown(void)
{
}

//--------------------------------------------------------------------------
// G_DiscardTiccmd
//	Server uses this if it's necessary to discard ticcmds (client has too
//	many waiting in the buffer, server tries to get rid of the extra lag
//	that causes).
//--------------------------------------------------------------------------
void G_DiscardTiccmd(ticcmd_t *discarded, ticcmd_t *current)
{
	// We're only interested in buttons.
	// Old Attack and Use buttons apply, if they're set.
	current->buttons |= discarded->buttons & (BT_ATTACK | BT_USE);
	if(discarded->buttons & BT_SPECIAL || current->buttons & BT_SPECIAL) 
		return;
	if(discarded->buttons & BT_CHANGE && !(current->buttons & BT_CHANGE))
	{
		// Use the old weapon change.
		current->buttons |= discarded->buttons & (BT_CHANGE | BT_WEAPONMASK);
	}
}

//===========================================================================
// GetGameAPI
//	Takes a copy of the engine's entry points and exported data. Returns
//	a pointer to the structure that contains our entry points and exports.
//===========================================================================
game_export_t *GetGameAPI(game_import_t *imports)
{
//	gl_export_t *glexp;

	// Take a copy of the imports, but only copy as much data as is 
	// allowed and legal.
	memset(&gi, 0, sizeof(gi));
	memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

	// Interface to DGL.
/*	glexp = (gl_export_t*) GL_GetAPI();
	memset(&gl, 0, sizeof(gl));
	memcpy(&gl, glexp, MIN_OF(sizeof(gl_export_t), glexp->apiSize));*/

	// Clear all of our exports.
	memset(&gx, 0, sizeof(gx));

	// Fill in the data for the exports.
	gx.apiSize = sizeof(gx);
	gx.PreInit = H2_PreInit;
	gx.PostInit = H2_PostInit;
	gx.Shutdown = H2_Shutdown;
	gx.BuildTicCmd = (void (*)(void*)) G_BuildTiccmd;
	gx.DiscardTicCmd = (void (*)(void*, void*)) G_DiscardTiccmd;
	gx.Ticker = H2_Ticker;
	gx.G_Drawer = G_Drawer;
	gx.MN_Drawer = MN_Drawer;
	gx.PrivilegedResponder = (boolean (*)(event_t*)) H2_PrivilegedResponder;
	gx.MN_Responder = MN_Responder;
	gx.G_Responder = G_Responder;
	gx.MobjThinker = P_MobjThinker;
	gx.MobjFriction = (fixed_t (*)(void*)) P_GetMobjFriction;
	gx.EndFrame = H2_EndFrame;
	gx.ConsoleBackground = H2_ConsoleBg;
	gx.UpdateState = G_UpdateState;
#undef Get
	gx.Get = G_Get;

	gx.NetServerStart = D_NetServerStarted;
	gx.NetServerStop = D_NetServerClose;
	gx.NetConnect = D_NetConnect;
	gx.NetDisconnect = D_NetDisconnect;
	gx.NetPlayerEvent = D_NetPlayerEvent;
	gx.NetWorldEvent = D_NetWorldEvent;
	gx.HandlePacket = D_HandlePacket;

	// The structure sizes.
	gx.ticcmd_size = sizeof(ticcmd_t);
	gx.vertex_size = sizeof(vertex_t);
	gx.seg_size = sizeof(seg_t);
	gx.sector_size = sizeof(sector_t);
	gx.subsector_size = sizeof(subsector_t);
	gx.node_size = sizeof(node_t);
	gx.line_size = sizeof(line_t);
	gx.side_size = sizeof(side_t);
	gx.polyobj_size = sizeof(polyobj_t);

	return &gx;
}

