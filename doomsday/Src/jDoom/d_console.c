// DOOM console stuff.

#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "d_config.h"
#include "d_netJD.h"
#include "g_game.h"
#include "g_common.h"
#include "s_sound.h"
#include "hu_stuff.h"
#include "m_menu.h"
#include "Mn_def.h"
#include "f_infine.h"

// Macros -----------------------------------------------------------------

#define DEFCC(name)		int name(int argc, char **argv)
#define OBSOLETE		CVF_HIDE|CVF_NO_ARCHIVE

// External Functions -----------------------------------------------------

DEFCC(CCmdCycleSpy);
DEFCC(CCmdCrosshair);
DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatNoClip);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdSuicide);
DEFCC(CCmdBeginChat);
DEFCC(CCmdMsgRefresh);
DEFCC(CCmdMovePlane);
DEFCC(CCmdMakeLocal);
DEFCC(CCmdSetCamera);
DEFCC(CCmdSetViewLock);
DEFCC(CCmdSpawnMobj);
DEFCC(CCmdPrintPlayerCoords);

// Public Functions -------------------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdPlayDemo);
DEFCC(CCmdRecordDemo);
DEFCC(CCmdStopDemo);
DEFCC(CCmdPause);
DEFCC(CCmdDoomFont);

// External Data ----------------------------------------------------------

extern boolean hu_showallfrags;

extern boolean mn_SuicideConsole;

// Public Data ------------------------------------------------------------

int     consoleFlat = 10;
float   consoleZoom = 1;

// Console variables.
cvar_t  gameCVars[] = {
	{"i_MouseSensiX", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
	"Mouse X axis sensitivity."},
	{"i_MouseSensiY", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
	"Mouse Y axis sensitivity."},
	{"i_jLookInvY", OBSOLETE, CVT_INT, &cfg.jlookInverseY, 0, 1,
	"1=Inverse joystick look Y axis."},
	{"i_mLookInvY", OBSOLETE, CVT_INT, &cfg.mlookInverseY, 0, 1,
	"1=Inverse mouse look Y axis."},
	{"i_JoyXAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[0], 0, 3,
	"0=None, 1=Move, 2=Turn, 3=Strafe."},
	{"i_JoyYAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[1], 0, 3,
	"0=None, 1=Move, 2=Turn, 3=Strafe."},
	{"i_JoyZAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[2], 0, 3,
	"0=None, 1=Move, 2=Turn, 3=Strafe."},
	//  "i_JoyDeadZone", OBSOLETE, CVT_INT, &cfg.joydead, 10, 90, "Joystick dead zone, in percents.", 
	//"FPS", OBSOLETE | CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1,
	//"1=Show the frames per second counter.",
	{"EchoMsg", OBSOLETE, CVT_BYTE, &cfg.echoMsg, 0, 1,
	"1=Echo all messages to the console."},
	{"LookSpeed", OBSOLETE, CVT_INT, &cfg.lookSpeed, 1, 5,
	"The speed of looking up/down."},
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
	"Lookspring", OBSOLETE, CVT_INT, &cfg.lookSpring, 0, 1,
	"1=Lookspring active.",
	"NoAutoAim", OBSOLETE, CVT_INT, &cfg.noAutoAim, 0, 1,
	"1=Autoaiming disabled.",
	"d_ViewSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &screenblocks, 3, 13,
	"View window size (3-13).",
	"d_sbSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20,
	"Status bar size (1-20).",
	"MapAlpha", OBSOLETE, CVT_FLOAT, &cfg.automapBack[0], 0, 1,
	"Alpha level of the automap background.",
	"TurningSkull", OBSOLETE, CVT_BYTE, &cfg.turningSkull, 0, 1,
	"1=Menu skull turns at slider items.",
	"hud_Health", OBSOLETE, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1,
	"1=Show health in HUD.",
	"hud_Armor", OBSOLETE, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1,
	"1=Show armor in HUD.",
	"hud_Ammo", OBSOLETE, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1,
	"1=Show ammo in HUD.",
	"hud_Keys", OBSOLETE, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1,
	"1=Show keys in HUD.",
	"hud_Frags", OBSOLETE, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1,
	"1=Show deathmatch frags in HUD.",
	"hud_Scale", OBSOLETE, CVT_FLOAT, &cfg.hudScale, 0.1f, 10,
	"Scaling for HUD info.",
	"hud_R", OBSOLETE, CVT_FLOAT, &cfg.hudColor[0], 0, 1, "HUD info color.",
	"hud_G", OBSOLETE, CVT_FLOAT, &cfg.hudColor[1], 0, 1, "HUD info color.",
	"hud_B", OBSOLETE, CVT_FLOAT, &cfg.hudColor[2], 0, 1, "HUD info color.",
	"hud_ShowAllFrags", OBSOLETE, CVT_BYTE, &hu_showallfrags, 0, 1,
	"Debug: HUD shows all frags of all players.",
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
	"s_3D", OBSOLETE, CVT_BYTE, &cfg.snd_3D, 0, 1, "1=Play sounds in 3D.",
	"s_ReverbVol", OBSOLETE, CVT_BYTE, &cfg.snd_ReverbFactor, 0, 100,
	"General reverb strength (0-100).",
	"s_Custom", OBSOLETE, CVT_BYTE, &cfg.customMusic, 0, 1,
	"1=Enable custom (external) music files.",
	"ReverbDebug", OBSOLETE | CVF_NO_ARCHIVE, CVT_BYTE, &cfg.reverbDebug, 0, 1,
	"1=Reverb debug information in the console.",
	"Messages", OBSOLETE, CVT_BYTE, &cfg.msgShow, 0, 1, "1=Show messages.",
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
	"n_Slot", OBSOLETE, CVT_BYTE, &cfg.netSlot, 0, 6,
	"The savegame slot to start from. 0=none.",
	"n_Jump", OBSOLETE, CVT_BYTE, &cfg.netJumping, 0, 1,
	"1=Allow jumping in multiplayer games.",
	"Deathmatch", OBSOLETE, CVT_BYTE, &cfg.netDeathmatch, 0, 2,
	"Start multiplayers games as deathmatch.",
	"NoCoopDamage", OBSOLETE, CVT_BYTE, &cfg.noCoopDamage, 0, 1,
	"1=Disable player-player damage in co-op games.",
	"NoCoopWeapons", OBSOLETE, CVT_BYTE, &cfg.noCoopWeapons, 0, 1,
	"1=Disable multiplayer weapons during co-op games.",
	"NoTeamDamage", OBSOLETE, CVT_BYTE, &cfg.noTeamDamage, 0, 1,
	"1=Disable team damage (player color = team).",
	"n_Color", OBSOLETE, CVT_BYTE, &cfg.netColor, 0, 3,
	"Player color: 0=green, 1=gray, 2=brown, 3=red.",
	"AllowJump", OBSOLETE, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.",
	"FastMonsters", OBSOLETE, CVT_BYTE, &fastparm, 0, 1,
	"1=Fast monsters in non-demo single player.",
	"ZClip", OBSOLETE, CVT_BYTE, &cfg.moveCheckZ, 0, 1,
	"1=Allow mobjs to move under/over each other.",
	"JumpPower", OBSOLETE, CVT_FLOAT, &cfg.jumpPower, 0, 100, "Jump power.",
	"AutoSwitch", OBSOLETE, CVT_BYTE, &cfg.weaponAutoSwitch, 0, 1,
	"1=Change weapon automatically when picking one up.",
	"SecretMsg", OBSOLETE, CVT_BYTE, &cfg.secretMsg, 0, 1,
	"1=Announce the discovery of secret areas.",
	"EyeHeight", OBSOLETE, CVT_INT, &cfg.plrViewHeight, 41, 54,
	"Player eye height. The original is 41.",
	"CounterCheat", OBSOLETE, CVT_BYTE, &cfg.counterCheat, 0, 63,
	"6-bit bitfield. Show kills, items and secret counters in automap.",
	"LevelTitle", OBSOLETE, CVT_BYTE, &cfg.levelTitle, 0, 1,
	"1=Show level title and author in the beginning.",
	"Menu_R", OBSOLETE, CVT_FLOAT, &cfg.menuColor[0], 0, 1,
	"Menu color red component.",
	"Menu_G", OBSOLETE, CVT_FLOAT, &cfg.menuColor[1], 0, 1,
	"Menu color green component.",
	"Menu_B", OBSOLETE, CVT_FLOAT, &cfg.menuColor[2], 0, 1,
	"Menu color blue component.",
	"MenuFog", OBSOLETE, CVT_INT, &cfg.menuFog, 0, 1,
	"Menu fog mode: 0=blue vertical, 1=black smoke.",
	"MsgCount", OBSOLETE, CVT_INT, &cfg.msgCount, 0, 8,
	"Number of HUD messages displayed at the same time.",
	"MsgScale", OBSOLETE | CVF_NO_MAX, CVT_FLOAT, &cfg.msgScale, 0, 0,
	"Scaling factor for HUD messages.",
	"MsgUptime", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.msgUptime, 35, 0,
	"Number of tics to keep HUD messages on screen.",
	"MsgBlink", OBSOLETE, CVT_BYTE, &cfg.msgBlink, 0, 1,
	"1=HUD messages blink when they're printed.",
	"CorpseTime", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0,
	"Corpse vanish time in seconds, 0=disabled.",

	"game-corpsetime", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0,
	"Corpse vanish time in seconds, 0=disabled.",

	//===========================================================================
	// New names (1.13.0 =>)
	//===========================================================================
	"input-mouse-x-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
	"Mouse X axis sensitivity.",
	"input-mouse-y-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
	"Mouse Y axis sensitivity.",
	"ctl-look-joy-inverse", 0, CVT_INT, &cfg.jlookInverseY, 0, 1,
	"1=Inverse joystick look Y axis.",
	"ctl-look-mouse-inverse", 0, CVT_INT, &cfg.mlookInverseY, 0, 1,
	"1=Inverse mouse look Y axis.",
	"ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1,
	"1=Joystick values => look angle delta.",
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
	"msg-echo", 0, CVT_BYTE, &cfg.echoMsg, 0, 1,
	"1=Echo all messages to the console.",
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
	"hud-face", 0, CVT_BYTE, &cfg.hudShown[HUD_FACE], 0, 1,
	"1=Show Doom guy's face in HUD.",
	"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1,
	"1=Show health in HUD.",
	"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1,
	"1=Show armor in HUD.",
	"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1,
	"1=Show ammo in HUD.",
	"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1,
	"1=Show keys in HUD.",
	"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1,
	"1=Show deathmatch frags in HUD.",
	"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10,
	"Scaling for HUD info.",
	"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1, "HUD info color red component.",
	"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1, "HUD info color green component.",
	"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1, "HUD info color alpha component.",
	"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1, "HUD info alpha value.",
	"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1, "HUD icon alpha value.",
	"hud-frags-all", 0, CVT_BYTE, &hu_showallfrags, 0, 1,
	"Debug: HUD shows all frags of all players.",

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
	"Scale for viewheight bobbing.",
	"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1,
	"Scale for player weapon bobbing.",
	"view-bob-weapon-switch-lower", 0, CVT_BYTE, &cfg.bobWeaponLower, 0, 1,
	"HUD weapon lowered during weapon switching.",

	"music-custom", 0, CVT_BYTE, &cfg.customMusic, 0, 1,
	"1=Enable custom (external) music files.",

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
	"server-game-deathmatch", 0, CVT_BYTE, &cfg.netDeathmatch, 0, 2,
	"Start multiplayers games as deathmatch.",
	"server-game-coop-nodamage", 0, CVT_BYTE, &cfg.noCoopDamage, 0, 1,
	"1=Disable player-player damage in co-op games.",
	"server-game-coop-noweapons", 0, CVT_BYTE, &cfg.noCoopWeapons, 0, 1,
	"1=Disable multiplayer weapons during co-op games.",
	"server-game-noteamdamage", 0, CVT_BYTE, &cfg.noTeamDamage, 0, 1,
	"1=Disable team damage (player color = team).",
	"server-game-deathmatch-killmsg", 0, CVT_BYTE, &cfg.killMessages, 0, 1,
	"1=Announce frags in deathmatch.",
	"server-game-nobfg", 0, CVT_BYTE, &cfg.noNetBFG, 0, 1,
	"1=Disable BFG9000 in all netgames.",
	"server-game-coop-nothing", 0, CVT_BYTE, &cfg.noCoopAnything, 0, 1,
	"1=Disable all multiplayer objects in co-op games.",
	"server-game-coop-respawn-items", 0, CVT_BYTE, &cfg.coopRespawnItems, 0, 1,
	"1=Respawn items in co-op games.",
    {"server-game-respawn-monsters-nightmare", 0, CVT_BYTE, 
        &cfg.respawnMonstersNightmare, 0, 1, 
        "1=Monster respawning in Nightmare difficulty enabled."},

	// Player data.
	"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 3,
	"Player color: 0=green, 1=gray, 2=brown, 3=red.",
	"player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.",
	"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100, "Jump power.",
	"player-air-movement", 0, CVT_BYTE, &cfg.airborneMovement, 0, 32,
	"Player movement speed while airborne.",
	"player-autoswitch", 0, CVT_BYTE, &cfg.weaponAutoSwitch, 0, 1,
	"1=Change weapon automatically when picking one up.",
	"player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54,
	"Player eye height. The original is 41.",
	"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1,
	"1=Camera players have no movement clipping.",
	"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1,
	"Player movement speed modifier.",

	// Compatibility options
	"game-raiseghosts", 0, CVT_BYTE, &cfg.raiseghosts, 0, 1,
	"1= Archviles raise ghosts from squished corpses.",
	"game-maxskulls", 0, CVT_BYTE, &cfg.maxskulls, 0, 1,
	"1= Pain Elementals can't spawn Lost Souls if more than twenty already exist.",
	"game-skullsinwalls", 0, CVT_BYTE, &cfg.allowskullsinwalls, 0, 1,
	"1= Pain Elementals can spawn Lost Souls inside walls.",

	"game-fastmonsters", 0, CVT_BYTE, &fastparm, 0, 1,
	"1=Fast monsters in non-demo single player.",
	"game-zclip", 0, CVT_BYTE, &cfg.moveCheckZ, 0, 1,
	"1=Allow mobjs to move under/over each other.",
	"game-corpse-time", CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0,
	"Corpse vanish time in seconds, 0=disabled.",
	"game-corpse-sliding", 0, CVT_BYTE, &cfg.slidingCorpses, 0, 1,
	"1=Corpses slide down stairs and ledges.",

	"hud-title", 0, CVT_BYTE, &cfg.levelTitle, 0, 1,
	"1=Show level title and author in the beginning.",
	"hud-title-noidsoft", 0, CVT_BYTE, &cfg.hideAuthorIdSoft, 0, 1,
	"1=Don't show map author if it's \"id Software\".",

	"msg-show", 0, CVT_BYTE, &cfg.msgShow, 0, 1, "1=Show messages.",
	"msg-secret", 0, CVT_BYTE, &cfg.secretMsg, 0, 1,
	"1=Announce the discovery of secret areas.",

	"msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2,
	"Alignment of HUD messages. 0 = left, 1 = center, 2 = right.",

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
	"spy", CCmdCycleSpy, "Spy mode: cycle player views in co-op.",
	"screenshot", CCmdScreenShot, "Takes a screenshot. Saved to DOOMnn.TGA.",
	"viewsize", CCmdViewSize, "View size adjustment.",
	"sbsize", CCmdViewSize, "Status bar size adjustment.",
	//  "playdemo",     CCmdPlayDemo,       "Play a demo.",
	//  "recorddemo",   CCmdRecordDemo,     "Record a demo in the current level or in the specified one.",
	//  "stopdemo",     CCmdStopDemo,       "Stop demo playback/recording.",
	"pause", CCmdPause, "Pause the game.",
	//  "cd",           CCmdCD,             "CD control.",
	//  "midi",         CCmdMidi,           "Music control.",
	"crosshair", CCmdCrosshair, "Crosshair setup.",
	"cheat", CCmdCheat, "Issue a cheat code using the original Doom cheats.",
	"god", CCmdCheatGod, "God mode.",
	"noclip", CCmdCheatNoClip, "No movement clipping (walk through walls).",
	"warp", CCmdCheatWarp, "Warp to another map.",
	"reveal", CCmdCheatReveal, "Map cheat.",
	"give", CCmdCheatGive, "Gives you weapons, ammo, power-ups, etc.",
	"kill", CCmdCheatMassacre, "Kill all the monsters on the level.",
	"suicide", 	CCmdSuicide, "Kill yourself. What did you think?",
	"doomfont", CCmdDoomFont, "Use the Doom font in the console.",
	"beginchat", CCmdBeginChat, "Begin chat mode.",
	"msgrefresh", CCmdMsgRefresh, "Show last HUD message.",

	"startinf", CCmdStartInFine, "Start an InFine script.",
	"stopinf", CCmdStopInFine, "Stop the currently playing interlude/finale.",
	"stopfinale", CCmdStopInFine,
	"Stop the currently playing interlude/finale.",

	"spawnmobj", CCmdSpawnMobj, "Spawn a new mobj.",
	"coord", CCmdPrintPlayerCoords,
	"Print the coordinates of the consoleplayer.",
	"message", CCmdLocalMessage, "Show a local game message.",

	// $democam: console commands
	"makelocp", CCmdMakeLocal, "Make local player.",
	"makecam", CCmdSetCamera, "Toggle camera mode.",
	"setlock", CCmdSetViewLock, "Set camera viewlock.",
	"lockmode", CCmdSetViewLock, "Set camera viewlock mode.",

	// $moveplane: console commands
	"movefloor", CCmdMovePlane, "Move a sector's floor plane.",
	"moveceil", CCmdMovePlane, "Move a sector's ceiling plane.",
	"movesec", CCmdMovePlane, "Move a sector's both planes.",
	NULL
};

// Private Data -----------------------------------------------------------

// Code -------------------------------------------------------------------

void D_ConsoleRegistration()
{
	int     i;

	for(i = 0; gameCVars[i].name; i++)
		Con_AddVariable(gameCVars + i);
	for(i = 0; gameCCmds[i].name; i++)
		Con_AddCommand(gameCCmds + i);
	D_NetConsoleRegistration();
}

void D_ConsoleBg(int *width, int *height)
{
	extern int consoleFlat;
	extern float consoleZoom;

	GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
	*width = 64 * consoleZoom;
	*height = 64 * consoleZoom;
}

int CCmdScreenShot(int argc, char **argv)
{
	G_ScreenShot();
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
		S_LocalSound(sfx_oof, NULL);
		Con_Printf("Can only suicide when in a game!\n", argv[0]);
		return true;

	}

	if(deathmatch)
	{
		S_LocalSound(sfx_oof, NULL);
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

int CCmdPause(int argc, char **argv)
{
	extern boolean sendpause;

	if(!menuactive)
		sendpause = true;
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

int CCmdDoomFont(int argc, char **argv)
{
	ddfont_t cfont;

	cfont.flags = DDFONT_WHITE;
	cfont.height = 8;
	cfont.sizeX = 1.5f;
	cfont.sizeY = 2;
	cfont.TextOut = ConTextOut;
	cfont.Width = ConTextWidth;
	cfont.Filter = ConTextFilter;
	Con_SetFont(&cfont);
	return true;
}
