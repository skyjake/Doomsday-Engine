// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Compiles for jDoom, jHeretic and jHexen.
// Based on the original DOOM/HEXEN menu code.
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// DESCRIPTION:
//  Common selection menu, options, episode etc.
//  Sliders and icons. Kinda widget stuff.
//
//  TODO: Needs a bit of a clean up now everything from each game is in and working
//-----------------------------------------------------------------------------

#ifdef WIN32
#  pragma warning(disable:4018)
#endif

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "Common/hu_stuff.h"
#include "Common/f_infine.h"
#include "Common/am_map.h"
#include "Common/m_swap.h"

#if __JDOOM__
# include "doomdef.h"
# include "jDoom/dstrings.h"
# include "d_main.h"
# include "d_config.h"
# include "g_game.h"
# include "g_common.h"
# include "jDoom/m_argv.h"
# include "jDoom/s_sound.h"
# include "jDoom/doomstat.h"
# include "jDoom/p_local.h"
# include "jDoom/m_menu.h"
# include "jDoom/m_ctrl.h"
# include "jDoom/Mn_def.h"
# include "jDoom/wi_stuff.h"
# include "Common/x_hair.h"
# include "Common/p_saveg.h"
#elif __JHERETIC__
# include "jHeretic/Doomdef.h"
# include "jHeretic/Info.h"
# include "jHeretic/P_local.h"
# include "jHeretic/R_local.h"
# include "jHeretic/Soundst.h"
# include "jHeretic/h_config.h"
# include "jHeretic/Mn_def.h"
# include "jHeretic/m_ctrl.h"
#elif __JHEXEN__
# include "jHexen/h2def.h"
# include "jHexen/p_local.h"
# include "jHexen/r_local.h"
# include "jHexen/soundst.h"
# include "jHexen/h2_actn.h"
# include "jHexen/mn_def.h"
# include "jHexen/m_ctrl.h"
# include "jHexen/x_config.h"
# include "LZSS.h"
#elif __JSTRIFE__
# include "jStrife/h2def.h"
# include "jStrife/p_local.h"
# include "jStrife/r_local.h"
# include "jStrife/soundst.h"
# include "jStrife/h2_actn.h"
# include "jStrife/mn_def.h"
# include "jStrife/d_config.h"
# include "LZSS.h"
#endif

// MACROS ------------------------------------------------------------------

#define DEFCC(name)		int name(int argc, char **argv)
#define OBSOLETE		CVF_HIDE|CVF_NO_ARCHIVE

DEFCC(CCmdMenuAction);

#define SAVESTRINGSIZE 	24
#define CVAR(typ, x)	(*(typ*)Con_GetVariable(x)->ptr)

// QuitDOOM messages
#ifndef __JDOOM__
#define NUM_QUITMESSAGES  0
#endif

// Sounds played in the menu
static int menusnds[] = {
#if __JDOOM__
	sfx_dorcls,			// close menu
	sfx_swtchx,			// open menu
	sfx_swtchn,			// cancel
	sfx_pstop,			// up/down
	sfx_stnmov,			// left/right
	sfx_pistol,			// enter
	sfx_oof				// bad sound (eg can't autosave)
#elif __JHERETIC__
	sfx_chat,
	sfx_switch,
	sfx_chat,
	sfx_switch,
	sfx_switch,
	sfx_stnmov,
	sfx_chat
#elif __JHEXEN__
	SFX_CHAT,
	SFX_PLATFORM_STOP,
	SFX_DOOR_LIGHT_CLOSE,
	SFX_FIGHTER_HAMMER_HITWALL,
	SFX_PICKUP_KEY,
	SFX_FIGHTER_HAMMER_HITWALL,
	SFX_CHAT
#endif
};

// TYPES -------------------------------------------------------------------

typedef struct {
	float *r, *g, *b, *a;
} rgba_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    SetMenu(MenuType_t menu);
void 	InitFonts(void);

void	M_NewGame(int option, void *data);
void	M_Episode(int option, void *data);		// Does nothing in jHEXEN
void 	M_ChooseClass(int option, void *data);		// Does something only in jHEXEN
void 	M_ChooseSkill(int option, void *data);
void 	M_LoadGame(int option, void *data);
void 	M_SaveGame(int option, void *data);
void    M_Options(int option, void *data);
void	M_GameFiles(int option, void *data);		// Does nothing in jDOOM
void 	M_EndGame(int option, void *data);
void    M_ReadThis(int option, void *data);
void    M_ReadThis2(int option, void *data);

#ifndef __JDOOM__
void    M_ReadThis3(int option, void *data);
#endif

void 	M_QuitDOOM(int option, void *data);

void 	M_ToggleVar(int option, void *data);

void 	M_OpenDCP(int option, void *data);
void 	M_ChangeMessages(int option, void *data);
void 	M_AlwaysRun(int option, void *data);
void 	M_LookSpring(int option, void *data);
void 	M_NoAutoAim(int option, void *data);
void 	M_AllowJump(int option, void *data);
void	M_HUDInfo(int option, void *data);
void 	M_HUDScale(int option, void *data);
void 	M_SfxVol(int option, void *data);
void 	M_MusicVol(int option, void *data);
void 	M_SizeDisplay(int option, void *data);
void 	M_SizeStatusBar(int option, void *data);
void 	M_StatusBarAlpha(int option, void *data);
void	M_HUDRed(int option, void *data);
void    M_HUDGreen(int option, void *data);
void    M_HUDBlue(int option, void *data);
void 	M_MouseLook(int option, void *data);
void 	M_MouseLookInverse(int option, void *data);
void 	M_MouseXSensi(int option, void *data);
void 	M_MouseYSensi(int option, void *data);
void 	M_JoyAxis(int option, void *data);
void 	M_JoyLook(int option, void *data);
void 	M_POVLook(int option, void *data);
void 	M_InverseJoyLook(int option, void *data);
void    M_FinishReadThis(int option, void *data);
void 	M_LoadSelect(int option, void *data);
void 	M_SaveSelect(int option, void *data);
void	M_Xhair(int option, void *data);
void 	M_XhairSize(int option, void *data);

#ifdef __JDOOM__
void	M_XhairR(int option, void *data);
void	M_XhairG(int option, void *data);
void	M_XhairB(int option, void *data);
void	M_XhairAlpha(int option, void *data);
#endif

void 	M_StartControlPanel(void);
void    M_ReadSaveStrings(void);
void 	M_DoSave(int slot);
void 	M_DoLoad(int slot);
void    M_QuickSave(void);
void    M_QuickLoad(void);

void 	M_DrawMainMenu(void);
void    M_DrawReadThis1(void);
void    M_DrawReadThis2(void);

#ifndef __JDOOM__
void    M_DrawReadThis3(void);
#endif

void 	M_DrawSkillMenu(void);
void 	M_DrawClassMenu(void);			// Does something only in jHEXEN
void 	M_DrawEpisode(void);			// Does nothing in jHEXEN
void 	M_DrawOptions(void);
void 	M_DrawOptions2(void);
void 	M_DrawGameplay(void);
void 	M_DrawHUDMenu(void);
void 	M_DrawMapMenu(void);
void 	M_DrawMouseMenu(void);
void 	M_DrawJoyMenu(void);
void 	M_DrawLoad(void);
void 	M_DrawSave(void);
void 	M_DrawFilesMenu(void);
void	M_DrawBackground(void);

void 	M_DrawBackgroundBox(int x, int y, int w, int h, float r, float g, float b, float a, boolean background, int border);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean gamekeydown[256];	// The NUMKEYS macro is local to g_game

extern boolean message_dontfuckwithme;
extern boolean chat_on;			// in heads-up code

#ifdef __JDOOM__
extern int joyaxis[3];
#endif

extern int     typein_time;	// in heads-up code

extern char   *borderLumps[];

extern dpatch_t hu_font[HU_FONTSIZE];
extern dpatch_t hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

enum {
	BORDERUP = 1,
	BORDERDOWN
} border_e;

#ifndef __JDOOM__
char   *endmsg[] = {
	"ARE YOU SURE YOU WANT TO QUIT?",
	"ARE YOU SURE YOU WANT TO END THE GAME?",
	"DO YOU WANT TO QUICKSAVE THE GAME NAMED",
	"DO YOU WANT TO QUICKLOAD THE GAME NAMED"
};
#endif

char    gammamsg[5][81];

boolean devparm = false;

boolean inhelpscreens;
boolean menuactive;
int     InfoType;
boolean mn_SuicideConsole;
Menu_t *currentMenu;

// Blocky mode, has default, 0 = high, 1 = normal
int     detailLevel;
int     screenblocks = 10;		// has default

#if __JHERETIC__
static int MenuEpisode;
#endif


// old save description before edit
char    saveOldString[SAVESTRINGSIZE];

char    savegamestrings[10][SAVESTRINGSIZE];

// -1 = no quicksave slot picked!
int     quickSaveSlot;

 // 1 = message to be printed
int     messageToPrint;

// ...and here is the message string!
char   *messageString;
int     messageFinal = false;

// message x & y
int     messx;
int     messy;
int     messageLastMenuActive;

// timed message = no input from user
boolean messageNeedsInput;

void    (*messageRoutine) (int response, void *data);

// old save description before edit
char    saveOldString[SAVESTRINGSIZE];

char    savegamestrings[10][SAVESTRINGSIZE];

// we are going to be entering a savegame string
int     saveStringEnter;
int     saveSlot;				// which slot to save in
int     saveCharIndex;			// which char we're editing

char    endstring[160];

#if __JDOOM__
static char *yesno[3] = { "NO", "YES", "MAYBE?" };
#else
static char *yesno[2] = { "NO", "YES" };
#endif

#if __JDOOM__ || __JHERETIC__
char   *episodemsg;
#endif

#if __JDOOM__ || __JHERETIC__
int     mouseSensitivity;    // has default
#endif

boolean shiftdown;

// Alpha level for the entire menu. Used primarily by M_WriteText2().
float   menu_alpha = 0;
int     menu_color = 0;
float   skull_angle = 0;

int     frame;    // used by any graphic animations that need to be pumped

int     MenuTime;

short   itemOn;    // menu item skull is on
short   previtemOn;    // menu item skull was last on (for restoring when leaving widget control)
short   skullAnimCounter;    // skull animation counter
short   whichSkull;    // which skull to draw

// PRIVATE DATA DEFINITIONS ------------------------------------------------

int        usegamma;

#ifndef __JDOOM__
static int SkullBaseLump;
#endif

#ifdef __JSTRIFE__
static int	cursors = 8;
dpatch_t cursorst[8];
#else
static int	cursors = 2;
dpatch_t cursorst[2];
#endif

static dpatch_t     borderpatches[8];

#if __JHEXEN__
static int MenuPClass;
#endif

static rgba_t widgetcolors[] = {		// ptrs to colors editable with the colour widget
	{ &cfg.automapL0[0], &cfg.automapL0[1], &cfg.automapL0[2], NULL },
	{ &cfg.automapL1[0], &cfg.automapL1[1], &cfg.automapL1[2], NULL },
	{ &cfg.automapL2[0], &cfg.automapL2[1], &cfg.automapL2[2], NULL },
	{ &cfg.automapL3[0], &cfg.automapL3[1], &cfg.automapL3[2], NULL },
	{ &cfg.automapBack[0], &cfg.automapBack[1], &cfg.automapBack[2], &cfg.automapBack[3] },
	{ &cfg.hudColor[0], &cfg.hudColor[1], &cfg.hudColor[2], &cfg.hudColor[3] }
};

static boolean WidgetEdit = false;	// No active widget by default.
static boolean rgba = false;		// used to swap between rgb / rgba modes for the color widget

static int editcolorindex = 0;		// The index of the widgetcolors array of the item being currently edited

static float currentcolor[4] = {0, 0, 0, 0};	// Used by the widget as temporay values

static int menuFogTexture = 0;
static float mfSpeeds[2] = { 1 * .05f, 1 * -.085f };

//static float mfSpeeds[2] = { 1*.05f, 2*-.085f };
static float mfAngle[2] = { 93, 12 };
static float mfPosAngle[2] = { 35, 77 };
static float mfPos[2][2];
static float mfAlpha = 0;

static float mfYjoin = 0.5f;
static boolean updown = true;

//static float   menuScale = .8f;

static float outFade = 0;
static boolean fadingOut = false;
static int menuDarkTicks = 15;
static int slamInTicks = 9;

// used to fade out the background a little when a widget is active
static float menu_calpha = 0;

static boolean FileMenuKeySteal;
static boolean slottextloaded;
//static char SlotText[NUMSAVESLOTS][SLOTTEXTLEN + 2];
//static char oldSlotText[SLOTTEXTLEN + 2];
//static int SlotStatus[NUMSAVESLOTS];
//static int slotptr;
//static int currentSlot;
static int quicksave;
static int quickload;

enum {
#ifndef __JDOOM__
	newgame = 0,
	multiplayer,
	options,
	gamefiles,
	readthis,
	quitdoom,
	main_end
#else
	newgame = 0,
	multiplayer,
	options,
	loadgame,
	savegame,
	readthis,
	quitdoom,
	main_end
#endif
} main_e;

MenuItem_t MainItems[] = {
#if __JDOOM__
	{ITT_EFUNC, "New Game", M_NewGame, 0},
	{ITT_EFUNC, "Multiplayer", SCEnterMultiplayerMenu, 0},
	{ITT_EFUNC, "Options", M_Options, 0},
	{ITT_EFUNC, "Load Game", M_LoadGame, 0},
	{ITT_EFUNC, "Save Game", M_SaveGame, 0},
	{ITT_EFUNC, "Read This!", M_ReadThis, 0},
	{ITT_EFUNC, "Quit Game", M_QuitDOOM, 0}
#elif __JSTRIFE__
	{ITT_EFUNC, "N", M_NewGame, 0, ""},
	{ITT_EFUNC, "M", SCEnterMultiplayerMenu, 0, ""},
	{ITT_EFUNC, "O", M_Options, 0, ""},
	{ITT_EFUNC, "L", M_LoadGame, 0, ""},
	{ITT_EFUNC, "S", M_SaveGame, 0, ""},
	{ITT_EFUNC, "R", M_ReadThis, 0, ""},
	{ITT_EFUNC, "Q", M_QuitDOOM, 0, ""}
#else
	{ITT_EFUNC, "new game", M_NewGame, 0},
	{ITT_EFUNC, "multiplayer", SCEnterMultiplayerMenu, 0},
	{ITT_EFUNC, "options", M_Options, 0},
	{ITT_EFUNC, "game files", M_GameFiles, 0},
	{ITT_EFUNC, "info", M_ReadThis, 0},
	{ITT_EFUNC, "quit game", M_QuitDOOM, 0}
#endif
};

Menu_t MainDef = {
#if __JHEXEN__
	110, 50,
	M_DrawMainMenu,
	6, MainItems,
	0, MENU_NONE,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT_B,
	0, 6
#elif __JHERETIC__ 
	110, 64,
	M_DrawMainMenu,
	6, MainItems,
	0, MENU_NONE,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT_B,
	0, 6
#elif __JSTRIFE__
	97, 64,
	M_DrawMainMenu,
	7, MainItems,
	0, MENU_NONE,
	hu_font_a,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT_B + 1,
	0, 7
#else
	97, 64,
	M_DrawMainMenu,
	7, MainItems,
	0, MENU_NONE,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT_B + 1,
	0, 7
#endif
};

#if __JHEXEN__
MenuItem_t ClassItems[] = {
	{ITT_EFUNC, "FIGHTER", M_ChooseClass, 0},
	{ITT_EFUNC, "CLERIC", M_ChooseClass, 1},
	{ITT_EFUNC, "MAGE", M_ChooseClass, 2}
};

Menu_t ClassDef = {
	66, 66,
	M_DrawClassMenu,
	3, ClassItems,
	0, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT_B + 1,
	0, 3
};
#endif

#ifdef __JHERETIC__
MenuItem_t EpisodeItems[] = {
	{ITT_EFUNC, "city of the damned", M_Episode, 1},
	{ITT_EFUNC, "hell's maw", M_Episode, 2},
	{ITT_EFUNC, "the dome of d'sparil", M_Episode, 3},
	{ITT_EFUNC, "the ossuary", M_Episode, 4},
	{ITT_EFUNC, "the stagnant demesne", M_Episode, 5}
};

Menu_t EpiDef = {
	48, 50,
	M_DrawEpisode,
	3, EpisodeItems,
	0, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT + 1,
	0, 3
};
#elif __JDOOM__
MenuItem_t EpisodeItems[] = {
	// Text defs TXT_EPISODE1..4.
	{ITT_EFUNC, "K", M_Episode, 0 /*, "M_EPI1" */ },
	{ITT_EFUNC, "T", M_Episode, 1 /*, "M_EPI2" */ },
	{ITT_EFUNC, "I", M_Episode, 2 /*, "M_EPI3" */ },
	{ITT_EFUNC, "T", M_Episode, 3 /*, "M_EPI4" */ }
};

Menu_t  EpiDef = {
	48, 63,
	M_DrawEpisode,
	4, EpisodeItems,
	0, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT + 1,
	0, 4
};
#endif


#ifndef __JDOOM__
static MenuItem_t FilesItems[] = {
	{ITT_EFUNC, "load game", M_LoadGame, 0},
	{ITT_EFUNC, "save game", M_SaveGame, 0}
};

static Menu_t FilesMenu = {
	110, 60,
	M_DrawFilesMenu,
	2, FilesItems,
	0, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT + 1,
	0, 2
};
#endif

enum { load_end = NUMSAVESLOTS };

static MenuItem_t LoadItems[] = {
	{ITT_EFUNC, "1", M_LoadSelect, 0, ""},
	{ITT_EFUNC, "2", M_LoadSelect, 1, ""},
	{ITT_EFUNC, "3", M_LoadSelect, 2, ""},
	{ITT_EFUNC, "4", M_LoadSelect, 3, ""},
	{ITT_EFUNC, "5", M_LoadSelect, 4, ""},
	{ITT_EFUNC, "6", M_LoadSelect, 5, ""},
#if __JDOOM__ || __JHERETIC__
	{ITT_EFUNC, "7", M_LoadSelect, 6, ""},
	{ITT_EFUNC, "8", M_LoadSelect, 7, ""}
#endif
};

static Menu_t LoadDef = {
#ifndef __JDOOM__
	80, 30,
#else
	80, 54,
#endif
	M_DrawLoad,
	NUMSAVESLOTS, LoadItems,
	0, MENU_MAIN,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT,
	0, NUMSAVESLOTS
};

static MenuItem_t SaveItems[] = {
	{ITT_EFUNC, "1", M_SaveSelect, 0, ""},
	{ITT_EFUNC, "2", M_SaveSelect, 1, ""},
	{ITT_EFUNC, "3", M_SaveSelect, 2, ""},
	{ITT_EFUNC, "4", M_SaveSelect, 3, ""},
	{ITT_EFUNC, "5", M_SaveSelect, 4, ""},
	{ITT_EFUNC, "6", M_SaveSelect, 5, ""},
#if __JDOOM__ || __JHERETIC__
	{ITT_EFUNC, "7", M_SaveSelect, 6, ""},
	{ITT_EFUNC, "8", M_SaveSelect, 7, ""}
#endif
};

static Menu_t SaveDef = {
#ifndef __JDOOM__
	80, 30,
#else
	80, 54,
#endif
	M_DrawSave,
	NUMSAVESLOTS, SaveItems,
	0, MENU_MAIN,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT,
	0, NUMSAVESLOTS
};

#if __JSTRIFE__
static MenuItem_t SkillItems[] = {
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_baby},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_easy},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_medium},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_hard},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_nightmare}
};

static Menu_t SkillDef = {
	120, 44,
	M_DrawSkillMenu,
	5, SkillItems,
	2, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT,
	0, 5
};

#elif __JHEXEN__
static MenuItem_t SkillItems[] = {
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_baby},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_easy},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_medium},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_hard},
	{ITT_EFUNC, NULL, M_ChooseSkill, sk_nightmare}
};

static Menu_t SkillDef = {
	120, 44,
	M_DrawSkillMenu,
	5, SkillItems,
	2, MENU_CLASS,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT,
	0, 5
};
#elif __JHERETIC__
static MenuItem_t SkillItems[] = {
	{ITT_EFUNC, "thou needet a wet-nurse", M_ChooseSkill, sk_baby},
	{ITT_EFUNC, "yellowbellies-r-us", M_ChooseSkill, sk_easy},
	{ITT_EFUNC, "bringest them oneth", M_ChooseSkill, sk_medium},
	{ITT_EFUNC, "thou art a smite-meister", M_ChooseSkill, sk_hard},
	{ITT_EFUNC, "black plague possesses thee", M_ChooseSkill, sk_nightmare}
};

static Menu_t SkillDef = {
	38, 30,
	M_DrawSkillMenu,
	5, SkillItems,
	2, MENU_EPISODE,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT,
	0, 5
};
#elif __JDOOM__
static MenuItem_t SkillItems[] = {
	{ITT_EFUNC, "I", M_ChooseSkill, 0, "M_JKILL"},
	{ITT_EFUNC, "H", M_ChooseSkill, 1, "M_ROUGH"},
	{ITT_EFUNC, "H", M_ChooseSkill, 2, "M_HURT"},
	{ITT_EFUNC, "U", M_ChooseSkill, 3, "M_ULTRA"},
	{ITT_EFUNC, "N", M_ChooseSkill, 4, "M_NMARE"}
};

static Menu_t SkillDef = {
	48, 63,
	M_DrawSkillMenu,
	5, SkillItems,
	2, MENU_EPISODE,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT,
	0, 5
};
#endif

static MenuItem_t OptionsItems[] = {
	{ITT_EFUNC, "end game", M_EndGame, 0},
	{ITT_EFUNC, "control panel", M_OpenDCP, 0},
	{ITT_SETMENU, "gameplay...", NULL, MENU_GAMEPLAY},
	{ITT_SETMENU, "hud...", NULL, MENU_HUD},
	{ITT_SETMENU, "automap...", NULL, MENU_MAP},
	{ITT_SETMENU, "sound...", NULL, MENU_OPTIONS2},
	{ITT_SETMENU, "controls...", NULL, MENU_CONTROLS},
	{ITT_SETMENU, "mouse options...", NULL, MENU_MOUSE},
	{ITT_SETMENU, "joystick options...", NULL, MENU_JOYSTICK}
};

static Menu_t OptionsDef = {
	98, 84,
	M_DrawOptions,
	9, OptionsItems,
	0, MENU_MAIN,
	hu_font_a,					//1, 0, 0,
	cfg.menuColor2,
	LINEHEIGHT_A,
	0, 9
};

static MenuItem_t Options2Items[] = {
	{ITT_LRFUNC, "SFX VOLUME :", M_SfxVol, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "MUSIC VOLUME :", M_MusicVol, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_EFUNC, "OPEN AUDIO PANEL", M_OpenDCP, 1},
};

static Menu_t Options2Def = {
#if __JHEXEN__ || __JSTRIFE__
	70, 25,
#elif __JHERETIC__
	70, 30,
#elif __JDOOM__
	70, 40,
#endif
	M_DrawOptions2,
#ifndef __JDOOM__
	7, Options2Items,
#else
	3, Options2Items,
#endif
	0, MENU_OPTIONS,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT_A,
#ifndef __JDOOM__
	0, 7
#else
	0, 3
#endif
};

MenuItem_t ReadItems1[] = {
	{ITT_EFUNC, "", M_ReadThis2, 0}
};

Menu_t  ReadDef1 = {
	280, 185,
	M_DrawReadThis1,
	1, ReadItems1,
	0, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT,
	0, 1
};

MenuItem_t ReadItems2[] = {
#ifdef __JDOOM__					// heretic and hexen have 3 readthis screens.
	{ITT_EFUNC, "", M_FinishReadThis, 0}
#else
	{ITT_EFUNC, "", M_ReadThis3, 0}
#endif
};

Menu_t  ReadDef2 = {
	330, 175,
	M_DrawReadThis2,
	1, ReadItems2,
	0, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT,
	0, 1
};

#ifndef __JDOOM__
MenuItem_t ReadItems3[] = {
	{ITT_EFUNC, "", M_FinishReadThis, 0}
};

Menu_t  ReadDef3 = {
	330, 175,
	M_DrawReadThis3,
	1, ReadItems3,
	0, MENU_MAIN,
	hu_font_b,					//1, 0, 0,
	cfg.menuColor,
	LINEHEIGHT,
	0, 1
};
#endif

static MenuItem_t HUDItems[] = {
#ifdef __JDOOM__
	{ITT_EFUNC, "show ammo :", M_HUDInfo, HUD_AMMO},
	{ITT_EFUNC, "show armor :", M_HUDInfo, HUD_ARMOR},
	{ITT_EFUNC, "show face :", M_HUDInfo, HUD_FACE},
	{ITT_EFUNC, "show health :", M_HUDInfo, HUD_HEALTH},
	{ITT_EFUNC, "show keys :", M_HUDInfo, HUD_KEYS},
	{ITT_LRFUNC, "scale", M_HUDScale, 0},
	{ITT_EFUNC, "   HUD color", SCColorWidget, 5},
#endif
	{ITT_EFUNC, "MESSAGES :", M_ChangeMessages, 0},
	{ITT_LRFUNC, "CROSSHAIR :", M_Xhair, 0},
	{ITT_LRFUNC, "CROSSHAIR SIZE :", M_XhairSize, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "SCREEN SIZE", M_SizeDisplay, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "STATUS BAR SIZE", M_SizeStatusBar, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "STATUS BAR ALPHA :", M_StatusBarAlpha, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
#if __JHEXEN__ || __JSTRIFE__
	{ITT_INERT, "FULLSCREEN HUD",	NULL, 0},
	{ITT_INERT, "FULLSCREEN HUD",	NULL, 0},
	{ITT_EFUNC, "SHOW MANA :", M_HUDInfo, HUD_MANA},
	{ITT_EFUNC, "SHOW HEALTH :", M_HUDInfo, HUD_HEALTH},
	{ITT_EFUNC, "SHOW ARTIFACT :", M_HUDInfo, HUD_ARTI},
	{ITT_EFUNC, "   HUD COLOUR", SCColorWidget, 5},
	{ITT_LRFUNC, "SCALE", M_HUDScale, 0},
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#elif __JHERETIC__
	{ITT_INERT, "FULLSCREEN HUD",	NULL, 0},
	{ITT_INERT, "FULLSCREEN HUD",	NULL, 0},
	{ITT_EFUNC, "SHOW AMMO :", M_HUDInfo, HUD_AMMO},
	{ITT_EFUNC, "SHOW ARMOR :", M_HUDInfo, HUD_ARMOR},
	{ITT_EFUNC, "SHOW ARTIFACT :", M_HUDInfo, HUD_ARTI},
	{ITT_EFUNC, "SHOW HEALTH :", M_HUDInfo, HUD_HEALTH},
	{ITT_EFUNC, "SHOW KEYS :", M_HUDInfo, HUD_KEYS},
	{ITT_EFUNC, "   HUD COLOUR", SCColorWidget, 5},
	{ITT_LRFUNC, "SCALE", M_HUDScale, 0},
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0}
#endif
};

#ifdef __JDOOM__
Menu_t ControlsDef = {
	32, 40,
	M_DrawControlsMenu,
	73, ControlsItems,
	1, MENU_OPTIONS,
	hu_font_a,					//1, 0, 0, 
	cfg.menuColor2,
	LINEHEIGHT_A,
	0, 16
};
#endif

#ifdef __JHERETIC__
Menu_t ControlsDef = {
	32, 26,
	M_DrawControlsMenu,
	92, ControlsItems,
	1, MENU_OPTIONS,
	hu_font_a,					//1, 0, 0,
	cfg.menuColor2,
	LINEHEIGHT_A,
	0, 17
};
#endif

#ifdef __JHEXEN__
Menu_t ControlsDef = {
	32, 21,
	M_DrawControlsMenu,
	92, ControlsItems,
	1, MENU_OPTIONS,
	hu_font_a,					//1, 0, 0,
	cfg.menuColor2, 
	LINEHEIGHT_A,
	0, 17
};
#endif

static Menu_t HUDDef = {
#ifndef __JDOOM__
	64, 30,
#else
	70, 40,
#endif
	M_DrawHUDMenu,
#if __JHEXEN__ || __JSTRIFE__
	23, HUDItems,
#elif __JHERETIC__
	25, HUDItems,
#elif __JDOOM__
	13, HUDItems,
#endif
	0, MENU_OPTIONS,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT_A,
#if __JHEXEN__ || __JSTRIFE__
	0, 15		// 21
#elif __JHERETIC__
	0, 15		// 23
#elif __JDOOM__
	0, 13
#endif
};


static MenuItem_t GameplayItems[] = {
	{ITT_EFUNC, "ALWAYS RUN :", M_AlwaysRun, 0},
	{ITT_EFUNC, "LOOKSPRING :", M_LookSpring, 0},
	{ITT_EFUNC, "AUTOAIM :", M_NoAutoAim, 0},
#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
	{ITT_EFUNC, "JUMPING :", M_AllowJump, 0},
#endif

#if __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, "COMPATIBILITY", NULL, 0 },
	{ITT_EFUNC, "AV RESURRECTS GHOSTS:", M_ToggleVar, 0, NULL, &cfg.raiseghosts},
	{ITT_EFUNC, "PE LIMITED TO 20 LOST SOULS :", M_ToggleVar, 0, NULL, &cfg.maxskulls},
	{ITT_EFUNC, "LS GET STUCK INSIDE WALLS :", M_ToggleVar, 0, NULL, &cfg.allowskullsinwalls},
	{ITT_EFUNC, "CORPSES SLIDE DOWN STAIRS :", M_ToggleVar, 0, NULL, &cfg.slidingCorpses}
#endif
};

#if __JHEXEN__
static Menu_t GameplayDef = {
	64, 25,
	M_DrawGameplay,
	3, GameplayItems,
	0, MENU_OPTIONS,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT_A,
	0, 3
};
#else
static Menu_t GameplayDef = {
#if __JHERETIC__
	72, 30,
#else
	64, 40,
#endif
	M_DrawGameplay,
#if __JDOOM__
	10, GameplayItems,
#else
	4, GameplayItems,
#endif
	0, MENU_OPTIONS,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT_A,
#if __JDOOM__
	0, 10
#else
	0, 4
#endif
};
#endif

static MenuItem_t MouseOptsItems[] = {
	{ITT_EFUNC, "MOUSE LOOK :", M_MouseLook, 0},
	{ITT_EFUNC, "INVERSE MLOOK :", M_MouseLookInverse, 0},
	{ITT_LRFUNC, "X SENSITIVITY", M_MouseXSensi, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "Y SENSITIVITY", M_MouseYSensi, 0},
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0}
#endif
};

static Menu_t MouseOptsMenu = {
#if __JHEXEN__ || __JSTRIFE__
	72, 25,
#elif __JHERETIC__
	72, 30,
#elif __JDOOM__
	70, 40,
#endif
	M_DrawMouseMenu,
#ifndef __JDOOM__
	8, MouseOptsItems,
#else
	4, MouseOptsItems,
#endif
	0, MENU_OPTIONS,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT_A,
#ifndef __JDOOM__
	0, 8
#else
	0, 4
#endif
};

static MenuItem_t JoyConfigItems[] = {
	{ITT_LRFUNC, "X AXIS :", M_JoyAxis, 0 << 8},
	{ITT_LRFUNC, "Y AXIS :", M_JoyAxis, 1 << 8},
	{ITT_LRFUNC, "Z AXIS :", M_JoyAxis, 2 << 8},
	{ITT_LRFUNC, "RX AXIS :", M_JoyAxis, 3 << 8},
	{ITT_LRFUNC, "RY AXIS :", M_JoyAxis, 4 << 8},
	{ITT_LRFUNC, "RZ AXIS :", M_JoyAxis, 5 << 8},
	{ITT_LRFUNC, "SLIDER 1 :", M_JoyAxis, 6 << 8},
	{ITT_LRFUNC, "SLIDER 2 :", M_JoyAxis, 7 << 8},
	{ITT_EFUNC, "JOY LOOK :", M_JoyLook, 0},
	{ITT_EFUNC, "INVERSE LOOK :", M_InverseJoyLook, 0},
	{ITT_EFUNC, "POV LOOK :", M_POVLook, 0}
};

static Menu_t JoyConfigMenu = {
#if __JHEXEN__ || __JSTRIFE__
	72, 25,
#elif __JHERETIC__
	80, 30,
#elif __JDOOM__
	70, 40,
#endif
	M_DrawJoyMenu,
	11, JoyConfigItems,
	0, MENU_OPTIONS,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT_A,
	0, 11
};

Menu_t* menulist[] = {
	&MainDef,
#if __JHEXEN__
	&ClassDef,
#endif
#if __JDOOM__ || __JHERETIC__
	&EpiDef,
#endif
	&SkillDef,
	&OptionsDef,
	&Options2Def,
	&GameplayDef,
	&HUDDef,
	&MapDef,
	&ControlsDef,
	&MouseOptsMenu,
	&JoyConfigMenu,
#ifndef __JDOOM__
	&FilesMenu,
#endif
	&LoadDef,
	&SaveDef,
	&MultiplayerMenu,
	&GameSetupMenu,
	&PlayerSetupMenu,
	NULL
};

#ifdef __JDOOM__
void M_SetNumItems(Menu_t * menu, int num)
{
	menu->itemCount = menu->numVisItems = num;
}
#endif

static MenuItem_t ColorWidgetItems[] = {
	{ITT_LRFUNC, "red :    ", M_WGCurrentColor, 0, NULL, &currentcolor[0] },
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "green :", M_WGCurrentColor, 0, NULL, &currentcolor[1] },
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "blue :  ", M_WGCurrentColor, 0, NULL, &currentcolor[2] },
#ifndef __JDOOM__
	{ITT_EMPTY, NULL, NULL, 0},
	{ITT_EMPTY, NULL, NULL, 0},
#endif
	{ITT_LRFUNC, "alpha :", M_WGCurrentColor, 0, NULL, &currentcolor[3] },
};

static Menu_t ColorWidgetMnu = {
	98, 60,
	NULL,
#ifndef __JDOOM__
	10, ColorWidgetItems,
#else
	4, ColorWidgetItems,
#endif
	0, MENU_OPTIONS,
	hu_font_a,
	cfg.menuColor2,
	LINEHEIGHT_A,
#ifndef __JDOOM__
	0, 10
#else
	0, 4
#endif
};

// Cvars for the menu
cvar_t menuCVars[] = 
{
	// Old names (obsolete) -------------------------------------------------------

	{"flash_R", OBSOLETE, CVT_FLOAT, &cfg.flashcolor[0], 0, 1,
        "Menu selection flash color, red component."},
	{"flash_G", OBSOLETE, CVT_FLOAT, &cfg.flashcolor[1], 0, 1,
        "Menu selection flash color, green component."},
	{"flash_B", OBSOLETE, CVT_FLOAT, &cfg.flashcolor[2], 0, 1,
        "Menu selection flash color, blue component."},
	{"flash_Speed", OBSOLETE, CVT_INT, &cfg.flashspeed, 0, 50,
        "Menu selection flash speed."},
	{"MenuScale", OBSOLETE, CVT_FLOAT, &cfg.menuScale, .1f, 1,
        "Scaling for menus."},
	{"MenuEffects", OBSOLETE, CVT_INT, &cfg.menuEffects, 0, 2,
        "Disable menu effects: 1=type-in, 2=all."},
	{"Menu_R", OBSOLETE, CVT_FLOAT, &cfg.menuColor[0], 0, 1,
        "Menu color red component."},
	{"Menu_G", OBSOLETE, CVT_FLOAT, &cfg.menuColor[1], 0, 1,
        "Menu color green component."},
	{"Menu_B", OBSOLETE, CVT_FLOAT, &cfg.menuColor[2], 0, 1,
        "Menu color blue component."},
    {"MenuFog", OBSOLETE, CVT_INT, &cfg.menuFog, 0, 1,
        "Menu fog mode: 0=blue vertical, 1=black smoke."},

	// NEW names ------------------------------------------------------------------

	{"menu-scale", 0, CVT_FLOAT, &cfg.menuScale, .1f, 1,
			"Scaling for menus."},
	{"menu-flash-r", 0, CVT_FLOAT, &cfg.flashcolor[0], 0, 1,
			"Menu selection flash color, red component."},
	{"menu-flash-g", 0, CVT_FLOAT, &cfg.flashcolor[1], 0, 1,
			"Menu selection flash color, green component."},
	{"menu-flash-b", 0, CVT_FLOAT, &cfg.flashcolor[2], 0, 1,
			"Menu selection flash color, blue component."},
	{"menu-flash-speed", 0, CVT_INT, &cfg.flashspeed, 0, 50,
			"Menu selection flash speed."},
	{"menu-turningskull", 0, CVT_BYTE, &cfg.turningSkull, 0, 1,
			"1=Menu skull turns at slider items."},
	{"menu-effect", 0, CVT_INT, &cfg.menuEffects, 0, 2,
			"Disable menu effects: 1=type-in, 2=all."},
	{"menu-color-r", 0, CVT_FLOAT, &cfg.menuColor[0], 0, 1,
			"Menu color red component."},
	{"menu-color-g", 0, CVT_FLOAT, &cfg.menuColor[1], 0, 1,
			"Menu color green component."},
	{"menu-color-b", 0, CVT_FLOAT, &cfg.menuColor[2], 0, 1,
			"Menu color blue component."},
	{"menu-colorb-r", 0, CVT_FLOAT, &cfg.menuColor2[0], 0, 1,
			"Menu color B red component."},
	{"menu-colorb-g", 0, CVT_FLOAT, &cfg.menuColor2[1], 0, 1,
			"Menu color B green component."},
	{"menu-colorb-b", 0, CVT_FLOAT, &cfg.menuColor2[2], 0, 1,
			"Menu color B blue component."},
	{"menu-glitter", 0, CVT_FLOAT, &cfg.menuGlitter, 0, 1,
			"Strength of type-in glitter."},
	{"menu-fog", 0, CVT_INT, &cfg.menuFog, 0, 4,
			"Menu fog mode: 0=shimmer, 1=black smoke, 2=blue vertical, 3=grey smoke, 4=dimmed."},
	{"menu-shadow", 0, CVT_FLOAT, &cfg.menuShadow, 0, 1,
			"Menu text shadow darkness."},
	{"menu-patch-replacement", 0, CVT_BYTE, &cfg.usePatchReplacement, 0, 1,
			"1=Enable the Patch Replacement strings."},
	{"menu-slam", 0, CVT_BYTE, &cfg.menuSlam, 0, 1,
			"1=Slam the menu when opening."},
#ifdef __JDOOM__
	{"menu-quitsound", 0, CVT_INT, &cfg.menuQuitSound, 0, 1,
			"1=Play a sound when quitting the game."},
#endif

	{NULL}
};

// Console commands for the menu
ccmd_t  menuCCmds[] = {
	{"helpscreen",   CCmdMenuAction, "Show the Help screens."},
	{"savegame",     CCmdMenuAction, "Open the save game menu."},
	{"loadgame",     CCmdMenuAction, "Open the load game menu."},
	{"soundmenu",    CCmdMenuAction, "Open the sound settings menu."},
	{"quicksave",    CCmdMenuAction, "Quicksave the game."},
	{"endgame",      CCmdMenuAction, "End the game."},
	{"togglemsgs",   CCmdMenuAction, "Messages on/off."},
	{"quickload",    CCmdMenuAction, "Load the quicksaved game."},
	{"quit",         CCmdMenuAction, "Quit the game and return to the OS."},
	{"togglegamma",  CCmdMenuAction, "Cycle gamma correction levels."},
	{NULL}
};

// Code -------------------------------------------------------------------

/*
 *  MN_Register:
 *
 *		Called during the PreInit of each game during start up
 *
 *		Register Cvars and CCmds for the opperation/look of the menu.
 *		The cvars/ccmds for options modified within the menu are in the main ccmd/cvar arrays
 */
void MN_Register(void)
{
	int     i;

	for(i = 0; menuCVars[i].name; i++)
		Con_AddVariable(menuCVars + i);
	for(i = 0; menuCCmds[i].name; i++)
		Con_AddCommand(menuCCmds + i);
}

/*
 *  M_LoadData:
 *
 *		Load any resources the menu needs on init
 */
void M_LoadData(void)
{
	int	i;
	char    buffer[9];

	// Load the cursor patches
	for(i = 0; i < cursors; i++)
	{
		sprintf(buffer, CURSORPREF, i+1);
    		R_CachePatch(&cursorst[i], buffer);
	}

	if(!menuFogTexture && !Get(DD_NOVIDEO))
	{
		menuFogTexture = gl.NewTexture();
		gl.TexImage(DGL_LUMINANCE, 64, 64, 0,
					W_CacheLumpName("menufog", PU_CACHE));
		gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
		gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
	}

	// Load the border patches
	for(i=1; i < 9; i++)
		R_CachePatch(&borderpatches[i-1], borderLumps[i]);
}

/*
 *  M_UnloadData:
 *
 *		The opposite of M_LoadData()
 */
void M_UnloadData(void)
{
	if(Get(DD_NOVIDEO))
		return;
	if(menuFogTexture)
		gl.DeleteTextures(1, (DGLuint*) &menuFogTexture);
	menuFogTexture = 0;
}

/*
 *  MN_Init:
 *
 *		Init vars, fonts, adjust the menu structs, and anything else that
 *		needs to be done before the menu can be used. 
 */
void MN_Init(void)
{
	MenuItem_t *item;

#if __JDOOM__ || __JHERETIC__
	int   i, w, maxw;
#endif

#if __JDOOM__ || __JHERETIC__
	// Init some strings.
	for(i = 0; i < 5; i++)
		strcpy(gammamsg[i], GET_TXT(TXT_GAMMALVL0 + i));
#endif

#ifdef __JDOOM__
	// Quit messages.
	endmsg[0] = GET_TXT(TXT_QUITMSG);
	for(i = 1; i <= NUM_QUITMESSAGES; i++)
		endmsg[i] = GET_TXT(TXT_QUITMESSAGE1 + i - 1);
#endif
#ifdef __JHERETIC__
	// Episode names.
	for(i = 0, maxw = 0; i < 4; i++)
	{
		EpisodeItems[i].text = GET_TXT(TXT_EPISODE1 + i);
		w = M_StringWidth(EpisodeItems[i].text, hu_font_b);
		if(w > maxw)
			maxw = w;
	}
	// Center the episodes menu appropriately.
	EpiDef.x = 160 - maxw / 2 + 12;
	// "Choose Episode"

	//episodemsg = GET_TXT(TXT_ASK_EPISODE);
#elif __JDOOM__
	// Episode names.
	for(i = 0, maxw = 0; i < 4; i++)
	{
		EpisodeItems[i].text = GET_TXT(TXT_EPISODE1 + i);
		w = M_StringWidth(EpisodeItems[i].text, hu_font_b);
		if(w > maxw)
			maxw = w;
	}
	// Center the episodes menu appropriately.
	EpiDef.x = 160 - maxw / 2 + 12;
	// "Choose Episode"
	episodemsg = GET_TXT(TXT_ASK_EPISODE);
#endif

	M_LoadData();

	currentMenu = &MainDef;
	menuactive = false;
	menu_alpha = 0;
	mfAlpha = 0;
	itemOn = currentMenu->lastOn;
	whichSkull = 0;
	skullAnimCounter = 10;
	messageToPrint = 0;
	messageString = NULL;
	messageLastMenuActive = menuactive;
	quickSaveSlot = -1;

#ifdef __JDOOM__
	// Here we could catch other version dependencies,
	//  like HELP1/2, and four episodes.

	switch (gamemode)
	{
	case commercial:
		// This is used because DOOM 2 had only one HELP
		//  page. I use CREDIT as second page now, but
		//  kept this hack for educational purposes.
		item = &MainItems[readthis];
		item->func = M_QuitDOOM;
		item->text = "Quit Game";
		M_SetNumItems(&MainDef, 6);
		MainDef.y = 64 + 8;
		SkillDef.prevMenu = MENU_MAIN;
		ReadDef1.drawFunc = M_DrawReadThis1;
		ReadDef1.x = 330;
		ReadDef1.y = 165;
		ReadItems1[0].func = M_FinishReadThis;
		break;
	case shareware:
		// Episode 2 and 3 are handled,
		//  branching to an ad screen.
	case registered:
		// We need to remove the fourth episode.
		M_SetNumItems(&EpiDef, 3);
		item = &MainItems[readthis];
		item->func = M_ReadThis;
		item->text = "Read This!";
		M_SetNumItems(&MainDef, 7);
		MainDef.y = 64;
		break;
	case retail:
		// We are fine.
		M_SetNumItems(&EpiDef, 4);
	default:
		break;
	}
#else
		item = &MainItems[readthis];
		item->func = M_ReadThis;
#endif

#ifndef __JDOOM__
	SkullBaseLump = W_GetNumForName(SKULLBASELMP);
#endif

#ifdef __JHERETIC__
	if(ExtendedWAD)
	{							// Add episodes 4 and 5 to the menu
		EpiDef.itemCount = EpiDef.numVisItems = 5;
		EpiDef.y = 50 - ITEM_HEIGHT;
	}
#endif

}

/*
 *  MN_Ticker:
 *
 *		Updates on Game Tick.
 */
void MN_Ticker(void)
{
	int     i;

	for(i = 0; i < 2; i++)
	{
		if(cfg.menuFog == 1)
		{
			mfAngle[i] += mfSpeeds[i] / 4;
			mfPosAngle[i] -= mfSpeeds[!i];
			mfPos[i][VX] = 160 + 120 * cos(mfPosAngle[i] / 180 * PI);
			mfPos[i][VY] = 100 + 100 * sin(mfPosAngle[i] / 180 * PI);
		}
		else
		{
			mfAngle[i] += mfSpeeds[i] / 4;
			mfPosAngle[i] -= 1.5f * mfSpeeds[!i];
			mfPos[i][VX] = 320 + 320 * cos(mfPosAngle[i] / 180 * PI);
			mfPos[i][VY] = 240 + 240 * sin(mfPosAngle[i] / 180 * PI);
		}
	}

	typein_time++;

	// Fade in/out the widget background filter  
	if(WidgetEdit)
	{
		if(menu_calpha < 0.5f)
			menu_calpha += .1f;
		if(menu_calpha > 0.5f)
			menu_calpha = 0.5f;
	}
	else
	{
		if(menu_calpha > 0)
			menu_calpha -= .1f;
		if(menu_calpha < 0)
			menu_calpha = 0;
	}

	// Smooth the menu & fog alpha on a curved ramp
	if(menuactive && !messageToPrint)
	{

		if(mfAlpha < (cfg.menuFog == 3 ? 0.65f : 1))
			mfAlpha = (mfAlpha * 1.2f) + .01f;
		if(mfAlpha > (cfg.menuFog == 3 ? 0.65f : 1))

			mfAlpha = (cfg.menuFog == 3 ? 0.65f : 1);

		if(menu_alpha < 1)
			menu_alpha += .1f;
		if(menu_alpha > 1)
			menu_alpha = 1;
	}
	else
	{
		if(mfAlpha > 0)
			mfAlpha = mfAlpha / 1.1f;
		if(mfAlpha < 0)
			mfAlpha = 0;
		if(menu_alpha > 0)
			menu_alpha -= .1f;
		if(menu_alpha < 0)
			menu_alpha = 0;
	}

	// Calculate the height of the menuFog 3 Y join
	if((menuactive || mfAlpha > 0) && updown && mfYjoin > 0.46f)
		mfYjoin = mfYjoin / 1.002f;
	else if((menuactive || mfAlpha > 0) && !updown && mfYjoin < 0.54f )
		mfYjoin = mfYjoin * 1.002f;

	if((menuactive || mfAlpha > 0) && (mfYjoin < 0.46f || mfYjoin > 0.54f))
		updown = !updown;

	// menu zoom in/out
	if(!menuactive && mfAlpha > 0)
	{
		outFade += 1 / (float) slamInTicks;
		if(outFade > 1)
			fadingOut = false;
	}

	// animate the cursor patches
	if(--skullAnimCounter <= 0)
	{
		whichSkull++;
		skullAnimCounter = 8;
		if (whichSkull > cursors-1)
			whichSkull = 0;
	}

	if(menuactive || mfAlpha > 0)
	{
		float   rewind = 20;

		MenuTime++;

		menu_color += cfg.flashspeed;
		if(menu_color >= 100)
			menu_color -= 100;

		if(cfg.turningSkull && currentMenu->items[itemOn].type == ITT_LRFUNC)
			skull_angle += 5;
		else if(skull_angle != 0)
		{
			if(skull_angle <= rewind || skull_angle >= 360 - rewind)
				skull_angle = 0;
			else if(skull_angle < 180)
				skull_angle -= rewind;
			else
				skull_angle += rewind;
		}
		if(skull_angle >= 360)
			skull_angle -= 360;

		// Used for jHeretic's rotating skulls
		frame = (MenuTime / 3) % 18;
	}
	MN_TickerEx();
}

/*
 *  M_SetMenuMatrix:
 *
 *		Sets the current view matrix up for rendering the menu
 */
void M_SetMenuMatrix(float time)
{
	boolean allowScaling = (currentMenu != &ReadDef1 && currentMenu != &ReadDef2
#ifndef __JDOOM__
                            && currentMenu != &ReadDef3
#endif
                            );

    // Use a plain 320x200 projection.
    gl.MatrixMode(DGL_PROJECTION);
    gl.LoadIdentity();
    gl.Ortho(0, 0, 320, 200, -1, 1);

	// Draw menu background.
	if(mfAlpha)
		M_DrawBackground();

	//gl.PopMatrix();

	//gl.MatrixMode(DGL_PROJECTION);
    //gl.PushMatrix();
    //gl.LoadIdentity();
    //gl.Ortho(0, 0, 320, 200, -1, 1);

	if(allowScaling)
	{
		// Scale by the menuScale.
        gl.MatrixMode(DGL_MODELVIEW);
		gl.Translatef(160, 100, 0);

		if(cfg.menuSlam){
			if(time > 1 && time <= 2)
			{
				time = 2 - time;
				gl.Scalef(cfg.menuScale * (.9f + time * .1f),
				  	cfg.menuScale * (.9f + time * .1f), 1);
			} else {
				gl.Scalef(cfg.menuScale * (2 - time),
				  	cfg.menuScale * (2 - time), 1);			
			}
		} else
			gl.Scalef(cfg.menuScale, cfg.menuScale, 1);

		gl.Translatef(-160, -100, 0);
	}
}

/*
 * This is the main menu drawing routine (called every tic by the drawing loop)
 * Draws the current menu 'page' by calling the funcs attached to each 
 * menu item.
 *
 * Also draws any current menu message 'Are you sure you want to quit?'
 */
void M_Drawer(void)
{
	static short x;
	static short y;
	short   i;
	short   max;
	char    string[40];
	int     start;
	float   scale;
	int     w, h, off_x, off_y;

	int     effTime = (MenuTime > menuDarkTicks) ? menuDarkTicks : MenuTime;
	float   temp = .5 * effTime / (float) menuDarkTicks;

	boolean allowScaling = (currentMenu != &ReadDef1 && currentMenu != &ReadDef2
#ifndef __JDOOM__
                            && currentMenu != &ReadDef3
#endif
                            );

	inhelpscreens = false;

    // FIXME: This FSP indicator is obsolete since the engine can display a 
    // much more accurate frame rate counter.
	if(cfg.showFPS)
	{
		char    fpsbuff[80];

		sprintf(fpsbuff, "%d FPS", DD_GetFrameRate());
		M_WriteText(320 - M_StringWidth(fpsbuff, hu_font_a), 0, fpsbuff);
		GL_Update(DDUF_TOP);
	}

	if(!menuactive && menu_alpha > 0 )  // fading out
	{
		temp = outFade + 1;
	} 
    else 
    {
		effTime = (MenuTime > slamInTicks) ? slamInTicks : MenuTime;
		temp = (effTime / (float) slamInTicks);
	}

    // These are popped in the end of the function.
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

	// Setup matrix.
	if(messageToPrint || ( menuactive || (menu_alpha > 0 || mfAlpha > 0)) )
		M_SetMenuMatrix(messageToPrint? 1 : temp);	// don't slam messages

	// Horiz. & Vertically center string and print it.
	if(messageToPrint)
	{
		start = 0;
		y = 100 - M_StringHeight(messageString, hu_font_a) / 2;
		while(*(messageString + start))
		{
			for(i = 0; i < strlen(messageString + start); i++)
				if(*(messageString + start + i) == '\n')
				{
					memset(string, 0, 40);
					strncpy(string, messageString + start, i);
					start += i + 1;
					break;
				}

			if(i == strlen(messageString + start))
			{
				strcpy(string, messageString + start);
				start += i;
			}

			x = 160 - M_StringWidth(string, hu_font_a) / 2;
			M_WriteText2(x, y, string, hu_font_a, cfg.menuColor2[0],
						 cfg.menuColor2[1], cfg.menuColor2[2], 1);
			y += SHORT(hu_font_a[17].height);
		}

		goto end_draw_menu;
	}

	if(!menuactive && menu_alpha == 0 && mfAlpha == 0)
		goto end_draw_menu;

	if(currentMenu->drawFunc)
		currentMenu->drawFunc();	// call Draw routine

	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y;
	max = currentMenu->itemCount;

    if(menu_alpha > 0)
	{
		for(i = currentMenu->firstItem;
			i < max && i < currentMenu->firstItem + currentMenu->numVisItems; i++)
		{
			if(currentMenu->items[i].lumpname)
			{
				if(currentMenu->items[i].lumpname[0])
				{
					WI_DrawPatch(x, y, 1, 1, 1, menu_alpha, 
                        W_GetNumForName(currentMenu->items[i].lumpname));
				}
			}
			else if(currentMenu->items[i].text)
			{
				float   t, r, g, b;

				// Which color?
				if(currentMenu->items[i].type == ITT_EMPTY ||
						currentMenu->items[i].type == ITT_INERT)
				{
	#ifndef __JDOOM__
					r = cfg.menuColor[0];
					g = cfg.menuColor[1];
					b = cfg.menuColor[2];
	#else
					// FIXME
					r = 1;
					g = .7f;
					b = .3f;
	#endif
				}
				else if(itemOn == i && !WidgetEdit)
				{
					// Selection!
					if(menu_color <= 50)
						t = menu_color / 50.0f;
					else
						t = (100 - menu_color) / 50.0f;
					r = currentMenu->color[0] * t + cfg.flashcolor[0] * (1 - t);
					g = currentMenu->color[1] * t + cfg.flashcolor[1] * (1 - t);
					b = currentMenu->color[2] * t + cfg.flashcolor[2] * (1 - t);
				}
				else
				{
					r = currentMenu->color[0];
					g = currentMenu->color[1];
					b = currentMenu->color[2];
                }

				// changed from font[0].height to font[17].height 
                // (in jHeretic/jHexen font[0] is 1 pixel high)
				WI_DrawParamText(x, y + currentMenu->itemHeight - 
								SHORT(currentMenu->font[17].height) - 1, 
								currentMenu->items[i].text, currentMenu->font, 
								r, g, b, menu_alpha, 
                                currentMenu->font == hu_font_b, true, 
                                ALIGN_LEFT);

			}
			y += currentMenu->itemHeight;
		}

		// Draw an alpha'd rect over the whole screen (eg when in a widget)
		if(WidgetEdit && menu_calpha > 0)
        {
#if 0
// FIXME: This will not do; there is no need to change the projection.
			gl.PopMatrix();
			gl.PushMatrix();

			gl.MatrixMode(DGL_PROJECTION);
			gl.PushMatrix();
			gl.LoadIdentity();
			gl.Ortho(0, 0, 320, 200, -1, 1);

			GL_SetNoTexture();
			GL_DrawRect(0, 0, 320, 200, 0.1f, 0.1f, 0.1f, menu_calpha);

            gl.MatrixMode(DGL_PROJECTION);
			gl.PopMatrix();

			// Note: do not re-setup the matrix for menu rendering as widgets
			// are drawn with their own scale, independant of cfg.menuscale.
#endif            
		}

		// DRAW Colour Widget?
		if(WidgetEdit)
        {
            Draw_BeginZoom(0.5f, 160, 100);
			DrawColorWidget();
		}

		// DRAW SKULL
		if(allowScaling)
		{
			scale = currentMenu->itemHeight / (float) LINEHEIGHT;
			w = SHORT(cursorst[whichSkull].width) * scale; // skull size
			h = SHORT(cursorst[whichSkull].height) * scale;
			off_x = (WidgetEdit? ColorWidgetMnu.x : currentMenu->x) + 
                SKULLXOFF * scale + w / 2;

			off_y =
				(WidgetEdit? ColorWidgetMnu.y : currentMenu->y) + (itemOn - 
                    (WidgetEdit? ColorWidgetMnu.firstItem : 
                        currentMenu->firstItem)) *
				currentMenu->itemHeight + currentMenu->itemHeight / 2 - 1;

			//if(currentMenu->font == hu_font_b)
			//	off_y += SKULLYOFF;

			GL_SetPatch(cursorst[whichSkull].lump);
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PushMatrix();
			gl.Translatef(off_x, off_y, 0);
			gl.Scalef(1, 1.0f / 1.2f, 1);
			if(skull_angle)
                gl.Rotatef(skull_angle, 0, 0, 1);
            gl.Scalef(1, 1.2f, 1);
            GL_DrawRect(-(w/2) , -(h / 2), w, h, 1, 1, 1, menu_alpha);
            gl.MatrixMode(DGL_MODELVIEW);
            gl.PopMatrix();
        }

        if(WidgetEdit)
        {
            Draw_EndZoom();
        }
	}

  end_draw_menu:

    // Restore original matrices.
	gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

	gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}

/*
 *  Notify2:
 *
 *		Setting the current player message
 */
void Notify2(char *msg)
{
	if(msg)
		P_SetMessage(players + consoleplayer, msg);

	S_LocalSound(menusnds[0], NULL);
}

/*
 *  M_Responder:
 *
 *		Handles player input in the menu
 */
boolean M_Responder(event_t *ev)
{
	int     ch;
	int     i;
	static int joywait = 0;

	int     firstVI, lastVI;	// first and last visible item
	const MenuItem_t *item;

	if(ev->data1 == DDKEY_RSHIFT)
	{
		shiftdown = (ev->type == ev_keydown || ev->type == ev_keyrepeat);
	}

	// Edit field responder. In Mn_mplr.c.  DJS - not for much longer...
	if(Ed_Responder(ev))
		return true;   	// eaten. get out of here

	// does the colour edit widget responder eat the event?
	if(Cl_Responder(ev))
		return true;	// eaten. get out of here

	ch = -1;

	if(ev->type == ev_joystick && joywait < Sys_GetTime())
	{
		if(ev->data3 == -1)
		{
			ch = DDKEY_UPARROW;
			joywait = Sys_GetTime() + 5;
		}
		else if(ev->data3 == 1)
		{
			ch = DDKEY_DOWNARROW;
			joywait = Sys_GetTime() + 5;
		}

		if(ev->data2 == -1)
		{
			ch = DDKEY_LEFTARROW;
			joywait = Sys_GetTime() + 2;
		}
		else if(ev->data2 == 1)
		{
			ch = DDKEY_RIGHTARROW;
			joywait = Sys_GetTime() + 2;
		}

		if(ev->data1 & 1)
		{
			ch = DDKEY_ENTER;
			joywait = Sys_GetTime() + 5;
		}
		if(ev->data1 & 2)
		{
			ch = DDKEY_BACKSPACE;
			joywait = Sys_GetTime() + 5;
		}
	}
	else
	{
		if(ev->type == ev_keydown || ev->type == ev_keyrepeat)
		{
			ch = ev->data1;
		}
	}

	if(ch == -1)
		return false;

	// Save Game string input
	if(saveStringEnter)
	{
		switch (ch)
		{
		case DDKEY_BACKSPACE:
			if(saveCharIndex > 0)
			{
				saveCharIndex--;
				savegamestrings[saveSlot][saveCharIndex] = 0;
			}
			break;

		case DDKEY_ESCAPE:
			saveStringEnter = 0;
			strcpy(&savegamestrings[saveSlot][0], saveOldString);
			break;

		case DDKEY_ENTER:
			saveStringEnter = 0;
			if(savegamestrings[saveSlot][0])
				M_DoSave(saveSlot);
			break;

		default:
			ch = toupper(ch);
			if(ch != 32)
				if(ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE)
					break;
			if(ch >= 32 && ch <= 127 && saveCharIndex < SAVESTRINGSIZE - 1 &&
			   M_StringWidth(savegamestrings[saveSlot], hu_font_a) < (SAVESTRINGSIZE - 2) * 8)
			{
				savegamestrings[saveSlot][saveCharIndex++] = ch;
				savegamestrings[saveSlot][saveCharIndex] = 0;
			}
			break;
		}
		return true;
	}

	// Take care of any messages that need input
	if(messageToPrint)
	{
		if(messageNeedsInput == true &&
		   !(ch == ' ' || ch == 'n' || ch == 'y' || ch == DDKEY_ESCAPE))
			return false;

		menuactive = messageLastMenuActive;
		messageToPrint = 0;
		if(messageRoutine)
			messageRoutine(ch, NULL);

		// Quit messages are 'final': no apparent effect.
		if(messageFinal)
		{
			menuactive = true;
			messageToPrint = 1;
			return false;
		}

		menuactive = false;
		menu_alpha = 0;
		S_LocalSound(menusnds[1], NULL);
		return true;
	}

	// Take a screenshot in dev mode
	if(devparm && ch == DDKEY_F1)
	{
		G_ScreenShot();
		return true;
	}

	// Pop-up menu?
	if(!menuactive)
	{
		if(ch == DDKEY_ESCAPE && !chat_on)
		{
			M_StartControlPanel();
			S_LocalSound(menusnds[3], NULL);
			return true;
		}
		return false;
	}

	firstVI = currentMenu->firstItem;
	lastVI = firstVI + currentMenu->numVisItems - 1;
	if(lastVI > currentMenu->itemCount - 1)
		lastVI = currentMenu->itemCount - 1;
	item = &currentMenu->items[itemOn];
	currentMenu->lastOn = itemOn;

	// Keys usable within menu
	switch (ch)
	{
	case DDKEY_DOWNARROW:
		i = 0;
		do
		{
			if(itemOn + 1 > lastVI)
			{
				itemOn = firstVI;
			}
			else
			{
				itemOn++;
			}
		} while(currentMenu->items[itemOn].type == ITT_EMPTY &&
				i++ < currentMenu->itemCount);
		menu_color = 0;
		S_LocalSound(menusnds[4], NULL);
		return (true);

	case DDKEY_UPARROW:
		i = 0;
		do
		{
			if(itemOn <= firstVI)
			{
				itemOn = lastVI;
			}
			else
			{
				itemOn--;
			}
		} while(currentMenu->items[itemOn].type == ITT_EMPTY &&
				i++ < currentMenu->itemCount);
		menu_color = 0;
		S_LocalSound(menusnds[4], NULL);
		return (true);

	case DDKEY_LEFTARROW:
		if(item->type == ITT_LRFUNC && item->func != NULL)
		{
			item->func(LEFT_DIR | item->option, item->data);
			S_LocalSound(menusnds[5], NULL);
		}
		else
		{
			// Let's try to change to the previous page.
			if(currentMenu->firstItem - currentMenu->numVisItems >= 0)
			{
				currentMenu->firstItem -= currentMenu->numVisItems;
				itemOn -= currentMenu->numVisItems;

				//Ensure cursor points to editable item     Anon $09 add next 3 lines
				firstVI = currentMenu->firstItem;
				while(currentMenu->items[itemOn].type == ITT_EMPTY &&
					  itemOn > firstVI)
					itemOn--;

				// Make a sound, too.
				S_LocalSound(menusnds[5], NULL);
			}
		}
		return (true);

	case DDKEY_RIGHTARROW:
		if(item->type == ITT_LRFUNC && item->func != NULL)
		{
			item->func(RIGHT_DIR | item->option, item->data);
			S_LocalSound(menusnds[5], NULL);
		}
		else
		{
			// Move on to the next page, if possible.
			if(currentMenu->firstItem + currentMenu->numVisItems <
			   currentMenu->itemCount)
			{
				currentMenu->firstItem += currentMenu->numVisItems;
				itemOn += currentMenu->numVisItems;
				if(itemOn > currentMenu->itemCount - 1)
					itemOn = currentMenu->itemCount - 1;
				S_LocalSound(menusnds[5], NULL);
			}
		}
		return (true);

	case DDKEY_ENTER:
		if(item->type == ITT_SETMENU)
		{
			M_SetupNextMenu(menulist[item->option]);
			S_LocalSound(menusnds[6], NULL);
		}
		else if(item->func != NULL)
		{
			currentMenu->lastOn = itemOn;
			if(item->type == ITT_LRFUNC)
			{
				item->func(RIGHT_DIR | item->option, item->data);
				S_LocalSound(menusnds[5], NULL);
			}
			else if(item->type == ITT_EFUNC)
			{
				item->func(item->option, item->data);
				S_LocalSound(menusnds[6], NULL);
			}
		}
		return (true);

	case DDKEY_ESCAPE:
		currentMenu->lastOn = itemOn;
		M_ClearMenus();
		S_LocalSound(menusnds[1], NULL);
		return (true);

	case DDKEY_BACKSPACE:
		currentMenu->lastOn = itemOn;
		if(currentMenu->prevMenu == MENU_NONE)
		{
			currentMenu->lastOn = itemOn;
			M_ClearMenus();
		}
		else
		{
			currentMenu = menulist[currentMenu->prevMenu];
			itemOn = currentMenu->lastOn;
			S_LocalSound(menusnds[3], NULL);
			typein_time = 0;
		}
		return (true);

	default:
		for(i = firstVI; i <= lastVI; i++)
		{
			if(currentMenu->items[i].text &&
			   currentMenu->items[i].type != ITT_EMPTY)
			{
				if(toupper(ch) == toupper(currentMenu->items[i].text[0]))
				{
					itemOn = i;
					return (true);
				}
			}
		}
		break;
	}

	return false;
}

/*
 *  Cl_Responder:
 *
 *		Used for managing input in a menu widget (overlay menu)
 */
boolean Cl_Responder(event_t *event)
{
	int     i;
	int     firstWVI, lastWVI;	// first and last visible item
	int     withalpha;          // if a 4 color widget is needed
	const MenuItem_t *item;

	// Is there an active edit field?
	if(!WidgetEdit)
		return false;

	// Check the type of the event.
	if(event->type != ev_keydown && event->type != ev_keyrepeat)
		return false;			// Not interested in anything else.

	if(rgba){
		withalpha = 0;
	} else {
		withalpha = 1;
	}

	firstWVI = ColorWidgetMnu.firstItem;
	lastWVI = firstWVI + ColorWidgetMnu.numVisItems - 1- withalpha;
	if(lastWVI > (ColorWidgetMnu.itemCount - 1 -withalpha))
		lastWVI = ColorWidgetMnu.itemCount - 1 - withalpha;
	item = &ColorWidgetMnu.items[itemOn];
	ColorWidgetMnu.lastOn = itemOn;

	switch (event->data1)
	{
	case DDKEY_DOWNARROW:
		i = 0;
		do
		{
			if(itemOn + 1 > lastWVI)
			{
				itemOn = firstWVI;
			}
			else
			{
				itemOn++;
			}
		} while(ColorWidgetMnu.items[itemOn].type == ITT_EMPTY &&
				i++ < ColorWidgetMnu.itemCount);
		menu_color = 0;
		S_LocalSound(menusnds[4], NULL);
		break;

	case DDKEY_UPARROW:
		i = 0;
		do
		{
			if(itemOn <= firstWVI)
			{
				itemOn = lastWVI;
			}
			else
			{
				itemOn--;
			}
		} while(ColorWidgetMnu.items[itemOn].type == ITT_EMPTY &&
				i++ < ColorWidgetMnu.itemCount);
		menu_color = 0;
		S_LocalSound(menusnds[4], NULL);
		break;

	case DDKEY_LEFTARROW:
		if(item->type == ITT_LRFUNC && item->func != NULL)
		{
			item->func(LEFT_DIR | item->option, item->data);
			S_LocalSound(menusnds[5], NULL);
		}
		break;

	case DDKEY_RIGHTARROW:
		if(item->type == ITT_LRFUNC && item->func != NULL)
		{
			item->func(RIGHT_DIR | item->option, item->data);
			S_LocalSound(menusnds[5], NULL);
		}
		break;

	case DDKEY_ENTER:
		// Set the new color
		*widgetcolors[editcolorindex].r = currentcolor[0];
		*widgetcolors[editcolorindex].g = currentcolor[1];
		*widgetcolors[editcolorindex].b = currentcolor[2];

		if (rgba)
			*widgetcolors[editcolorindex].a = currentcolor[3];

		// Restore the position of the skull
		itemOn = previtemOn;

		WidgetEdit = false;
		Notify2(NULL);
		break;

	case DDKEY_BACKSPACE:
	case DDKEY_ESCAPE:
		// Restore the position of the skull
		itemOn = previtemOn;
		WidgetEdit = false;
		break;
	default:
		break;
	}
	// All keydown events eaten while a color widget is active.
	return true;
}

/*
 *  DrawColorWidget:
 *
 *		The colour widget edits the "hot" currentcolour[]
 *		The widget responder handles setting the specified vars to that
 *		of the currentcolour.
 *
 *		boolean rgba is used to control if rgb or rgba input is needed,
 *		as defined in the widgetcolors array.
 */
void DrawColorWidget()
{
	int	w = 0;
	Menu_t *menu = &ColorWidgetMnu;

	if(WidgetEdit){

#ifdef __JDOOM__
	w = 38;
#else
	w = 46;
#endif

		M_DrawBackgroundBox(menu->x -30, menu->y -40,
#ifndef __JDOOM__
						180, (rgba? 170 : 140),
#else
						160, (rgba? 85 : 75),
#endif
							 1, 1, 1, menu_alpha, true, BORDERUP);

		GL_SetNoTexture();
		GL_DrawRect(menu->x+w, menu->y-30, 24, 22, currentcolor[0], currentcolor[1], currentcolor[2], currentcolor[3]);
		M_DrawBackgroundBox(menu->x+w, menu->y-30, 24, 22, 1, 1, 1, menu_alpha, false, BORDERDOWN);
#ifdef __JDOOM__
		M_DrawSlider(menu, 0, 11, currentcolor[0] * 10 + .25f);
		M_WriteText2(menu->x, menu->y, ColorWidgetItems[0].text, hu_font_a, 1, 1, 1, menu_alpha);
		M_DrawSlider(menu, 1, 11, currentcolor[1] * 10 + .25f);
		M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A), ColorWidgetItems[1].text, hu_font_a, 1, 1, 1, menu_alpha);
		M_DrawSlider(menu, 2, 11, currentcolor[2] * 10 + .25f);
		M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 2), ColorWidgetItems[2].text, hu_font_a, 1, 1, 1, menu_alpha);
#else
		M_DrawSlider(menu, 1, 11, currentcolor[0] * 10 + .25f);
		M_WriteText2(menu->x, menu->y, ColorWidgetItems[0].text, hu_font_a, 1, 1, 1, menu_alpha);
		M_DrawSlider(menu, 4, 11, currentcolor[1] * 10 + .25f);
		M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 3), ColorWidgetItems[3].text, hu_font_a, 1, 1, 1, menu_alpha);
		M_DrawSlider(menu, 7, 11, currentcolor[2] * 10 + .25f);
		M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 6), ColorWidgetItems[6].text, hu_font_a, 1, 1, 1, menu_alpha);
#endif
		if(rgba){
#ifdef __JDOOM__
			M_DrawSlider(menu, 3, 11, currentcolor[3] * 10 + .25f);
			M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 3), ColorWidgetItems[3].text, hu_font_a, 1, 1, 1, menu_alpha);
#else
			M_DrawSlider(menu, 10, 11, currentcolor[3] * 10 + .25f);
			M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 9), ColorWidgetItems[9].text, hu_font_a, 1, 1, 1, menu_alpha);
#endif
		}

	}
}

/*
 *  SCColorWidget:
 *
 *		Inform the menu to activate the color widget
 *		An intermediate step. Used to copy the existing rgba values pointed
 *		to by the index (these match an index in the widgetcolors array) into
 *		the "hot" currentcolor[] slots. Also switches between rgb/rgba input.
 */
void SCColorWidget(int index, void *data)
{
	currentcolor[0] = *widgetcolors[index].r;
	currentcolor[1] = *widgetcolors[index].g;
	currentcolor[2] = *widgetcolors[index].b;

	// Set the index of the colour being edited
	editcolorindex = index;

	// Remember the position of the Skull on the main menu
	previtemOn = itemOn;

	// Set the start position to 0;
	itemOn = 0;

	// Do we want rgb or rgba sliders?
	if (widgetcolors[index].a){
		rgba = true;
		currentcolor[3] = *widgetcolors[index].a;
	} else {
		rgba = false;
		currentcolor[3] = 1.0f;
	}

	// Activate the widget
	WidgetEdit = true;
}

void M_ToggleVar(int index, void *data)
{
	*(boolean *)data = !*(boolean *)data;
}


//---------------------------------------------------------------------------
//
// PROC M_DrawTitle
//
//---------------------------------------------------------------------------

void M_DrawTitle(char *text, int y)
{
	WI_DrawParamText(160 - M_StringWidth(text, hu_font_b) / 2, y, text,
					 hu_font_b, cfg.menuColor[0], cfg.menuColor[1],
					 cfg.menuColor[2], menu_alpha, true, true, ALIGN_LEFT);
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawMenuItem
//
//---------------------------------------------------------------------------

void M_WriteMenuText(const Menu_t * menu, int index, char *text)
{
	int     off = 0;

	if(menu->items[index].text)
		off = M_StringWidth(menu->items[index].text, menu->font) + 4;

	M_WriteText2(menu->x + off, menu->y + menu->itemHeight * (index  - menu->firstItem), text,
				 menu->font, 1, 1, 1, menu_alpha);
}

//
// User wants to load this game
//
void M_LoadSelect(int option, void *data)
{
#if __JDOOM__ || __JHERETIC__
	char    name[256];

	SV_SaveGameFile(option, name);
	G_LoadGame(name);
#else
	G_LoadGame(option);
#endif

	mfAlpha = 0;
	menu_alpha = 0;
	menuactive = false;
	fadingOut = false;
	M_ClearMenus();
}


//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int option, void *data)
{
	// we are going to be intercepting all chars
	saveStringEnter = 1;

	saveSlot = option;
	strcpy(saveOldString, savegamestrings[option]);
	if(!strcmp(savegamestrings[option], EMPTYSTRING))
		savegamestrings[option][0] = 0;
	saveCharIndex = strlen(savegamestrings[option]);
}

void M_StartMessage(char *string, void *routine, boolean input)
{

	messageLastMenuActive = menuactive;
	messageToPrint = 1;
	messageString = string;
	messageRoutine = routine;
	messageNeedsInput = input;
	menuactive = true;
	typein_time = 0;

}

void M_StopMessage(void)
{
	menuactive = messageLastMenuActive;
	messageToPrint = 0;
}

void DrawMessage(void)
{

}

const char* QuitEndMsg[] = 
{
	"ARE YOU SURE YOU WANT TO QUIT?",	
    "ARE YOU SURE YOU WANT TO END THE GAME?",
	"DO YOU WANT TO QUICKSAVE THE GAME NAMED",
	"DO YOU WANT TO QUICKLOAD THE GAME NAMED",
	"ARE YOU SURE YOU WANT TO SUICIDE?",
	NULL 
};

#define BETA_FLASH_TEXT "BETA"

void M_Options(int option, void *data)
{
	M_SetupNextMenu(&OptionsDef);
}

/*
 *  M_DrawBackground:
 *
 *		Draws a 'fancy' menu effect.
 *		looks a bit of a mess really (sorry)...
 */
void M_DrawBackground(void)
{
	int     i;
	const float xscale = 2.0f;
	const float yscale = 1.0f;

	if(cfg.menuEffects > 1)
		return;

	if(cfg.menuFog == 2)
	{
		gl.Disable(DGL_TEXTURING);
		gl.Color4f(mfAlpha, mfAlpha / 2, 0, mfAlpha / 3);
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
		GL_DrawRectTiled(0, 0, 320, 200, 1, 1);
		gl.Enable(DGL_TEXTURING);
	}

	if(cfg.menuFog == 4)
	{
		GL_SetNoTexture();
		GL_DrawRect(0, 0, 320, 200, 0.0f, 0.0f, 0.0f, mfAlpha/2.5f);
		return;
	}


	gl.Bind(menuFogTexture);
	gl.Color3f(mfAlpha, mfAlpha, mfAlpha);
	gl.MatrixMode(DGL_TEXTURE);
	for(i = 0; i < 3; i++)
	{
		if(i || cfg.menuFog == 1)
		{
			if(cfg.menuFog == 0)
				gl.Color3f(mfAlpha / 3, mfAlpha / 2, mfAlpha / 2);
			else
				gl.Color3f(mfAlpha, mfAlpha, mfAlpha);

			gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
		}
		else if(cfg.menuFog == 2)
		{
			gl.Color3f(mfAlpha / 5, mfAlpha / 3, mfAlpha / 2);
			gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_SRC_ALPHA);
		}
		else if(cfg.menuFog == 0)
		{
			gl.Color3f(mfAlpha * 0.15, mfAlpha * 0.2, mfAlpha * 0.3);
			gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_SRC_ALPHA);
		}

		if(cfg.menuFog == 3){
			// The fancy one

			gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_SRC_ALPHA);

			gl.LoadIdentity();

			gl.Translatef(mfPos[i][VX] / 320, mfPos[i][VY] / 200, 0);
			gl.Rotatef(mfAngle[i] * 1, 0, 0, 1);
			gl.Translatef(-mfPos[i][VX] / 320, -mfPos[i][VY] / 200, 0);

			gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
			gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);

			gl.Begin(DGL_QUADS);

			// Top Half
			gl.Color4f(mfAlpha * 0.25, mfAlpha * 0.3, mfAlpha * 0.4, 1 - (mfAlpha * 0.8) );
			gl.TexCoord2f( 0, 0);
			gl.Vertex2f(0, 0);

			gl.Color4f(mfAlpha * 0.25, mfAlpha * 0.3, mfAlpha * 0.4, 1 - (mfAlpha * 0.8) );
			gl.TexCoord2f( xscale, 0);
			gl.Vertex2f(320, 0);

			gl.Color4f(mfAlpha * 0.7, mfAlpha * 0.7, mfAlpha * 0.8, 1 - (0-(mfAlpha * 0.9)));
			gl.TexCoord2f( xscale, yscale * mfYjoin );
			gl.Vertex2f(320, 200 * mfYjoin);

			gl.Color4f(mfAlpha * 0.7, mfAlpha * 0.7, mfAlpha * 0.8, 1 - (0-(mfAlpha * 0.9)));
			gl.TexCoord2f( 0, yscale * mfYjoin );
			gl.Vertex2f(0, 200 * mfYjoin);

			// Bottom Half
			gl.Color4f(mfAlpha * 0.7, mfAlpha * 0.7, mfAlpha * 0.8, 1 - (0-(mfAlpha * 0.9)));
			gl.TexCoord2f( 0, yscale * mfYjoin );
			gl.Vertex2f(0, 200 * mfYjoin);

			gl.Color4f(mfAlpha * 0.7, mfAlpha * 0.7, mfAlpha * 0.8, 1 - (0-(mfAlpha * 0.9)));
			gl.TexCoord2f( xscale, yscale * mfYjoin );
			gl.Vertex2f(320, 200 * mfYjoin);

			gl.Color4f(mfAlpha * 0.25, mfAlpha * 0.3, mfAlpha * 0.4, 1 - (mfAlpha * 0.8) );
			gl.TexCoord2f( xscale, yscale);
			gl.Vertex2f(320, 200);

			gl.Color4f(mfAlpha * 0.25, mfAlpha * 0.3, mfAlpha * 0.4, 1 - (mfAlpha * 0.8) );
			gl.TexCoord2f( 0, yscale);
			gl.Vertex2f(0, 200);

			gl.End();

		} else {

			gl.LoadIdentity();

			gl.Translatef(mfPos[i][VX] / 320, mfPos[i][VY] / 200, 0);
			gl.Rotatef(mfAngle[i] * (cfg.menuFog == 0 ? 0.5 : 1), 0, 0, 1);
			gl.Translatef(-mfPos[i][VX] / 320, -mfPos[i][VY] / 200, 0);
			if(cfg.menuFog == 2)
				GL_DrawRectTiled(0, 0, 320, 200, 270 / 8, 4 * 225);
			else if(cfg.menuFog == 0)
				GL_DrawRectTiled(0, 0, 320, 200, 270 / 4, 8 * 225);
			else
				GL_DrawRectTiled(0, 0, 320, 200, 270, 225);
		}

   	}

    gl.MatrixMode(DGL_TEXTURE);
	gl.LoadIdentity();
    
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
}

//---------------------------------------------------------------------------
//
// PROC M_DrawMainMenu
//
//---------------------------------------------------------------------------

void M_DrawMainMenu(void)
{
#if __JHEXEN__
	int     frame;

	frame = (MenuTime / 5) % 7;

	gl.Color4f( 1, 1, 1, menu_alpha);
	GL_DrawPatch_CS(88, 0, W_GetNumForName("M_HTIC"));
	GL_DrawPatch_CS(37, 80, SkullBaseLump + (frame + 2) % 7);
	GL_DrawPatch_CS(278, 80, SkullBaseLump + frame);

#elif __JHERETIC__
	WI_DrawPatch(88, 0, 1, 1, 1, menu_alpha, W_GetNumForName("M_HTIC"));

	gl.Color4f( 1, 1, 1, menu_alpha);
	GL_DrawPatch_CS(40, 10, SkullBaseLump + (17 - frame));
	GL_DrawPatch_CS(232, 10, SkullBaseLump + frame);
#elif __JDOOM__
	WI_DrawPatch(94, 2, 1, 1, 1, menu_alpha, W_GetNumForName("M_DOOM"));
#elif __JSTRIFE__
	Menu_t *menu = &MainDef;
	int	yoffset = 0;

	WI_DrawPatch(86, 2, 1, 1, 1, menu_alpha, W_GetNumForName("M_STRIFE"));
	
	WI_DrawPatch(menu->x, menu->y + yoffset, 1, 1, 1, menu_alpha, W_GetNumForName("M_NGAME"));
	WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1, menu_alpha, W_GetNumForName("M_NGAME"));
	WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1, menu_alpha, W_GetNumForName("M_OPTION"));
	WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1, menu_alpha, W_GetNumForName("M_LOADG"));
	WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1, menu_alpha, W_GetNumForName("M_SAVEG"));
	WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1, menu_alpha, W_GetNumForName("M_RDTHIS"));
	WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1, menu_alpha, W_GetNumForName("M_QUITG"));
#endif
}

#if __JHEXEN__
//==========================================================================
//
// M_DrawClassMenu
//
//==========================================================================

void M_DrawClassMenu(void)
{
	Menu_t *menu = &ClassDef;

	pclass_t class;
	static char *boxLumpName[3] = {
		"m_fbox",
		"m_cbox",
		"m_mbox"
	};
	static char *walkLumpName[3] = {
		"m_fwalk1",
		"m_cwalk1",
		"m_mwalk1"
	};

	M_WriteText2(34, 24, "CHOOSE CLASS:", hu_font_b, menu->color[0], menu->color[1], menu->color[2], menu_alpha);

	class = (pclass_t) currentMenu->items[itemOn].option;

	gl.Color4f( 1, 1, 1, menu_alpha);
	GL_DrawPatch_CS(174, 8, W_GetNumForName(boxLumpName[class]));
	GL_DrawPatch_CS(174 + 24, 8 + 12,
					W_GetNumForName(walkLumpName[class]) +
					((MenuTime >> 3) & 3));
}
#endif


//---------------------------------------------------------------------------
//
// PROC M_DrawEpisode
//
//---------------------------------------------------------------------------
#if __JDOOM__ || __JHERETIC__
int     epi;

void M_DrawEpisode(void)
{
#ifdef __JHERETIC__
	M_DrawTitle("WHICH EPISODE?", 4);
#elif __JDOOM__
	WI_DrawPatch(96, 14, 1, 1, 1, menu_alpha, W_GetNumForName("M_NEWG"));
	M_DrawTitle(episodemsg, 40);
#endif
}
#endif

//---------------------------------------------------------------------------
//
// PROC M_DrawSkillMenu
//
//---------------------------------------------------------------------------

void M_DrawSkillMenu(void)
{
#if __JHEXEN__ || __JSTRIFE__
	M_DrawTitle("CHOOSE SKILL LEVEL:", 16);
#elif __JHERETIC__
	M_DrawTitle("SKILL LEVEL?", 4);
#elif __JDOOM__
	WI_DrawPatch(96, 14, 1, 1, 1, menu_alpha, W_GetNumForName("M_NEWG"));
	WI_DrawPatch(54, 38, 1, 1, 1, menu_alpha, W_GetNumForName("M_SKILL"));
#endif
}

//---------------------------------------------------------------------------
//
// PROC DrawFilesMenu
//
//---------------------------------------------------------------------------

void M_DrawFilesMenu(void)
{
	// clear out the quicksave/quickload stuff
	quicksave = 0;
	quickload = 0;
}

//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
#if __JDOOM__ || __JHERETIC__
	int     i;
	char    name[256];

	for(i = 0; i < load_end; i++)
	{
		SV_SaveGameFile(i, name);
		if(!SV_GetSaveDescription(name, savegamestrings[i]))
		{
			strcpy(savegamestrings[i], EMPTYSTRING);
			LoadItems[i].type = ITT_EMPTY;
		}
		else
			LoadItems[i].type = ITT_EFUNC;
	}
#else
	int     i;
	LZFILE *fp;
	char    name[100];
	char    versionText[HXS_VERSION_TEXT_LENGTH];
	char    description[HXS_DESCRIPTION_LENGTH];
	boolean found;

	for(i = 0; i < load_end; i++)
	{
		found = false;
		sprintf(name, "%shex%d.hxs", SavePath, i);
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
		if(!found)
		{
			strcpy(savegamestrings[i], EMPTYSTRING);
			LoadItems[i].type = ITT_EMPTY;
		}
		else
		{
			strcpy(savegamestrings[i], description);
			LoadItems[i].type = ITT_EFUNC;
		}
	}
#endif

}

//---------------------------------------------------------------------------
//
// PROC DrawLoadMenu
//
//---------------------------------------------------------------------------

void M_DrawLoad(void)
{
	int     i;

	Menu_t *menu = &LoadDef;

#ifndef __JDOOM__
	M_DrawTitle("LOAD GAME", 4);
#else
	WI_DrawPatch(72, 28, 1, 1, 1, menu_alpha, W_GetNumForName("M_LOADG"));
#endif
	for(i = 0; i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x, LoadDef.y + (menu->itemHeight * i) + SKULLYOFF);
		M_WriteText2(LoadDef.x, LoadDef.y + (menu->itemHeight * i) + SKULLYOFF, savegamestrings[i],
					 menu->font, menu->color[0], menu->color[1], menu->color[2], menu_alpha);
	}

}

//---------------------------------------------------------------------------
//
// PROC DrawSaveMenu
//
//---------------------------------------------------------------------------

void M_DrawSave(void)
{
	int     i;

	Menu_t *menu = &SaveDef;

#ifndef __JDOOM__
	M_DrawTitle("SAVE GAME", 4);
#else
	WI_DrawPatch(72, 28, 1, 1, 1, menu_alpha, W_GetNumForName("M_SAVEG"));
#endif
	for(i = 0; i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x, LoadDef.y + (menu->itemHeight * i) + SKULLYOFF);
		M_WriteText2(LoadDef.x, LoadDef.y + (menu->itemHeight * i) + SKULLYOFF, savegamestrings[i],
					 menu->font, menu->color[0], menu->color[1], menu->color[2], menu_alpha);
	}

	if(saveStringEnter)
	{
		i = M_StringWidth(savegamestrings[saveSlot], hu_font_a);
		M_WriteText2(LoadDef.x + i, LoadDef.y + (menu->itemHeight * saveSlot) + SKULLYOFF, "_", hu_font_a, menu->color[0], menu->color[1], menu->color[2], menu_alpha);
	}

}

//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x, int y)
{
#ifndef __JDOOM__
	gl.Color4f( 1, 1, 1, menu_alpha);
	GL_DrawPatch_CS(x - 8, y - 4, W_GetNumForName("M_FSLOT"));
#else
	gl.Color4f( 1, 1, 1, menu_alpha);
	GL_DrawPatch_CS(x - 8, y + 8, W_GetNumForName("M_LSLEFT"));
	GL_DrawPatch_CS(x + 8 * 24, y + 8, W_GetNumForName("M_LSRGHT"));

	GL_SetPatch(W_GetNumForName("M_LSCNTR"));
	gl.Color4f( 1, 1, 1, menu_alpha);
	GL_DrawRectTiled(x - 3, y - 3, 24 * 8, 14, 8, 14);
#endif
}

//
// M_Responder calls this when user is finished /not in jHexen it doesn't... yet
//
void M_DoSave(int slot)
{
	G_SaveGame(slot, savegamestrings[slot]);
	M_ClearMenus();

	// PICK QUICKSAVE SLOT YET?
	if(quickSaveSlot == -2)
		quickSaveSlot = slot;
}

//
//      M_QuickSave
//
char    tempstring[80];

void M_QuickSaveResponse(int ch, void *data)
{
	if(ch == 'y')
	{
		M_DoSave(quickSaveSlot);
		S_LocalSound(menusnds[1], NULL);
	}
}

void M_QuickSave(void)
{
	if(!usergame)
	{
		S_LocalSound(menusnds[7], NULL);
		return;
	}

	if(gamestate != GS_LEVEL)
		return;

	if(quickSaveSlot < 0)
	{
		M_StartControlPanel();
		M_ReadSaveStrings();
		M_SetupNextMenu(&SaveDef);
		quickSaveSlot = -2;		// means to pick a slot now
		return;
	}
	sprintf(tempstring, QSPROMPT, savegamestrings[quickSaveSlot]);
	M_StartMessage(tempstring, M_QuickSaveResponse, true);
}

//
// M_QuickLoad
//
void M_QuickLoadResponse(int ch, void *data)
{
	if(ch == 'y')
	{
		M_LoadSelect(quickSaveSlot, NULL);
		S_LocalSound(menusnds[1], NULL);
	}
}

void M_QuickLoad(void)
{
	if(IS_NETGAME)
	{
		M_StartMessage(QLOADNET, NULL, false);
		return;
	}

	if(quickSaveSlot < 0)
	{
		M_StartMessage(QSAVESPOT, NULL, false);
		return;
	}
	sprintf(tempstring, QLPROMPT, savegamestrings[quickSaveSlot]);
	M_StartMessage(tempstring, M_QuickLoadResponse, true);
}

//
// M_ReadThis
//
void M_ReadThis(int option, void *data)
{
	option = 0;
	M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int option, void *data)
{
	option = 0;
	M_SetupNextMenu(&ReadDef2);
}

#ifndef __JDOOM__
void M_ReadThis3(int option, void *data)
{
	option = 0;
	M_SetupNextMenu(&ReadDef3);
}
#endif

void M_FinishReadThis(int option, void *data)
{
	option = 0;
	M_SetupNextMenu(&MainDef);
}

//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
	inhelpscreens = true;

#ifdef __JDOOM__
	switch (gamemode)
	{
	case commercial:
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP",PU_CACHE));
		WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("HELP"));
		break;
	case shareware:
	case registered:
	case retail:
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP1",PU_CACHE));
		WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("HELP1"));
		break;
	default:
		break;
	}
#else
	GL_DrawRawScreen(W_GetNumForName("HELP1"), 0, 0);
#endif
	return;
}

//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
	inhelpscreens = true;
#ifdef __JDOOM__
	switch (gamemode)
	{
	case retail:
	case commercial:
		// This hack keeps us from having to change menus.
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("CREDIT",PU_CACHE));
		WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("CREDIT"));
		break;
	case shareware:
	case registered:
		//V_DrawPatchDirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
		WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("HELP2"));
		break;
	default:
		break;
	}
#else
	GL_DrawRawScreen(W_GetNumForName("HELP2"), 0, 0);
#endif
	return;
}

#ifndef __JDOOM__
//
// Read This Menus - third page used in hexen & heretic
//
void M_DrawReadThis3(void)
{
	inhelpscreens = true;
	GL_DrawRawScreen(W_GetNumForName("CREDIT"), 0, 0);
	return;
}

#endif

//---------------------------------------------------------------------------
//
// PROC M_DrawOptions
//
//---------------------------------------------------------------------------

void M_DrawOptions(void)
{
#ifndef __JDOOM__
	WI_DrawPatch(88, 0, 1, 1, 1, menu_alpha, W_GetNumForName("M_HTIC"));

	M_DrawTitle("OPTIONS", 56);
#else
	WI_DrawPatch(94, 2, 1, 1, 1, menu_alpha, W_GetNumForName("M_DOOM"));
	M_DrawTitle("OPTIONS", 64);
#endif
}

//---------------------------------------------------------------------------
//
// PROC M_DrawOptions2
//
//---------------------------------------------------------------------------

void M_DrawOptions2(void)
{
	Menu_t *menu = &Options2Def;

#if __JHEXEN__ || __JSTRIFE__
	M_DrawTitle("SOUND OPTIONS", 0);

	M_DrawSlider(menu, 1, 18, Get(DD_SFX_VOLUME) / 15);
	M_DrawSlider(menu, 4, 18, Get(DD_MUSIC_VOLUME) / 15);
#elif __JHERETIC__

	M_DrawTitle("SOUND", 4);

	M_DrawSlider(menu, 1, 16, snd_SfxVolume );
	M_DrawSlider(menu, 4, 16, snd_MusicVolume );
#elif __JDOOM__

	M_DrawTitle("SOUND OPTIONS", menu->y - 20);

	M_DrawSlider(menu, 0, 16, snd_SfxVolume);
	M_DrawSlider(menu, 1, 16, snd_MusicVolume);
#endif
}

void M_DrawGameplay(void)
{
	Menu_t *menu = &GameplayDef;

#if __JHEXEN__
	M_DrawTitle("GAMEPLAY", 0);
	M_WriteMenuText(menu, 0, yesno[cfg.alwaysRun != 0]);
	M_WriteMenuText(menu, 1, yesno[cfg.lookSpring != 0]);
	M_WriteMenuText(menu, 2, yesno[cfg.noAutoAim != 0]);
#else

#ifdef __JHERETIC__
	M_DrawTitle("GAMEPLAY", 4);
#else
	M_DrawTitle("GAMEPLAY", menu->y - 20);
#endif

	M_WriteMenuText(menu, 0, yesno[cfg.alwaysRun != 0]);
	M_WriteMenuText(menu, 1, yesno[cfg.lookSpring != 0]);
	M_WriteMenuText(menu, 2, yesno[!cfg.noAutoAim]);
	M_WriteMenuText(menu, 3, yesno[cfg.jumpEnabled != 0]);
#ifdef __JDOOM__
	M_WriteMenuText(menu, 6, yesno[cfg.raiseghosts != 0]);
	M_WriteMenuText(menu, 7, yesno[cfg.maxskulls != 0]);
	M_WriteMenuText(menu, 8, yesno[cfg.allowskullsinwalls != 0]);
	M_WriteMenuText(menu, 9, yesno[cfg.slidingCorpses != 0]);
#endif
#endif
}

void M_DrawHUDMenu(void)
{
	Menu_t *menu = &HUDDef;
	char   *xhairnames[7] = { "NONE", "CROSS", "ANGLES", "SQUARE",
		"OPEN SQUARE", "DIAMOND", "V"
	};

#ifndef __JDOOM__
	//const MenuItem_t *item = menu->items + menu->firstItem;
	char  *token;

	M_DrawTitle("hud options", 4);

	// Draw the page arrows.
	gl.Color4f( 1, 1, 1, menu_alpha);
	token = (!menu->firstItem || MenuTime & 8) ? "invgeml2" : "invgeml1";
	GL_DrawPatch_CS(menu->x -20, menu->y - 16, W_GetNumForName(token));
	token = (menu->firstItem + menu->numVisItems >= menu->itemCount ||
			 MenuTime & 8) ? "invgemr2" : "invgemr1";
	GL_DrawPatch_CS(312 - (menu->x -20), menu->y - 16, W_GetNumForName(token));
#else
	M_DrawTitle("HUD OPTIONS", menu->y - 20);
#endif

#if __JHEXEN__ || __JSTRIFE__
	if(menu->firstItem < menu->numVisItems)
    {
		M_WriteMenuText(menu, 0, yesno[cfg.msgShow != 0]);
		M_WriteMenuText(menu, 1, xhairnames[cfg.xhair]);
		M_DrawSlider(menu, 3, 9, cfg.xhairSize);
		M_DrawSlider(menu, 6, 11, cfg.screenblocks - 3);
		M_DrawSlider(menu, 9, 20, cfg.sbarscale - 1);
		M_DrawSlider(menu, 12, 11, cfg.statusbarAlpha * 10 + .25f);
	} 
    else 
    {
		M_WriteMenuText(menu, 16, yesno[cfg.hudShown[HUD_MANA] != 0]);
		M_WriteMenuText(menu, 17, yesno[cfg.hudShown[HUD_HEALTH]]);
		M_WriteMenuText(menu, 18, yesno[cfg.hudShown[HUD_ARTI]]);
		M_DrawColorBox(menu,19, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], menu_alpha);
		M_DrawSlider(menu, 21, 10, cfg.hudScale * 10 - 3 + .5f);
	}
#elif __JHERETIC__
	if(menu->firstItem < menu->numVisItems)
    {
		M_WriteMenuText(menu, 0, yesno[cfg.msgShow != 0]);
		M_WriteMenuText(menu, 1, xhairnames[cfg.xhair]);
		M_DrawSlider(menu, 3, 9, cfg.xhairSize);
		M_DrawSlider(menu, 6, 11, cfg.screenblocks - 3);
		M_DrawSlider(menu, 9, 20, cfg.sbarscale - 1);
		M_DrawSlider(menu, 12, 11, cfg.statusbarAlpha * 10 + .25f);
	} 
    else 
    {
		M_WriteMenuText(menu, 16, yesno[cfg.hudShown[HUD_AMMO]]);
		M_WriteMenuText(menu, 17, yesno[cfg.hudShown[HUD_ARMOR]]);
		M_WriteMenuText(menu, 18, yesno[cfg.hudShown[HUD_ARTI]]);
		M_WriteMenuText(menu, 19, yesno[cfg.hudShown[HUD_HEALTH]]);
		M_WriteMenuText(menu, 20, yesno[cfg.hudShown[HUD_KEYS]]);
		M_DrawColorBox(menu, 21, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], menu_alpha);
		M_DrawSlider(menu, 23, 10, cfg.hudScale * 10 - 3 + .5f);
	}
#elif __JDOOM__
	M_WriteMenuText(menu, 0, yesno[cfg.hudShown[HUD_AMMO]]);
	M_WriteMenuText(menu, 1, yesno[cfg.hudShown[HUD_ARMOR]]);
	M_WriteMenuText(menu, 2, yesno[cfg.hudShown[HUD_FACE]]);
	M_WriteMenuText(menu, 3, yesno[cfg.hudShown[HUD_HEALTH]]);
	M_WriteMenuText(menu, 4, yesno[cfg.hudShown[HUD_KEYS]]);
	M_DrawSlider(menu, 5, 10, cfg.hudScale * 10 - 3 + .5f);
	M_DrawColorBox(menu,6, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], menu_alpha);
	M_WriteMenuText(menu, 7, yesno[cfg.msgShow != 0]);
	M_WriteMenuText(menu, 8, xhairnames[cfg.xhair]);
	M_DrawSlider(menu, 9, 9, cfg.xhairSize);
	M_DrawSlider(menu, 10, 11, cfg.screenblocks - 3 );
	M_DrawSlider(menu, 11, 20, cfg.sbarscale - 1);
	M_DrawSlider(menu, 12, 11, cfg.statusbarAlpha * 10 + .25f);
#endif
}

void M_FloatMod10(float *variable, int option)
{
	int     val = (*variable + .05f) * 10;

	if(option == RIGHT_DIR)
	{
		if(val < 10)
			val++;
	}
	else if(val > 0)
		val--;
	*variable = val / 10.0f;
}

void M_LookSpring(int option, void *data)
{
	cfg.lookSpring = !cfg.lookSpring;
}

void M_NoAutoAim(int option, void *data)
{
	cfg.noAutoAim = !cfg.noAutoAim;
}

void M_Xhair(int option, void *data)
{
#ifndef __JDOOM__
	cfg.xhair += option == RIGHT_DIR ? 1 : -1;
	if(cfg.xhair < 0)
		cfg.xhair = 0;
	if(cfg.xhair > NUM_XHAIRS)
		cfg.xhair = NUM_XHAIRS;
#else
	if(option == RIGHT_DIR)
	{
		if(cfg.xhair < NUM_XHAIRS)
			cfg.xhair++;
	}
	else if(cfg.xhair > 0)
		cfg.xhair--;
#endif
}

void M_XhairSize(int option, void *data)
{
#ifndef __JDOOM__
	cfg.xhairSize += option == RIGHT_DIR ? 1 : -1;
	if(cfg.xhairSize < 0)
		cfg.xhairSize = 0;
	if(cfg.xhairSize > 9)
		cfg.xhairSize = 9;
#else
	if(option == RIGHT_DIR)
	{
		if(cfg.xhairSize < 8)
			cfg.xhairSize++;
	}
	else if(cfg.xhairSize > 0)
		cfg.xhairSize--;
#endif
}


#ifdef __JDOOM__
void M_XhairR(int option, void *data)
{
	int     val = cfg.xhairColor[0];

	val += (option == RIGHT_DIR ? 17 : -17);
	if(val < 0)
		val = 0;
	if(val > 255)
		val = 255;
	cfg.xhairColor[0] = val;
}

void M_XhairG(int option, void *data)
{
	int     val = cfg.xhairColor[1];

	val += (option == RIGHT_DIR ? 17 : -17);
	if(val < 0)
		val = 0;
	if(val > 255)
		val = 255;
	cfg.xhairColor[1] = val;
}

void M_XhairB(int option, void *data)
{
	int     val = cfg.xhairColor[2];

	val += (option == RIGHT_DIR ? 17 : -17);
	if(val < 0)
		val = 0;
	if(val > 255)
		val = 255;
	cfg.xhairColor[2] = val;
}

void M_XhairAlpha(int option, void *data)
{
	int     val = cfg.xhairColor[3];

	val += (option == RIGHT_DIR ? 17 : -17);
	if(val < 0)
		val = 0;
	if(val > 255)
		val = 255;
	cfg.xhairColor[3] = val;
}
#endif

//---------------------------------------------------------------------------
//
// PROC M_SizeStatusBar
//
//---------------------------------------------------------------------------

void M_SizeStatusBar(int option, void *data)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.sbarscale < 20)
			cfg.sbarscale++;
	}
	else if(cfg.sbarscale > 1)
		cfg.sbarscale--;

	R_SetViewSize(cfg.screenblocks, 0);
}

void M_StatusBarAlpha(int option, void *data)
{
	M_FloatMod10(&cfg.statusbarAlpha, option);
}

//===========================================================================
// M_WGCurrentColor
//===========================================================================
void M_WGCurrentColor(int option, void *data)
{
	M_FloatMod10(data, option);
}

//---------------------------------------------------------------------------
//
// PROC M_DrawMouseMenu
//
//---------------------------------------------------------------------------

void M_DrawMouseMenu(void)
{
	Menu_t *menu = &MouseOptsMenu;

#ifndef __JDOOM__
	M_DrawTitle("MOUSE OPTIONS", 0);
	M_WriteMenuText(menu, 0, yesno[cfg.usemlook != 0]);
	M_WriteMenuText(menu, 1, yesno[cfg.mlookInverseY != 0]);
	M_DrawSlider(&MouseOptsMenu, 3, 18, cfg.mouseSensiX);
	M_DrawSlider(&MouseOptsMenu, 6, 18, cfg.mouseSensiY);
#else
	M_DrawTitle("MOUSE OPTIONS", menu->y - 20);

	M_WriteMenuText(menu, 0, yesno[cfg.usemlook]);
	M_WriteMenuText(menu, 1, yesno[cfg.mlookInverseY]);
	M_DrawSlider(menu, 2, 21, cfg.mouseSensiX / 2);
	M_DrawSlider(menu, 3, 21, cfg.mouseSensiY / 2);
#endif
}

void M_DrawJoyMenu()
{
	Menu_t *menu = &JoyConfigMenu;

	char   *axisname[] = { "-", "MOVE", "TURN", "STRAFE", "LOOK" };

#ifndef __JDOOM__
	M_DrawTitle("JOYSTICK OPTIONS", 0);
#else
	M_DrawTitle("JOYSTICK OPTIONS", menu->y - 20);
#endif
	M_WriteMenuText(menu, 0, axisname[cfg.joyaxis[0]]);
	M_WriteMenuText(menu, 1, axisname[cfg.joyaxis[1]]);
	M_WriteMenuText(menu, 2, axisname[cfg.joyaxis[2]]);
	M_WriteMenuText(menu, 3, axisname[cfg.joyaxis[3]]);
	M_WriteMenuText(menu, 4, axisname[cfg.joyaxis[4]]);
	M_WriteMenuText(menu, 5, axisname[cfg.joyaxis[5]]);
	M_WriteMenuText(menu, 6, axisname[cfg.joyaxis[6]]);
	M_WriteMenuText(menu, 7, axisname[cfg.joyaxis[7]]);
	M_WriteMenuText(menu, 8, yesno[cfg.usejlook]);
	M_WriteMenuText(menu, 9, yesno[cfg.jlookInverseY]);
	M_WriteMenuText(menu, 10, yesno[cfg.povLookAround]);
}

void M_GameFiles(int option, void *data)
{
#ifndef __JDOOM__
	M_SetupNextMenu(&FilesMenu);
#endif
}

void M_NewGame(int option, void *data)
{
	if(IS_NETGAME)				// && !Get(DD_PLAYBACK))
	{
		M_StartMessage(NEWGAME, NULL, false);
		return;
	}
#ifdef __JDOOM__
	if(gamemode == commercial)
		M_SetupNextMenu(&SkillDef);
	else
#endif
#if __JHEXEN__
		M_SetupNextMenu(&ClassDef);
#elif __JSTRIFE__
		M_SetupNextMenu(&SkillDef);
#else
		M_SetupNextMenu(&EpiDef);
#endif
}

//
// M_QuitResponse
//
void M_QuitResponse(int option, void *data)
{
#ifdef __JDOOM__
	int     quitsounds[8] = {
		sfx_pldeth,
		sfx_dmpain,
		sfx_popain,
		sfx_slop,
		sfx_telept,
		sfx_posit1,
		sfx_posit3,
		sfx_sgtatk
	};
	int     quitsounds2[8] = {
		sfx_vilact,
		sfx_getpow,
		sfx_boscub,
		sfx_slop,
		sfx_skeswg,
		sfx_kntdth,
		sfx_bspact,
		sfx_sgtatk
	};
#endif
	if(option != 'y')
		return;

	// No need to close down the menu question after this.
	messageFinal = true;

#ifdef __JDOOM__
	// Play an exit sound if it is enabled.
	if(cfg.menuQuitSound && !IS_NETGAME)
	{
		if(gamemode == commercial)
			S_LocalSound(quitsounds2[(gametic >> 2) & 7], NULL);
		else
			S_LocalSound(quitsounds[(gametic >> 2) & 7], NULL);

		// Wait for 1.5 seconds.
		Con_Executef(true, "after 53 quit!");
	}
	else
	{
		Sys_Quit();
	}
#else
		Sys_Quit();
#endif
}

//---------------------------------------------------------------------------
//
// PROC M_QuitDOOM
//
//---------------------------------------------------------------------------

void M_QuitDOOM(int option, void *data)
{

	Con_Open(false);

#ifdef __JDOOM__
	// We pick index 0 which is language sensitive,
	//  or one at random, between 1 and maximum number.
	if(language != english)
		sprintf(endstring, "%s\n\n%s", endmsg[0], DOSY);
	else

		sprintf(endstring, "%s\n\n%s",
				endmsg[(gametic % (NUM_QUITMESSAGES + 1))], DOSY);
#else
		sprintf(endstring, "%s\n\n%s", endmsg[0], DOSY);
#endif

	M_StartMessage(endstring, M_QuitResponse, true);
}

//
// M_EndGame
//
void M_EndGameResponse(int option, void *data)
{
	if(option != 'y')
		return;

	currentMenu->lastOn = itemOn;
	mfAlpha = 0;
	menu_alpha = 0;
	fadingOut = false;
	menuactive = false;

	M_ClearMenus();
	G_StartTitle();
}

//---------------------------------------------------------------------------
//
// PROC M_EndGame
//
//---------------------------------------------------------------------------

void M_EndGame(int option, void *data)
{
	option = 0;
	if(!usergame)
	{
		S_LocalSound(menusnds[7], NULL);
		return;
	}

	if(IS_NETGAME)
	{
		M_StartMessage(NETEND, NULL, false);
		return;
	}

	M_StartMessage(ENDGAME, M_EndGameResponse, true);
}

//---------------------------------------------------------------------------
//
// PROC M_ChangeMessages
//
//---------------------------------------------------------------------------

void M_ChangeMessages(int option, void *data)
{
	cfg.msgShow = !cfg.msgShow;
	P_SetMessage(players + consoleplayer, !cfg.msgShow ? MSGOFF : MSGON);
	message_dontfuckwithme = true;
}

void M_AlwaysRun(int option, void *data)
{
	cfg.alwaysRun = !cfg.alwaysRun;
}


void M_AllowJump(int option, void *data)
{
#if __JDOOM__ || __JHERETIC__
	cfg.jumpEnabled = !cfg.jumpEnabled;
#endif
}

void M_HUDInfo(int option, void *data)
{
	cfg.hudShown[option] = !cfg.hudShown[option];
}

void M_HUDScale(int option, void *data)
{
	int     val = (cfg.hudScale + .05f) * 10;

	if(option == RIGHT_DIR)
	{
		if(val < 12)
			val++;
	}
	else if(val > 3)
		val--;
	cfg.hudScale = val / 10.0f;
}

#ifdef __JDOOM__
void M_HUDRed(int option, void *data)
{
	M_FloatMod10(&cfg.hudColor[0], option);
}

void M_HUDGreen(int option, void *data)
{
	M_FloatMod10(&cfg.hudColor[1], option);
}

void M_HUDBlue(int option, void *data)
{
	M_FloatMod10(&cfg.hudColor[2], option);
}
#endif


void M_MouseLook(int option, void *data)
{
	cfg.usemlook = !cfg.usemlook;
	S_LocalSound(menusnds[0], NULL);
}

void M_JoyLook(int option, void *data)
{
	cfg.usejlook = !cfg.usejlook;
	S_LocalSound(menusnds[0], NULL);
}

void M_POVLook(int option, void *data)
{
	cfg.povLookAround = !cfg.povLookAround;
	S_LocalSound(menusnds[0], NULL);
}

void M_InverseJoyLook(int option, void *data)
{
	cfg.jlookInverseY = !cfg.jlookInverseY;
	S_LocalSound(menusnds[0], NULL);
}

void M_JoyAxis(int option, void *data)
{
	if(option & RIGHT_DIR)
	{
		if(cfg.joyaxis[option >> 8] < 4)
			cfg.joyaxis[option >> 8]++;
	}
	else
	{
		if(cfg.joyaxis[option >> 8] > 0)
			cfg.joyaxis[option >> 8]--;
	}
}

void M_MouseLookInverse(int option, void *data)
{
	cfg.mlookInverseY = !cfg.mlookInverseY;
	S_LocalSound(menusnds[0], NULL);
}



//---------------------------------------------------------------------------
//
// PROC M_LoadGame
//
//---------------------------------------------------------------------------

void M_LoadGame(int option, void *data)
{
	if(IS_CLIENT && !Get(DD_PLAYBACK))
	{
		M_StartMessage(LOADNET, NULL, false);
		return;
	}
	M_SetupNextMenu(&LoadDef);
	M_ReadSaveStrings();
}

//---------------------------------------------------------------------------
//
// PROC M_SaveGame
//
//---------------------------------------------------------------------------

void M_SaveGame(int option, void *data)
{
	if(!usergame || Get(DD_PLAYBACK))
	{
		M_StartMessage(SAVEDEAD, NULL, false);
		return;
	}
	if(IS_CLIENT)
	{
#ifdef __JDOOM__
		M_StartMessage(GET_TXT(TXT_SAVENET), NULL, false);
#endif
		return;
	}
	if(gamestate != GS_LEVEL)
		return;

	M_SetupNextMenu(&SaveDef);
	M_ReadSaveStrings();
}

//==========================================================================
//
// M_ChooseClass
//
//==========================================================================

void M_ChooseClass(int option, void *data)
{
#if __JHEXEN__
	if(IS_NETGAME)
	{
		P_SetMessage(&players[consoleplayer],
					 "YOU CAN'T START A NEW GAME FROM WITHIN A NETGAME!");
		return;
	}
	MenuPClass = option;
	switch (MenuPClass)
	{
	case PCLASS_FIGHTER:
		SkillDef.x = 120;
		SkillItems[0].text = "SQUIRE";
		SkillItems[1].text = "KNIGHT";
		SkillItems[2].text = "WARRIOR";
		SkillItems[3].text = "BERSERKER";
		SkillItems[4].text = "TITAN";
		break;
	case PCLASS_CLERIC:
		SkillDef.x = 116;
		SkillItems[0].text = "ALTAR BOY";
		SkillItems[1].text = "ACOLYTE";
		SkillItems[2].text = "PRIEST";
		SkillItems[3].text = "CARDINAL";
		SkillItems[4].text = "POPE";
		break;
	case PCLASS_MAGE:
		SkillDef.x = 112;
		SkillItems[0].text = "APPRENTICE";
		SkillItems[1].text = "ENCHANTER";
		SkillItems[2].text = "SORCERER";
		SkillItems[3].text = "WARLOCK";
		SkillItems[4].text = "ARCHIMAGE";
		break;
	}
	M_SetupNextMenu(&SkillDef);
#endif
}

//---------------------------------------------------------------------------
//
// PROC M_Episode
//
//---------------------------------------------------------------------------

void M_Episode(int option, void *data)
{
#ifdef __JHERETIC__
	if(shareware && option > 1)
	{
		Con_Message("ONLY AVAILABLE IN THE REGISTERED VERSION\n");
		option = 0;
	}
	else
	{
		MenuEpisode = option;
		M_SetupNextMenu(&SkillDef);
	}
#elif __JDOOM__
	if((gamemode == shareware) && option)
	{
		M_StartMessage(SWSTRING, NULL, false);
		M_SetupNextMenu(&ReadDef1);
		return;
	}

	// Yet another hack...
	if((gamemode == registered) && (option > 2))
	{
		Con_Message("M_Episode: 4th episode requires Ultimate DOOM\n");
		option = 0;
	}

	epi = option;
	M_SetupNextMenu(&SkillDef);
#endif
}



//---------------------------------------------------------------------------
//
// PROC M_ChooseSkill
//
//---------------------------------------------------------------------------

void M_VerifyNightmare(int option, void *data)
{
#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
	if(option != 'y')
		return;
#ifdef __JHERETIC__
	G_DeferedInitNew(sk_nightmare, MenuEpisode, 1);
#elif __JDOOM__
	G_DeferedInitNew(sk_nightmare, epi + 1, 1);
#elif __JSTRIFE__
	G_DeferredNewGame(sk_nightmare);
#endif
	M_ClearMenus();
#endif
}


void M_ChooseSkill(int option, void *data)
{
#if __JHEXEN__
	extern int SB_state;

	cfg.PlayerClass[consoleplayer] = MenuPClass;
	G_DeferredNewGame(option);
	SB_SetClassData();
	SB_state = -1;

#else
	if(option == sk_nightmare)
	{
#if __JSTRIFE__
		M_StartMessage("u nuts? FIXME!!!", M_VerifyNightmare, true);
#else
		M_StartMessage(NIGHTMARE, M_VerifyNightmare, true);
#endif
		return;
	}
#endif

#ifdef __JHERETIC__
	G_DeferedInitNew(option, MenuEpisode, 1);
#elif __JDOOM__
	G_DeferedInitNew(option, epi + 1, 1);
#elif __JSTRIFE__
	G_DeferredNewGame(option);
#endif

	mfAlpha = 0;
	menu_alpha = 0;
	menuactive = false;
	fadingOut = false;
	M_ClearMenus();
}

void M_OpenDCP(int option, void *data)
{
	M_ClearMenus();
	Con_Execute(option ? "panel audio" : "panel", true);
}

//---------------------------------------------------------------------------
//
// PROC M_MouseXSensi
//
//---------------------------------------------------------------------------

void M_MouseXSensi(int option, void *data)
{
#ifdef __JDOOM__
	if(option == RIGHT_DIR)
	{
		if(cfg.mouseSensiX < 39)
		{
			cfg.mouseSensiX += 2;
		}
	}
	else if(cfg.mouseSensiX > 1)
	{
		cfg.mouseSensiX -= 2;
	}
#else
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
#endif
}

//---------------------------------------------------------------------------
//
// PROC M_MouseYSensi
//
//---------------------------------------------------------------------------

void M_MouseYSensi(int option, void *data)
{
#ifdef __JDOOM__
	if(option == RIGHT_DIR)
	{
		if(cfg.mouseSensiY < 39)
		{
			cfg.mouseSensiY += 2;
		}
	}
	else if(cfg.mouseSensiY > 1)
	{
		cfg.mouseSensiY -= 2;
	}
#else
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
#endif
}

//---------------------------------------------------------------------------
//
// PROC M_SfxVol
//
//---------------------------------------------------------------------------

void M_SfxVol(int option, void *data)
{
#if __JHEXEN__ || __JSTRIFE__
	int     vol = Get(DD_SFX_VOLUME);

	vol += option == RIGHT_DIR ? 15 : -15;
	if(vol < 0)
		vol = 0;
	if(vol > 255)
		vol = 255;
	//soundchanged = true; // we'll set it when we leave the menu
	Set(DD_SFX_VOLUME, vol);
#else
	int     vol = snd_SfxVolume;

	switch (option)
	{
	case 0:
		if(vol)
			vol--;
		break;
	case 1:
		if(vol < 15)
			vol++;
		break;
	}
	//SetSfxVolume(vol*17); // 15*17 = 255
	Set(DD_SFX_VOLUME, vol * 17);
#endif
}

//---------------------------------------------------------------------------
//
// PROC M_MusicVol
//
//---------------------------------------------------------------------------

void M_MusicVol(int option, void *data)
{
#if __JHEXEN__ || __JSTRIFE__
	int     vol = Get(DD_MUSIC_VOLUME);

	vol += option == RIGHT_DIR ? 15 : -15;
	if(vol < 0)
		vol = 0;
	if(vol > 255)
		vol = 255;
	Set(DD_MUSIC_VOLUME, vol);
#else
	int     vol = snd_MusicVolume;

	switch (option)
	{
	case 0:
		if(vol)
			vol--;
		break;
	case 1:
		if(vol < 15)
			vol++;
		break;
	}
	//SetMIDIVolume(vol*17);
	Set(DD_MUSIC_VOLUME, vol * 17);
#endif
}

//---------------------------------------------------------------------------
//
// PROC M_SizeDisplay
//
//---------------------------------------------------------------------------

void M_SizeDisplay(int option, void *data)
{

	if(option == RIGHT_DIR)
	{
		if(cfg.screenblocks < 13)
		{
			cfg.screenblocks++;
		}
	}
	else if(cfg.screenblocks > 3)
	{
		cfg.screenblocks--;
	}
	R_SetViewSize(cfg.screenblocks, 0);	//detailLevel);
}

//---------------------------------------------------------------------------
//
// PROC MN_ActivateMenu
//
//---------------------------------------------------------------------------

void MN_ActivateMenu(void)
{
	if(menuactive)
	{
		return;
	}

	menuactive = true;
	FileMenuKeySteal = false;
	MenuTime = 0;
	fadingOut = false;
	currentMenu = &MainDef;
	itemOn = currentMenu->lastOn;

	if(!IS_NETGAME && !Get(DD_PLAYBACK))
	{
		paused = true;
	}
	S_LocalSound(menusnds[1], NULL);
	slottextloaded = false;		//reload the slot text, when needed
}

//---------------------------------------------------------------------------
//
// PROC MN_DeactivateMenu
//
//---------------------------------------------------------------------------

void MN_DeactivateMenu(void)
{
	if(!currentMenu)
		return;

	MenuTime = 0;
	currentMenu->lastOn = itemOn;
	menuactive = false;
	fadingOut = true;
	outFade = 0;

	if(!IS_NETGAME)
	{
		paused = false;
	}

	S_LocalSound(menusnds[0], NULL);
}

//
// M_StartControlPanel
//
void M_StartControlPanel(void)
{
	// intro might call this repeatedly
	if(menuactive)
		return;

	Con_Open(false);
	menuactive = true;
	menu_color = 0;
	MenuTime = 0;
	fadingOut = false;
	skull_angle = 0;
	currentMenu = &MainDef;
	itemOn = currentMenu->lastOn;
	typein_time = 0;
}

//---------------------------------------------------------------------------
//
// PROC SetMenu
//
//---------------------------------------------------------------------------

void SetMenu(MenuType_t menu)
{
	currentMenu->lastOn = itemOn;
	currentMenu = menulist[menu];
	itemOn = currentMenu->lastOn;
}

//
// M_ClearMenus
//
void M_ClearMenus(void)
{
	menuactive = false;
	fadingOut = true;
	outFade = 0;
}

//
// M_SetupNextMenu
//
void M_SetupNextMenu(Menu_t * menudef)
{
	if(!menudef)
		return;
	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
	menu_color = 0;
	skull_angle = 0;
	typein_time = 0;
}

#ifdef __JDOOM__
// Height is in pixels, and determines the scale of the whole thing.
// [left/right:6, middle:8, thumb:5]
void M_DrawThermo2(int x, int y, int thermWidth, int thermDot, int height)
{
	float   scale = height / 13.0f;	// 13 is the normal scale.
	int     xx;

	xx = x;
	GL_SetPatch(W_GetNumForName("M_THERML"));
	GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, menu_alpha);
	xx += 6 * scale;
	GL_SetPatch(W_GetNumForName("M_THERM2"));	// in jdoom.wad
	GL_DrawRectTiled(xx, y, 8 * thermWidth * scale, height, 8 * scale, height);
	xx += 8 * thermWidth * scale;
	GL_SetPatch(W_GetNumForName("M_THERMR"));
	GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, menu_alpha);
	GL_SetPatch(W_GetNumForName("M_THERMO"));
	GL_DrawRect(x + (6 + thermDot * 8) * scale, y, 6 * scale, height, 1, 1, 1,
				menu_alpha);
}
#endif

/*
 * Draws a little colour box using the background box for a border
 */
void M_DrawColorBox(const Menu_t * menu, int index, float r, float g, float b, float a)
{
	int x = menu->x + 4;
	int y = menu->y + menu->itemHeight * (index  - menu->firstItem) + 4;

	if (a < 0) a = 1;

	M_DrawBackgroundBox(x, y, 2, 1, 1, 1, 1, 1, false, 1);
	GL_SetNoTexture();
	GL_DrawRect(x-1,y-1, 4, 3, r, g, b, a);
}

/*
 *  Draws a box using the border patches.
 *		Border is drawn outside.
 */
void M_DrawBackgroundBox(int x, int y, int w, int h, float red, float green, float blue, float alpha, boolean background, int border)
{
	dpatch_t	*t,*b,*l,*r,*tl,*tr,*br,*bl;

	int	up;

	switch(border)
    {
    case BORDERUP:
        t = &borderpatches[2];
        b = &borderpatches[0];
        l = &borderpatches[1];
        r = &borderpatches[3];
        tl = &borderpatches[6];
        tr = &borderpatches[7];
        br = &borderpatches[4];
        bl = &borderpatches[5];

        up = -1;
        break;
        
    case BORDERDOWN:
        t = &borderpatches[0];
        b = &borderpatches[2];
        l = &borderpatches[3];
        r = &borderpatches[1];
        tl = &borderpatches[4];
        tr = &borderpatches[5];
        br = &borderpatches[6];
        bl = &borderpatches[7];

        up = 1;
        break;
        
    default:
        break;
	}

	GL_SetColorAndAlpha(red, green, blue, menu_alpha);

	if(background)
    {
		GL_SetFlat(R_FlatNumForName(borderLumps[0]));
		GL_DrawRectTiled(x, y, w, h, 64, 64);
	}

	if(border)
    {
		// Top
		GL_SetPatch(t->lump);
		GL_DrawRectTiled(x, y - SHORT(t->height), w, SHORT(t->height), 
                         up * SHORT(t->width), up * SHORT(t->height));
		// Bottom
		GL_SetPatch(b->lump);
		GL_DrawRectTiled(x, y + h, w, SHORT(b->height), up * SHORT(b->width), 
                         up * SHORT(b->height));
		// Left
		GL_SetPatch(l->lump);
		GL_DrawRectTiled(x - SHORT(l->width), y, SHORT(l->width), h, 
                         up * SHORT(l->width), up * SHORT(l->height));
		// Right
		GL_SetPatch(r->lump);
		GL_DrawRectTiled(x + w, y, SHORT(r->width), h, up * SHORT(r->width), 
                         up * SHORT(r->height));

		// Top Left
		GL_SetPatch(tl->lump);
		GL_DrawRectTiled(x - SHORT(tl->width), y - SHORT(tl->height), 
                         SHORT(tl->width), SHORT(tl->height), 
                         up * SHORT(tl->width), up * SHORT(tl->height));
		// Top Right
		GL_SetPatch(tr->lump);
		GL_DrawRectTiled(x + w, y - SHORT(tr->height), SHORT(tr->width), 
                         SHORT(tr->height), up * SHORT(tr->width), 
                         up * SHORT(tr->height));
		// Bottom Right
		GL_SetPatch(br->lump);
		GL_DrawRectTiled(x + w, y + h, SHORT(br->width), SHORT(br->height), 
                         up * SHORT(br->width), up * SHORT(br->height));
		// Bottom Left
		GL_SetPatch(bl->lump);
		GL_DrawRectTiled(x - SHORT(bl->width), y + h, SHORT(bl->width), 
                         SHORT(bl->height), up * SHORT(bl->width), 
                         up * SHORT(bl->height));
	}
}

//---------------------------------------------------------------------------
//
// PROC M_DrawSlider
//
//---------------------------------------------------------------------------

void M_DrawSlider(const Menu_t * menu, int item, int width, int slot)
{
#ifndef __JDOOM__
	int     x;
	int     y;

	x = menu->x + 24;
	y = menu->y + 2 + (menu->itemHeight * (item  - menu->firstItem));

	gl.Color4f( 1, 1, 1, menu_alpha);

	GL_DrawPatch_CS(x - 32, y, W_GetNumForName("M_SLDLT"));
	GL_DrawPatch_CS(x + width * 8, y, W_GetNumForName("M_SLDRT"));

	GL_SetPatch(W_GetNumForName("M_SLDMD1"));
	GL_DrawRectTiled(x - 1, y + 1, width * 8 + 2, 13, 8, 13);

	gl.Color4f( 1, 1, 1, menu_alpha);
	GL_DrawPatch_CS(x + 4 + slot * 8, y + 7, W_GetNumForName("M_SLDKB"));
#else
	int     offx = 0;

	if(menu->items[item].text)
		offx = M_StringWidth(menu->items[item].text, menu->font);

		offx /= 4;
		offx *= 4;
	M_DrawThermo2(menu->x + 6 + offx, menu->y + menu->itemHeight * item,
				  width, slot, menu->itemHeight - 1);
#endif
}

#ifdef __JDOOM__
void M_DrawThermo(int x, int y, int thermWidth, int thermDot)
{
	M_DrawThermo2(x, y, thermWidth, thermDot, 13);
}
#endif


int CCmdMenuAction(int argc, char **argv)
{
	if(!stricmp(argv[0], "helpscreen"))	// F1
	{
		M_StartControlPanel();
		MenuTime = 0;
#ifdef __JDOOM__
		if(gamemode == retail)
			currentMenu = &ReadDef2;
		else
#endif
			currentMenu = &ReadDef1;

		itemOn = 0;
		S_LocalSound(menusnds[3], NULL);
	}
	else if(!stricmp(argv[0], "SaveGame"))	// F2
	{
		M_StartControlPanel();
		MenuTime = 0;
		S_LocalSound(menusnds[3], NULL);
		M_SaveGame(0, NULL);
	}
	else if(!stricmp(argv[0], "LoadGame"))
	{
		M_StartControlPanel();
		MenuTime = 0;
		S_LocalSound(menusnds[3], NULL);
		M_LoadGame(0, NULL);
	}
	else if(!stricmp(argv[0], "SoundMenu"))	// F4
	{
		M_StartControlPanel();
		MenuTime = 0;
		currentMenu = &Options2Def;
		itemOn = 0;				// sfx_vol
		S_LocalSound(menusnds[3], NULL);
	}
	else if(!stricmp(argv[0], "QuickSave"))	// F6
	{
		S_LocalSound(menusnds[3], NULL);
		MenuTime = 0;
		M_QuickSave();
	}
	else if(!stricmp(argv[0], "EndGame"))	// F7
	{
		S_LocalSound(menusnds[3], NULL);
		MenuTime = 0;
		M_EndGame(0, NULL);
	}
	else if(!stricmp(argv[0], "ToggleMsgs"))	// F8
	{
		MenuTime = 0;
		M_ChangeMessages(0, NULL);
		S_LocalSound(menusnds[3], NULL);
	}
	else if(!stricmp(argv[0], "QuickLoad"))	// F9
	{
		S_LocalSound(menusnds[3], NULL);
		MenuTime = 0;
		M_QuickLoad();
	}
	else if(!stricmp(argv[0], "quit"))	// F10
	{
		if(IS_DEDICATED)
			Con_Execute("quit!", true);
		else
		{
		S_LocalSound(menusnds[3], NULL);
			M_QuitDOOM(0, NULL);
		}
	}
	else if(!stricmp(argv[0], "ToggleGamma"))	// F11
	{
		char    buf[50];

		usegamma++;
		if(usegamma > 4)
			usegamma = 0;
#ifdef __JDOOM__
		P_SetMessage(players + consoleplayer, gammamsg[usegamma]);
#endif
		sprintf(buf, "setgamma %i", usegamma);
		Con_Execute(buf, false);
	}
	return true;
}
