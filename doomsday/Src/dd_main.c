
//**************************************************************************
//**
//** DD_MAIN.c
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <time.h>
#include <string.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAXWADFILES 128

// TYPES -------------------------------------------------------------------

typedef struct
{
	int *readPtr;
	int *writePtr;
} ddvalue_t;

typedef struct
{
	char *name;
	void (*func)(char **args, int tag);
	int requiredArgs;
	int tag;
} execOpt_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_ExecuteSetViewSize(void);
void G_CheckDemoStatus();
void F_Drawer(void);
boolean F_Responder(event_t *ev);
void S_InitScript(void);
void Net_Drawer(void);
void ErrorBox(boolean error, char *format, ...);
int CheckArg(char *tag, char **value);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PageDrawer(void);
static void HandleArgs(int state);
static void CheckRecordFrom(void);
static void ExecOptionFILE(char **args, int tag);
static void ExecOptionMAXZONE(char **args, int tag);
static void CreateSavePath(void);
static void WarpCheck(void);

#ifdef TIMEBOMB
static void DoTimeBomb(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean		renderTextures;
extern char			skyflatname[9];
extern HWND			hWndMain;
extern HINSTANCE	hInstDGL;
extern fixed_t		mapgravity;
extern int			gotframe;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

directory_t ddRuntimeDir, ddBinDir;
int verbose = 0;			// For debug messages (-verbose).
boolean DevMaps;			// true = Map development mode
char *DevMapsDir = "";		// development maps directory
int shareware;				// true if only episode 1 present
boolean debugmode;			// checkparm of -debug
boolean nofullscreen;		// checkparm of -nofullscreen
boolean cdrom;				// true if cd-rom mode active
boolean cmdfrag;			// true if a CMD_FRAG packet should be sent out
boolean singletics;			// debug flag to cancel adaptiveness
int isDedicated = false;
int maxzone = 0x2000000;	// Default zone heap. (32meg)
boolean autostart;
FILE *debugfile;

char *iwadlist[MAXWADFILES];
char *defaultWads = ""; // A list of wad names, whitespace in between (in .cfg).
char configFileName[256];
char defsFileName[256], topDefsFileName[256];
char ddBasePath[256] = "";	// Doomsday root directory is at...?

int queryResult = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int WarpMap;
static int demosequence;
static int pagetic;
static char *pagename;
static char *wadfiles[MAXWADFILES];

// CODE --------------------------------------------------------------------

//===========================================================================
// DD_AddIWAD
//	Adds the given IWAD to the list of default IWADs.
//===========================================================================
void DD_AddIWAD(const char *path)
{
	int i = 0;
	char buf[256];
	
	while(iwadlist[i]) i++;
	M_TranslatePath(path, buf);
	iwadlist[i] = calloc(strlen(buf) + 1, 1); // This mem is not freed?
	strcpy(iwadlist[i], buf);
}

//===========================================================================
// AddToWadList
//===========================================================================
#define ATWSEPS ",; \t"
static void AddToWadList(char *list)
{
	int	i=0;
	int len = strlen(list);
	char *buffer = malloc(len+1), *token;
	
	strcpy(buffer, list);
	token = strtok(buffer, ATWSEPS);
	while(token)
	{
		DD_AddStartupWAD(token/*, false*/);
		token = strtok(NULL, ATWSEPS);
	}
	free(buffer);
}

//===========================================================================
// autoDataAdder (f_forall_func_t)
//===========================================================================
static int autoDataAdder
	(const char *fileName, filetype_t type, void *loadFiles)
{
	// Skip directories.
	if(type == FT_DIRECTORY) return true;

	if(loadFiles)
		W_AddFile(fileName, false);
	else
		DD_AddStartupWAD(fileName);	

	// Continue searching.
	return true;
}

//===========================================================================
// DD_AddAutoData
//	Files with the extensions wad, lmp, pk3 and zip in the automatical data 
//	directory are added to the wadfiles list.
//===========================================================================
void DD_AddAutoData(boolean loadFiles)
{
	const char *extensions[] = { "wad", "lmp", "pk3", "zip", NULL };
	char pattern[256];
	int i;

	for(i = 0; extensions[i]; i++)
	{
		sprintf(pattern, "%sAuto\\*.%s", R_GetDataPath(), extensions[i]);
		F_ForAll(pattern, (void*) loadFiles, autoDataAdder);
	}
}

//===========================================================================
// DD_SetConfigFile
//===========================================================================
void DD_SetConfigFile(char *filename)
{
	strcpy(configFileName, filename);
}

//===========================================================================
// DD_SetDefsFile (exported)
//	Set the primary DED file, which is included immediately after 
//	Doomsday.ded.
//===========================================================================
void DD_SetDefsFile(char *filename)
{
	sprintf(topDefsFileName, "%sDefs\\%s", ddBasePath, filename);
}

//===========================================================================
// DD_Verbosity
//	Sets the level of verbosity that was requested using the -verbose
//	option(s).
//===========================================================================
void DD_Verbosity(void)
{
	int i;

	for(i = 1, verbose = 0; i < Argc(); ++i)
		if(ArgRecognize("-verbose", Argv(i))) verbose++;
}

//===========================================================================
// DD_Main
//	Engine and game initialization. When complete, starts the game loop.
//	What a mess...
//===========================================================================
void DD_Main(void)
{
	int		p;
	char	buff[10];
	FILE	*newout;
	char	*outfilename = "Doomsday.out";
	boolean	userdir_ok = true;
	
	DD_Verbosity();

	// The -userdir option sets the working directory.
	if(ArgCheckWith("-userdir", 1))
	{
		Dir_MakeDir(ArgNext(), &ddRuntimeDir);
		userdir_ok = Dir_ChDir(&ddRuntimeDir);
	}

	// We'll redirect stdout to a log file.
	CheckArg("-out", &outfilename);
	newout = freopen(outfilename, "w", stdout);
	if(!newout) ErrorBox(false, "Redirection of stdout failed. "
		"You won't see anything that's printf()ed.");
	setbuf(stdout, NULL);
	
	// The current working directory is the runtime dir.
	Dir_GetDir(&ddRuntimeDir);
	
	// The standard base directory is two levels upwards.
	if(ArgCheck("-stdbasedir"))
		strcpy(ddBasePath, "..\\..\\");
	
	if(ArgCheckWith("-basedir", 1))
	{
		strcpy(ddBasePath, ArgNext());
		Dir_ValidDir(ddBasePath);
	}

	Dir_MakeAbsolute(ddBasePath);
	Dir_ValidDir(ddBasePath);
	
	// We need to get the console initialized. Otherwise Con_Message() will
	// crash the system (yikes).
	Con_Init();
	Con_Message("Con_Init: Initializing the console.\n");
	
	// Create the startup messages window.
	SW_Init();
	
	Con_Message("Executable: "DOOMSDAY_VERSIONTEXT".\n");
	
	// Print the used command line.
	if(verbose)
	{
		Con_Message("Command line (%i strings):\n", Argc());
		for(p = 0; p < Argc(); p++) Con_Message("  %i: %s\n", p, Argv(p));
	}
	
	// Initialize the key mappings.
	DD_InitInput();
	
	// Any startup hooks?
	Plug_DoHook(HOOK_STARTUP);
	
	DD_AddStartupWAD("}Data\\Doomsday.wad");
	R_InitExternalResources();

	// The name of the .cfg will invariably be overwritten by the Game.
	strcpy(configFileName, "Doomsday.cfg");
	sprintf(defsFileName, "%sDefs\\Doomsday.ded", ddBasePath);
	
	// Was the change to userdir OK?
	if(!userdir_ok) Con_Message("--(!)-- User directory not found "
		"(check -userdir).\n");
	
	Con_Message("Z_Init: Init zone memory allocation daemon.\n");
	Z_Init();
	bamsInit();		// Binary angle calculations.

	// Initialize the zip file database.
	Zip_Init();

	Def_Init();
	
	if(ArgCheck("-dedicated"))
	{
		SW_Shutdown();
		isDedicated = true;
		Sys_ConInit();
	}
	
	// Load help resources.
	if(!isDedicated) DD_InitHelp();
	
	autostart = false;
	shareware = false; // Always false for Hexen
	
	HandleArgs(0); // Everything but WADs.
	
	novideo = ArgCheck("-novideo") || isDedicated;
	
	if(gx.PreInit) gx.PreInit();
	
	// Initialize subsystems
	Net_Init(); // Network before anything else.
	
	// Now we can hide the mouse cursor for good.
	Sys_HideMouse();
	
	// Load defaults before initing other systems
	Con_Message("Parsing configuration files.\n");
	// Check for a custom config file.
	if(ArgCheckWith("-config", 1))
	{
		// This will override the default config file.
		strcpy(configFileName, ArgNext());
		Con_Message("Custom config file: %s\n", configFileName);
	}
	
	// This'll be the default config file.
	Con_ParseCommands(configFileName, true);
	
	// Parse additional files (that should be parsed BEFORE init).
	if(ArgCheckWith("-cparse", 1))
	{
		for(;;)
		{
			char *arg = ArgNext();
			if(!arg || arg[0] == '-') break;
			Con_Message("Parsing: %s\n", arg);
			Con_ParseCommands(arg, false);
		}
	}
	
	if(defaultWads) AddToWadList(defaultWads); // These must take precedence.
	HandleArgs(1); // Only the WADs.
	
	Con_Message("W_Init: Init WADfiles.\n");
	
	// Add real files from the Auto directory to the wadfiles list.
	DD_AddAutoData(false);

	W_InitMultipleFiles(wadfiles);
	F_InitDirec();

	// Load files from the Auto directory. (If already loaded, won't be
	// loaded again.) This is done again because virtual files may now
	// exist in the Auto directory.
	DD_AddAutoData(true);

	// No more WADs will be loaded in startup mode after this point.
	W_EndStartup();

	// Execute the startup script (Startup.cfg).
	Con_ParseCommands("startup.cfg", false);

	// Now the game can identify the game mode.
	gx.UpdateState(DD_GAME_MODE);
	
	// Now that we've read the WADs we can initialize definitions.
	Def_Read();
	
	if(ArgCheck("-nowsk")) // No Windows system keys?
	{
		// Disable Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
		// A bit of a hack, I'm afraid...
		SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, TRUE, 0, 0);
		Con_Message("Windows system keys disabled.\n");
	}
	
	if(ArgCheckWith("-dumplump", 1))
	{
		char *arg = ArgNext();
		char fname[100];
		FILE *file;
		int lump = W_GetNumForName(arg);
		byte *lumpPtr = W_CacheLumpNum(lump, PU_STATIC);
		sprintf(fname, "%s.dum", arg);
		file = fopen(fname, "wb");
		if(!file)
		{
			Con_Error("Couldn't open %s for writing. %s\n", fname,
				strerror(errno));
		}
		fwrite(lumpPtr, 1, lumpinfo[lump].size, file);
		fclose(file);
		Con_Error("%s dumped to %s.\n", arg, fname);
	}
	
	if(ArgCheck("-dumpwaddir"))
	{
		printf("Lumps (%d total):\n", numlumps);
		for(p=0; p<numlumps; p++)
		{
			strncpy(buff, lumpinfo[p].name, 8);
			buff[8] = 0;
			printf("%04d - %-8s (hndl: %d, pos: %d, size: %d)\n",
				p, buff, lumpinfo[p].handle, lumpinfo[p].position, lumpinfo[p].size);
		}
		Con_Error("---End of lumps---\n");
	}
	
	Con_Message("Sys_Init: Setting up machine state.\n");
	Sys_Init();
	
	Con_Message("R_Init: Init the refresh daemon.\n");
	R_Init();
	
	Con_Message("Net_InitGame: Initializing game data.\n");
	Net_InitGame();
	Demo_Init();
	
	// Engine initialization is complete. Now start the GL driver and go
	// briefly to the Console Startup mode, so the results of the game
	// init, the config file parsing and possible netgame connection will
	// be visible.
	SW_Shutdown();			// The message window can be closed.
	if(!isDedicated)
	{
		Sys_ShowWindow(true);	// Show the main window (was created hidden).
		GL_Init();
		GL_InitRefresh(true);
	}
	
	// Start printing messages in the startup.
	Con_StartupInit();
	Con_Message("Con_StartupInit: Init startup screen.\n");
	
	if(gx.PostInit) gx.PostInit();
	
	// Try to load the autoexec file. This is done here to make sure
	// everything is initialized: the user can do here anything that
	// s/he'd be able to do in the game.
	Con_ParseCommands("autoexec.cfg", false);
	
	// Parse additional files.
	if(ArgCheckWith("-parse", 1))
	{
		for(;;)
		{
			char *arg = ArgNext();
			if(!arg || arg[0] == '-') break;
			Con_Message("Parsing: %s\n", arg);
			Con_ParseCommands(arg, false);
		}
	}
	
	// A console command on the command line?
	for(p = 1; p < Argc() - 1; p++)
	{
		if(stricmp(Argv(p), "-command")
			&& stricmp(Argv(p), "-cmd")) continue;
		for(++p; p < Argc(); p++)
		{
			char *arg = Argv(p);
			if(arg[0] == '-')
			{
				p--;
				break;
			}
			Con_Execute(arg, false);
		}
	}
	
	// In dedicated mode the console must be opened, so all input events
	// will be handled by it.
	if(isDedicated) Con_Open(true);
	
	Plug_DoHook(HOOK_INIT);	// Any initialization hooks?
	Con_UpdateKnownWords();	// For word completion (with Tab).
	
	// Client connection command.
	if(ArgCheckWith("-connect", 1))
		Con_Executef(false, "connect %s", ArgNext());
	
	// Server start command.
	// (shortcut for -command "net init tcpip; net server start").
	if(ArgExists("-server"))
	{
		if(!N_InitService(NSP_TCPIP, true))
			Con_Message("Can't start server: TCP/IP not available.\n");
		else
			Con_Executef(false, "net server start");
	}
	
	DD_GameLoop();			// Never returns...
}

//==========================================================================
// HandleArgs
//==========================================================================
static void HandleArgs(int state)
{
	int p;
	
	if(state == 0)
	{
		debugmode = ArgExists("-debug");
		nofullscreen = ArgExists("-nofullscreen") | ArgExists("-window");
		renderTextures = !ArgExists("-notex");
	}
	
	// Process all -file options.
	if(state)
	{
		for(p = 0; p < Argc(); p++)
		{
			if(stricmp(Argv(p), "-file")
				&& stricmp(Argv(p), "-iwad")
				&& stricmp(Argv(p), "-f")) continue;
			while(++p != Argc() && !ArgIsOption(p))
				DD_AddStartupWAD(Argv(p));
		}
	}
}

//===========================================================================
// DD_CheckTimeDemo
//===========================================================================
void DD_CheckTimeDemo(void)
{
	static boolean checked = false;
	
	if(!checked)
	{
		checked = true;
		if(ArgCheckWith("-timedemo", 1)		// Timedemo mode.
			|| ArgCheckWith("-playdemo", 1))	// Play-once mode.
		{
			char buf[200];
			sprintf(buf, "playdemo %s", ArgNext());
			Con_Execute(buf, false);
		}
	}
}

//==========================================================================
// DD_AddStartupWAD
//	This is a 'public' WAD file addition routine. The caller can put a
//	greater-than character (>) in front of the name to prepend the base
//	path to the file name (providing it's a relative path).
//==========================================================================
void DD_AddStartupWAD(const char *file)
{
	int i;
	char *new, temp[300];
	
	i = 0;
	while(wadfiles[i]) i++;
	M_TranslatePath(file, temp);
	new = calloc(strlen(temp) + 1, 1); // This is never freed?
	strcat(new, temp);
	wadfiles[i] = new;
}

//===========================================================================
// DD_CheckQuery
//	Queries are a (poor) way to extend the API without adding new functions.
//===========================================================================
void DD_CheckQuery(int query, int parm)
{
/*	int					i;
	jtnetserver_t		*buf;
	serverdataquery_t	*sdq;
	modemdataquery_t	*mdq;*/
	
	switch(query)
	{
	case DD_TEXTURE_HEIGHT_QUERY:
		queryResult = textures[parm]->height << FRACBITS;
		break;
		
	case DD_NET_QUERY:
		switch(parm)
		{
		case DD_PROTOCOL:
			queryResult = (int) N_GetProtocolName();
			break;
		}
		break;
			
	default:
		break;
	}
}

ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] =
{
	{ &screenWidth,		0 },
	{ &screenHeight,	0 },
	{ &netgame,			0 },
	{ &isServer,		0 },		// An *open* server?
	{ &isClient,		0 },
	{ &allowFrames,		&allowFrames },
	{ &skyflatnum,		0 },
	{ &gametic,			0 },
	{ &viewwindowx,		&viewwindowx },
	{ &viewwindowy,		&viewwindowy },
	{ &viewwidth,		&viewwidth },
	{ &viewheight,		&viewheight },
	{ &viewpw,			0 },
	{ &viewph,			0 },
	{ &viewx,			&viewx },
	{ &viewy,			&viewy },
	{ &viewz,			&viewz },
	{ &viewxOffset,		&viewxOffset },
	{ &viewyOffset,		&viewyOffset },
	{ &viewzOffset,		&viewzOffset },
	{ &viewangle,		&viewangle },
	{ &viewangleoffset,	&viewangleoffset },
	{ &consoleplayer,	&consoleplayer },
	{ &displayplayer,	&displayplayer },
	{ 0, 0 },
	{ &mipmapping,		0 },
	{ &linearRaw,		0 },
	{ &defResX,			&defResX },
	{ &defResY,			&defResY },
	{ &skyDetail,		0 },
	{ &sfx_volume,		&sfx_volume },
	{ &mus_volume,		&mus_volume },
	{ &mouseInverseY,	&mouseInverseY },
	{ &usegamma,		0 },
	{ &queryResult,		0 },
	{ &LevelFullBright,	&LevelFullBright },
	{ &CmdReturnValue,	0 },
	{ &game_ready,		&game_ready },
	{ &openrange,		0 },
	{ &opentop,			0 },
	{ &openbottom,		0 },
	{ &lowfloor,		0 },
	{ &isDedicated,		0 },
	{ &novideo,			0 },
	{ &defs.count.mobjs.num, 0 },
	{ &mapgravity,		&mapgravity },
	{ &gotframe,		0 },
	{ &playback,		0 },
	{ &defs.count.sounds.num, 0 },
	{ &defs.count.music.num, 0 },
	{ &numlumps,		0 },
	{ &send_all_players, &send_all_players },
	{ &pspOffX,			&pspOffX },
	{ &pspOffY,			&pspOffY },
	{ &psp_move_speed,	&psp_move_speed },
	{ &cplr_thrust_mul,	&cplr_thrust_mul },
	{ &clientPaused,	&clientPaused },
	{ &weaponOffsetScaleY, &weaponOffsetScaleY }
};

//===========================================================================
// DD_GetInteger
//===========================================================================
int DD_GetInteger(int ddvalue)
{
	if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
	{
		// How about some specials?
		switch(ddvalue)
		{
		case DD_DYNLIGHT_TEXTURE:
			return dltexname;

		case DD_TRACE_ADDRESS:
			return (int) &trace;
			
		case DD_TRANSLATIONTABLES_ADDRESS:
			return (int) translationtables;
			
		case DD_MAP_NAME:
			if(mapinfo && mapinfo->name[0]) return (int) mapinfo->name;
			break;
			
		case DD_MAP_AUTHOR:
			if(mapinfo && mapinfo->author[0]) return (int) mapinfo->author;
			break;
			
		case DD_MAP_MUSIC:
			if(mapinfo) return Def_GetMusicNum(mapinfo->music);
			return -1;
			
		case DD_WINDOW_HANDLE:
			return (int) hWndMain;
		}
		return 0;
	}
	if(ddValues[ddvalue].readPtr == NULL) return 0;
	return *ddValues[ddvalue].readPtr;
}

//===========================================================================
// DD_SetInteger
//===========================================================================
void DD_SetInteger(int ddvalue, int parm)
{
	if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
	{
		DD_CheckQuery(ddvalue, parm);
		// How about some special values?
		if(ddvalue == DD_SKYFLAT_NAME)
		{
			memset(skyflatname, 0, 9);
			strncpy(skyflatname, (char*) parm, 9);
		}
		else if(ddvalue == DD_TRANSLATED_SPRITE_TEXTURE)
		{
			// See DD_TSPR_PARM in dd_share.h.
			int lump = parm & 0xffffff, cls = (parm>>24) & 0xf, table = (parm>>28) & 0xf;
			if(table)
				GL_SetTranslatedSprite(lump, table, cls);
			else
				GL_SetSprite(lump, 0);
		}
		else if(ddvalue == DD_TEXTURE_GLOW)
		{
			// See DD_TGLOW_PARM in dd_share.h.
			int tnum = parm & 0xffff, istex = (parm & 0x80000000) != 0,
				glowstate = (parm & 0x10000) != 0;
			if(istex)
			{
				if(glowstate)
					textures[tnum]->flags |= TXF_GLOW;
				else
					textures[tnum]->flags &= ~TXF_GLOW;
			}
			else
			{
				flat_t *fl = R_GetFlat(tnum);
				if(glowstate)
					fl->flags |= TXF_GLOW;
				else
					fl->flags &= ~TXF_GLOW;
			}
		}
		return;
	}
	if(ddValues[ddvalue].writePtr)
		*ddValues[ddvalue].writePtr = parm;
}

//===========================================================================
// DD_GetPlayer
//===========================================================================
ddplayer_t *DD_GetPlayer(int number)
{
	return (ddplayer_t*) &players[number];
}