
//**************************************************************************
//**
//** D_MAIN.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <io.h>
#include <ctype.h>

// Oh, gross hack! But io.h clashes with vldoor_e::open/close...
#define __P_SPEC__ 

#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "s_sound.h"
#include "p_local.h"
#include "d_console.h"
#include "d_action.h"
#include "d_config.h"

#include "v_video.h"

#include "m_argv.h"
#include "m_menu.h"

#include "g_game.h"
#include "g_common.h"
#include "g_update.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"

#include "am_map.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "r_main.h"
#include "d_main.h"
#include "d_items.h"
#include "m_bams.h"
#include "d_netjd.h"
#include "acfnlink.h"
#include "g_update.h"

// MACROS ------------------------------------------------------------------

#define	BGCOLOR		7
#define	FGCOLOR		8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_Init(void);
void G_BuildTiccmd (ticcmd_t* cmd);
void D_Display (void);
int D_PrivilegedResponder(event_t *event);
void D_DefaultBindings();
fixed_t P_GetMobjFriction(mobj_t* mo);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void D_DoAdvanceDemo (void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The interface to the Doomsday engine.
game_import_t	gi;
game_export_t	gx;

boolean			devparm;		// started game with -devparm
boolean         nomonsters;		// checkparm of -nomonsters
boolean         respawnparm;	// checkparm of -respawn
boolean         fastparm;		// checkparm of -fast

skill_t			startskill;
int				startepisode;
int				startmap;
boolean			autostart;
FILE*			debugfile;
boolean			advancedemo;

// Demo loop.
int             demosequence;
int             pagetic;
char            *pagename;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
static char gameModeString[17];

// CODE --------------------------------------------------------------------

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker (void)
{
    if (--pagetic < 0)
	D_AdvanceDemo ();
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
    advancedemo = true;
}

void D_GetDemoLump(int num, char *out)
{
	sprintf(out, "%cDEMO%i", gamemode == shareware? 'S'
		: gamemode == registered? 'R'
		: gamemode == retail? 'U'
		: gamemission == pack_plut? 'P'
		: gamemission == pack_tnt? 'T'
		: '2', num);
}

//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
void D_DoAdvanceDemo (void)
{
	char buf[10];

    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;
	GL_SetFilter(0);
	
    if ( gamemode == retail )
		demosequence = (demosequence+1)%7;
    else
		demosequence = (demosequence+1)%6;
    
    switch (demosequence)
    {
	case 0:
		if ( gamemode == commercial )
			pagetic = 35 * 11;
		else
			pagetic = 170;
		gamestate = GS_DEMOSCREEN;
		pagename = "TITLEPIC";
		if ( gamemode == commercial )
			S_StartMusicNum(mus_dm2ttl, false);
		else
			S_StartMusic("intro", false);
		break;
	case 1:
		D_GetDemoLump(1, buf);
		G_DeferedPlayDemo(buf);
		break;
	case 2:
		pagetic = 200;
		gamestate = GS_DEMOSCREEN;
		pagename = "CREDIT";
		break;
	case 3:
		D_GetDemoLump(2, buf);
		G_DeferedPlayDemo(buf);
		break;
	case 4:
		gamestate = GS_DEMOSCREEN;
		if ( gamemode == commercial)
		{
			pagetic = 35 * 11;
			pagename = "TITLEPIC";
			S_StartMusicNum(mus_dm2ttl, false);
		}
		else
		{
			pagetic = 200;
			
			if ( gamemode == retail )
				pagename = "CREDIT";
			else
				pagename = "HELP2";
		}
		break;
	case 5:
		D_GetDemoLump(3, buf);
		G_DeferedPlayDemo(buf);
		break;
        // THE DEFINITIVE DOOM Special Edition demo
	case 6:
		D_GetDemoLump(4, buf);
		G_DeferedPlayDemo(buf);
		break;
    }
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
	G_StopDemo();
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo ();
}




//      print title for every printed line
char            title[128];

//===========================================================================
// DetectIWADs
//	Check which known IWADs are found. The purpose of this routine is to
//	find out which IWADs the user lets us to know about, but we don't 
//	decide which one gets loaded or even see if the WADs are actually 
//	there. The default location for IWADs is Data\jDoom\.
//===========================================================================
void DetectIWADs (void)
{
	typedef struct 
	{
		char		*file;
		char		*override;
	} fspec_t;
	
	// The '>' means the paths are affected by the base path.
	char *paths[] = 
	{
		">Data\\jDoom\\",
		">Data\\",
		">",
		">Iwads\\",
		"",
		0 
	};
	fspec_t iwads[] = 
	{
		"TNT.wad",		"-tnt",
		"Plutonia.wad",	"-plutonia",
		"Doom2.wad",	"-doom2",
		"Doom1.wad",	"-sdoom",
		"Doom.wad",		"-doom",
		"Doom.wad",		"-ultimate",
		0, 0
	};
	int	i, k;
	fspec_t *found = NULL;
	boolean overridden = false;
	char fn[256];

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
			if(overridden && !ArgExists(iwads[i].override)) continue;
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
		char		**lumps;
		GameMode_t	mode;	
	} identify_t;

	char *shareware_lumps[] = 
	{
		// List of lumps to detect shareware with.
		"e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6", 
		"e1m7", "e1m8", "e1m9",
		"d_e1m1", "floor4_8", "floor7_2", NULL
	};
	char *registered_lumps[] = 
	{
		// List of lumps to detect registered with.
		"e2m1", "e2m2", "e2m3", "e2m4", "e2m5", "e2m6", 
		"e2m7", "e2m8", "e2m9",
		"e3m1", "e3m2", "e3m3", "e3m4", "e3m5", "e3m6", 
		"e3m7", "e3m8", "e3m9",
		"cybre1", "cybrd8", "floor7_2", NULL
	};
	char *retail_lumps[] =
	{
		// List of lumps to detect Ultimate Doom with.
		"e4m1", "e4m2", "e4m3", "e4m4", "e4m5", "e4m6", 
		"e4m7", "e4m8", "e4m9", 
		"m_epi4", NULL
	};
	char *commercial_lumps[] =
	{
		// List of lumps to detect Doom II with.
		"map01", "map02", "map03", "map04", "map10", "map20",
		"map25", "map30",
		"vilen1", "vileo1", "vileq1", "grnrock", NULL
	};
	char *plutonia_lumps[] =
	{
		"_deutex_", "mc5", "mc11", "mc16", "mc20", NULL
	};
	char *tnt_lumps[] =
	{
		"cavern5", "cavern7", "stonew1", NULL
	};
	identify_t list[] =
	{
		commercial_lumps, commercial,	// Doom2 is easiest to detect.
		retail_lumps, retail,			// Ultimate Doom is obvious.
		registered_lumps, registered,
		shareware_lumps, shareware
	};
	int i, num = sizeof(list)/sizeof(identify_t);

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
		if(ArgCheck("-plutonia")) gamemission = pack_plut;
		if(ArgCheck("-tnt")) gamemission = pack_tnt;
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
	gamemode = shareware; // Assume the minimum.
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
		  gamemode == shareware?  "doom1-share"
		: gamemode == registered? "doom1"
		: gamemode == retail?     "doom1-ultimate"
		: gamemode == commercial? 
			( gamemission == pack_plut? "doom2-plut"
			: gamemission == pack_tnt?  "doom2-tnt"
			: "doom2" )
		: "-");
}
		
/*
    char*	doom1wad;
    char*	doomwad;
    char*	doomuwad;
    char*	doom2wad;

    char*	doom2fwad;
    char*	plutoniawad;
    char*	tntwad;

#ifdef NORMALUNIX
    char *home;
    char *doomwaddir;
    doomwaddir = getenv("DOOMWADDIR");
    if (!doomwaddir)
	doomwaddir = ".";

    // Commercial.
    doom2wad = malloc(strlen(doomwaddir)+1+9+1);
    sprintf(doom2wad, "%s/doom2.wad", doomwaddir);

    // Retail.
    doomuwad = malloc(strlen(doomwaddir)+1+8+1);
    sprintf(doomuwad, "%s/doomu.wad", doomwaddir);
    
    // Registered.
    doomwad = malloc(strlen(doomwaddir)+1+8+1);
    sprintf(doomwad, "%s/doom.wad", doomwaddir);
    
    // Shareware.
    doom1wad = malloc(strlen(doomwaddir)+1+9+1);
    sprintf(doom1wad, "%s/doom1.wad", doomwaddir);

     // Bug, dear Shawn.
    // Insufficient malloc, caused spurious realloc errors.
    plutoniawad = malloc(strlen(doomwaddir)+1+12+1);
    sprintf(plutoniawad, "%s/plutonia.wad", doomwaddir);

    tntwad = malloc(strlen(doomwaddir)+1+9+1);
    sprintf(tntwad, "%s/tnt.wad", doomwaddir);


    // French stuff.
    doom2fwad = malloc(strlen(doomwaddir)+1+10+1);
    sprintf(doom2fwad, "%s/doom2f.wad", doomwaddir);

    home = getenv("HOME");
    if (!home)
      I_Error("Please set $HOME to your home directory");
    sprintf(basedefault, "%s/.doomrc", home);
#endif

    if (ArgCheck ("-shdev"))
    {
	gamemode = shareware;
	devparm = true;
	D_AddFile (DEVDATA"doom1.wad");
	D_AddFile (DEVMAPS"data_se/texture1.lmp");
	D_AddFile (DEVMAPS"data_se/pnames.lmp");
	strcpy (basedefault,DEVDATA"default.cfg");
	return;
    }

    if (ArgCheck ("-regdev"))
    {
	gamemode = registered;
	devparm = true;
	D_AddFile (DEVDATA"doom.wad");
	D_AddFile (DEVMAPS"data_se/texture1.lmp");
	D_AddFile (DEVMAPS"data_se/texture2.lmp");
	D_AddFile (DEVMAPS"data_se/pnames.lmp");
	strcpy (basedefault,DEVDATA"default.cfg");
	return;
    }

    if (ArgCheck ("-comdev"))
    {
	gamemode = commercial;
	devparm = true;
	// I don't bother
//	if(plutonia)
	    //D_AddFile (DEVDATA"plutonia.wad");
	//else if(tnt)
	  //  D_AddFile (DEVDATA"tnt.wad");
	//else
	    D_AddFile (DEVDATA"doom2.wad");
	    
	D_AddFile (DEVMAPS"cdata/texture1.lmp");
	D_AddFile (DEVMAPS"cdata/pnames.lmp");
	strcpy (basedefault,DEVDATA"default.cfg");
	return;
    }

    if ( !access (doom2fwad,R_OK) )
    {
	gamemode = commercial;
	// C'est ridicule!
	// Let's handle languages in config files, okay?
	language = french;
	printf("French version\n");
	D_AddFile (doom2fwad);
	return;
    }

    if ( !access (doom2wad,R_OK) )
    {
	gamemode = commercial;
	D_AddFile (doom2wad);
	return;
    }

    if ( !access (plutoniawad, R_OK ) )
    {
      gamemode = commercial;
      D_AddFile (plutoniawad);
      return;
    }

    if ( !access ( tntwad, R_OK ) )
    {
      gamemode = commercial;
      D_AddFile (tntwad);
      return;
    }

    if ( !access (doomuwad,R_OK) )
    {
      gamemode = retail;
      D_AddFile (doomuwad);
      return;
    }

    if ( !access (doomwad,R_OK) )
    {
      gamemode = registered;
      D_AddFile (doomwad);
      return;
    }

    if ( !access (doom1wad,R_OK) )
    {
      gamemode = shareware;
      D_AddFile (doom1wad);
      return;
    }

    printf("Game mode indeterminate.\n");
    gamemode = indetermined;

    // We don't abort. Let's see what the PWAD contains.
    //exit(1);
    //I_Error ("Game mode indeterminate\n");
}
*/
//
// Find a Response File
//
/*void FindResponseFile (void)
{
    int             i;
#define MAXARGVS        100
	
    for (i = 1;i < myargc;i++)
	if (myargv[i][0] == '@')
	{
	    FILE *          handle;
	    int             size;
	    int             k;
	    int             index;
	    int             indexinfile;
	    char    *infile;
	    char    *file;
	    char    *moreargs[20];
	    char    *firstargv;
			
	    // READ THE RESPONSE FILE INTO MEMORY
	    handle = fopen (&myargv[i][1],"rb");
	    if (!handle)
	    {
		printf ("\nNo such response file!");
		exit(1);
	    }
	    printf("Found response file %s!\n",&myargv[i][1]);
	    fseek (handle,0,SEEK_END);
	    size = ftell(handle);
	    fseek (handle,0,SEEK_SET);
	    file = malloc (size);
	    fread (file,size,1,handle);
	    fclose (handle);
			
	    // KEEP ALL CMDLINE ARGS FOLLOWING @RESPONSEFILE ARG
	    for (index = 0,k = i+1; k < myargc; k++)
		moreargs[index++] = myargv[k];
			
	    firstargv = myargv[0];
	    myargv = malloc(sizeof(char *)*MAXARGVS);
	    memset(myargv,0,sizeof(char *)*MAXARGVS);
	    myargv[0] = firstargv;
			
	    infile = file;
	    indexinfile = k = 0;
	    indexinfile++;  // SKIP PAST ARGV[0] (KEEP IT)
	    do
	    {
		myargv[indexinfile++] = infile+k;
		while(k < size &&
		      ((*(infile+k)>= ' '+1) && (*(infile+k)<='z')))
		    k++;
		*(infile+k) = 0;
		while(k < size &&
		      ((*(infile+k)<= ' ') || (*(infile+k)>'z')))
		    k++;
	    } while(k < size);
			
	    for (k = 0;k < index;k++)
		myargv[indexinfile++] = moreargs[k];
	    myargc = indexinfile;
	
	    // DISPLAY ARGS
	    printf("%d command-line args:\n",myargc);
	    for (k=1;k<myargc;k++)
		printf("%s\n",myargv[k]);

	    break;
	}
}
*/

void D_SetPlayerPtrs(void)
{
	int i;
	
	for(i=0; i<MAXPLAYERS; i++)
	{
		players[i].plr = DD_GetPlayer(i);
		players[i].plr->extradata = (void*) &players[i];
	}
}

//===========================================================================
// D_PreInit
//===========================================================================
void D_PreInit(void)
{
	int		i;

	if(gi.version < DOOMSDAY_VERSION) Con_Error("jDoom requires at least "
		"Doomsday "DOOMSDAY_VERSION_TEXT"!\n");

	// Setup the DGL interface.
	G_InitDGL();

	// Config defaults. The real settings are read from the .cfg files
	// but these will be used no such files are found.
	memset(&cfg, 0, sizeof(cfg));
	cfg.dclickuse = true;
	cfg.mouseSensiX = cfg.mouseSensiY = 8;
	cfg.povLookAround = true;
	cfg.joyaxis[0] = JOYAXIS_TURN;
	cfg.joyaxis[1] = JOYAXIS_MOVE;
	cfg.sbarscale = 20;	// Full size.
	cfg.echoMsg = true;
	cfg.lookSpeed = 3;
	cfg.menuScale = .9f;
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
	cfg.hudScale = .6f;
	cfg.hudColor[0] = 1;
	cfg.hudColor[1] = cfg.hudColor[2] = 0;
	cfg.xhairSize = 1;
	for(i = 0; i < 4; i++) cfg.xhairColor[i] = 255;
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
	cfg.automapAlpha = .6f;
	cfg.automapLineAlpha = 1;
	cfg.msgCount = 4;
	cfg.msgScale = .8f;
	cfg.msgUptime = 5 * TICSPERSEC;
	cfg.msgBlink = true;
	cfg.customMusic = true;
	cfg.killMessages = true;
	cfg.bobWeapon = 1;
	cfg.bobView = 1;
	cfg.cameraNoClip = true;

	D_SetPlayerPtrs();
	DD_SetConfigFile("jDoom.cfg");
	DD_SetDefsFile("jDoom\\jDoom.ded");
	Set(DD_HIGHRES_TEXTURE_PATH, (int) ">Data\\jDoom\\Textures\\");
	Con_DefineActions(actions);
	Set(DD_SKYFLAT_NAME, (int) SKYFLATNAME);
	// Add the JDoom cvars and ccmds to the console databases.
	D_ConsoleRegistration();

	DD_AddStartupWAD(">Data\\jDoom\\jDoom.wad");	// FONTA and FONTB, M_THERM2

	DetectIWADs();

    /*p = ArgCheck ("-playdemo");
    if(!p) p = ArgCheck ("-timedemo");
    if(p && p < myargc-1)
    {
		sprintf(file, "%s.lmp", gi.Argv(p+1));
		gi.AddStartupWAD(file);
		Con_Message("Playing demo %s.lmp.\n", gi.Argv(p+1));
    }*/
	SV_Init();
    modifiedgame = false;
}

char *BorderLumps[] = 
{
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
//	Called after engine init has been completed.
//===========================================================================
void D_PostInit(void)
{
	int		p;
	char	file[256];

	Con_Message("jDoom "VERSIONTEXT"\n");

	XG_ReadTypes();

	D_IdentifyVersion();
	D_DefaultBindings();
	R_SetViewSize(screenblocks, 0);
	G_SetGlowing();

	// Initialize weapon info using definitions.
	P_InitWeaponInfo();

	// Game parameters.
	monsterinfight = GetDefInt("AI|Infight", 0);
    nomonsters = ArgCheck ("-nomonsters");
    respawnparm = ArgCheck ("-respawn");
    fastparm = ArgCheck ("-fast");
    devparm = ArgCheck ("-devparm");
    if(ArgCheck("-altdeath")) cfg.netDeathmatch = 2;
    else if(ArgCheck("-deathmatch")) cfg.netDeathmatch = 1;

	// Print a game mode banner with rulers.
	Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER, 
		gamemode == retail? 
		"The Ultimate DOOM Startup\n"
		: gamemode == shareware?
		"DOOM Shareware Startup\n"
		: gamemode == registered?
		"DOOM Registered Startup\n"
		: gamemode == commercial?
		(gamemission == pack_plut?
			"Final DOOM: The Plutonia Experiment\n"
			: gamemission == pack_tnt? "Final DOOM: TNT: Evilution\n"
			: "DOOM 2: Hell on Earth\n")
		: "Public DOOM\n");
	Con_FPrintf(CBLF_RULER, "");

	if(gamemode == commercial) // Doom2 has a different background.
		BorderLumps[0] = "GRNROCK";
	R_SetBorderGfx(BorderLumps);

    // get skill / episode / map from parms
    gameskill = startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
	autostart = false;
		
    p = ArgCheck ("-skill");
    if(p && p < myargc-1)
    {
		startskill = Argv(p+1)[0] - '1';
		autostart = true;
    }

    p = ArgCheck ("-episode");
    if(p && p < myargc-1)
    {
		startepisode = Argv(p+1)[0] - '0';
		startmap = 1;
		autostart = true;
    }
	
    p = ArgCheck ("-timer");
    if(p && p < myargc-1 && deathmatch)
    {
		int time;
		time = atoi(Argv(p+1));
		Con_Message("Levels will end after %d minute", time);
		if(time>1) Con_Message("s");
		Con_Message(".\n");
    }

    p = ArgCheck ("-avg");
    if(p && p < myargc-1 && deathmatch)
		Con_Message("Austin Virtual Gaming: Levels will end after 20 minutes\n");

    p = ArgCheck ("-warp");
    if(p && p < myargc-1)
    {
		if(gamemode == commercial)
			startmap = atoi(Argv(p+1));
		else
		{
			startepisode = Argv(p+1)[0]-'0';
			startmap = Argv(p+2)[0]-'0';
		}
		autostart = true;
    }

    // turbo option
    if((p = ArgCheck("-turbo")))
    {
		int			scale = 200;
		extern int	forwardmove[2];
		extern int	sidemove[2];
	
		if(p < myargc-1)
			scale = atoi(Argv(p+1));
		if (scale < 10) scale = 10;
		if (scale > 400) scale = 400;
		Con_Message("turbo scale: %i%%\n", scale);
		forwardmove[0] = forwardmove[0]*scale/100;
		forwardmove[1] = forwardmove[1]*scale/100;
		sidemove[0] = sidemove[0]*scale/100;
		sidemove[1] = sidemove[1]*scale/100;
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
    }*/

    Con_Message("P_Init: Init Playloop state.\n");
    P_Init();

    //printf ("S_Init: Setting up sound.\n");
    //S_Init();//snd_SfxVolume /* *8 */, snd_MusicVolume /* *8*/ );

    Con_Message("HU_Init: Setting up heads up display.\n");
    HU_Init();

    Con_Message("ST_Init: Init status bar.\n");
    ST_Init();

    Con_Message("M_Init: Init miscellaneous info.\n");
    M_Init();

    // start the apropriate game based on parms
/*    p = ArgCheck ("-record");
    if (p && p < myargc-1)
	{
		G_RecordDemo (myargv(p+1));
		autostart = true;
    }*/
	
/*    p = ArgCheck ("-playdemo");
    if (p && p < myargc-1)
    {
		singledemo = true;              // quit after one demo
		G_DeferedPlayDemo(myargv(p+1));
		return;
    }*/
	
/*    p = ArgCheck ("-timedemo");
    if (p && p < myargc-1)
    {
		G_TimeDemo (myargv(p+1));
		return;
    }*/
	
    p = ArgCheck ("-loadgame");
    if (p && p < myargc-1)
    {
		SV_SaveGameFile(Argv(p+1)[0] - '0', file);
		G_LoadGame(file);
    }

    if(gameaction != ga_loadgame)
    {
		if(autostart || IS_NETGAME)
		{
			G_InitNew(startskill, startepisode, startmap);
			//if(demorecording) G_BeginRecording();
		}
		else
			D_StartTitle();                // start up intro loop
    }
}

void D_Shutdown(void)
{
}

void D_Ticker(void)
{
	if(advancedemo) D_DoAdvanceDemo();
	M_Ticker();
	G_Ticker();
}

void D_EndFrame(void)
{
//	S_UpdateSounds(players[displayplayer].plr->mo);
}


char *D_Get(int id)
{
	switch(id)
	{
	case DD_GAME_ID:
		return "jDoom "VERSION_TEXT;

	case DD_GAME_MODE:
		return gameModeString;

	case DD_GAME_CONFIG:
		return gameConfigString;

	case DD_VERSION_SHORT:
		return VERSION_TEXT;

	case DD_VERSION_LONG:
		return VERSIONTEXT"\njDoom is based on linuxdoom-1.10.";

	case DD_ACTION_LINK:
		return (char*) actionlinks;

	case DD_PSPRITE_BOB_X:
		return (char*) (FRACUNIT + FixedMul(
			FixedMul(FRACUNIT*cfg.bobWeapon, players[consoleplayer].bob), 
			finecosine[(128 * leveltime) & FINEMASK]));

	case DD_PSPRITE_BOB_Y:
		return (char*) (32*FRACUNIT + FixedMul(
			FixedMul(FRACUNIT*cfg.bobWeapon, players[consoleplayer].bob),
			finesine[(128 * leveltime) & FINEMASK & (FINEANGLES/2-1)]));

	default:
		break;
	}
	// ID not recognized, return NULL.
	return 0;
}

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
//	glexp = (gl_export_t*) GL_GetAPI();
//	memset(&gl, 0, sizeof(gl));
//	memcpy(&gl, glexp, MIN_OF(sizeof(gl_export_t), glexp->apiSize));

	// Clear all of our exports.
	memset(&gx, 0, sizeof(gx));

	// Fill in the data for the exports.
	gx.apiSize = sizeof(gx);
	gx.PreInit = D_PreInit;
	gx.PostInit = D_PostInit;
	gx.Shutdown = D_Shutdown;
	gx.BuildTicCmd = G_BuildTiccmd;
	gx.DiscardTicCmd = G_DiscardTiccmd;
	gx.Ticker = D_Ticker;
	gx.G_Drawer = D_Display;
	gx.MN_Drawer = M_Drawer;
	gx.PrivilegedResponder = D_PrivilegedResponder;
	gx.MN_Responder = M_Responder;
	gx.G_Responder = G_Responder;
	gx.MobjThinker = P_MobjThinker;
	gx.MobjFriction = P_GetMobjFriction;
	gx.EndFrame = D_EndFrame;
	gx.ConsoleBackground = D_ConsoleBg;
	gx.UpdateState = G_UpdateState;
#undef Get
	gx.Get = D_Get;
	gx.R_Init = R_Init;

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
