/* $Id$
 *
 * Compiles for jDoom, jHeretic and jHexen.
 * Based on the original DOOM menu code.
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * mn_menu.c: Common selection menu, options, episode etc.
 *            Sliders and icons. Kinda widget stuff.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  pragma warning(disable:4018)
#endif

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "hu_stuff.h"
#include "f_infine.h"
#include "am_map.h"
#include "x_hair.h"
#include "p_player.h"

#if __JDOOM__
# include "doomdef.h"
# include "dstrings.h"
# include "d_config.h"
# include "g_game.h"
# include "g_common.h"
# include "m_argv.h"
# include "s_sound.h"
# include "doomstat.h"
# include "p_local.h"
# include "m_menu.h"
# include "m_ctrl.h"
# include "mn_def.h"
# include "wi_stuff.h"
# include "p_saveg.h"
#elif __JHERETIC__
# include "jheretic.h"
#elif __JHEXEN__
# include "jhexen.h"
# include <lzss.h>
#elif __JSTRIFE__
# include "h2def.h"
# include "p_local.h"
# include "r_local.h"
# include "soundst.h"
# include "h2_actn.h"
# include "mn_def.h"
# include "d_config.h"
# include <LZSS.h>
#endif

// MACROS ------------------------------------------------------------------

#define OBSOLETE        CVF_HIDE|CVF_NO_ARCHIVE

#define SAVESTRINGSIZE     24
#define CVAR(typ, x)    (*(typ*)Con_GetVariable(x)->ptr)

// QuitDOOM messages
#ifndef __JDOOM__
#define NUM_QUITMESSAGES  0
#endif

// TYPES -------------------------------------------------------------------

typedef struct {
    float *r, *g, *b, *a;
} rgba_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void Ed_MakeCursorVisible(void);
extern void Ed_MakeCursorVisible(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void SetMenu(MenuType_t menu);
void InitFonts(void);

void M_NewGame(int option, void *data);
void M_Episode(int option, void *data);        // Does nothing in jHEXEN
void M_ChooseClass(int option, void *data);    // Does something only in jHEXEN
void M_ChooseSkill(int option, void *data);
void M_LoadGame(int option, void *data);
void M_SaveGame(int option, void *data);
void M_GameFiles(int option, void *data);        // Does nothing in jDOOM
void M_EndGame(int option, void *data);
void M_ReadThis(int option, void *data);
void M_ReadThis2(int option, void *data);

#ifndef __JDOOM__
void M_ReadThis3(int option, void *data);
#endif

void M_QuitDOOM(int option, void *data);

void M_ToggleVar(int option, void *data);

void M_OpenDCP(int option, void *data);
void M_ChangeMessages(int option, void *data);
void M_AutoSwitch(int option, void *data);
void M_HUDInfo(int option, void *data);
void M_HUDScale(int option, void *data);
void M_SfxVol(int option, void *data);
void M_WeaponOrder(int option, void *data);
void M_MusicVol(int option, void *data);
void M_SizeDisplay(int option, void *data);
void M_SizeStatusBar(int option, void *data);
void M_StatusBarAlpha(int option, void *data);
void M_HUDRed(int option, void *data);
void M_HUDGreen(int option, void *data);
void M_HUDBlue(int option, void *data);
void M_MouseXSensi(int option, void *data);
void M_MouseYSensi(int option, void *data);
void M_JoyAxis(int option, void *data);
void M_FinishReadThis(int option, void *data);
void M_LoadSelect(int option, void *data);
void M_SaveSelect(int option, void *data);
void M_Xhair(int option, void *data);
void M_XhairSize(int option, void *data);

#ifdef __JDOOM__
void M_XhairR(int option, void *data);
void M_XhairG(int option, void *data);
void M_XhairB(int option, void *data);
void M_XhairAlpha(int option, void *data);
#endif

void M_StartControlPanel(void);
void M_ReadSaveStrings(void);
void M_DoSave(int slot);
void M_DoLoad(int slot);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);

#ifndef __JDOOM__
void M_DrawReadThis3(void);
#endif

void M_DrawSkillMenu(void);
void M_DrawClassMenu(void);            // Does something only in jHEXEN
void M_DrawEpisode(void);            // Does nothing in jHEXEN
void M_DrawOptions(void);
void M_DrawOptions2(void);
void M_DrawGameplay(void);
void M_DrawHUDMenu(void);
void M_DrawMapMenu(void);
void M_DrawWeaponMenu(void);
void M_DrawMouseMenu(void);
void M_DrawJoyMenu(void);
void M_DrawLoad(void);
void M_DrawSave(void);
void M_DrawFilesMenu(void);
void M_DrawBackground(void);

void M_DrawBackgroundBox(int x, int y, int w, int h,
                         float r, float g, float b, float a,
                         boolean background, int border);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern EditField_t *ActiveEdit;    // in Mn_def.h

extern boolean message_dontfuckwithme;
extern boolean chat_on;            // in heads-up code

#ifdef __JDOOM__
extern int joyaxis[3];
#endif

extern char *weaponNames[];

extern int     typein_time;    // in heads-up code

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
Menu_t *currentMenu;

// Blocky mode, has default, 0 = high, 1 = normal
int     detailLevel;
int     screenblocks = 10;        // has default

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

int     messageResponse;

// message x & y
int     messx;
int     messy;
int     messageLastMenuActive;

// timed message = no input from user
boolean messageNeedsInput;

boolean    (*messageRoutine) (int response, void *data);

// old save description before edit
char    saveOldString[SAVESTRINGSIZE];

char    savegamestrings[10][SAVESTRINGSIZE];

// we are going to be entering a savegame string
int     saveStringEnter;
int     saveSlot;                // which slot to save in
int     saveCharIndex;            // which char we're editing

char    endstring[160];

#if __JDOOM__
static char *yesno[3] = { "NO", "YES", "MAYBE?" };
#else
static char *yesno[2] = { "NO", "YES" };
#endif

#if __JDOOM__ || __JHERETIC__
char   *episodemsg;
int     epi;
int     mouseSensitivity;    // has default
#endif

boolean shiftdown;
static char shiftTable[59] =    // Contains characters 32 to 90.
{
    /* 32 */ 0, 0, 0, 0, 0, 0, 0, '"',
    /* 40 */ 0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
    /* 50 */ '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
    /* 60 */ 0, '+', 0, 0, 0, 0, 0, 0, 0, 0,
    /* 70 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 80 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 90 */ 0
};

// Alpha level for the entire menu. Used primarily by M_WriteText2().
float   menu_alpha = 0;
int     menu_color = 0;
float   skull_angle = 0;

int     frame;    // used by any graphic animations that need to be pumped
int     usegamma;
int     MenuTime;

short   itemOn;    // menu item skull is on
short   previtemOn;    // menu item skull was last on (for restoring when leaving widget control)
short   skullAnimCounter;    // skull animation counter
short   whichSkull;    // which skull to draw

// Sounds played in the menu
int menusnds[] = {
#if __JDOOM__
    sfx_dorcls,            // close menu
    sfx_swtchx,            // open menu
    sfx_swtchn,            // cancel
    sfx_pstop,             // up/down
    sfx_stnmov,            // left/right
    sfx_pistol,            // accept
    sfx_oof                // bad sound (eg can't autosave)
#elif __JHERETIC__
    sfx_switch,
    sfx_chat,
    sfx_switch,
    sfx_switch,
    sfx_stnmov,
    sfx_chat,
    sfx_chat
#elif __JHEXEN__
    SFX_PLATFORM_STOP,
    SFX_DOOR_LIGHT_CLOSE,
    SFX_FIGHTER_HAMMER_HITWALL,
    SFX_PICKUP_KEY,
    SFX_FIGHTER_HAMMER_HITWALL,
    SFX_CHAT,
    SFX_CHAT
#endif
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#ifndef __JDOOM__
static int SkullBaseLump;
#endif

#ifdef __JSTRIFE__
static int    cursors = 8;
dpatch_t cursorst[8];
#else
static int    cursors = 2;
dpatch_t cursorst[2];
#endif

static dpatch_t     borderpatches[8];

#if __JHEXEN__
static int MenuPClass;
#endif

static rgba_t widgetcolors[] = {        // ptrs to colors editable with the colour widget
    { &cfg.automapL0[0], &cfg.automapL0[1], &cfg.automapL0[2], NULL },
    { &cfg.automapL1[0], &cfg.automapL1[1], &cfg.automapL1[2], NULL },
    { &cfg.automapL2[0], &cfg.automapL2[1], &cfg.automapL2[2], NULL },
    { &cfg.automapL3[0], &cfg.automapL3[1], &cfg.automapL3[2], NULL },
    { &cfg.automapBack[0], &cfg.automapBack[1], &cfg.automapBack[2], &cfg.automapBack[3] },
    { &cfg.hudColor[0], &cfg.hudColor[1], &cfg.hudColor[2], &cfg.hudColor[3] }
};

static boolean WidgetEdit = false;    // No active widget by default.
static boolean rgba = false;        // used to swap between rgb / rgba modes for the color widget

static int editcolorindex = 0;        // The index of the widgetcolors array of the item being currently edited

static float currentcolor[4] = {0, 0, 0, 0};    // Used by the widget as temporay values

static int menuFogTexture = 0;
static float mfSpeeds[2] = { 1 * .05f, 1 * -.085f };

static float mfAngle[2] = { 93, 12 };
static float mfPosAngle[2] = { 35, 77 };
static float mfPos[2][2];
static float mfAlpha = 0;

static float mfYjoin = 0.5f;
static boolean updown = true;

static float outFade = 0;
static boolean fadingOut = false;
static int menuDarkTicks = 15;
static int quitAsk = 0; // set ON so menu fog is rendered with the quit message
#if !defined( __JHEXEN__ ) && !defined( __JHERETIC__ )
static int quitYet = 0; // prevents multiple quit responses
#endif
static int slamInTicks = 9;

// used to fade out the background a little when a widget is active
static float menu_calpha = 0;

//static boolean FileMenuKeySteal;
//static boolean slottextloaded;
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
    {ITT_EFUNC, 0, "{case}New Game", M_NewGame, 0, "M_NGAME"},
    {ITT_EFUNC, 0, "{case}Multiplayer", SCEnterMultiplayerMenu, 0},
    {ITT_SETMENU, 0, "{case}Options", NULL, MENU_OPTIONS, "M_OPTION" },
    {ITT_EFUNC, 0, "{case}Load Game", M_LoadGame, 0, "M_LOADG"},
    {ITT_EFUNC, 0, "{case}Save Game", M_SaveGame, 0, "M_SAVEG"},
    {ITT_EFUNC, 0, "{case}Read This!", M_ReadThis, 0, "M_RDTHIS"},
    {ITT_EFUNC, 0, "{case}Quit Game", M_QuitDOOM, 0, "M_QUITG"}
#elif __JSTRIFE__
    {ITT_EFUNC, 0, "N", M_NewGame, 0, ""},
    {ITT_EFUNC, 0, "M", SCEnterMultiplayerMenu, 0, ""},
    {ITT_SETMENU, 0, "O", NULL, 0, MENU_OPTIONS},
    {ITT_EFUNC, 0, "L", M_LoadGame, 0, ""},
    {ITT_EFUNC, 0, "S", M_SaveGame, 0, ""},
    {ITT_EFUNC, 0, "R", M_ReadThis, 0, ""},
    {ITT_EFUNC, 0, "Q", M_QuitDOOM, 0, ""}
#else
    {ITT_EFUNC, 0, "new game", M_NewGame, 0},
    {ITT_EFUNC, 0, "multiplayer", SCEnterMultiplayerMenu, 0},
    {ITT_SETMENU, 0, "options", NULL, MENU_OPTIONS},
    {ITT_SETMENU, 0, "game files", NULL, MENU_FILES},
    {ITT_EFUNC, 0, "info", M_ReadThis, 0},
    {ITT_EFUNC, 0, "quit game", M_QuitDOOM, 0}
#endif
};

Menu_t MainDef = {
#if __JHEXEN__
    110, 50,
    M_DrawMainMenu,
    6, MainItems,
    0, MENU_NONE, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT_B,
    0, 6
#elif __JHERETIC__
    110, 64,
    M_DrawMainMenu,
    6, MainItems,
    0, MENU_NONE, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT_B,
    0, 6
#elif __JSTRIFE__
    97, 64,
    M_DrawMainMenu,
    7, MainItems,
    0, MENU_NONE, 0,
    hu_font_a,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT_B + 1,
    0, 7
#else
    97, 64,
    M_DrawMainMenu,
    7, MainItems,
    0, MENU_NONE, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT_B + 1,
    0, 7
#endif
};

#if __JHEXEN__
MenuItem_t ClassItems[] = {
    {ITT_EFUNC, 0, "FIGHTER", M_ChooseClass, 0},
    {ITT_EFUNC, 0, "CLERIC", M_ChooseClass, 1},
    {ITT_EFUNC, 0, "MAGE", M_ChooseClass, 2}
};

Menu_t ClassDef = {
    66, 66,
    M_DrawClassMenu,
    3, ClassItems,
    0, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT_B + 1,
    0, 3
};
#endif

#ifdef __JHERETIC__
MenuItem_t EpisodeItems[] = {
    {ITT_EFUNC, 0, "city of the damned", M_Episode, 1},
    {ITT_EFUNC, 0, "hell's maw", M_Episode, 2},
    {ITT_EFUNC, 0, "the dome of d'sparil", M_Episode, 3},
    {ITT_EFUNC, 0, "the ossuary", M_Episode, 4},
    {ITT_EFUNC, 0, "the stagnant demesne", M_Episode, 5}
};

Menu_t EpiDef = {
    48, 50,
    M_DrawEpisode,
    3, EpisodeItems,
    0, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT + 1,
    0, 3
};
#elif __JDOOM__
MenuItem_t EpisodeItems[] = {
    // Text defs TXT_EPISODE1..4.
    {ITT_EFUNC, 0, "K", M_Episode, 0, "M_EPI1" },
    {ITT_EFUNC, 0, "T", M_Episode, 1, "M_EPI2" },
    {ITT_EFUNC, 0, "I", M_Episode, 2, "M_EPI3" },
    {ITT_EFUNC, 0, "T", M_Episode, 3, "M_EPI4" }
};

Menu_t  EpiDef = {
    48, 63,
    M_DrawEpisode,
    4, EpisodeItems,
    0, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT + 1,
    0, 4
};
#endif


#ifndef __JDOOM__
static MenuItem_t FilesItems[] = {
    {ITT_EFUNC, 0, "load game", M_LoadGame, 0},
    {ITT_EFUNC, 0, "save game", M_SaveGame, 0}
};

static Menu_t FilesMenu = {
    110, 60,
    M_DrawFilesMenu,
    2, FilesItems,
    0, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT + 1,
    0, 2
};
#endif

enum { load_end = NUMSAVESLOTS };

static MenuItem_t LoadItems[] = {
    {ITT_EFUNC, 0, "1", M_LoadSelect, 0, ""},
    {ITT_EFUNC, 0, "2", M_LoadSelect, 1, ""},
    {ITT_EFUNC, 0, "3", M_LoadSelect, 2, ""},
    {ITT_EFUNC, 0, "4", M_LoadSelect, 3, ""},
    {ITT_EFUNC, 0, "5", M_LoadSelect, 4, ""},
    {ITT_EFUNC, 0, "6", M_LoadSelect, 5, ""},
#if __JDOOM__ || __JHERETIC__
    {ITT_EFUNC, 0, "7", M_LoadSelect, 6, ""},
    {ITT_EFUNC, 0, "8", M_LoadSelect, 7, ""}
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
    0, MENU_MAIN, 0,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A + 8,
    0, NUMSAVESLOTS
};

static MenuItem_t SaveItems[] = {
    {ITT_EFUNC, 0, "1", M_SaveSelect, 0, ""},
    {ITT_EFUNC, 0, "2", M_SaveSelect, 1, ""},
    {ITT_EFUNC, 0, "3", M_SaveSelect, 2, ""},
    {ITT_EFUNC, 0, "4", M_SaveSelect, 3, ""},
    {ITT_EFUNC, 0, "5", M_SaveSelect, 4, ""},
    {ITT_EFUNC, 0, "6", M_SaveSelect, 5, ""},
#if __JDOOM__ || __JHERETIC__
    {ITT_EFUNC, 0, "7", M_SaveSelect, 6, ""},
    {ITT_EFUNC, 0, "8", M_SaveSelect, 7, ""}
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
    0, MENU_MAIN, 0,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A + 8,
    0, NUMSAVESLOTS
};

#if __JSTRIFE__
static MenuItem_t SkillItems[] = {
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_baby},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_easy},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_medium},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_hard},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_nightmare}
};

static Menu_t SkillDef = {
    120, 44,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT,
    0, 5
};

#elif __JHEXEN__
static MenuItem_t SkillItems[] = {
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_baby},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_easy},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_medium},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_hard},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, sk_nightmare}
};

static Menu_t SkillDef = {
    120, 44,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_CLASS, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT,
    0, 5
};
#elif __JHERETIC__
static MenuItem_t SkillItems[] = {
    {ITT_EFUNC, 0, "thou needet a wet-nurse", M_ChooseSkill, sk_baby},
    {ITT_EFUNC, 0, "yellowbellies-r-us", M_ChooseSkill, sk_easy},
    {ITT_EFUNC, 0, "bringest them oneth", M_ChooseSkill, sk_medium},
    {ITT_EFUNC, 0, "thou art a smite-meister", M_ChooseSkill, sk_hard},
    {ITT_EFUNC, 0, "black plague possesses thee", M_ChooseSkill, sk_nightmare}
};

static Menu_t SkillDef = {
    38, 30,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_EPISODE, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT,
    0, 5
};
#elif __JDOOM__
static MenuItem_t SkillItems[] = {
    // Text defs TXT_SKILL1..5.
    {ITT_EFUNC, 0, "I", M_ChooseSkill, 0, "M_JKILL"},
    {ITT_EFUNC, 0, "H", M_ChooseSkill, 1, "M_ROUGH"},
    {ITT_EFUNC, 0, "H", M_ChooseSkill, 2, "M_HURT"},
    {ITT_EFUNC, 0, "U", M_ChooseSkill, 3, "M_ULTRA"},
    {ITT_EFUNC, MIF_NOTALTTXT, "N", M_ChooseSkill, 4, "M_NMARE"}
};

static Menu_t SkillDef = {
    48, 63,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_EPISODE, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT,
    0, 5
};
#endif

static MenuItem_t OptionsItems[] = {
    {ITT_EFUNC, 0, "end game", M_EndGame, 0},
    {ITT_EFUNC, 0, "control panel", M_OpenDCP, 0},
    {ITT_SETMENU, 0, "gameplay...", NULL, MENU_GAMEPLAY},
    {ITT_SETMENU, 0, "hud...", NULL, MENU_HUD},
    {ITT_SETMENU, 0, "automap...", NULL, MENU_MAP},
    {ITT_SETMENU, 0, "weapons...", NULL, MENU_WEAPONSETUP},
    {ITT_SETMENU, 0, "sound...", NULL, MENU_OPTIONS2},
    {ITT_SETMENU, 0, "controls...", NULL, MENU_CONTROLS},
    {ITT_SETMENU, 0, "mouse options...", NULL, MENU_MOUSE},
    {ITT_SETMENU, 0, "joystick options...", NULL, MENU_JOYSTICK}
};

static Menu_t OptionsDef = {
    94, 84,
    M_DrawOptions,
    10, OptionsItems,
    0, MENU_MAIN, 0,
    hu_font_a,                    //1, 0, 0,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 10
};

static MenuItem_t Options2Items[] = {
    {ITT_LRFUNC, 0, "SFX VOLUME :", M_SfxVol, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "MUSIC VOLUME :", M_MusicVol, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_EFUNC, 0, "OPEN AUDIO PANEL", M_OpenDCP, 1},
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
    0, MENU_OPTIONS, 0,
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
    {ITT_EFUNC, 0, "", M_ReadThis2, 0}
};

Menu_t  ReadDef1 = {
    280, 185,
    M_DrawReadThis1,
    1, ReadItems1,
    0, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT,
    0, 1
};

MenuItem_t ReadItems2[] = {
#ifdef __JDOOM__                    // heretic and hexen have 3 readthis screens.
    {ITT_EFUNC, 0, "", M_FinishReadThis, 0}
#else
    {ITT_EFUNC, 0, "", M_ReadThis3, 0}
#endif
};

Menu_t  ReadDef2 = {
    330, 175,
    M_DrawReadThis2,
    1, ReadItems2,
    0, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT,
    0, 1
};

#ifndef __JDOOM__
MenuItem_t ReadItems3[] = {
    {ITT_EFUNC, 0, "", M_FinishReadThis, 0}
};

Menu_t  ReadDef3 = {
    330, 175,
    M_DrawReadThis3,
    1, ReadItems3,
    0, MENU_MAIN, 0,
    hu_font_b,                    //1, 0, 0,
    cfg.menuColor,
    LINEHEIGHT,
    0, 1
};
#endif

static MenuItem_t HUDItems[] = {
#ifdef __JDOOM__
    {ITT_EFUNC, 0, "show ammo :", M_ToggleVar, 0, NULL, "hud-ammo" },
    {ITT_EFUNC, 0, "show armor :", M_ToggleVar, 0, NULL, "hud-armor" },
    {ITT_EFUNC, 0, "show face :", M_ToggleVar, 0, NULL, "hud-face" },
    {ITT_EFUNC, 0, "show health :", M_ToggleVar, 0, NULL,"hud-health" },
    {ITT_EFUNC, 0, "show keys :", M_ToggleVar, 0, NULL, "hud-keys" },

    {ITT_LRFUNC, 0, "scale", M_HUDScale, 0},
    {ITT_EFUNC, 0, "   HUD color", SCColorWidget, 5},
#endif
    {ITT_EFUNC, 0, "MESSAGES :", M_ChangeMessages, 0},
    {ITT_LRFUNC, 0, "CROSSHAIR :", M_Xhair, 0},
    {ITT_LRFUNC, 0, "CROSSHAIR SIZE :", M_XhairSize, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "SCREEN SIZE", M_SizeDisplay, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "STATUS BAR SIZE", M_SizeStatusBar, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "STATUS BAR ALPHA :", M_StatusBarAlpha, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
#if __JHEXEN__ || __JSTRIFE__
    {ITT_INERT, 0, "FULLSCREEN HUD",    NULL, 0},
    {ITT_INERT, 0, "FULLSCREEN HUD",    NULL, 0},
    {ITT_EFUNC, 0, "SHOW MANA :", M_ToggleVar, 0, NULL, "hud-mana" },
    {ITT_EFUNC, 0, "SHOW HEALTH :", M_ToggleVar, 0, NULL, "hud-health" },
    {ITT_EFUNC, 0, "SHOW ARTIFACT :", M_ToggleVar, 0, NULL, "hud-artifact" },
    {ITT_EFUNC, 0, "   HUD COLOUR", SCColorWidget, 5},
    {ITT_LRFUNC, 0, "SCALE", M_HUDScale, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#elif __JHERETIC__
    {ITT_INERT, 0, "FULLSCREEN HUD",    NULL, 0},
    {ITT_INERT, 0, "FULLSCREEN HUD",    NULL, 0},
    {ITT_EFUNC, 0, "SHOW AMMO :", M_ToggleVar, 0, NULL, "hud-ammo" },
    {ITT_EFUNC, 0, "SHOW ARMOR :", M_ToggleVar, 0, NULL, "hud-armor" },
    {ITT_EFUNC, 0, "SHOW ARTIFACT :", M_ToggleVar, 0, NULL, "hud-artifact" },
    {ITT_EFUNC, 0, "SHOW HEALTH :", M_ToggleVar, 0, NULL, "hud-health" },
    {ITT_EFUNC, 0, "SHOW KEYS :", M_ToggleVar, 0, NULL, "hud-keys" },
    {ITT_EFUNC, 0, "   HUD COLOUR", SCColorWidget, 5},
    {ITT_LRFUNC, 0, "SCALE", M_HUDScale, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0}
#endif
};

#ifdef __JDOOM__
Menu_t ControlsDef = {
    32, 40,
    M_DrawControlsMenu,
    NUM_CONTROLS_ITEMS, ControlsItems,
    1, MENU_OPTIONS, 1,
    hu_font_a,                    //1, 0, 0,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 16
};
#endif

#ifdef __JHERETIC__
Menu_t ControlsDef = {
    32, 26,
    M_DrawControlsMenu,
    NUM_CONTROLS_ITEMS, ControlsItems,
    1, MENU_OPTIONS, 1,
    hu_font_a,                    //1, 0, 0,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 17
};
#endif

#ifdef __JHEXEN__
Menu_t ControlsDef = {
    32, 21,
    M_DrawControlsMenu,
    NUM_CONTROLS_ITEMS, ControlsItems,
    1, MENU_OPTIONS, 1,
    hu_font_a,                    //1, 0, 0,
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
    0, MENU_OPTIONS, 0,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
#if __JHEXEN__ || __JSTRIFE__
    0, 15        // 21
#elif __JHERETIC__
    0, 15        // 23
#elif __JDOOM__
    0, 13
#endif
};

static MenuItem_t WeaponItems[] = {
    {ITT_EMPTY, 0, "Use left/right to move", NULL, 0 },
    {ITT_EMPTY, 0, "item up/down the list." , NULL, 0 },
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "WEAPON ORDER", NULL, 0},
    {ITT_LRFUNC, 0, "1 :", M_WeaponOrder, 0 << NUMWEAPONS },
    {ITT_LRFUNC, 0, "2 :", M_WeaponOrder, 1 << NUMWEAPONS },
    {ITT_LRFUNC, 0, "3 :", M_WeaponOrder, 2 << NUMWEAPONS },
    {ITT_LRFUNC, 0, "4 :", M_WeaponOrder, 3 << NUMWEAPONS },
#if !__JHEXEN__
    {ITT_LRFUNC, 0, "5 :", M_WeaponOrder, 4 << NUMWEAPONS },
    {ITT_LRFUNC, 0, "6 :", M_WeaponOrder, 5 << NUMWEAPONS },
    {ITT_LRFUNC, 0, "7 :", M_WeaponOrder, 6 << NUMWEAPONS },
    {ITT_LRFUNC, 0, "8 :", M_WeaponOrder, 7 << NUMWEAPONS },
#endif
#if __JDOOM__
    {ITT_LRFUNC, 0, "9 :", M_WeaponOrder, 8 << NUMWEAPONS },
#endif
    {ITT_EFUNC, 0, "Use with Next/Previous :", M_ToggleVar, 0, NULL, "player-weapon-nextmode"},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_LRFUNC, 0, "AUTOSWITCH :", M_AutoSwitch, 0},
#if __JDOOM__
    {ITT_EFUNC, 0, "BERSERK AUTOSWITCH :", M_ToggleVar, 0, NULL, "player-autoswitch-berserk"}
#endif
};

static Menu_t WeaponDef = {
#if !__JDOOM__
    60, 30,
#else
    78, 44,
#endif
    M_DrawWeaponMenu,
#ifdef __JDOOM__
    17, WeaponItems,
#elif __JHERETIC__
    15, WeaponItems,
#elif __JHEXEN__
    11, WeaponItems,
#endif
    0, MENU_OPTIONS, 1,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
#ifdef __JDOOM__
    0, 17
#elif __JHERETIC__
    0, 15
#elif __JHEXEN__
    0, 11
#endif
};

static MenuItem_t GameplayItems[] = {
    {ITT_EFUNC, 0, "ALWAYS RUN :", M_ToggleVar, 0, NULL, "ctl-run"},
    {ITT_EFUNC, 0, "USE LOOKSPRING :", M_ToggleVar, 0, NULL, "ctl-look-spring"},
    {ITT_EFUNC, 0, "USE AUTOAIM :", M_ToggleVar, 0, NULL, "ctl-aim-noauto"},
#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
    {ITT_EFUNC, 0, "ALLOW JUMPING :", M_ToggleVar, 0, NULL, "player-jump"},
#endif

#if __JDOOM__ || __JHERETIC__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "COMPATIBILITY", NULL, 0 },
#  if __JDOOM__
    {ITT_EFUNC, 0, "ANY BOSS TRIGGER 666 :", M_ToggleVar, 0, NULL,
        "game-anybossdeath666"},
    {ITT_EFUNC, 0, "AV RESURRECTS GHOSTS :", M_ToggleVar, 0, NULL,
        "game-raiseghosts"},
    {ITT_EFUNC, 0, "PE LIMITED TO 20 LOST SOULS :", M_ToggleVar, 0, NULL,
        "game-maxskulls"},
    {ITT_EFUNC, 0, "LS CAN GET STUCK INSIDE WALLS :", M_ToggleVar, 0, NULL,
        "game-skullsinwalls"},
#  endif
#  if __JDOOM__ || __JHERETIC__
    {ITT_EFUNC, 0, "MONSTERS CAN GET STUCK IN DOORS :", M_ToggleVar, 0, NULL,
        "game-monsters-stuckindoors"},
    {ITT_EFUNC, 0, "SOME OBJECTS HANG OVER LEDGES :", M_ToggleVar, 0, NULL,
        "game-objects-hangoverledges"},
    {ITT_EFUNC, 0, "OBJECTS FALL UNDER OWN WEIGHT :", M_ToggleVar, 0, NULL,
        "game-objects-falloff"},
    {ITT_EFUNC, 0, "CORPSES SLIDE DOWN STAIRS :", M_ToggleVar, 0, NULL,
        "game-corpse-sliding"},
    {ITT_EFUNC, 0, "USE EXACTLY DOOM'S CLIPPING CODE :", M_ToggleVar, 0, NULL,
        "game-objects-clipping"},
    {ITT_EFUNC, 0, "  ^IFNOT NORTHONLY WALLRUNNING :", M_ToggleVar, 0, NULL,
        "game-player-wallrun-northonly"},
#  endif
#  if __JDOOM__
    {ITT_EFUNC, 0, "ZOMBIE PLAYERS CAN EXIT LEVELS :", M_ToggleVar, 0, NULL,
        "game-zombiescanexit"},
#  endif
#endif
};

#if __JHEXEN__
static Menu_t GameplayDef = {
    64, 25,
    M_DrawGameplay,
    3, GameplayItems,
    0, MENU_OPTIONS, 0,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 3
};
#else
static Menu_t GameplayDef = {
#if __JHERETIC__
    30, 30,
#else
    30, 40,
#endif
    M_DrawGameplay,
#if __JDOOM__
    17, GameplayItems,
#else
    12, GameplayItems,
#endif
    0, MENU_OPTIONS, 0,
    hu_font_a,
    cfg.menuColor2,
    LINEHEIGHT_A,
#if __JDOOM__
    0, 17
#else
    0, 12
#endif
};
#endif

static MenuItem_t MouseOptsItems[] = {
    {ITT_EFUNC, 0, "MOUSE LOOK :", M_ToggleVar, 0, NULL, "ctl-look-mouse"},
    {ITT_EFUNC, 0, "INVERSE MLOOK :", M_ToggleVar, 0, NULL, "ctl-look-mouse-inverse"},
    {ITT_LRFUNC, 0, "X SENSITIVITY", M_MouseXSensi, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "Y SENSITIVITY", M_MouseYSensi, 0},
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0}
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
    0, MENU_OPTIONS, 0,
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
    {ITT_LRFUNC, 0, "X AXIS :", M_JoyAxis, 0 << 8},
    {ITT_LRFUNC, 0, "Y AXIS :", M_JoyAxis, 1 << 8},
    {ITT_LRFUNC, 0, "Z AXIS :", M_JoyAxis, 2 << 8},
    {ITT_LRFUNC, 0, "RX AXIS :", M_JoyAxis, 3 << 8},
    {ITT_LRFUNC, 0, "RY AXIS :", M_JoyAxis, 4 << 8},
    {ITT_LRFUNC, 0, "RZ AXIS :", M_JoyAxis, 5 << 8},
    {ITT_LRFUNC, 0, "SLIDER 1 :", M_JoyAxis, 6 << 8},
    {ITT_LRFUNC, 0, "SLIDER 2 :", M_JoyAxis, 7 << 8},
    {ITT_EFUNC, 0, "JOY LOOK :", M_ToggleVar, 0, NULL, "ctl-look-joy"},
    {ITT_EFUNC, 0, "INVERSE LOOK :", M_ToggleVar, 0, NULL, "ctl-look-joy-inverse"},
    {ITT_EFUNC, 0, "POV LOOK :", M_ToggleVar, 0, NULL, "ctl-look-pov"}
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
    0, MENU_OPTIONS, 0,
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
    &WeaponDef,
    NULL
};

static MenuItem_t ColorWidgetItems[] = {
    {ITT_LRFUNC, 0, "red :    ", M_WGCurrentColor, 0, NULL, &currentcolor[0] },
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "green :", M_WGCurrentColor, 0, NULL, &currentcolor[1] },
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "blue :  ", M_WGCurrentColor, 0, NULL, &currentcolor[2] },
#ifndef __JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "alpha :", M_WGCurrentColor, 0, NULL, &currentcolor[3] },
};

static Menu_t ColorWidgetMnu = {
    98, 60,
    NULL,
#ifndef __JDOOM__
    10, ColorWidgetItems,
#else
    4, ColorWidgetItems,
#endif
    0, MENU_OPTIONS, 1,
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
    {"menu-patch-replacement", 0, CVT_BYTE, &cfg.usePatchReplacement, 0, 2,
        "Patch Replacement strings. 1=Enable external, 2=Enable built-in."},
    {"menu-slam", 0, CVT_BYTE, &cfg.menuSlam, 0, 1,
        "1=Slam the menu when opening."},
    {"menu-quick-ask", 0, CVT_BYTE, &cfg.askQuickSaveLoad, 0, 1,
        "1=Ask me to confirm when quick saving/loading."},
#ifdef __JDOOM__
    {"menu-quitsound", 0, CVT_INT, &cfg.menuQuitSound, 0, 1,
        "1=Play a sound when quitting the game."},
#endif

    {NULL}
};

// Console commands for the menu
ccmd_t  menuCCmds[] = {
    {"menu",        CCmdMenuAction, "Open/Close the menu."},
    {"menuup",      CCmdMenuAction, "Move the menu cursor up."},
    {"menudown",    CCmdMenuAction, "Move the menu cursor down."},
    {"menuleft",    CCmdMenuAction, "Move the menu cursor left."},
    {"menuright",   CCmdMenuAction, "Move the menu cursor right."},
    {"menuselect",  CCmdMenuAction, "Select/Accept the current menu item."},
    {"menucancel",  CCmdMenuAction, "Return to the previous menu page."},
    {"helpscreen",  CCmdMenuAction, "Show the Help screens."},
    {"savegame",    CCmdMenuAction, "Open the save game menu."},
    {"loadgame",    CCmdMenuAction, "Open the load game menu."},
    {"soundmenu",   CCmdMenuAction, "Open the sound settings menu."},
    {"quicksave",   CCmdMenuAction, "Quicksave the game."},
    {"endgame",     CCmdMenuAction, "End the game."},
    {"togglemsgs",  CCmdMenuAction, "Messages on/off."},
    {"quickload",   CCmdMenuAction, "Load the quicksaved game."},
    {"quit",        CCmdMenuAction, "Quit the game and return to the OS."},
    {"togglegamma", CCmdMenuAction, "Cycle gamma correction levels."},
    {"messageyes",  CCmdMsgResponse, "Respond - YES to the message promt."},
    {"messageno",   CCmdMsgResponse, "Respond - NO to the message promt."},
    {"messagecancel", CCmdMsgResponse, "Respond - CANCEL to the message promt."},
    {NULL}
};

// Code -------------------------------------------------------------------

/*
 * Called during the PreInit of each game during start up
 *
 *    Register Cvars and CCmds for the opperation/look of the menu.
 *    The cvars/ccmds for options modified within the menu are in the main ccmd/cvar arrays
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
 *  Load any resources the menu needs on init
 */
void M_LoadData(void)
{
    int i;
    char buffer[9];

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
 * The opposite of M_LoadData()
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
 * Init vars, fonts, adjust the menu structs, and anything else that
 * needs to be done before the menu can be used.
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
        w = M_StringWidth(EpisodeItems[i].text, EpiDef.font);
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
        w = M_StringWidth(EpisodeItems[i].text, EpiDef.font);
        if(w > maxw)
            maxw = w;
    }
    // Center the episodes menu appropriately.
    EpiDef.x = 160 - maxw / 2 + 12;
    // "Choose Episode"
    episodemsg = GET_TXT(TXT_ASK_EPISODE);

    // Skill names.
    for(i = 0, maxw = 0; i < 5; i++)
    {
        SkillItems[i].text = GET_TXT(TXT_SKILL1 + i);
        w = M_StringWidth(SkillItems[i].text, SkillDef.font);
        if(w > maxw)
            maxw = w;
    }
    // Center the skill menu appropriately.
    SkillDef.x = 160 - maxw / 2 + 12;
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
        item->text = "{case}Quit Game";
        item->lumpname = "M_QUITG";
        MainDef.itemCount = 6;
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
        EpiDef.itemCount = 3;
        item = &MainItems[readthis];
        item->func = M_ReadThis;
        item->text = "{case}Read This!";
        item->lumpname = "M_RDTHIS";
        MainDef.itemCount = 7;
        MainDef.y = 64;
        break;
    case retail:
        // We are fine.
        EpiDef.itemCount = 4;
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
    if(gamemode == extended)
    {                            // Add episodes 4 and 5 to the menu
        EpiDef.itemCount = EpiDef.numVisItems = 5;
        EpiDef.y = 50 - ITEM_HEIGHT;
    }
#endif

}

/*
 * Updates on Game Tick.
 */
void MN_Ticker(void)
{
    int     i;

    // Check if there has been a response to a message
    if(messageToPrint)
    {
        if(messageNeedsInput == true)
        {
            if(messageRoutine)
                messageRoutine(0, NULL);
        }
    }

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
    if((menuactive && !messageToPrint) || quitAsk && menuactive)
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
 * Sets the view matrix up for rendering the menu
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
#define BUFSIZE 80

    static short x;
    static short y;
    short   i;
    short   max;
    char    string[BUFSIZE];
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
        M_SetMenuMatrix(messageToPrint? 1 : temp);    // don't slam messages

    // Don't change back to the menu after a canceled quit
    if(!menuactive && quitAsk)
        goto end_draw_menu;

    // Horiz. & Vertically center string and print it.
    if(messageToPrint)
    {
        start = 0;

        y = 100 - M_StringHeight(messageString, hu_font_a) / 2;
        while(*(messageString + start))
        {
            for(i = 0; i < strlen(messageString + start); i++)
                if(*(messageString + start + i) == '\n' || i > BUFSIZE-1)
                {
                    memset(string, 0, BUFSIZE);
                    strncpy(string, messageString + start, i);
                    string[BUFSIZE] = 0;
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
        currentMenu->drawFunc();    // call Draw routine

    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->itemCount;

    if(menu_alpha > 0)
    {
        for(i = currentMenu->firstItem;
            i < max && i < currentMenu->firstItem + currentMenu->numVisItems; i++)
        {
            float   t, r, g, b;

            // Which color?
#if __JDOOM__
            if(!cfg.usePatchReplacement)
            {
                r = 1;
                g = b = 0;
            }
            else
            {
#endif
            if(currentMenu->items[i].type == ITT_EMPTY ||
                    currentMenu->items[i].type >= ITT_INERT)
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
            else if(itemOn == i && !WidgetEdit && cfg.usePatchReplacement)
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
#if __JDOOM__
            }
#endif
            if(currentMenu->items[i].lumpname)
            {
                if(currentMenu->items[i].lumpname[0])
                {
                    WI_DrawPatch(x, y, r, g, b, menu_alpha,
                                W_GetNumForName(currentMenu->items[i].lumpname),
                                (currentMenu->items[i].flags & MIF_NOTALTTXT)? NULL :
                                 currentMenu->items[i].text, true, ALIGN_LEFT);
                }
            }
            else if(currentMenu->items[i].text)
            {
                // changed from font[0].height to font[17].height
                // (in jHeretic/jHexen font[0] is 1 pixel high)
                WI_DrawParamText(x, y,
                                currentMenu->items[i].text, currentMenu->font,
                                r, g, b, menu_alpha,
                                currentMenu->font == hu_font_b,
                                cfg.usePatchReplacement? true : false,
                                ALIGN_LEFT);
            }
            y += currentMenu->itemHeight;
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
            if(itemOn >= 0)
            {
                Menu_t* mn = (WidgetEdit? &ColorWidgetMnu : currentMenu);

                scale = mn->itemHeight / (float) LINEHEIGHT;
                w = SHORT(cursorst[whichSkull].width) * scale; // skull size
                h = SHORT(cursorst[whichSkull].height) * scale;

                off_x = mn->x;
                off_x += (SKULLXOFF * scale);

                off_y = mn->y;
                off_y += (itemOn - mn->firstItem + 1) * mn->itemHeight;
                off_y -= h/2.f;

#ifndef __JDOOM__
                if(mn->itemHeight < LINEHEIGHT)
                {
                    // In Heretic and Hexen, the small font requires a slightly
                    // different offset.
                    off_y += SKULLYOFF;
                }
#endif

                GL_SetPatch(cursorst[whichSkull].lump);
                gl.MatrixMode(DGL_MODELVIEW);
                gl.PushMatrix();

                gl.Translatef(off_x, off_y, 0);
                gl.Scalef(1, 1.0f / 1.2f, 1);
                if(skull_angle)
                    gl.Rotatef(skull_angle, 0, 0, 1);
                gl.Scalef(1, 1.2f, 1);

                GL_DrawRect(-w/2.f, -h/2.f, w, h, 1, 1, 1, menu_alpha);

                gl.MatrixMode(DGL_MODELVIEW);
                gl.PopMatrix();
            }
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
 * Responds to alphanumeric input for edit fields.
 */
boolean M_EditResponder(event_t *ev)
{
    int     ch = -1;
    char   *ptr;

    if(!saveStringEnter && !ActiveEdit && !messageToPrint)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
        shiftdown = (ev->type == ev_keydown || ev->type == ev_keyrepeat);

    if(ev->type == ev_keydown || ev->type == ev_keyrepeat)
        ch = ev->data1;

    if(ch == -1)
        return false;

    // String input
    if(saveStringEnter || ActiveEdit)
    {
        ch = toupper(ch);
        if(!(ch != 32 && (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE)))
        {
            if(ch >= ' ' && ch <= 'Z')
            {
                if(shiftdown && shiftTable[ch - 32])
                    ch = shiftTable[ch - 32];

                if(saveStringEnter)
                {
                    if(saveCharIndex < SAVESTRINGSIZE - 1 &&
                        M_StringWidth(savegamestrings[saveSlot], hu_font_a)
                        < (SAVESTRINGSIZE - 2) * 8)
                    {
                        savegamestrings[saveSlot][saveCharIndex++] = ch;
                        savegamestrings[saveSlot][saveCharIndex] = 0;
                    }
                }
                else
                {
                    if(strlen(ActiveEdit->text) < MAX_EDIT_LEN - 2)
                    {
                        ptr = ActiveEdit->text + strlen(ActiveEdit->text);
                        ptr[0] = ch;
                        ptr[1] = 0;
                        Ed_MakeCursorVisible();
                    }
                }
            }
            return true;
        }
    }

    // Take a screenshot in dev mode
    // Still needed? Shouldn't be here at least...
    if(devparm && ch == DDKEY_F1)
    {
        G_ScreenShot();
        return true;
    }

    return false;
}

void M_EndAnyKeyMsg(void)
{
    M_StopMessage();
    M_ClearMenus();
    S_LocalSound(menusnds[1], NULL);
}

/*
 * This is the "fallback" responder, its the last stage in the event chain
 * so if an event reaches here it means there was no suitable binding for it.
 *
 * Handles the hotkey selection in the menu and "press any key" messages.
 * Returns true if it ate the event.
 */
boolean M_Responder(event_t *ev)
{
    int     ch = -1;
    int     i;
    int     cid;
    int     firstVI, lastVI;    // first and last visible item
    boolean skip;

    if(!menuactive || WidgetEdit || currentMenu->noHotKeys)
        return false;

    if(ev->type == ev_keydown || ev->type == ev_keyrepeat)
        ch = ev->data1;

    if(ch == -1)
        return false;

    // Handle "Press any key to continue" messages
    if(messageToPrint && !messageNeedsInput)
    {
        M_EndAnyKeyMsg();
        return true;
    }

    firstVI = currentMenu->firstItem;
    lastVI = firstVI + currentMenu->numVisItems - 1;

    if(lastVI > currentMenu->itemCount - 1)
        lastVI = currentMenu->itemCount - 1;
    currentMenu->lastOn = itemOn;

    // first letter of each item is treated as a hotkey
    for(i = firstVI; i <= lastVI; i++)
    {
        if(currentMenu->items[i].text && currentMenu->items[i].type != ITT_EMPTY)
        {
            cid = 0;
            if(currentMenu->items[i].text[0] == '{')
            {   // A parameter string, skip till '}' is found
                for(cid = 0, skip = true;
                     cid < strlen(currentMenu->items[i].text), skip; ++cid)
                    if(currentMenu->items[i].text[cid] == '}')
                        skip = false;
            }

            if(toupper(ch) == toupper(currentMenu->items[i].text[cid]))
            {
                itemOn = i;
                return (true);
            }
        }
    }
    return false;
}

/*
 * The colour widget edits the "hot" currentcolour[]
 * The widget responder handles setting the specified vars to that
 * of the currentcolour.
 *
 * boolean rgba is used to control if rgb or rgba input is needed,
 * as defined in the widgetcolors array.
 */
void DrawColorWidget()
{
    int    w = 0;
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
 * Inform the menu to activate the color widget
 * An intermediate step. Used to copy the existing rgba values pointed
 * to by the index (these match an index in the widgetcolors array) into
 * the "hot" currentcolor[] slots. Also switches between rgb/rgba input.
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
    char *cvarname = (char *) data;

    if(!data)
        return;

    DD_Executef(true, "toggle %s", cvarname);
    S_LocalSound(menusnds[0], NULL);
}

void M_DrawTitle(char *text, int y)
{
    WI_DrawParamText(160 - M_StringWidth(text, hu_font_b) / 2, y, text,
                     hu_font_b, cfg.menuColor[0], cfg.menuColor[1],
                     cfg.menuColor[2], menu_alpha, true, true, ALIGN_LEFT);
}

void M_WriteMenuText(const Menu_t * menu, int index, char *text)
{
    int     off = 0;

    if(menu->items[index].text)
        off = M_StringWidth(menu->items[index].text, menu->font) + 4;

    M_WriteText2(menu->x + off, menu->y + menu->itemHeight * (index  - menu->firstItem), text,
                 menu->font, 1, 1, 1, menu_alpha);
}

/*
 * User wants to load this game
 */
void M_LoadSelect(int option, void *data)
{
    Menu_t *menu = &SaveDef;

#if __JDOOM__ || __JHERETIC__
    char    name[256];

    SV_SaveGameFile(option, name);
    G_LoadGame(name);
#else
    G_LoadGame(option);
#endif

    menu->lastOn = option;
    mfAlpha = 0;
    menu_alpha = 0;
    menuactive = false;
    fadingOut = false;
    M_ClearMenus();
}

/*
 * User wants to save. Start string input for M_Responder
 */
void M_SaveSelect(int option, void *data)
{
    Menu_t *menu = &LoadDef;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    menu->lastOn = saveSlot = option;
    strcpy(saveOldString, savegamestrings[option]);
    if(!strcmp(savegamestrings[option], EMPTYSTRING))
        savegamestrings[option][0] = 0;
    saveCharIndex = strlen(savegamestrings[option]);
}

void M_StartMessage(char *string, void *routine, boolean input)
{
    messageResponse = 0;
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    typein_time = 0;

    // Enable the message binding class
    DD_SetBindClass(GBC_MESSAGE, true);
}

void M_StopMessage(void)
{
    menuactive = messageLastMenuActive;
    messageToPrint = 0;

    // Disable the menu binding class
    DD_SetBindClass(GBC_MESSAGE, false);
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

/*
 *  Draws a 'fancy' menu effect.
 *  looks a bit of a mess really (sorry)...
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
    WI_DrawPatch(88, 0, 1, 1, 1, menu_alpha, W_GetNumForName("M_HTIC"),
                 NULL, false, ALIGN_LEFT);

    gl.Color4f( 1, 1, 1, menu_alpha);
    GL_DrawPatch_CS(40, 10, SkullBaseLump + (17 - frame));
    GL_DrawPatch_CS(232, 10, SkullBaseLump + frame);
#elif __JDOOM__
    WI_DrawPatch(94, 2, 1, 1, 1, menu_alpha, W_GetNumForName("M_DOOM"),
                 NULL, false, ALIGN_LEFT);
#elif __JSTRIFE__
    Menu_t *menu = &MainDef;
    int    yoffset = 0;

    WI_DrawPatch(86, 2, 1, 1, 1, menu_alpha, W_GetNumForName("M_STRIFE"),
                 NULL, false, ALIGN_LEFT);

    WI_DrawPatch(menu->x, menu->y + yoffset, 1, 1, 1, menu_alpha,
                 W_GetNumForName("M_NGAME"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menu_alpha, W_GetNumForName("M_NGAME"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menu_alpha, W_GetNumForName("M_OPTION"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menu_alpha, W_GetNumForName("M_LOADG"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menu_alpha, W_GetNumForName("M_SAVEG"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menu_alpha, W_GetNumForName("M_RDTHIS"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menu_alpha, W_GetNumForName("M_QUITG"), NULL, false, ALIGN_LEFT);
#endif
}

#if __JHEXEN__
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

#if __JDOOM__ || __JHERETIC__
void M_DrawEpisode(void)
{
#ifdef __JDOOM__
    Menu_t *menu = &EpiDef;
#endif

#ifdef __JHERETIC__
    M_DrawTitle("WHICH EPISODE?", 4);
#elif __JDOOM__
    WI_DrawPatch(50, 40, menu->color[0], menu->color[1], menu->color[2], menu_alpha,
                 W_GetNumForName("M_EPISOD"), "{case}Which Episode{scaley=1.25,y=-3}?",
                 true, ALIGN_LEFT);
#endif
}
#endif

void M_DrawSkillMenu(void)
{
#if __JHEXEN__ || __JSTRIFE__
    M_DrawTitle("CHOOSE SKILL LEVEL:", 16);
#elif __JHERETIC__
    M_DrawTitle("SKILL LEVEL?", 4);
#elif __JDOOM__
    Menu_t *menu = &SkillDef;
    WI_DrawPatch(96, 14, menu->color[0], menu->color[1], menu->color[2], menu_alpha,
                 W_GetNumForName("M_NEWG"), "{case}NEW GAME", true, ALIGN_LEFT);
    WI_DrawPatch(54, 38, menu->color[0], menu->color[1], menu->color[2], menu_alpha,
                 W_GetNumForName("M_SKILL"), "{case}Choose Skill Level:", true,
                 ALIGN_LEFT);
#endif
}

void M_DrawFilesMenu(void)
{
    // clear out the quicksave/quickload stuff
    quicksave = 0;
    quickload = 0;
}

/*
 *  read the strings from the savegame files
 */
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
            LoadItems[i].type = ITT_INERT;
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
            LoadItems[i].type = ITT_INERT;
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

#ifdef __JDOOM__
#define SAVEGAME_BOX_YOFFSET 3
#else
#define SAVEGAME_BOX_YOFFSET 5
#endif

void M_DrawLoad(void)
{
    int     i;

    Menu_t *menu = &LoadDef;

#ifndef __JDOOM__
    M_DrawTitle("LOAD GAME", 4);
#else
    WI_DrawPatch(72, 28, menu->color[0], menu->color[1], menu->color[2], menu_alpha,
                 W_GetNumForName("M_LOADG"), "{case}LOAD GAME", true, ALIGN_LEFT);
#endif
    for(i = 0; i < load_end; i++)
    {
        M_DrawSaveLoadBorder(LoadDef.x, SAVEGAME_BOX_YOFFSET + LoadDef.y + (menu->itemHeight * i));
        M_WriteText2(LoadDef.x, SAVEGAME_BOX_YOFFSET + LoadDef.y + (menu->itemHeight * i),
                     savegamestrings[i], menu->font,
                     menu->color[0], menu->color[1], menu->color[2], menu_alpha);
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
    WI_DrawPatch(72, 28, menu->color[0], menu->color[1], menu->color[2], menu_alpha,
                 W_GetNumForName("M_SAVEG"), "{case}SAVE GAME", true, ALIGN_LEFT);
#endif
    for(i = 0; i < load_end; i++)
    {
        M_DrawSaveLoadBorder(SaveDef.x, SAVEGAME_BOX_YOFFSET + SaveDef.y + (menu->itemHeight * i));
        M_WriteText2(SaveDef.x, SAVEGAME_BOX_YOFFSET + SaveDef.y + (menu->itemHeight * i),
                     savegamestrings[i], menu->font,
                     menu->color[0], menu->color[1], menu->color[2], menu_alpha);
    }

    if(saveStringEnter)
    {
        i = M_StringWidth(savegamestrings[saveSlot], hu_font_a);
        M_WriteText2(SaveDef.x + i, SAVEGAME_BOX_YOFFSET + SaveDef.y + (menu->itemHeight * saveSlot),
                     "_", hu_font_a, menu->color[0], menu->color[1], menu->color[2], menu_alpha);
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

boolean M_QuickSaveResponse(int ch, void *data)
{
    if(messageResponse == 1) // Yes
    {
        M_DoSave(quickSaveSlot);
        S_LocalSound(menusnds[1], NULL);
        M_StopMessage();
        M_ClearMenus();
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        M_ClearMenus();
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
    return false;
}

void M_QuickSave(void)
{
    if(!usergame)
    {
        S_LocalSound(menusnds[6], NULL);
        return;
    }

    if(gamestate != GS_LEVEL)
        return;

    if(quickSaveSlot < 0)
    {
        M_StartControlPanel();
        M_ReadSaveStrings();
        M_SetupNextMenu(&SaveDef);
        quickSaveSlot = -2;        // means to pick a slot now
        return;
    }
    sprintf(tempstring, QSPROMPT, savegamestrings[quickSaveSlot]);

    if(!cfg.askQuickSaveLoad)
    {
        M_DoSave(quickSaveSlot);
        S_LocalSound(menusnds[1], NULL);
        return;
    }

    M_StartMessage(tempstring, M_QuickSaveResponse, true);
}

//
// M_QuickLoad
//
boolean M_QuickLoadResponse(int ch, void *data)
{
    if(messageResponse == 1) // Yes
    {
        M_LoadSelect(quickSaveSlot, NULL);
        S_LocalSound(menusnds[1], NULL);
        M_StopMessage();
        M_ClearMenus();
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        M_ClearMenus();
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
    return false;
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

    if(!cfg.askQuickSaveLoad)
    {
        M_LoadSelect(quickSaveSlot, NULL);
        S_LocalSound(menusnds[1], NULL);
        return;
    }

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
        WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("HELP"), NULL, false, ALIGN_LEFT);
        break;
    case shareware:
    case registered:
    case retail:
        WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("HELP1"), NULL, false, ALIGN_LEFT);
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
        WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("CREDIT"), NULL, false, ALIGN_LEFT);
        break;
    case shareware:
    case registered:
        WI_DrawPatch(0, 0, 1, 1, 1, 1, W_GetNumForName("HELP2"), NULL, false, ALIGN_LEFT);
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

void M_DrawOptions(void)
{
    //Menu_t *menu = &OptionsDef;
#ifndef __JDOOM__
    WI_DrawPatch(88, 0, 1, 1, 1, menu_alpha, W_GetNumForName("M_HTIC"),
                 NULL, false, ALIGN_LEFT);

    M_DrawTitle("OPTIONS", 56);
#else
    WI_DrawPatch(94, 2, 1, 1, 1, menu_alpha, W_GetNumForName("M_DOOM"),
                 NULL, false, ALIGN_LEFT);
    WI_DrawPatch(160, 64, cfg.menuColor[0], cfg.menuColor[1], cfg.menuColor[2],
                 menu_alpha, W_GetNumForName("M_OPTTTL"), "{case}OPTIONS",
                 true, ALIGN_CENTER);
#endif
}

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
    int idx = 0;
    Menu_t *menu = &GameplayDef;

#if __JHEXEN__
    M_DrawTitle("GAMEPLAY", 0);
    M_WriteMenuText(menu, idx++, yesno[cfg.alwaysRun != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.lookSpring != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.noAutoAim != 0]);
#else

#ifdef __JHERETIC__
    M_DrawTitle("GAMEPLAY", 4);
#else
    M_DrawTitle("GAMEPLAY", menu->y - 20);
#endif

    M_WriteMenuText(menu, idx++, yesno[cfg.alwaysRun != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.lookSpring != 0]);
    M_WriteMenuText(menu, idx++, yesno[!cfg.noAutoAim]);
    M_WriteMenuText(menu, idx++, yesno[cfg.jumpEnabled != 0]);

    idx = 6;
#ifdef __JDOOM__
    M_WriteMenuText(menu, idx++, yesno[cfg.anybossdeath != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.raiseghosts != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.maxskulls != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.allowskullsinwalls != 0]);
#endif
#if __JDOOM__ || __JHERETIC__
    M_WriteMenuText(menu, idx++, yesno[cfg.monstersStuckInDoors != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.avoidDropoffs != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.fallOff != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.slidingCorpses != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.moveBlock != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.wallRunNorthOnly != 0]);
#endif
#if __JDOOM__
    M_WriteMenuText(menu, idx++, yesno[cfg.zombiesCanExit != 0]);
#endif
#endif
}

void M_DrawWeaponMenu(void)
{
    Menu_t *menu = &WeaponDef;
    int i = 0;
    char   *autoswitch[] = { "NEVER", "IF BETTER", "ALWAYS" };
#if __JHEXEN__
    char   *weaponids[] = { "First", "Second", "Third", "Fourth"};
#endif

#if __JDOOM__
    byte berserkAutoSwitch = cfg.berserkAutoSwitch;
#endif

    M_DrawTitle("WEAPONS", menu->y - 20);

    for(i=0; i < NUMWEAPONS; i++)
    {
#ifdef __JDOOM__
        M_WriteMenuText(menu, 4+i, GET_TXT(TXT_WEAPON0 + cfg.weaponOrder[i]));
#elif __JHERETIC__
        // FIXME: We should allow different weapon preferences per player class.
        //        However, since the only other class in jHeretic is the chicken
        //        which has only 1 weapon anyway -we'll just show the names of the
        //        player's weapons for now.
        M_WriteMenuText(menu, 4+i, GET_TXT(TXT_TXT_WPNSTAFF + cfg.weaponOrder[i]));
#elif __JHEXEN__
        // FIXME: We should allow different weapon preferences per player class.
        //        Then we can show the real names here.
        M_WriteMenuText(menu, 4+i, weaponids[cfg.weaponOrder[i]]);
#endif
    }

#if __JHEXEN__
    M_WriteMenuText(menu, 8, yesno[cfg.weaponNextMode]);
    M_WriteMenuText(menu, 10, autoswitch[cfg.weaponAutoSwitch]);
#elif __JHERETIC__
    M_WriteMenuText(menu, 12, yesno[cfg.weaponNextMode]);
    M_WriteMenuText(menu, 14, autoswitch[cfg.weaponAutoSwitch]);
#elif __JDOOM__
    M_WriteMenuText(menu, 13, yesno[cfg.weaponNextMode]);
    M_WriteMenuText(menu, 15, autoswitch[cfg.weaponAutoSwitch]);

    M_WriteMenuText(menu, 16, yesno[berserkAutoSwitch != 0]);
#endif
}

void M_WeaponOrder(int option, void *data)
{
    //Menu_t *menu = &WeaponDef;
    int choice = option >> NUMWEAPONS;
    int temp;

    if(option & RIGHT_DIR)
    {
        if(choice < NUMWEAPONS-1)
        {
            temp = cfg.weaponOrder[choice+1];
            cfg.weaponOrder[choice+1] = cfg.weaponOrder[choice];
            cfg.weaponOrder[choice] = temp;

            itemOn++;
        }
    }
    else
    {
        if(choice > 0)
        {
            temp = cfg.weaponOrder[choice];
            cfg.weaponOrder[choice] = cfg.weaponOrder[choice-1];
            cfg.weaponOrder[choice-1] = temp;

            itemOn--;
        }
    }
}

void M_AutoSwitch(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.weaponAutoSwitch < 2)
            cfg.weaponAutoSwitch++;
    }
    else if(cfg.weaponAutoSwitch > 0)
        cfg.weaponAutoSwitch--;
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

void M_NewGame(int option, void *data)
{
    if(IS_NETGAME)                // && !Get(DD_PLAYBACK))
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
boolean M_QuitResponse(int option, void *data)
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

    if(messageResponse == 1)
    {
        // No need to close down the menu question after this.
#ifdef __JDOOM__
        // Play an exit sound if it is enabled.
        if(cfg.menuQuitSound && !IS_NETGAME)
        {
            if(!quitYet)
            {
                if(gamemode == commercial)
                    S_LocalSound(quitsounds2[(gametic >> 2) & 7], NULL);
                else
                    S_LocalSound(quitsounds[(gametic >> 2) & 7], NULL);

                // Wait for 1.5 seconds.
                DD_Executef(true, "after 53 quit!");
                quitYet = true;
            }
        }
        else
        {
            Sys_Quit();
        }
        return true;
#else
        Sys_Quit();
        return true;
#endif
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        M_ClearMenus();
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
    return false;
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
    sprintf(endstring, "%s\n\n%s",
            endmsg[(gametic % (NUM_QUITMESSAGES + 1))], DOSY);
#else
    sprintf(endstring, "%s\n\n%s", endmsg[0], DOSY);
#endif

    quitAsk = 1;
    M_StartMessage(endstring, M_QuitResponse, true);
}

//
// M_EndGame
//
boolean M_EndGameResponse(int option, void *data)
{
    if(messageResponse == 1)
    {
        currentMenu->lastOn = itemOn;
        mfAlpha = 0;
        menu_alpha = 0;
        fadingOut = false;
        menuactive = false;
        M_StopMessage();
        M_ClearMenus();
        G_StartTitle();
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        M_ClearMenus();
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
    return false;
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
        S_LocalSound(menusnds[6], NULL);
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

    M_SetupNextMenu(&SaveDef);;
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

boolean M_VerifyNightmare(int option, void *data)
{
#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
    if(messageResponse == 1) // Yes
    {
#ifdef __JHERETIC__
        G_DeferedInitNew(sk_nightmare, MenuEpisode, 1);
#elif __JDOOM__
        G_DeferedInitNew(sk_nightmare, epi + 1, 1);
#elif __JSTRIFE__
        G_DeferredNewGame(sk_nightmare);
#endif
        M_StopMessage();
        M_ClearMenus();
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        M_ClearMenus();
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
#endif
    return false;
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
    DD_Execute(option ? "panel audio" : "panel", true);
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
    R_SetViewSize(cfg.screenblocks, 0);    //detailLevel);
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
    quitAsk = 0;

    // Enable the menu binding class
    DD_SetBindClass(GBC_CLASS3, true);
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

    // Disable the menu binding class
    DD_SetBindClass(GBC_CLASS3, false);
}

//
// M_SetupNextMenu
//
void M_SetupNextMenu(Menu_t * menudef)
{
    int i;

    if(!menudef)
        return;
    currentMenu = menudef;

    // Have we been to this menu before?
    // If so move the cursor to the last selected item
    if(currentMenu->lastOn)
        itemOn = currentMenu->lastOn;
    else
    {
        // Select the first active item in this menu
        for(i=0; ;i++)
        {
            if(menudef->items[i].type == ITT_EMPTY)
                continue;
            else
                break;
        }

        if(i > menudef->itemCount)
            itemOn = -1;
        else
            itemOn = i;
    }

    menu_color = 0;
    skull_angle = 0;
    typein_time = 0;
}

/*
 * Draws a little colour box using the background box for a border
 */
void M_DrawColorBox(const Menu_t * menu, int index, float r, float g, float b, float a)
{
    int x = menu->x + 4;
    int y = menu->y + menu->itemHeight * (index  - menu->firstItem) + 3;

    if (a < 0) a = 1;

    M_DrawBackgroundBox(x, y, 2, 1, 1, 1, 1, 1, false, 1);
    GL_SetNoTexture();
    GL_DrawRect(x-1,y-1, 4, 3, r, g, b, a);
}

/*
 *  Draws a box using the border patches.
 *        Border is drawn outside.
 */
void M_DrawBackgroundBox(int x, int y, int w, int h, float red, float green,
                         float blue, float alpha, boolean background, int border)
{
    dpatch_t    *t = 0, *b = 0, *l = 0, *r = 0, *tl = 0, *tr = 0, *br = 0, *bl = 0;
    int         up = -1;

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

/*
 * Draws a menu slider control
 */
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
    int     x =0, y =0, xx;
    int height = menu->itemHeight - 1;
    float   scale = height / 13.0f;

    if(menu->items[item].text)
        x = M_StringWidth(menu->items[item].text, menu->font);

    x += menu->x + 6;
    y = menu->y + menu->itemHeight * item;

    xx = x;
    GL_SetPatch(W_GetNumForName("M_THERML"));
    GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, menu_alpha);
    xx += 6 * scale;
    GL_SetPatch(W_GetNumForName("M_THERM2"));    // in jdoom.wad
    GL_DrawRectTiled(xx, y, 8 * width * scale, height, 8 * scale, height);
    xx += 8 * width * scale;
    GL_SetPatch(W_GetNumForName("M_THERMR"));
    GL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, menu_alpha);
    GL_SetPatch(W_GetNumForName("M_THERMO"));
    GL_DrawRect(x + (6 + slot * 8) * scale, y, 6 * scale, height, 1, 1, 1,
                menu_alpha);
#endif
}

/*
 * handles response to messages requiring input
 */
DEFCC(CCmdMsgResponse)
{
    if(messageToPrint)
    {
        // Handle "Press any key to continue" messages
        if(!messageNeedsInput)
        {
            M_EndAnyKeyMsg();
            return true;
        }
        else
        {
            if(!stricmp(argv[0], "messageyes"))
            {
                messageResponse = 1;
                return true;
            }
            else if(!stricmp(argv[0], "messageno"))
            {
                messageResponse = -1;
                return true;
            }
            else if(!stricmp(argv[0], "messagecancel"))
            {
                messageResponse = -2;
                return true;
            }
        }
    }

    return false;
}

/*
 * handles menu commands, actions and navigation
 */
DEFCC(CCmdMenuAction)
{
    int mode = 0, i;

    if(!menuactive)
    {
        if(!stricmp(argv[0], "menu") && !chat_on)   // Open menu
        {
            M_StartControlPanel();
            S_LocalSound(menusnds[2], NULL);
            return true;
        }
    }
    else
    {
        int    firstVI, lastVI; // first and last visible item
        int    itemCountOffset = 0;
        const MenuItem_t *item;
        Menu_t *menu = currentMenu;

        // Determine what state the menu is in currently
        if(ActiveEdit)
            mode = 1;
        else if(WidgetEdit)
        {
            mode = 2;
            menu = &ColorWidgetMnu;

            if(!rgba)
                itemCountOffset = 1;

        }
        else if(saveStringEnter)
            mode = 3;
        else if(inhelpscreens)
            mode = 4;

        firstVI = menu->firstItem;
        lastVI = firstVI + menu->numVisItems - 1 - itemCountOffset;
        if(lastVI > menu->itemCount - 1 - itemCountOffset)
            lastVI = menu->itemCount - 1 - itemCountOffset;
        item = &menu->items[itemOn];
        menu->lastOn = itemOn;

        if(!stricmp(argv[0], "menuup"))
        {
            // If a message is being displayed, menuup does nothing
            if(messageToPrint)
                return true;

            switch (mode)
            {
            case 2: // Widget edit
                if(!WidgetEdit)
                    break;
            case 0: // Menu nav
                i = 0;
                do
                {
                    if(itemOn <= firstVI)
                        itemOn = lastVI;
                    else
                        itemOn--;
                } while(menu->items[itemOn].type == ITT_EMPTY &&
                        i++ < menu->itemCount);
                menu_color = 0;
                S_LocalSound(menusnds[3], NULL);
                break;

            default:
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menudown"))
        {
            // If a message is being displayed, menudown does nothing
            if(messageToPrint)
                return true;

            switch (mode)
            {
            case 2: // Widget edit
                if(!WidgetEdit)
                    break;
            case 0: // Menu nav
                i = 0;
                do
                {
                    if(itemOn + 1 > lastVI)
                        itemOn = firstVI;
                    else
                        itemOn++;
                } while(menu->items[itemOn].type == ITT_EMPTY &&
                        i++ < menu->itemCount);
                menu_color = 0;
                S_LocalSound(menusnds[3], NULL);
                break;

            default:
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menuleft"))
        {
            // If a message is being displayed, menuleft does nothing
            if(messageToPrint)
                return true;

            switch (mode)
            {
            case 0: // Menu nav
            case 2: // Widget edit
                if(item->type == ITT_LRFUNC && item->func != NULL)
                {
                    item->func(LEFT_DIR | item->option, item->data);
                    S_LocalSound(menusnds[4], NULL);
                }
                else
                {
                    if(mode == 2)
                        break;

                    menu_nav_left:

                    // Let's try to change to the previous page.
                    if(menu->firstItem - menu->numVisItems >= 0)
                    {
                        menu->firstItem -= menu->numVisItems;
                        itemOn -= menu->numVisItems;

                        //Ensure cursor points to an editable item
                        firstVI = menu->firstItem;
                        while(menu->items[itemOn].type == ITT_EMPTY &&
                              (itemOn > firstVI))
                            itemOn--;

                        while(!menu->items[itemOn].type &&
                             itemOn < menu->numVisItems)
                            itemOn++;

                        // Make a sound, too.
                        S_LocalSound(menusnds[4], NULL);
                    }
                }
                break;

            default:
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menuright"))
        {
            // If a message is being displayed, menuright does nothing
            if(messageToPrint)
                return true;

            switch (mode)
            {
            case 0: // Menu nav
            case 2: // Widget edit
                if(item->type == ITT_LRFUNC && item->func != NULL)
                {
                    item->func(RIGHT_DIR | item->option, item->data);
                    S_LocalSound(menusnds[4], NULL);
                }
                else
                {
                    if(mode == 2)
                        break;

        menu_nav_right:

                    // Move on to the next page, if possible.
                    if(menu->firstItem + menu->numVisItems <
                       menu->itemCount)
                    {
                        menu->firstItem += menu->numVisItems;
                        itemOn += menu->numVisItems;

                        //Ensure cursor points to an editable item
                        firstVI = menu->firstItem;
                        while((menu->items[itemOn].type == ITT_EMPTY ||
                               itemOn >= menu->itemCount) && itemOn > firstVI)
                            itemOn--;

                        while(menu->items[itemOn].type == ITT_EMPTY &&
                              itemOn < menu->numVisItems)
                            itemOn++;

                        // Make a sound, too.
                        S_LocalSound(menusnds[4], NULL);
                    }
                }
                break;

            default:
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menuselect"))
        {
            // If a message is being displayed, menuselect does nothing
            if(messageToPrint)
                return true;

            switch (mode)
            {
            case 4: // In helpscreens
            case 0: // Menu nav
                if(item->type == ITT_SETMENU)
                {
                    M_SetupNextMenu(menulist[item->option]);
                    S_LocalSound(menusnds[5], NULL);
                }
                else if(item->type == ITT_NAVLEFT)
                {
                    goto menu_nav_left;
                }
                else if(item->type == ITT_NAVRIGHT)
                {
                    goto menu_nav_right;
                }
                else if(item->func != NULL)
                {
                    menu->lastOn = itemOn;
                    if(item->type == ITT_LRFUNC)
                    {
                        item->func(RIGHT_DIR | item->option, item->data);
                        S_LocalSound(menusnds[5], NULL);
                    }
                    else if(item->type == ITT_EFUNC)
                    {
                        item->func(item->option, item->data);
                        S_LocalSound(menusnds[5], NULL);
                    }
                }
                break;

            case 1: // Edit Field
                ActiveEdit->firstVisible = 0;
                ActiveEdit = NULL;
                S_LocalSound(menusnds[0], NULL);
                break;

            case 2: // Widget edit
                // Set the new color
                *widgetcolors[editcolorindex].r = currentcolor[0];
                *widgetcolors[editcolorindex].g = currentcolor[1];
                *widgetcolors[editcolorindex].b = currentcolor[2];

                if(rgba)
                    *widgetcolors[editcolorindex].a = currentcolor[3];

                // Restore the position of the skull
                itemOn = previtemOn;

                WidgetEdit = false;
                S_LocalSound(menusnds[0], NULL);
                break;

            case 3: // Save string edit: Save
                saveStringEnter = 0;
                if(savegamestrings[saveSlot][0])
                    M_DoSave(saveSlot);
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menucancel"))
        {
            int     c;
            // If a message is being displayed, menucancel does nothing
            if(messageToPrint)
                return true;

            switch (mode)
            {
            case 0: // Menu nav: Previous menu
                menu->lastOn = itemOn;
                if(menu->prevMenu == MENU_NONE)
                {
                    menu->lastOn = itemOn;
                    S_LocalSound(menusnds[1], NULL);
                    M_ClearMenus();
                }
                else
                {
                    M_SetupNextMenu(menulist[menu->prevMenu]);
                    S_LocalSound(menusnds[2], NULL);
                }
                break;

            case 1: // Edit Field: Del char
                c = strlen(ActiveEdit->text);
                if(c > 0)
                    ActiveEdit->text[c - 1] = 0;
                Ed_MakeCursorVisible();
                break;

            case 2: // Widget edit: Close widget
                // Restore the position of the skull
                itemOn = previtemOn;
                WidgetEdit = false;
                break;

            case 3: // Save string edit: Del char
                if(saveCharIndex > 0)
                {
                    saveCharIndex--;
                    savegamestrings[saveSlot][saveCharIndex] = 0;
                }
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menu"))
        {
            switch (mode)
            {
            case 0: // Menu nav: Close menu
                if(messageToPrint)
                    M_StopMessage();

                currentMenu->lastOn = itemOn;
                M_ClearMenus();
                S_LocalSound(menusnds[1], NULL);
                break;

            case 1: // Edit Field
                ActiveEdit->firstVisible = 0;
                strcpy(ActiveEdit->text, ActiveEdit->oldtext);
                ActiveEdit = NULL;
                break;

            case 2: // Widget edit: Close widget
                // Restore the position of the skull
                itemOn = previtemOn;
                WidgetEdit = false;
                break;

            case 3: // Save string edit: Cancel
                saveStringEnter = 0;
                strcpy(&savegamestrings[saveSlot][0], saveOldString);
                break;

            case 4: // In helpscreens: Exit and close menu
                M_SetupNextMenu(&MainDef);
                M_ClearMenus();
                break;
            }
            return true;
        }
    }

    // If a message is being displayed, hotkeys don't work
    if(messageToPrint)
        return true;

    // Hotkey menu shortcuts
    if(!stricmp(argv[0], "helpscreen"))    // F1
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
        S_LocalSound(menusnds[2], NULL);
    }
    else if(!stricmp(argv[0], "SaveGame"))    // F2
    {
        M_StartControlPanel();
        MenuTime = 0;
        S_LocalSound(menusnds[2], NULL);
        M_SaveGame(0, NULL);
    }
    else if(!stricmp(argv[0], "LoadGame"))
    {
        M_StartControlPanel();
        MenuTime = 0;
        S_LocalSound(menusnds[2], NULL);
        M_LoadGame(0, NULL);
    }
    else if(!stricmp(argv[0], "SoundMenu"))    // F4
    {
        M_StartControlPanel();
        MenuTime = 0;
        currentMenu = &Options2Def;
        itemOn = 0;                // sfx_vol
        S_LocalSound(menusnds[2], NULL);
    }
    else if(!stricmp(argv[0], "QuickSave"))    // F6
    {
        S_LocalSound(menusnds[2], NULL);
        MenuTime = 0;
        M_QuickSave();
    }
    else if(!stricmp(argv[0], "EndGame"))    // F7
    {
        S_LocalSound(menusnds[2], NULL);
        MenuTime = 0;
        M_EndGame(0, NULL);
    }
    else if(!stricmp(argv[0], "ToggleMsgs"))    // F8
    {
        MenuTime = 0;
        M_ChangeMessages(0, NULL);
        S_LocalSound(menusnds[2], NULL);
    }
    else if(!stricmp(argv[0], "QuickLoad"))    // F9
    {
        S_LocalSound(menusnds[2], NULL);
        MenuTime = 0;
        M_QuickLoad();
    }
    else if(!stricmp(argv[0], "quit"))    // F10
    {
        if(IS_DEDICATED)
            DD_Execute("quit!", true);
        else
        {
            S_LocalSound(menusnds[2], NULL);
            MenuTime = 0;
            M_QuitDOOM(0, NULL);
        }
    }
    else if(!stricmp(argv[0], "ToggleGamma"))    // F11
    {
        char    buf[50];

        usegamma++;
        if(usegamma > 4)
            usegamma = 0;
#ifdef __JDOOM__
        P_SetMessage(players + consoleplayer, gammamsg[usegamma]);
#endif
        sprintf(buf, "setgamma %i", usegamma);
        DD_Execute(buf, false);
    }

    return true;
}
