
// MN_menu.c

#include <ctype.h>
#include "Doomdef.h"
#include "f_infine.h"
#include "P_local.h"
#include "R_local.h"
#include "Soundst.h"
#include "settings.h"
#include "Mn_def.h"

#define NUMSAVESLOTS	8

#define CVAR(typ, x)	(*(typ*)Con_GetVariable(x)->ptr)

// Control flags.
#define CLF_ACTION		0x1		// The control is an action (+/- in front).
#define CLF_REPEAT		0x2		// Bind down + repeat.

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

void S_LevelMusic(void);

static char *yesno[2] = { "NO", "YES" };

// Private Functions
static void InitFonts(void);
void SetMenu(MenuType_t menu);
static boolean SCNetCheck(int option);
static boolean SCQuitGame(int option);
static boolean SCEpisode(int option);
static boolean SCSkill(int option);
static boolean SCSfxVolume(int option);
static boolean SCMusicVolume(int option);
static boolean SCScreenSize(int option);
static boolean SCLoadGame(int option);
static boolean SCSaveGame(int option);
static boolean SCMessages(int option);
static boolean SCEndGame(int option);
static boolean SCInfo(int option);

static boolean SCMouseXSensi(int option);
static boolean SCMouseYSensi(int option);
static boolean SCMouseLook(int option);
static boolean SCMouseLookInverse(int option);
//static boolean SCInverseY(int option);
static boolean SCJoyLook(int option);
static boolean SCPOVLook(int option);
static boolean SCInverseJoyLook(int option);
static boolean SCJoyAxis(int option);
static boolean SCFullscreenMana(int option);
static boolean SCFullscreenArmor(int option);
static boolean SCFullscreenKeys(int option);
static boolean SCLookSpring(int option);
static boolean SCAutoAim(int option);
static boolean SCStatusBarSize(int option);
static boolean SCAlwaysRun(int option);
static boolean SCAllowJump(int option);
static boolean SCControlConfig(int option);
static boolean SCCrosshair(int option);
static boolean SCCrosshairSize(int option);
static boolean SCOpenDCP(int option);
static boolean SCMapKills(int option);
static boolean SCMapItems(int option);
static boolean SCMapSecrets(int option);

static void DrawMainMenu(void);
static void DrawEpisodeMenu(void);
static void DrawSkillMenu(void);
static void DrawOptionsMenu(void);
static void DrawOptions2Menu(void);
static void DrawGameplayMenu(void);
static void DrawHUDMenu(void);
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

// External Data

//boolean F_Responder(event_t *ev);

//extern int detailLevel;
//extern int screenblocks;

// Public Data

boolean MenuActive;
int InfoType;
//boolean messageson = true;
boolean shiftdown;

// Private Data

/*static MenuRes_t resolutions[] =
{
	320, 240,
	640, 480,
	800, 600,
	1024, 768,
	1152, 864,
	1280, 1024,
	1600, 1200,
	0, 0	// The terminator.
};
static int selRes = 0;	// Will be determined when needed.*/

//
// !!! Add new controls to the end, the existing indices must remain unchanged !!!
//
static Control_t controls[] =
{
	// Actions (must be first so the H2A_* constants can be used).
	"left",			CLF_ACTION,		DDKEY_LEFTARROW, 0, 0,
	"right",		CLF_ACTION,		DDKEY_RIGHTARROW, 0, 0,
	"forward",		CLF_ACTION,		DDKEY_UPARROW, 0, 0,
	"backward",		CLF_ACTION,		DDKEY_DOWNARROW, 0, 0,
	"strafel",		CLF_ACTION,		',', 0, 0,
	"strafer",		CLF_ACTION,		'.', 0, 0,
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
	"weapon5",		CLF_ACTION,		'5', 0, 0,
	"weapon6",		CLF_ACTION,		'6', 0, 0,
	"weapon7",		CLF_ACTION,		'7', 0, 0,
	"weapon8",		CLF_ACTION,		'8', 0, 0,
	"weapon9",		CLF_ACTION,		'9', 0, 0,

	"cantdie",		CLF_ACTION,		0, 0, 0,
	"invisib",		CLF_ACTION,		0, 0, 0,
	"health",		CLF_ACTION,		0, 0, 0,
	"sphealth",		CLF_ACTION,		0, 0, 0,
	"tomepwr",		CLF_ACTION,		DDKEY_BACKSPACE, 0, 0,
	"torch",		CLF_ACTION,		0, 0, 0,
	"firebomb",		CLF_ACTION,		0, 0, 0,
	"egg",			CLF_ACTION,		0, 0, 0,
	"flyarti",		CLF_ACTION,		0, 0, 0,
	"teleport",		CLF_ACTION,		0, 0, 0,

	"panic",		CLF_ACTION,		0, 0, 0,
	"demostop",		CLF_ACTION,		'o', 0, 0,

	// Menu hotkeys (default: F1 - F12).
/*42*/"infoscreen",	0,				DDKEY_F1, 0, 0,
	"loadgame",		0,				DDKEY_F3, 0, 0,
	"savegame",		0,				DDKEY_F2, 0, 0,
	"soundmenu",	0,				DDKEY_F4, 0, 0,
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
	"jump",			CLF_ACTION,		0, 0, 0,
	"beginChat",	0,				't', 0, 0,
	"beginChat 0",	0,				'g', 0, 0,
	"beginChat 1",	0,				'y', 0, 0,
	"beginChat 2",	0,				'r', 0, 0,
	"beginChat 3",	0,				'b', 0, 0,
	"screenshot",	0,				0, 0, 0,
	"",				0,				0, 0, 0		// terminator
};
static Control_t *grabbing = NULL;

static float menuDark = 0;	// Background darkening.
static float menuDarkMax = 0.6f, menuDarkSpeed = 1 / 15.0f;
static int menuDarkDir = 0;	

static int FontABaseLump;
static int FontBBaseLump;
static int SkullBaseLump;
Menu_t *CurrentMenu;
int CurrentItPos;
static int MenuEpisode;
int MenuTime;
//static boolean soundchanged;

boolean askforquit;
boolean typeofask;
static boolean FileMenuKeySteal;
static boolean slottextloaded;
static char SlotText[NUMSAVESLOTS][SLOTTEXTLEN+2];
static char oldSlotText[SLOTTEXTLEN+2];
static int SlotStatus[NUMSAVESLOTS];
static int slotptr;
static int currentSlot;
static int quicksave;
static int quickload;

static MenuItem_t MainItems[] =
{
	{ ITT_EFUNC, "NEW GAME", SCNetCheck, 1, MENU_EPISODE },
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
	0, 6
};

static MenuItem_t EpisodeItems[] =
{
	// Episode names from TXT_EPISODE* (in SCNetCheck).
	{ ITT_EFUNC, "CITY OF THE DAMNED", SCEpisode, 1, MENU_NONE },
	{ ITT_EFUNC, "HELL'S MAW", SCEpisode, 2, MENU_NONE },
	{ ITT_EFUNC, "THE DOME OF D'SPARIL", SCEpisode, 3, MENU_NONE },
	{ ITT_EFUNC, "THE OSSUARY", SCEpisode, 4, MENU_NONE },
	{ ITT_EFUNC, "THE STAGNANT DEMESNE", SCEpisode, 5, MENU_NONE }
};

static Menu_t EpisodeMenu =
{
	80, 50,
	DrawEpisodeMenu,
	3, EpisodeItems,
	0,
	MENU_MAIN,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 3
};

static MenuItem_t FilesItems[] =
{
	{ ITT_EFUNC, "LOAD GAME", SCNetCheck, 2, MENU_LOAD },
	{ ITT_SETMENU, "SAVE GAME", NULL, 0, MENU_SAVE }
};

static Menu_t FilesMenu =
{
	110, 60,
	DrawFilesMenu,
	2, FilesItems,
	0,
	MENU_MAIN,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 2
};

static MenuItem_t LoadItems[] =
{
	{ ITT_EFUNC, NULL, SCLoadGame, 0, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 1, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 2, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 3, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 4, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 5, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 6, MENU_NONE },
	{ ITT_EFUNC, NULL, SCLoadGame, 7, MENU_NONE }
};

static Menu_t LoadMenu =
{
	70, 30,
	DrawLoadMenu,
	NUMSAVESLOTS, LoadItems,
	0,
	MENU_FILES,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, NUMSAVESLOTS
};

static MenuItem_t SaveItems[] =
{
	{ ITT_EFUNC, NULL, SCSaveGame, 0, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 1, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 2, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 3, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 4, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 5, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 6, MENU_NONE },
	{ ITT_EFUNC, NULL, SCSaveGame, 7, MENU_NONE }
};

static Menu_t SaveMenu =
{
	70, 30,
	DrawSaveMenu,
	NUMSAVESLOTS, SaveItems,
	0,
	MENU_FILES,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, NUMSAVESLOTS
};

static MenuItem_t SkillItems[] =
{
	{ ITT_EFUNC, "THOU NEEDETH A WET-NURSE", SCSkill, sk_baby, MENU_NONE },
	{ ITT_EFUNC, "YELLOWBELLIES-R-US", SCSkill, sk_easy, MENU_NONE },
	{ ITT_EFUNC, "BRINGEST THEM ONETH", SCSkill, sk_medium, MENU_NONE },
	{ ITT_EFUNC, "THOU ART A SMITE-MEISTER", SCSkill, sk_hard, MENU_NONE },
	{ ITT_EFUNC, "BLACK PLAGUE POSSESSES THEE",
		SCSkill, sk_nightmare, MENU_NONE }
};

static Menu_t SkillMenu =
{
	38, 30,
	DrawSkillMenu,
	5, SkillItems,
	2,
	MENU_EPISODE,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 5
};

static MenuItem_t OptionsItems[] =
{
	{ ITT_EFUNC, "END GAME", SCEndGame, 0, MENU_NONE },
	{ ITT_EFUNC, "CONTROL PANEL", SCOpenDCP, 0, MENU_NONE },
	{ ITT_SETMENU, "GAMEPLAY...", NULL, 0, MENU_GAMEPLAY },
	{ ITT_SETMENU, "HUD...", NULL, 0, MENU_HUD },
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
	8, OptionsItems,
	0,
	MENU_MAIN,
	MN_DrTextA_CS, 9,
	0, 8
};

static MenuItem_t Options2Items[] =
{
	{ ITT_LRFUNC, "SFX VOLUME :", SCSfxVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "MUSIC VOLUME :", SCMusicVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "OPEN AUDIO PANEL", SCOpenDCP, 1, MENU_NONE },
/*	{ ITT_LRFUNC, "CD VOLUME", SCCDVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },*/
/*	{ ITT_LRFUNC, "MUSIC :", SCMusicDevice, 0, MENU_NONE },
	{ ITT_EFUNC, "3D SOUNDS :", SC3DSounds, 0, MENU_NONE },
	{ ITT_LRFUNC, "REVERB VOLUME :", SCReverbVolume, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "SFX FREQUENCY :", SCSfxFrequency, 0, MENU_NONE },
	{ ITT_EFUNC, "16 BIT INTERPOLATION :", SCSfx16bit, 0, MENU_NONE }
*/
};

static Menu_t Options2Menu =
{
	70, 30,
	DrawOptions2Menu,
	7, Options2Items,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10,
	0, 7
};

static MenuItem_t HUDItems[] =
{
	{ ITT_EFUNC, "FULLSCREEN AMMO :", SCFullscreenMana, 0, MENU_NONE },
	{ ITT_EFUNC, "FULLSCREEN ARMOR :", SCFullscreenArmor, 0, MENU_NONE },
	{ ITT_EFUNC, "FULLSCREEN KEYS :", SCFullscreenKeys, 0, MENU_NONE },
	{ ITT_LRFUNC, "CROSSHAIR :", SCCrosshair, 0, MENU_NONE },
	{ ITT_LRFUNC, "CROSSHAIR SIZE :", SCCrosshairSize, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "SCREEN SIZE :", SCScreenSize, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "STATUS BAR SIZE :", SCStatusBarSize, 0, MENU_NONE},
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "MAP KILLS COUNT :", SCMapKills, 0, MENU_NONE },
	{ ITT_LRFUNC, "MAP ITEMS COUNT :", SCMapItems, 0, MENU_NONE },
	{ ITT_LRFUNC, "MAP SECRETS COUNT :", SCMapSecrets, 0, MENU_NONE }
};

static Menu_t HUDMenu =
{
	64, 30,
	DrawHUDMenu,
	16, HUDItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 9,
	0, 16
};

static MenuItem_t GameplayItems[] =
{
	{ ITT_EFUNC, "MESSAGES :", SCMessages, 0, MENU_NONE },
	{ ITT_EFUNC, "ALWAYS RUN :", SCAlwaysRun, 0, MENU_NONE },
	{ ITT_EFUNC, "LOOKSPRING :", SCLookSpring, 0, MENU_NONE },
	{ ITT_EFUNC, "NO AUTOAIM :", SCAutoAim, 0, MENU_NONE },
	{ ITT_EFUNC, "JUMPING ALLOWED :", SCAllowJump, 0, MENU_NONE },
};

static Menu_t GameplayMenu =
{
	72, 30,
	DrawGameplayMenu,
	5, GameplayItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 9,
	0, 5
};

static MenuItem_t ControlsItems[] =
{
	{ ITT_EMPTY, "PLAYER ACTIONS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "LEFT :", SCControlConfig, A_TURNLEFT, MENU_NONE },
	{ ITT_EFUNC, "RIGHT :", SCControlConfig, A_TURNRIGHT, MENU_NONE },
	{ ITT_EFUNC, "FORWARD :", SCControlConfig, A_FORWARD, MENU_NONE },
	{ ITT_EFUNC, "BACKWARD :", SCControlConfig, A_BACKWARD, MENU_NONE },
	{ ITT_EFUNC, "STRAFE LEFT :", SCControlConfig, A_STRAFELEFT, MENU_NONE },
	{ ITT_EFUNC, "STRAFE RIGHT :", SCControlConfig, A_STRAFERIGHT, MENU_NONE },
	{ ITT_EFUNC, "FIRE :", SCControlConfig, A_FIRE, MENU_NONE },
	{ ITT_EFUNC, "USE :", SCControlConfig, A_USE, MENU_NONE },	
	{ ITT_EFUNC, "JUMP : ", SCControlConfig, 60 /*A_JUMP*/, MENU_NONE },
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
	{ ITT_EFUNC, "STAFF/GAUNTLETS :", SCControlConfig, A_WEAPON1, MENU_NONE },
	{ ITT_EFUNC, "ELVENWAND :", SCControlConfig, A_WEAPON2, MENU_NONE },
	{ ITT_EFUNC, "CROSSBOW :", SCControlConfig, A_WEAPON3, MENU_NONE },
	{ ITT_EFUNC, "DRAGON CLAW :", SCControlConfig, A_WEAPON4, MENU_NONE },
	{ ITT_EFUNC, "HELLSTAFF :", SCControlConfig, A_WEAPON5, MENU_NONE },
	{ ITT_EFUNC, "PHOENIX ROD :", SCControlConfig, A_WEAPON6, MENU_NONE },
	{ ITT_EFUNC, "FIREMACE :", SCControlConfig, A_WEAPON7, MENU_NONE },
	{ ITT_EFUNC, "PANIC :", SCControlConfig, A_PANIC, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "ARTIFACTS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "INVINCIBILITY :", SCControlConfig, A_INVULNERABILITY, MENU_NONE },
	{ ITT_EFUNC, "SHADOWSPHERE :", SCControlConfig, A_INVISIBILITY, MENU_NONE },
	{ ITT_EFUNC, "QUARTZ FLASK :", SCControlConfig, A_HEALTH, MENU_NONE },
	{ ITT_EFUNC, "MYSTIC URN :", SCControlConfig, A_SUPERHEALTH, MENU_NONE },
	{ ITT_EFUNC, "TOME OF POWER:", SCControlConfig, A_TOMEOFPOWER, MENU_NONE },
	{ ITT_EFUNC, "TORCH :", SCControlConfig, A_TORCH, MENU_NONE },
	{ ITT_EFUNC, "TIME BOMB :", SCControlConfig, A_FIREBOMB, MENU_NONE },
	{ ITT_EFUNC, "MORPH OVUM :", SCControlConfig, A_EGG, MENU_NONE },
	{ ITT_EFUNC, "WINGS OF WRATH :", SCControlConfig, A_FLY, MENU_NONE },
	{ ITT_EFUNC, "CHAOS DEVICE :", SCControlConfig, A_TELEPORT, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "INVENTORY", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "INVENTORY LEFT :", SCControlConfig, 53, MENU_NONE },
	{ ITT_EFUNC, "INVENTORY RIGHT :", SCControlConfig, 54, MENU_NONE },
	{ ITT_EFUNC, "USE ARTIFACT :", SCControlConfig, A_USEARTIFACT, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "MENU HOTKEYS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "INFO :", SCControlConfig, 42, MENU_NONE },
	{ ITT_EFUNC, "SOUND MENU :", SCControlConfig, 45, MENU_NONE },
	{ ITT_EFUNC, "LOAD GAME :", SCControlConfig, 43, MENU_NONE },
	{ ITT_EFUNC, "SAVE GAME :", SCControlConfig, 44, MENU_NONE },
	{ ITT_EFUNC, "QUICK LOAD :", SCControlConfig, 49, MENU_NONE },
	{ ITT_EFUNC, "QUICK SAVE :", SCControlConfig, 46, MENU_NONE },
//	{ ITT_EFUNC, "SUICIDE :", SCControlConfig, 44, MENU_NONE },
	{ ITT_EFUNC, "END GAME :", SCControlConfig, 47, MENU_NONE },
	{ ITT_EFUNC, "QUIT :", SCControlConfig, 50, MENU_NONE },
	{ ITT_EFUNC, "MESSAGES ON/OFF:", SCControlConfig, 48, MENU_NONE },
	{ ITT_EFUNC, "GAMMA CORRECTION :", SCControlConfig, 51, MENU_NONE },
	{ ITT_EFUNC, "SPY MODE :", SCControlConfig, 52, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "SCREEN", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "SMALLER VIEW :", SCControlConfig, 56, MENU_NONE },
	{ ITT_EFUNC, "LARGER VIEW :", SCControlConfig, 55, MENU_NONE },
	{ ITT_EFUNC, "SMALLER STATBAR :", SCControlConfig, 58, MENU_NONE },
	{ ITT_EFUNC, "LARGER STATBAR :", SCControlConfig, 57, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, "MISCELLANEOUS", NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "SCREENSHOT :", SCControlConfig, 66, MENU_NONE },
	{ ITT_EFUNC, "PAUSE :", SCControlConfig, 59, MENU_NONE },
	{ ITT_EFUNC, "CHAT :", SCControlConfig, 61, MENU_NONE },
	{ ITT_EFUNC, "GREEN CHAT :", SCControlConfig, 62, MENU_NONE },
	{ ITT_EFUNC, "YELLOW CHAT :", SCControlConfig, 63, MENU_NONE },
	{ ITT_EFUNC, "RED CHAT :", SCControlConfig, 64, MENU_NONE },
	{ ITT_EFUNC, "BLUE CHAT :", SCControlConfig, 65, MENU_NONE },
	{ ITT_EFUNC, "STOP DEMO :", SCControlConfig, A_STOPDEMO, MENU_NONE }
};

static Menu_t ControlsMenu =
{
	32, 26,
	DrawControlsMenu,
	77, ControlsItems,
	1,
	MENU_OPTIONS,
	MN_DrTextA_CS, 9,
	0, 17
};

static MenuItem_t MouseOptsItems[] =
{
//	{ ITT_EFUNC, "INVERSE Y :", SCInverseY, 0, MENU_NONE },
	{ ITT_EFUNC, "MOUSE LOOK :", SCMouseLook, 0, MENU_NONE },
	{ ITT_EFUNC, "INVERSE MLOOK :", SCMouseLookInverse, 0, MENU_NONE },
	{ ITT_LRFUNC, "X SENSITIVITY :", SCMouseXSensi, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "Y SENSITIVITY :", SCMouseYSensi, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
};

static Menu_t MouseOptsMenu = 
{
	72, 30,
	DrawMouseOptsMenu,
	8, MouseOptsItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10,
	0, 8
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
	80, 30,
	DrawJoyConfigMenu,
	11, JoyConfigItems,
	0,
	MENU_OPTIONS,
	MN_DrTextA_CS, 10,
	0, 11
};

static Menu_t *Menus[] =
{
	&MainMenu,
	&EpisodeMenu,
	&SkillMenu,
	&OptionsMenu,
	&Options2Menu,
	&GameplayMenu,
	&HUDMenu,
	&ControlsMenu,
	&MouseOptsMenu,
	&JoyConfigMenu,
	&FilesMenu,
	&LoadMenu,
	&SaveMenu,
	&MultiplayerMenu,
	&GameSetupMenu,
	&PlayerSetupMenu
};

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
//	messageson = true;
	SkullBaseLump = W_GetNumForName("M_SKL00");
	if(ExtendedWAD)
	{ // Add episodes 4 and 5 to the menu
		EpisodeMenu.itemCount = EpisodeMenu.numVisItems = 5;
		EpisodeMenu.y = 50-ITEM_HEIGHT;
	}
	// Find the correct resolution.
//	selRes = findRes(Get(DD_SCREEN_WIDTH), Get(DD_SCREEN_HEIGHT));
}

//---------------------------------------------------------------------------
//
// PROC InitFonts
//
//---------------------------------------------------------------------------

static void InitFonts(void)
{
	FontABaseLump = W_GetNumForName("FONTA_S")+1;
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
		text[k] = MN_FilterChar(text[k]);
		/*char ch = toupper(text[k]);
		if(ch == '_') ch = '[';	// Mysterious... (from save slots).
		else if(ch == '\\') ch = '/';
		// Check that the character is printable.
		else if(ch < 32 || ch > 'Z') ch = 32; // Character out of range.
		text[k] = ch;*/
	}
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextA
//
// Draw text using font A.
//
//---------------------------------------------------------------------------

void MN_DrTextA(char *text, int x, int y)
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
			GL_DrawPatch(x, y, FontABaseLump+c-33);
			x += p->width-1;
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextA_CS
//
// Draw text using font A, in the current rendering state.
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

void MN_DrTextAGreen_CS(char *text, int x, int y)
{
	int color[4];

	gl.GetIntegerv(DGL_RGBA, color);

	// Set a green color, but retain alpha.
	gl.Color4ub(102, 204, 102, color[3]);

	MN_DrTextA_CS(text, x, y);

	// Restore the old color.
	gl.Color4ub(color[0], color[1], color[2], color[3]);
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

	if(!text) return 0;
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

void MN_DrTextB(char *text, int x, int y)
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
			GL_DrawPatch(x, y, FontBBaseLump+c-33);
			x += p->width-1;
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextB_CS
//
// Draw text using font B, in the current state.
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

	if(!text) return 0;
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
// PROC MN_DrawMenuItem
//
//---------------------------------------------------------------------------

void MN_DrawMenuText(Menu_t *menu, int index, char *text)
{
	int max = 0, off = 0;
	int i;
	char *str;

	for(i=0; i<menu->itemCount; i++)
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
// PROC MN_DrawTitle
//
//---------------------------------------------------------------------------

void MN_DrawTitle(char *text, int y)
{
	MN_DrTextB_CS(text, 160 - MN_TextBWidth(text)/2, y);
}

//---------------------------------------------------------------------------
//
// PROC MN_Ticker
//
//---------------------------------------------------------------------------

void MN_Ticker(void)
{
	// Background darkening. First choose the right direction.
	if(MenuActive)
	{
		if(menuDark < menuDarkMax) menuDarkDir = 1;
	}
	else
	{
		if(menuDark > 0) menuDarkDir = -1;
	}
	// Make a modification, if needed.
	if(menuDarkDir)
	{
		menuDark += menuDarkDir * menuDarkSpeed;
		if(menuDark < 0) menuDark = 0;
		if(menuDark > menuDarkMax) menuDark = menuDarkMax;
	}

	if(!MenuActive) return;
	MenuTime++;

	// Call the extended ticker (multiplayer stuff).
	MN_TickerEx();
}

//---------------------------------------------------------------------------
//
// PROC DrawMessage
//
//---------------------------------------------------------------------------

void DrawMessage(void)
{
	player_t *player;

	player = &players[consoleplayer];
	if(player->messageTics <= 0 || !player->message)
	{ // No message
		return;
	}
	MN_DrTextA(player->message, 160-MN_TextAWidth(player->message)/2, 1);
}

//---------------------------------------------------------------------------
//
// PROC MN_Drawer
//
//---------------------------------------------------------------------------

char *QuitEndMsg[] =
{
	"ARE YOU SURE YOU WANT TO QUIT?",
	"ARE YOU SURE YOU WANT TO END THE GAME?",
	"DO YOU WANT TO QUICKSAVE THE GAME NAMED",
	"DO YOU WANT TO QUICKLOAD THE GAME NAMED"
};

void MN_Drawer(void)
{
	int i;
	int x;
	int y;
	MenuItem_t *item;
	char *selName;
	float alpha = menuDark/menuDarkMax;

	DrawMessage();

/*	if(players[displayplayer].plr->mo)
	{
		char buff[80];
		sprintf(buff, "%i, %i", players[displayplayer].plr->mo->x >> 16,
			players[displayplayer].plr->mo->y >> 16);
		MN_DrTextA(buff, 0, 10);
	}*/

	// FPS.
	if(cfg.showFPS)
	{
		char fpsbuff[80];
		sprintf(fpsbuff, "%d FPS", DD_GetFrameRate());
		MN_DrTextA(fpsbuff, 320-MN_TextAWidth(fpsbuff), 0);
		GL_Update(DDUF_TOP);
	}

	// Does the background need to be darkened?
	if(menuDark > 0)
	{
		GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
		GL_SetNoTexture();
		GL_DrawRect(0, 0, 320, 200, 0, 0, 0, menuDark);
	}

	if(MenuActive == false)
	{
		if(askforquit)
		{
			gl.Color4f(1, 1, 1, 1-alpha);
			MN_DrTextA_CS(QuitEndMsg[typeofask-1], 160-
				MN_TextAWidth(QuitEndMsg[typeofask-1])/2, 80);
			if(typeofask == 3)
			{
				MN_DrTextA_CS(SlotText[quicksave-1], 160-
					MN_TextAWidth(SlotText[quicksave-1])/2, 90);
				MN_DrTextA_CS("?", 160+
					MN_TextAWidth(SlotText[quicksave-1])/2, 90);
			}
			if(typeofask == 4)
			{
				MN_DrTextA_CS(SlotText[quickload-1], 160-
					MN_TextAWidth(SlotText[quickload-1])/2, 90);
				MN_DrTextA_CS("?", 160+
					MN_TextAWidth(SlotText[quickload-1])/2, 90);
			}
			GL_Update(DDUF_FULLSCREEN);
		}
	}
	if(MenuActive || menuDark > 0)
	{
		gl.Color4f(1, 1, 1, alpha);
		GL_Update(DDUF_FULLSCREEN);
		if(InfoType)
		{
			MN_DrawInfo();
			return;
		}

		// Apply the menu scale.
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PushMatrix();
		gl.Translatef(160, 100, 0);
		gl.Scalef(cfg.menuScale, cfg.menuScale, cfg.menuScale);
		gl.Translatef(-160, -100, 0);

		if(cfg.screenblocks < 10)
		{
			GL_Update(DDUF_BORDER);
		}
		if(CurrentMenu->drawFunc != NULL)
		{
			CurrentMenu->drawFunc();
		}
		x = CurrentMenu->x;
		y = CurrentMenu->y;
		for(i=0, item = CurrentMenu->items + CurrentMenu->firstItem; 
			i < CurrentMenu->numVisItems && CurrentMenu->firstItem + i < CurrentMenu->itemCount; 
			i++, y += CurrentMenu->itemHeight, item++)
		{
			if(item->type != ITT_EMPTY || item->text)
			{
				// Decide which color to use.
				if(item->type == ITT_EMPTY)
					gl.Color4f(.4f, .8f, .4f, alpha); // Green for titles.
				else
					gl.Color4f(1, 1, 1, alpha);

				if(item->text)
					CurrentMenu->textDrawer(item->text, x, y);
			}
		}
		// Back to normal color.
		gl.Color4f(1, 1, 1, alpha);

		y = CurrentMenu->y + ((CurrentItPos-CurrentMenu->firstItem) * CurrentMenu->itemHeight) 
			+ SELECTOR_YOFFSET - (10 - CurrentMenu->itemHeight/2);
		selName = MenuTime&16 ? "M_SLCTR1" : "M_SLCTR2";
		GL_DrawPatch_CS(x+SELECTOR_XOFFSET, y, W_GetNumForName(selName));

		// Restore old matrix.
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PopMatrix();
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

	frame = (MenuTime/3)%18;
	GL_DrawPatch_CS(88, 0, W_GetNumForName("M_HTIC"));
	GL_DrawPatch_CS(40, 10, SkullBaseLump+(17-frame));
	GL_DrawPatch_CS(232, 10, SkullBaseLump+frame);
}

//---------------------------------------------------------------------------
//
// PROC DrawEpisodeMenu
//
//---------------------------------------------------------------------------

static void DrawEpisodeMenu(void)
{
	MN_DrawTitle("WHICH EPISODE?", 4);
}

//---------------------------------------------------------------------------
//
// PROC DrawSkillMenu
//
//---------------------------------------------------------------------------

static void DrawSkillMenu(void)
{
	MN_DrawTitle("SKILL LEVEL?", 4);
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
	players[consoleplayer].message = NULL;
	players[consoleplayer].messageTics = 1;
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
//              Loads in the text message for each slot
//===========================================================================

void MN_LoadSlotText(void)
{
//	FILE *fp;
//	int             count;
	int             i;
	char    name[256];

	for (i = 0; i < NUMSAVESLOTS; i++)
	{
/*		if(cdrom)
		{
			sprintf(name, SAVEGAMENAMECD"%d.hsg", i);
		}
		else
		{
			sprintf(name, SAVEGAMENAME"%d.hsg", i);
		}*/
		SV_SaveGameFile(i, name);
		//fp = fopen(name, "rb+");
		//if(!fp)
		if(!SV_GetSaveDescription(name, SlotText[i]))
		{
			SlotText[i][0] = 0; // empty the string
			SlotStatus[i] = 0;
			continue;
		}
		//count = fread(&SlotText[i], SLOTTEXTLEN, 1, fp);
		//fclose(fp);
		SlotStatus[i] = 1;
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
	for(i = 0; i < NUMSAVESLOTS; i++)
	{
		GL_DrawPatch_CS(x, y, W_GetNumForName("M_FSLOT"));
		if(SlotStatus[i])
		{
			MN_DrTextA_CS(SlotText[i], x+5, y+5);
		}
		y += ITEM_HEIGHT;
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawOptionsMenu
//
//---------------------------------------------------------------------------

static void DrawOptionsMenu(void)
{
/*	if(messageson)
	{
		MN_DrTextB("ON", 196, 50);
	}
	else
	{
		MN_DrTextB("OFF", 196, 50);
	}
	DrawSlider(&OptionsMenu, 3, 10, mouseSensitivityX);*/

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
/*	char *musDevStr[3] = { "NO", "MIDI", "CUSTOM / MIDI" };
	char *freqStr[4] = { "11 KHZ", "22 KHZ", "INVALID!", "44 KHZ" };
	int i, temp = (int) (*(float*) Con_GetVariable("sound-reverb-volume")->ptr * 10 + .5f);
*/
	MN_DrawTitle("SOUND", 4);

	DrawSlider(menu, 1, 18, Get(DD_SFX_VOLUME)/15);
	DrawSlider(menu, 4, 18, Get(DD_MUSIC_VOLUME)/15);
/*	i = Get(DD_MUSIC_DEVICE);
	if(i && cfg.customMusic) i = 2;
	if(i < 0) i = 0;
	if(i > 2) i = 2;
	MN_DrawMenuText(menu, 6, musDevStr[i]);
	MN_DrawMenuText(menu, 7, onoff[CVAR(int, "s_3d")]);
	DrawSlider(menu, 9, 11, temp);
	MN_DrawMenuText(menu, 11, freqStr[CVAR(int, "s_resample")-1]);
	MN_DrawMenuText(menu, 12, onoff[CVAR(int, "s_16bit")]);
*/
}

#if 0
static boolean SCMusicDevice(int option)
{
	int snd_MusicDevice = Get(DD_MUSIC_DEVICE);
	
	if(option == RIGHT_DIR)
	{
		if(snd_MusicDevice < 1) 
		{
			snd_MusicDevice++;
			cfg.customMusic = false;
		}
		else if(!cfg.customMusic)
			cfg.customMusic = true;
	}
	else if(cfg.customMusic)
		cfg.customMusic = false;
	else if(snd_MusicDevice > 0) 
		snd_MusicDevice--;

#pragma message("mn_menu: Setting of the music device is not implemented.")

	// Setup the music.
//	gi.SetMusicDevice(snd_MusicDevice);
	
	// Restart the song of the current map.
	if(snd_MusicDevice) S_LevelMusic();
	return true;
}
#endif

static void DrawGameplayMenu(void)
{
	Menu_t *menu = &GameplayMenu;
	
	MN_DrawTitle("GAMEPLAY", 4);

	MN_DrawMenuText(menu, 0, yesno[cfg.messageson]);
	MN_DrawMenuText(menu, 1, yesno[cfg.alwaysRun]);
	MN_DrawMenuText(menu, 2, yesno[cfg.lookSpring]);
	MN_DrawMenuText(menu, 3, yesno[cfg.noAutoAim]);
	MN_DrawMenuText(menu, 4, yesno[cfg.jumpEnabled]);
}

static void DrawHUDMenu(void)
{
	Menu_t *menu = &HUDMenu;
	char *xhairnames[7] = { "NONE", "CROSS", "ANGLES", "SQUARE",
		"OPEN SQUARE", "DIAMOND", "V" };
	char *countnames[4] = { "NO", "YES", "PERCENT", "COUNT+PCNT" };
	
	MN_DrawTitle("HEAD-UP DISPLAY", 4);

	MN_DrawMenuText(menu, 0, yesno[cfg.showFullscreenMana]);
	MN_DrawMenuText(menu, 1, yesno[cfg.showFullscreenArmor]);
	MN_DrawMenuText(menu, 2, yesno[cfg.showFullscreenKeys]);
	MN_DrawMenuText(menu, 3, xhairnames[cfg.xhair]);
	DrawSlider(menu, 5, 9, cfg.xhairSize);
	DrawSlider(menu, 8, 9, cfg.screenblocks-3);
	DrawSlider(menu, 11, 20, cfg.sbarscale-1);
	MN_DrawMenuText(menu, 13, countnames[(cfg.counterCheat & 0x1) 
		| ((cfg.counterCheat & 0x8)>>2)]);
	MN_DrawMenuText(menu, 14, countnames[((cfg.counterCheat & 0x2)>>1)
		| ((cfg.counterCheat & 0x10)>>3)]);
	MN_DrawMenuText(menu, 15, countnames[((cfg.counterCheat & 0x4)>>2)
		| ((cfg.counterCheat & 0x20)>>4)]);
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

	MN_DrawTitle("GRAPHICS", 4);

	MN_DrawMenuText(menu, 2, yesno[CVAR(int, "usemodels")!=0]);
	DrawSlider(menu, 4, 5, Get(DD_SKY_DETAIL)-3);
	MN_DrawMenuText(menu, 6, mipStr[Get(DD_MIPMAPPING)]);
	MN_DrawMenuText(menu, 7, yesno[Get(DD_SMOOTH_IMAGES)]);
	MN_DrawMenuText(menu, 8, texQStr[*(int*)cv->ptr]);

	// This isn't very good practice, but here we can reset the 
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

	MN_DrawTitle("EFFECTS", 4);
	MN_DrawMenuText(menu, 0, yesno[cfg.showFPS]);
	MN_DrawMenuText(menu, 1, onoff[CVAR(int, "dynlights")]);
	MN_DrawMenuText(menu, 2, dlblendStr[CVAR(int, "dlblend")]);
	MN_DrawMenuText(menu, 3, yesno[CVAR(int, "sprlight")]);
	temp = (int) (CVAR(float, "dlfactor") * 10 + .5f);
	DrawSlider(menu, 5, 11, temp);
	MN_DrawMenuText(menu, 7, flareStr[CVAR(int, "flares")]);
	DrawSlider(menu, 9, 11, CVAR(int, "flareintensity") / 10);
	DrawSlider(menu, 12, 11, CVAR(int, "flaresize"));
	MN_DrawMenuText(menu, 14, alignStr[CVAR(int, "spralign")]);
	MN_DrawMenuText(menu, 15, onoff[CVAR(int, "sprblend")]);
}

static void DrawResolutionMenu(void)
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
}*/

static boolean SCOpenDCP(int option)
{
//	MN_DeactivateMenu();
	Con_Execute(option? "panel audio" : "panel", true);
	return true;
}

/*static boolean SCResSelector(int option)
{
	if(option == RIGHT_DIR)
	{
		if(resolutions[selRes+1].width)
			selRes++;
	}
	else if(selRes > 0) selRes--;
	return true;
}

static boolean SCResMakeCurrent(int option)
{
	if(GL_ChangeResolution(resolutions[selRes].width, 
		resolutions[selRes].height, 0))
		P_SetMessage(&players[consoleplayer], "RESOLUTION CHANGED", true);
	else
		P_SetMessage(&players[consoleplayer], "RESOLUTION CHANGE FAILED", true);
	return true;
}

static boolean SCResMakeDefault(int option)
{
	Set(DD_DEFAULT_RES_X, resolutions[selRes].width);
	Set(DD_DEFAULT_RES_Y, resolutions[selRes].height);
	return true;
}*/

static boolean SCLookSpring(int option)
{
	cfg.lookSpring ^= 1;
	if(cfg.lookSpring)
		P_SetMessage(&players[consoleplayer], "USING LOOKSPRING", true);
	else
		P_SetMessage(&players[consoleplayer], "NO LOOKSPRING", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCAutoAim(int option)
{
	P_SetMessage(&players[consoleplayer], 
		(cfg.noAutoAim^=1)? "NO AUTOAIM" : "AUTOAIM ON", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCFullscreenArmor(int option)
{
	cfg.showFullscreenArmor ^= 1;
	P_SetMessage(&players[consoleplayer], 
		cfg.showFullscreenArmor? "ARMOR SHOWN IN FULLSCREEN VIEW"
		: "NO ARMOR IN FULLSCREEN VIEW", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCFullscreenKeys(int option)
{
	cfg.showFullscreenKeys ^= 1;
	P_SetMessage(&players[consoleplayer], 
		cfg.showFullscreenArmor? "KEYS SHOWN IN FULLSCREEN VIEW"
		: "NO KEYS IN FULLSCREEN VIEW", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCFullscreenMana(int option)
{
	cfg.showFullscreenMana ^= 1;
	if(cfg.showFullscreenMana)
	{
		P_SetMessage(&players[consoleplayer], "AMMO SHOWN IN FULLSCREEN VIEW", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "NO AMMO IN FULLSCREEN VIEW", true);
	}
	S_LocalSound(sfx_chat, NULL);
	return true;
}

//===========================================================================
// SCMapKills
//===========================================================================
static boolean SCMapKills(int option)
{
	int op = (cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8)>>2);

	op += option == RIGHT_DIR? 1 : -1;
	if(op < 0) op = 0; 
	if(op > 3) op = 3;
	cfg.counterCheat &= ~0x9;
	cfg.counterCheat |= (op & 0x1) | ((op & 0x2)<<2);
	return true;
}

//===========================================================================
// SCMapItems
//===========================================================================
static boolean SCMapItems(int option)
{
	int op = ((cfg.counterCheat & 0x2)>>1) | ((cfg.counterCheat & 0x10)>>3);

	op += option == RIGHT_DIR? 1 : -1;
	if(op < 0) op = 0; 
	if(op > 3) op = 3;
	cfg.counterCheat &= ~0x12;
	cfg.counterCheat |= ((op & 0x1)<<1) | ((op & 0x2)<<3);
	return true;
}

//===========================================================================
// SCMapSecrets
//===========================================================================
static boolean SCMapSecrets(int option)
{
	int op = ((cfg.counterCheat & 0x4)>>2) | ((cfg.counterCheat & 0x20)>>4);

	op += option == RIGHT_DIR? 1 : -1;
	if(op < 0) op = 0; 
	if(op > 3) op = 3;
	cfg.counterCheat &= ~0x24;
	cfg.counterCheat |= ((op & 0x1)<<2) | ((op & 0x2)<<4);
	return true;
}

static boolean SCCrosshair(int option)
{
	cfg.xhair += option==RIGHT_DIR? 1 : -1;
	if(cfg.xhair < 0) cfg.xhair = 0;
	if(cfg.xhair > NUM_XHAIRS) cfg.xhair = NUM_XHAIRS;
	return true;
}

static boolean SCCrosshairSize(int option)
{
	cfg.xhairSize += option==RIGHT_DIR? 1 : -1;
	if(cfg.xhairSize < 0) cfg.xhairSize = 0;
	if(cfg.xhairSize > 9) cfg.xhairSize = 9;
	return true;
}

//---------------------------------------------------------------------------
//
// PROC DrawMouseOptsMenu
//
//---------------------------------------------------------------------------

static void DrawMouseOptsMenu(void)
{
	Menu_t *menu = &MouseOptsMenu;

	MN_DrawTitle("MOUSE", 4);

//	MN_DrawMenuText(menu, 0, yesno[Get(DD_MOUSE_INVERSE_Y)]);
	MN_DrawMenuText(menu, 0, yesno[cfg.usemlook]);
	MN_DrawMenuText(menu, 1, yesno[cfg.mlookInverseY]);
	DrawSlider(&MouseOptsMenu, 3, 18, cfg.mouseSensiX);
	DrawSlider(&MouseOptsMenu, 6, 18, cfg.mouseSensiY);
}

static boolean SCControlConfig(int option)
{
	if(grabbing != NULL) Con_Error("SCControlConfig: grabbing is not NULL!!!\n");
	
	grabbing = controls + option;
	return true;
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

	//MN_DrTextB_CS("CONTROLS", 120, 4);
	MN_DrawTitle("CONTROLS", 4);

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
			   token[0] == '-') spacecat(prbuff, token);
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

		MN_DrTextA_CS(prbuff, menu->x+134, menu->y + i*menu->itemHeight);
	}
}

static void DrawJoyConfigMenu()
{
	char *axisname[5] = { "-", "MOVE", "TURN", "STRAFE", "LOOK" };
	Menu_t	*menu = &JoyConfigMenu;

	MN_DrawTitle("JOYSTICK", 4);

	MN_DrawMenuText(menu, 0, axisname[cfg.joyaxis[0]]);
	MN_DrawMenuText(menu, 1, axisname[cfg.joyaxis[1]]);
	MN_DrawMenuText(menu, 2, axisname[cfg.joyaxis[2]]);
	MN_DrawMenuText(menu, 3, axisname[cfg.joyaxis[3]]);
	MN_DrawMenuText(menu, 4, axisname[cfg.joyaxis[4]]);
	MN_DrawMenuText(menu, 5, axisname[cfg.joyaxis[5]]);
	MN_DrawMenuText(menu, 6, axisname[cfg.joyaxis[6]]);
	MN_DrawMenuText(menu, 7, axisname[cfg.joyaxis[7]]);
	MN_DrawMenuText(menu, 8, yesno[cfg.usejlook]);
	MN_DrawMenuText(menu, 9, yesno[cfg.jlookInverseY]);
	MN_DrawMenuText(menu, 10, yesno[cfg.povLookAround]);
}

//---------------------------------------------------------------------------
//
// PROC SCNetCheck
//
//---------------------------------------------------------------------------

static boolean SCNetCheck(int option)
{
	int i, w, maxw = 0;

	// Update the names of the episodes in the New Game menu.
	for(i = 0; i < 5; i++)
	{
		EpisodeItems[i].text = GET_TXT(TXT_EPISODE1 + i);
		w = MN_TextBWidth(EpisodeItems[i].text);
		if(w > maxw) maxw = w;
	}
	EpisodeMenu.x = 160 - maxw/2 + 4; // +4 for the selection arrow.

	if(!IS_NETGAME)
	{ // okay to go into the menu
		return true;
	}
	switch(option)
	{
		case 1:
			P_SetMessage(&players[consoleplayer],
				"YOU CAN'T START A NEW GAME IN NETPLAY!", true);
			break;
		case 2:
			P_SetMessage(&players[consoleplayer],
				"YOU CAN'T LOAD A GAME IN NETPLAY!", true);
			break;
		default:
			break;
	}
	MenuActive = false;
	return false;
}

//---------------------------------------------------------------------------
//
// PROC SCQuitGame
//
//---------------------------------------------------------------------------

static boolean SCQuitGame(int option)
{
	Con_Open(false);
	MenuActive = false;
	askforquit = true;
	typeofask = 1; //quit game
	if(!IS_NETGAME && !Get(DD_PLAYBACK))
	{
		paused = true;
	}
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCEndGame
//
//---------------------------------------------------------------------------

static boolean SCEndGame(int option)
{
	if(Get(DD_PLAYBACK) || IS_NETGAME)
	{
		return false;
	}
	MenuActive = false;
	askforquit = true;
	typeofask = 2; //endgame
	if(!IS_NETGAME && !Get(DD_PLAYBACK))
	{
		paused = true;
	}
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCMessages
//
//---------------------------------------------------------------------------

static boolean SCMessages(int option)
{
	cfg.messageson ^= 1;
	if(cfg.messageson)
	{
		P_SetMessage(&players[consoleplayer], "MESSAGES ON", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "MESSAGES OFF", true);
	}
	S_LocalSound(sfx_chat, NULL);
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCLoadGame
//
//---------------------------------------------------------------------------

static boolean SCLoadGame(int option)
{
	char name[256];

	if(!SlotStatus[option])
	{ // slot's empty...don't try and load
		return false;
	}
/*	if(cdrom)
	{
		sprintf(name, SAVEGAMENAMECD"%d.hsg", option);
	}
	else
	{
		sprintf(name, SAVEGAMENAME"%d.hsg", option);
	}*/
	SV_SaveGameFile(option, name);
	G_LoadGame(name);
	MN_DeactivateMenu();
	//BorderNeedRefresh = true;
	GL_Update(DDUF_BORDER);
	if(quickload == -1)
	{
		quickload = option+1;
		players[consoleplayer].message = NULL;
		players[consoleplayer].messageTics = 1;
	}
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCSaveGame
//
//---------------------------------------------------------------------------

static boolean SCSaveGame(int option)
{
	char *ptr;

	// Can't save if not in a level.
	if(!usergame
		|| IS_CLIENT
		|| Get(DD_PLAYBACK) 
		|| gamestate != GS_LEVEL) 
	{
		// Can't save if not playing.
		FileMenuKeySteal = false;
		return true;
	}

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
		return false;
	}
	else
	{
		G_SaveGame(option, SlotText[option]);
		FileMenuKeySteal = false;
		MN_DeactivateMenu();
	}
	//BorderNeedRefresh = true;
	GL_Update(DDUF_BORDER);
	if(quicksave == -1)
	{
		quicksave = option+1;
		players[consoleplayer].message = NULL;
		players[consoleplayer].messageTics = 1;
	}
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCEpisode
//
//---------------------------------------------------------------------------

static boolean SCEpisode(int option)
{
	if(shareware && option > 1)
	{
		P_SetMessage(&players[consoleplayer],
			"ONLY AVAILABLE IN THE REGISTERED VERSION", true);
	}
	else
	{
		MenuEpisode = option;
		SetMenu(MENU_SKILL);
	}
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCSkill
//
//---------------------------------------------------------------------------

static boolean SCSkill(int option)
{
	G_DeferedInitNew(option, MenuEpisode, 1);
	MN_DeactivateMenu();
	return true;
}

static boolean SCSfxVolume(int option)
{
	int vol = Get(DD_SFX_VOLUME);

	vol += option==RIGHT_DIR? 15 : -15;
	if(vol < 0) vol = 0;
	if(vol > 255) vol = 255;
	//soundchanged = true; // we'll set it when we leave the menu
	Set(DD_SFX_VOLUME, vol);
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCMusicVolume
//
//---------------------------------------------------------------------------

static boolean SCMusicVolume(int option)
{
	int vol = Get(DD_MUSIC_VOLUME);

	vol += option==RIGHT_DIR? 15 : -15;
	if(vol < 0) vol = 0;
	if(vol > 255) vol = 255;
	Set(DD_MUSIC_VOLUME, vol);
	return true;
}

#if 0
static boolean SC3DSounds(int option)
{
	cvar_t	*cv3ds = Con_GetVariable("s_3d");

	P_SetMessage(&players[consoleplayer], (*(int*) cv3ds->ptr ^= 1)? 
		"3D SOUND MODE" : "2D SOUND MODE", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCReverbVolume(int option)
{
#pragma message("mn_menu.c: reverb volume setting waiting for the Panel.")
/*
	extern sector_t *listenerSector;
	cvar_t	*cv = Con_GetVariable("sound-reverb-volume");
	float	val = *(float*) cv->ptr;

	val += option==RIGHT_DIR? .1f : -.1f;
	if(val > cv->max) val = cv->max;
	if(val < cv->min) val = cv->min;
	*(float*) cv->ptr = val;
*/
	return true;
}

static boolean SCSfxFrequency(int option)
{
	cvar_t	*cv = Con_GetVariable("s_resample");
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
	*(int*) cv->ptr = val;
	return true;
}

static boolean SCSfx16bit(int option)
{
	cvar_t	*cv = Con_GetVariable("s_16bit");

	P_SetMessage(&players[consoleplayer], (*(int*) cv->ptr ^= 1)? 
		"16 BIT INTERPOLATION" : "8 BIT INTERPOLATION", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}
#endif

//---------------------------------------------------------------------------
//
// PROC SCStatusBarSize
//
//---------------------------------------------------------------------------

static boolean SCStatusBarSize(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.sbarscale < 20) cfg.sbarscale++;
	}
	else 
		if(cfg.sbarscale > 1) cfg.sbarscale--;

	R_SetViewSize(cfg.screenblocks, 0);
	return true;
}


//---------------------------------------------------------------------------
//
// PROC SCScreenSize
//
//---------------------------------------------------------------------------

static boolean SCScreenSize(int option)
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
	return true;
}

static boolean SCAlwaysRun(int option)
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
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCAllowJump(int option)
{
	cfg.jumpEnabled = !cfg.jumpEnabled;
	P_SetMessage(&players[consoleplayer], cfg.jumpEnabled?
		"JUMPING ALLOWED" : "JUMPING NOT ALLOWED", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCMouseLook(int option)
{
	cfg.usemlook ^= 1;
	if(cfg.usemlook)
	{
		P_SetMessage(&players[consoleplayer], "MOUSE LOOK ON", true);
	}
	else
	{
		P_SetMessage(&players[consoleplayer], "MOUSE LOOK OFF", true);
	}
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCJoyLook(int option)
{
	P_SetMessage(&players[consoleplayer], (cfg.usejlook^=1)? "JOYSTICK LOOK ON" : "JOYSTICK LOOK OFF", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCPOVLook(int option)
{
	P_SetMessage(&players[consoleplayer], (cfg.povLookAround^=1)? "POV LOOK ON" : "POV LOOK OFF", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCInverseJoyLook(int option)
{
	P_SetMessage(&players[consoleplayer], (cfg.jlookInverseY^=1)? "INVERSE JOYLOOK" : "NORMAL JOYLOOK", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

static boolean SCJoyAxis(int option)
{
	if(option & RIGHT_DIR)
	{
		if(cfg.joyaxis[option >> 8] < 4) cfg.joyaxis[option >> 8]++;
	}
	else
	{
		if(cfg.joyaxis[option >> 8] > 0) cfg.joyaxis[option >> 8]--;
	}
	return true;
}

/*static boolean SCYAxis(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.joyaxis[1] < 4) cfg.joyaxis[1]++;
	}
	else
	{
		if(cfg.joyaxis[1] > 0) cfg.joyaxis[1]--;
	}
	return true;
}

static boolean SCZAxis(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.joyaxis[2] < 4) cfg.joyaxis[2]++;
	}
	else
	{
		if(cfg.joyaxis[2] > 0) cfg.joyaxis[2]--;
	}
	return true;
}*/

static boolean SCMouseLookInverse(int option)
{
	cfg.mlookInverseY ^= 1;
	if(cfg.mlookInverseY)
		P_SetMessage(&players[consoleplayer], "INVERSE MOUSE LOOK", true);
	else
		P_SetMessage(&players[consoleplayer], "NORMAL MOUSE LOOK", true);
	S_LocalSound(sfx_chat, NULL);
	return true;
}

/*
static boolean SCInverseY(int option)
{
	int val = Get(DD_MOUSE_INVERSE_Y);
		
	P_SetMessage(&players[consoleplayer], 
		(val^=1)? "INVERSE MOUSE Y AXIS" : "NORMAL MOUSE Y AXIS", true);
	Set(DD_MOUSE_INVERSE_Y, val);
	S_LocalSound(sfx_chat, NULL);
	return true;
}
*/

//---------------------------------------------------------------------------
//
// PROC SCMouseXSensi
//
//---------------------------------------------------------------------------

static boolean SCMouseXSensi(int option)
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
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCMouseYSensi
//
//---------------------------------------------------------------------------

static boolean SCMouseYSensi(int option)
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
	return true;
}

//---------------------------------------------------------------------------
//
// PROC SCInfo
//
//---------------------------------------------------------------------------

static boolean SCInfo(int option)
{
	InfoType = 1;
	S_LocalSound(sfx_dorcls, NULL);
	if(!IS_NETGAME && !Get(DD_PLAYBACK))
	{
		paused = true;
	}
	return true;
}

// Set default bindings for unbound Controls.
void H_DefaultBindings()
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
// FUNC H_PrivilegedResponder
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

int H_PrivilegedResponder(event_t *event)
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
			/*if(event->data1 == '`') // Tilde clears everything.
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
			else */if(event->data1 == DDKEY_ESCAPE)
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
		Con_Execute(cmd, false);//true);
		// We've finished the grab.
		grabbing = NULL;
		S_LocalSound(sfx_chat, NULL);
		return true;
	}

	// Process the screen shot key right away.
	if(ravpic && event->data1 == DDKEY_F1)
	{
		if(event->type == ev_keydown) G_ScreenShot();
		// All F1 events are eaten.
		return true;
	}
	return false;
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
		shiftdown = (event->type == ev_keydown || event->type == ev_keyrepeat);
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
			paused = false;
			MN_DeactivateMenu();
			GL_Update(DDUF_BORDER);
			menuDark = 0;	// Darkness immediately gone.
		}
		S_LocalSound(sfx_dorcls, NULL);
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
							players[consoleplayer].messageTics = 0;
								//set the msg to be cleared
							players[consoleplayer].message = NULL;
							typeofask = 0;
							askforquit = false;
							paused = false;
							G_StartTitle(); // go to intro/demo mode.
							break;
						case 3:
							P_SetMessage(&players[consoleplayer], "QUICKSAVING....", false);
							FileMenuKeySteal = true;
							SCSaveGame(quicksave-1);
							askforquit = false;
							typeofask = 0;
							//BorderNeedRefresh = true;
							GL_Update(DDUF_BORDER);
							return true;
						case 4:
							P_SetMessage(&players[consoleplayer], "QUICKLOADING....", false);
							SCLoadGame(quickload-1);
							askforquit = false;
							typeofask = 0;
							//BorderNeedRefresh = true;
							GL_Update(DDUF_BORDER);
							return true;
						default:
							return true; // eat the 'y' keypress
					}
				}
				return false;
			case 'n':
			case DDKEY_ESCAPE:
				if(askforquit)
				{
					players[consoleplayer].messageTics = 1; //set the msg to be cleared
					askforquit = false;
					typeofask = 0;
					paused = false;
					GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
					return true;
				}
				return false;
		}
		return false; // don't let the keys filter thru
	}
	if(MenuActive == false)
	{
		if(key == DDKEY_ESCAPE 
			/*|| gamestate == GS_DEMOSCREEN*/
			|| FI_IsMenuTrigger(event)
			|| Get(DD_PLAYBACK))
		{
			MN_ActivateMenu();
			return(false); // allow bindings (like demostop)
		}
		return(false);
	}
	if(!FileMenuKeySteal)
	{
		int firstVI = CurrentMenu->firstItem, lastVI = firstVI + CurrentMenu->numVisItems-1;
		if(lastVI > CurrentMenu->itemCount-1)
			lastVI = CurrentMenu->itemCount-1;
		item = &CurrentMenu->items[CurrentItPos];
		switch(key)
		{
			case DDKEY_DOWNARROW:
				do
				{
					if(CurrentItPos+1 > lastVI)
					{
						CurrentItPos = firstVI;
					}
					else
					{
						CurrentItPos++;
					}
				} while(CurrentMenu->items[CurrentItPos].type == ITT_EMPTY);
				S_LocalSound(sfx_switch, NULL);
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
				S_LocalSound(sfx_switch, NULL);
				return(true);
				break;
			case DDKEY_LEFTARROW:
				if(item->type == ITT_LRFUNC && item->func != NULL)
				{
					item->func(LEFT_DIR | item->option);
					S_LocalSound(sfx_keyup, NULL);
				}
				else
				{
					// Let's try to change to the previous page.
					if(CurrentMenu->firstItem - CurrentMenu->numVisItems >= 0)
					{
						CurrentMenu->firstItem -= CurrentMenu->numVisItems;
						CurrentItPos -= CurrentMenu->numVisItems;
						// Make a sound, too.
						S_LocalSound(sfx_dorcls, NULL);
					}
				}
				return(true);
				break;
			case DDKEY_RIGHTARROW:
				if(item->type == ITT_LRFUNC && item->func != NULL)
				{
					item->func(RIGHT_DIR | item->option);
					S_LocalSound(sfx_keyup, NULL);
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
						S_LocalSound(sfx_dorcls, NULL);						
					}
				}
				return(true);
				break;
			case DDKEY_ENTER:
				if(item->type == ITT_SETMENU)
				{
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
						if(item->func(item->option))
						{
							if(item->menu != MENU_NONE)
							{
								SetMenu(item->menu);
							}
						}
					}
				}
				S_LocalSound(sfx_dorcls, NULL);
				return(true);
				break;
			case DDKEY_ESCAPE:
				MN_DeactivateMenu();
				return(true);
			case DDKEY_BACKSPACE:
				S_LocalSound(sfx_switch, NULL);
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
/*				for(i = 0; i < CurrentMenu->itemCount; i++)
				{
					if(CurrentMenu->items[i].text)
					{
						if(toupper(key)
							== toupper(CurrentMenu->items[i].text[0]))
						{
							CurrentItPos = i;
							return(true);
						}
					}
				}*/
				for(i = firstVI; i <= lastVI; i++)
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
	CurrentMenu = &MainMenu;
	CurrentItPos = CurrentMenu->oldItPos;
	if(!IS_NETGAME && !Get(DD_PLAYBACK))
	{
		paused = true;
	}
	S_LocalSound(sfx_dorcls, NULL);
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
	if(!IS_NETGAME)
	{
		paused = false;
	}
	S_LocalSound(sfx_dorcls, NULL);
/*	if(soundchanged)
	{
		S_SetMaxVolume(true); //recalc the sound curve
		soundchanged = false;
	}*/
	players[consoleplayer].message = NULL;
	players[consoleplayer].messageTics = 1;
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawInfo
//
//---------------------------------------------------------------------------

void MN_DrawInfo(void)
{
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
	y = menu->y+2+(item * menu->itemHeight);

	GL_SetPatch(W_GetNumForName("M_SLDMD1"));
	GL_DrawRectTiled(x-1, y+1, width*8+2, 13, 8, 13);

	GL_DrawPatch_CS(x-32, y, W_GetNumForName("M_SLDLT"));
	GL_DrawPatch_CS(x + width*8, y, W_GetNumForName("M_SLDRT"));
	GL_DrawPatch_CS(x+4+slot*8, y+7, W_GetNumForName("M_SLDKB"));
}

//---------------------------------------------------------------------------
//
// CCMD CCmdMenuAction
//
//---------------------------------------------------------------------------

int CCmdMenuAction(int argc, char **argv)
{
	// Can we get out of here early?
	if(/*MenuActive == true || */chatmodeon) return true;

	if(!stricmp(argv[0], "infoscreen"))
	{
		SCInfo(0); // start up info screens
		MenuActive = true;
		//fadingOut = false;
	}
	else if(!stricmp(argv[0], "savegame"))
	{
		if(gamestate == GS_LEVEL && !Get(DD_PLAYBACK))
		{
			MenuActive = true;
			FileMenuKeySteal = false;
			MenuTime = 0;
			CurrentMenu = &SaveMenu;
			CurrentItPos = CurrentMenu->oldItPos;
			if(!IS_NETGAME && !Get(DD_PLAYBACK))
			{
				paused = true;
			}
			S_LocalSound(sfx_dorcls, NULL);
			slottextloaded = false; //reload the slot text, when needed
		}
	}
	else if(!stricmp(argv[0], "loadgame"))
	{
		if(SCNetCheck(2))
		{
			MenuActive = true;
			FileMenuKeySteal = false;
			MenuTime = 0;
			CurrentMenu = &LoadMenu;
			CurrentItPos = CurrentMenu->oldItPos;
			if(!IS_NETGAME && !Get(DD_PLAYBACK))
			{
				paused = true;
			}
			S_LocalSound(sfx_dorcls, NULL);
			slottextloaded = false; //reload the slot text, when needed
		}
	}
	else if(!stricmp(argv[0], "soundmenu"))
	{
		MenuActive = true;
		FileMenuKeySteal = false;
		MenuTime = 0;
		CurrentMenu = &Options2Menu;
		CurrentItPos = CurrentMenu->oldItPos;
		if(!IS_NETGAME && !Get(DD_PLAYBACK))
		{
			paused = true;
		}
		S_LocalSound(sfx_dorcls, NULL);
		slottextloaded = false; //reload the slot text, when needed
	}
/*	else if(!stricmp(argv[0], "suicide"))
	{
		Con_Open(false);
		MenuActive = false;
		askforquit = true;
		typeofask = 5; // suicide
		return true;
	}*/
	else if(!stricmp(argv[0], "quicksave"))
	{
		if(gamestate == GS_LEVEL && !Get(DD_PLAYBACK))
		{
			if(!quicksave || quicksave == -1)
			{
				MenuActive = true;
				FileMenuKeySteal = false;
				MenuTime = 0;
				CurrentMenu = &SaveMenu;
				CurrentItPos = CurrentMenu->oldItPos;
				if(!IS_NETGAME && !Get(DD_PLAYBACK))
				{
					paused = true;
				}
				S_LocalSound(sfx_dorcls, NULL);
				slottextloaded = false; //reload the slot text, when needed
				quicksave = -1;
				P_SetMessage(&players[consoleplayer],
					"CHOOSE A QUICKSAVE SLOT", true);
			}
			else
			{
				askforquit = true;
				typeofask = 3;
				if(!IS_NETGAME && !Get(DD_PLAYBACK))
				{
					paused = true;
				}
				S_LocalSound(sfx_chat, NULL);
			}
		}
	}
	else if(!stricmp(argv[0], "endgame"))
	{
		if(gamestate == GS_LEVEL && !Get(DD_PLAYBACK))
		{
			S_LocalSound(sfx_chat, NULL);
			SCEndGame(0);
		}
	}
	else if(!stricmp(argv[0], "toggleMsgs"))
	{
		SCMessages(0);
	}
	else if(!stricmp(argv[0], "quickload"))
	{
		if(!quickload || quickload == -1)
		{
			MenuActive = true;
			FileMenuKeySteal = false;
			MenuTime = 0;
			CurrentMenu = &LoadMenu;
			CurrentItPos = CurrentMenu->oldItPos;
			if(!IS_NETGAME && !Get(DD_PLAYBACK))
			{
				paused = true;
			}
			S_LocalSound(sfx_dorcls, NULL);
			slottextloaded = false; //reload the slot text, when needed
			quickload = -1;
			P_SetMessage(&players[consoleplayer],
				"CHOOSE A QUICKLOAD SLOT", true);
		}
		else
		{
			askforquit = true;
			if(!IS_NETGAME && !Get(DD_PLAYBACK))
			{
				paused = true;
			}
			typeofask = 4;
			S_LocalSound(sfx_chat, NULL);
		}
	}
	else if(!stricmp(argv[0], "quit"))
	{
		if(gamestate == GS_LEVEL)
		{
			SCQuitGame(0);
			S_LocalSound(sfx_chat, NULL);
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
		//SB_PaletteFlash(true); // force change
		// Reset the textures.
		//GL_ClearTextureMem();
		sprintf(cmd, "setgamma %d", gamma);
		Con_Execute(cmd, true);
		/*P_SetMessage(&players[consoleplayer], GammaText[gamma],
			false);*/
	}
	return true;
}
