/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * con_main.c: Console Subsystem
 *
 * Should be completely redesigned.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

#include "de_platform.h"

#ifdef WIN32
#	include <process.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_misc.h"

#ifdef TextOut
// Windows has its own TextOut.
#undef TextOut
#endif

// MACROS ------------------------------------------------------------------

#define SC_EMPTY_QUOTE	-1

// Length of the print buffer. Used in Con_Printf. If console messages are
// longer than this, an error will occur.
#define PRBUFF_LEN		8000

#define OBSOLETE		CVF_NO_ARCHIVE|CVF_HIDE

// Macros for accessing the console command values through the void ptr.
#define CV_INT(var)		(*(int*) var->ptr)
#define CV_BYTE(var)	(*(byte*) var->ptr)
#define CV_FLOAT(var)	(*(float*) var->ptr)
#define CV_CHARPTR(var)	(*(char**) var->ptr)

// Operators for the "if" command.
enum 
{
	IF_EQUAL,
	IF_NOT_EQUAL,
	IF_GREATER,
	IF_LESS,
	IF_GEQUAL,
	IF_LEQUAL
};

// TYPES -------------------------------------------------------------------

typedef struct calias_s {
	char *name;
	char *command;
} calias_t;

typedef struct execbuff_s {
	boolean	used;				// Is this in use?
	timespan_t when;			// System time when to execute the command.
	char subCmd[1024];			// A single command w/args.
} execbuff_t;

typedef struct knownword_S {
	char word[64];				// 64 chars MAX for words.
} knownword_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ListCmds);
D_CMD(ListVars);
D_CMD(Console);
D_CMD(Version);
D_CMD(Quit);
D_CMD(LoadFile);
D_CMD(UnloadFile);
D_CMD(ResetLumps);
D_CMD(ListFiles);
D_CMD(BackgroundTurn);
D_CMD(Dump);
D_CMD(SkyDetail);
D_CMD(Fog);
D_CMD(Font);
D_CMD(Alias);
D_CMD(ListAliases);
D_CMD(SetGamma);
D_CMD(SetRes);
D_CMD(Chat);
D_CMD(Parse);
D_CMD(DeleteBind);
D_CMD(Wait);
D_CMD(Repeat);
D_CMD(Echo);
D_CMD(FlareConfig);
D_CMD(ClearBindings);
D_CMD(AddSub);
D_CMD(Ping);
D_CMD(Login);
D_CMD(Logout);
D_CMD(Dir);
D_CMD(Toggle);
D_CMD(If);

#ifdef _DEBUG
D_CMD(TranslateFont);
#endif

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int executeSubCmd(const char *subCmd);
void Con_SplitIntoSubCommands(const char *command, timespan_t markerOffset);
calias_t *Con_GetAlias(const char *name);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean paletted, r_s3tc;	// Use GL_EXT_paletted_texture
extern int freezeRLs;

extern cvar_t netCVars[], inputCVars[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ddfont_t Cfont;				// The console font.

float	CcolYellow[3] = { 1, .85f, .3f };

boolean	ConsoleSilent = false;
int		CmdReturnValue = 0;	

float	ConsoleOpenY;			// Where the console bottom is when open.

int		consoleTurn;			// The rotation variable.
int		consoleLight = 50, consoleAlpha = 75;
int		conCompMode = 0;		// Completion mode.
int		conSilentCVars = 1;		
boolean	consoleDump = true;
int		consoleActiveKey = '`';	// Tilde.
boolean	consoleShowKeys = false;
boolean	consoleShowFPS = false;
boolean consoleShadowText = true;

cvar_t	*cvars = NULL;
int		numCVars = 0;

ccmd_t	*ccmds = NULL;			// The list of console commands.
int		numCCmds = 0;			// Number of console commands.

calias_t *caliases = NULL;
int		numCAliases = 0;

// Console variables (old <= 1.5.x versions first, 1.6.0 below).
// (Cvars provide access to global variables via the console.)
// FIXME: These don't really all belong here.
cvar_t engineCVars[] = 
{
	"bgAlpha",			OBSOLETE,			CVT_INT,	&consoleAlpha,	0, 100,	"Console background translucency.",
	"bgLight",			OBSOLETE,			CVT_INT,	&consoleLight,	0, 100,	"Console background light level.",
	"CompletionMode",	OBSOLETE,			CVT_INT,	&conCompMode,	0, 1,	"How to complete words when pressing Tab:\n0=Show completions, 1=Cycle through them.",
	"ConsoleDump",		OBSOLETE,			CVT_BYTE,	&consoleDump,	0, 1,	"1=Dump all console messages to Doomsday.out.",
	"ConsoleKey",		OBSOLETE,			CVT_INT,	&consoleActiveKey, 0, 255, "Key to activate the console (ASCII code, default is tilde, 96).",
	"ConsoleShowKeys",	OBSOLETE,			CVT_BYTE,	&consoleShowKeys, 0, 1,	"1=Show ASCII codes of pressed keys in the console.",
	"SilentCVars",		OBSOLETE,			CVT_BYTE,	&conSilentCVars,0, 1,	"1=Don't show the value of a cvar when setting it.",
	"dlBlend",			OBSOLETE,			CVT_INT,	&dlBlend,		0, 3,	"Dynamic lights color blending mode:\n0=normal, 1=additive, 2=no blending.",
//	"dlClip",			OBSOLETE,			CVT_INT,	&clipLights,	0, 1,	"1=Clip dynamic lights (try using with dlblend 2).",
	"dlFactor",			OBSOLETE,			CVT_FLOAT,	&dlFactor,		0, 1,	"Intensity factor for dynamic lights.",
	"DynLights",		OBSOLETE,			CVT_INT,	&useDynLights,	0, 1,	"1=Render dynamic lights.",
	"WallGlow",			OBSOLETE,			CVT_INT,	&useWallGlow,	0, 1,	"1=Render glow on walls.",
	"GlowHeight",		OBSOLETE,			CVT_INT,	&glowHeight,	0, 1024,"Height of wall glow.",
	//"i_JoySensi",		OBSOLETE,			CVT_INT,	&joySensitivity,0, 9,	"Joystick sensitivity.",
	//"i_MouseInvY",		OBSOLETE,			CVT_INT,	&mouseInverseY, 0, 1,	"1=Inversed mouse Y axis.",
	//"i_mWheelSensi",OBSOLETE|CVF_NO_MAX,		CVT_INT,	&mouseWheelSensi, 0, 0, "Mouse wheel sensitivity.",
	"i_KeyWait1",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&repWait1,		6, 0,	"The number of 35 Hz ticks to wait before first key repeat.",
	"i_KeyWait2",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&repWait2,		1, 0,	"The number of 35 Hz ticks to wait between key repeats.",
	//"i_MouseDisX",		OBSOLETE,			CVT_INT,	&mouseDisableX,	0, 1,	"1=Disable mouse X axis.",
	//"i_MouseDisY",		OBSOLETE,			CVT_INT,	&mouseDisableY,	0, 1,	"1=Disable mouse Y axis.",
	"MaxDl",		OBSOLETE|CVF_NO_MAX,		CVT_INT,	&maxDynLights,	0, 0,	"The maximum number of dynamic lights. 0=no limit.",
	"n_ServerName",		OBSOLETE,			CVT_CHARPTR, &serverName,	0, 0,	"The name of this computer if it's a server.",
	"n_ServerInfo",		OBSOLETE,			CVT_CHARPTR, &serverInfo,	0, 0,	"The description given of this computer if it's a server.",
	"n_PlrName",		OBSOLETE,			CVT_CHARPTR, &playerName,	0, 0,	"Your name in multiplayer games.",
	"npt_Active",		OBSOLETE,			CVT_INT,	&nptActive,		0, 3,	"Network protocol: 0=TCP/IP, 1=IPX, 2=Modem, 3=Serial link.",
	"npt_IPAddress",	OBSOLETE,			CVT_CHARPTR, &nptIPAddress, 0, 0,	"TCP/IP address for searching servers.",
	"npt_IPPort",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&nptIPPort,		0, 0,	"TCP/IP port to use for all data traffic.",
	"npt_Modem",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&nptModem,		0, 0,	"Index of the selected modem.",
	"npt_PhoneNum",		OBSOLETE,			CVT_CHARPTR, &nptPhoneNum,	0, 0,	"Phone number to dial to when connecting.",
	"npt_SerialPort",	OBSOLETE,			CVT_INT,	&nptSerialPort,	1, 8,	"COM port to use for serial link connection.",
	"npt_SerialBaud", OBSOLETE|CVF_NO_MAX,	CVT_INT,	&nptSerialBaud,	0, 0,	"Baud rate for serial link connections.",
	"npt_SerialStopBits", OBSOLETE,		CVT_INT,	&nptSerialStopBits, 0, 2, "0=1 bit, 1=1.5 bits, 2=2 bits.",
	"npt_SerialParity", OBSOLETE,			CVT_INT,	&nptSerialParity, 0, 3,	"0=None, 1=odd, 2=even, 3=mark parity.",
	"npt_SerialFlowCtrl", OBSOLETE,		CVT_INT,	&nptSerialFlowCtrl, 0, 4, "0=None, 1=XON/XOFF, 2=RTS, 3=DTR, 4=RTS/DTR flow control.",
	"r_Ambient",		OBSOLETE,			CVT_INT,	&r_ambient,		0, 255, "Ambient light level.",
	"r_FOV",			OBSOLETE,		CVT_FLOAT,	&fieldOfView,	1, 179, "Field of view.",
	"r_Gamma",		OBSOLETE|CVF_PROTECTED,	CVT_INT,	&usegamma,		0, 4,	"The gamma correction level (0-4).",
	"r_SmoothRaw",	OBSOLETE|CVF_PROTECTED,	CVT_INT,	&linearRaw,		0, 1,	"1=Fullscreen images (320x200) use linear interpolation.",
	"r_Mipmapping",	OBSOLETE|CVF_PROTECTED,	CVT_INT,	&mipmapping,	0, 5,	"The mipmapping mode for textures.",
	"r_SkyDetail",	OBSOLETE|CVF_PROTECTED,	CVT_INT,	&skyDetail,		3, 7,	"Number of sky sphere quadrant subdivisions.",
	"r_SkyRows",	OBSOLETE|CVF_PROTECTED,	CVT_INT,	&skyRows,		1, 8,	"Number of sky sphere rows.",
	"r_SkyDist",	OBSOLETE|CVF_NO_MAX,		CVT_FLOAT,	&skyDist,		1, 0,	"Sky sphere radius.",
	"r_FullSky",		OBSOLETE,			CVT_INT,	&r_fullsky,		0, 1,	"1=Always render the full sky sphere.",
	"r_Paletted",	OBSOLETE|CVF_PROTECTED,	CVT_BYTE,	&paletted,		0, 1,	"1=Use the GL_EXT_shared_texture_palette extension.",
	"r_SpriteFilter",	OBSOLETE,			CVT_INT,	&filterSprites,	0, 1,	"1=Render smooth sprites.",
	"r_Textures",	OBSOLETE|CVF_NO_ARCHIVE,	CVT_BYTE,	&renderTextures,0, 1,	"1=Render with textures.",
	"r_TexQuality",		OBSOLETE,			CVT_INT,	&texQuality,	0, 8,	"The quality of textures (0-8).",
	"r_MaxSpriteAngle", OBSOLETE,			CVT_FLOAT,	&maxSpriteAngle, 0, 90,	"Maximum angle for slanted sprites (spralign 2).",
	"r_dlMaxRad",		OBSOLETE,			CVT_INT,	&dlMaxRad,		64,	512, "Maximum radius of dynamic lights (default: 128).",
	"r_dlRadFactor",	OBSOLETE,			CVT_FLOAT,	&dlRadFactor,	0.1f, 10, "A multiplier for dynlight radii (default: 1).",
	"r_TexGlow",		OBSOLETE,			CVT_INT,	&r_texglow,		0, 1,	"1=Enable glowing textures.",
	"r_WeaponOffsetScale", OBSOLETE|CVF_NO_MAX, CVT_FLOAT, &weaponOffsetScale, 0, 0, "Scaling of player weapon (x,y) offset.",
	"r_NoSpriteZ",		OBSOLETE,			CVT_INT,	&r_nospritez,	0, 1,	"1=Don't write sprites in the Z buffer.",
	"r_Precache",		OBSOLETE,			CVT_BYTE,	&r_precache_sprites, 0, 1, "1=Precache sprites at level setup (slow).",
	"r_UseSRVO",		OBSOLETE,			CVT_INT,	&r_use_srvo,	0, 2,	"1=Use short-range visual offsets for models.\n2=Use SRVO for sprites, too (unjags actor movement).",
	"r_UseVisAngle",	OBSOLETE,			CVT_INT,	&r_use_srvo_angle, 0, 1, "1=Use separate visual angle for mobjs (unjag actors).",
	"r_Detail",			OBSOLETE,			CVT_INT,	&r_detail,		0, 1,	"1=Render with detail textures.",
	"DefaultWads",		OBSOLETE,			CVT_CHARPTR, &defaultWads,	0, 0,	"The list of WADs to be loaded at startup.",
	"DefResX",		OBSOLETE|CVF_NO_MAX,		CVT_INT,	&defResX,		320, 0,	"Default resolution (X).",
	"DefResY",		OBSOLETE|CVF_NO_MAX,		CVT_INT,	&defResY,		240, 0, "Default resolution (Y).",
	"SimpleSky",		OBSOLETE,			CVT_INT,	&simpleSky,		0, 2,	"Sky rendering mode: 0=normal, 1=quads.",
	"SprAlign",			OBSOLETE,			CVT_INT,	&alwaysAlign,	0, 3,	"1=Always align sprites with the view plane.\n2=Align to camera, unless slant > r_maxSpriteAngle.",
	"SprBlend",			OBSOLETE,			CVT_INT,	&missileBlend,	0, 1,	"1=Use additive blending for explosions.",
	"SprLight",			OBSOLETE,			CVT_INT,	&litSprites,	0, 1,	"1=Sprites lit using dynamic lights.",
	"s_VolMIDI",	OBSOLETE|CVF_PROTECTED,	CVT_INT,	&mus_volume/*&snd_MusicVolume*/, 0, 255, "MIDI music volume (0-255).",
	"s_VolSFX",			OBSOLETE,			CVT_INT,	&sfx_volume/*&snd_SfxVolume*/,	0, 255,	"Sound effects volume (0-255).",
	"UseModels",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&useModels,		0, 1,	"Render using 3D models when possible.",
	"UseParticles",		OBSOLETE,			CVT_INT,	&r_use_particles, 0, 1,	"1=Render particle effects.",
	"MaxParticles",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&r_max_particles, 0, 0,	"Maximum number of particles to render. 0=no limit.",
	"PtcSpawnRate",		OBSOLETE,			CVT_FLOAT,	&r_particle_spawn_rate, 0, 5, "Particle spawn rate multiplier (default: 1).",
	"ModelLight",		OBSOLETE,			CVT_INT,	&modelLight,	0, 10,	"Maximum number of light sources on models.",
	"ModelInter",		OBSOLETE,			CVT_INT,	&frameInter,	0, 1,	"1=Interpolate frames.",
	"ModelAspectMod", OBSOLETE|CVF_NO_MAX|CVF_NO_MIN, CVT_FLOAT, &rModelAspectMod, 0, 0, "Scale for MD2 z-axis when model is loaded.",
	"ModelMaxZ",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&r_maxmodelz,	0, 0,	"Farther than this models revert back to sprites.",
//	"PredictTics",	OBSOLETE|CVF_NO_MAX,		CVT_INT,	&predict_tics,	0, 0,	"Max tics to predict ahead.",
//	"MaxQueuePackets",	OBSOLETE,			CVT_INT,	&maxQueuePackets, 0, 16, "Max packets in send queue.",
	"sv_MasterAware",	OBSOLETE,			CVT_INT,	&masterAware,	0, 1,	"1=Send info to master server.",
	"MasterAddress",	OBSOLETE,			CVT_CHARPTR, &masterAddress, 0, 0,	"Master server IP address / name.",
	"MasterPort",		OBSOLETE,			CVT_INT,	&masterPort,	0, 65535, "Master server TCP/IP port.",
	"vid_Gamma",		OBSOLETE,			CVT_FLOAT,	&vid_gamma,		0.1f, 6, "Display gamma correction factor: 1=normal.",
	"vid_Contrast",		OBSOLETE,			CVT_FLOAT,	&vid_contrast,	0, 10,	"Display contrast: 1=normal.",
	"vid_Bright",		OBSOLETE,			CVT_FLOAT,	&vid_bright,	-2, 2,	"Display brightness: -1=dark, 0=normal, 1=light.",

//===========================================================================
// The new, consistent names (1.6.0 ->)
//===========================================================================
	// Console
	"con-alpha",			0,			CVT_INT,	&consoleAlpha,	0, 100,	"Console background translucency.",
	"con-light",			0,			CVT_INT,	&consoleLight,	0, 100,	"Console background light level.",
	"con-completion",		0,			CVT_INT,	&conCompMode,	0, 1,	"How to complete words when pressing Tab:\n0=Show completions, 1=Cycle through them.",
	"con-dump",				0,			CVT_BYTE,	&consoleDump,	0, 1,	"1=Dump all console messages to Doomsday.out.",
	"con-key-activate",		0,			CVT_INT,	&consoleActiveKey, 0, 255, "Key to activate the console (ASCII code, default is tilde, 96).",
	"con-key-show",			0,			CVT_BYTE,	&consoleShowKeys, 0, 1,	"1=Show ASCII codes of pressed keys in the console.",
	"con-var-silent",		0,			CVT_BYTE,	&conSilentCVars,0, 1,	"1=Don't show the value of a cvar when setting it.",
	"con-progress",			0,			CVT_BYTE,	&progress_enabled, 0, 1, "1=Show progress bar.",
	"con-fps",				0,			CVT_BYTE,	&consoleShowFPS, 0, 1,	"1=Show FPS counter on screen.",
	"con-text-shadow",		0,			CVT_BYTE,	&consoleShadowText, 0, 1, "1=Text in the console has a shadow (might be slow).",

	// User Interface
	"ui-panel-help",		0,			CVT_BYTE,	&panel_show_help, 0, 1,	"1=Enable help window in Control Panel.",
	"ui-panel-tips",		0,			CVT_BYTE,	&panel_show_tips, 0, 1, "1=Show help indicators in Control Panel.",
	"ui-cursor-width",		CVF_NO_MAX,	CVT_INT,	&uiMouseWidth,	1, 0,	"Mouse cursor width.",
	"ui-cursor-height",		CVF_NO_MAX,	CVT_INT,	&uiMouseHeight,	1, 0,	"Mouse cursor height.",

	// Video
	"vid-res-x",			CVF_NO_MAX,	CVT_INT,	&defResX,		320, 0,	"Default resolution (X).",
	"vid-res-y",			CVF_NO_MAX,	CVT_INT,	&defResY,		240, 0, "Default resolution (Y).",
	"vid-gamma",			0,			CVT_FLOAT,	&vid_gamma,		0.1f, 6, "Display gamma correction factor: 1=normal.",
	"vid-contrast",			0,			CVT_FLOAT,	&vid_contrast,	0, 10,	"Display contrast: 1=normal.",
	"vid-bright",			0,			CVT_FLOAT,	&vid_bright,	-2, 2,	"Display brightness: -1=dark, 0=normal, 1=light.",

	// Render
	"rend-dev-freeze",		0,			CVT_INT,	&freezeRLs,		0, 1,	"1=Stop updating rendering lists.",
	"rend-dev-wireframe",	0,			CVT_INT,	&renderWireframe, 0, 1,	"1=Render player view in wireframe mode.",
	"rend-dev-framecount",	CVF_PROTECTED, CVT_INT,	&framecount, 	0, 0,	"Frame counter.",
	// * Render-Info
	"rend-info-tris",		0,			CVT_BYTE,	&rend_info_tris, 0, 1,	"1=Print triangle count after rendering a frame.",
	"rend-info-lums",		0,			CVT_BYTE,	&rend_info_lums, 0, 1,	"1=Print lumobj count after rendering a frame.",
	// * Render-Light
	"rend-light-ambient",	0,			CVT_INT,	&r_ambient,		0, 255, "Ambient light level.",
	"rend-light",			0,			CVT_INT,	&useDynLights,	0, 1,	"1=Render dynamic lights.",
	"rend-light-blend",		0,			CVT_INT,	&dlBlend,		0, 3,	"Dynamic lights color blending mode:\n0=normal, 1=additive, 2=no blending.",
//	"rend-light-clip",		0,			CVT_INT,	&clipLights,	0, 1,	"1=Clip dynamic lights (try using with dlblend 2).",
	"rend-light-bright",	0,			CVT_FLOAT,	&dlFactor,		0, 1,	"Intensity factor for dynamic lights.",
	"rend-light-num",		0,			CVT_INT,	&maxDynLights,	0, 8000, "The maximum number of dynamic lights. 0=no limit.",
//	"rend-light-shrink",	0,			CVT_FLOAT,	&dlContract,	0, 1,	"Shrink dynlight wall polygons horizontally.",
	"rend-light-radius-scale",	0,		CVT_FLOAT,	&dlRadFactor,	0.1f, 10, "A multiplier for dynlight radii (default: 1).",
	"rend-light-radius-max", 0,			CVT_INT,	&dlMaxRad,		64,	512, "Maximum radius of dynamic lights (default: 128).",
	"rend-light-wall-angle", CVF_NO_MAX, CVT_FLOAT,	&rend_light_wall_angle, 0, 0, "Intensity of angle-based wall light.",
	"rend-light-multitex",	0,			CVT_INT,	&useMultiTexLights, 0, 1, "1=Use multitexturing when rendering dynamic lights.",
	// * Render-Light-Decor
	"rend-light-decor",		0,			CVT_BYTE,	&useDecorations, 0, 1,	"1=Enable surface light decorations.",
	"rend-light-decor-plane-far", CVF_NO_MAX, CVT_FLOAT, &decorPlaneMaxDist, 0, 0, "Maximum distance at which plane light decorations are visible.",
	"rend-light-decor-wall-far", CVF_NO_MAX, CVT_FLOAT, &decorWallMaxDist, 0, 0, "Maximum distance at which wall light decorations are visible.",
	"rend-light-decor-plane-bright", 0,	CVT_FLOAT,	&decorPlaneFactor, 0, 10, "Brightness of plane light decorations.",
	"rend-light-decor-wall-bright", 0,	CVT_FLOAT,	&decorWallFactor, 0, 10, "Brightness of wall light decorations.",
	"rend-light-decor-angle",	0,		CVT_FLOAT,	&decorFadeAngle, 0, 1,	"Reduce brightness if surface/view angle too steep.",
	"rend-light-sky",		0,			CVT_INT,	&rendSkyLight, 	0, 1,	"1=Use special light color in sky sectors.",
	// * Render-Glow
	"rend-glow",			0,			CVT_INT,	&r_texglow,		0, 1,	"1=Enable glowing textures.",
	"rend-glow-wall",		0,			CVT_INT,	&useWallGlow,	0, 1,	"1=Render glow on walls.",
	"rend-glow-height",		0,			CVT_INT,	&glowHeight,	0, 1024,"Height of wall glow.",
	"rend-glow-fog-bright", 0,			CVT_FLOAT,	&glowFogBright, 0, 1,	"Brightness of wall glow when fog is enabled.",
	// * Render-Halo
	"rend-halo",			0,			CVT_INT,	&haloMode,		0, 5,	"Number of flares to draw per light.",
	"rend-halo-bright",		0,			CVT_INT,	&haloBright,	0, 100,	"Halo/flare brightness.",
	"rend-halo-occlusion",	CVF_NO_MAX,	CVT_INT,	&haloOccludeSpeed, 0, 0, "Rate at which occluded halos fade.",
	"rend-halo-size",		0,			CVT_INT,	&haloSize,		0, 100,	"Size of halos.",
	"rend-halo-secondary-limit", CVF_NO_MAX, CVT_FLOAT, &minHaloSize, 0, 0,	"Minimum halo size.",
	"rend-halo-fade-far",	CVF_NO_MAX,	CVT_FLOAT,	&haloFadeMax,	0, 0,	"Distance at which halos are no longer visible.",
	"rend-halo-fade-near",	CVF_NO_MAX,	CVT_FLOAT,	&haloFadeMin,	0, 0,	"Distance to begin fading halos.",
	// * Render-FakeRadio
	"rend-fakeradio", 		0,			CVT_INT,	&rendFakeRadio, 0, 1,	"1=Enable simulated radiosity lighting.",
	// * Render-Camera
	"rend-camera-fov",		0,			CVT_FLOAT,	&fieldOfView,	1, 179, "Field of view.",
	// * Render-Texture
	"rend-tex",				CVF_NO_ARCHIVE,	CVT_INT, &renderTextures,0, 1,	"1=Render with textures.",
	"rend-tex-gamma",		CVF_PROTECTED, CVT_INT,	&usegamma,		0, 4,	"The gamma correction level (0-4).",
	"rend-tex-mipmap",		CVF_PROTECTED, CVT_INT,	&mipmapping,	0, 5,	"The mipmapping mode for textures.",
	"rend-tex-paletted",	CVF_PROTECTED, CVT_BYTE, &paletted,		0, 1,	"1=Use the GL_EXT_shared_texture_palette extension.",
	"rend-tex-external-always", 0,		CVT_BYTE,	&loadExtAlways, 0, 1,	"1=Always use external texture resources (overrides -pwadtex).",
	"rend-tex-quality",		0,			CVT_INT,	&texQuality,	0, 8,	"The quality of textures (0-8).",
	"rend-tex-filter-sprite", 0,		CVT_INT,	&filterSprites,	0, 1,	"1=Render smooth sprites.",
	"rend-tex-filter-raw",	CVF_PROTECTED, CVT_INT,	&linearRaw,		0, 1,	"1=Fullscreen images (320x200) use linear interpolation.",
	"rend-tex-filter-smart", 0,			CVT_INT,	&useSmartFilter, 0, 1,	"1=Use hq2x-filtering on all textures.",
	"rend-tex-detail",		0,			CVT_INT,	&r_detail,		0, 1,	"1=Render with detail textures.",
	"rend-tex-detail-scale", CVF_NO_MIN|CVF_NO_MAX, CVT_FLOAT, &detailScale, 0, 0, "Global detail texture factor.",
	"rend-tex-detail-strength", 0,		CVT_FLOAT,	&detailFactor, 0, 10,	"Global detail texture strength factor.",
	//"rend-tex-detail-far",	CVF_NO_MAX, CVT_FLOAT,	&detailMaxDist,	1, 0,	"Maximum distance where detail textures are visible.",
	"rend-tex-detail-multitex", 0,		CVT_INT,	&useMultiTexDetails, 0, 1,	"1=Use multitexturing when rendering detail textures.",
	"rend-tex-anim-smooth",	0,			CVT_BYTE,	&smoothTexAnim,	0, 1,	"1=Enable interpolated texture animation.",
	// * Render-Sky
	"rend-sky-detail",		CVF_PROTECTED, CVT_INT,	&skyDetail,		3, 7,	"Number of sky sphere quadrant subdivisions.",
	"rend-sky-rows",		CVF_PROTECTED, CVT_INT,	&skyRows,		1, 8,	"Number of sky sphere rows.",
	"rend-sky-distance",	CVF_NO_MAX,	CVT_FLOAT,	&skyDist,		1, 0,	"Sky sphere radius.",
	"rend-sky-full",		0,			CVT_INT,	&r_fullsky,		0, 1,	"1=Always render the full sky sphere.",
	"rend-sky-simple",		0,			CVT_INT,	&simpleSky,		0, 2,	"Sky rendering mode: 0=normal, 1=quads.",
	// * Render-Sprite
	"rend-sprite-align-angle", 0,		CVT_FLOAT,	&maxSpriteAngle, 0, 90,	"Maximum angle for slanted sprites (spralign 2).",
	"rend-sprite-noz",		0,			CVT_INT,	&r_nospritez,	0, 1,	"1=Don't write sprites in the Z buffer.",
	"rend-sprite-precache",	0,			CVT_BYTE,	&r_precache_sprites, 0, 1, "1=Precache sprites at level setup (slow).",
	"rend-sprite-align",	0,			CVT_INT,	&alwaysAlign,	0, 3,	"1=Always align sprites with the view plane.\n2=Align to camera, unless slant > r_maxSpriteAngle.",
	"rend-sprite-blend",	0,			CVT_INT,	&missileBlend,	0, 1,	"1=Use additive blending for explosions.",
	"rend-sprite-lit",		0,			CVT_INT,	&litSprites,	0, 1,	"1=Sprites lit using dynamic lights.",
	// * Render-Model
	"rend-model",		CVF_NO_MAX,		CVT_INT,	&useModels,		0, 1,	"Render using 3D models when possible.",
	"rend-model-lights",	0,			CVT_INT,	&modelLight,	0, 10,	"Maximum number of light sources on models.",
	"rend-model-inter",		0,			CVT_INT,	&frameInter,	0, 1,	"1=Interpolate frames.",
	"rend-model-aspect", CVF_NO_MAX|CVF_NO_MIN, CVT_FLOAT, &rModelAspectMod, 0, 0, "Scale for MD2 z-axis when model is loaded.",
	"rend-model-distance",	CVF_NO_MAX,	CVT_INT,	&r_maxmodelz,	0, 0,	"Farther than this models revert back to sprites.",
	"rend-model-precache",	0,			CVT_BYTE,	&r_precache_skins, 0, 1, "1=Precache 3D models at level setup (slow).",
	"rend-model-lod",		CVF_NO_MAX,	CVT_FLOAT,	&rend_model_lod, 0, 0,	"Custom level of detail factor. 0=LOD disabled, 1=normal.",
	"rend-model-mirror-hud", 0,			CVT_INT,	&mirrorHudModels, 0, 1,	"1=Mirror HUD weapon models.",
	"rend-model-spin-speed", CVF_NO_MAX|CVF_NO_MIN, CVT_FLOAT, &modelSpinSpeed, 0, 0, "Speed of model spinning, 1=normal.",
	"rend-model-shiny-multitex", 0,		CVT_INT,	&modelShinyMultitex, 0, 1, "1=Enable multitexturing with shiny model skins.",
	// * Render-HUD
	"rend-hud-offset-scale", CVF_NO_MAX, CVT_FLOAT, &weaponOffsetScale, 0, 0, "Scaling of player weapon (x,y) offset.",	
	"rend-hud-fov-shift",	CVF_NO_MAX,	CVT_FLOAT,	&weaponFOVShift,	0, 1, "When FOV > 90 player weapon is shifted downward.",
	// * Render-Mobj
	"rend-mobj-smooth-move", 0,			CVT_INT,	&r_use_srvo,	0, 2,	"1=Use short-range visual offsets for models.\n2=Use SRVO for sprites, too (unjags actor movement).",
	"rend-mobj-smooth-turn", 0,			CVT_INT,	&r_use_srvo_angle, 0, 1, "1=Use separate visual angle for mobjs (unjag actors).",
	// * Rend-Particle
	"rend-particle",		0,			CVT_INT,	&r_use_particles, 0, 1,	"1=Render particle effects.",
	"rend-particle-max",	CVF_NO_MAX,	CVT_INT,	&r_max_particles, 0, 0,	"Maximum number of particles to render. 0=no limit.",
	"rend-particle-rate",	0,			CVT_FLOAT,	&r_particle_spawn_rate, 0, 5, "Particle spawn rate multiplier (default: 1).",
	"rend-particle-diffuse", CVF_NO_MAX, CVT_FLOAT,	&rend_particle_diffuse, 0, 0, "Diffuse factor for particles near the camera.",
	"rend-particle-visible-near", CVF_NO_MAX, CVT_INT, &rend_particle_nearlimit, 0, 0, "Minimum visible distance for a particle.",
	// * Rend-Shadow
	"rend-shadow",			0,			CVT_INT,	&useShadows,	0, 1,	"1=Render shadows under objects.",
	"rend-shadow-darkness",	0,			CVT_FLOAT,	&shadowFactor,	0, 1,	"Darkness factor for object shadows.",
	"rend-shadow-far",		CVF_NO_MAX,	CVT_INT,	&shadowMaxDist,	0, 0,	"Maximum distance where shadows are visible.",
	"rend-shadow-radius-max", CVF_NO_MAX, CVT_INT,	&shadowMaxRad,	0, 0,	"Maximum radius of object shadows.",

	// Server
	"server-name",		0,				CVT_CHARPTR, &serverName,	0, 0,	"The name of this computer if it's a server.",
	"server-info",		0,				CVT_CHARPTR, &serverInfo,	0, 0,	"The description given of this computer if it's a server.",
	"server-public",	0,				CVT_INT,	&masterAware,	0, 1,	"1=Send info to master server.",

	// Client
//	"client-predict",	CVF_NO_MAX,		CVT_INT,	&predict_tics,	0, 0,	"Max tics to predict ahead.",

	// Network
	"net-name",				0,			CVT_CHARPTR, &playerName,	0, 0,	"Your name in multiplayer games.",
	"net-protocol",			0,			CVT_INT,	&nptActive,		0, 3,	"Network protocol: 0=TCP/IP, 1=IPX, 2=Modem, 3=Serial link.",
	"net-ip-address",		0,			CVT_CHARPTR, &nptIPAddress, 0, 0,	"TCP/IP address for searching servers.",
	"net-ip-port",			CVF_NO_MAX,	CVT_INT,	&nptIPPort,		0, 0,	"TCP/IP port to use for all data traffic on this computer.",
	"net-modem",			CVF_NO_MAX,	CVT_INT,	&nptModem,		0, 0,	"Index of the selected modem.",
	"net-modem-phone",		0,			CVT_CHARPTR, &nptPhoneNum,	0, 0,	"Phone number to dial to when connecting.",
	"net-serial-port",		CVF_NO_MAX, CVT_INT,	&nptSerialPort,	0, 0,	"COM port to use for serial link connection.",
	"net-serial-baud",		CVF_NO_MAX,	CVT_INT,	&nptSerialBaud,	0, 0,	"Baud rate for serial link connections.",
	"net-serial-stopbits",	0,			CVT_INT,	&nptSerialStopBits, 0, 2, "0=1 bit, 1=1.5 bits, 2=2 bits.",
	"net-serial-parity",	0,			CVT_INT,	&nptSerialParity, 0, 3,	"0=None, 1=odd, 2=even, 3=mark parity.",
	"net-serial-flowctrl",	0,			CVT_INT,	&nptSerialFlowCtrl, 0, 4, "0=None, 1=XON/XOFF, 2=RTS, 3=DTR, 4=RTS/DTR flow control.",
	"net-master-address",	0,			CVT_CHARPTR, &masterAddress, 0, 0,	"Master server IP address / name.",
	"net-master-port",		0,			CVT_INT,	&masterPort,	0, 65535, "Master server TCP/IP port.",
	"net-master-path",		0,			CVT_CHARPTR, &masterPath,	0, 0,	"Master server path name.",
//	"net-queue-packets",	0,			CVT_INT,	&maxQueuePackets, 0, 16, "Max packets in send queue.",

	// Sound
	"sound-volume",			0,			CVT_INT,	&sfx_volume,	0, 255, "Sound effects volume (0-255).",
	"sound-info",				0,			CVT_INT,	&sound_info,	0, 1,	"1=Show sound debug information.",
	"sound-rate",				0,			CVT_INT,	&sound_rate,	11025, 44100, "Sound effects sample rate (11025, 22050, 44100).",
	"sound-16bit",				0,			CVT_INT,	&sound_16bit,	0, 1,	"1=16-bit sound effects/resampling.",
	"sound-3d",					0,			CVT_INT,	&sound_3dmode,	0, 1,	"1=Play sound effects in 3D.",
	"sound-reverb-volume",		0,			CVT_FLOAT,	&sfx_reverb_strength, 0, 10, "Reverb effects general volume (0=disable).",

	// Music 
	"music-volume",			0,			CVT_INT,	&mus_volume,	0, 255, "Music volume (0-255).",
	"music-source",			0,			CVT_INT,	&mus_preference, 0, 2,	"Preferred music source: 0=Original MUS, 1=External files, 2=CD.",

	// Input
	// * Input-Key
	"input-key-wait1",		CVF_NO_MAX,		CVT_INT,	&repWait1,		6, 0,	"The number of 35 Hz ticks to wait before first key repeat.",
	"input-key-wait2",		CVF_NO_MAX,		CVT_INT,	&repWait2,		1, 0,	"The number of 35 Hz ticks to wait between key repeats.",
	"input-key-show-scancodes", 0,			CVT_BYTE,	&showScanCodes,	0, 1,	"1=Show scancodes of all pressed keys in the console.",
	// * Input-Joy
	//"input-joy-sensi",			0,			CVT_INT,	&joySensitivity,0, 9,	"Joystick sensitivity.",
	//"input-joy-deadzone",		0,			CVT_INT,	&joyDeadZone,	0, 90,	"Joystick dead zone, in percents.", 
	// * Input-Mouse
	//"input-mouse-wheel-sensi",	CVF_NO_MAX,	CVT_INT,	&mouseWheelSensi, 0, 0, "Mouse wheel sensitivity.",
	//"input-mouse-x-disable",	0,			CVT_INT,	&mouseDisableX,	0, 1,	"1=Disable mouse X axis.",
	//"input-mouse-y-disable",	0,			CVT_INT,	&mouseDisableY,	0, 1,	"1=Disable mouse Y axis.",
	//"input-mouse-y-inverse",	0,			CVT_INT,	&mouseInverseY, 0, 1,	"1=Inversed mouse Y axis.",
	//"input-mouse-filter",		0,			CVT_BYTE,	&mouseFilter, 0, 1,		"1=Filter mouse X and Y axes.",

	// File
	"file-startup",				0,			CVT_CHARPTR, &defaultWads,	0, 0,	"The list of WADs to be loaded at startup.",
	
	NULL
};

// Console commands. Names in LOWER CASE (yeah, that's cOnsISteNt).
ccmd_t engineCCmds[] =
{
//	"actions",		CCmdListActs,		"List all action commands.",
	"add",			CCmdAddSub,			"Add something to a cvar.",
	"after",		CCmdWait,			"Execute the specified command after a delay.",
	"alias",		CCmdAlias,			"Create aliases for a (set of) console commands.",
	"bgturn",		CCmdBackgroundTurn, "Set console background rotation speed.",
	"bind",			CCmdBind,			"Bind a console command to an event.",
	"bindaxis",		CCmdBindAxis,		"Bind a control to an input device axis.",
	"bindr",		CCmdBind,			"Bind a console command to an event (keys with repeat).",
	"chat",			CCmdChat,			"Broadcast a chat message.",
	"chatnum",		CCmdChat,			"Send a chat message to the specified player.",
	"chatto",		CCmdChat,			"Send a chat message to the specified player.",
	"clear",		CCmdConsole,		"Clear the console buffer.",
	"clearbinds",	CCmdClearBindings,	"Deletes all existing bindings.",
	"conlocp",		CCmdMakeCamera,		"Connect a local player.",
	"connect",		CCmdConnect,		"Connect to a server using TCP/IP.",
	"dec",			CCmdAddSub,			"Subtract 1 from a cvar.",
	"delbind",		CCmdDeleteBind,		"Deletes all bindings to the given console command.",
	"demolump",		CCmdDemoLump,		"Write a reference lump file for a demo.",
	"dir",			CCmdDir,			"Print contents of directories.",
	"dump",			CCmdDump,			"Dump a data lump currently loaded in memory.",
	"dumpkeymap",	CCmdDumpKeyMap,		"Write the current keymap to a file.",
	"echo",			CCmdEcho,			"Echo the parameters on separate lines.",
	"exec",			CCmdParse,			"Loads and executes a file containing console commands.",
	"flareconfig",	CCmdFlareConfig,	"Configure lens flares.",
	"fog",			CCmdFog,			"Modify fog settings.",
	"font",			CCmdFont,			"Modify console font settings.",
	"help",			CCmdConsole,		"Show information about the console.",
	"huffman",		CCmdHuffmanStats,	"Print Huffman efficiency and number of bytes sent.",
	"if",			CCmdIf,				"Execute a command if the condition is true.",
	"inc",			CCmdAddSub,			"Add 1 to a cvar.",
	"keymap",		CCmdKeyMap,			"Load a DKM keymap file.",
	"kick",			CCmdKick,			"Kick client out of the game.",
	"listaliases",	CCmdListAliases,	"List all aliases and their expanded forms.",
	"listbindings",	CCmdListBindings,	"List all event bindings.",
	"listcmds",		CCmdListCmds,		"List all console commands.",
	"listfiles",	CCmdListFiles,		"List all the loaded data files and show information about them.",
	"listvars",		CCmdListVars,		"List all console variables and their values.",
	"load",			CCmdLoadFile,		"Load a data file (a WAD or a lump).",
	"login",		CCmdLogin,			"Log in to server console.",
	"logout",		CCmdLogout,			"Terminate remote connection to server console.",
	"lowres",		CCmdLowRes,			"Select the poorest rendering quality.",
	"ls",			CCmdDir,			"Print contents of directories.",
	"mipmap",		CCmdMipMap,			"Set the mipmapping mode.",
	"net",			CCmdNet,			"Network setup and control.",
	"panel",		CCmdOpenPanel,		"Open the Doomsday Control Panel.",
	"pausedemo",	CCmdPauseDemo,		"Pause/resume demo recording.",
	"ping",			CCmdPing,			"Ping the server (or a player if you're the server).",
	"playdemo",		CCmdPlayDemo,		"Play a demo.",
	"playext",		CCmdPlayExt,		"Play an external music file.",
	"playmusic",	CCmdPlayMusic,		"Play a song, an external music file or a CD track.",
	"quit!",		CCmdQuit,			"Exit the game immediately.",
	"recorddemo",	CCmdRecordDemo,		"Start recording a demo.",
	"repeat",		CCmdRepeat,			"Repeat a command at given intervals.",
	"reset",		CCmdResetLumps,		"Reset the data files into what they were at startup.",
	"safebind",		CCmdBind,			"Bind a command to an event, unless the event is already bound.",
	"safebindr",	CCmdBind,			"Bind a command to an event, unless the event is already bound.",
	"say",			CCmdChat,			"Broadcast a chat message.",
	"saynum",		CCmdChat,			"Send a chat message to the specified player.",
	"sayto",		CCmdChat,			"Send a chat message to the specified player.",
	"setcon",		CCmdSetConsole,		"Set console and viewplayer.",
	"setgamma",		CCmdSetGamma,		"Set the gamma correction level.",
	"setname",		CCmdSetName,		"Set your name.",
	"setres",		CCmdSetRes,			"Change video mode resolution or window size.",
	"settics",		CCmdSetTicks,		"Set number of game tics per second (default: 35).",
	"setvidramp",	CCmdUpdateGammaRamp, "Update display's hardware gamma ramp.",
	"skydetail",	CCmdSkyDetail,		"Set the number of sky sphere quadrant subdivisions.",
	"skyrows",		CCmdSkyDetail,		"Set the number of sky sphere rows.",
	"smoothscr",	CCmdSmoothRaw,		"Set the rendering mode of fullscreen images.",
	"stopdemo",		CCmdStopDemo,		"Stop currently playing demo.",
	"stopmusic",	CCmdStopMusic,		"Stop any currently playing music.",
	"sub",			CCmdAddSub,			"Subtract something from a cvar.",
	"texreset",		CCmdResetTextures,	"Force a texture reload.",
	"toggle",		CCmdToggle,			"Toggle the value of a cvar between zero and nonzero.",
	"uicolor",		CCmdUIColor,		"Change Doomsday user interface colors.",
	"unload",		CCmdUnloadFile,		"Unload a data file from memory.",
	"version",		CCmdVersion,		"Show detailed version information.",
	"viewgrid",		CCmdViewGrid,		"Setup splitscreen configuration.",
	"write",		CCmdWriteConsole,	"Write variables, bindings and aliases to a file.",

#ifdef _DEBUG
	"TranslateFont", CCmdTranslateFont,	"Ha ha.",
#endif
	NULL
};

knownword_t *knownWords = 0;	// The list of known words (for completion).
int numKnownWords = 0;

char trimmedFloatBuffer[32];	// Returned by TrimmedFloat().

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean ConsoleInited;	// Has Con_Init() been called?
static boolean ConsoleActive;	// Is the console active?
static float ConsoleY;			// Where the console bottom is currently?
static float ConsoleDestY;		// Where the console bottom should be?
static timespan_t ConsoleTime;	// How many seconds has the console been open?
static float ConsoleBlink;		// Cursor blink timer (35 Hz tics).

static float funnyAng;
static boolean openingOrClosing = true;

static float fontFx, fontSy;	// Font x factor and y size.
static cbline_t *cbuffer;		// This is the buffer.
int bufferLines;				// How many lines are there in the buffer?
static int maxBufferLines;		// Maximum number of lines in the buffer.
static int maxLineLen;			// Maximum length of a line.
static int bPos;				// Where the write cursor is? (which line)
static int bFirst;				// The first visible line.
static int bLineOff;			// How many lines from bPos? (+vislines)
static char cmdLine[256];		// The command line.
static int cmdCursor;			// Position of the cursor on the command line.
static cbline_t *oldCmds;		// The old commands buffer.
static int numOldCmds;
static int ocPos;				// Old commands buffer position.
static int complPos;			// Where is the completion cursor?
static int lastCompletion;		// The index of the last completion (in knownWords).

static execbuff_t *exBuff;
static int exBuffSize;
static execbuff_t *curExec;

// CODE --------------------------------------------------------------------

void PrepareCmdArgs(cmdargs_t *cargs, const char *lpCmdLine)
{
	int		i, len = strlen(lpCmdLine);

	// Prepare the data.
	memset(cargs, 0, sizeof(cmdargs_t));
	strcpy(cargs->cmdLine, lpCmdLine);
	// Prepare.
	for(i=0; i<len; i++)
	{
#define IS_ESC_CHAR(x)	((x) == '"' || (x) == '\\' || (x) == '{' || (x) == '}')
		// Whitespaces are separators.
		if(ISSPACE(cargs->cmdLine[i])) cargs->cmdLine[i] = 0;
		if(cargs->cmdLine[i] == '\\'	
			&& IS_ESC_CHAR(cargs->cmdLine[i+1]))	// Escape sequence?
		{
			memmove(cargs->cmdLine+i, cargs->cmdLine+i+1, sizeof(cargs->cmdLine)-i-1);
			len--;
			continue;
		}
		if(cargs->cmdLine[i] == '"') // Find the end.
		{
			int start = i;
			cargs->cmdLine[i] = 0;
			for(++i; i<len && cargs->cmdLine[i] != '"'; i++)
			{
				if(cargs->cmdLine[i] == '\\' 
					&& IS_ESC_CHAR(cargs->cmdLine[i+1])) // Escape sequence?
				{
					memmove(cargs->cmdLine+i, cargs->cmdLine+i+1, sizeof(cargs->cmdLine)-i-1);
					len--;
					continue;
				}
			}
			// Quote not terminated?
			if(i == len) break;
			// An empty set of quotes?
			if(i == start+1) 
				cargs->cmdLine[i] = SC_EMPTY_QUOTE;
			else
				cargs->cmdLine[i] = 0;
		}
		if(cargs->cmdLine[i] == '{') // Find matching end.
		{
			// Braces are another notation for quotes.
			int level = 0;
			int start = i;
			cargs->cmdLine[i] = 0;
			for(++i; i < len; i++)
			{
				if(cargs->cmdLine[i] == '\\' 
					&& IS_ESC_CHAR(cargs->cmdLine[i+1])) // Escape sequence?
				{
					memmove(cargs->cmdLine+i, cargs->cmdLine+i+1, 
						sizeof(cargs->cmdLine)-i-1);
					len--;
					i++;
					continue;
				}
				if(cargs->cmdLine[i] == '}') 
				{
					if(!level) break;
					level--;
				}
				if(cargs->cmdLine[i] == '{') level++;
			}
			// Quote not terminated?
			if(i == len) break;
			// An empty set of braces?
			if(i == start + 1) 
				cargs->cmdLine[i] = SC_EMPTY_QUOTE;
			else
				cargs->cmdLine[i] = 0;
		}
	}
	// Scan through the cmdLine and get the beginning of each token.
	cargs->argc = 0;
	for(i = 0; i < len; i++)
	{
		if(!cargs->cmdLine[i]) continue;
		// Is this an empty quote?
		if(cargs->cmdLine[i] == SC_EMPTY_QUOTE)
			cargs->cmdLine[i] = 0;	// Just an empty string.
		cargs->argv[cargs->argc++] = cargs->cmdLine + i;
		i += strlen(cargs->cmdLine+i);
	}
}

char *TrimmedFloat(float val)
{
	char *ptr = trimmedFloatBuffer;

	sprintf(ptr, "%f", val);
	// Get rid of the extra zeros.
	for(ptr += strlen(ptr)-1; ptr >= trimmedFloatBuffer; ptr--)
	{
		if(*ptr == '0') *ptr = 0;
		else if(*ptr == '.') 
		{
			*ptr = 0;
			break;
		}
		else break;
	}
	return trimmedFloatBuffer;
}

//--------------------------------------------------------------------------
//
// Console Variable Handling
//
//--------------------------------------------------------------------------

//===========================================================================
// Con_SetString
//===========================================================================
void Con_SetString(const char *name, char *text)
{
	cvar_t *cvar = Con_GetVariable(name);

	if(!cvar) return;
	if(cvar->type == CVT_CHARPTR)
	{
		// Free the old string, if one exists.
		if(cvar->flags & CVF_CAN_FREE && CV_CHARPTR(cvar)) 
			free(CV_CHARPTR(cvar));
		// Allocate a new string.
		cvar->flags |= CVF_CAN_FREE;
		CV_CHARPTR(cvar) = malloc(strlen(text) + 1);
		strcpy(CV_CHARPTR(cvar), text);
	}
	else 
		Con_Error("Con_SetString: cvar is not of type char*.\n");
}

cvar_t *Con_GetVariable(const char *name)
{
	int		i;

	for(i = 0; i < numCVars; i++)
		if(!stricmp(name, cvars[i].name))
			return cvars+i;
	// No match...
	return NULL;
}

//===========================================================================
// Con_SetInteger
//	Also works with bytes.
//===========================================================================
void Con_SetInteger(const char *name, int value)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var) return;
	if(var->type == CVT_INT) CV_INT(var) = value;
	if(var->type == CVT_BYTE) CV_BYTE(var) = value;
	if(var->type == CVT_FLOAT) CV_FLOAT(var) = value;
}

//===========================================================================
// Con_SetFloat
//===========================================================================
void Con_SetFloat(const char *name, float value)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var) return;
	if(var->type == CVT_INT) CV_INT(var) = (int) value;
	if(var->type == CVT_BYTE) CV_BYTE(var) = (byte) value;
	if(var->type == CVT_FLOAT) CV_FLOAT(var) = value;
}

//===========================================================================
// Con_GetInteger
//===========================================================================
int Con_GetInteger(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var) return 0;
	if(var->type == CVT_BYTE) return CV_BYTE(var);
	if(var->type == CVT_FLOAT) return CV_FLOAT(var);
	if(var->type == CVT_CHARPTR) return strtol(CV_CHARPTR(var), 0, 0);
	return CV_INT(var); 
}

//===========================================================================
// Con_GetFloat
//===========================================================================
float Con_GetFloat(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var) return 0;
	if(var->type == CVT_INT) return CV_INT(var);
	if(var->type == CVT_BYTE) return CV_BYTE(var);
	if(var->type == CVT_CHARPTR) return strtod(CV_CHARPTR(var), 0);
	return CV_FLOAT(var);
}

//===========================================================================
// Con_GetByte
//===========================================================================
byte Con_GetByte(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var) return 0;
	if(var->type == CVT_INT) return CV_INT(var);
	if(var->type == CVT_FLOAT) return CV_FLOAT(var);
	if(var->type == CVT_CHARPTR) return strtol(CV_CHARPTR(var), 0, 0);
	return CV_BYTE(var); 
}

//===========================================================================
// Con_GetString
//===========================================================================
char *Con_GetString(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var || var->type != CVT_CHARPTR) return "";
	return CV_CHARPTR(var);
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

static int C_DECL wordListSorter(const void *e1, const void *e2)
{
	return stricmp(*(char**)e1, *(char**)e2);
}

static int C_DECL knownWordListSorter(const void *e1, const void *e2)
{
	return stricmp(( (knownword_t*) e1)->word, 
		( (knownword_t*) e2)->word);
}

//===========================================================================
// Con_Init
//===========================================================================
void Con_Init()
{
	ConsoleInited = true;

	ConsoleActive = false;
	ConsoleY = 0;
	ConsoleOpenY = 90;
	ConsoleDestY = 0;
	ConsoleTime = 0;

	funnyAng = 0;

	// Font size in VGA coordinates. (Everything is in VGA coords.)
	fontFx = 1;
	fontSy = 9;

	// The buffer.
	cbuffer = 0;
	bufferLines = 0;	
	maxBufferLines = 512; //256;
	maxLineLen = 70;		// Should fit the screen (adjusted later).

	cmdCursor = 0;

	// The old commands buffer.
	oldCmds = 0;
	numOldCmds = 0;
	ocPos = 0;				// No commands yet.

	bPos = 0;
	bFirst = 0;
	bLineOff = 0;

	complPos = 0;
	lastCompletion = -1;

	exBuff = NULL;
	exBuffSize = 0;

	// Register the engine commands and variables.
	Con_AddCommandList(engineCCmds);
	Con_AddVariableList(engineCVars);
	Con_AddVariableList(netCVars);
	Con_AddVariableList(inputCVars);
	H_Register();
}

//===========================================================================
// Con_MaxLineLength
//===========================================================================
void Con_MaxLineLength(void)
{
	int cw = FR_TextWidth("A"); 

	if(!cw)
	{
		maxLineLen = 70; 
		return;
	}
	maxLineLen = screenWidth/cw - 2;
	if(maxLineLen > 250) maxLineLen = 250;
}

//===========================================================================
// Con_UpdateKnownWords
//	Variables with CVF_HIDE are not considered known words.
//===========================================================================
void Con_UpdateKnownWords()
{
	int		i, c, known_vars;
	int		len;

	// Count the number of visible console variables.
	for(i = known_vars = 0; i < numCVars; i++)
		if(!(cvars[i].flags & CVF_HIDE)) known_vars++;

	// Fill the known words table.
	numKnownWords = numCCmds + known_vars + numCAliases;
	knownWords = realloc(knownWords, 
		len = sizeof(knownword_t) * numKnownWords);
	memset(knownWords, 0, len);

	// Commands, variables and aliases are known words.
	for(i = 0, c = 0; i < numCCmds; i++, c++)
	{
		strncpy(knownWords[c].word, ccmds[i].name, 63);
	}
	for(i = 0; i < numCVars; i++) 
	{
		if(!(cvars[i].flags & CVF_HIDE))
			strncpy(knownWords[c++].word, cvars[i].name, 63);
	}
	for(i = 0; i < numCAliases; i++, c++)
	{
		strncpy(knownWords[c].word, caliases[i].name, 63);
	}

	// Sort it so we get nice alphabetical word completions.
	qsort(knownWords, numKnownWords, sizeof(knownword_t), knownWordListSorter);
}

void Con_AddCommandList(ccmd_t *cmdlist)
{
	for(; cmdlist->name; cmdlist++) Con_AddCommand(cmdlist);
}

void Con_AddCommand(ccmd_t *cmd)
{
	numCCmds++;
	ccmds = realloc(ccmds, sizeof(ccmd_t) * numCCmds);
	memcpy(ccmds + numCCmds-1, cmd, sizeof(ccmd_t));

	// Sort them.
	qsort(ccmds, numCCmds, sizeof(ccmd_t), wordListSorter);

	// Update the list of known words.
	// This must be done right away because ccmds' address can change.
	//Con_UpdateKnownWords();
}

/*
 * Returns a pointer to the ccmd_t with the specified name, or NULL.
 */
ccmd_t *Con_GetCommand(const char *name)
{
	int i;

	for(i = 0; i < numCCmds; i++)
	{
		if(!stricmp(ccmds[i].name, name)) return &ccmds[i];		
	}
	return NULL;
}

/*
 * Returns true if the given string is a valid command or alias.
 */
boolean Con_IsValidCommand(const char *name)
{
	return Con_GetCommand(name) || Con_GetAlias(name);
}

void Con_AddVariableList(cvar_t *varlist)
{	
	for(; varlist->name; varlist++) Con_AddVariable(varlist);
}

void Con_AddVariable(cvar_t *var)
{
	numCVars++;
	cvars = realloc(cvars, sizeof(cvar_t) * numCVars);
	memcpy(cvars+numCVars-1, var, sizeof(cvar_t));

	// Sort them.
	qsort(cvars, numCVars, sizeof(cvar_t), wordListSorter);

	// Update the list of known words.
	// This must be done right away because ccmds' address can change.
	//Con_UpdateKnownWords();
}

// Returns NULL if the specified alias can't be found.
calias_t *Con_GetAlias(const char *name)
{
	int			i;

	// Try to find the alias.
	for(i=0; i<numCAliases; i++)
		if(!stricmp(caliases[i].name, name))
			return caliases+i;
	return NULL;
}

//===========================================================================
// Con_Alias
//	Create an alias.
//===========================================================================
void Con_Alias(char *aName, char *command)
{
	calias_t	*cal = Con_GetAlias(aName);
	boolean		remove = false;

	// Will we remove this alias?
	if(command == NULL) 
		remove = true;
	else if(command[0] == 0)
		remove = true;
	
	if(cal && remove)
	{
		// This alias will be removed.
		int idx = cal - caliases;
		free(cal->name);
		free(cal->command);
		if(idx < numCAliases-1)
			memmove(caliases+idx, caliases+idx+1, sizeof(calias_t)*(numCAliases-idx-1));
		caliases = realloc(caliases, sizeof(calias_t) * --numCAliases);
		// We're done.
		return;		
	}

	// Does the alias already exist?
	if(cal)
	{
		cal->command = realloc(cal->command, strlen(command)+1);
		strcpy(cal->command, command);
		return;
	}

	// We need to create a new alias.
	caliases = realloc(caliases, sizeof(calias_t) * (++numCAliases));
	cal = caliases + numCAliases-1;
	// Allocate memory for them.
	cal->name = malloc(strlen(aName)+1);
	cal->command = malloc(strlen(command)+1);
	strcpy(cal->name, aName);
	strcpy(cal->command, command);
//	cal->refcount = 0;

	// Sort them.
	qsort(caliases, numCAliases, sizeof(calias_t), wordListSorter);

	Con_UpdateKnownWords();
}

//===========================================================================
// Con_WriteAliasesToFile
//	Called by the config file writer.
//===========================================================================
void Con_WriteAliasesToFile(FILE *file)
{
	int i;
	calias_t *cal;

	for(i = 0, cal = caliases; i < numCAliases; i++, cal++)
	{
		fprintf(file, "alias \"");		
		M_WriteTextEsc(file, cal->name);
		fprintf(file, "\" \"");
		M_WriteTextEsc(file, cal->command);
		fprintf(file, "\"\n");
	}
}

void Con_ClearBuffer()
{
	int		i;

	// Free the buffer.
	for(i=0; i<bufferLines; i++) free(cbuffer[i].text);
	free(cbuffer);
	cbuffer = 0;
	bufferLines = 0;
	bPos = 0;
	bFirst = 0;
	bLineOff = 0;
}

// Send a console command to the server.
// This shouldn't be called unless we're logged in with the right password.
void Con_Send(const char *command, int silent)
{
	unsigned short len = strlen(command) + 1;

	Msg_Begin(pkt_command);
	// Mark high bit for silent commands.
	Msg_WriteShort(len | (silent? 0x8000 : 0));
	Msg_Write(command, len);
	// Send it reliably.
	Net_SendBuffer(0, SPF_ORDERED);
}

void Con_ClearExecBuffer()
{
	free(exBuff);
	exBuff = NULL;
	exBuffSize = 0;
}

void Con_QueueCmd(const char *singleCmd, timespan_t atSecond)
{
	execbuff_t *ptr = NULL;
	int	i;

	// Look for an empty spot.
	for(i = 0; i < exBuffSize; i++)
		if(!exBuff[i].used)
		{
			ptr = exBuff + i;
			break;
		}

	if(ptr == NULL)
	{
		// No empty places, allocate a new one.
		exBuff = realloc(exBuff, sizeof(execbuff_t) * ++exBuffSize);
		ptr = exBuff + exBuffSize - 1;
	}
	ptr->used = true;
	strcpy(ptr->subCmd, singleCmd);
	ptr->when = atSecond;
}

void Con_Shutdown()
{	
	int		i, k;
	char	*ptr;

	// Free the buffer.
	Con_ClearBuffer();

	// Free the old commands.
	for(i=0; i<numOldCmds; i++) free(oldCmds[i].text);
	free(oldCmds);
	oldCmds = 0;
	numOldCmds = 0;

	free(knownWords);
	knownWords = 0;
	numKnownWords = 0;

	// Free the data of the data cvars.
	for(i = 0; i < numCVars; i++)
		if(cvars[i].flags & CVF_CAN_FREE && cvars[i].type == CVT_CHARPTR)
		{
			ptr = *(char**) cvars[i].ptr;
			// Multiple vars could be using the same pointer,
			// make sure it gets freed only once.
			for(k = i; k < numCVars; k++)
				if(cvars[k].type == CVT_CHARPTR
					&& ptr == *(char**)cvars[k].ptr)
				{
					cvars[k].flags &= ~CVF_CAN_FREE;
				}
			free(ptr);
		}
	free(cvars);
	cvars = NULL;
	numCVars = 0;

	free(ccmds);
	ccmds = NULL;
	numCCmds = 0;

	// Free the alias data.
	for(i=0; i<numCAliases; i++)
	{
		free(caliases[i].command);
		free(caliases[i].name);
	}
	free(caliases);
	caliases = NULL;
	numCAliases = 0;

	Con_ClearExecBuffer();
}

/*
 * The execbuffer is used to schedule commands for later.
 * Returns false if an executed command fails.
 */
boolean Con_CheckExecBuffer(void)
{
	boolean allDone;
	boolean	ret = true;
	int		i, count = 0;
	char	storage[256];
	
	do // We'll keep checking until all is done.
	{
		allDone = true;

		// Execute the commands marked for this or a previous tic.
		for(i = 0; i < exBuffSize; i++)
		{	
			execbuff_t *ptr = exBuff + i;
			if(!ptr->used || ptr->when > sysTime) continue;
			// We'll now execute this command.
			curExec = ptr;
			ptr->used = false;
			strcpy(storage, ptr->subCmd);
			if(!executeSubCmd(storage)) ret = false;
			allDone = false;
		}

		if(count++ > 100)
		{
			Con_Message("Console execution buffer overflow! "
				"Everything canceled.\n");
			Con_ClearExecBuffer();
			break;
		}
	}
	while(!allDone);
	return ret;
}

void Con_Ticker(timespan_t time)
{
	float step = time * 35;

	Con_CheckExecBuffer();

	if(ConsoleY == 0) openingOrClosing = true;

	// Move the console to the destination Y.
	if(ConsoleDestY > ConsoleY)
	{
		float diff = (ConsoleDestY - ConsoleY)/4;
		if(diff < 1) diff = 1;
		ConsoleY += diff * step;		
		if(ConsoleY > ConsoleDestY) ConsoleY = ConsoleDestY;
	}
	else if(ConsoleDestY < ConsoleY)
	{
		float diff = (ConsoleY - ConsoleDestY)/4;
		if(diff < 1) diff = 1;
		ConsoleY -= diff * step;		
		if(ConsoleY < ConsoleDestY) ConsoleY = ConsoleDestY;
	}

	if(ConsoleY == ConsoleOpenY) openingOrClosing = false;

	funnyAng += step * consoleTurn / 10000;

	if(!ConsoleActive) return;	// We have nothing further to do here.

	ConsoleTime += time;		// Increase the ticker.
	ConsoleBlink += step;		// Cursor blink timer (0 = visible).
}

cbline_t *Con_GetBufferLine(int num)
{
	int i, newLines;

	if(num < 0) return 0;	// This is unacceptable!
	// See if we already have that line.
	if(num < bufferLines) return cbuffer + num;
	// Then we'll have to allocate more lines. Usually just one, though.
	newLines = num+1 - bufferLines;
	bufferLines += newLines;
	cbuffer = realloc(cbuffer, sizeof(cbline_t)*bufferLines);
	for(i=0; i<newLines; i++)
	{
		cbline_t *line = cbuffer + i+bufferLines-newLines;
		memset(line, 0, sizeof(cbline_t));
	}
	return cbuffer + num;
}

static void addLineText(cbline_t *line, char *txt)
{
	int newLen = line->len + strlen(txt);

	if(newLen > maxLineLen)
		//Con_Error("addLineText: Too long console line.\n");
		return;	// Can't do anything.

	// Allocate more memory.
	line->text = realloc(line->text, newLen+1);
	// Copy the new text to the appropriate location.
	strcpy(line->text+line->len, txt);
	// Update the length of the line.
	line->len = newLen;
}

/*
static void setLineFlags(int num, int fl)
{
	cbline_t *line = Con_GetBufferLine(num);

	if(!line) return;
	line->flags = fl;
}
*/

static void addOldCmd(const char *txt)
{
	cbline_t *line;

	if(!strcmp(txt, "")) return; // Don't add empty commands.

	// Allocate more memory.
	oldCmds = realloc(oldCmds, sizeof(cbline_t) * ++numOldCmds);
	line = oldCmds + numOldCmds-1;
	// Clear the line.
	memset(line, 0, sizeof(cbline_t));
	line->len = strlen(txt);
	line->text = malloc(line->len+1); // Also room for the zero in the end.
	strcpy(line->text, txt);
}

static void printcvar(cvar_t *var, char *prefix)
{
	char equals = '=';
	
	if(var->flags & CVF_PROTECTED) equals = ':';

	Con_Printf(prefix);
	switch(var->type)
	{
	case CVT_NULL:
		Con_Printf("%s", var->name);
		break;
	case CVT_BYTE:
		Con_Printf("%s %c %d", var->name, equals, CV_BYTE(var));
		break;
	case CVT_INT:
		Con_Printf("%s %c %d", var->name, equals, CV_INT(var));
		break;
	case CVT_FLOAT:
		Con_Printf("%s %c %g", var->name, equals, CV_FLOAT(var));
		break;
	case CVT_CHARPTR:
		Con_Printf("%s %c %s", var->name, equals, CV_CHARPTR(var));
		break;
	default:
		Con_Printf("%s (bad type!)", var->name);
		break;
	}
	Con_Printf("\n");
}

/*
 * expandWithArguments
 *	expCommand gets reallocated in the expansion process.
 *	This could be bit more clever.
 */
static void expandWithArguments(char **expCommand, cmdargs_t *args)
{
	char *text = *expCommand;
	int size = strlen(text)+1;
	int	i, p, off;

	for(i = 0; text[i]; i++)
	{
		if(text[i] == '%' && (text[i+1] >= '1' && text[i+1] <= '9'))
		{
			char *substr;
			int aidx = text[i+1]-'1'+1;
			
			// Expand! (or delete)
			if(aidx > args->argc-1)
				substr = "";
			else
				substr = args->argv[aidx];

			// First get rid of the %n.
			memmove(text+i, text+i+2, size-i-2);
			// Reallocate.
			off = strlen(substr);
			text = *expCommand = realloc(*expCommand, size+=off-2);
			if(off)
			{
				// Make room for the insert.
				memmove(text+i+off, text+i, size-i-off);												
				memcpy(text+i, substr, off);
			}
			i += off-1;
		}
		else if(text[i] == '%' && text[i+1] == '0')
		{
			// First get rid of the %0.
			memmove(text+i, text+i+2, size-i-2);
			text = *expCommand = realloc(*expCommand, size-=2);
			for(p=args->argc-1; p>0; p--)
			{
				off = strlen(args->argv[p])+1;
				text = *expCommand = realloc(*expCommand, size+=off);
				memmove(text+i+off, text+i, size-i-off);
				text[i] = ' ';
				memcpy(text+i+1, args->argv[p], off-1);
			}
		}
	}
}

/*
 * The command is executed forthwith!!
 */
static int executeSubCmd(const char *subCmd)
{
	int			i;
//	char		prefix;
	cmdargs_t	args;

	PrepareCmdArgs(&args, subCmd);
	if(!args.argc) return true;

	if(args.argc == 1)	// Possibly a control command?
	{
/*		prefix = args.argv[0][0];
		if(prefix == '+' || prefix == '-')
		{
			return Con_ActionCommand(args.argv[0], true);
		}
		// What about a prefix-less action?
		if(strlen(args.argv[0]) <= 8 
			&& Con_ActionCommand(args.argv[0], false)) 
			return true; // There was one!*/
		
		if(P_ControlExecute(args.argv[0]))
		{
			// It was a control command.  No further processing is
			// necessary.
			return true;
		}
	}

	// If logged in, send command to server at this point.
	if(netLoggedIn)
	{
		// We have logged in on the server. Send the command there.
		Con_Send(subCmd, ConsoleSilent);		
		return true;
	}

	// Try to find a matching command.
	for(i = 0; i < numCCmds; i++)
		if(!stricmp(ccmds[i].name, args.argv[0]))
		{
			int cret = ccmds[i].func(args.argc, args.argv);
			if(cret == false)
				Con_Printf( "Error: '%s' failed.\n", ccmds[i].name);
			// We're quite done.
			return cret;
		}
	// Then try the cvars?
	for(i = 0; i < numCVars; i++)
		if(!stricmp(cvars[i].name, args.argv[0]))
		{
			cvar_t *var = cvars + i;
			boolean out_of_range = false, setting = false;
			if(args.argc == 2 || (args.argc == 3 && !stricmp(args.argv[1], "force")))
			{
				char *argptr = args.argv[ args.argc-1 ];
				boolean forced = args.argc==3;
				setting = true;
				if(var->flags & CVF_PROTECTED && !forced)
				{
					Con_Printf("%s is protected. You shouldn't change its value.\n", var->name);
					Con_Printf("Use the command: '%s force %s' to modify it anyway.\n",
						var->name, argptr);
				}
				else if(var->type == CVT_BYTE)
				{
					byte val = (byte) strtol(argptr, NULL, 0);
					if(!forced && ((!(var->flags & CVF_NO_MIN) && val < var->min) ||
						(!(var->flags & CVF_NO_MAX) && val > var->max)))
						out_of_range = true;
					else
						CV_BYTE(var) = val;
				}
				else if(var->type == CVT_INT)
				{
					int val = strtol(argptr, NULL, 0);
					if(!forced && ((!(var->flags & CVF_NO_MIN) && val < var->min) ||
						(!(var->flags & CVF_NO_MAX) && val > var->max)))
						out_of_range = true;
					else
						CV_INT(var) = val;
				}
				else if(var->type == CVT_FLOAT)
				{
					float val = strtod(argptr, NULL);
					if(!forced && ((!(var->flags & CVF_NO_MIN) && val < var->min) ||
						(!(var->flags & CVF_NO_MAX) && val > var->max)))
						out_of_range = true;
					else
						CV_FLOAT(var) = val;
				}
				else if(var->type == CVT_CHARPTR)
				{
					Con_SetString(var->name, argptr);
				}
			}
			if(out_of_range)
			{
				if(!(var->flags & (CVF_NO_MIN|CVF_NO_MAX)))
				{
					char temp[20];
					strcpy(temp, TrimmedFloat(var->min));
					Con_Printf( "Error: %s <= %s <= %s\n", 
						temp, var->name, TrimmedFloat(var->max));
				}
				else if(var->flags & CVF_NO_MAX)
				{
					Con_Printf( "Error: %s >= %s\n", var->name, 
						TrimmedFloat(var->min));
				}
				else 
				{
					Con_Printf( "Error: %s <= %s\n", var->name, 
						TrimmedFloat(var->max));
				}
			}
			else if(!setting || !conSilentCVars)	// Show the value.
			{
				printcvar(var, "");
			}
			return true;
		}

	// How about an alias then?
	for(i = 0; i < numCAliases; i++)
		if(!stricmp(args.argv[0], caliases[i].name))
		{
			calias_t *cal = caliases + i;
			char *expCommand;
			// Expand the command with arguments.
			expCommand = malloc(strlen(cal->command)+1);
			strcpy(expCommand, cal->command);
			expandWithArguments(&expCommand, &args);
			// Do it, man!
			Con_SplitIntoSubCommands(expCommand, 0);
			free(expCommand);
			return true;
		}

	// What *is* that?
	Con_Printf("%s: no such command or variable.\n", args.argv[0]);
	return false;
}

/*
 * Splits the command into subcommands and queues them into the 
 * execution buffer.
 */
void Con_SplitIntoSubCommands(const char *command, timespan_t markerOffset)
{
	int	gPos = 0, scPos = 0;
	char subCmd[2048];
	int	inQuotes = false, escape = false;

	// Is there a command to execute?
	if(!command || command[0] == 0) return;

	// Jump over initial semicolons.
	while(command[gPos] == ';' && command[gPos] != 0) gPos++;

	// The command may actually contain many commands, separated
	// with semicolons. This isn't a very clear algorithm...
	for(strcpy(subCmd, ""); command[gPos];)
	{
		escape = false;
		if(inQuotes && command[gPos] == '\\') // Escape sequence?
		{
			subCmd[scPos++] = command[gPos++];
			escape = true;
		}
		if(command[gPos] == '"' && !escape) 
			inQuotes = !inQuotes;

		// Collect characters.
		subCmd[scPos++] = command[gPos++];
		if(subCmd[0] == ' ') scPos = 0;	// No spaces in the beginning.

		if((command[gPos] == ';' && !inQuotes) || command[gPos] == 0)
		{
			while(command[gPos] == ';' && command[gPos] != 0) gPos++;
			// The subcommand ends.
			subCmd[scPos] = 0;
		}
		else continue;
	
		// Queue it.
		Con_QueueCmd(subCmd, sysTime + markerOffset); 
		
		scPos = 0;
	}
}


//===========================================================================
// Con_Execute
//	Returns false if a command fails.
//===========================================================================
int Con_Execute(const char *command, int silent)
{
	int ret;

	if(silent) ConsoleSilent = true;
	
	Con_SplitIntoSubCommands(command, 0);
	ret = Con_CheckExecBuffer();

	if(silent) ConsoleSilent = false;
	return ret;
}

//===========================================================================
// Con_Executef
//===========================================================================
int Con_Executef(int silent, const char *command, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, command);
	vsprintf(buffer, command, argptr);
	va_end(argptr);
	return Con_Execute(buffer, silent);
}

static void processCmd()
{
	DD_ClearKeyRepeaters();

	// Add the command line to the oldCmds buffer.
	addOldCmd(cmdLine);
	ocPos = numOldCmds;

	Con_Execute(cmdLine, false);
}

static void updateCmdLine()
{
	if(ocPos == numOldCmds)
		strcpy(cmdLine, "");
	else
		strcpy(cmdLine, oldCmds[ocPos].text);
	cmdCursor = complPos = strlen(cmdLine);
	lastCompletion = -1;
	if(isDedicated) Sys_ConUpdateCmdLine(cmdLine);
}

//===========================================================================
// stramb
//	Ambiguous string check. 'amb' is cut at the first character that 
//	differs when compared to 'str' (case ignored).
//===========================================================================
void stramb(char *amb, const char *str)
{
	while(*str 
		&& tolower( (unsigned) *amb) == tolower( (unsigned) *str)) 
	{
		amb++;
		str++;
	}
	*amb = 0;
}

//===========================================================================
// completeWord
//	Look at the last word and try to complete it. If there are
//	several possibilities, print them.
//===========================================================================
static void completeWord()
{
	int		pass, i, c, cp = strlen(cmdLine)-1;
	int		numcomp = 0;
	char	word[100], *wordBegin;
	char	unambiguous[256];
	char	*completion=0;	// Pointer to the completed word.
	cvar_t	*cvar;

	if(conCompMode == 1) cp = complPos-1;
	memset(unambiguous, 0, sizeof(unambiguous));

	if(cp < 0) return;

	// Skip over any whitespace behind the cursor.
	while(cp > 0 && cmdLine[cp] == ' ') cp--;
	
	// Rewind the word pointer until space or a semicolon is found.
	while(cp > 0 
		&& cmdLine[cp-1] != ' ' 
		&& cmdLine[cp-1] != ';' 
		&& cmdLine[cp-1] != '"') cp--;

	// Now cp is at the beginning of the word that needs completing.
	strcpy(word, wordBegin = cmdLine + cp);

	if(conCompMode == 1)
		word[complPos - cp] = 0;	// Only check a partial word.

	// The completions we know are the cvars and ccmds.
	for(pass = 1; pass <= 2; pass++)
	{
		if(pass == 2)	// Print the possible completions.
			Con_Printf("Completions:\n");

		// Look through the known words.
		for(c=0, i = (conCompMode==0? 0 : (lastCompletion+1)); 
			c < numKnownWords; c++, i++)
		{
			if(i > numKnownWords-1) i = 0;
			if(!strnicmp(knownWords[i].word, word, strlen(word)))
			{
				// This matches!
				if(!unambiguous[0])
					strcpy(unambiguous, knownWords[i].word);
				else
					stramb(unambiguous, knownWords[i].word);
				// Pass #1: count completions, update completion.
				if(pass == 1) 
				{
					numcomp++;
					completion = knownWords[i].word;
					if(conCompMode == 1)
					{
						lastCompletion = i;
						break;
					}
				}
				else // Pass #2: print it.
				{
					// Print the value of all cvars.
					if((cvar = Con_GetVariable(knownWords[i].word)) != NULL)
						printcvar(cvar, "  ");
					else
						Con_Printf("  %s\n", knownWords[i].word);
				}
			}
		}
		if(numcomp <= 1 || conCompMode == 1) break;				
	}

	// Was a single completion found?
	if(numcomp == 1)
	{
		strcpy(wordBegin, completion);
		strcat(wordBegin, " ");
		cmdCursor = strlen(cmdLine);
	}
	else if(numcomp > 1)
	{
		// More than one completion; only complete the unambiguous part.
		strcpy(wordBegin, unambiguous);
		cmdCursor = strlen(cmdLine);
	}
}

/*
 * Con_Responder
 *	Returns true if the event is eaten.
 */
boolean Con_Responder(event_t *event)
{
	byte ch;

	if(consoleShowKeys && event->type == ev_keydown)
	{
		Con_Printf("Keydown: ASCII %i (0x%x)\n", event->data1, event->data1);
	}

	// Special console key: Shift-Escape opens the Control Panel.
	if(shiftDown 
		&& event->type == ev_keydown 
		&& event->data1 == DDKEY_ESCAPE)
	{
		Con_Execute("panel", true);
		return true;
	}

	if(!ConsoleActive)
	{
		// In this case we are only interested in the activation key.
		if(event->type == ev_keydown && event->data1 == consoleActiveKey/* && !MenuActive*/)
		{
			Con_Open(true);
			return true;
		}
		return false;
	}

	// All keyups are eaten by the console.
	if(event->type == ev_keyup) return true; 

	// We only want keydown events.
	if(event->type != ev_keydown && event->type != ev_keyrepeat) 
		return false;

	// In this case the console is active and operational.
	// Check the shutdown key.
	if(event->data1 == consoleActiveKey)
	{
		if(shiftDown) // Shift-Tilde to fullscreen and halfscreen.
			ConsoleDestY = ConsoleOpenY = (ConsoleDestY == 200? 100 : 200);
		else
			Con_Open(false);
		return true;
	}

	// Hitting Escape in the console closes it.
	if(event->data1 == DDKEY_ESCAPE)
	{
		Con_Open(false);
		return false;	// Let the menu know about this.
	}

	switch(event->data1)
	{
	case DDKEY_UPARROW:
		if(--ocPos < 0) ocPos = 0;
		// Update the command line.
		updateCmdLine();
		return true;
	
	case DDKEY_DOWNARROW:
		if(++ocPos > numOldCmds) ocPos = numOldCmds;
		updateCmdLine();
		return true;
	
	case DDKEY_PGUP:
		bLineOff += 2;
		if(bLineOff > bPos-1) bLineOff = bPos-1;
		return true;
	
	case DDKEY_PGDN:
		bLineOff -= 2;
		if(bLineOff < 0) bLineOff = 0;
		return true;
	
	case DDKEY_INS:
		ConsoleOpenY -= fontSy * (shiftDown? 3 : 1);
		if(ConsoleOpenY < fontSy) ConsoleOpenY = fontSy;
		ConsoleDestY = ConsoleOpenY;
		return true;
	
	case DDKEY_DEL:
		ConsoleOpenY += fontSy * (shiftDown? 3 : 1);
		if(ConsoleOpenY > 200) ConsoleOpenY = 200;
		ConsoleDestY = ConsoleOpenY;
		return true;
	
	case DDKEY_END:
		bLineOff = 0;
		return true;
	
	case DDKEY_HOME:
		bLineOff = bPos-1;
		return true;

	case DDKEY_ENTER:
		// Print the command line with yellow text.
		Con_FPrintf(CBLF_YELLOW, ">%s\n", cmdLine);
		// Process the command line.
		processCmd();		
		// Clear it.
		memset(cmdLine, 0, sizeof(cmdLine));
		cmdCursor = 0;
		complPos = 0;
		lastCompletion = -1;
		ConsoleBlink = 0;
		if(isDedicated) Sys_ConUpdateCmdLine(cmdLine);
		return true;

	case DDKEY_BACKSPACE:
		if(cmdCursor > 0)
		{
			memmove(cmdLine + cmdCursor - 1, cmdLine + cmdCursor, 
				sizeof(cmdLine) - cmdCursor);
			--cmdCursor;
			complPos = cmdCursor;
			lastCompletion = -1;
			ConsoleBlink = 0;
			if(isDedicated) Sys_ConUpdateCmdLine(cmdLine);
		}
		return true;

	case DDKEY_TAB:
		completeWord();
		ConsoleBlink = 0;
		if(isDedicated) Sys_ConUpdateCmdLine(cmdLine);
		return true;

	case DDKEY_LEFTARROW:
		if(cmdCursor > 0) --cmdCursor;
		complPos = cmdCursor;
		ConsoleBlink = 0;
		break;

	case DDKEY_RIGHTARROW:
		if(cmdLine[cmdCursor] != 0 && cmdCursor < maxLineLen) 
			++cmdCursor;
		complPos = cmdCursor;
		ConsoleBlink = 0;
		break;

	default:	// Check for a character.
		ch = event->data1;
		if(ch < 32 || (ch > 127 && ch < DD_HIGHEST_KEYCODE)) return true;
		ch = DD_ModKey(ch);

		if(cmdCursor < maxLineLen)
		{
			// Push the rest of the stuff forwards.
			memmove(cmdLine + cmdCursor + 1, cmdLine + cmdCursor,
				sizeof(cmdLine) - cmdCursor - 1);

			// The last char is always zero, though.
			cmdLine[sizeof(cmdLine) - 1] = 0;
		}
		cmdLine[cmdCursor] = ch;
		if(cmdCursor < maxLineLen) ++cmdCursor;
		complPos = cmdCursor; //strlen(cmdLine);
		lastCompletion = -1;
		ConsoleBlink = 0;

		if(isDedicated) Sys_ConUpdateCmdLine(cmdLine);
		return true;
	}
	// The console is very hungry for keys...
	return true;
}

static void consoleSetColor(int fl, float alpha)
{
	float	r=0, g=0, b=0;
	int		count=0;

	// Calculate the average of the given colors.
	if(fl & CBLF_BLACK)
	{
		count++;
	}
	if(fl & CBLF_BLUE)
	{
		b += 1;
		count++;
	}
	if(fl & CBLF_GREEN)
	{
		g += 1;
		count++;
	}
	if(fl & CBLF_CYAN)
	{
		g += 1;
		b += 1;
		count++;
	}
	if(fl & CBLF_RED)
	{
		r += 1;
		count++;
	}
	if(fl & CBLF_MAGENTA)
	{
		r += 1;
		b += 1;
		count++;
	}
	if(fl & CBLF_YELLOW)
	{
		r += CcolYellow[0];
		g += CcolYellow[1];
		b += CcolYellow[2];
		count++;
	}
	if(fl & CBLF_WHITE)
	{
		r += 1;
		g += 1;
		b += 1;
		count++;
	}
	// Calculate the average.
	if(count)
	{
		r /= count;
		g /= count;
		b /= count;
	}
	if(fl & CBLF_LIGHT)
	{
		r += (1-r)/2;
		g += (1-g)/2;
		b += (1-b)/2;
	}
	gl.Color4f(r, g, b, alpha);
}

void Con_SetFont(ddfont_t *cfont)
{
	Cfont = *cfont;
}

//===========================================================================
// Con_DrawRuler2
//===========================================================================
void Con_DrawRuler2(int y, int lineHeight, float alpha, int scrWidth)
{
	int xoff = 5;
	int rh = 6;

	UI_GradientEx(xoff, y + (lineHeight - rh)/2 + 1,
		scrWidth - 2*xoff, rh, rh/2, UI_COL(UIC_SHADOW),
		UI_COL(UIC_BG_DARK), alpha/3, alpha);
	UI_DrawRectEx(xoff, y + (lineHeight - rh)/2 + 1, 
		scrWidth - 2*xoff, rh, rh/2, false,
		UI_COL(UIC_TEXT), NULL,  /*UI_COL(UIC_BG_DARK), UI_COL(UIC_BG_LIGHT),*/
		alpha, -1);
}

//===========================================================================
// Con_DrawRuler
//===========================================================================
void Con_DrawRuler(int y, int lineHeight, float alpha)
{
	Con_DrawRuler2(y, lineHeight, alpha, screenWidth);
}

/*
 * Draw a 'side' text in the console. This is intended for extra
 * information about the current game mode.
 */
void Con_DrawSideText(const char *text, int line, float alpha)
{
	char buf[300];
	float gtosMulY = screenHeight/200.0f;
	float fontScaledY = Cfont.height * Cfont.sizeY;
	float y = ConsoleY*gtosMulY - fontScaledY*(1 + line);
	int ssw;

	if(y > -fontScaledY)
	{
		// The side text is a bit transparent.
		alpha *= .6f;

		// scaled screen width		
		ssw = (int) (screenWidth/Cfont.sizeX);	 
		
		// Print the version.
		strncpy(buf, text, sizeof(buf));
		if(Cfont.Filter) Cfont.Filter(buf);

		if(consoleShadowText)
		{
			// Draw a shadow.
			gl.Color4f(0, 0, 0, alpha);
			Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 2, y/Cfont.sizeY + 1);
		}
		gl.Color4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], alpha);
		Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 3, y/Cfont.sizeY);
	}
}

//===========================================================================
// Con_Drawer
//	Slightly messy...
//===========================================================================
void Con_Drawer(void)
{
	int		i, k;	// Line count and buffer cursor.
	float	x, y;
	float	closeFade = 1;
	float	gtosMulY = screenHeight/200.0f;
	char	buff[256], temp[256];
	float	fontScaledY;
	int		bgX = 64, bgY = 64;

	if(ConsoleY == 0) return;	// We have nothing to do here.

	// Do we have a font?
	if(Cfont.TextOut == NULL)
	{
		Cfont.flags   = DDFONT_WHITE;
		Cfont.height  = FR_TextHeight("Con");
		Cfont.sizeX   = 1;
		Cfont.sizeY   = 1;
		Cfont.TextOut = FR_TextOut;
		Cfont.Width   = FR_TextWidth;
		Cfont.Filter  = NULL;
	}

	fontScaledY = Cfont.height * Cfont.sizeY;
	fontSy = fontScaledY/gtosMulY;

	// Go into screen projection mode.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	BorderNeedRefresh = true;

	if(openingOrClosing)
	{
		closeFade = ConsoleY / (float) ConsoleOpenY;
	}

	// The console is composed of two parts: the main area background and the 
	// border.
	gl.Color4f(consoleLight/100.0f, consoleLight/100.0f, 
		consoleLight/100.0f, closeFade * consoleAlpha/100);

	// The background.
	if(gx.ConsoleBackground) gx.ConsoleBackground(&bgX, &bgY);

	// Let's make it a bit more interesting.
	gl.MatrixMode(DGL_TEXTURE);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Translatef(2*sin(funnyAng/4), 2*cos(funnyAng/4), 0);
	gl.Rotatef(funnyAng*3, 0, 0, 1);
	GL_DrawRectTiled(0, (int)ConsoleY*gtosMulY+4, 
		screenWidth, -screenHeight-4, bgX, bgY);
	gl.PopMatrix();

	// The border.
	GL_DrawRect(0, (int)ConsoleY*gtosMulY+3, screenWidth, 2,
		0,0,0, closeFade);

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Scalef(Cfont.sizeX, Cfont.sizeY, 1);

	// The game & version number.
	Con_DrawSideText(gx.Get(DD_GAME_ID), 2, closeFade);
	Con_DrawSideText(gx.Get(DD_GAME_MODE), 1, closeFade);

	gl.Color4f(1, 1, 1, closeFade);

	// The text in the console buffer will be drawn from the bottom up (!).
	for(i = bPos-bLineOff-1, y = ConsoleY*gtosMulY-fontScaledY*2; 
		i >= 0 && i < bufferLines && y > -fontScaledY; i--)
	{
		cbline_t *line = cbuffer + i;
		if(line->flags & CBLF_RULER)
		{
			// Draw a ruler here, and nothing else.
			Con_DrawRuler2(y/Cfont.sizeY, Cfont.height, closeFade,
				screenWidth/Cfont.sizeX);
		}
		else
		{
			memset(buff, 0, sizeof(buff));
			strncpy(buff, line->text, 255);

			x = line->flags & CBLF_CENTER? (screenWidth/Cfont.sizeX 
				- Cfont.Width(buff))/2 : 2;

			if(Cfont.Filter) 
				Cfont.Filter(buff);
			else if(consoleShadowText)
			{
				// Draw a shadow.
				gl.Color3f(0, 0, 0);
				Cfont.TextOut(buff, x+2, y/Cfont.sizeY+2);
			}
			
			// Set the color.
			if(Cfont.flags & DDFONT_WHITE) // Can it be colored?
				consoleSetColor(line->flags, closeFade);
			Cfont.TextOut(buff, 
				x, y/Cfont.sizeY);
		}		
		// Move up.
		y -= fontScaledY;
	}
	
	// The command line.
	strcpy(buff, ">");
	strcat(buff, cmdLine);

	if(Cfont.Filter) Cfont.Filter(buff);
	if(consoleShadowText)
	{
		// Draw a shadow.
		gl.Color3f(0, 0, 0);
		Cfont.TextOut(buff, 4, 2+(ConsoleY*gtosMulY-fontScaledY)/Cfont.sizeY);
	}
	if(Cfont.flags & DDFONT_WHITE)
		gl.Color4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], closeFade);
	else
		gl.Color4f(1, 1, 1, closeFade);
	Cfont.TextOut(buff, 2, (ConsoleY*gtosMulY-fontScaledY)/Cfont.sizeY);

	// Width of the current char.
	temp[0] = cmdLine[cmdCursor];
	temp[1] = 0;
	k = Cfont.Width(temp);
	if(!k) k = Cfont.Width(" ");

	// What is the width?
	memset(temp, 0, sizeof(temp));
	strncpy(temp, buff, MIN_OF(250, cmdCursor) + 1);
	i = Cfont.Width(temp);

	// Draw the cursor in the appropriate place.
	gl.Disable(DGL_TEXTURING);
	GL_DrawRect(2 + i, 
		(ConsoleY*gtosMulY-fontScaledY)/Cfont.sizeY,
		k, Cfont.height,
		CcolYellow[0], CcolYellow[1], CcolYellow[2],
		closeFade * ( ((int)ConsoleBlink) & 0x10? .2f : .5f ));
	gl.Enable(DGL_TEXTURING);

	// Restore the original matrices.
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
}

//===========================================================================
// Con_AddRuler
//	A ruler line will be added into the console. bPos is moved down by 1.
//===========================================================================
void Con_AddRuler(void)
{
	cbline_t *line = Con_GetBufferLine(bPos++);
	int i;

	line->flags |= CBLF_RULER;
	if(consoleDump) 
	{
		// A 70 characters long line.
		for(i = 0; i < 7; i++)
		{
			fprintf(outFile, "----------");
			if(isDedicated) Sys_ConPrint(0, "----------");
		}
		fprintf(outFile, "\n");
		if(isDedicated) Sys_ConPrint(0, "\n");
	}
}

void conPrintf(int flags, const char *format, va_list args)
{
	unsigned int i;
	int			lbc; // line buffer cursor
	char		*prbuff, *lbuf = malloc(maxLineLen+1);
	cbline_t	*line;

	if(flags & CBLF_RULER)
	{
		Con_AddRuler();
		flags &= ~CBLF_RULER;
	}

	// Allocate a print buffer that will surely be enough (64Kb).
	// FIXME: No need to allocate on EVERY printf call!
	prbuff = malloc(65536);

	// Format the message to prbuff.
	vsprintf(prbuff, format, args);

	if(consoleDump) fprintf(outFile, "%s", prbuff);
	if(SW_IsActive()) SW_Printf(prbuff);

	// Servers might have to send the text to a number of clients.
	if(isServer)
	{
		if(flags & CBLF_TRANSMIT)	
			Sv_SendText(NSP_BROADCAST, flags, prbuff);
		else if(net_remoteuser) // Is somebody logged in?
			Sv_SendText(net_remoteuser, flags | SV_CONSOLE_FLAGS, prbuff);
	}

	if(isDedicated)
	{
		Sys_ConPrint(flags, prbuff);
		free(lbuf);
		free(prbuff);
		return;
	}

	// We have the text we want to add in the buffer in prbuff.
	line = Con_GetBufferLine(bPos);	// Get a pointer to the current line.
	line->flags = flags;
	memset(lbuf, 0, maxLineLen+1);	
	for(i=0, lbc=0; i<strlen(prbuff); i++)
	{
		if(prbuff[i] == '\n' || lbc+line->len >= maxLineLen)	// A new line?
		{
			// Set the line text.
			addLineText(line, lbuf);
			// Clear the line write buffer.
			memset(lbuf, 0, maxLineLen+1);
			lbc = 0;
			// Get the next line.
			line = Con_GetBufferLine(++bPos);
			line->flags = flags;
			// Newlines won't get in the buffer at all.
			if(prbuff[i] == '\n') continue;
		}
		lbuf[lbc++] = prbuff[i];
	}
	// Something still in the write buffer?
	if(lbc) addLineText(line, lbuf);

	// Clean up.
	free(lbuf);
	free(prbuff);

	// Now that something new has been printed, it will be shown.
	bLineOff = 0;

	// Check if there are too many lines.
	if(bufferLines > maxBufferLines)
	{
		int rev = bufferLines - maxBufferLines;
		// The first 'rev' lines get removed.
		for(i = 0; (int) i < rev; i++) free(cbuffer[i].text);
		memmove(cbuffer, cbuffer + rev, sizeof(cbline_t)*(bufferLines-rev));
		//for(i=0; (int)i<rev; i++) memset(cbuffer+bufferLines-rev+i, 0, sizeof(cbline_t));
		cbuffer = realloc(cbuffer, sizeof(cbline_t)*(bufferLines-=rev));
		// Move the current position.
		bPos -= rev;
	}
}

// Print into the buffer.
void Con_Printf(const char *format, ...)
{
	va_list		args;

	if(ConsoleSilent) return;

	va_start(args, format);	
	conPrintf(CBLF_WHITE, format, args);
	va_end(args);
}

void Con_FPrintf(int flags, const char *format, ...) // Flagged printf
{
	va_list		args;
	
	if(ConsoleSilent) return;

	va_start(args, format);
	conPrintf(flags, format, args);
	va_end(args);
}

// As you can see, several commands can be handled inside one command function.
int CCmdConsole(int argc, char **argv)
{
	if(!stricmp(argv[0], "help"))
	{
		if(argc == 2)
		{
			int	i;
			if(!stricmp(argv[1], "(what)"))
			{
				Con_Printf( "You've got to be kidding!\n");
				return true;
			}
			// We need to look through the cvars and ccmds to see if there's a match.
			for(i=0; i<numCCmds; i++)
				if(!stricmp(argv[1], ccmds[i].name))
				{
					Con_Printf("%s\n", ccmds[i].help);
					return true;
				}
			for(i=0; i<numCVars; i++)
				if(!stricmp(argv[1], cvars[i].name))
				{
					Con_Printf("%s\n", cvars[i].help);
					return true;
				}
			Con_Printf("There's no help about '%s'.\n", argv[1]);
		}
		else
		{
			Con_FPrintf(CBLF_RULER | CBLF_YELLOW | CBLF_CENTER, 
				"-=- Doomsday "DOOMSDAY_VERSION_TEXT" Console -=-\n");
			Con_Printf("Keys:\n");
			Con_Printf("Tilde         Open/close the console.\n");
			Con_Printf("Shift-Tilde   Switch between half and full screen mode.\n");
			Con_Printf("PageUp/Down   Scroll up/down two lines.\n");
			Con_Printf("Ins/Del       Move console window up/down one line.\n");
			Con_Printf("Shift-Ins/Del Move console window three lines at a time.\n");
			Con_Printf("Home          Jump to the beginning of the buffer.\n");
			Con_Printf("End           Jump to the end of the buffer.\n");
			Con_Printf("\n");
			Con_Printf("Type \"listcmds\" to see a list of available commands.\n");
			Con_Printf("Type \"help (what)\" to see information about (what).\n");
			Con_FPrintf(CBLF_RULER, "\n");
		}
	}
	else if(!stricmp(argv[0], "clear"))
	{
		Con_ClearBuffer();
	}
	return true;	
}

int CCmdListCmds(int argc, char **argv)
{
	int		i;

	Con_Printf("Console commands:\n");
	for(i = 0; i < numCCmds; i++)
	{
		if(argc > 1) // Is there a filter?
			if(strnicmp(ccmds[i].name, argv[1], strlen(argv[1])))
				continue;
		Con_Printf( "  %s (%s)\n", ccmds[i].name, ccmds[i].help);
	}
	return true;
}

int CCmdListVars(int argc, char **argv)
{
	int		i;

	Con_Printf("Console variables:\n");
	for(i = 0; i < numCVars; i++)
	{
		if(cvars[i].flags & CVF_HIDE) continue;
		if(argc > 1) // Is there a filter?
			if(strnicmp(cvars[i].name, argv[1], strlen(argv[1])))
				continue;
		printcvar(cvars+i, "  ");
	}
	return true;
}

D_CMD(ListAliases)
{
	int		i;

	Con_Printf("Aliases:\n");
	for(i=0; i<numCAliases; i++)
	{
		if(argc > 1) // Is there a filter?
			if(strnicmp(caliases[i].name, argv[1], strlen(argv[1])))
				continue;
		Con_Printf( "  %s == %s\n", caliases[i].name, caliases[i].command);
	}
	return true;
}

int CCmdVersion(int argc, char **argv)
{
	Con_Printf("Doomsday Engine %s ("__TIME__")\n", DOOMSDAY_VERSIONTEXT);
	Con_Printf("%s\n", gl.GetString(DGL_VERSION));
	Con_Printf("Game DLL: %s\n", gx.Get(DD_VERSION_LONG));
	return true;
}

int CCmdQuit(int argc, char **argv)
{
	// No questions asked.
	Sys_Quit();
	return true;
}

void Con_Open(int yes)
{
	// The console cannot be closed in dedicated mode.
	if(isDedicated) yes = true;

	// Clear all action keys, keyup events won't go 
	// to bindings processing when the console is open.
	P_ControlReset();
	
	openingOrClosing = true;
	if(yes)
	{
		ConsoleActive = true;
		ConsoleDestY = ConsoleOpenY;
		ConsoleTime = 0;
		ConsoleBlink = 0;
	}
	else
	{
		strcpy(cmdLine, "");
		cmdCursor = 0;
		ConsoleActive = false;
		ConsoleDestY = 0;
	}
}

//===========================================================================
// UpdateEngineState
//	What is this kind of a routine doing in Console.c?
//===========================================================================
void UpdateEngineState()
{
	// Update refresh.
	Con_Message("Updating state...\n");

	// Update the dir/WAD translations.
	F_InitDirec(); 

	gx.UpdateState(DD_PRE);
	R_Update();
	P_ValidateLevel();
	gx.UpdateState(DD_POST);
}

int CCmdLoadFile(int argc, char **argv)
{
	//extern int RegisteredSong;
	int		i, succeeded = false;	
	
	if(argc == 1)
	{
		Con_Printf( "Usage: load (file) ...\n");
		return true;
	}
	for(i=1; i<argc; i++)
	{
		Con_Message( "Loading %s...\n", argv[i]);
		if(W_AddFile(argv[i], true))	
		{
			Con_Message( "OK\n");
			succeeded = true; // At least one has been loaded.
		}
		else
			Con_Message( "Failed!\n");
	}
	// We only need to update if something was actually loaded.
	if(succeeded)
	{
		// Update the lumpcache.
		//W_UpdateCache();
		// The new wad may contain lumps that alter the current ones
		// in use.
		UpdateEngineState();
	}
	return true;
}

int CCmdUnloadFile(int argc, char **argv)
{
	//extern int RegisteredSong;
	int		i, succeeded = false;	

	if(argc == 1)
	{
		Con_Printf( "Usage: unload (file) ...\n");
		return true;
	}
	for(i=1; i<argc; i++)
	{
		Con_Message("Unloading %s...\n", argv[i]);
		if(W_RemoveFile(argv[i]))
		{
			Con_Message("OK\n");
			succeeded = true;
		}
		else
			Con_Message("Failed!\n");
	}
	if(succeeded) UpdateEngineState();
	return true;
}

int CCmdListFiles(int argc, char **argv)
{
	extern int numrecords;
	extern filerecord_t *records;
	int		i;
	
	for(i = 0; i < numrecords; i++)
	{
		Con_Printf("%s (%d lump%s%s)",
			records[i].filename,
			records[i].numlumps,
			records[i].numlumps!=1? "s" : "",
			!(records[i].flags & FRF_RUNTIME)? ", startup" : "");
		if(records[i].iwad)	
			Con_Printf(" [%08x]", W_CRCNumberForRecord(i));
		Con_Printf("\n");
	}

	Con_Printf("Total: %d lumps in %d files.\n", numlumps, numrecords);
	return true;
}

int CCmdResetLumps(int argc, char **argv)
{
	GL_SetFilter(0);
	W_Reset();
	Con_Message("Only startup files remain.\n");	
	UpdateEngineState();
	return true;
}

int CCmdBackgroundTurn(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf( "Usage: bgturn (speed)\n");
		Con_Printf( "Negative speeds are allowed. Default: 20.\n");
		Con_Printf( "Current bgturn = %d.\n", consoleTurn);
		return true;
	}
	consoleTurn = atoi(argv[1]);
	if(!consoleTurn) funnyAng = 0;
	return true;
}

int CCmdDump(int argc, char **argv)
{
	char fname[100];
	FILE *file;
	int lump;
	byte *lumpPtr;

	if(argc != 2)
	{
		Con_Printf( "Usage: dump (name)\n");
		Con_Printf( "Writes out the specified lump to (name).dum.\n");
		return true;		
	}
	if(W_CheckNumForName(argv[1]) == -1)
	{
		Con_Printf( "No such lump.\n");
		return false;
	}
	lump = W_GetNumForName(argv[1]);
	lumpPtr = W_CacheLumpNum(lump, PU_STATIC);

	sprintf(fname, "%s.dum", argv[1]);
	file = fopen(fname, "wb");
	if(!file) 
	{
		Con_Printf("Couldn't open %s for writing. %s\n", fname,
				   strerror(errno));
		Z_ChangeTag(lumpPtr, PU_CACHE);
		return false;
	}
	fwrite(lumpPtr, 1, lumpinfo[lump].size, file);
	fclose(file);
	Z_ChangeTag(lumpPtr, PU_CACHE);

	Con_Printf( "%s dumped to %s.\n", argv[1], fname);
	return true;
}

D_CMD(Font)
{
	if(argc == 1 || argc > 3)
	{
		Con_Printf("Usage: %s (cmd) (args)\n", argv[0]);
		Con_Printf("Commands: default, name, size, xsize, ysize.\n");
		Con_Printf("Names: Fixed, Fixed12, System, System12, Large, Small7, Small8, Small10.\n");
		Con_Printf("Size 1.0 is normal.\n");
		return true;
	}
	if(!stricmp(argv[1], "default"))
	{
		FR_DestroyFont(FR_GetCurrent());
		FR_PrepareFont("Fixed");
		Cfont.flags   = DDFONT_WHITE;
		Cfont.height  = FR_TextHeight("Con");
		Cfont.sizeX   = 1;
		Cfont.sizeY   = 1;
		Cfont.TextOut = FR_TextOut;
		Cfont.Width   = FR_TextWidth;
		Cfont.Filter  = NULL;
	}
	else if(!stricmp(argv[1], "name") && argc == 3)
	{
		FR_DestroyFont(FR_GetCurrent());
		if(!FR_PrepareFont(argv[2])) FR_PrepareFont("Fixed");
		Cfont.height = FR_TextHeight("Con");
	}
	else if(argc == 3)
	{
		if(!stricmp(argv[1], "xsize") || !stricmp(argv[1], "size"))
			Cfont.sizeX = strtod(argv[2], NULL);
		if(!stricmp(argv[1], "ysize") || !stricmp(argv[1], "size"))
			Cfont.sizeY = strtod(argv[2], NULL);
		// Make sure the sizes are valid.
		if(Cfont.sizeX <= 0) Cfont.sizeX = 1;
		if(Cfont.sizeY <= 0) Cfont.sizeY = 1;
	}
	return true;
}

//===========================================================================
// CCmdAlias
//	Aliases will be saved to the config file.
//===========================================================================
int CCmdAlias(int argc, char **argv)
{
	if(argc != 3 && argc != 2)
	{
		Con_Printf("Usage: %s (alias) (cmd)\n", argv[0]);
		Con_Printf("Example: alias bigfont \"font size 3\".\n");
		Con_Printf("Use %%1-%%9 to pass the alias arguments to the command.\n");
		return true;
	}
	Con_Alias(argv[1], argc==3? argv[2] : NULL);
	if(argc != 3)
		Con_Printf("Alias '%s' deleted.\n", argv[1]);
	return true;
}

D_CMD(SetGamma)
{
	int	newlevel;

	if(argc != 2)
	{
		Con_Printf( "Usage: %s (0-4)\n", argv[0]);
		return true;
	}
	newlevel = strtol(argv[1], NULL, 0);
	// Clamp it to the min and max.
	if(newlevel < 0) newlevel = 0;
	if(newlevel > 4) newlevel = 4;
	// Only reload textures if it's necessary.
	if(newlevel != usegamma)
	{
		usegamma = newlevel;
		GL_UpdateGamma();
		Con_Printf( "Gamma correction set to level %d.\n", usegamma);
	}
	else
		Con_Printf( "Gamma correction already set to %d.\n", usegamma);
	return true;
}

D_CMD(Parse)
{
	int		i;

	if(argc == 1)
	{
		Con_Printf("Usage: %s (file) ...\n", argv[0]);
		return true;
	}
	for(i = 1; i < argc; i++)
	{
		Con_Printf("Parsing %s.\n", argv[i]);
		Con_ParseCommands(argv[i], false);
	}
	return true;
}

D_CMD(DeleteBind)
{
	int		i;

	if(argc == 1)
	{
		Con_Printf( "Usage: %s (cmd) ...\n", argv[0]);
		return true;
	}
	for(i=1; i<argc; i++) B_ClearBinding(argv[i]);
	return true;
}	

//===========================================================================
// CCmdWait
//===========================================================================
int CCmdWait(int argc, char **argv)
{
	timespan_t offset;

	if(argc != 3)
	{
		Con_Printf("Usage: %s (tics) (cmd)\n", argv[0]);
		Con_Printf("For example, '%s 35 \"echo End\"'.\n", argv[0]);
		return true;
	}
	offset = strtod(argv[1], NULL) / 35; // Offset in seconds.
	if(offset < 0) offset = 0;
	Con_SplitIntoSubCommands(argv[2], offset);
	return true;
}

D_CMD(Repeat)
{
	int count;
	timespan_t interval, offset;

	if(argc != 4)
	{
		Con_Printf("Usage: %s (count) (interval) (cmd)\n", argv[0]);
		Con_Printf("For example, '%s 10 35 \"screenshot\".\n", argv[0]);
		return true;
	}
	count = atoi(argv[1]);
	interval = strtod(argv[2], NULL) / 35; // In seconds.
	offset = 0;
	while(count-- > 0)
	{
		offset += interval;
		Con_SplitIntoSubCommands(argv[3], offset);
	}
	return true;
}

D_CMD(Echo)
{
	int	i;

	for(i = 1; i < argc; i++) Con_Printf( "%s\n", argv[i]);
	return true;
}

// Rather messy, wouldn't you say?
D_CMD(AddSub)
{
	boolean force = false, incdec;
	float	val, mod = 0;
	cvar_t	*cvar;

	incdec = !stricmp(argv[0], "inc") || !stricmp(argv[0], "dec");
	if(argc == 1)
	{
		Con_Printf( "Usage: %s (cvar) %s(force)\n", argv[0],
			incdec? "" : "(val) ");
		Con_Printf( "Use force to make cvars go off limits.\n");
		return true;
	}
	if((incdec && argc >= 3) || (!incdec && argc >= 4))
	{
		force = !stricmp(argv[incdec? 2 : 3], "force");
	}
	cvar = Con_GetVariable(argv[1]);
	if(!cvar) return false;
	val = Con_GetFloat(argv[1]);

	if(!stricmp(argv[0], "inc"))
		mod = 1;
	else if(!stricmp(argv[0], "dec"))
		mod = -1;
	else
		mod = strtod(argv[2], NULL);
	if(!stricmp(argv[0], "sub")) mod = -mod;

	val += mod;
	if(!force)
	{
		if(!(cvar->flags & CVF_NO_MAX) && val > cvar->max) val = cvar->max;
		if(!(cvar->flags & CVF_NO_MIN) && val < cvar->min) val = cvar->min;
	}
	Con_SetFloat(argv[1], val);
	return true;
}

/*
 * Toggle the value of a variable between zero and nonzero.
 */
int CCmdToggle(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf("Usage: %s (cvar)\n", argv[0]);
		return true;
	}

	Con_SetInteger(argv[1], Con_GetInteger(argv[1])? 0 : 1);
	return true;
}

/*
 * Execute a command if the condition passes.
 */
int CCmdIf(int argc, char **argv)
{
	struct { const char *opstr; int op; } operators[] =
	{
		{ "not",	IF_NOT_EQUAL },
		{ "=",		IF_EQUAL },
		{ ">",		IF_GREATER },
		{ "<",		IF_LESS },
		{ ">=",		IF_GEQUAL },
		{ "<=",		IF_LEQUAL },
		{ NULL }
	};
	int i, oper;
	cvar_t *var;
	boolean isTrue = false;

	if(argc != 5 && argc != 6)
	{
		Con_Printf("Usage: %s (cvar) (operator) (value) (cmd) (else-cmd)\n", argv[0]);
		Con_Printf("Operator must be one of: not, =, >, <, >=, <=.\n");
		Con_Printf("The (else-cmd) can be omitted.\n");
		return true;
	}

	var = Con_GetVariable(argv[1]);
	if(!var) return false;

	// Which operator?
	for(i = 0; operators[i].opstr; i++)
		if(!stricmp(operators[i].opstr, argv[2]))
		{
			oper = operators[i].op;
			break;
		}
	if(!operators[i].opstr) return false;	// Bad operator.

	// Value comparison depends on the type of the variable.
	if(var->type == CVT_BYTE || var->type == CVT_INT)
	{
		int value = (var->type == CVT_INT? CV_INT(var) : CV_BYTE(var));
		int test = strtol(argv[3], 0, 0);

		isTrue = 
			 (oper == IF_EQUAL?		value == test
			: oper == IF_NOT_EQUAL?	value != test
			: oper == IF_GREATER?	value > test
			: oper == IF_LESS?		value < test
			: oper == IF_GEQUAL?	value >= test
			: value <= test);
	}
	else if(var->type == CVT_FLOAT)
	{
		float value = CV_FLOAT(var);
		float test = strtod(argv[3], 0);

		isTrue = 
			 (oper == IF_EQUAL?		value == test
			: oper == IF_NOT_EQUAL?	value != test
			: oper == IF_GREATER?	value > test
			: oper == IF_LESS?		value < test
			: oper == IF_GEQUAL?	value >= test
			: value <= test);
	}
	else if(var->type == CVT_CHARPTR)
	{
		int comp = stricmp(CV_CHARPTR(var), argv[3]);

		isTrue = 
			 (oper == IF_EQUAL?		comp == 0
			: oper == IF_NOT_EQUAL?	comp != 0
			: oper == IF_GREATER?	comp > 0
			: oper == IF_LESS?		comp < 0
			: oper == IF_GEQUAL?	comp >= 0
			: comp <= 0);
	}

	// Should the command be executed?
	if(isTrue)
	{
		Con_Execute(argv[4], ConsoleSilent);
	}
	else if(argc == 6)
	{
		Con_Execute(argv[5], ConsoleSilent);
	}
	CmdReturnValue = isTrue;
	return true;
}

/*
 * Prints a file name to the console.
 * This is a f_forall_func_t.
 */
int Con_PrintFileName(const char *fn, filetype_t type, void *dir)
{
	// Exclude the path.
	Con_Printf("  %s\n", fn + strlen(dir));

	// Continue the listing.
	return true;
}

/*
 * Print contents of directories as Doomsday sees them.
 */
int CCmdDir(int argc, char **argv)
{
	char dir[256], pattern[256];
	int i;

	if(argc == 1)
	{
		Con_Printf("Usage: %s (dirs)\n", argv[0]);
		Con_Printf("Prints the contents of one or more directories.\n");
		Con_Printf("Virtual files are listed, too.\n");
		Con_Printf("Paths are relative to the base path:\n");
		Con_Printf("  %s\n", ddBasePath);
		return true;
	}

	for(i = 1; i < argc; i++)
	{
		M_PrependBasePath(argv[i], dir);
		Dir_ValidDir(dir);
		Dir_MakeAbsolute(dir);
		Con_Printf("Directory: %s\n", dir);

		// Make the pattern.
		sprintf(pattern, "%s*.*", dir);
		F_ForAll(pattern, dir, Con_PrintFileName);
	}

	return true;
}

/*
 * Print a 'global' message (to stdout and the console).
 */
void Con_Message(const char *message, ...)
{
	va_list argptr;
	char *buffer;
	
	if(message[0])
	{
		buffer = malloc(0x10000);
		
		va_start(argptr, message);
		vsprintf(buffer, message, argptr);
		va_end(argptr);

#ifdef UNIX
		if(!isDedicated)
		{
			// These messages are supposed to be visible in the real console.
			fprintf(stderr, "%s", buffer);
		}
#endif
		
		// These messages are always dumped. If consoleDump is set,
		// Con_Printf() will dump the message for us.
		if(!consoleDump) printf("%s", buffer);
		
		// Also print in the console.
		Con_Printf(buffer);
		
		free(buffer);
	}
	Con_DrawStartupScreen(true);
}

/*
 * Print an error message and quit.
 */
void Con_Error (const char *error, ...)
{
	static boolean errorInProgress = false;
	extern int bufferLines;
	int i;
	char buff[2048], err[256];
	va_list argptr;

	// Already in an error?
	if(!ConsoleInited || errorInProgress)
	{
		fprintf(outFile, "Con_Error: Stack overflow imminent, aborting...\n");

		va_start(argptr, error);
		vsprintf(buff, error, argptr);
		va_end(argptr);
		Sys_MessageBox(buff, true);

		// Exit immediately, lest we go into an infinite loop.
		exit(1);
	}

	// We've experienced a fatal error; program will be shut down.
	errorInProgress = true;

	// Get back to the directory we started from.
	Dir_ChDir(&ddRuntimeDir);

	va_start(argptr,error);
	vsprintf(err, error, argptr);
	va_end(argptr);
	fprintf(outFile, "%s\n", err);

	strcpy(buff, "");
	for(i = 5; i > 1; i--)
	{
		cbline_t *cbl = Con_GetBufferLine(bufferLines - i);
		if(!cbl || !cbl->text) continue;
		strcat(buff, cbl->text);
		strcat(buff, "\n");
	}
	strcat(buff, "\n");
	strcat(buff, err);

	Sys_Shutdown();
	B_Shutdown();
	Con_Shutdown();

#ifdef WIN32
	ChangeDisplaySettings(0, 0); // Restore original mode, just in case.
#endif

	// Be a bit more graphic.
	Sys_ShowCursor(true);
	Sys_ShowCursor(true);
	if(err[0]) // Only show if a message given.
	{
		Sys_MessageBox(buff, true);
	}

	DD_Shutdown();

	// Open Doomsday.out in a text editor.
	fflush(outFile); // Make sure all the buffered stuff goes into the file.
	Sys_OpenTextEditor("Doomsday.out");

	// Get outta here.
	exit(1);
}

