
//**************************************************************************
//**
//** MN_MENU.C
//**
//** Heavily based on Hexen's original menu code.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <math.h>
#include "h2def.h"
#include "p_local.h"
#include "r_local.h"
#include "soundst.h"
//#include "g_demo.h"
#include "h2_actn.h"
#include "mn_def.h"
#include "settings.h"
#include "LZSS.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

// Control flags.
#define CLF_ACTION		0x1		// The control is an action (+/- in front).
#define CLF_REPEAT		0x2		// Bind down + repeat.

#define	CVAR(typ, x)	(*(typ*)Con_GetVariable(x)->ptr)

// TYPES -------------------------------------------------------------------

typedef struct
{
	int	width, height;
} MenuRes_t;

typedef struct
{
	char		*command;		// The command to execute.
	int			flags;
	int			defKey;			// 
	int			defMouse;		// Zero means there is no default.
	int			defJoy;			//
} Control_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

float MN_GL_SetupState(float time, float offset);
void MN_GL_RestoreState();
//boolean F_Responder(event_t *ev);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void SetMenu(MenuType_t menu);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void InitFonts(void);
static void SCQuitGame(int option);
static void SCClass(int option);
static void SCSkill(int option);
static void SCOpenDCP(int option);
static void SCMouseXSensi(int option);
static void SCMouseYSensi(int option);
static void SCMouseLook(int option);
static void SCMouseLookInverse(int option);
static void SCJoyLook(int option);
static void SCPOVLook(int option);
static void SCInverseJoyLook(int option);
static void SCEnableJoy(int option);
static void SCJoyAxis(int option);
static void SCFullscreenMana(int option);
static void SCLookSpring(int option);
static void SCAutoAim(int option);
static void SCSkyDetail(int option);
static void SCMipmapping(int option);
static void SCLinearRaw(int option);
static void SCForceTexReload(int option);
static void SCResSelector(int option);
static void SCResMakeCurrent(int option);
static void SCResMakeDefault(int option);
static void SCSfxVolume(int option);
static void SCMusicVolume(int option);
static void SCCDVolume(int option);
static void SCScreenSize(int option);
static void SCStatusBarSize(int option);
static void SCMusicDevice(int option);
static boolean SCNetCheck(int option);
static void SCNetCheck2(int option);
static void SCLoadGame(int option);
static void SCSaveGame(int option);
static void SCMessages(int option);
static void SCAlwaysRun(int option);
static void SCControlConfig(int option);
static void SCJoySensi(int option);
static void SCEndGame(int option);
static void SCInfo(int option);
static void SCCrosshair(int option);
static void SCCrosshairSize(int option);

//static void SCBorderUpdate(int option);
static void SCTexQuality(int option);
static void SCFPSCounter(int option);
static void SCIceCorpse(int option);
static void SCDynLights(int option);
static void SCDLBlend(int option);
static void SCDLIntensity(int option);
static void SCFlares(int option);
static void SCFlareIntensity(int option);
static void SCFlareSize(int option);
static void SCSpriteAlign(int option);
static void SCSpriteBlending(int option);
static void SCSpriteLight(int option);
static void SCUseModels(int option);

static void SC3DSounds(int option);
static void SCReverbVolume(int option);
static void SCSfxFrequency(int option);
static void SCSfx16bit(int option);

static void DrawMainMenu(void);
static void DrawClassMenu(void);
static void DrawSkillMenu(void);
static void DrawOptionsMenu(void);
static void DrawOptions2Menu(void);
static void DrawGameplayMenu(void);
static void DrawGraphicsMenu(void);
static void DrawEffectsMenu(void);
static void DrawResolutionMenu(void);
static void DrawMouseOptsMenu(void);
static void DrawControlsMenu(void);
static void DrawJoyConfigMenu(void);
static void DrawFileSlots(Menu_t *menu);
static void DrawFilesMenu(void);
static void MN_DrawInfo(void);
static void DrawLoadMenu(void);
static void DrawSaveMenu(void);
static void DrawSlider(Menu_t *menu, int item, int width, int slot);
void MN_LoadSlotText(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean gamekeydown[256]; // The NUMKEYS macro is local to g_game

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean	MenuActive;
int		InfoType;
boolean messageson = true;
boolean mn_SuicideConsole;
boolean shiftdown;
Menu_t	*CurrentMenu;
int		CurrentItPos;
int		MenuTime;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *YesNo[2] = { "NO", "YES" };

static int FontABaseLump;
static int FontAYellowBaseLump;
static int FontBBaseLump;
static int MauloBaseLump;
static int MenuPClass;

//
// !!! Add new controls to the end, the existing indices must remain unchanged !!!
//
static Control_t controls[] =
{
	// Actions (must be first so the A_* constants can be used).
	"left",			CLF_ACTION,		DDKEY_LEFTARROW, 0, 0,
	"right",		CLF_ACTION,		DDKEY_RIGHTARROW, 0, 0,
	"forward",		CLF_ACTION,		DDKEY_UPARROW, 0, 0,
	"backward",		CLF_ACTION,		DDKEY_DOWNARROW, 0, 0,
	"strafel",		CLF_ACTION,		',', 0, 0,
	"strafer",		CLF_ACTION,		'.', 0, 0,
	"jump",			CLF_ACTION,		'/', 2, 5,
	"fire",			CLF_ACTION,		DDKEY_RCTRL, 1, 1,
	"use",			CLF_ACTION,		' ', 0, 4,
	"strafe",		CLF_ACTION,		DDKEY_RALT, 3, 2,
	
	"speed",		CLF_ACTION,		DDKEY_RSHIFT, 0, 3,
	"flyup",		CLF_ACTION,		DDKEY_PGUP, 0, 8,
	"flydown",		CLF_ACTION,		DDKEY_INS, 0, 9,
	"falldown",		CLF_ACTION,		DDKEY_HOME, 0, 0,
	"lookup",		CLF_ACTION,		DDKEY_PGDN, 0, 6,
	"lookdown",		CLF_ACTION,		DDKEY_DEL, 0, 7,
	"lookcntr",		CLF_ACTION,		DDKEY_END, 0, 0,
	"usearti",		CLF_ACTION,		DDKEY_ENTER, 0, 0,
	"mlook",		CLF_ACTION,		'm', 0, 0,
	"jlook",		CLF_ACTION,		'j', 0, 0,
	
	"nextwpn",		CLF_ACTION,		0, 0, 0,
	"prevwpn",		CLF_ACTION,		0, 0, 0,
	"weapon1",		CLF_ACTION,		'1', 0, 0,
	"weapon2",		CLF_ACTION,		'2', 0, 0,
	"weapon3",		CLF_ACTION,		'3', 0, 0,
	"weapon4",		CLF_ACTION,		'4', 0, 0,
	"panic",		CLF_ACTION,		DDKEY_BACKSPACE, 0, 0,
	"torch",		CLF_ACTION,		0, 0, 0,
	"health",		CLF_ACTION,		'\\', 0, 0,
	"mystic",		CLF_ACTION,		0, 0, 0,
	
	"krater",		CLF_ACTION,		0, 0, 0,
	"spdboots",		CLF_ACTION,		0, 0, 0,
	"blast",		CLF_ACTION,		'9', 0, 0,
	"teleport",		CLF_ACTION,		'8', 0, 0,
	"teleothr",		CLF_ACTION,		'7', 0, 0,
	"poison",		CLF_ACTION,		'0', 0, 0,
	"cantdie",		CLF_ACTION,		'5', 0, 0,
	"servant",		CLF_ACTION,		0, 0, 0,
	"egg",			CLF_ACTION,		'6', 0, 0,
	"demostop",		CLF_ACTION,		'o', 0, 0,

	// Menu hotkeys (default: F1 - F12).
	"infoscreen",	0,				DDKEY_F1, 0, 0,
	"loadgame",		0,				DDKEY_F3, 0, 0,
	"savegame",		0,				DDKEY_F2, 0, 0,
	"soundmenu",	0,				DDKEY_F4, 0, 0,
	"suicide",		0,				DDKEY_F5, 0, 0,
	"quicksave",	0,				DDKEY_F6, 0, 0,
	"endgame",		0,				DDKEY_F7, 0, 0,
	"togglemsgs",	0,				DDKEY_F8, 0, 0,
	"quickload",	0,				DDKEY_F9, 0, 0,
	"quit",			0,				DDKEY_F10, 0, 0,
	"togglegamma",	0,				DDKEY_F11, 0, 0,
	"spy",			0,				DDKEY_F12, 0, 0,

	// Inventory.
	"invleft",		CLF_REPEAT,		'[', 0, 0,
	"invright",		CLF_REPEAT,		']', 0, 0,

	// Screen controls.
	"viewsize +",	CLF_REPEAT,		'=', 0, 0,
	"viewsize -",	CLF_REPEAT,		'-', 0, 0,
	"sbsize +",		CLF_REPEAT,		0, 0, 0,
	"sbsize -",		CLF_REPEAT,		0, 0, 0,

	// Misc.
	"pause",		0,				DDKEY_PAUSE, 0, 0,
	"",				0,				0, 0, 0
};

static Control_t *grabbing = NULL;
static float bgAlpha=0, outFade=0;
static boolean fadingOut = false;
static int menuDarkTicks = 15;
static int slamInTicks = 9;

boolean askforquit;
boolean typeofask;
static boolean FileMenuKeySteal;
static boolean slottextloaded;
static char SlotText[6][SLOTTEXTLEN+2];
static char oldSlotText[SLOTTEXTLEN+2];
static int SlotStatus[6];
static int slotptr;
static int currentSlot;
static int quicksave;
static int quickload;

static MenuItem_t MainItems[] =
{
	{ ITT_SETMENU, "NEW GAME", SCNetCheck2, 1, MENU_CLASS },
	{ ITT_EFUNC, "MULTIPLAYER", SCEnterMultiplayerMenu, 0, MENU_NONE },
	{ ITT_SETMENU, "OPTIONS", NULL, 0, MENU_OPTIONS },
	{ ITT_SETMENU, "GAME FILES", NULL, 0, MENU_FILES },
	{ ITT_EFUNC, "INFO", SCInfo, 0, MENU_NONE },
	{ ITT_EFUNC, "QUIT GAME", SCQuitGame, 0, MENU_NONE }
};

static Menu_t MainMenu =
{
	110, 56,
	DrawMainMenu,
	6, MainItems,
	0,
	MENU_NONE,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 6, 0
};

static MenuItem_t ClassItems[] =
{
	{ ITT_EFUNC, "FIGHTER", SCClass, 0, MENU_NONE },
	{ ITT_EFUNC, "CLERIC", SCClass, 1, MENU_NONE },
	{ ITT_EFUNC, "MAGE", SCClass, 2, MENU_NONE }
};

static Menu_t ClassMenu =
{
	66, 66,
	DrawClassMenu,
	3, ClassItems,
	0,
	MENU_MAIN,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 3, 0
};

static MenuItem_t FilesItems[] =
{
	{ ITT_SETMENU, "LOAD GAME", SCNetCheck2, 2, MENU_LOAD },
	{ ITT_SETMENU, "SAVE GAME", SCNetCheck2, 4, MENU_SAVE }
};

static Menu_t FilesMenu =
{
	110, 60,
	DrawFilesMenu,
	2, FilesItems,
	0,
	MENU_MAIN,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 2, 0
};

static MenuItem_t LoadItems[] =
{
	{ ITT_EFUNC, NULL, SCLoadGame, 0, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 1, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 2, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 3, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 4, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 5, MENU_NONE }
};

static Menu_t LoadMenu =
{
	70, 30,
	DrawLoadMenu,
	6, LoadItems,
	0,
	MENU_FILES,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 6, 0
};

static MenuItem_t SaveItems[] =
{
	{ ITT_EFUNC, NULL, SCSaveGame, 0, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 1, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 2, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 3, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 4, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 5, MENU_NONE }
};

static Menu_t SaveMenu =
{
	70, 30,
	DrawSaveMenu,
	6, SaveItems,
	0,
	MENU_FILES,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 6, 0
};

static MenuItem_t SkillItems[] =
{
	{ ITT_EFUNC, NULL, SCSkill, sk_baby, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSkill, sk_easy, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSkill, sk_medium, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSkill, sk_hard, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSkill, sk_nightmare, MENU_NONE }
};

static Menu_t SkillMenu =
{
	120, 44,
	DrawSkillMenu,
	5, SkillItems,
	2,
	MENU_CLASS,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 5, 0
};

static MenuItem_t OptionsItems[] =
{
	{ ITT_EFUNC, "END GAME", SCEndGame, 0, MENU_NONE },
	{ ITT_EFUNC, "CONTROL PANEL", SCOpenDCP, 0, MENU_NONE },
	{ ITT_SETMENU, "GAMEPLAY...", NULL, 0, MENU_GAMEPLAY },
//	{ ITT_SETMENU, "GRAPHICS...", NULL, 0, MENU_GRAPHICS },
	{ ITT_SETMENU, "SOUND...", NULL, 0, MENU_OPTIONS2 },
	{ ITT_SETMENU, "CONTROLS...", NULL, 0, MENU_CONTROLS },
	{ ITT_SETMENU, "MOUSE OPTIONS...", NULL, 0, MENU_MOUSEOPTS },
	{ ITT_SETMENU, "JOYSTICK OPTIONS...", NULL, 0, MENU_JOYCONFIG }
};

static Menu_t OptionsMenu =
{
	110, 80,
	DrawOptionsMenu,
	7, OptionsItems,
	0,
	MENU_MAIN,
	MN_DrTextA_CS, 9,//ITEM_HEIGHT
	0, 7, 0
};

static MenuItem_t Options2Items[] =
{
	{ ITT_LRFUNC, "SFX VOLUME", SCSfxVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "MUSIC VOLUME", SCMusicVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "OPEN AUDIO PANEL", SCOpenDCP, 1, MENU_NONE }
/*	{ ITT_LRFUNC, "CD VOLUME", SCCDVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "MUSIC DEVICE :", SCMusicDevice, 0, MENU_NONE },
	{ ITT_EFUNC, "3D SOUNDS :", SC3DSounds, 0, MENU_NONE },
	{ ITT_LRFUNC, "REVERB VOLUME :", SCReverbVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "SFX FREQUENCY :", SCSfxFrequency, 0, MENU_NONE },
	{ ITT_EFUNC, "16 BIT INTERPOLATION :", SCSfx16bit, 0, MENU_NONE }*/
};

static Menu_t Options2Menu =
{
	70, 25,
	DrawOptions2Menu,
	7, Options2Items,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10,
	0, 7, 0
};

static MenuItem_t GameplayItems[] =
{
	{ ITT_EFUNC, "MESSAGES :", SCMessages, 0, MENU_NONE },
	{ ITT_EFUNC, "ALWAYS RUN :", SCAlwaysRun, 0, MENU_NONE },
	{ ITT_EFUNC, "LOOKSPRING :", SCLookSpring, 0, MENU_NONE },
	{ ITT_EFUNC, "NO AUTOAIM :", SCAutoAim, 0, MENU_NONE },
	{ ITT_EFUNC, "FULLSCREEN MANA :", SCFullscreenMana, 0, MENU_NONE },
	{ ITT_LRFUNC, "CROSSHAIR :", SCCrosshair, 0, MENU_NONE },
	{ ITT_LRFUNC, "CROSSHAIR SIZE :", SCCrosshairSize, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "SCREEN SIZE", SCScreenSize, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "STATUS BAR SIZE", SCStatusBarSize, 0, MENU_NONE},
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE }
};

static Menu_t GameplayMenu =
{
	64, 25,
	DrawGameplayMenu,
	15, GameplayItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10,
	0, 15, 0
};

/*static MenuItem_t GraphicsItems[] = 
{
	{ ITT_SETMENU, "RESOLUTION...", NULL, 0, MENU_RESOLUTION },
	{ ITT_SETMENU, "EFFECTS...", NULL, 0, MENU_EFFECTS },
	{ ITT_EFUNC, "3D MODELS :", SCUseModels, 0, MENU_NONE },
	{ ITT_LRFUNC, "SKY DETAIL", SCSkyDetail, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "MIPMAPPING :", SCMipmapping, 0, MENU_NONE },
	{ ITT_EFUNC, "SMOOTH GFX :", SCLinearRaw, 0, MENU_NONE },
	//{ ITT_EFUNC, "UPDATE BORDERS :", SCBorderUpdate, 0, MENU_NONE },
	{ ITT_LRFUNC, "TEX QUALITY :", SCTexQuality, 0, MENU_NONE },
	{ ITT_EFUNC, "FORCE TEX RELOAD", SCForceTexReload, 0, MENU_NONE }
};*/

/*static Menu_t GraphicsMenu =
{
	80, 27,
	DrawGraphicsMenu,
	10, GraphicsItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10, //ITEM_HEIGHT,
	0, 10, 0
};*/

/*static MenuItem_t EffectsItems[] =
{
	{ ITT_EFUNC, "FPS COUNTER :", SCFPSCounter, 0, MENU_NONE },
	{ ITT_EFUNC, "FROZEN THINGS :", SCIceCorpse, 0, MENU_NONE },
	{ ITT_EFUNC, "DYNAMIC LIGHTS :", SCDynLights, 0, MENU_NONE },
	{ ITT_LRFUNC, "DYNLIGHT BLENDING :", SCDLBlend, 0, MENU_NONE },
	{ ITT_EFUNC, "LIGHTS ON SPRITES :", SCSpriteLight, 0, MENU_NONE },
	{ ITT_LRFUNC, "DYNLIGHT INTENSITY :", SCDLIntensity, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "LENS FLARES :", SCFlares, 0, MENU_NONE },
	{ ITT_LRFUNC, "FLARE INTENSITY :", SCFlareIntensity, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "FLARE SIZE :", SCFlareSize, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "ALIGN SPRITES TO :", SCSpriteAlign, 0, MENU_NONE },
	{ ITT_EFUNC, "SPRITE BLENDING :", SCSpriteBlending, 0, MENU_NONE }
};*/

/*static Menu_t EffectsMenu =
{
	68, 25,
	DrawEffectsMenu,
	17, EffectsItems,
	0,
	MENU_GRAPHICS,
	MN_DrTextA_CS, 10,
	0, 17, 0
};*/

/*static MenuItem_t ResolutionItems[] =
{
	{ ITT_LRFUNC, "RESOLUTION :", SCResSelector, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "USE", SCResMakeCurrent, 0, MENU_NONE },
	{ ITT_EFUNC, "SET DEFAULT", SCResMakeDefault, 0, MENU_NONE }
};

static Menu_t ResolutionMenu =
{
	88, 60,
	DrawResolutionMenu,
	4, ResolutionItems,
	0,
	MENU_GRAPHICS,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 4, 0
};*/

static MenuItem_t ControlsItems[] =
{
	{ ITT_EMPTY, "PLAYER ACTIONS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "LEFT :", SCControlConfig, A_TURNLEFT, MENU_NONE },
	{ ITT_EFUNC, "RIGHT :", SCControlConfig, A_TURNRIGHT, MENU_NONE },
	{ ITT_EFUNC, "FORWARD :", SCControlConfig, A_FORWARD, MENU_NONE },
	{ ITT_EFUNC, "BACKWARD :", SCControlConfig, A_BACKWARD, MENU_NONE },
	{ ITT_EFUNC, "STRAFE LEFT :", SCControlConfig, A_STRAFELEFT, MENU_NONE },
	{ ITT_EFUNC, "STRAFE RIGHT :", SCControlConfig, A_STRAFERIGHT, MENU_NONE },
	{ ITT_EFUNC, "JUMP :", SCControlConfig, A_JUMP, MENU_NONE },
	{ ITT_EFUNC, "FIRE :", SCControlConfig, A_FIRE, MENU_NONE },
	{ ITT_EFUNC, "USE :", SCControlConfig, A_USE, MENU_NONE },	
	{ ITT_EFUNC, "STRAFE :", SCControlConfig, A_STRAFE, MENU_NONE },
	{ ITT_EFUNC, "SPEED :", SCControlConfig, A_SPEED, MENU_NONE },
	{ ITT_EFUNC, "FLY UP :", SCControlConfig, A_FLYUP, MENU_NONE },
	{ ITT_EFUNC, "FLY DOWN :", SCControlConfig, A_FLYDOWN, MENU_NONE },
	{ ITT_EFUNC, "FALL DOWN :", SCControlConfig, A_FLYCENTER, MENU_NONE },
	{ ITT_EFUNC, "LOOK UP :", SCControlConfig, A_LOOKUP, MENU_NONE },
	{ ITT_EFUNC, "LOOK DOWN :", SCControlConfig, A_LOOKDOWN, MENU_NONE },
	{ ITT_EFUNC, "LOOK CENTER :", SCControlConfig, A_LOOKCENTER, MENU_NONE },
	{ ITT_EFUNC, "MOUSE LOOK :", SCControlConfig, A_MLOOK, MENU_NONE },
	{ ITT_EFUNC, "JOYSTICK LOOK :", SCControlConfig, A_JLOOK, MENU_NONE },
	{ ITT_EFUNC, "NEXT WEAPON :", SCControlConfig, A_NEXTWEAPON, MENU_NONE },
	{ ITT_EFUNC, "PREV WEAPON :", SCControlConfig, A_PREVIOUSWEAPON, MENU_NONE },
	{ ITT_EFUNC, "WEAPON 1 :", SCControlConfig, A_WEAPON1, MENU_NONE },
	{ ITT_EFUNC, "WEAPON 2 :", SCControlConfig, A_WEAPON2, MENU_NONE },
	{ ITT_EFUNC, "WEAPON 3 :", SCControlConfig, A_WEAPON3, MENU_NONE },
	{ ITT_EFUNC, "WEAPON 4 :", SCControlConfig, A_WEAPON4, MENU_NONE },
	{ ITT_EFUNC, "PANIC :", SCControlConfig, A_PANIC, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "ARTIFACTS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "TORCH :", SCControlConfig, A_TORCH, MENU_NONE },
	{ ITT_EFUNC, "QUARTZ FLASK :", SCControlConfig, A_HEALTH, MENU_NONE },
	{ ITT_EFUNC, "MYSTIC URN :", SCControlConfig, A_MYSTICURN, MENU_NONE },
	{ ITT_EFUNC, "KRATER OF MIGHT :", SCControlConfig, A_KRATER, MENU_NONE },
	{ ITT_EFUNC, "BOOTS OF SPEED :", SCControlConfig, A_SPEEDBOOTS, MENU_NONE },
	{ ITT_EFUNC, "REPULSION :", SCControlConfig, A_BLASTRADIUS, MENU_NONE },
	{ ITT_EFUNC, "CHAOS DEVICE :", SCControlConfig, A_TELEPORT, MENU_NONE },
	{ ITT_EFUNC, "BANISHMENT :", SCControlConfig, A_TELEPORTOTHER, MENU_NONE },
	{ ITT_EFUNC, "BOOTS OF SPEED :", SCControlConfig, A_SPEEDBOOTS, MENU_NONE },
	{ ITT_EFUNC, "FLECHETTE :", SCControlConfig, A_POISONBAG, MENU_NONE },		
	{ ITT_EFUNC, "DEFENDER :", SCControlConfig, A_INVULNERABILITY, MENU_NONE },
	{ ITT_EFUNC, "DARK SERVANT :", SCControlConfig, A_DARKSERVANT, MENU_NONE },
	{ ITT_EFUNC, "PORKELATOR :", SCControlConfig, A_EGG, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "INVENTORY", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "INVENTORY LEFT :", SCControlConfig, 52, MENU_NONE },
	{ ITT_EFUNC, "INVENTORY RIGHT :", SCControlConfig, 53, MENU_NONE },
	{ ITT_EFUNC, "USE ARTIFACT :", SCControlConfig, A_USEARTIFACT, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "MENU HOTKEYS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "INFO :", SCControlConfig, 40, MENU_NONE },
	{ ITT_EFUNC, "SOUND MENU :", SCControlConfig, 43, MENU_NONE },
	{ ITT_EFUNC, "LOAD GAME :", SCControlConfig, 41, MENU_NONE },
	{ ITT_EFUNC, "SAVE GAME :", SCControlConfig, 42, MENU_NONE },
	{ ITT_EFUNC, "QUICK LOAD :", SCControlConfig, 48, MENU_NONE },
	{ ITT_EFUNC, "QUICK SAVE :", SCControlConfig, 45, MENU_NONE },
	{ ITT_EFUNC, "SUICIDE :", SCControlConfig, 44, MENU_NONE },
	{ ITT_EFUNC, "END GAME :", SCControlConfig, 46, MENU_NONE },
	{ ITT_EFUNC, "QUIT :", SCControlConfig, 49, MENU_NONE },
	{ ITT_EFUNC, "MESSAGES ON/OFF:", SCControlConfig, 47, MENU_NONE },
	{ ITT_EFUNC, "GAMMA CORRECTION :", SCControlConfig, 50, MENU_NONE },
	{ ITT_EFUNC, "SPY MODE :", SCControlConfig, 51, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "SCREEN", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "SMALLER VIEW :", SCControlConfig, 55, MENU_NONE },
	{ ITT_EFUNC, "LARGER VIEW :", SCControlConfig, 54, MENU_NONE },
	{ ITT_EFUNC, "SMALLER ST. BAR :", SCControlConfig, 57, MENU_NONE },
	{ ITT_EFUNC, "LARGER ST. BAR :", SCControlConfig, 56, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "MISCELLANEOUS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "STOP DEMO :", SCControlConfig, A_STOPDEMO, MENU_NONE },
	{ ITT_EFUNC, "PAUSE :", SCControlConfig, 58, MENU_NONE }
};

static Menu_t ControlsMenu =
{
	32, 26,
	DrawControlsMenu,
	71, ControlsItems,
	1,
	MENU_OPTIONS,
	MN_DrTextA_CS, 9,
	0, 18, 0
};

static MenuItem_t MouseOptsItems[] =
{
	{ ITT_EFUNC, "MOUSE LOOK :", SCMouseLook, 0, MENU_NONE },
	{ ITT_EFUNC, "INVERSE MLOOK :", SCMouseLookInverse, 0, MENU_NONE },
	{ ITT_LRFUNC, "X SENSITIVITY", SCMouseXSensi, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "Y SENSITIVITY", SCMouseYSensi, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE }
};

static Menu_t MouseOptsMenu = 
{
	72, 25,
	DrawMouseOptsMenu,
	6, MouseOptsItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10, //ITEM_HEIGHT,
	0, 6, 0
};

static MenuItem_t JoyConfigItems[] =
{
	{ ITT_LRFUNC, "X AXIS :", SCJoyAxis, 0 << 8, MENU_NONE },
	{ ITT_LRFUNC, "Y AXIS :", SCJoyAxis, 1 << 8, MENU_NONE },
	{ ITT_LRFUNC, "Z AXIS :", SCJoyAxis, 2 << 8, MENU_NONE },
	{ ITT_LRFUNC, "RX AXIS :", SCJoyAxis, 3 << 8, MENU_NONE },
	{ ITT_LRFUNC, "RY AXIS :", SCJoyAxis, 4 << 8, MENU_NONE },
	{ ITT_LRFUNC, "RZ AXIS :", SCJoyAxis, 5 << 8, MENU_NONE },
	{ ITT_LRFUNC, "SLIDER 1 :", SCJoyAxis, 6 << 8, MENU_NONE },
	{ ITT_LRFUNC, "SLIDER 2 :", SCJoyAxis, 7 << 8, MENU_NONE },
	{ ITT_EFUNC, "JOY LOOK :", SCJoyLook, 0, MENU_NONE },
	{ ITT_EFUNC, "INVERSE LOOK :", SCInverseJoyLook, 0, MENU_NONE },
	{ ITT_EFUNC, "POV LOOK :", SCPOVLook, 0, MENU_NONE }
};

static Menu_t JoyConfigMenu =
{
	72, 25,
	DrawJoyConfigMenu,
	11, JoyConfigItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10, //ITEM_HEIGHT,
	0, 11, 0
};

static Menu_t *Menus[] =
{
	&MainMenu,
	&ClassMenu,
	&SkillMenu,
	&OptionsMenu,
	&Options2Menu,
	&GameplayMenu,
	0, //&GraphicsMenu,
	0, //&EffectsMenu,
	0, //&ResolutionMenu,
	&ControlsMenu,
	&MouseOptsMenu,
	&JoyConfigMenu,
	&FilesMenu,
	&LoadMenu,
	&SaveMenu,
	&MultiplayerMenu,
	0, //&ProtocolMenu,
	0, //&HostMenu,
	0, //&JoinMenu,
	&GameSetupMenu,
	&PlayerSetupMenu,
	0, //&NetGameMenu,
	0, //&TCPIPMenu,
	0, //&SerialMenu,
	0 //&ModemMenu
};

/*static char *GammaText[] = 
{
	TXT_GAMMA_LEVEL_OFF,
	TXT_GAMMA_LEVEL_1,
	TXT_GAMMA_LEVEL_2,
	TXT_GAMMA_LEVEL_3,
	TXT_GAMMA_LEVEL_4
};*/
	
// CODE --------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// PROC MN_Init
//
//---------------------------------------------------------------------------

/*static int findRes(int w, int h)
{
	int i;

	for(i=0; resolutions[i].width; i++)
		if(resolutions[i].width == w && resolutions[i].height == h)
			return i;
	return -1;
}*/

void MN_Init(void)
{
	InitFonts();
	MenuActive = false;
	MauloBaseLump = W_GetNumForName("FBULA0"); // ("M_SKL00");
	CurrentMenu = &MainMenu;
}

//---------------------------------------------------------------------------
//
// PROC InitFonts
//
//---------------------------------------------------------------------------

static void InitFonts(void)
{
	FontABaseLump = W_GetNumForName("FONTA_S")+1;
	FontAYellowBaseLump = W_GetNumForName("FONTAY_S")+1;
	FontBBaseLump = W_GetNumForName("FONTB_S")+1;
}

//---------------------------------------------------------------------------
//
// PROC MN_TextFilter
//
//---------------------------------------------------------------------------

int MN_FilterChar(int ch)
{
	ch = toupper(ch);
	if(ch == '_') ch = '[';
	else if(ch == '\\') ch = '/';
	else if(ch < 32 || ch > 'Z') ch = 32; // We don't have this char.
	return ch;
}

void MN_TextFilter(char *text)
{
	int		k;

	for(k=0; text[k]; k++)
	{
/*		char ch = toupper(text[k]);
		if(ch == '_') ch = '[';	// Mysterious... (from save slots).
		else if(ch == '\\') ch = '/';
		// Check that the character is printable.
		else if(ch < 32 || ch > 'Z') ch = 32; // Character out of range.
		text[k] = ch;			*/
		text[k] = MN_FilterChar(text[k]);
	}
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextA
//
// Draw text using font A.
//
//---------------------------------------------------------------------------

void MN_DrTextA_CS(char *text, int x, int y)
{
	char c;
	patch_t *p;

	while((c = *text++) != 0)
	{
		c = MN_FilterChar(c);
		if(c < 33)
		{
			x += 5;
		}
		else
		{
			p = W_CacheLumpNum(FontABaseLump+c-33, PU_CACHE);
			GL_DrawPatch_CS(x, y, FontABaseLump+c-33);
			x += p->width-1;
		}
	}
}

void MN_DrTextA(char *text, int x, int y)
{
	GL_SetColorAndAlpha(1, 1, 1, 1);
	MN_DrTextA_CS(text, x, y);
}

//==========================================================================
//
// MN_DrTextAYellow
//
//==========================================================================

void MN_DrTextAYellow_CS(char *text, int x, int y)
{
	char c;
	patch_t *p;

	while((c = *text++) != 0)
	{
		c = MN_FilterChar(c);
		if(c < 33)
		{
			x += 5;
		}
		else
		{
			p = W_CacheLumpNum(FontAYellowBaseLump+c-33, PU_CACHE);
			GL_DrawPatch_CS(x, y, FontAYellowBaseLump+c-33);
			x += p->width-1;
		}
	}
}

void MN_DrTextAYellow(char *text, int x, int y)
{
	GL_SetColorAndAlpha(1, 1, 1, 1);
	MN_DrTextAYellow_CS(text, x, y);
}

//---------------------------------------------------------------------------
//
// FUNC MN_TextAWidth
//
// Returns the pixel width of a string using font A.
//
//---------------------------------------------------------------------------

int MN_TextAWidth(char *text)
{
	char c;
	int width;
	patch_t *p;

	width = 0;
	while((c = *text++) != 0)
	{
		c = MN_FilterChar(c);
		if(c < 33)
		{
			width += 5;
		}
		else
		{
			p = W_CacheLumpNum(FontABaseLump+c-33, PU_CACHE);
			width += p->width-1;
		}
	}
	return(width);
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextB
//
// Draw text using font B.
//
//---------------------------------------------------------------------------

void MN_DrTextB_CS(char *text, int x, int y)
{
	char c;
	patch_t *p;

	while((c = *text++) != 0)
	{
		c = MN_FilterChar(c);
		if(c < 33)
		{
			x += 8;
		}
		else
		{
			p = W_CacheLumpNum(FontBBaseLump+c-33, PU_CACHE);
			GL_DrawPatch_CS(x, y, FontBBaseLump+c-33);
			x += p->width-1;
		}
	}
}

void MN_DrTextB(char *text, int x, int y)
{
	GL_SetColorAndAlpha(1, 1, 1, 1);
	MN_DrTextB_CS(text, x, y);
}

//---------------------------------------------------------------------------
//
// FUNC MN_TextBWidth
//
// Returns the pixel width of a string using font B.
//
//---------------------------------------------------------------------------

int MN_TextBWidth(char *text)
{
	char c;
	int width;
	patch_t *p;

	width = 0;
	while((c = *text++) != 0)
	{
		c = MN_FilterChar(c);
		if(c < 33)
		{
			width += 5;
		}
		else
		{
			p = W_CacheLumpNum(FontBBaseLump+c-33, PU_CACHE);
			width += p->width-1;
		}
	}
	return(width);
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawTitle
//
//---------------------------------------------------------------------------

void MN_DrawTitle(char *text, int y)
{
	MN_DrTextB_CS(text, 160 - MN_TextBWidth(text)/2, y);
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawMenuItem
//
//---------------------------------------------------------------------------

void MN_DrawMenuText(Menu_t *menu, int index, char *text)
{
	int max = 0, off = 0;
	int i;
	char *str;

	for(i = 0; i < menu->itemCount; i++)
	{
		str = menu->items[i].text;
		if(!str) continue;
		if(!strchr(str, ':')) continue;
		if(menu->textDrawer == MN_DrTextB_CS)
			off = MN_TextBWidth(str) + 16;
		else
			off = MN_TextAWidth(str) + 8;
		if(off > max) max = off;
	}
	menu->textDrawer(text, menu->x + max, menu->y + menu->itemHeight*index);
}

//---------------------------------------------------------------------------
//
// PROC MN_Ticker
//
//---------------------------------------------------------------------------

void MN_Ticker(void)
{
/*	if(Get(DD_PLAYBACK) && actions[A_STOPDEMO].on)
	{
		G_CheckDemoStatus();
	}*/
	if(MenuActive == false)
	{
		if(bgAlpha > 0) 
		{
			bgAlpha -= .5/(float)menuDarkTicks;
			if(bgAlpha < 0) bgAlpha = 0;
		}
		if(fadingOut)
		{
			outFade += 1/(float)slamInTicks;
			if(outFade > 1) fadingOut = false;
		}
		return;
	}
	MenuTime++;

	// The extended ticker handles multiplayer menu stuff.
	MN_TickerEx();
}

//==========================================================================
//
// DrawMessage
//
//==========================================================================

static void DrawMessage(void)
{
	player_t *player;

	player = &players[consoleplayer];
	if(player->messageTics <= 0 || !player->message)
	{ // No message
		return;
	}
	if(player->yellowMessage)
	{
		MN_DrTextAYellow(player->message, 
			160-MN_TextAWidth(player->message)/2, 1);
	}
	else
	{
		MN_DrTextA(player->message, 160-MN_TextAWidth(player->message)/2, 1);
	}
}



char *QuitEndMsg[] =
{
	"ARE YOU SURE YOU WANT TO QUIT?",
	"ARE YOU SURE YOU WANT TO END THE GAME?",
	"DO YOU WANT TO QUICKSAVE THE GAME NAMED",
	"DO YOU WANT TO QUICKLOAD THE GAME NAMED",
	"ARE YOU SURE YOU WANT TO SUICIDE?"
};


#define BETA_FLASH_TEXT "BETA"

float MN_GL_SetupState(float time, float offset)
{
	float alpha;

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	if(time > 1 && time <= 2)
	{
		time = 2-time;
		gl.Translatef(160, 100, 0);
		gl.Scalef(cfg.menuScale * (.9f + time*.1f), 
			cfg.menuScale * (.9f+time*.1f), 1);
		gl.Translatef(-160, -100, 0);
		gl.Color4f(1, 1, 1, alpha = time);
	}
	else
	{
		gl.Translatef(160, 100, 0);
		gl.Scalef(cfg.menuScale * (2-time), cfg.menuScale * (2-time), 1);
		gl.Translatef(-160, -100, 0);
		gl.Color4f(1, 1, 1, alpha = time*time);
	}
	gl.Translatef(0, -offset, 0);
	return alpha;
}

void MN_GL_RestoreState()
{
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

//---------------------------------------------------------------------------
//
// PROC MN_Drawer
//
//---------------------------------------------------------------------------

void MN_Drawer(void)
{
	int i;
	int x;
	int y;
	MenuItem_t *item;
	char *selName;
	
	DrawMessage();

	// FPS.
	if(cfg.showFPS)
	{
		char fpsbuff[80];
		sprintf(fpsbuff, "%d FPS", DD_GetFrameRate());
		MN_DrTextA(fpsbuff, 320-MN_TextAWidth(fpsbuff), 0);
		GL_Update(DDUF_TOP);
	}
	
/*#ifdef USEA3D
	{
		char tbuff[80];
		extern int numBuffers, snd_Channels;
		sprintf(tbuff, "CH:%d / BF:%d", snd_Channels, numBuffers);
		MN_DrTextA(tbuff, 210, 9);
	}
#endif*/

#ifdef TIMEBOMB
	// Beta blinker ***
	if(leveltime&16)
	{
		MN_DrTextA( BETA_FLASH_TEXT,
				160-(MN_TextAWidth(BETA_FLASH_TEXT)>>1), 12);
	}
#endif // TIMEBOMB

	if(MenuActive == false)
	{
		if(bgAlpha > 0)
		{
			GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
			GL_SetNoTexture();
			GL_DrawRect(0, 0, 320, 200, 0, 0, 0, bgAlpha);
		}
		if(askforquit)
		{
			MN_DrTextA(QuitEndMsg[typeofask-1], 160-
				MN_TextAWidth(QuitEndMsg[typeofask-1])/2, 80);
			if(typeofask == 3)
			{
				MN_DrTextA(SlotText[quicksave-1], 160-
					MN_TextAWidth(SlotText[quicksave-1])/2, 90);
				MN_DrTextA("?", 160+
					MN_TextAWidth(SlotText[quicksave-1])/2, 90);
			}
			if(typeofask == 4)
			{
				MN_DrTextA(SlotText[quickload-1], 160-
					MN_TextAWidth(SlotText[quickload-1])/2, 90);
				MN_DrTextA("?", 160+
					MN_TextAWidth(SlotText[quicksave-1])/2, 90);
			}
			GL_Update(DDUF_FULLSCREEN);
		}
	}
	if(MenuActive || fadingOut)
	{
		int effTime = (MenuTime>menuDarkTicks)? menuDarkTicks : MenuTime;
		float temp = .5 * effTime/(float)menuDarkTicks;
		float alpha;

		GL_Update(DDUF_FULLSCREEN);
		
		if(!fadingOut)
		{
			if(temp > bgAlpha) bgAlpha = temp;
			effTime = (MenuTime>slamInTicks)? slamInTicks : MenuTime;
			temp = effTime / (float)slamInTicks;
	
			// Draw a dark background. It makes it easier to read the menus.
			GL_SetNoTexture();
			GL_DrawRect(0, 0, 320, 200, 0, 0, 0, bgAlpha);
		}
		else temp = outFade+1;

		alpha = MN_GL_SetupState(temp, CurrentMenu->offset);								

		if(InfoType)
		{
			MN_DrawInfo();
			MN_GL_RestoreState();
			return;
		}
		GL_Update(DDUF_BORDER);

		if(CurrentMenu->drawFunc != NULL)
		{
			CurrentMenu->drawFunc();
		}

		x = CurrentMenu->x;
		y = CurrentMenu->y;
		for(i=0, item=CurrentMenu->items + CurrentMenu->firstItem; 
			i<CurrentMenu->numVisItems && CurrentMenu->firstItem + i < CurrentMenu->itemCount; 
			i++, y += CurrentMenu->itemHeight, item++)
		{
			if(item->type != ITT_EMPTY || item->text)
			{
				// Decide which color to use.
				if(item->type == ITT_EMPTY)
					GL_SetColorAndAlpha(.95f, 0, 0, alpha); // Red for titles.
				else
					GL_SetColorAndAlpha(1, 1, 1, alpha);

				if(item->text)
					CurrentMenu->textDrawer(item->text, x, y);
			}
		}
		// Back to normal color.
		GL_SetColorAndAlpha(1, 1, 1, alpha);
		
		y = CurrentMenu->y+((CurrentItPos-CurrentMenu->firstItem)*CurrentMenu->itemHeight)+SELECTOR_YOFFSET
			- (10-CurrentMenu->itemHeight/2);
		selName = MenuTime&16 ? "M_SLCTR1" : "M_SLCTR2";
		GL_DrawPatch_CS(x+SELECTOR_XOFFSET, y, W_GetNumForName(selName));

		MN_GL_RestoreState();
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawMainMenu
//
//---------------------------------------------------------------------------

static void DrawMainMenu(void)
{
	int frame;

	frame = (MenuTime/5)%7;
	
	GL_DrawPatch_CS(88, 0, W_GetNumForName("M_HTIC"));

// Old Gold skull positions: (40, 10) and (232, 10)
	GL_DrawPatch_CS(37, 80, MauloBaseLump+(frame+2)%7);
	GL_DrawPatch_CS(278, 80, MauloBaseLump+frame);
}

//==========================================================================
//
// DrawClassMenu
//
//==========================================================================

static void DrawClassMenu(void)
{
	pclass_t class;
	static char *boxLumpName[3] =
	{
		"m_fbox",
		"m_cbox",
		"m_mbox"
	};
	static char *walkLumpName[3] =
	{
		"m_fwalk1",
		"m_cwalk1",
		"m_mwalk1"
	};

	MN_DrTextB_CS("CHOOSE CLASS:", 34, 24);
	class = (pclass_t)CurrentMenu->items[CurrentItPos].option;
	GL_DrawPatch_CS(174, 8, W_GetNumForName(boxLumpName[class]));
	GL_DrawPatch_CS(174+24, 8+12,
		W_GetNumForName(walkLumpName[class]) + ((MenuTime>>3) & 3));
}

//---------------------------------------------------------------------------
//
// PROC DrawSkillMenu
//
//---------------------------------------------------------------------------

static void DrawSkillMenu(void)
{
	MN_DrTextB_CS("CHOOSE SKILL LEVEL:", 74, 16);
}

//---------------------------------------------------------------------------
//
// PROC DrawFilesMenu
//
//---------------------------------------------------------------------------

static void DrawFilesMenu(void)
{
// clear out the quicksave/quickload stuff
	quicksave = 0;
	quickload = 0;
	P_ClearMessage(&players[consoleplayer]);
}

//---------------------------------------------------------------------------
//
// PROC DrawLoadMenu
//
//---------------------------------------------------------------------------

static void DrawLoadMenu(void)
{
	MN_DrTextB_CS("LOAD GAME", 160-MN_TextBWidth("LOAD GAME")/2, 10);
	if(!slottextloaded)
	{
		MN_LoadSlotText();
	}
	DrawFileSlots(&LoadMenu);
}

//---------------------------------------------------------------------------
//
// PROC DrawSaveMenu
//
//---------------------------------------------------------------------------

static void DrawSaveMenu(void)
{
	MN_DrTextB_CS("SAVE GAME", 160-MN_TextBWidth("SAVE GAME")/2, 10);
	if(!slottextloaded)
	{
		MN_LoadSlotText();
	}
	DrawFileSlots(&SaveMenu);
}

//===========================================================================
//
// MN_LoadSlotText
//
// For each slot, looks for save games and reads the description field.
//
//===========================================================================

void MN_LoadSlotText(void)
{
	int slot;
	LZFILE *fp;
	char name[100];
	char versionText[HXS_VERSION_TEXT_LENGTH];
	char description[HXS_DESCRIPTION_LENGTH];
	boolean found;

	for(slot = 0; slot < 6; slot++)
	{
		found = false;
		sprintf(name, "%shex%d.hxs", SavePath, slot);
		M_TranslatePath(name, name);
		fp = lzOpen(name, "rp");
		if(fp)
		{
			lzRead(description, HXS_DESCRIPTION_LENGTH, fp);
			lzRead(versionText, HXS_VERSION_TEXT_LENGTH, fp);
			lzClose(fp);
			if(!strcmp(versionText, HXS_VERSION_TEXT))
			{
				found = true;
			}
		}
		if(found)
		{
			memcpy(SlotText[slot], description, SLOTTEXTLEN);
			SlotStatus[slot] = 1;
		}
		else
		{
			memset(SlotText[slot], 0, SLOTTEXTLEN);
			SlotStatus[slot] = 0;
		}
	}
	slottextloaded = true;
}

//---------------------------------------------------------------------------
//
// PROC DrawFileSlots
//
//---------------------------------------------------------------------------

static void DrawFileSlots(Menu_t *menu)
{
	int i;
	int x;
	int y;

	x = menu->x;
	y = menu->y;
	for(i = 0; i < 6; i++)
	{
		GL_DrawPatch_CS(x, y, W_GetNumForName("M_FSLOT"));
		if(SlotStatus[i])
		{
			MN_DrTextA_CS(SlotText[i], x+5, y+5);
		}
		y += menu->itemHeight;
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawOptionsMenu
//
//---------------------------------------------------------------------------

static void DrawOptionsMenu(void)
{
	GL_DrawPatch_CS(88, 0, W_GetNumForName("M_HTIC"));
	MN_DrTextB_CS("OPTIONS", 154-MN_TextBWidth("OPTIONS")/2, 56);
}

//---------------------------------------------------------------------------
//
// PROC DrawOptions2Menu
//
//---------------------------------------------------------------------------

static void DrawOptions2Menu(void)
{
	Menu_t *menu = &Options2Menu;
/*	char *musDevStr[3] = { "NONE", "MIDI", "CD" };
	char *freqStr[4] = { "11 KHZ", "22 KHZ", "INVALID!", "44 KHZ" };
	int temp = (int) (*(float*) Con_GetVariable("s_reverbVol")->ptr * 10 + .5f);
*/
	MN_DrawTitle("SOUND OPTIONS", 4);

	DrawSlider(menu, 1, 18, Get(DD_SFX_VOLUME)/15);
	DrawSlider(menu, 4, 18, Get(DD_MUSIC_VOLUME)/15);
/*#pragma message("mn_menu.c: Music device will be moved to Panel.")
	//DrawSlider(menu, 7, 18, gi.CD(DD_GET_VOLUME,0)/15);
	//MN_DrawMenuText(menu, 9, musDevStr[Get(DD_MUSIC_DEVICE)]);
	MN_DrawMenuText(menu, 10, *(int*) Con_GetVariable("s_3d")->ptr? "ON" : "OFF");
	DrawSlider(menu, 12, 11, temp);
	MN_DrawMenuText(menu, 14, freqStr[*(int*) Con_GetVariable("s_resample")->ptr - 1]);
	MN_DrawMenuText(menu, 15, *(int*) Con_GetVariable("s_16bit")->ptr? "ON" : "OFF");
*/
}

static void SCMusicDevice(int option)
{
/*	int snd_MusicDevice = Get(DD_MUSIC_DEVICE);
	
	if(option == RIGHT_DIR)
	{
		if(snd_MusicDevice < 2) snd_MusicDevice++;
	}
	else if(snd_MusicDevice > 0) snd_MusicDevice--;

	// Setup the music.
	gi.SetMusicDevice(snd_MusicDevice);
	
	// Restart the song of the current map.
	S_StartSong(gamemap, true);*/
}

static void DrawGameplayMenu(void)
{
	Menu_t *menu = &GameplayMenu;
	char *xhairnames[7] = { "NONE", "CROSS", "ANGLES", "SQUARE",
		"OPEN SQUARE", "DIAMOND", "V" };

	MN_DrawTitle("GAMEPLAY OPTIONS", 4);
	MN_DrawMenuText(menu, 0, YesNo[messageson != 0]);
	MN_DrawMenuText(menu, 1, YesNo[cfg.alwaysRun != 0]);
	MN_DrawMenuText(menu, 2, YesNo[cfg.lookSpring != 0]);
	MN_DrawMenuText(menu, 3, YesNo[cfg.noAutoAim != 0]);
	MN_DrawMenuText(menu, 4, YesNo[cfg.showFullscreenMana != 0]);
	MN_DrawMenuText(menu, 5, xhairnames[cfg.xhair]);
	DrawSlider(menu, 7, 9, cfg.xhairSize);
	DrawSlider(menu, 10, 9, cfg.screenblocks-3);
	DrawSlider(menu, 13, 20, cfg.sbarscale-1);
}

/*static void DrawGraphicsMenu(void)
{
	char *mipStr[6] = 
	{
		"N", "L", "N, MIP N", "L, MIP N", "N, MIP L", "L, MIP L"
	};
	char *texQStr[9] =
	{
		"0 - MINIMUM",
		"1 - VERY LOW",
		"2 - LOW",
		"3 - POOR",
		"4 - AVERAGE",
		"5 - GOOD",
		"6 - HIGH",
		"7 - VERY HIGH",
		"8 - MAXIMUM"
	};
	Menu_t *menu = &GraphicsMenu;
	cvar_t *cv = Con_GetVariable("r_texquality");

	MN_DrawTitle("GRAPHICS OPTIONS", 4);

	MN_DrawMenuText(menu, 2, YesNo[CVAR(int,"usemodels") != 0]);
	DrawSlider(menu, 4, 5, Get(DD_SKY_DETAIL)-3);
	MN_DrawMenuText(menu, 6, mipStr[Get(DD_MIPMAPPING)]);
	MN_DrawMenuText(menu, 7, YesNo[Get(DD_SMOOTH_IMAGES) != 0]);
	MN_DrawMenuText(menu, 8, texQStr[*(int*) cv->ptr]);

	// This isn't very good programming, but here we can reset the 
	// current resolution selection.
	selRes = findRes(Get(DD_SCREEN_WIDTH), Get(DD_SCREEN_HEIGHT));
}

static void DrawEffectsMenu(void)
{
	char	*dlblendStr[4] = { "MULTIPLY", "ADD", "NONE", "DON'T RENDER" };
	char	*flareStr[6] = { "OFF", "1", "2", "3", "4", "5" };
	char	*alignStr[4] = { "CAMERA", "VIEW PLANE", "CAMERA (R)", "VIEW PLANE (R)" };
	Menu_t	*menu = &EffectsMenu;
	int		x = menu->x + 140, y = menu->y, h = menu->itemHeight;
	int		temp;

	MN_DrawTitle("GRAPHICS EFFECTS", 4);

	MN_DrawMenuText(menu, 0, YesNo[cfg.showFPS!=0]);
	MN_DrawMenuText(menu, 1, cfg.translucentIceCorpse? "TRANSLUCENT" : "OPAQUE");
	MN_DrawMenuText(menu, 2, YesNo[CVAR(int,"dynlights")!=0]);
	MN_DrawMenuText(menu, 3, dlblendStr[CVAR(int,"dlblend")]);
	MN_DrawMenuText(menu, 4, YesNo[CVAR(int,"sprlight")!=0]);
	temp = (int) (CVAR(float, "dlfactor") * 10 + .5f);
	DrawSlider(menu, 6, 11, temp);
	MN_DrawMenuText(menu, 8, flareStr[CVAR(int, "flares")]);
	DrawSlider(menu, 10, 11, CVAR(int, "flareintensity") / 10);
	DrawSlider(menu, 13, 11, CVAR(int, "flaresize"));
	MN_DrawMenuText(menu, 15, alignStr[CVAR(int,"spralign")]);
	MN_DrawMenuText(menu, 16, YesNo[CVAR(int,"sprblend")!=0]);
}
*/
/*static void DrawResolutionMenu(void)
{
	char buffer[40];
	Menu_t *menu = &ResolutionMenu;
	
	if(selRes == -1)
		strcpy(buffer, "NOT AVAILABLE");
	else
	{
		sprintf(buffer, "%d X %d%s",
			resolutions[selRes].width,
			resolutions[selRes].height,
			selRes == findRes(Get(DD_DEFAULT_RES_X), Get(DD_DEFAULT_RES_Y))? 
				" (DEFAULT)" : "");			
	}
	MN_DrTextA_CS(buffer, menu->x+8, menu->y+menu->itemHeight+4);
}
*/
/*static void SCResSelector(int option)
{
	if(option == RIGHT_DIR)
	{
		if(resolutions[selRes+1].width)
			selRes++;
	}
	else if(selRes > 0) selRes--;
}

static void SCResMakeCurrent(int option)
{
	if(GL_ChangeResolution(resolutions[selRes].width, 
		resolutions[selRes].height, 0))
		P_SetMessage(&players[consoleplayer], "RESOLUTION CHANGED", true);
	else
		P_SetMessage(&players[consoleplayer], "RESOLUTION CHANGE FAILED", true);
}

static void SCResMakeDefault(int option)
{
	Set(DD_DEFAULT_RES_X, resolutions[selRes].width);
	Set(DD_DEFAULT_RES_Y, resolutions[selRes].height);
}*/

static void SCLookSpring(int option)
{
	cfg.lookSpring ^= 1;
	if(cfg.lookSpring)
		P_SetMessage(&players[consoleplayer], "USING LOOKSPRING", true);
	else
		P_SetMessage(&players[consoleplayer], "NO LOOKSPRING", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCAutoAim(int option)
{
	P_SetMessage(&players[consoleplayer], (cfg.noAutoAim^=1)? "NO AUTOAIM" : "AUTOAIM ON", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCFullscreenMana(int option)
{
	cfg.showFullscreenMana = !cfg.showFullscreenMana;
	if(cfg.showFullscreenMana)
	{
		P_SetMessage(&players[consoleplayer], "MANA SHOWN IN FULLSCREEN VIEW", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "NO MANA IN FULLSCREEN VIEW", true);
	}
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCCrosshair(int option)
{
	cfg.xhair += option==RIGHT_DIR? 1 : -1;
	if(cfg.xhair < 0) cfg.xhair = 0;
	if(cfg.xhair > NUM_XHAIRS) cfg.xhair = NUM_XHAIRS;
}

static void SCCrosshairSize(int option)
{
	cfg.xhairSize += option==RIGHT_DIR? 1 : -1;
	if(cfg.xhairSize < 0) cfg.xhairSize = 0;
	if(cfg.xhairSize > 9) cfg.xhairSize = 9;
}

static void SCSkyDetail(int option)
{
	int skyDetail = Get(DD_SKY_DETAIL);

	if(option == RIGHT_DIR)
	{
		if(skyDetail < 7)
		{
			skyDetail++;
		}
	}
	else if(skyDetail > 3)
	{
		skyDetail--;
	}
	Rend_SkyParams(DD_SKY, DD_COLUMNS, skyDetail);
}

static void SCMipmapping(int option)
{
	int mipmapping = Get(DD_MIPMAPPING);

	if(option == RIGHT_DIR)
	{
		if(mipmapping < 5) mipmapping++;
	}
	else if(mipmapping > 0) mipmapping--;

	GL_TextureFilterMode(DD_TEXTURES, mipmapping);
}

static void SCLinearRaw(int option)
{
	int linearRaw = Get(DD_SMOOTH_IMAGES);

	linearRaw ^= 1;
	if(linearRaw)
	{
		P_SetMessage(&players[consoleplayer], "GRAPHICS SCREENS USE LINEAR INTERPOLATION", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "GRAPHICS SCREENS AREN'T INTERPOLATED", true);
	}
	S_LocalSound(SFX_CHAT, NULL);
	GL_TextureFilterMode(DD_RAWSCREENS, linearRaw);
}

/*static void SCBorderUpdate(int option)
{
	cvar_t	*cv = Con_GetVariable("borderupd");

	P_SetMessage(&players[consoleplayer], (*(int*) cv->ptr ^= 1)? 
		"BORDERS UPDATED EVERY FRAME" : "BORDERS UPDATED WHEN NEEDED", true);
	S_LocalSound(SFX_CHAT, NULL);
}*/

static void ChangeIntCVar(char *name, int delta)
{
	cvar_t	*cv = Con_GetVariable(name);
	int		val = *(int*) cv->ptr;

	val += delta;
	if(val > cv->max) val = cv->max;
	if(val < cv->min) val = cv->min;
	*(int*) cv->ptr = val;
}

static void SCTexQuality(int option)
{
	ChangeIntCVar("r_texquality", option==RIGHT_DIR? 1 : -1);
}

static void SCForceTexReload(int option)
{
	Con_Execute("texreset", false);
	P_SetMessage(&players[consoleplayer], "ALL TEXTURES DELETED", true);
}

static void SCFPSCounter(int option)
{
	cfg.showFPS ^= 1;
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCIceCorpse(int option)
{
	P_SetMessage(&players[consoleplayer], (cfg.translucentIceCorpse ^= 1)? 
		"FROZEN MONSTERS ARE NOW TRANSLUCENT" : "FROZEN MONSTERS NOT TRANSLUCENT", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCDynLights(int option)
{
	cvar_t	*cvDL = Con_GetVariable("dynlights");

	P_SetMessage(&players[consoleplayer], (*(int*) cvDL->ptr ^= 1)? 
		"DYNAMIC LIGHTS ENABLED" : "DYNAMIC LIGHTS DISABLED", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCDLBlend(int option)
{
	ChangeIntCVar("dlblend", option==RIGHT_DIR? 1 : -1);
}

static void SCDLIntensity(int option)
{
	cvar_t	*cv = Con_GetVariable("dlfactor");
	float	val = *(float*) cv->ptr;

	val += option==RIGHT_DIR? .1f : -.1f;
	if(val > cv->max) val = cv->max;
	if(val < cv->min) val = cv->min;
	*(float*) cv->ptr = val;
}

static void SCFlares(int option)
{
	ChangeIntCVar("flares", option==RIGHT_DIR? 1 : -1);
}

static void SCFlareIntensity(int option)
{
	ChangeIntCVar("flareintensity", option==RIGHT_DIR? 10 : -10);
}

static void SCFlareSize(int option)
{
	ChangeIntCVar("flaresize", option==RIGHT_DIR? 1 : -1);
}

static void SCSpriteAlign(int option)
{
/*	cvar_t *cv = Con_GetVariable("spralign");

	P_SetMessage(&players[consoleplayer], (*(int*) cv->ptr ^= 1)? 
		"SPRITES ALIGNED TO VIEW PLANE" : "SPRITES ALIGNED TO CAMERA", true);
	S_LocalSound(SFX_CHAT, NULL);*/

	ChangeIntCVar("spralign", option==RIGHT_DIR? 1 : -1);
}

static void SCSpriteBlending(int option)
{
	cvar_t *cv = Con_GetVariable("sprblend");

	P_SetMessage(&players[consoleplayer], (*(int*) cv->ptr ^= 1)? 
		"ADDITIVE BLENDING FOR EXPLOSIONS" : "NO SPRITE BLENDING", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCSpriteLight(int option)
{
	cvar_t *cv = Con_GetVariable("sprlight");

	P_SetMessage(&players[consoleplayer], (*(int*) cv->ptr ^= 1)? 
		"SPRITES LIT BY DYNAMIC LIGHTS" : "SPRITES NOT LIGHT BY LIGHTS", true);
	S_LocalSound(SFX_CHAT, NULL);
}

//---------------------------------------------------------------------------
//
// PROC DrawMouseOptsMenu
//
//---------------------------------------------------------------------------

static void DrawMouseOptsMenu(void)
{
	Menu_t *menu = &MouseOptsMenu;

	MN_DrawTitle("MOUSE OPTIONS", 4);
//	MN_DrawMenuText(menu, 0, YesNo[Get(DD_MOUSE_INVERSE_Y)!=0]);
	MN_DrawMenuText(menu, 0, YesNo[cfg.usemlook!=0]);
	MN_DrawMenuText(menu, 1, YesNo[cfg.mlookInverseY!=0]);
	DrawSlider(&MouseOptsMenu, 3, 18, cfg.mouseSensiX);
	DrawSlider(&MouseOptsMenu, 6, 18, cfg.mouseSensiY);
}

static void SCControlConfig(int option)
{
	if(grabbing != NULL) Con_Error("SCControlConfig: grabbing is not NULL!!!\n");
	
	grabbing = controls + option;
}

void spacecat(char *str, const char *catstr)
{
	if(str[0]) strcat(str, " ");
	
	// Also do some filtering.
	switch(catstr[0])
	{
	case '\\':
		strcat(str, "bkslash");
		break;
	
	case '[':
		strcat(str, "sqbtopen");
		break;

	case ']':
		strcat(str, "sqbtclose");
		break;

	default:
		strcat(str, catstr);
	}
}

static void DrawControlsMenu(void)
{
	int			i, k;
	char		controlCmd[80];
	char		buff[80], prbuff[80], *token;
	Menu_t		*menu = CurrentMenu;
	MenuItem_t	*item = menu->items + menu->firstItem;
	Control_t	*ctrl;

	MN_DrTextB_CS("CONTROLS", 120, 4);

	// Draw the page arrows.
	token = (!menu->firstItem || MenuTime&8)? "invgeml2" : "invgeml1";
	GL_DrawPatch_CS(menu->x, menu->y-16, W_GetNumForName(token));
	token = (menu->firstItem+menu->numVisItems >= menu->itemCount || MenuTime&8)? 
		"invgemr2" : "invgemr1";
	GL_DrawPatch_CS(312-menu->x, menu->y-16, W_GetNumForName(token));

	for(i=0; i<menu->numVisItems && menu->firstItem+i < menu->itemCount; i++, item++)
	{
		if(item->type == ITT_EMPTY) continue;
		
		ctrl = controls + item->option;
		strcpy(buff, "");
		if(ctrl->flags & CLF_ACTION)
			sprintf(controlCmd, "+%s", ctrl->command);
		else
			strcpy(controlCmd, ctrl->command);
		// Let's gather all the bindings for this command.
		if(!B_BindingsForCommand(controlCmd, buff))
			strcpy(buff, "NONE");
	
		// Now we must interpret what the bindings string says.
		// It may contain characters we can't print.
		strcpy(prbuff, "");
		token = strtok(buff, " ");
		while(token)
		{
			if(token[0] == '+')
				spacecat(prbuff, token+1);
			if((token[0] == '*' && !(ctrl->flags & CLF_REPEAT)) ||
			   token[0] == '-')	spacecat(prbuff, token);
			token = strtok(NULL, " ");
		}
		strupr(prbuff);
		for(k=0; prbuff[k]; k++)
			if(prbuff[k] < 32 || prbuff[k] > 'Z')
				prbuff[k] = ' ';

		if(grabbing == ctrl)
		{
			// We're grabbing for this control.
			spacecat(prbuff, "...");
		}

		MN_DrTextAYellow_CS(prbuff, menu->x+134, menu->y + i*menu->itemHeight);
	}
}

static void SCJoySensi(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.joySensitivity < 9) cfg.joySensitivity++;
	}
	else if(cfg.joySensitivity > 1) cfg.joySensitivity--;
}

static void DrawJoyConfigMenu()
{
	char *axisname[5] = { "-", "MOVE", "TURN", "STRAFE", "LOOK" };
	Menu_t	*menu = &JoyConfigMenu;

	MN_DrawTitle("JOYSTICK OPTIONS", 4);

	MN_DrawMenuText(menu, 0, axisname[cfg.joyaxis[0]]);
	MN_DrawMenuText(menu, 1, axisname[cfg.joyaxis[1]]);
	MN_DrawMenuText(menu, 2, axisname[cfg.joyaxis[2]]);
	MN_DrawMenuText(menu, 3, axisname[cfg.joyaxis[3]]);
	MN_DrawMenuText(menu, 4, axisname[cfg.joyaxis[4]]);
	MN_DrawMenuText(menu, 5, axisname[cfg.joyaxis[5]]);
	MN_DrawMenuText(menu, 6, axisname[cfg.joyaxis[6]]);
	MN_DrawMenuText(menu, 7, axisname[cfg.joyaxis[7]]);
	MN_DrawMenuText(menu, 8, YesNo[cfg.usejlook]);
	MN_DrawMenuText(menu, 9, YesNo[cfg.jlookInverseY]);
	MN_DrawMenuText(menu, 10, YesNo[cfg.povLookAround]);
}

//---------------------------------------------------------------------------
//
// PROC SCQuitGame
//
//---------------------------------------------------------------------------

static void SCQuitGame(int option)
{
	Con_Open(false);
	MenuActive = false;
	askforquit = true;
	typeofask = 1; //quit game
	if(!IS_NETGAME && !Get(DD_PLAYBACK)) paused = true;
}

//---------------------------------------------------------------------------
//
// PROC SCEndGame
//
//---------------------------------------------------------------------------

static void SCEndGame(int option)
{
	if(Get(DD_PLAYBACK))
	{
		return;
	}
	if(SCNetCheck(3))
	{
		MenuActive = false;
		askforquit = true;
		typeofask = 2; //endgame
		if(!IS_NETGAME && !Get(DD_PLAYBACK))
		{
			paused = true;
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC SCMessages
//
//---------------------------------------------------------------------------

static void SCMessages(int option)
{
	messageson ^= 1;
	if(messageson)
	{
		P_SetMessage(&players[consoleplayer], "MESSAGES ON", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "MESSAGES OFF", true);
	}
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCAlwaysRun(int option)
{
	cfg.alwaysRun ^= 1;
	if(cfg.alwaysRun)
	{
		P_SetMessage(&players[consoleplayer], "ALWAYS RUNNING", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "NORMAL RUNNING", true);
	}
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCMouseLook(int option)
{
//	extern int mousepresent;

	cfg.usemlook ^= 1;
	if(cfg.usemlook)
	{
		// Make sure the mouse is initialized.
/*		usemouse = 1;
		if(!mousepresent) I_StartupMouse();
		if(!mousepresent)
			P_SetMessage(&players[consoleplayer], "MOUSE INIT FAILED", true);
		else*/
		P_SetMessage(&players[consoleplayer], "MOUSE LOOK ON", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "MOUSE LOOK OFF", true);
	}
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCJoyLook(int option)
{
	P_SetMessage(&players[consoleplayer], 
		(cfg.usejlook^=1)? "JOYSTICK LOOK ON" : "JOYSTICK LOOK OFF", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCPOVLook(int option)
{
	P_SetMessage(&players[consoleplayer], (cfg.povLookAround^=1)? "POV LOOK ON" : "POV LOOK OFF", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCInverseJoyLook(int option)
{
	P_SetMessage(&players[consoleplayer], (cfg.jlookInverseY^=1)? "INVERSE JOYLOOK" : "NORMAL JOYLOOK", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCEnableJoy(int option)
{
	CVAR(int, "i_usejoystick") ^= 1;
}

static void SCJoyAxis(int option)
{
	if(option & RIGHT_DIR)
	{
		if(cfg.joyaxis[option >> 8] < 4) cfg.joyaxis[option >> 8]++;
	}
	else
	{
		if(cfg.joyaxis[option >> 8] > 0) cfg.joyaxis[option >> 8]--;
	}
}

/*static void SCYAxis(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.joyaxis[1] < 4) cfg.joyaxis[1]++;
	}
	else
	{
		if(cfg.joyaxis[1] > 0) cfg.joyaxis[1]--;
	}
}

static void SCZAxis(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.joyaxis[2] < 4) cfg.joyaxis[2]++;
	}
	else
	{
		if(cfg.joyaxis[2] > 0) cfg.joyaxis[2]--;
	}
}*/

static void SCMouseLookInverse(int option)
{
	cfg.mlookInverseY ^= 1;
	if(cfg.mlookInverseY)
		P_SetMessage(&players[consoleplayer], "INVERSE MOUSE LOOK", true);
	else
		P_SetMessage(&players[consoleplayer], "NORMAL MOUSE LOOK", true);
	S_LocalSound(SFX_CHAT, NULL);
}

//===========================================================================
//
// SCNetCheck
//
//===========================================================================

static boolean SCNetCheck(int option)
{
	if(!netgame)
	{
		return true;
	}
	switch(option)
	{
		case 1: // new game
			P_SetMessage(&players[consoleplayer],
				"YOU CAN'T START A NEW GAME IN NETPLAY!", true);
			break;
		case 2: // load game
			if(!IS_CLIENT) return true;
			P_SetMessage(&players[consoleplayer],
				"YOU CAN'T LOAD A GAME IN NETPLAY!", true);
			break;
		case 3: // end game
			P_SetMessage(&players[consoleplayer],
				"YOU CAN'T END A GAME IN NETPLAY!", true);
			break;
		case 4: // save game
			if(!IS_CLIENT) return true;
			P_SetMessage(&players[consoleplayer],
				"YOU CAN'T SAVE A GAME IN NETPLAY!", true);
			break;
	}
	MenuActive = false;
	S_LocalSound(SFX_CHAT, NULL);
	return false;
}

//===========================================================================
//
// SCNetCheck2
//
//===========================================================================

static void SCNetCheck2(int option)
{
	SCNetCheck(option);
	return;
}

//---------------------------------------------------------------------------
//
// PROC SCLoadGame
//
//---------------------------------------------------------------------------

static void SCLoadGame(int option)
{
	if(!SlotStatus[option])
	{ // Don't try to load from an empty slot
		return;
	}
	// Update save game menu position.
	SaveMenu.oldItPos = option;

	G_LoadGame(option);
	MN_DeactivateMenu();
	//BorderNeedRefresh = true;
	GL_Update(DDUF_BORDER);
	if(quickload == -1)
	{
		quickload = option+1;
		P_ClearMessage(&players[consoleplayer]);
	}
}

//---------------------------------------------------------------------------
//
// PROC SCSaveGame
//
//---------------------------------------------------------------------------

static void SCSaveGame(int option)
{
	char *ptr;

	// Can't save if not in a level.
	if(IS_CLIENT
		|| Get(DD_PLAYBACK) 
		|| gamestate != GS_LEVEL) return; 

	if(!FileMenuKeySteal)
	{
		FileMenuKeySteal = true;
		strcpy(oldSlotText, SlotText[option]);
		ptr = SlotText[option];
		while(*ptr)
		{
			ptr++;
		}
		*ptr = '_';
		*(ptr+1) = 0;
		SlotStatus[option]++;
		currentSlot = option;
		slotptr = ptr-SlotText[option];
		return;
	}
	else
	{
		G_SaveGame(option, SlotText[option]);
		FileMenuKeySteal = false;
		MN_DeactivateMenu();
		// Update save game menu position.
		LoadMenu.oldItPos = option;
	}
	//BorderNeedRefresh = true;
	GL_Update(DDUF_BORDER);
	if(quicksave == -1)
	{
		quicksave = option+1;
		P_ClearMessage(&players[consoleplayer]);
	}
}

//==========================================================================
//
// SCClass
//
//==========================================================================

static void SCClass(int option)
{
	if(netgame)
	{
		P_SetMessage(&players[consoleplayer],
			"YOU CAN'T START A NEW GAME FROM WITHIN A NETGAME!", true);
		return;
	}
	MenuPClass = option;
	switch(MenuPClass)
	{
		case PCLASS_FIGHTER:
			SkillMenu.x = 120;
			SkillItems[0].text = "SQUIRE";
			SkillItems[1].text = "KNIGHT";
			SkillItems[2].text = "WARRIOR";
			SkillItems[3].text = "BERSERKER";
			SkillItems[4].text = "TITAN";
			break;
		case PCLASS_CLERIC:
			SkillMenu.x = 116;
			SkillItems[0].text = "ALTAR BOY";
			SkillItems[1].text = "ACOLYTE";
			SkillItems[2].text = "PRIEST";
			SkillItems[3].text = "CARDINAL";
			SkillItems[4].text = "POPE";
			break;
		case PCLASS_MAGE:
			SkillMenu.x = 112;
			SkillItems[0].text = "APPRENTICE";
			SkillItems[1].text = "ENCHANTER";
			SkillItems[2].text = "SORCERER";
			SkillItems[3].text = "WARLOCK";
			SkillItems[4].text = "ARCHIMAGE";
			break;
	}
	SetMenu(MENU_SKILL);
}

//---------------------------------------------------------------------------
//
// PROC SCSkill
//
//---------------------------------------------------------------------------

static void SCSkill(int option)
{
	extern int SB_state;

	cfg.PlayerClass[consoleplayer] = MenuPClass;
	G_DeferredNewGame(option);
	SB_SetClassData();
	SB_state = -1;
	MN_DeactivateMenu();
}

static void SCOpenDCP(int option)
{
	Con_Execute(option? "panel audio" : "panel", true);
}

//---------------------------------------------------------------------------
//
// PROC SCMouseXSensi
//
//---------------------------------------------------------------------------

static void SCMouseXSensi(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.mouseSensiX < 17)
		{
			cfg.mouseSensiX++;
		}
	}
	else if(cfg.mouseSensiX)
	{
		cfg.mouseSensiX--;
	}
}

//---------------------------------------------------------------------------
//
// PROC SCMouseYSensi
//
//---------------------------------------------------------------------------

static void SCMouseYSensi(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.mouseSensiY < 17)
		{
			cfg.mouseSensiY++;
		}
	}
	else if(cfg.mouseSensiY)
	{
		cfg.mouseSensiY--;
	}
}

//---------------------------------------------------------------------------
//
// PROC SCSfxVolume
//
//---------------------------------------------------------------------------

static void SCSfxVolume(int option)
{
	int vol = Get(DD_SFX_VOLUME);

	vol += option==RIGHT_DIR? 15 : -15;
	if(vol < 0) vol = 0;
	if(vol > 255) vol = 255;
	//soundchanged = true; // we'll set it when we leave the menu
	Set(DD_SFX_VOLUME, vol);
}

//---------------------------------------------------------------------------
//
// PROC SCMusicVolume
//
//---------------------------------------------------------------------------

static void SCMusicVolume(int option)
{
	int vol = Get(DD_MUSIC_VOLUME);

	vol += option==RIGHT_DIR? 15 : -15;
	if(vol < 0) vol = 0;
	if(vol > 255) vol = 255;
	Set(DD_MUSIC_VOLUME, vol);
}

//---------------------------------------------------------------------------
//
// PROC SCCDVolume
//
//---------------------------------------------------------------------------

static void SCCDVolume(int option)
{
/*	int vol = gi.CD(DD_GET_VOLUME, 0);

	vol += option==RIGHT_DIR? 15 : -15;
	gi.CD(DD_SET_VOLUME, vol);*/
}

static void SC3DSounds(int option)
{
	P_SetMessage(&players[consoleplayer], (CVAR(int,"s_3d") ^= 1)? 
		"3D SOUND MODE" : "2D SOUND MODE", true);
	S_LocalSound(SFX_CHAT, NULL);
}

static void SCReverbVolume(int option)
{
/*	extern sector_t *listenerSector;
	cvar_t	*cv = Con_GetVariable("s_reverbVol");
	float	val = *(float*) cv->ptr;

	val += option==RIGHT_DIR? .1f : -.1f;
	if(val > cv->max) val = cv->max;
	if(val < cv->min) val = cv->min;
	*(float*) cv->ptr = val;

	// This'll force the sound updater to set the reverb.
	listenerSector = NULL;*/
}

static void SCSfxFrequency(int option)
{
/*	cvar_t	*cv = Con_GetVariable("s_resample");
	int		oldval = *(int*) cv->ptr, val = oldval;
	
	val += option==RIGHT_DIR? 1 : -1;
	if(val > 4) val = 4;
	if(val < 1) val = 1;
	if(val == 3)
	{
		if(oldval == 4) 
			val = 2; 
		else 
			val = 4;
	}
	*(int*) cv->ptr = val;*/
}

static void SCSfx16bit(int option)
{
	/*cvar_t	*cv = Con_GetVariable("s_16bit");

	P_SetMessage(&players[consoleplayer], (*(int*) cv->ptr ^= 1)? 
		"16 BIT INTERPOLATION" : "8 BIT INTERPOLATION", true);
	S_LocalSound(SFX_CHAT, NULL);*/
}

//---------------------------------------------------------------------------
//
// PROC SCScreenSize
//
//---------------------------------------------------------------------------

static void SCScreenSize(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.screenblocks < 11)
		{
			cfg.screenblocks++;
		}
	}
	else if(cfg.screenblocks > 3)
	{
		cfg.screenblocks--;
	}
	R_SetViewSize(cfg.screenblocks, 0);//detailLevel);
}

//---------------------------------------------------------------------------
//
// PROC SCStatusBarSize
//
//---------------------------------------------------------------------------

static void SCStatusBarSize(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.sbarscale < 20) cfg.sbarscale++;
	}
	else 
		if(cfg.sbarscale > 1) cfg.sbarscale--;

	R_SetViewSize(cfg.screenblocks, 0);
}

static void SCUseModels(int option)
{
	CVAR(int, "usemodels") ^= 1;
}

//---------------------------------------------------------------------------
//
// PROC SCInfo
//
//---------------------------------------------------------------------------

static void SCInfo(int option)
{
	InfoType = 1;
	S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
	if(!netgame && !Get(DD_PLAYBACK))
	{
		paused = true;
	}
}

// Set default bindings for unbound Controls.
void H2_DefaultBindings()
{
	int			i;
	Control_t	*ctr;
	char		evname[80], cmd[256], buff[256];
	event_t		event;
	
	// Check all Controls.
	for(i=0; controls[i].command[0]; i++)
	{
		ctr = controls + i;
		// If this command is bound to something, skip it.
		sprintf(cmd, "%s%s", ctr->flags & CLF_ACTION? "+" : "",
			ctr->command);
		if(B_BindingsForCommand(cmd, buff)) continue;

		// This Control has no bindings, set it to the default.
		sprintf(buff, "\"%s\"", ctr->command);
		if(ctr->defKey)
		{
			event.type = ev_keydown;
			event.data1 = ctr->defKey;
			B_EventBuilder(evname, &event, false);
			sprintf(cmd, "%s %s %s", ctr->flags & CLF_REPEAT? "safebindr" : "safebind",
				evname+1, buff);
			Con_Execute(cmd, true);
		}
		if(ctr->defMouse)
		{
			event.type = ev_mousebdown;
			event.data1 = 1 << (ctr->defMouse-1);
			B_EventBuilder(evname, &event, false);
			sprintf(cmd, "%s %s %s", ctr->flags & CLF_REPEAT? "safebindr" : "safebind",
				evname+1, buff);
			Con_Execute(cmd, true);
		}
		if(ctr->defJoy)
		{
			event.type = ev_joybdown;
			event.data1 = 1 << (ctr->defJoy-1);
			B_EventBuilder(evname, &event, false);
			sprintf(cmd, "%s %s %s", ctr->flags & CLF_REPEAT? "safebindr" : "safebind",
				evname+1, buff);
			Con_Execute(cmd, true);
		}
	}
}

//---------------------------------------------------------------------------
//
// FUNC H2_PrivilegedResponder
//
//---------------------------------------------------------------------------

int findtoken(char *string, char *token, char *delim)
{
	char *ptr = strtok(string, delim);
	while(ptr)
	{
		if(!stricmp(ptr, token)) return true;
		ptr = strtok(NULL, delim);
	}
	return false;
}

int H2_PrivilegedResponder(event_t *event)
{
	// We're interested in key or button down events.
	if(grabbing && (event->type == ev_keydown || event->type == ev_mousebdown 
		|| event->type == ev_joybdown || event->type == ev_povdown))
	{
		// We'll grab this event.
		char cmd[256], buff[256], evname[80];
		boolean del = false;

		// Check for a cancel.
		if(event->type == ev_keydown)
		{
			if(event->data1 == '`') // Tilde clears everything.
			{
				if(grabbing->flags & CLF_ACTION)
					sprintf(cmd, "delbind +%s -%s", grabbing->command,
						grabbing->command);
				else
					sprintf(cmd, "delbind \"%s\"", grabbing->command);
				Con_Execute(cmd, true);
				grabbing = NULL;
				return true;
			}
			else if(event->data1 == DDKEY_ESCAPE)
			{
				grabbing = NULL;
				return true;
			}
		}

		// We shall issue a silent console command, but first we need
		// a textual representation of the event.
		B_EventBuilder(evname, event, false); // "Deconstruct" into a name.

		// If this binding already exists, remove it.
		sprintf(cmd, "%s%s", grabbing->flags & CLF_ACTION? "+" : "",
			grabbing->command);
		if(B_BindingsForCommand(cmd, buff))
			if(findtoken(buff, evname, " "))		// Get rid of it?
			{
				del = true;
				strcpy(buff, "");
			}
		if(!del) sprintf(buff, "\"%s\"", grabbing->command);
		sprintf(cmd, "%s %s %s", grabbing->flags & CLF_REPEAT? "bindr" : "bind",
			evname+1, buff);
		Con_Execute(cmd, true);
		// We've finished the grab.
		grabbing = NULL;
		S_LocalSound(SFX_CHAT, NULL);
		return true;
	}

	// Process the screen shot key right away.
	if(ravpic && event->data1 == DDKEY_F1)
	{
		if(event->type == ev_keydown) G_ScreenShot();
		// All F1 events are eaten.
		return true;
	}
	return false; //F_Responder(event);
}

//---------------------------------------------------------------------------
//
// FUNC MN_Responder
//
//---------------------------------------------------------------------------

boolean MN_Responder(event_t *event)
{
	int key;
	int i;
	MenuItem_t *item;
	char *textBuffer;

	if(event->data1 == DDKEY_RSHIFT)
	{
		shiftdown = (event->type == ev_keydown);
	}
	// Edit field responder. In Mn_mplr.c.
	if(Ed_Responder(event)) return true;

	if(event->type != ev_keydown && event->type != ev_keyrepeat)
	{
		return(false);
	}
	key = event->data1;
	if(InfoType)
	{
		if(shareware)
		{
			InfoType = (InfoType+1)%5;
		}
		else
		{
			InfoType = (InfoType+1)%4;
		}
		if(key == DDKEY_ESCAPE)
		{
			InfoType = 0;
		}
		if(!InfoType)
		{
			if(!netgame && !Get(DD_PLAYBACK))
			{
				paused = false;
			}
			MN_DeactivateMenu();
			fadingOut = false;			
			bgAlpha = 0;
			SB_state = -1; //refresh the statbar
			//BorderNeedRefresh = true;
			GL_Update(DDUF_BORDER);
		}
		S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
		return(true); //make the info screen eat the keypress
	}

	if(askforquit)
	{
		switch(key)
		{
			case 'y':
				if(askforquit)
				{
					switch(typeofask)
					{
						case 1:
							//G_CheckDemoStatus(); 
							Sys_Quit();
							break;
						case 2:
							P_ClearMessage(&players[consoleplayer]);
							typeofask = 0;
							askforquit = false;
							paused = false;
							//I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
							//OGL_SetFilter(0);
							GL_SetFilter(0);
							G_StartTitle(); // go to intro/demo mode.
							break;
						case 3:
							P_SetMessage(&players[consoleplayer], 
								"QUICKSAVING....", false);
							FileMenuKeySteal = true;
							SCSaveGame(quicksave-1);
							askforquit = false;
							typeofask = 0;
							//BorderNeedRefresh = true;
							GL_Update(DDUF_BORDER);
							return true;
						case 4:
							P_SetMessage(&players[consoleplayer], 
								"QUICKLOADING....", false);
							SCLoadGame(quickload-1);
							askforquit = false;
							typeofask = 0;
							//BorderNeedRefresh = true;
							GL_Update(DDUF_BORDER);
							return true;
						case 5:
							askforquit = false;
							typeofask = 0;	
							//BorderNeedRefresh = true;
							GL_Update(DDUF_BORDER);
							mn_SuicideConsole = true;
							return true;
							break;
						default:
							return true; // eat the 'y' keypress
					}
				}
				return false;
			case 'n':
			case DDKEY_ESCAPE:
				if(askforquit)
				{
					players[consoleplayer].messageTics = 0;
					askforquit = false;
					typeofask = 0;
					paused = false;
					/*UpdateState |= I_FULLSCRN;
					BorderNeedRefresh = true;*/
					GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
					return true;
				}
				return false;
		}
		return false; // don't let the keys filter thru
	}
	if(MenuActive == false && !chatmodeon)
	{
		switch(key)
		{
/*			case DDKEY_MINUS:
				if(automapactive)
				{ // Don't screen size in automap
					return(false);
				}
				SCScreenSize(LEFT_DIR);
				S_LocalSound(SFX_PICKUP_KEY, NULL);
				GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
				return(true);
			case DDKEY_EQUALS:
				if(automapactive)
				{ // Don't screen size in automap
					return(false);
				}
				SCScreenSize(RIGHT_DIR);
				S_LocalSound(SFX_PICKUP_KEY, NULL);
				GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
				return(true);*/
#ifdef __NeXT__
			case 'q':
				MenuActive = false;
				askforquit = true;
				typeofask = 5; // suicide
				return true;
#endif
#ifndef __NeXT__
#endif
		}

	}

	if(MenuActive == false)
	{
		if(key == DDKEY_ESCAPE /*|| gamestate == GS_DEMOSCREEN */
			|| FI_IsMenuTrigger(event)
			|| Get(DD_PLAYBACK))// && !democam.mode))
		{
			MN_ActivateMenu();
			return(false); // allow bindings (like demostop)
		}
		return(false);
	}
	if(!FileMenuKeySteal)
	{
		int firstVI = CurrentMenu->firstItem;
		int lastVI = firstVI + CurrentMenu->numVisItems-1;
		item = &CurrentMenu->items[CurrentItPos];
		switch(key)
		{
			case DDKEY_DOWNARROW:
				do
				{
					if(CurrentItPos+1 > lastVI)//CurrentMenu->itemCount-1)
					{
						CurrentItPos = firstVI;//0;
					}
					else
					{
						CurrentItPos++;
					}
				} while(CurrentMenu->items[CurrentItPos].type == ITT_EMPTY);
				S_LocalSound(SFX_FIGHTER_HAMMER_HITWALL, NULL);
				return(true);
				break;
			case DDKEY_UPARROW:
				do
				{
					if(CurrentItPos <= firstVI)//0)
					{
						CurrentItPos = lastVI;//CurrentMenu->itemCount-1;
					}
					else
					{
						CurrentItPos--;
					}
				} while(CurrentMenu->items[CurrentItPos].type == ITT_EMPTY);
				S_LocalSound(SFX_FIGHTER_HAMMER_HITWALL, NULL);
				return(true);
				break;
			case DDKEY_LEFTARROW:
				if(item->type == ITT_LRFUNC && item->func != NULL)
				{
					item->func(LEFT_DIR | item->option);
					S_LocalSound(SFX_PICKUP_KEY, NULL);
				}
				else
				{
					// Let's try to change to the previous page.
					if(CurrentMenu->firstItem - CurrentMenu->numVisItems >= 0)
					{
						CurrentMenu->firstItem -= CurrentMenu->numVisItems;
						CurrentItPos -= CurrentMenu->numVisItems;
						// Make a sound, too.
						S_LocalSound(SFX_PICKUP_KEY, NULL);
					}
				}
				return(true);
				break;
			case DDKEY_RIGHTARROW:
				if(item->type == ITT_LRFUNC && item->func != NULL)
				{
					item->func(RIGHT_DIR | item->option);
					S_LocalSound(SFX_PICKUP_KEY, NULL);
				}
				else
				{
					// Move on to the next page, if possible.
					if(CurrentMenu->firstItem + CurrentMenu->numVisItems < 
						CurrentMenu->itemCount)
					{
						CurrentMenu->firstItem += CurrentMenu->numVisItems;
						CurrentItPos += CurrentMenu->numVisItems;
						if(CurrentItPos > CurrentMenu->itemCount-1)
							CurrentItPos = CurrentMenu->itemCount-1;
						S_LocalSound(SFX_PICKUP_KEY, NULL);						
					}
				}
				return(true);
				break;
			case DDKEY_ENTER:
				if(item->type == ITT_SETMENU)
				{
					if(item->func != NULL)	
					{
						item->func(item->option);
					}
					SetMenu(item->menu);
				}
				else if(item->func != NULL)
				{
					CurrentMenu->oldItPos = CurrentItPos;
					if(item->type == ITT_LRFUNC)
					{
						item->func(RIGHT_DIR | item->option);
					}
					else if(item->type == ITT_EFUNC)
					{
						item->func(item->option);
					}
				}
				S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
				return(true);
				break;
			case DDKEY_ESCAPE:
				MN_DeactivateMenu();
				return(true);
			case DDKEY_BACKSPACE:
				S_LocalSound(SFX_PICKUP_KEY, NULL);
				if(CurrentMenu->prevMenu == MENU_NONE)
				{
					MN_DeactivateMenu();
				}
				else
				{
					SetMenu(CurrentMenu->prevMenu);
				}
				return(true);
			default:
				for(i = firstVI; i <= lastVI/*CurrentMenu->itemCount*/; i++)
				{
					if(CurrentMenu->items[i].text && CurrentMenu->items[i].type != ITT_EMPTY)
					{
						if(toupper(key)
							== toupper(CurrentMenu->items[i].text[0]))
						{
							CurrentItPos = i;
							return(true);
						}
					}
				}
				break;
		}
		return(false);
	}
	else
	{ // Editing file names
		textBuffer = &SlotText[currentSlot][slotptr];
		if(key == DDKEY_BACKSPACE)
		{
			if(slotptr)
			{
				*textBuffer-- = 0;
				*textBuffer = ASCII_CURSOR;
				slotptr--;
			}
			return(true);
		}
		if(key == DDKEY_ESCAPE)
		{
			memset(SlotText[currentSlot], 0, SLOTTEXTLEN+2);
			strcpy(SlotText[currentSlot], oldSlotText);
			SlotStatus[currentSlot]--;
			MN_DeactivateMenu();
			return(true);
		}
		if(key == DDKEY_ENTER)
		{
			SlotText[currentSlot][slotptr] = 0; // clear the cursor
			item = &CurrentMenu->items[CurrentItPos];
			CurrentMenu->oldItPos = CurrentItPos;
			if(item->type == ITT_EFUNC)
			{
				item->func(item->option);
				if(item->menu != MENU_NONE)
				{
					SetMenu(item->menu);
				}
			}
			return(true);
		}
		if(slotptr < SLOTTEXTLEN && key != DDKEY_BACKSPACE)
		{
			if((key >= 'a' && key <= 'z'))
			{
				*textBuffer++ = key-32;
				*textBuffer = ASCII_CURSOR;
				slotptr++;
				return(true);
			}
			if(((key >= '0' && key <= '9') || key == ' '
				|| key == ',' || key == '.' || key == '-')
				&& !shiftdown)
			{
				*textBuffer++ = key;
				*textBuffer = ASCII_CURSOR;
				slotptr++;
				return(true);
			}
			if(shiftdown && key == '1')
			{
				*textBuffer++ = '!';
				*textBuffer = ASCII_CURSOR;
				slotptr++;
				return(true);
			}
		}
		return(true);
	}
	return(false);
}

int CCmdMenuAction(int argc, char **argv)
{
	// Can we get out of here early?
	if(/*MenuActive == true || */chatmodeon) return true;

	if(!stricmp(argv[0], "infoscreen"))
	{
		SCInfo(0); // start up info screens
		MenuActive = true;
		fadingOut = false;
	}
	else if(!stricmp(argv[0], "savegame"))
	{
		if(gamestate == GS_LEVEL && !Get(DD_PLAYBACK))
		{
			MenuActive = true;
			fadingOut = false;
			FileMenuKeySteal = false;
			MenuTime = 0;
			CurrentMenu = &SaveMenu;
			CurrentItPos = CurrentMenu->oldItPos;
			if(!netgame && !Get(DD_PLAYBACK))
			{
				paused = true;
			}
			S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
			slottextloaded = false; //reload the slot text, when needed
		}
	}
	else if(!stricmp(argv[0], "loadgame"))
	{
		if(SCNetCheck(2))
		{
			MenuActive = true;
			fadingOut = false;
			FileMenuKeySteal = false;
			MenuTime = 0;
			CurrentMenu = &LoadMenu;
			CurrentItPos = CurrentMenu->oldItPos;
			if(!netgame && !Get(DD_PLAYBACK))
			{
				paused = true;
			}
			S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
			slottextloaded = false; //reload the slot text, when needed
		}
	}
	else if(!stricmp(argv[0], "soundmenu"))
	{
		MenuActive = true;
		fadingOut = false;
		FileMenuKeySteal = false;
		MenuTime = 0;
		CurrentMenu = &Options2Menu;
		CurrentItPos = CurrentMenu->oldItPos;
		if(!netgame && !Get(DD_PLAYBACK))
		{
			paused = true;
		}
		S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
		slottextloaded = false; //reload the slot text, when needed
	}
	else if(!stricmp(argv[0], "suicide"))
	{
		Con_Open(false);
		MenuActive = false;
		askforquit = true;
		typeofask = 5; // suicide
		return true;
	}
	else if(!stricmp(argv[0], "quicksave"))
	{
		if(gamestate == GS_LEVEL && !Get(DD_PLAYBACK))
		{
			if(!quicksave || quicksave == -1)
			{
				MenuActive = true;
				fadingOut = false;
				FileMenuKeySteal = false;
				MenuTime = 0;
				CurrentMenu = &SaveMenu;
				CurrentItPos = CurrentMenu->oldItPos;
				if(!netgame && !Get(DD_PLAYBACK))
				{
					paused = true;
				}
				S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
				slottextloaded = false; //reload the slot text
				quicksave = -1;
				P_SetMessage(&players[consoleplayer],
					"CHOOSE A QUICKSAVE SLOT", true);
			}
			else
			{
				askforquit = true;
				typeofask = 3;
				if(!netgame && !Get(DD_PLAYBACK))
				{
					paused = true;
				}
				S_LocalSound(SFX_CHAT, NULL);
			}
		}
	}
	else if(!stricmp(argv[0], "endgame"))
	{
		if(SCNetCheck(3))
		{
			if(gamestate == GS_LEVEL && !Get(DD_PLAYBACK))
			{
				S_LocalSound(SFX_CHAT, NULL);
				SCEndGame(0);
			}
		}
	}
	else if(!stricmp(argv[0], "toggleMsgs"))
	{
		SCMessages(0);
	}
	else if(!stricmp(argv[0], "quickload"))
	{
		if(SCNetCheck(2))
		{
			if(!quickload || quickload == -1)
			{
				MenuActive = true;
				fadingOut = false;
				FileMenuKeySteal = false;
				MenuTime = 0;
				CurrentMenu = &LoadMenu;
				CurrentItPos = CurrentMenu->oldItPos;
				if(!netgame && !Get(DD_PLAYBACK))
				{
					paused = true;
				}
				S_LocalSound(SFX_DOOR_LIGHT_CLOSE, NULL);
				slottextloaded = false; // reload the slot text
				quickload = -1;
				P_SetMessage(&players[consoleplayer],
					"CHOOSE A QUICKLOAD SLOT", true);
			}
			else
			{
				askforquit = true;
				if(!netgame && !Get(DD_PLAYBACK))
				{
					paused = true;
				}
				typeofask = 4;
				S_LocalSound(SFX_CHAT, NULL);
			}
		}
	}
	else if(!stricmp(argv[0], "quit"))
	{
		if(IS_DEDICATED) 
			Con_Execute("quit!", true);
		else if(gamestate == GS_LEVEL || gamestate == GS_FINALE)
		{
			SCQuitGame(0);
			S_LocalSound(SFX_CHAT, NULL);
		}
	}
	else if(!stricmp(argv[0], "toggleGamma"))
	{
		int gamma = Get(DD_GAMMA) + 1;
		char cmd[20];
		if(gamma > 4)
		{
			gamma = 0;
		}
		sprintf(cmd, "setgamma %d", gamma);
		Con_Execute(cmd, true);
		P_SetMessage(&players[consoleplayer], 
			GET_TXT(TXT_TXT_GAMMA_LEVEL_OFF + gamma), false);
	}
/*	else if(!stricmp(argv[0], 
			case DDKEY_F12: // F12 - reload current map (devmaps mode)
				if(netgame || DevMaps == false)
				{
					return false;
				}
				if(actions[A_SPEED].on)
				{ // Monsters ON
					nomonsters = false;
				}
				if(actions[A_STRAFE].on)
				{ // Monsters OFF
					nomonsters = true;
				}
				G_DeferedInitNew(gameskill, gameepisode, gamemap);
				P_SetMessage(&players[consoleplayer], TXT_CHEATWARP,
					false);
				return true;
*/	
	return true;
}

//---------------------------------------------------------------------------
//
// PROC MN_ActivateMenu
//
//---------------------------------------------------------------------------

void MN_ActivateMenu(void)
{
	if(MenuActive)
	{
		return;
	}
/*	if(paused)
	{
		S_ResumeSound();
	}*/
	MenuActive = true;
	FileMenuKeySteal = false;
	MenuTime = 0;
	fadingOut = false;
	CurrentMenu = &MainMenu;
	CurrentItPos = CurrentMenu->oldItPos;
	if(!netgame && !Get(DD_PLAYBACK))
	{
		paused = true;
	}
	S_LocalSound(SFX_PLATFORM_STOP, NULL);
	slottextloaded = false; //reload the slot text, when needed
}

//---------------------------------------------------------------------------
//
// PROC MN_DeactivateMenu
//
//---------------------------------------------------------------------------

void MN_DeactivateMenu(void)
{
	if(!CurrentMenu) return;

	CurrentMenu->oldItPos = CurrentItPos;
	MenuActive = false;
	if(!netgame)
	{
		paused = false;
	}
	S_LocalSound(SFX_PLATFORM_STOP, NULL);
	P_ClearMessage(&players[consoleplayer]);

	fadingOut = true;
	outFade = 0;
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawInfo
//
//---------------------------------------------------------------------------

void MN_DrawInfo(void)
{
	GL_SetFilter(0);
	GL_DrawRawScreen(W_GetNumForName("TITLE") + InfoType, 0, 0);
}


//---------------------------------------------------------------------------
//
// PROC SetMenu
//
//---------------------------------------------------------------------------

void SetMenu(MenuType_t menu)
{
	CurrentMenu->oldItPos = CurrentItPos;
	CurrentMenu = Menus[menu];
	CurrentItPos = CurrentMenu->oldItPos;
}

//---------------------------------------------------------------------------
//
// PROC DrawSlider
//
//---------------------------------------------------------------------------

static void DrawSlider(Menu_t *menu, int item, int width, int slot)
{
	int		x;
	int		y;

	x = menu->x+24;
	y = menu->y+2+(item*menu->itemHeight);

	// It seems M_SLDMD1 and M_SLDM2 are pretty much identical?
	GL_SetPatch(W_GetNumForName("M_SLDMD1"));
	GL_DrawRectTiled(x-1, y+1, width*8+2, 13, 8, 13);

	GL_DrawPatch_CS(x-32, y, W_GetNumForName("M_SLDLT"));
	GL_DrawPatch_CS(x + width*8, y, W_GetNumForName("M_SLDRT"));
	GL_DrawPatch_CS(x+4+slot*8, y+7, W_GetNumForName("M_SLDKB"));
}

