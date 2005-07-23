
//**************************************************************************
//**
//** HCONSOLE.C
//**
//** Hexen specific console stuff.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "x_config.h"
#include "d_net.h"
#include "jHexen/mn_def.h"
#include "../Common/hu_stuff.h"
#include "f_infine.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

#define DEFCC(x)	int x(int argc, char **argv)
#define OBSOLETE	CVF_HIDE|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    S_InitScript();
void    SN_InitSequenceScript(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdPause);
int     CCmdCheat(int argc, char **argv);
int     CCmdScriptInfo(int argc, char **argv);
int     CCmdSuicide(int argc, char **argv);
int     CCmdSetDemoMode(int argc, char **argv);
int     CCmdCrosshair(int argc, char **argv);
int     CCmdViewSize(int argc, char **argv);
int     CCmdInventory(int argc, char **argv);
int     CCmdScreenShot(int argc, char **argv);

DEFCC(CCmdHexenFont);

DEFCC(CCmdCycleSpy);
DEFCC(CCmdTest);
DEFCC(CCmdSpawnMobj);
DEFCC(CCmdPrintPlayerCoords);
DEFCC(CCmdMovePlane);

DEFCC(CCmdBeginChat);
DEFCC(CCmdMsgRefresh);

// The cheats.
int     CCmdCheatGod(int argc, char **argv);
int     CCmdCheatClip(int argc, char **argv);
int     CCmdCheatGive(int argc, char **argv);
int     CCmdCheatWarp(int argc, char **argv);
int     CCmdCheatPig(int argc, char **argv);
int     CCmdCheatMassacre(int argc, char **argv);
int     CCmdCheatShadowcaster(int argc, char **argv);
int     CCmdCheatWhere(int argc, char **argv);
int     CCmdCheatRunScript(int argc, char **argv);
int     CCmdCheatReveal(int argc, char **argv);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern ccmd_t netCCmds[];

extern boolean mn_SuicideConsole;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     consoleFlat = 60;
float   consoleZoom = 1;

cvar_t  gameCVars[] = {
	"i_MouseSensiX", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
	"Mouse X axis sensitivity.",
	"i_MouseSensiY", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
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
	//  "i_JoyDeadZone", OBSOLETE, CVT_INT, &cfg.joydead, 10, 90, "Joystick dead zone, in percents.", 
	//"FPS", OBSOLETE | CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1,
	//"1=Show the frames per second counter.",
	"EchoMsg", OBSOLETE, CVT_INT, &cfg.echoMsg, 0, 1,
	"1=Echo all messages to the console.",
	"IceCorpse", OBSOLETE, CVT_INT, &cfg.translucentIceCorpse, 0, 1,
	"1=Translucent frozen monsters.",
	"ImmediateUse", OBSOLETE, CVT_INT, &cfg.chooseAndUse, 0, 1,
	"1=Use items immediately from the inventory.",
	"LookSpeed", OBSOLETE, CVT_INT, &cfg.lookSpeed, 1, 5,
	"The speed of looking up/down.",
	"bgFlat", OBSOLETE | CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
	"The number of the flat to use for the console background.",
	"bgZoom", OBSOLETE, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
	"Zoom factor for the console background.",
	"povLook", OBSOLETE, CVT_BYTE, &cfg.povLookAround, 0, 1,
	"1=Look around using the POV hat.",
	"i_mLook", OBSOLETE, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
	"i_jLook", OBSOLETE, CVT_INT, &cfg.usejlook, 0, 1,
	"1=Joystick look active.",
	"AlwaysRun", OBSOLETE, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
	"LookSpring", OBSOLETE, CVT_INT, &cfg.lookSpring, 0, 1,
	"1=Lookspring active.",
	"NoAutoAim", OBSOLETE, CVT_INT, &cfg.noAutoAim, 0, 1,
	"1=Autoaiming disabled.",
	"h_ViewSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 11,
	"View window size (3-11).",
	"h_sbSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20,
	"Status bar size (1-20).",
	"dClickUse", OBSOLETE, CVT_INT, &cfg.dclickuse, 0, 1,
	"1=Double click forward/strafe equals pressing the use key.",
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
	"s_3D", OBSOLETE, CVT_INT, &cfg.snd_3D, 0, 1, "1=Play sounds in 3D.",
	"s_ReverbVol", OBSOLETE, CVT_FLOAT, &cfg.snd_ReverbFactor, 0, 1,
	"General reverb strength (0-1).",
	"SoundDebug", OBSOLETE | CVF_NO_ARCHIVE, CVT_INT, &DebugSound, 0, 1,
	"1=Display sound debug information.",
	"ReverbDebug", OBSOLETE | CVF_NO_ARCHIVE, CVT_BYTE, &cfg.reverbDebug, 0, 1,
	"1=Reverberation debug information in the console.",
	"ShowMana", OBSOLETE, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2,
	"Show mana when the status bar is hidden.",
	//"SaveDir", OBSOLETE|CVF_PROTECTED, CVT_CHARPTR, &SavePath, 0, 0, "The directory for saved games.", 
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
	"RandClass", OBSOLETE, CVT_BYTE, &cfg.netRandomclass, 0, 1,
	"1=Respawn in a random class (deathmatch).",
	"n_Skill", OBSOLETE, CVT_BYTE, &cfg.netSkill, 0, 4,
	"Skill level in multiplayer games.",
	"n_Map", OBSOLETE, CVT_BYTE, &cfg.netMap, 1, 99,
	"Map to use in multiplayer games.",
	"Deathmatch", OBSOLETE, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
	"1=Start multiplayers games as deathmatch.",
	"n_Class", OBSOLETE, CVT_BYTE, &cfg.netClass, 0, 2,
	"Player class in multiplayer games.",
	"n_Color", OBSOLETE, CVT_BYTE, &cfg.netColor, 0, 8,
	"Player color: 0=blue, 1=red, 2=yellow, 3=green, 4=jade, 5=white, \n6=hazel, 7=purple, 8=auto.",
	"n_mobDamage", OBSOLETE, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100,
	"Enemy (mob) damage modifier, multiplayer (1..100).",
	"n_mobHealth", OBSOLETE, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20,
	"Enemy (mob) health modifier, multiplayer (1..20).",
	"OverrideHubMsg", OBSOLETE, CVT_BYTE, &cfg.overrideHubMsg, 0, 2,
	"Override the transition hub message.",
	"DemoDisabled", OBSOLETE, CVT_BYTE, &cfg.demoDisabled, 0, 2,
	"Disable demos.",
	"MaulatorTime", OBSOLETE | CVF_NO_MAX, CVT_INT, &MaulatorSeconds, 1, 0,
	"Dark Servant lifetime, in seconds (default: 25).",
	"FastMonsters", OBSOLETE, CVT_BYTE, &cfg.fastMonsters, 0, 1,
	"1=Fast monsters in non-demo single player.",
	"MapTitle", OBSOLETE, CVT_BYTE, &cfg.mapTitle, 0, 1,
	"1=Show map title after entering map.",
	"CounterCheat", OBSOLETE, CVT_BYTE, &cfg.counterCheat, 0, 63,
	"6-bit bitfield. Show kills, items and secret counters in automap.",

	//===========================================================================
	// New names (1.1.0 =>)
	//===========================================================================
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
	"con-flat", CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
	"The number of the flat to use for the console background.",
	"con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
	"Zoom factor for the console background.",
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
	"view-cross-type", CVF_NO_MAX | CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0,
	"The current crosshair.",
	"view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1,
	"Scaling factor for viewheight bobbing.",
	"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1,
	"Scaling factor for player weapon bobbing.",
	"ctl-aim-noauto", 0, CVT_INT, &cfg.noAutoAim, 0, 1,
	"1=Autoaiming disabled.",
	"ctl-look-joy", 0, CVT_INT, &cfg.usejlook, 0, 1, "1=Joystick look active.",
	"ctl-look-joy-inverse", 0, CVT_INT, &cfg.jlookInverseY, 0, 1,
	"1=Inverse joystick look Y axis.",
	"ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1,
	"1=Joystick values => look angle delta.",
	"ctl-look-mouse", 0, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
	"ctl-look-mouse-inverse", 0, CVT_INT, &cfg.mlookInverseY, 0, 1,
	"1=Inverse mouse look Y axis.",
	"ctl-look-pov", 0, CVT_BYTE, &cfg.povLookAround, 0, 1,
	"1=Look around using the POV hat.",
	"ctl-look-speed", 0, CVT_INT, &cfg.lookSpeed, 1, 5,
	"The speed of looking up/down.",
	"ctl-look-spring", 0, CVT_INT, &cfg.lookSpring, 0, 1,
	"1=Lookspring active.",
	"ctl-run", 0, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
	"ctl-use-dclick", 0, CVT_INT, &cfg.dclickuse, 0, 1,
	"1=Double click forward/strafe equals pressing the use key.",
	"ctl-use-immediate", 0, CVT_INT, &cfg.chooseAndUse, 0, 1,
	"1=Use items immediately from the inventory.",
	"game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1,
	"1=Fast monsters in non-demo single player.",
	"game-icecorpse", 0, CVT_INT, &cfg.translucentIceCorpse, 0, 1,
	"1=Translucent frozen monsters.",
	"game-maulator-time", CVF_NO_MAX, CVT_INT, &MaulatorSeconds, 1, 0,
	"Dark Servant lifetime, in seconds (default: 25).",
	"hud-fps", CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1,
	"1=Show the frames per second counter.",
	"hud-mana", 0, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2,
	"Show mana when the status bar is hidden. 1= top, 2= bottom, 0= off",
	"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1,
	"Show health when the status bar is hidden.",
	"hud-artifact", 0, CVT_BYTE, &cfg.hudShown[HUD_ARTI], 0, 1,
	"Show artifact when the status bar is hidden.",
	"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20,
	"Status bar size (1-20).",
	"hud-title", 0, CVT_BYTE, &cfg.mapTitle, 0, 1,
	"1=Show map title after entering map.",
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
	"input-mouse-x-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
	"Mouse X axis sensitivity.",
	"input-mouse-y-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
	"Mouse Y axis sensitivity.",

    	"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, .1f, 1,
    	"Scaling for HUD elements (status bar hidden).",
	"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, "HUD info color.",
	"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, "HUD info color.",
	"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, "HUD info color.",
	"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, "HUD info alpha value.",
	"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, "HUD icon alpha value.",

	"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarAlpha, 0, 1,
	"Status bar Alpha level.",
	"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1,
	"Status bar icons & counters Alpha level.",

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
	"msg-hub-override", 0, CVT_BYTE, &cfg.overrideHubMsg, 0, 2,
	"Override the transition hub message.",
	"msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[0], 0, 1, "Normal color of HUD messages red component.",
	"msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[1], 0, 1, "Normal color of HUD messages green component.",
	"msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[2], 0, 1, "Normal color of HUD messages blue component.",

	"player-class", 0, CVT_BYTE, &cfg.netClass, 0, 2,
	"Player class in multiplayer games.",
	"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 8,
	"Player color: 0=blue, 1=red, 2=yellow, 3=green, 4=jade, 5=white,\n6=hazel, 7=purple, 8=auto.",
	"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1,
	"1=Camera players have no movement clipping.",
	"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100, "Jump power.",
	"server-game-deathmatch", 0, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
	"1=Start multiplayers games as deathmatch.",
	"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1,
	"Player movement speed modifier.",
	"server-game-map", 0, CVT_BYTE, &cfg.netMap, 1, 99,
	"Map to use in multiplayer games.",
	"server-game-mod-damage", 0, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100,
	"Enemy (mob) damage modifier, multiplayer (1..100).",
	"server-game-mod-health", 0, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20,
	"Enemy (mob) health modifier, multiplayer (1..20).",
	"server-game-nomonsters", 0, CVT_BYTE, &cfg.netNomonsters, 0, 1,
	"1=No monsters.",
	"server-game-randclass", 0, CVT_BYTE, &cfg.netRandomclass, 0, 1,
	"1=Respawn in a random class (deathmatch).",
	"server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4,
	"Skill level in multiplayer games.",
	"view-size", CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
	"View window size (3-13).",
	NULL
};

ccmd_t  gameCCmds[] = {
	"cheat", CCmdCheat, "Issue a cheat code using the original Hexen cheats.",
	"class", CCmdCheatShadowcaster, "Change player class.",
	"noclip", CCmdCheatClip, "Movement clipping on/off.",
	"crosshair", CCmdCrosshair, "Crosshair settings.",
#ifdef DEMOCAM
	"demomode", CCmdSetDemoMode, "Set demo external camera mode.",
#endif
	"give", CCmdCheatGive,
	"Cheat command to give you various kinds of things.",
	"god", CCmdCheatGod, "I don't think He needs any help...",
	"suicide", 	CCmdSuicide, "Kill yourself. What did you think?",
	"kill", CCmdCheatMassacre, "Kill all the monsters on the level.",
	"hexenfont", CCmdHexenFont, "Use the Hexen font.",
	"invleft", CCmdInventory, "Move inventory cursor to the left.",
	"invright", CCmdInventory, "Move inventory cursor to the right.",
	//  "midi",         CCmdMidi,               "MIDI music control.",
	"pause", CCmdPause, "Pause the game (same as pressing the pause key).",
	"pig", CCmdCheatPig, "Turn yourself into a pig. Go ahead.",
	"reveal", CCmdCheatReveal, "Map cheat.",
	"runscript", CCmdCheatRunScript, "Run a script.",
	"scriptinfo", CCmdScriptInfo,
	"Show information about all scripts or one particular script.",
	"viewsize", CCmdViewSize, "Set the view size.",
	"sbsize", CCmdViewSize, "Set the status bar size.",
	"screenshot", CCmdScreenShot, "Take a screenshot.",
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
	/*  "movefloor",    CCmdMovePlane,          "Move a sector's floor plane.",
	   "moveceil",      CCmdMovePlane,          "Move a sector's ceiling plane.",
	   "movesec",       CCmdMovePlane,          "Move a sector's both planes.", */

	//  "test",         CCmdTest,               "Test.",
	NULL
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

// Add the console variables and commands.
void H2_ConsoleRegistration()
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
		S_LocalSound(SFX_CHAT, NULL);
		Con_Printf("Can only suicide when in a game!\n", argv[0]);
		return true;

	}

	if(deathmatch)
	{
		S_LocalSound(SFX_CHAT, NULL);
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

DEFCC(CCmdHexenFont)
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
	S_StartSoundAtVolume(NULL, SFX_CHAT, atoi(argv[1]));
	return true;
}
#endif
