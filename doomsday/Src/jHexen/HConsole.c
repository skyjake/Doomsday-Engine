
//**************************************************************************
//**
//** HCONSOLE.C
//**
//** Hexen specific console stuff.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "settings.h"
#include "d_net.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define DEFCC(x)	int x(int argc, char **argv)
#define OBSOLETE	CVF_HIDE|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void S_InitScript();
void SN_InitSequenceScript(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int CCmdPause(int argc, char **argv);
int CCmdCheat(int argc, char **argv);
int CCmdScriptInfo(int argc, char **argv);
int CCmdSuicide(int argc, char **argv);
int CCmdSetDemoMode(int argc, char **argv);
int CCmdCrosshair(int argc, char **argv);
int CCmdViewSize(int argc, char **argv);
int CCmdInventory(int argc, char **argv);
int CCmdScreenShot(int argc, char **argv);

DEFCC(CCmdHexenFont);
DEFCC(CCmdMenuAction);
DEFCC(CCmdCycleSpy);
DEFCC(CCmdTest);
DEFCC(CCmdSpawnMobj);
DEFCC(CCmdPrintPlayerCoords);
DEFCC(CCmdMovePlane);

// The cheats.
int CCmdCheatGod(int argc, char **argv);
int CCmdCheatClip(int argc, char **argv);
int CCmdCheatGive(int argc, char **argv);
int CCmdCheatWarp(int argc, char **argv);
int CCmdCheatPig(int argc, char **argv);
int CCmdCheatMassacre(int argc, char **argv);
int CCmdCheatShadowcaster(int argc, char **argv);
int CCmdCheatWhere(int argc, char **argv);
int CCmdCheatRunScript(int argc, char **argv);
int CCmdCheatReveal(int argc, char **argv);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern ccmd_t netCCmds[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int consoleFlat = 60;
float consoleZoom = 1;


cvar_t gameCVars[] =
{
	"i_MouseSensiX", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25, "Mouse X axis sensitivity.", 
	"i_MouseSensiY", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25, "Mouse Y axis sensitivity.", 
	"i_jLookInvY", OBSOLETE, CVT_INT, &cfg.jlookInverseY, 0, 1, "1=Inverse joystick look Y axis.", 
	"i_mLookInvY", OBSOLETE, CVT_INT, &cfg.mlookInverseY, 0, 1, "1=Inverse mouse look Y axis.", 
	"i_JoyXAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[0], 0, 4, "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.", 
	"i_JoyYAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[1], 0, 4, "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.", 
	"i_JoyZAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[2], 0, 4, "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.", 
//	"i_JoyDeadZone", OBSOLETE, CVT_INT, &cfg.joydead, 10, 90, "Joystick dead zone, in percents.", 
	"FPS", OBSOLETE|CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1, "1=Show the frames per second counter.", 
	"EchoMsg", OBSOLETE, CVT_INT, &cfg.echoMsg, 0, 1, "1=Echo all messages to the console.", 
	"IceCorpse", OBSOLETE, CVT_INT, &cfg.translucentIceCorpse, 0, 1, "1=Translucent frozen monsters.", 
	"ImmediateUse", OBSOLETE, CVT_INT, &cfg.chooseAndUse, 0, 1, "1=Use items immediately from the inventory.", 
	"LookSpeed", OBSOLETE, CVT_INT, &cfg.lookSpeed, 1, 5, "The speed of looking up/down.", 
	"bgFlat", OBSOLETE|CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0, "The number of the flat to use for the console background.", 
	"bgZoom", OBSOLETE, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f, "Zoom factor for the console background.", 
	"povLook", OBSOLETE, CVT_BYTE, &cfg.povLookAround, 0, 1, "1=Look around using the POV hat.", 
	"i_mLook", OBSOLETE, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.", 
	"i_jLook", OBSOLETE, CVT_INT, &cfg.usejlook, 0, 1, "1=Joystick look active.", 
	"AlwaysRun", OBSOLETE, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.", 
	"LookSpring", OBSOLETE, CVT_INT, &cfg.lookSpring, 0, 1, "1=Lookspring active.", 
	"NoAutoAim", OBSOLETE, CVT_INT, &cfg.noAutoAim, 0, 1, "1=Autoaiming disabled.", 
	"h_ViewSize", OBSOLETE|CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 11, "View window size (3-11).", 
	"h_sbSize", OBSOLETE|CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20, "Status bar size (1-20).", 
	"dClickUse", OBSOLETE, CVT_INT, &cfg.dclickuse, 0, 1, "1=Double click forward/strafe equals pressing the use key.", 
	"XHair", OBSOLETE|CVF_NO_MAX|CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0, "The current crosshair.", 
	"XHairR", OBSOLETE, CVT_BYTE, &cfg.xhairColor[0], 0, 255, "Red crosshair color component.", 
	"XHairG", OBSOLETE, CVT_BYTE, &cfg.xhairColor[1], 0, 255, "Green crosshair color component.", 
	"XHairB", OBSOLETE, CVT_BYTE, &cfg.xhairColor[2], 0, 255, "Blue crosshair color component.", 
	"XHairSize", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.xhairSize, 0, 0, "Crosshair size: 1=Normal.", 
	"s_3D", OBSOLETE, CVT_INT, &cfg.snd_3D, 0, 1, "1=Play sounds in 3D.", 
	"s_ReverbVol", OBSOLETE, CVT_FLOAT, &cfg.snd_ReverbFactor, 0, 1, "General reverb strength (0-1).", 
	"SoundDebug", OBSOLETE|CVF_NO_ARCHIVE, CVT_INT, &DebugSound, 0, 1, "1=Display sound debug information.", 
	"ReverbDebug", OBSOLETE|CVF_NO_ARCHIVE, CVT_BYTE, &cfg.reverbDebug, 0, 1, "1=Reverberation debug information in the console.", 
	"ShowMana", OBSOLETE, CVT_INT, &cfg.showFullscreenMana, 0, 2, "Show mana when the status bar is hidden.", 
	//"SaveDir", OBSOLETE|CVF_PROTECTED, CVT_CHARPTR, &SavePath, 0, 0, "The directory for saved games.", 
	"ChatMacro0", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[0], 0, 0, "Chat macro 1.", 
	"ChatMacro1", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[1], 0, 0, "Chat macro 2.", 
	"ChatMacro2", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[2], 0, 0, "Chat macro 3.", 
	"ChatMacro3", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[3], 0, 0, "Chat macro 4.", 
	"ChatMacro4", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[4], 0, 0, "Chat macro 5.", 
	"ChatMacro5", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[5], 0, 0, "Chat macro 6.", 
	"ChatMacro6", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[6], 0, 0, "Chat macro 7.", 
	"ChatMacro7", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[7], 0, 0, "Chat macro 8.", 
	"ChatMacro8", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[8], 0, 0, "Chat macro 9.", 
	"ChatMacro9", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[9], 0, 0, "Chat macro 10.", 
	"NoMonsters", OBSOLETE, CVT_BYTE, &cfg.netNomonsters, 0, 1, "1=No monsters.", 
	"RandClass", OBSOLETE, CVT_BYTE, &cfg.netRandomclass, 0, 1, "1=Respawn in a random class (deathmatch).", 
	"n_Skill", OBSOLETE, CVT_BYTE, &cfg.netSkill, 0, 4, "Skill level in multiplayer games.", 
	"n_Map", OBSOLETE, CVT_BYTE, &cfg.netMap, 1, 99, "Map to use in multiplayer games.", 
	"Deathmatch", OBSOLETE, CVT_BYTE, &cfg.netDeathmatch, 0, 1, "1=Start multiplayers games as deathmatch.", 
	"n_Class", OBSOLETE, CVT_BYTE, &cfg.netClass, 0, 2, "Player class in multiplayer games.", 
	"n_Color", OBSOLETE, CVT_BYTE, &cfg.netColor, 0, 8, "Player color: 0=blue, 1=red, 2=yellow, 3=green, 4=jade, 5=white, \n6=hazel, 7=purple, 8=auto.", 
	"n_mobDamage", OBSOLETE, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100, "Enemy (mob) damage modifier, multiplayer (1..100).", 
	"n_mobHealth", OBSOLETE, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20, "Enemy (mob) health modifier, multiplayer (1..20).", 
	"OverrideHubMsg", OBSOLETE, CVT_BYTE, &cfg.overrideHubMsg, 0, 2, "Override the transition hub message.", 
	"DemoDisabled", OBSOLETE, CVT_BYTE, &cfg.demoDisabled, 0, 2, "Disable demos.", 
	"MenuScale", OBSOLETE, CVT_FLOAT, &cfg.menuScale, .1f, 1, "Scaling for menu screens.", 
	"MaulatorTime", OBSOLETE|CVF_NO_MAX, CVT_INT, &MaulatorSeconds, 1, 0, "Dark Servant lifetime, in seconds (default: 25).", 
	"FastMonsters", OBSOLETE, CVT_BYTE, &cfg.fastMonsters, 0, 1, "1=Fast monsters in non-demo single player.", 
	"MapTitle", OBSOLETE, CVT_BYTE, &cfg.mapTitle, 0, 1, "1=Show map title after entering map.", 

//===========================================================================
// New names (1.1.0 =>)
//===========================================================================
	"chat-macro0",				0,			CVT_CHARPTR, &cfg.chat_macros[0], 0, 0, "Chat macro 1.",
	"chat-macro1",				0,			CVT_CHARPTR, &cfg.chat_macros[1], 0, 0, "Chat macro 2.",
	"chat-macro2",				0,			CVT_CHARPTR, &cfg.chat_macros[2], 0, 0, "Chat macro 3.",
	"chat-macro3",				0,			CVT_CHARPTR, &cfg.chat_macros[3], 0, 0, "Chat macro 4.",
	"chat-macro4",				0,			CVT_CHARPTR, &cfg.chat_macros[4], 0, 0, "Chat macro 5.",
	"chat-macro5",				0,			CVT_CHARPTR, &cfg.chat_macros[5], 0, 0, "Chat macro 6.",
	"chat-macro6",				0,			CVT_CHARPTR, &cfg.chat_macros[6], 0, 0, "Chat macro 7.",
	"chat-macro7",				0,			CVT_CHARPTR, &cfg.chat_macros[7], 0, 0, "Chat macro 8.",
	"chat-macro8",				0,			CVT_CHARPTR, &cfg.chat_macros[8], 0, 0, "Chat macro 9.",
	"chat-macro9",				0,			CVT_CHARPTR, &cfg.chat_macros[9], 0, 0, "Chat macro 10.",
	"con-flat",				CVF_NO_MAX,		CVT_INT,	&consoleFlat,		0, 0,	"The number of the flat to use for the console background.",
	"con-zoom",					0,			CVT_FLOAT,	&consoleZoom,		0.1f, 100.0f, "Zoom factor for the console background.",
	"view-cross-r",		0,			CVT_BYTE,	&cfg.xhairColor[0], 0, 255,	"Crosshair color red component.",
	"view-cross-g",		0,			CVT_BYTE,	&cfg.xhairColor[1], 0, 255, "Crosshair color green component.",
	"view-cross-b",		0,			CVT_BYTE,	&cfg.xhairColor[2], 0, 255, "Crosshair color blue component.",
	"view-cross-a",		0,			CVT_BYTE,	&cfg.xhairColor[3], 0, 255, "Crosshair color alpha component.",
	"view-cross-size",		CVF_NO_MAX,		CVT_INT,	&cfg.xhairSize,		0, 0,	"Crosshair size: 1=Normal.",
	"view-cross-type", CVF_NO_MAX|CVF_PROTECTED, CVT_INT,	&cfg.xhair,			0, 0,	"The current crosshair.",
	"view-bob-height",			0,			CVT_FLOAT,	&cfg.bobView,		0, 1,	"Scaling factor for viewheight bobbing.",
	"view-bob-weapon",			0,			CVT_FLOAT,	&cfg.bobWeapon,		0, 1,	"Scaling factor for player weapon bobbing.",
	"ctl-aim-noauto",			0,			CVT_INT,	&cfg.noAutoAim,		0, 1,	"1=Autoaiming disabled.",
	"ctl-look-joy",				0,			CVT_INT,	&cfg.usejlook,		0, 1,	"1=Joystick look active.",
	"ctl-look-joy-inverse",		0,			CVT_INT,	&cfg.jlookInverseY, 0, 1,	"1=Inverse joystick look Y axis.",
	"ctl-look-joy-delta",		0,			CVT_INT,	&cfg.jlookDeltaMode, 0, 1, "1=Joystick values => look angle delta.",
	"ctl-look-mouse",			0,			CVT_INT,	&cfg.usemlook,		0, 1,	"1=Mouse look active.",
	"ctl-look-mouse-inverse",	0,			CVT_INT,	&cfg.mlookInverseY, 0, 1,	"1=Inverse mouse look Y axis.",
	"ctl-look-pov",				0,			CVT_BYTE,	&cfg.povLookAround,	0, 1,	"1=Look around using the POV hat.",
	"ctl-look-speed",			0,			CVT_INT,	&cfg.lookSpeed,		1, 5,	"The speed of looking up/down.",
	"ctl-look-spring",			0,			CVT_INT,	&cfg.lookSpring,	0, 1,	"1=Lookspring active.",
	"ctl-run",					0,			CVT_INT,	&cfg.alwaysRun,		0, 1,	"1=Always run.",
	"ctl-use-dclick",			0,			CVT_INT,	&cfg.dclickuse,		0, 1,	"1=Double click forward/strafe equals pressing the use key.",
	"ctl-use-immediate",		0,			CVT_INT,	&cfg.chooseAndUse,	0, 1,	"1=Use items immediately from the inventory.",
	"game-fastmonsters",		0,			CVT_BYTE,	&cfg.fastMonsters,	0, 1,	"1=Fast monsters in non-demo single player.",
	"game-icecorpse",			0,			CVT_INT,	&cfg.translucentIceCorpse, 0, 1, "1=Translucent frozen monsters.",
	"game-maulator-time",	CVF_NO_MAX,		CVT_INT,	&MaulatorSeconds,	1, 0,	"Dark Servant lifetime, in seconds (default: 25).",
	"hud-fps",				CVF_NO_ARCHIVE,	CVT_INT,	&cfg.showFPS,		0, 1,	"1=Show the frames per second counter.",
	"hud-mana",					0,			CVT_INT,	&cfg.showFullscreenMana, 0, 2, "Show mana when the status bar is hidden.",
	"hud-status-size",		CVF_PROTECTED,	CVT_INT,	&cfg.sbarscale,		1, 20,	"Status bar size (1-20).",
	"hud-title",				0,			CVT_BYTE,	&cfg.mapTitle,		0, 1,		"1=Show map title after entering map.",
	"input-joy-x",				0,			CVT_INT,	&cfg.joyaxis[0],	0, 4,	"X axis control: 0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
	"input-joy-y",				0,			CVT_INT,	&cfg.joyaxis[1],	0, 4,	"Y axis control.",
	"input-joy-z",				0,			CVT_INT,	&cfg.joyaxis[2],	0, 4,	"Z axis control.",
	"input-joy-rx",				0,			CVT_INT,	&cfg.joyaxis[3],	0, 4,	"X rotational axis control.",
	"input-joy-ry",				0,			CVT_INT,	&cfg.joyaxis[4],	0, 4,	"Y rotational axis control.",
	"input-joy-rz",				0,			CVT_INT,	&cfg.joyaxis[5],	0, 4,	"Z rotational axis control.",
	"input-joy-slider1",		0,			CVT_INT,	&cfg.joyaxis[6],	0, 4,	"First slider control.",
	"input-joy-slider2",		0,			CVT_INT,	&cfg.joyaxis[7],	0, 4,	"Second slider control.",
	"input-mouse-x-sensi", CVF_NO_MAX,		CVT_INT,	&cfg.mouseSensiX,	0, 25, "Mouse X axis sensitivity.",
	"input-mouse-y-sensi", CVF_NO_MAX,		CVT_INT,	&cfg.mouseSensiY,	0, 25, "Mouse Y axis sensitivity.",
	"menu-scale",				0,			CVT_FLOAT,	&cfg.menuScale,		.1f, 1,	"Scaling for menu screens.",
	"msg-echo",					0,			CVT_INT,	&cfg.echoMsg,		0, 1,	"1=Echo all messages to the console.",
	"msg-hub-override",			0,			CVT_BYTE,	&cfg.overrideHubMsg,0, 2,	"Override the transition hub message.",
	"player-class",				0,			CVT_BYTE,	&cfg.netClass,		0, 2,	"Player class in multiplayer games.",
	"player-color",				0,			CVT_BYTE,	&cfg.netColor,		0, 8,	"Player color: 0=blue, 1=red, 2=yellow, 3=green, 4=jade, 5=white,\n6=hazel, 7=purple, 8=auto.",
	"player-camera-noclip",		0,			CVT_INT,	&cfg.cameraNoClip,	0, 1,	"1=Camera players have no movement clipping.",
	"player-jump-power",		0,			CVT_FLOAT,	&cfg.jumpPower,		0, 100,	"Jump power.",
	"server-game-deathmatch",	0,			CVT_BYTE,	&cfg.netDeathmatch,	0, 1,	"1=Start multiplayers games as deathmatch.",
	"server-game-map",			0,			CVT_BYTE,	&cfg.netMap,		1, 99,	"Map to use in multiplayer games.",
	"server-game-mod-damage",	0,			CVT_BYTE,	&cfg.netMobDamageModifier, 1, 100,	"Enemy (mob) damage modifier, multiplayer (1..100).",
	"server-game-mod-health",	0,			CVT_BYTE,	&cfg.netMobHealthModifier, 1, 20,	"Enemy (mob) health modifier, multiplayer (1..20).",
	"server-game-nomonsters",	0,			CVT_BYTE,	&cfg.netNomonsters,	0, 1,	"1=No monsters.",
	"server-game-randclass",	0,			CVT_BYTE,	&cfg.netRandomclass,0, 1,	"1=Respawn in a random class (deathmatch).",
	"server-game-skill",		0,			CVT_BYTE,	&cfg.netSkill,		0, 4,	"Skill level in multiplayer games.",
	"view-size",			CVF_PROTECTED,	CVT_INT,	&cfg.screenblocks,	3, 11,	"View window size (3-11).",
	NULL
};

ccmd_t gameCCmds[] =
{
	"cheat",		CCmdCheat,				"Issue a cheat code using the original Hexen cheats.",
	"class",		CCmdCheatShadowcaster,	"Change player class.",
	"noclip",		CCmdCheatClip,			"Movement clipping on/off.",
	"crosshair",	CCmdCrosshair,			"Crosshair settings.",	
#ifdef DEMOCAM
	"demomode",		CCmdSetDemoMode,		"Set demo external camera mode.",
#endif
	"give",			CCmdCheatGive,			"Cheat command to give you various kinds of things.",
	"god",			CCmdCheatGod,			"I don't think He needs any help...",
	"kill",			CCmdCheatMassacre,		"Kill all the monsters on the level.",
	"hexenfont",	CCmdHexenFont,			"Use the Hexen font.",
	"invleft",		CCmdInventory,			"Move inventory cursor to the left.",
	"invright",		CCmdInventory,			"Move inventory cursor to the right.",
//	"midi",			CCmdMidi,				"MIDI music control.",
	"pause",		CCmdPause,				"Pause the game (same as pressing the pause key).",
	"pig",			CCmdCheatPig,			"Turn yourself into a pig. Go ahead.",
	"reveal",		CCmdCheatReveal,		"Map cheat.",
	"runscript",	CCmdCheatRunScript,		"Run a script.",
	"scriptinfo",	CCmdScriptInfo,			"Show information about all scripts or one particular script.",
	"viewsize",		CCmdViewSize,			"Set the view size.",
	"sbsize",		CCmdViewSize,			"Set the status bar size.",
	"screenshot",	CCmdScreenShot,			"Take a screenshot.",
	"warp",			CCmdCheatWarp,			"Warp to a map.",
	"where",		CCmdCheatWhere,			"Prints your map number and exact location.",
	"spy",			CCmdCycleSpy,			"Change the viewplayer when not in deathmatch.",

	// Menu actions.
	"infoscreen",	CCmdMenuAction,			"Display the original Hexen help screens.",
	"savegame",		CCmdMenuAction,			"Save the game.",
	"loadgame",		CCmdMenuAction,			"Load a saved game.",
	"soundmenu",	CCmdMenuAction,			"Open the sound menu.",
	"suicide",		CCmdMenuAction,			"Kill yourself. What did you think?",
	"quicksave",	CCmdMenuAction,			"Quicksave the game.",
	"endgame",		CCmdMenuAction,			"End the current game.",
	"togglemsgs",	CCmdMenuAction,			"Toggle messages on/off (cvar messages).",
	"quickload",	CCmdMenuAction,			"Load the last quicksaved game.",
	"quit",			CCmdMenuAction,			"Quit JHexen.",
	"togglegamma",	CCmdMenuAction,			"Change the gamma correction level.",
	
	"spawnmobj",	CCmdSpawnMobj,			"Spawn a new mobj.",
	"coord",		CCmdPrintPlayerCoords, "Print the coordinates of the consoleplayer.",

	// $democam: console commands
	"makelocp",		CCmdMakeLocal,			"Make local player.",
	"makecam",		CCmdSetCamera,			"Toggle camera mode.",
	"setlock",		CCmdSetViewLock,		"Set camera viewlock.",
	"lockmode",		CCmdSetViewLock,		"Set camera viewlock mode.",

	"startinf",		CCmdStartInFine,	"Start an InFine script.",
	"stopinf",		CCmdStopInFine,		"Stop the currently playing interlude/finale.",
	"stopfinale",	CCmdStopInFine,		"Stop the currently playing interlude/finale.",

	// $moveplane: console commands
/*	"movefloor",	CCmdMovePlane,			"Move a sector's floor plane.",
	"moveceil",		CCmdMovePlane,			"Move a sector's ceiling plane.",
	"movesec",		CCmdMovePlane,			"Move a sector's both planes.",*/

//	"test",			CCmdTest,				"Test.",
	NULL
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

// Add the console variables and commands.
void H2_ConsoleRegistration()
{
	int		i;
	
	for(i=0; gameCVars[i].name; i++) Con_AddVariable(gameCVars+i);
	for(i=0; gameCCmds[i].name; i++) Con_AddCommand(gameCCmds+i);
	D_NetConsoleRegistration();
}

int CCmdViewSize(int argc, char **argv)
{
	int	min=3, max=11, *val = &cfg.screenblocks;

	if(argc != 2)
	{
		Con_Printf( "Usage: %s (size)\n", argv[0]);
		Con_Printf( "Size can be: +, -, (num).\n");
		return true;
	}
	if(!stricmp(argv[0], "sbsize"))
	{
		min = 1;
		max = 20;
		val = &cfg.sbarscale;
	}
	if(!stricmp(argv[1], "+"))
		(*val)++;
	else if(!stricmp(argv[1], "-"))
		(*val)--;
	else
		*val = strtol(argv[1], NULL, 0);

	if(*val < min) *val = min;
	if(*val > max) *val = max;

	// Update the view size if necessary.
	R_SetViewSize(cfg.screenblocks, 0);
	return true;
}

int CCmdScreenShot(int argc, char **argv)
{
	G_ScreenShot();
	return true;
}

DEFCC(CCmdHexenFont)
{
	ddfont_t	cfont;

	cfont.flags = DDFONT_WHITE;
	cfont.height = 9;
	cfont.sizeX = 1.2f;
	cfont.sizeY = 2;
	cfont.TextOut = MN_DrTextA_CS;
	cfont.Width = MN_TextAWidth;
	cfont.Filter = MN_TextFilter;
	Con_SetFont(&cfont);
	return true;
}

#if 0
DEFCC(CCmdTest)
{
	/*int		i;

	for(i=0; i<MAXPLAYERS; i++)
		if(players[i].plr->ingame)
		{
			Con_Printf( "%i: cls:%i col:%i look:%f\n", i, PlayerClass[i],
				PlayerColor[i], players[i].plr->lookdir);
		}*/
	if(argc != 2) return false;
	S_StartSoundAtVolume(NULL, SFX_CHAT, atoi(argv[1]));
	return true;
}
#endif

