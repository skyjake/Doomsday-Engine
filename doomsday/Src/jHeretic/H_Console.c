
//**************************************************************************
//**
//** HCONSOLE.C
//**
//** Heretic specific console stuff.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include "Doomdef.h"
#include "Soundst.h"
#include "settings.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define DEFCC(x)	int x(int argc, char **argv)
#define OBSOLETE	CVF_HIDE|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdPause);
DEFCC(CCmdCheat);
DEFCC(CCmdViewSize);
DEFCC(CCmdInventory);
DEFCC(CCmdScreenShot);
DEFCC(CCmdHereticFont);
DEFCC(CCmdMenuAction);
DEFCC(CCmdCycleSpy);
DEFCC(CCmdCrosshair);
DEFCC(CCmdSpawnMobj);
DEFCC(CCmdPrintPlayerCoords);
DEFCC(CCmdMovePlane);

// The cheats.
DEFCC(CCmdCheatGod); 
DEFCC(CCmdCheatClip); 
DEFCC(CCmdCheatGive); 
DEFCC(CCmdCheatWarp); 
DEFCC(CCmdCheatPig); 
DEFCC(CCmdCheatMassacre); 
DEFCC(CCmdCheatWhere); 
DEFCC(CCmdCheatReveal); 
DEFCC(CCmdBeginChat);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char *chat_macros[10];

extern int testcvar;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int consoleFlat = 6;
float consoleZoom = 1;

cvar_t gameCVars[] =
{
	"i_MouseSensiX", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 100, "Mouse X axis sensitivity.", 
	"i_MouseSensiY", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 100, "Mouse Y axis sensitivity.", 
	"i_jLookInvY", OBSOLETE, CVT_INT, &cfg.jlookInverseY, 0, 1, "1=Inverse joystick look Y axis.", 
	"i_mLookInvY", OBSOLETE, CVT_INT, &cfg.mlookInverseY, 0, 1, "1=Inverse mouse look Y axis.", 
	"i_JoyXAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[0], 0, 4, "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.", 
	"i_JoyYAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[1], 0, 4, "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.", 
	"i_JoyZAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[2], 0, 4, "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.", 
	//"i_JoyDeadZone", OBSOLETE, CVT_INT, &cfg.joydead, 10, 90, "Joystick dead zone, in percents.", 
	"FPS", OBSOLETE|CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1, "1=Show the frames per second counter.", 
	"EchoMsg", OBSOLETE, CVT_INT, &cfg.echoMsg, 0, 1, "1=Echo all messages to the console.", 
	"ImmediateUse", OBSOLETE, CVT_INT, &cfg.chooseAndUse, 0, 1, "1=Use items immediately from the inventory.", 
	"LookSpeed", OBSOLETE, CVT_INT, &cfg.lookSpeed, 1, 5, "The speed of looking up/down.", 
	"dClickUse", OBSOLETE, CVT_INT, &cfg.dclickuse, 0, 1, "1=Doubleclick forward/strafe equals use key.", 
	"bgFlat", OBSOLETE|CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0, "The number of the flat to use for the console background.", 
	"bgZoom", OBSOLETE, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f, "Zoom factor for the console background.", 
	"PovLook", OBSOLETE, CVT_BYTE, &cfg.povLookAround, 0, 1, "1=Look around using the POV hat.", 
	"i_mLook", OBSOLETE, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.", 
	"i_jLook", OBSOLETE, CVT_INT, &cfg.usejlook, 0, 1, "1=Joystick look active.", 
	"AlwaysRun", OBSOLETE, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.", 
	"LookSpring", OBSOLETE, CVT_INT, &cfg.lookSpring, 0, 1, "1=Lookspring active.", 
	"NoAutoAim", OBSOLETE, CVT_INT, &cfg.noAutoAim, 0, 1, "1=Autoaiming disabled.", 
	"h_ViewSize", OBSOLETE|CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 11, "View window size (3-11).", 
	"h_sbSize", OBSOLETE|CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20, "Status bar size (1-20).", 
	"XHair", OBSOLETE|CVF_NO_MAX|CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0, "The current crosshair.", 
	"XHairR", OBSOLETE, CVT_BYTE, &cfg.xhairColor[0], 0, 255, "Red crosshair color component.", 
	"XHairG", OBSOLETE, CVT_BYTE, &cfg.xhairColor[1], 0, 255, "Green crosshair color component.", 
	"XHairB", OBSOLETE, CVT_BYTE, &cfg.xhairColor[2], 0, 255, "Blue crosshair color component.", 
	"XHairSize", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.xhairSize, 0, 0, "Crosshair size: 1=Normal.", 
//	"s_3D", OBSOLETE, CVT_INT, &cfg.snd_3D, 0, 1, "1=Play sounds in 3D.", 
//	"s_ReverbVol", OBSOLETE, CVT_FLOAT, &cfg.snd_ReverbFactor, 0, 1, "General reverb strength (0-1).", 
	"s_Custom", OBSOLETE, CVT_BYTE, &cfg.customMusic, 0, 1, "1=Enable custom (external) music files.\n", 
//	"SoundDebug", OBSOLETE|CVF_NO_ARCHIVE, CVT_INT, &DebugSound, 0, 1, "1=Display sound debug information.", 
	//"ReverbDebug", OBSOLETE|CVF_NO_ARCHIVE, CVT_BYTE, &cfg.reverbDebug, 0, 1, "1=Reverberation debug information in the console.", 
	"Messages", OBSOLETE, CVT_INT, &cfg.messageson, 0, 1, "1=Show messages.", 
	"ShowMana", OBSOLETE, CVT_INT, &cfg.showFullscreenMana, 0, 1, "1=Show mana when the status bar is hidden.", 
	"ShowArmor", OBSOLETE, CVT_INT, &cfg.showFullscreenArmor, 0, 1, "1=Show armor when the status bar is hidden.", 
	"ShowKeys", OBSOLETE, CVT_INT, &cfg.showFullscreenKeys, 0, 1, "1=Show keys when the status bar is hidden.", 
	"ChatMacro0", OBSOLETE, CVT_CHARPTR, &chat_macros[0], 0, 0, "Chat macro 1.", 
	"ChatMacro1", OBSOLETE, CVT_CHARPTR, &chat_macros[1], 0, 0, "Chat macro 2.", 
	"ChatMacro2", OBSOLETE, CVT_CHARPTR, &chat_macros[2], 0, 0, "Chat macro 3.", 
	"ChatMacro3", OBSOLETE, CVT_CHARPTR, &chat_macros[3], 0, 0, "Chat macro 4.", 
	"ChatMacro4", OBSOLETE, CVT_CHARPTR, &chat_macros[4], 0, 0, "Chat macro 5.", 
	"ChatMacro5", OBSOLETE, CVT_CHARPTR, &chat_macros[5], 0, 0, "Chat macro 6.", 
	"ChatMacro6", OBSOLETE, CVT_CHARPTR, &chat_macros[6], 0, 0, "Chat macro 7.", 
	"ChatMacro7", OBSOLETE, CVT_CHARPTR, &chat_macros[7], 0, 0, "Chat macro 8.", 
	"ChatMacro8", OBSOLETE, CVT_CHARPTR, &chat_macros[8], 0, 0, "Chat macro 9.", 
	"ChatMacro9", OBSOLETE, CVT_CHARPTR, &chat_macros[9], 0, 0, "Chat macro 10.", 
	"NoMonsters", OBSOLETE, CVT_BYTE, &cfg.netNomonsters, 0, 1, "1=No monsters.", 
	"Respawn", OBSOLETE, CVT_BYTE, &cfg.netRespawn, 0, 1, "1= -respawn was used.", 
	"n_Skill", OBSOLETE, CVT_BYTE, &cfg.netSkill, 0, 4, "Skill level in multiplayer games.", 
	"n_Map", OBSOLETE, CVT_BYTE, &cfg.netMap, 1, 31, "Map to use in multiplayer games.", 
	"n_Episode", OBSOLETE, CVT_BYTE, &cfg.netEpisode, 1, 6, "Episode to use in multiplayer games.", 
	"n_Jump", OBSOLETE, CVT_BYTE, &cfg.netJumping, 0, 1, "1=Allow jumping in multiplayer games.", 
	"Deathmatch", OBSOLETE, CVT_BYTE, &cfg.netDeathmatch, 0, 1, "1=Start multiplayers games as deathmatch.", 
	"n_Color", OBSOLETE, CVT_BYTE, &cfg.netColor, 0, 4, "Player color: 0=green, 1=yellow, 2=red, 3=blue, 4=default.", 
	"RingFilter", OBSOLETE, CVT_INT, &cfg.ringFilter, 1, 2, "Ring effect filter. 1=Brownish, 2=Blue.", 
	"AllowJump", OBSOLETE, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.", 
	"TomeTimer", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0, "Countdown seconds for the Tome of Power.", 
	"TomeSound", OBSOLETE|CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0, "Seconds for countdown sound of Tome of Power.", 
	"FastMonsters", OBSOLETE, CVT_BYTE, &cfg.fastMonsters, 0, 1, "1=Fast monsters in non-demo single player.", 
	"EyeHeight", OBSOLETE, CVT_INT, &cfg.eyeHeight, 41, 54, "Player eye height (the original is 41).", 
	"MenuScale", OBSOLETE, CVT_FLOAT, &cfg.menuScale, .1f, 1, "Scaling for menu screens.", 
	"LevelTitle", OBSOLETE, CVT_BYTE, &cfg.levelTitle, 0, 1, "1=Show level title and author in the beginning.", 
	"CounterCheat", OBSOLETE, CVT_BYTE, &cfg.counterCheat, 0, 63, "6-bit bitfield. Show kills, items and secret counters in automap.", 

//===========================================================================
// New names (1.2.0 =>)
//===========================================================================

	"input-mouse-x-sensi",	CVF_NO_MAX,	CVT_INT,	&cfg.mouseSensiX,	0, 100, "Mouse X axis sensitivity.",
	"input-mouse-y-sensi",	CVF_NO_MAX,	CVT_INT,	&cfg.mouseSensiY,	0, 100, "Mouse Y axis sensitivity.",
	"ctl-look-joy-inverse",	0,		CVT_INT,	&cfg.jlookInverseY, 0, 1,	"1=Inverse joystick look Y axis.",
	"ctl-look-mouse-inverse", 0,	CVT_INT,	&cfg.mlookInverseY, 0, 1,	"1=Inverse mouse look Y axis.",
	"input-joy-x",		0,			CVT_INT,	&cfg.joyaxis[0],	0, 4,	"X axis control: 0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
	"input-joy-y",		0,			CVT_INT,	&cfg.joyaxis[1],	0, 4,	"Y axis control.",
	"input-joy-z",		0,			CVT_INT,	&cfg.joyaxis[2],	0, 4,	"Z axis control.",
	"input-joy-rx",		0,			CVT_INT,	&cfg.joyaxis[3],	0, 4,	"X rotational axis control.",
	"input-joy-ry",		0,			CVT_INT,	&cfg.joyaxis[4],	0, 4,	"Y rotational axis control.",
	"input-joy-rz",		0,			CVT_INT,	&cfg.joyaxis[5],	0, 4,	"Z rotational axis control.",
	"input-joy-slider1", 0,			CVT_INT,	&cfg.joyaxis[6],	0, 4,	"First slider control.",
	"input-joy-slider2", 0,			CVT_INT,	&cfg.joyaxis[7],	0, 4,	"Second slider control.",
//	"input-joy-deadzone",	0,			CVT_INT,	&cfg.joydead,		10, 90,	"Joystick dead zone, in percents.", 
	"player-camera-noclip", 0,		CVT_INT,	&cfg.cameraNoClip,	0, 1,	"1=Camera players have no movement clipping.",
	"ctl-look-joy-delta",	0,			CVT_INT,	&cfg.jlookDeltaMode, 0, 1, "1=Joystick values => look angle delta.",
	"hud-fps",			CVF_NO_ARCHIVE,	CVT_INT,	&cfg.showFPS,		0, 1,	"1=Show the frames per second counter.",
	"msg-echo",			0,			CVT_INT,	&cfg.echoMsg,		0, 1,	"1=Echo all messages to the console.",

	"ctl-use-immediate", 0,			CVT_INT,	&cfg.chooseAndUse,	0, 1,	"1=Use items immediately from the inventory.",
	"ctl-look-speed",	0,			CVT_INT,	&cfg.lookSpeed,		1, 5,	"The speed of looking up/down.",
	"ctl-use-dclick",	0,			CVT_INT,	&cfg.dclickuse,		0, 1,	"1=Doubleclick forward/strafe equals use key.",
	"con-flat",		CVF_NO_MAX,		CVT_INT,	&consoleFlat,		0, 0,	"The number of the flat to use for the console background.",
	"con-zoom",			0,			CVT_FLOAT,	&consoleZoom,		0.1f, 100.0f, "Zoom factor for the console background.",
	"ctl-look-pov",		0,			CVT_BYTE,	&cfg.povLookAround,	0, 1,	"1=Look around using the POV hat.",
	
	"ctl-look-mouse",	0,			CVT_INT,	&cfg.usemlook,		0, 1,	"1=Mouse look active.",
	"ctl-look-joy",		0,			CVT_INT,	&cfg.usejlook,		0, 1,	"1=Joystick look active.",
	"ctl-run",			0,			CVT_INT,	&cfg.alwaysRun,		0, 1,	"1=Always run.",
	"ctl-look-spring",	0,			CVT_INT,	&cfg.lookSpring,	0, 1,	"1=Lookspring active.",
	"ctl-aim-noauto",	0,			CVT_INT,	&cfg.noAutoAim,		0, 1,	"1=Autoaiming disabled.",
	"view-size",	CVF_PROTECTED,	CVT_INT,	&cfg.screenblocks,	3, 11,	"View window size (3-11).",
	"hud-status-size",	CVF_PROTECTED,	CVT_INT,	&cfg.sbarscale,		1, 20,	"Status bar size (1-20).",
	
	"view-cross-type", CVF_NO_MAX|CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0,	"The current crosshair.",
	"view-cross-r",		0,			CVT_BYTE,	&cfg.xhairColor[0], 0, 255,	"Crosshair color red component.",
	"view-cross-g",		0,			CVT_BYTE,	&cfg.xhairColor[1], 0, 255, "Crosshair color green component.",
	"view-cross-b",		0,			CVT_BYTE,	&cfg.xhairColor[2], 0, 255, "Crosshair color blue component.",
	"view-cross-a",		0,			CVT_BYTE,	&cfg.xhairColor[3], 0, 255, "Crosshair color alpha component.",
	"view-cross-size",	CVF_NO_MAX,		CVT_INT,	&cfg.xhairSize,		0, 0,	"Crosshair size: 1=Normal.",

	"view-bob-height",	0,			CVT_FLOAT,	&cfg.bobView,		0, 1,	"Scaling factor for viewheight bobbing.",
	"view-bob-weapon",	0,			CVT_FLOAT,	&cfg.bobWeapon,		0, 1,	"Scaling factor for player weapon bobbing.",

/*	"sound-3d",			0,			CVT_INT,	&cfg.snd_3D,		0, 1,	"1=Play sounds in 3D.",
	"sound-reverb-volume", 0,		CVT_FLOAT,	&cfg.snd_ReverbFactor, 0, 1, "General reverb strength (0-1).",
*/	"music-custom",		0,			CVT_BYTE,	&cfg.customMusic,	0, 1,	"1=Enable custom (external) music files.\n",
//	"sound-debug",	CVF_NO_ARCHIVE,	CVT_INT,	&DebugSound,		0, 1,	"1=Display sound debug information.",
//	"sound-reverb-debug", CVF_NO_ARCHIVE,	CVT_BYTE,	&cfg.reverbDebug,	0, 1,	"1=Reverberation debug information in the console.",

	"msg-show",			0,			CVT_INT,	&cfg.messageson,	0, 1,	"1=Show messages.",
	"hud-mana",			0,			CVT_INT,	&cfg.showFullscreenMana, 0, 1, "1=Show mana when the status bar is hidden.",
	"hud-armor",		0,			CVT_INT,	&cfg.showFullscreenArmor, 0, 1, "1=Show armor when the status bar is hidden.",
	"hud-keys",			0,			CVT_INT,	&cfg.showFullscreenKeys, 0, 1,	"1=Show keys when the status bar is hidden.",

	"chat-macro0",		0,			CVT_CHARPTR, &chat_macros[0],	0, 0, "Chat macro 1.",
	"chat-macro1",		0,			CVT_CHARPTR, &chat_macros[1],	0, 0, "Chat macro 2.",
	"chat-macro2",		0,			CVT_CHARPTR, &chat_macros[2],	0, 0, "Chat macro 3.",
	"chat-macro3",		0,			CVT_CHARPTR, &chat_macros[3],	0, 0, "Chat macro 4.",
	"chat-macro4",		0,			CVT_CHARPTR, &chat_macros[4],	0, 0, "Chat macro 5.",
	"chat-macro5",		0,			CVT_CHARPTR, &chat_macros[5],	0, 0, "Chat macro 6.",
	"chat-macro6",		0,			CVT_CHARPTR, &chat_macros[6],	0, 0, "Chat macro 7.",
	"chat-macro7",		0,			CVT_CHARPTR, &chat_macros[7],	0, 0, "Chat macro 8.",
	"chat-macro8",		0,			CVT_CHARPTR, &chat_macros[8],	0, 0, "Chat macro 9.",
	"chat-macro9",		0,			CVT_CHARPTR, &chat_macros[9],	0, 0, "Chat macro 10.",

	// Game settings for servers.
	"server-game-nomonsters",	0,		CVT_BYTE,	&cfg.netNomonsters,	0, 1,	"1=No monsters.",
	"server-game-respawn",		0,		CVT_BYTE,	&cfg.netRespawn,	0, 1,	"1= -respawn was used.",
	"server-game-skill",		0,		CVT_BYTE,	&cfg.netSkill,		0, 4,	"Skill level in multiplayer games.",
	"server-game-map",			0,		CVT_BYTE,	&cfg.netMap,		1, 31,	"Map to use in multiplayer games.",
	"server-game-episode",		0,		CVT_BYTE,	&cfg.netEpisode,	1, 6,	"Episode to use in multiplayer games.",
	"server-game-jump",			0,		CVT_BYTE,	&cfg.netJumping,	0, 1,	"1=Allow jumping in multiplayer games.",
	"server-game-deathmatch",	0,		CVT_BYTE,	&cfg.netDeathmatch,	0, 1,	"1=Start multiplayers games as deathmatch.",

	// Player data.
	"player-color",			0,			CVT_BYTE,	&cfg.netColor,		0, 4,	"Player color: 0=green, 1=yellow, 2=red, 3=blue, 4=default.",

	"view-ringfilter",		0,			CVT_INT,	&cfg.ringFilter,	1, 2,	"Ring effect filter. 1=Brownish, 2=Blue.",
	"player-jump",			0,			CVT_INT,	&cfg.jumpEnabled,	0, 1,	"1=Allow jumping.",
	"player-jump-power",	0,			CVT_FLOAT,	&cfg.jumpPower,		0, 100,	"Jump power.",
	"hud-tome-timer",	CVF_NO_MAX,		CVT_INT,	&cfg.tomeCounter,	0, 0,	"Countdown seconds for the Tome of Power.",
	"hud-tome-sound",	CVF_NO_MAX,		CVT_INT,	&cfg.tomeSound,		0, 0,	"Seconds for countdown sound of Tome of Power.",
	"game-fastmonsters",	0,			CVT_BYTE,	&cfg.fastMonsters,	0, 1,	"1=Fast monsters in non-demo single player.",
	"player-eyeheight",		0,			CVT_INT,	&cfg.eyeHeight,		41, 54,	"Player eye height (the original is 41).",
	"menu-scale",			0,			CVT_FLOAT,	&cfg.menuScale,		.1f, 1,	"Scaling for menu screens.",
	"hud-title",			0,			CVT_BYTE,	&cfg.levelTitle,	0, 1,	"1=Show level title and author in the beginning.",
	"map-cheat-counter",	0,			CVT_BYTE,	&cfg.counterCheat,	0, 63,	"6-bit bitfield. Show kills, items and secret counters in automap.",
	"map-cheat-counter-scale", 0,		CVT_FLOAT,	&cfg.counterCheatScale, .1f, 1, "Size factor for the counters in the automap.",
	"game-corpsetime",	CVF_NO_MAX,		CVT_INT,	&cfg.corpseTime,	0, 0,	"Number of seconds after which dead monsters disappear.",

	"xg-dev",			0,				CVT_INT,	&xgDev,				0, 1,	"1=Print XG debug messages.",

	NULL
};

ccmd_t gameCCmds[] =
{
	"cheat",		CCmdCheat,				"Issue a cheat code using the original Hexen cheats.",
	"noclip",		CCmdCheatClip,			"Movement clipping on/off.",
	"crosshair",	CCmdCrosshair,			"Crosshair settings.",	
/*#ifdef DEMOCAM
	"demomode",		CCmdSetDemoMode,		"Set demo external camera mode.",
#endif*/
	"give",			CCmdCheatGive,			"Cheat command to give you various kinds of things.",
	"god",			CCmdCheatGod,			"I don't think He needs any help...",
	"kill",			CCmdCheatMassacre,		"Kill all the monsters on the level.",
	"hereticfont",	CCmdHereticFont,		"Use the Heretic font.",
	"invleft",		CCmdInventory,			"Move inventory cursor to the left.",
	"invright",		CCmdInventory,			"Move inventory cursor to the right.",
//	"midi",			CCmdMidi,				"MIDI music control.",
	"pause",		CCmdPause,				"Pause the game (same as pressing the pause key).",
	"chicken",		CCmdCheatPig,			"Turn yourself into a chicken. Go ahead.",
	"reveal",		CCmdCheatReveal,		"Map cheat.",
	"viewsize",		CCmdViewSize,			"Set the view size.",
	"sbsize",		CCmdViewSize,			"Set the status bar size.",
	"screenshot",	CCmdScreenShot,			"Take a screenshot.",
//	"sndchannels",	CCmdSndChannels,		"Set or query the number of sound channels.",
	"warp",			CCmdCheatWarp,			"Warp to a map.",
	"where",		CCmdCheatWhere,			"Prints your map number and exact location.",

	"spy",			CCmdCycleSpy,			"Change the viewplayer when not in deathmatch.",

	// Menu actions.
	"infoscreen",	CCmdMenuAction,			"Display the original Hexen help screens.",
	"savegame",		CCmdMenuAction,			"Save the game.",
	"loadgame",		CCmdMenuAction,			"Load a saved game.",
	"soundmenu",	CCmdMenuAction,			"Open the sound menu.",
	"quicksave",	CCmdMenuAction,			"Quicksave the game.",
	"endgame",		CCmdMenuAction,			"End the current game.",
	"togglemsgs",	CCmdMenuAction,			"Toggle messages on/off (cvar messages).",
	"quickload",	CCmdMenuAction,			"Load the last quicksaved game.",
	"quit",			CCmdMenuAction,			"Quit jHeretic.",
	"togglegamma",	CCmdMenuAction,			"Change the gamma correction level.",

	"beginchat",	CCmdBeginChat,			"Enter chat mode. Destination optional.",

	"spawnmobj",	CCmdSpawnMobj,			"Spawn a new mobj.",
	"coord",		CCmdPrintPlayerCoords, "Print the coordinates of the consoleplayer.",

	// $democam: console commands
	"makelocp",		CCmdMakeLocal,			"Make local player.",
	"makecam",		CCmdSetCamera,			"Toggle camera mode.",
	"setlock",		CCmdSetViewLock,		"Set camera viewlock.",
	"lockmode",		CCmdSetViewLock,		"Set camera viewlock mode.",

	"startinf",		CCmdStartInFine,		"Start an InFine script.",
	"stopinf",		CCmdStopInFine,			"Stop the currently playing interlude/finale.",
	"stopfinale",	CCmdStopInFine,			"Stop the currently playing interlude/finale.",

	// $moveplane: console commands
	"movefloor",	CCmdMovePlane,			"Move a sector's floor plane.",
	"moveceil",		CCmdMovePlane,			"Move a sector's ceiling plane.",
	"movesec",		CCmdMovePlane,			"Move a sector's both planes.",

	NULL
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

// Add the console variables and commands.
void H_ConsoleRegistration()
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

DEFCC(CCmdHereticFont)
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
	S_LocalSound(NULL, atoi(argv[1]));
	return true;
}
#endif
