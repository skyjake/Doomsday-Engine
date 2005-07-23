
//**************************************************************************
//**
//** HCONSOLE.C
//**
//** Heretic specific console stuff.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include "jHeretic/Doomdef.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_config.h"
#include "jHeretic/Mn_def.h"
#include "g_common.h"
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
DEFCC(CCmdSuicide);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatWhere);
DEFCC(CCmdCheatReveal);

DEFCC(CCmdBeginChat);
DEFCC(CCmdMsgRefresh);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int testcvar;

extern boolean mn_SuicideConsole;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     consoleFlat = 6;
float   consoleZoom = 1;

cvar_t  gameCVars[] = {
	"i_MouseSensiX", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 100,
	"Mouse X axis sensitivity.",
	"i_MouseSensiY", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 100,
	"Mouse Y axis sensitivity.",
	"i_jLookInvY", OBSOLETE, CVT_INT, &cfg.jlookInverseY, 0, 1,
	"1=Inverse joystick look Y axis.",
	"i_mLookInvY", OBSOLETE, CVT_INT, &cfg.mlookInverseY, 0, 1,
	"1=Inverse mouse look Y axis.",
	"i_JoyXAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[0], 0, 4,
	"0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
	"i_JoyYAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[1], 0, 4,
	"0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
	"i_JoyZAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[2], 0, 4,
	"0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
	//"i_JoyDeadZone", OBSOLETE, CVT_INT, &cfg.joydead, 10, 90, "Joystick dead zone, in percents.", 
	//"FPS", OBSOLETE | CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1,
	//"1=Show the frames per second counter.",
	"EchoMsg", OBSOLETE, CVT_INT, &cfg.echoMsg, 0, 1,
	"1=Echo all messages to the console.",
	"ImmediateUse", OBSOLETE, CVT_INT, &cfg.chooseAndUse, 0, 1,
	"1=Use items immediately from the inventory.",
	"LookSpeed", OBSOLETE, CVT_INT, &cfg.lookSpeed, 1, 5,
	"The speed of looking up/down.",
	"dClickUse", OBSOLETE, CVT_INT, &cfg.dclickuse, 0, 1,
	"1=Doubleclick forward/strafe equals use key.",
	"bgFlat", OBSOLETE | CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
	"The number of the flat to use for the console background.",
	"bgZoom", OBSOLETE, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
	"Zoom factor for the console background.",
	"PovLook", OBSOLETE, CVT_BYTE, &cfg.povLookAround, 0, 1,
	"1=Look around using the POV hat.",
	"i_mLook", OBSOLETE, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
	"i_jLook", OBSOLETE, CVT_INT, &cfg.usejlook, 0, 1,
	"1=Joystick look active.",
	"AlwaysRun", OBSOLETE, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
	"LookSpring", OBSOLETE, CVT_INT, &cfg.lookSpring, 0, 1,
	"1=Lookspring active.",
	"NoAutoAim", OBSOLETE, CVT_INT, &cfg.noAutoAim, 0, 1,
	"1=Autoaiming disabled.",
	"h_ViewSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
	"View window size (3-13).",
	"h_sbSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20,
	"Status bar size (1-20).",
	"XHair", OBSOLETE | CVF_NO_MAX | CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0,
	"The current crosshair.",
	"XHairR", OBSOLETE, CVT_BYTE, &cfg.xhairColor[0], 0, 255,
	"Red crosshair color component.",
	"XHairG", OBSOLETE, CVT_BYTE, &cfg.xhairColor[1], 0, 255,
	"Green crosshair color component.",
	"XHairB", OBSOLETE, CVT_BYTE, &cfg.xhairColor[2], 0, 255,
	"Blue crosshair color component.",
	"XHairSize", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.xhairSize, 0, 0,
	"Crosshair size: 1=Normal.",
	//  "s_3D", OBSOLETE, CVT_INT, &cfg.snd_3D, 0, 1, "1=Play sounds in 3D.", 
	//  "s_ReverbVol", OBSOLETE, CVT_FLOAT, &cfg.snd_ReverbFactor, 0, 1, "General reverb strength (0-1).", 
	"s_Custom", OBSOLETE, CVT_BYTE, &cfg.customMusic, 0, 1,
	"1=Enable custom (external) music files.\n",
	//  "SoundDebug", OBSOLETE|CVF_NO_ARCHIVE, CVT_INT, &DebugSound, 0, 1, "1=Display sound debug information.", 
	//"ReverbDebug", OBSOLETE|CVF_NO_ARCHIVE, CVT_BYTE, &cfg.reverbDebug, 0, 1, "1=Reverberation debug information in the console.", 
	"Messages", OBSOLETE, CVT_BYTE, &cfg.msgShow, 0, 1, "1=Show messages.",
	"ShowAmmo", OBSOLETE, CVT_BYTE, &cfg.hudShown[0], 0, 1,
	"1=Show ammo when the status bar is hidden.",
	"ShowArmor", OBSOLETE, CVT_BYTE, &cfg.hudShown[1], 0, 1,
	"1=Show armor when the status bar is hidden.",
	"ShowKeys", OBSOLETE, CVT_BYTE, &cfg.hudShown[2], 0, 1,
	"1=Show keys when the status bar is hidden.",
	"ChatMacro0", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[0], 0, 0,
	"Chat macro 1.",
	"ChatMacro1", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[1], 0, 0,
	"Chat macro 2.",
	"ChatMacro2", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[2], 0, 0,
	"Chat macro 3.",
	"ChatMacro3", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[3], 0, 0,
	"Chat macro 4.",
	"ChatMacro4", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[4], 0, 0,
	"Chat macro 5.",
	"ChatMacro5", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[5], 0, 0,
	"Chat macro 6.",
	"ChatMacro6", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[6], 0, 0,
	"Chat macro 7.",
	"ChatMacro7", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[7], 0, 0,
	"Chat macro 8.",
	"ChatMacro8", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[8], 0, 0,
	"Chat macro 9.",
	"ChatMacro9", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[9], 0, 0,
	"Chat macro 10.",
	"NoMonsters", OBSOLETE, CVT_BYTE, &cfg.netNomonsters, 0, 1,
	"1=No monsters.",
	"Respawn", OBSOLETE, CVT_BYTE, &cfg.netRespawn, 0, 1,
	"1= -respawn was used.",
	"n_Skill", OBSOLETE, CVT_BYTE, &cfg.netSkill, 0, 4,
	"Skill level in multiplayer games.",
	"n_Map", OBSOLETE, CVT_BYTE, &cfg.netMap, 1, 31,
	"Map to use in multiplayer games.",
	"n_Episode", OBSOLETE, CVT_BYTE, &cfg.netEpisode, 1, 6,
	"Episode to use in multiplayer games.",
	"n_Jump", OBSOLETE, CVT_BYTE, &cfg.netJumping, 0, 1,
	"1=Allow jumping in multiplayer games.",
	"Deathmatch", OBSOLETE, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
	"1=Start multiplayers games as deathmatch.",
	"n_Color", OBSOLETE, CVT_BYTE, &cfg.netColor, 0, 4,
	"Player color: 0=green, 1=yellow, 2=red, 3=blue, 4=default.",
	"RingFilter", OBSOLETE, CVT_INT, &cfg.ringFilter, 1, 2,
	"Ring effect filter. 1=Brownish, 2=Blue.",
	"AllowJump", OBSOLETE, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.",
	"TomeTimer", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0,
	"Countdown seconds for the Tome of Power.",
	"TomeSound", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0,
	"Seconds for countdown sound of Tome of Power.",
	"FastMonsters", OBSOLETE, CVT_BYTE, &cfg.fastMonsters, 0, 1,
	"1=Fast monsters in non-demo single player.",
	"EyeHeight", OBSOLETE, CVT_INT, &cfg.eyeHeight, 41, 54,
	"Player eye height (the original is 41).",

	"LevelTitle", OBSOLETE, CVT_BYTE, &cfg.levelTitle, 0, 1,
	"1=Show level title and author in the beginning.",
	"CounterCheat", OBSOLETE, CVT_BYTE, &cfg.counterCheat, 0, 63,
	"6-bit bitfield. Show kills, items and secret counters in automap.",

	//===========================================================================
	// New names (1.2.0 =>)
	//===========================================================================

	"input-mouse-x-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 100,
	"Mouse X axis sensitivity.",
	"input-mouse-y-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 100,
	"Mouse Y axis sensitivity.",
	"ctl-look-joy-inverse", 0, CVT_INT, &cfg.jlookInverseY, 0, 1,
	"1=Inverse joystick look Y axis.",
	"ctl-look-mouse-inverse", 0, CVT_INT, &cfg.mlookInverseY, 0, 1,
	"1=Inverse mouse look Y axis.",
	"input-joy-x", 0, CVT_INT, &cfg.joyaxis[0], 0, 4,
	"X axis control: 0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
	"input-joy-y", 0, CVT_INT, &cfg.joyaxis[1], 0, 4, "Y axis control.",
	"input-joy-z", 0, CVT_INT, &cfg.joyaxis[2], 0, 4, "Z axis control.",
	"input-joy-rx", 0, CVT_INT, &cfg.joyaxis[3], 0, 4,
	"X rotational axis control.",
	"input-joy-ry", 0, CVT_INT, &cfg.joyaxis[4], 0, 4,
	"Y rotational axis control.",
	"input-joy-rz", 0, CVT_INT, &cfg.joyaxis[5], 0, 4,
	"Z rotational axis control.",
	"input-joy-slider1", 0, CVT_INT, &cfg.joyaxis[6], 0, 4,
	"First slider control.",
	"input-joy-slider2", 0, CVT_INT, &cfg.joyaxis[7], 0, 4,
	"Second slider control.",
	//  "input-joy-deadzone",   0,          CVT_INT,    &cfg.joydead,       10, 90, "Joystick dead zone, in percents.", 
	"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1,
	"1=Camera players have no movement clipping.",
	"ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1,
	"1=Joystick values => look angle delta.",
	"hud-fps", CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1,
	"1=Show the frames per second counter.",

	"ctl-use-immediate", 0, CVT_INT, &cfg.chooseAndUse, 0, 1,
	"1=Use items immediately from the inventory.",
	"ctl-look-speed", 0, CVT_INT, &cfg.lookSpeed, 1, 5,
	"The speed of looking up/down.",
	"ctl-use-dclick", 0, CVT_INT, &cfg.dclickuse, 0, 1,
	"1=Doubleclick forward/strafe equals use key.",
	"con-flat", CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
	"The number of the flat to use for the console background.",
	"con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
	"Zoom factor for the console background.",
	"ctl-look-pov", 0, CVT_BYTE, &cfg.povLookAround, 0, 1,
	"1=Look around using the POV hat.",

	"ctl-look-mouse", 0, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
	"ctl-look-joy", 0, CVT_INT, &cfg.usejlook, 0, 1, "1=Joystick look active.",
	"ctl-run", 0, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
	"ctl-look-spring", 0, CVT_INT, &cfg.lookSpring, 0, 1,
	"1=Lookspring active.",
	"ctl-aim-noauto", 0, CVT_INT, &cfg.noAutoAim, 0, 1,
	"1=Autoaiming disabled.",
	"view-size", CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
	"View window size (3-13).",
	"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20,
	"Status bar size (1-20).",
	"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarAlpha, 0, 1,
	"Status bar Alpha level.",
	"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1,
	"Status bar icons & counters Alpha level.",

	"view-cross-type", CVF_NO_MAX | CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0,
	"The current crosshair.",
	"view-cross-r", 0, CVT_BYTE, &cfg.xhairColor[0], 0, 255,
	"Crosshair color red component.",
	"view-cross-g", 0, CVT_BYTE, &cfg.xhairColor[1], 0, 255,
	"Crosshair color green component.",
	"view-cross-b", 0, CVT_BYTE, &cfg.xhairColor[2], 0, 255,
	"Crosshair color blue component.",
	"view-cross-a", 0, CVT_BYTE, &cfg.xhairColor[3], 0, 255,
	"Crosshair color alpha component.",
	"view-cross-size", CVF_NO_MAX, CVT_INT, &cfg.xhairSize, 0, 0,
	"Crosshair size: 1=Normal.",

	"view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1,
	"Scaling factor for viewheight bobbing.",
	"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1,
	"Scaling factor for player weapon bobbing.",

	/*  "sound-3d",         0,          CVT_INT,    &cfg.snd_3D,        0, 1,   "1=Play sounds in 3D.",
	   "sound-reverb-volume", 0,        CVT_FLOAT,  &cfg.snd_ReverbFactor, 0, 1, "General reverb strength (0-1).",
	 */ "music-custom", 0, CVT_BYTE, &cfg.customMusic, 0, 1,
	"1=Enable custom (external) music files.\n",
	//  "sound-debug",  CVF_NO_ARCHIVE, CVT_INT,    &DebugSound,        0, 1,   "1=Display sound debug information.",
	//  "sound-reverb-debug", CVF_NO_ARCHIVE,   CVT_BYTE,   &cfg.reverbDebug,   0, 1,   "1=Reverberation debug information in the console.",

	"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1,
	"1=Show ammo when the status bar is hidden.",
	"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1,
	"1=Show armor when the status bar is hidden.",
	"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1,
	"1=Show keys when the status bar is hidden.",
	"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1,
	"1=Show health when the status bar is hidden.",
	"hud-artifact", 0, CVT_BYTE, &cfg.hudShown[HUD_ARTI], 0, 1,
	"1=Show artifact when the status bar is hidden.",

    	"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, .1f, 1,
    	"Scaling for HUD elements (status bar hidden).",
	"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, "HUD info color red component.",
	"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, "HUD info color green component.",
	"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, "HUD info color alpha component.",
	"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, "HUD info alpha value.",
	"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, "HUD icon alpha value.",

	"chat-macro0", 0, CVT_CHARPTR, &cfg.chat_macros[0], 0, 0, "Chat macro 1.",
	"chat-macro1", 0, CVT_CHARPTR, &cfg.chat_macros[1], 0, 0, "Chat macro 2.",
	"chat-macro2", 0, CVT_CHARPTR, &cfg.chat_macros[2], 0, 0, "Chat macro 3.",
	"chat-macro3", 0, CVT_CHARPTR, &cfg.chat_macros[3], 0, 0, "Chat macro 4.",
	"chat-macro4", 0, CVT_CHARPTR, &cfg.chat_macros[4], 0, 0, "Chat macro 5.",
	"chat-macro5", 0, CVT_CHARPTR, &cfg.chat_macros[5], 0, 0, "Chat macro 6.",
	"chat-macro6", 0, CVT_CHARPTR, &cfg.chat_macros[6], 0, 0, "Chat macro 7.",
	"chat-macro7", 0, CVT_CHARPTR, &cfg.chat_macros[7], 0, 0, "Chat macro 8.",
	"chat-macro8", 0, CVT_CHARPTR, &cfg.chat_macros[8], 0, 0, "Chat macro 9.",
	"chat-macro9", 0, CVT_CHARPTR, &cfg.chat_macros[9], 0, 0, "Chat macro 10.",

	// Game settings for servers.
	"server-game-nomonsters", 0, CVT_BYTE, &cfg.netNomonsters, 0, 1,
	"1=No monsters.",
	"server-game-respawn", 0, CVT_BYTE, &cfg.netRespawn, 0, 1,
	"1= -respawn was used.",
	"server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4,
	"Skill level in multiplayer games.",
	"server-game-map", 0, CVT_BYTE, &cfg.netMap, 1, 31,
	"Map to use in multiplayer games.",
	"server-game-episode", 0, CVT_BYTE, &cfg.netEpisode, 1, 6,
	"Episode to use in multiplayer games.",
	"server-game-jump", 0, CVT_BYTE, &cfg.netJumping, 0, 1,
	"1=Allow jumping in multiplayer games.",
	"server-game-deathmatch", 0, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
	"1=Start multiplayers games as deathmatch.",

	// Player data.
	"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 4,
	"Player color: 0=green, 1=yellow, 2=red, 3=blue, 4=default.",

	"view-ringfilter", 0, CVT_INT, &cfg.ringFilter, 1, 2,
	"Ring effect filter. 1=Brownish, 2=Blue.",
	"player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.",
	"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100, "Jump power.",
	"hud-tome-timer", CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0,
	"Countdown seconds for the Tome of Power.",
	"hud-tome-sound", CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0,
	"Seconds for countdown sound of Tome of Power.",
	"game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1,
	"1=Fast monsters in non-demo single player.",
	"player-eyeheight", 0, CVT_INT, &cfg.eyeHeight, 41, 54,
	"Player eye height (the original is 41).",
	"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1,
	"Player movement speed modifier.",

	"hud-title", 0, CVT_BYTE, &cfg.levelTitle, 0, 1,
	"1=Show level title and author in the beginning.",
	"game-corpsetime", CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0,
	"Number of seconds after which dead monsters disappear.",

	"msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2,
	"Alignment of HUD messages. 0 = left, 1 = center, 2 = right.",
	"msg-echo", 0, CVT_INT, &cfg.echoMsg, 0, 1,
	"1=Echo all messages to the console.",
	"msg-show", 0, CVT_BYTE, &cfg.msgShow, 0, 1, "1=Show messages.",
	"msg-count", 0, CVT_INT, &cfg.msgCount, 0, 8,
	"Number of HUD messages displayed at the same time.",
	"msg-scale", CVF_NO_MAX, CVT_FLOAT, &cfg.msgScale, 0, 0,
	"Scaling factor for HUD messages.",
	"msg-uptime", CVF_NO_MAX, CVT_INT, &cfg.msgUptime, 35, 0,
	"Number of tics to keep HUD messages on screen.",
	"msg-blink", 0, CVT_BYTE, &cfg.msgBlink, 0, 1,
	"1=HUD messages blink when they're printed.",
	"msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[0], 0, 1, "Color of HUD messages red component.",
	"msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[1], 0, 1, "Color of HUD messages green component.",
	"msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[2], 0, 1, "Color of HUD messages blue component.",

	"xg-dev", 0, CVT_INT, &xgDev, 0, 1, "1=Print XG debug messages.",

	NULL
};

ccmd_t  gameCCmds[] = {
	"cheat", CCmdCheat, "Issue a cheat code using the original Hexen cheats.",
	"noclip", CCmdCheatClip, "Movement clipping on/off.",
	"crosshair", CCmdCrosshair, "Crosshair settings.",
	/*#ifdef DEMOCAM
	   "demomode",      CCmdSetDemoMode,        "Set demo external camera mode.",
	   #endif */
	"give", CCmdCheatGive,
	"Cheat command to give you various kinds of things.",
	"god", CCmdCheatGod, "I don't think He needs any help...",
	"suicide", 	CCmdSuicide, "Kill yourself. What did you think?",
	"kill", CCmdCheatMassacre, "Kill all the monsters on the level.",
	"hereticfont", CCmdHereticFont, "Use the Heretic font.",
	"invleft", CCmdInventory, "Move inventory cursor to the left.",
	"invright", CCmdInventory, "Move inventory cursor to the right.",
	//  "midi",         CCmdMidi,               "MIDI music control.",
	"pause", CCmdPause, "Pause the game (same as pressing the pause key).",
	"chicken", CCmdCheatPig, "Turn yourself into a chicken. Go ahead.",
	"reveal", CCmdCheatReveal, "Map cheat.",
	"viewsize", CCmdViewSize, "Set the view size.",
	"sbsize", CCmdViewSize, "Set the status bar size.",
	"screenshot", CCmdScreenShot, "Take a screenshot.",
	//  "sndchannels",  CCmdSndChannels,        "Set or query the number of sound channels.",
	"warp", CCmdCheatWarp, "Warp to a map.",
	"where", CCmdCheatWhere, "Prints your map number and exact location.",

	"spy", CCmdCycleSpy, "Change the viewplayer when not in deathmatch.",

	"beginchat", CCmdBeginChat, "Begin chat mode.",
	"msgrefresh", CCmdMsgRefresh, "Show last HUD message.",

	"spawnmobj", CCmdSpawnMobj, "Spawn a new mobj.",
	"coord", CCmdPrintPlayerCoords,
	"Print the coordinates of the consoleplayer.",
	"message", CCmdLocalMessage, "Show a local game message.",
	
	// $democam: console commands
	"makelocp", CCmdMakeLocal, "Make local player.",
	"makecam", CCmdSetCamera, "Toggle camera mode.",
	"setlock", CCmdSetViewLock, "Set camera viewlock.",
	"lockmode", CCmdSetViewLock, "Set camera viewlock mode.",

	"startinf", CCmdStartInFine, "Start an InFine script.",
	"stopinf", CCmdStopInFine, "Stop the currently playing interlude/finale.",
	"stopfinale", CCmdStopInFine,
	"Stop the currently playing interlude/finale.",

	// $moveplane: console commands
	"movefloor", CCmdMovePlane, "Move a sector's floor plane.",
	"moveceil", CCmdMovePlane, "Move a sector's ceiling plane.",
	"movesec", CCmdMovePlane, "Move a sector's both planes.",

	NULL
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

// Add the console variables and commands.
void H_ConsoleRegistration()
{
	int     i;

	for(i = 0; gameCVars[i].name; i++)
		Con_AddVariable(gameCVars + i);
	for(i = 0; gameCCmds[i].name; i++)
		Con_AddCommand(gameCCmds + i);
	D_NetConsoleRegistration();
}


int CCmdPause(int argc, char **argv)
{
	extern boolean sendpause;

	if(!menuactive)
		sendpause = true;
	return true;
}

void SuicideResponse(int option, void *data)
{
	if(option != 'y')
		return;

	GL_Update(DDUF_BORDER);
	mn_SuicideConsole = true;	
}

int CCmdSuicide(int argc, char **argv)
{
	if(gamestate != GS_LEVEL)
	{
		S_LocalSound(sfx_chat, NULL);
		Con_Printf("Can only suicide when in a game!\n", argv[0]);
		return true;

	}

	if(deathmatch)
	{
		S_LocalSound(sfx_chat, NULL);
		Con_Printf("Can't suicide during a deathmatch!\n", argv[0]);
		return true;		
	}

	if (!stricmp(argv[0], "suicide"))
	{
		Con_Open(false);
		menuactive = false;
		M_StartMessage("Are you sure you want to suicide?\n\nPress Y or N.", SuicideResponse, true);
		return true;
	}
	return false;
}

int CCmdViewSize(int argc, char **argv)
{
	int     min = 3, max = 13, *val = &cfg.screenblocks;

	if(argc != 2)
	{
		Con_Printf("Usage: %s (size)\n", argv[0]);
		Con_Printf("Size can be: +, -, (num).\n");
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

	if(*val < min)
		*val = min;
	if(*val > max)
		*val = max;

	// Update the view size if necessary.
	R_SetViewSize(cfg.screenblocks, 0);
	return true;
}

int CCmdScreenShot(int argc, char **argv)
{
	G_ScreenShot();
	return true;
}

int ConTextOut(char *text, int x, int y)
{
	extern int typein_time;
	int     old = typein_time;

	typein_time = 0xffffff;
	M_WriteText2(x, y, text, hu_font_a, -1, -1, -1, -1);
	typein_time = old;
	return 0;
}


int ConTextWidth(char *text)
{
	return M_StringWidth(text, hu_font_a);
}

void ConTextFilter(char *text)
{
	strupr(text);
}

DEFCC(CCmdHereticFont)
{
	ddfont_t cfont;

	cfont.flags = DDFONT_WHITE;
	cfont.height = 9;
	cfont.sizeX = 1.2f;
	cfont.sizeY = 2;
	cfont.TextOut = ConTextOut;
	cfont.Width = ConTextWidth;
	cfont.Filter = ConTextFilter;
	Con_SetFont(&cfont);
	return true;
}

#if 0
DEFCC(CCmdTest)
{
	/*int       i;

	   for(i=0; i<MAXPLAYERS; i++)
	   if(players[i].plr->ingame)
	   {
	   Con_Printf( "%i: cls:%i col:%i look:%f\n", i, PlayerClass[i],
	   PlayerColor[i], players[i].plr->lookdir);
	   } */
	if(argc != 2)
		return false;
	S_LocalSound(NULL, atoi(argv[1]));
	return true;
}
#endif
