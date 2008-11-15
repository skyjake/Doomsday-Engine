/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * hu_menu.c: Common selection menu, options, episode etc.
 *            Sliders and icons. Kinda widget stuff.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "m_argv.h"
#include "hu_stuff.h"
#include "f_infine.h"
#include "am_map.h"
#include "x_hair.h"
#include "p_player.h"
#include "g_controls.h"
#include "p_saveg.h"
#include "g_common.h"
#include "hu_menu.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

#define SAVESTRINGSIZE      24

// QuitDOOM messages
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
#define NUM_QUITMESSAGES  0
#endif

static const float MENUFOGSPEED[2] = {.05f, -.085f};

// TYPES -------------------------------------------------------------------

typedef struct rgba_s {
    float          *r, *g, *b, *a;
} rgba_t;

typedef struct menufoglayer_s {
    float           texOffset[2];
    float           texAngle;
    float           posAngle;
} menufoglayer_t;

typedef struct menufogdata_s {
    DGLuint         texture;
    menufoglayer_t  layers[2];
    float           alpha, targetAlpha;
    float           joinY;
    boolean         scrollDir;
} menufogdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void Ed_MakeCursorVisible(void);
void M_InitControlsMenu(void);
void M_ControlGrabDrawer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void M_NewGame(int option, void* data);
void M_Episode(int option, void* data); // Does nothing in jHEXEN
void M_ChooseClass(int option, void* data); // Does something only in jHEXEN
void M_ChooseSkill(int option, void* data);
void M_LoadGame(int option, void* data);
void M_SaveGame(int option, void* data);
void M_GameFiles(int option, void* data); // Does nothing in jDOOM
void M_EndGame(int option, void* data);
#if !__JDOOM64__
void M_ReadThis(int option, void* data);
void M_ReadThis2(int option, void* data);

# if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
void M_ReadThis3(int option, void* data);
# endif
#endif

void M_QuitDOOM(int option, void* data);

void M_ToggleVar(int option, void* data);

void M_OpenDCP(int option, void* data);
void M_ChangeMessages(int option, void* data);
void M_WeaponAutoSwitch(int option, void* data);
void M_AmmoAutoSwitch(int option, void* data);
void M_HUDInfo(int option, void* data);
void M_HUDScale(int option, void* data);
void M_SfxVol(int option, void* data);
void M_WeaponOrder(int option, void* data);
void M_MusicVol(int option, void* data);
void M_SizeDisplay(int option, void* data);
#if !__JDOOM64__
void M_SizeStatusBar(int option, void* data);
void M_StatusBarAlpha(int option, void* data);
#endif
void M_HUDRed(int option, void* data);
void M_HUDGreen(int option, void* data);
void M_HUDBlue(int option, void* data);
void M_FinishReadThis(int option, void* data);
void M_LoadSelect(int option, void* data);
void M_SaveSelect(int option, void* data);
void M_Xhair(int option, void* data);
void M_XhairSize(int option, void* data);

#if __JDOOM__ || __JDOOM64__
void M_XhairR(int option, void* data);
void M_XhairG(int option, void* data);
void M_XhairB(int option, void* data);
void M_XhairAlpha(int option, void* data);
#endif

void M_ReadSaveStrings(void);
void M_DoSave(int slot);
void M_DoLoad(int slot);
static void M_QuickSave(void);
static void M_QuickLoad(void);

#if __JDOOM64__
void M_WeaponRecoil(int option, void* data);
#endif

void M_DrawMainMenu(void);
void M_DrawNewGameMenu(void);
void M_DrawReadThis(void);
void M_DrawSkillMenu(void);
void M_DrawClassMenu(void); // Does something only in jHEXEN
void M_DrawEpisode(void); // Does nothing in jHEXEN
void M_DrawOptions(void);
void M_DrawOptions2(void);
void M_DrawGameplay(void);
void M_DrawHUDMenu(void);
void M_DrawMapMenu(void);
void M_DrawWeaponMenu(void);
void M_DrawLoad(void);
void M_DrawSave(void);
void M_DrawFilesMenu(void);
void M_DrawBackgroundBox(int x, int y, int w, int h,
                         float r, float g, float b, float a,
                         boolean background, int border);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern editfield_t* ActiveEdit;
extern char* weaponNames[];
extern menu_t ControlsDef;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
char* endmsg[] = {
    "ARE YOU SURE YOU WANT TO QUIT?",
    "ARE YOU SURE YOU WANT TO END THE GAME?",
    "DO YOU WANT TO QUICKSAVE THE GAME NAMED",
    "DO YOU WANT TO QUICKLOAD THE GAME NAMED"
};
#endif

#if __JDOOM__ || __JDOOM64__
/// The end message strings will be initialized in Hu_MenuInit().
char* endmsg[NUM_QUITMESSAGES + 1];
#endif

const char* QuitEndMsg[] =
{
    "ARE YOU SURE YOU WANT TO QUIT?",
    "ARE YOU SURE YOU WANT TO END THE GAME?",
    "DO YOU WANT TO QUICKSAVE THE GAME NAMED",
    "DO YOU WANT TO QUICKLOAD THE GAME NAMED",
    "ARE YOU SURE YOU WANT TO SUICIDE?",
    NULL
};

menu_t* currentMenu;

#if __JHERETIC__
static int MenuEpisode;
#endif

// -1 = no quicksave slot picked!
int quickSaveSlot;

 // 1 = message to be printed.
int messageToPrint;

// ...and here is the message string!
char* messageString;

int messageResponse;
int messageLastMenuActive;

// Timed message = no input from user.
boolean messageNeedsInput;

boolean  (*messageRoutine) (int response, void* data);

char tempstring[80];

// Old save description before edit.
char saveOldString[SAVESTRINGSIZE+1];

char savegamestrings[10][SAVESTRINGSIZE+1];

// We are going to be entering a savegame string.
int saveStringEnter;
int saveSlot; // Which slot to save in.
int saveCharIndex; // Which char we're editing.

char endstring[160];

#if __JDOOM__ || __JDOOM64__
static char* yesno[3] = {"NO", "YES", "MAYBE?"};
#else
static char* yesno[2] = {"NO", "YES"};
#endif

#if __JDOOM__ || __JHERETIC__
char* episodemsg;
int epi;
#endif

static char shiftTable[59] = // Contains characters 32 to 90.
{
    /* 32 */ 0, 0, 0, 0, 0, 0, 0, '"',
    /* 40 */ 0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
    /* 50 */ '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
    /* 60 */ 0, '+', 0, 0, 0, 0, 0, 0, 0, 0,
    /* 70 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 80 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 90 */ 0
};

int menu_color = 0;
float skull_angle = 0;

int frame; // Used by any graphic animations that need to be pumped.
int menuTime;

short itemOn; // Menu item skull is on.
short previtemOn; // Menu item skull was last on (for restoring when leaving widget control).
short skullAnimCounter; // Skull animation counter.
short whichSkull; // Which skull to draw.

// Sounds played in the menu.
int menusnds[] = {
#if __WOLFTC__
    SFX_MENUBC,            // close menu
    SFX_MENUBC,            // open menu
    SFX_MENUBC,            // cancel
    SFX_MENUMV,            // up/down
    SFX_MENUMV,            // left/right
    SFX_MENUSL,            // accept
    SFX_MENUMV             // bad sound (eg can't autosave)
#elif __JDOOM__ || __JDOOM64__
    SFX_DORCLS,            // close menu
    SFX_SWTCHX,            // open menu
    SFX_SWTCHN,            // cancel
    SFX_PSTOP,             // up/down
    SFX_STNMOV,            // left/right
    SFX_PISTOL,            // accept
    SFX_OOF                // bad sound (eg can't autosave)
#elif __JHERETIC__
    SFX_SWITCH,
    SFX_CHAT,
    SFX_SWITCH,
    SFX_SWITCH,
    SFX_STNMOV,
    SFX_CHAT,
    SFX_CHAT
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

static boolean menuActive;

static float menuAlpha = 0; // Alpha level for the entire menu.
static float menuTargetAlpha = 0; // Target alpha for the entire UI.

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
static int SkullBaseLump;
#endif

static int cursors = NUMCURSORS;
dpatch_t cursorst[NUMCURSORS];

#if __JHEXEN__
static int MenuPClass;
#endif

static rgba_t widgetcolors[] = { // Ptrs to colors editable with the colour widget
    { &cfg.automapL0[0], &cfg.automapL0[1], &cfg.automapL0[2], NULL },
    { &cfg.automapL1[0], &cfg.automapL1[1], &cfg.automapL1[2], NULL },
    { &cfg.automapL2[0], &cfg.automapL2[1], &cfg.automapL2[2], NULL },
    { &cfg.automapL3[0], &cfg.automapL3[1], &cfg.automapL3[2], NULL },
    { &cfg.automapBack[0], &cfg.automapBack[1], &cfg.automapBack[2], &cfg.automapBack[3] },
    { &cfg.hudColor[0], &cfg.hudColor[1], &cfg.hudColor[2], &cfg.hudColor[3] }
};

static boolean widgetEdit = false; // No active widget by default.
static boolean rgba = false; // Used to swap between rgb / rgba modes for the color widget.

static int editcolorindex = 0; // The index of the widgetcolors array of the item being currently edited.

static float currentcolor[4] = {0, 0, 0, 0}; // Used by the widget as temporay values.

static menufogdata_t menuFogData;

static float outFade = 0;
static boolean fadingOut = false;
static int menuDarkTicks = 15;
static int quitAsk = 0; // Set ON so menu fog is rendered with the quit message.
#if !defined( __JHEXEN__ ) && !defined( __JHERETIC__ )
static int quitYet = 0; // Prevents multiple quit responses.
#endif
static int slamInTicks = 9;

// Used to fade out the background a little when a widget is active.
static float menu_calpha = 0;

static int quicksave;
static int quickload;

#if __JDOOM__
#define READTHISID          (4)
#else
#define READTHISID          (2)
#endif

#if __JDOOM__ || __JDOOM64__
static dpatch_t m_doom;
static dpatch_t m_newg;
static dpatch_t m_skill;
static dpatch_t m_episod;
static dpatch_t m_ngame;
static dpatch_t m_option;
static dpatch_t m_loadg;
static dpatch_t m_saveg;
static dpatch_t m_rdthis;
static dpatch_t m_quitg;
static dpatch_t m_optttl;
static dpatch_t dpLSLeft;
static dpatch_t dpLSRight;
static dpatch_t dpLSCntr;
# if __JDOOM__
static dpatch_t credit;
static dpatch_t help;
static dpatch_t help1;
static dpatch_t help2;
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
static dpatch_t m_htic;
static dpatch_t dpFSlot;
#endif

menuitem_t MainItems[] = {
#if __JDOOM__
    {ITT_SETMENU, 0, "{case}New Game", NULL, MENU_NEWGAME, &m_ngame},
    {ITT_SETMENU, 0, "{case}Options", NULL, MENU_OPTIONS, &m_option},
    {ITT_EFUNC, 0, "{case}Load Game", M_LoadGame, 0, &m_loadg},
    {ITT_EFUNC, 0, "{case}Save Game", M_SaveGame, 0, &m_saveg},
    {ITT_EFUNC, 0, "{case}Read This!", M_ReadThis, 0, &m_rdthis},
    {ITT_EFUNC, 0, "{case}Quit Game", M_QuitDOOM, 0, &m_quitg}
#elif __JDOOM64__
    {ITT_SETMENU, 0, "{case}New Game", NULL, MENU_NEWGAME},
    {ITT_SETMENU, 0, "{case}Options", NULL, MENU_OPTIONS},
    {ITT_EFUNC, 0, "{case}Load Game", M_LoadGame, 0},
    {ITT_EFUNC, 0, "{case}Save Game", M_SaveGame, 0},
    {ITT_EFUNC, 0, "{case}Quit Game", M_QuitDOOM, 0}
#elif __JSTRIFE__
    {ITT_SETMENU, 0, "N", NULL, MENU_NEWGAME},
    {ITT_SETMENU, 0, "O", NULL, MENU_OPTIONS},
    {ITT_EFUNC, 0, "L", M_LoadGame, 0},
    {ITT_EFUNC, 0, "S", M_SaveGame, 0},
    {ITT_EFUNC, 0, "R", M_ReadThis, 0},
    {ITT_EFUNC, 0, "Q", M_QuitDOOM, 0}
#else
    {ITT_SETMENU, 0, "new game", NULL, MENU_NEWGAME},
    {ITT_SETMENU, 0, "options", NULL, MENU_OPTIONS},
    {ITT_SETMENU, 0, "game files", NULL, MENU_FILES},
    {ITT_EFUNC, 0, "info", M_ReadThis, 0},
    {ITT_EFUNC, 0, "quit game", M_QuitDOOM, 0}
#endif
};

menu_t MainDef = {
#if __JHEXEN__
    0,
    110, 50,
    M_DrawMainMenu,
    5, MainItems,
    0, MENU_NONE,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B,
    0, 5
#elif __JHERETIC__
    0,
    110, 64,
    M_DrawMainMenu,
    5, MainItems,
    0, MENU_NONE,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B,
    0, 5
#elif __JSTRIFE__
    0,
    97, 64,
    M_DrawMainMenu,
    6, MainItems,
    0, MENU_NONE,
    huFontA,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B + 1,
    0, 6
#elif __JDOOM64__
    0,
    97, 64,
    M_DrawMainMenu,
    5, MainItems,
    0, MENU_NONE,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B + 1,
    0, 5
#else
    0,
    97, 64,
    M_DrawMainMenu,
    6, MainItems,
    0, MENU_NONE,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B + 1,
    0, 6
#endif
};

menuitem_t NewGameItems[] = {
#if __JDOOM__
    {ITT_EFUNC, 0, "{case}Singleplayer", M_NewGame, 0},
    {ITT_EFUNC, 0, "{case}Multiplayer", SCEnterMultiplayerMenu, 0}
#elif __JDOOM64__
    {ITT_EFUNC, 0, "{case}Singleplayer", M_NewGame, 0},
    {ITT_EFUNC, 0, "{case}Multiplayer", SCEnterMultiplayerMenu, 0}
#elif __JSTRIFE__
    {ITT_EFUNC, 0, "S", M_NewGame, 0},
    {ITT_EFUNC, 0, "M", SCEnterMultiplayerMenu, 0, ""}
#else
    {ITT_EFUNC, 0, "Singleplayer", M_NewGame, 0},
    {ITT_EFUNC, 0, "Multiplayer", SCEnterMultiplayerMenu, 0}
#endif
};

menu_t NewGameDef = {
#if __JHEXEN__
    0,
    110, 50,
    M_DrawNewGameMenu,
    2, NewGameItems,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B,
    0, 2
#elif __JHERETIC__
    0,
    110, 64,
    M_DrawNewGameMenu,
    2, NewGameItems,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B,
    0, 2
#elif __JSTRIFE__
    0,
    97, 64,
    M_DrawNewGameMenu,
    2, NewGameItems,
    0, MENU_MAIN,
    huFontA,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B + 1,
    0, 2
#elif __JDOOM64__
    0,
    97, 64,
    M_DrawNewGameMenu,
    2, NewGameItems,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B + 1,
    0, 2
#else
    0,
    97, 64,
    M_DrawNewGameMenu,
    2, NewGameItems,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B + 1,
    0, 2
#endif
};

#if __JHEXEN__
menuitem_t ClassItems[] = {
    {ITT_EFUNC, 0, "FIGHTER", M_ChooseClass, 0},
    {ITT_EFUNC, 0, "CLERIC", M_ChooseClass, 1},
    {ITT_EFUNC, 0, "MAGE", M_ChooseClass, 2}
};

menu_t ClassDef = {
    0,
    66, 66,
    M_DrawClassMenu,
    3, ClassItems,
    0, MENU_NEWGAME,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT_B + 1,
    0, 3
};
#endif

#if __JHERETIC__
menuitem_t EpisodeItems[] = {
    {ITT_EFUNC, 0, "city of the damned", M_Episode, 1},
    {ITT_EFUNC, 0, "hell's maw", M_Episode, 2},
    {ITT_EFUNC, 0, "the dome of d'sparil", M_Episode, 3},
    {ITT_EFUNC, 0, "the ossuary", M_Episode, 4},
    {ITT_EFUNC, 0, "the stagnant demesne", M_Episode, 5}
};

menu_t EpiDef = {
    0,
    48, 50,
    M_DrawEpisode,
    3, EpisodeItems,
    0, MENU_NEWGAME,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT + 1,
    0, 3
};
#elif __JDOOM__
menuitem_t EpisodeItems[] = {
    // Text defs TXT_EPISODE1..4.
    {ITT_EFUNC, 0, "K", M_Episode, 0},
    {ITT_EFUNC, 0, "T", M_Episode, 1},
    {ITT_EFUNC, 0, "I", M_Episode, 2},
    {ITT_EFUNC, 0, "T", M_Episode, 3}
};

menu_t EpiDef = {
    0,
    48, 63,
    M_DrawEpisode,
    4, EpisodeItems,
    0, MENU_NEWGAME,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT + 1,
    0, 4
};
#endif


#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
static menuitem_t FilesItems[] = {
    {ITT_EFUNC, 0, "load game", M_LoadGame, 0},
    {ITT_EFUNC, 0, "save game", M_SaveGame, 0}
};

static menu_t FilesMenu = {
    0,
    110, 60,
    M_DrawFilesMenu,
    2, FilesItems,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT + 1,
    0, 2
};
#endif

static menuitem_t LoadItems[] = {
    {ITT_EFUNC, 0, "1", M_LoadSelect, 0},
    {ITT_EFUNC, 0, "2", M_LoadSelect, 1},
    {ITT_EFUNC, 0, "3", M_LoadSelect, 2},
    {ITT_EFUNC, 0, "4", M_LoadSelect, 3},
    {ITT_EFUNC, 0, "5", M_LoadSelect, 4},
    {ITT_EFUNC, 0, "6", M_LoadSelect, 5},
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {ITT_EFUNC, 0, "7", M_LoadSelect, 6},
    {ITT_EFUNC, 0, "8", M_LoadSelect, 7}
#endif
};

static menu_t LoadDef = {
    0,
#if __JDOOM__ || __JDOOM64__
    80, 44,
#else
    80, 30,
#endif
    M_DrawLoad,
    NUMSAVESLOTS, LoadItems,
    0, MENU_MAIN,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A + 8,
    0, NUMSAVESLOTS
};

static menuitem_t SaveItems[] = {
    {ITT_EFUNC, 0, "1", M_SaveSelect, 0},
    {ITT_EFUNC, 0, "2", M_SaveSelect, 1},
    {ITT_EFUNC, 0, "3", M_SaveSelect, 2},
    {ITT_EFUNC, 0, "4", M_SaveSelect, 3},
    {ITT_EFUNC, 0, "5", M_SaveSelect, 4},
    {ITT_EFUNC, 0, "6", M_SaveSelect, 5},
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {ITT_EFUNC, 0, "7", M_SaveSelect, 6},
    {ITT_EFUNC, 0, "8", M_SaveSelect, 7}
#endif
};

static menu_t SaveDef = {
    0,
#if __JDOOM__ || __JDOOM64__
    80, 44,
#else
    80, 30,
#endif
    M_DrawSave,
    NUMSAVESLOTS, SaveItems,
    0, MENU_MAIN,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A + 8,
    0, NUMSAVESLOTS
};

#if __JSTRIFE__
static menuitem_t SkillItems[] = {
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_BABY},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_EASY},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_MEDIUM},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_HARD},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_NIGHTMARE}
};

static menu_t SkillDef = {
    0,
    120, 44,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_NEWGAME,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT,
    0, 5
};

#elif __JHEXEN__
static menuitem_t SkillItems[] = {
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_BABY},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_EASY},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_MEDIUM},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_HARD},
    {ITT_EFUNC, 0, NULL, M_ChooseSkill, SM_NIGHTMARE}
};

static menu_t SkillDef = {
    0,
    120, 44,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_CLASS,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT,
    0, 5
};
#elif __JHERETIC__
static menuitem_t SkillItems[] = {
    {ITT_EFUNC, 0, "thou needet a wet-nurse", M_ChooseSkill, SM_BABY},
    {ITT_EFUNC, 0, "yellowbellies-r-us", M_ChooseSkill, SM_EASY},
    {ITT_EFUNC, 0, "bringest them oneth", M_ChooseSkill, SM_MEDIUM},
    {ITT_EFUNC, 0, "thou art a smite-meister", M_ChooseSkill, SM_HARD},
    {ITT_EFUNC, 0, "black plague possesses thee", M_ChooseSkill, SM_NIGHTMARE}
};

static menu_t SkillDef = {
    0,
    38, 30,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_EPISODE,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT,
    0, 5
};
#elif __JDOOM64__
static menuitem_t SkillItems[] = {
    {ITT_EFUNC, 0, "I", M_ChooseSkill, 0, &skillModeNames[0]},
    {ITT_EFUNC, 0, "H", M_ChooseSkill, 1, &skillModeNames[1]},
    {ITT_EFUNC, 0, "H", M_ChooseSkill, 2, &skillModeNames[2]},
    {ITT_EFUNC, 0, "U", M_ChooseSkill, 3, &skillModeNames[3]},
};
static menu_t SkillDef = {
    0,
    48, 63,
    M_DrawSkillMenu,
    4, SkillItems,
    2, MENU_NEWGAME,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT,
    0, 4
};
#else
static menuitem_t SkillItems[] = {
    // Text defs TXT_SKILL1..5.
    {ITT_EFUNC, 0, "I", M_ChooseSkill, 0, &skillModeNames[0]},
    {ITT_EFUNC, 0, "H", M_ChooseSkill, 1, &skillModeNames[1]},
    {ITT_EFUNC, 0, "H", M_ChooseSkill, 2, &skillModeNames[2]},
    {ITT_EFUNC, 0, "U", M_ChooseSkill, 3, &skillModeNames[3]},
    {ITT_EFUNC, MIF_NOTALTTXT, "N", M_ChooseSkill, 4, &skillModeNames[4]}
};

static menu_t SkillDef = {
    0,
    48, 63,
    M_DrawSkillMenu,
    5, SkillItems,
    2, MENU_EPISODE,
    huFontB,
    cfg.menuColor,
    NULL, false,
    LINEHEIGHT,
    0, 5
};
#endif

static menuitem_t OptionsItems[] = {
    {ITT_EFUNC, 0, "end game", M_EndGame, 0},
    {ITT_EFUNC, 0, "control panel", M_OpenDCP, 0},
    {ITT_SETMENU, 0, "controls...", NULL, MENU_CONTROLS},
    {ITT_SETMENU, 0, "gameplay...", NULL, MENU_GAMEPLAY},
    {ITT_SETMENU, 0, "hud...", NULL, MENU_HUD},
    {ITT_SETMENU, 0, "automap...", NULL, MENU_MAP},
    {ITT_SETMENU, 0, "weapons...", NULL, MENU_WEAPONSETUP},
    {ITT_SETMENU, 0, "sound...", NULL, MENU_OPTIONS2},
    {ITT_EFUNC, 0, "mouse options", M_OpenDCP, 2},
    {ITT_EFUNC, 0, "joystick options", M_OpenDCP, 2}
};

static menu_t OptionsDef = {
    0,
    94, 63,
    M_DrawOptions,
    10, OptionsItems,
    0, MENU_MAIN,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
    0, 10
};

static menuitem_t Options2Items[] = {
    {ITT_LRFUNC, 0, "SFX VOLUME :", M_SfxVol, 0},
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "MUSIC VOLUME :", M_MusicVol, 0},
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_EFUNC, 0, "OPEN AUDIO PANEL", M_OpenDCP, 1},
};

static menu_t Options2Def = {
    0,
#if __JHEXEN__ || __JSTRIFE__
    70, 25,
#elif __JHERETIC__
    70, 30,
#elif __JDOOM__ || __JDOOM64__
    70, 40,
#endif
    M_DrawOptions2,
#if __JDOOM__ || __JDOOM64__
    3, Options2Items,
#else
    7, Options2Items,
#endif
    0, MENU_OPTIONS,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
#if __JDOOM__ || __JDOOM64__
    0, 3
#else
    0, 7
#endif
};

#if !__JDOOM64__
menuitem_t ReadItems1[] = {
    {ITT_EFUNC, 0, "", M_ReadThis2, 0}
};

menu_t ReadDef1 = {
    MNF_NOSCALE,
    280, 185,
    M_DrawReadThis,
    1, ReadItems1,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    "HELP1",
#if __JDOOM__
    false,
#else
    true,
#endif
    LINEHEIGHT,
    0, 1
};

menuitem_t ReadItems2[] = {
# if __JDOOM__
    {ITT_EFUNC, 0, "", M_FinishReadThis, 0}
# else
    {ITT_EFUNC, 0, "", M_ReadThis3, 0} // heretic and hexen have 3 readthis screens.
# endif
};

menu_t ReadDef2 = {
    MNF_NOSCALE,
    330, 175,
    M_DrawReadThis,
    1, ReadItems2,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    "HELP2",
#if __JDOOM__
    false,
#else
    true,
#endif
    LINEHEIGHT,
    0, 1
};

# if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
menuitem_t ReadItems3[] = {
    {ITT_EFUNC, 0, "", M_FinishReadThis, 0}
};

menu_t ReadDef3 = {
    MNF_NOSCALE,
    330, 175,
    M_DrawReadThis,
    1, ReadItems3,
    0, MENU_MAIN,
    huFontB,
    cfg.menuColor,
    "CREDIT", true,
    LINEHEIGHT,
    0, 1
};
# endif
#endif

static menuitem_t HUDItems[] = {
#if __JDOOM64__
    {ITT_EFUNC, 0, "show ammo :", M_ToggleVar, 0, NULL, "hud-ammo" },
    {ITT_EFUNC, 0, "show armor :", M_ToggleVar, 0, NULL, "hud-armor" },
    {ITT_EFUNC, 0, "show power keys :", M_ToggleVar, 0, NULL, "hud-power" },
    {ITT_EFUNC, 0, "show health :", M_ToggleVar, 0, NULL,"hud-health" },
    {ITT_EFUNC, 0, "show keys :", M_ToggleVar, 0, NULL, "hud-keys" },
    {ITT_LRFUNC, 0, "scale", M_HUDScale, 0},
    {ITT_EFUNC, 0, "   HUD color", SCColorWidget, 5},
#elif __JDOOM__
    {ITT_EFUNC, 0, "show ammo :", M_ToggleVar, 0, NULL, "hud-ammo"},
    {ITT_EFUNC, 0, "show armor :", M_ToggleVar, 0, NULL, "hud-armor"},
    {ITT_EFUNC, 0, "show face :", M_ToggleVar, 0, NULL, "hud-face"},
    {ITT_EFUNC, 0, "show health :", M_ToggleVar, 0, NULL, "hud-health"},
    {ITT_EFUNC, 0, "show keys :", M_ToggleVar, 0, NULL, "hud-keys"},
    {ITT_EFUNC, 0, "single key display :", M_ToggleVar, 0, NULL, "hud-keys-combine"},
    {ITT_LRFUNC, 0, "scale", M_HUDScale, 0},
    {ITT_EFUNC, 0, "   HUD color", SCColorWidget, 5},
#endif
    {ITT_EFUNC, 0, "MESSAGES :", M_ChangeMessages, 0},
    {ITT_LRFUNC, 0, "CROSSHAIR :", M_Xhair, 0},
    {ITT_LRFUNC, 0, "CROSSHAIR SIZE :", M_XhairSize, 0},
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "SCREEN SIZE", M_SizeDisplay, 0},
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
#if !__JDOOM64__
    {ITT_LRFUNC, 0, "STATUS BAR SIZE", M_SizeStatusBar, 0},
# if !__JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
# endif
    {ITT_LRFUNC, 0, "STATUS BAR ALPHA :", M_StatusBarAlpha, 0},
# if !__JDOOM__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
# endif
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

static menu_t HUDDef = {
    0,
#if __JDOOM__ || __JDOOM64__
    70, 40,
#else
    64, 30,
#endif
    M_DrawHUDMenu,
#if __WOLFTC__
    13, HUDItems,
#elif __JHEXEN__ || __JSTRIFE__
    23, HUDItems,
#elif __JHERETIC__
    25, HUDItems,
#elif __JDOOM64__
    11, HUDItems,
#elif __JDOOM__
    14, HUDItems,
#endif
    0, MENU_OPTIONS,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
#if __WOLFTC__
    0, 13
#elif __JHEXEN__ || __JSTRIFE__
    0, 15        // 21
#elif __JHERETIC__
    0, 15        // 23
#elif __JDOOM64__
    0, 11
#elif __JDOOM__
    0, 14
#endif
};

static menuitem_t WeaponItems[] = {
    {ITT_EMPTY,  0, "Use left/right to move", NULL, 0 },
    {ITT_EMPTY,  0, "item up/down the list." , NULL, 0 },
    {ITT_EMPTY,  0, NULL, NULL, 0},
    {ITT_EMPTY,  0, "PRIORITY ORDER", NULL, 0},
    {ITT_LRFUNC, 0, "1 :", M_WeaponOrder, 0 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "2 :", M_WeaponOrder, 1 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "3 :", M_WeaponOrder, 2 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "4 :", M_WeaponOrder, 3 << NUM_WEAPON_TYPES },
#if !__JHEXEN__
    {ITT_LRFUNC, 0, "5 :", M_WeaponOrder, 4 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "6 :", M_WeaponOrder, 5 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "7 :", M_WeaponOrder, 6 << NUM_WEAPON_TYPES },
    {ITT_LRFUNC, 0, "8 :", M_WeaponOrder, 7 << NUM_WEAPON_TYPES },
#endif
#if __JDOOM__ || __JDOOM64__
    {ITT_LRFUNC, 0, "9 :", M_WeaponOrder, 8 << NUM_WEAPON_TYPES },
#endif
#if __JDOOM64__
    {ITT_LRFUNC, 0, "10 :", M_WeaponOrder, 9 << NUM_WEAPON_TYPES },
#endif
    {ITT_EFUNC,  0, "Use with Next/Previous :", M_ToggleVar, 0, NULL, "player-weapon-nextmode"},
    {ITT_EMPTY,  0, NULL, NULL, 0},
    {ITT_EMPTY,  0, "AUTOSWITCH", NULL, 0},
    {ITT_LRFUNC, 0, "PICKUP WEAPON :", M_WeaponAutoSwitch, 0},
    {ITT_EFUNC,  0, "   IF NOT FIRING :", M_ToggleVar, 0, NULL, "player-autoswitch-notfiring"},
    {ITT_LRFUNC, 0, "PICKUP AMMO :", M_AmmoAutoSwitch, 0},
#if __JDOOM__ || __JDOOM64__
    {ITT_EFUNC,  0, "PICKUP BERSERK :", M_ToggleVar, 0, NULL, "player-autoswitch-berserk"}
#endif
};

static menu_t WeaponDef = {
    MNF_NOHOTKEYS,
#if __JDOOM__ || __JDOOM64__
    68, 34,
#else
    50, 20,
#endif
    M_DrawWeaponMenu,
#if __JDOOM64__
    21, WeaponItems,
#elif __JDOOM__
    20, WeaponItems,
#elif __JHERETIC__
    18, WeaponItems,
#elif __JHEXEN__
    14, WeaponItems,
#endif
    0, MENU_OPTIONS,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
#if __JDOOM64__
    0, 21
#elif __JDOOM__
    0, 20
#elif __JHERETIC__
    0, 18
#elif __JHEXEN__
    0, 14
#endif
};

static menuitem_t GameplayItems[] = {
    {ITT_EFUNC, 0, "ALWAYS RUN :", M_ToggleVar, 0, NULL, "ctl-run"},
    {ITT_EFUNC, 0, "USE LOOKSPRING :", M_ToggleVar, 0, NULL, "ctl-look-spring"},
    {ITT_EFUNC, 0, "USE AUTOAIM :", M_ToggleVar, 0, NULL, "ctl-aim-noauto"},
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __JSTRIFE__
    {ITT_EFUNC, 0, "ALLOW JUMPING :", M_ToggleVar, 0, NULL, "player-jump"},
#endif

#if __JDOOM64__
    { ITT_EFUNC, 0, "WEAPON RECOIL : ", M_WeaponRecoil, 0 },
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, "COMPATIBILITY", NULL, 0 },
# if __JDOOM__ || __JDOOM64__
    {ITT_EFUNC, 0, "ANY BOSS TRIGGER 666 :", M_ToggleVar, 0, NULL,
        "game-anybossdeath666"},
#  if !__JDOOM64__
    {ITT_EFUNC, 0, "AV RESURRECTS GHOSTS :", M_ToggleVar, 0, NULL,
        "game-raiseghosts"},
#  endif
    {ITT_EFUNC, 0, "PE LIMITED TO 20 LOST SOULS :", M_ToggleVar, 0, NULL,
        "game-maxskulls"},
    {ITT_EFUNC, 0, "LS CAN GET STUCK INSIDE WALLS :", M_ToggleVar, 0, NULL,
        "game-skullsinwalls"},
# endif
# if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {ITT_EFUNC, 0, "MONSTERS CAN GET STUCK IN DOORS :", M_ToggleVar, 0, NULL,
        "game-monsters-stuckindoors"},
    {ITT_EFUNC, 0, "SOME OBJECTS NEVER HANG OVER LEDGES :", M_ToggleVar, 0, NULL,
        "game-objects-neverhangoverledges"},
    {ITT_EFUNC, 0, "OBJECTS FALL UNDER OWN WEIGHT :", M_ToggleVar, 0, NULL,
        "game-objects-falloff"},
    {ITT_EFUNC, 0, "CORPSES SLIDE DOWN STAIRS :", M_ToggleVar, 0, NULL,
        "game-corpse-sliding"},
    {ITT_EFUNC, 0, "USE EXACTLY DOOM'S CLIPPING CODE :", M_ToggleVar, 0, NULL,
        "game-objects-clipping"},
    {ITT_EFUNC, 0, "  ^IFNOT NORTHONLY WALLRUNNING :", M_ToggleVar, 0, NULL,
        "game-player-wallrun-northonly"},
# endif
# if __JDOOM__ || __JDOOM64__
    {ITT_EFUNC, 0, "ZOMBIE PLAYERS CAN EXIT MAPS :", M_ToggleVar, 0, NULL,
        "game-zombiescanexit"},
    {ITT_EFUNC, 0, "FIX OUCH FACE :", M_ToggleVar, 0, NULL, "hud-face-ouchfix"},
# endif
#endif
};

#if __JHEXEN__
static menu_t GameplayDef = {
    0,
    64, 25,
    M_DrawGameplay,
    3, GameplayItems,
    0, MENU_OPTIONS,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
    0, 3
};
#else
static menu_t GameplayDef = {
    0,
#if __JHERETIC__
    30, 30,
#else
    30, 40,
#endif
    M_DrawGameplay,
#if __JDOOM64__
    17, GameplayItems,
#elif __JDOOM__
    18, GameplayItems,
#else
    12, GameplayItems,
#endif
    0, MENU_OPTIONS,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
#if __JDOOM64__
    0, 17
#elif __JDOOM__
    0, 18
#else
    0, 12
#endif
};
#endif

menu_t* menulist[] = {
    &MainDef,
    &NewGameDef,
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
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    &FilesMenu,
#endif
    &LoadDef,
    &SaveDef,
    &MultiplayerMenu,
    &GameSetupMenu,
    &PlayerSetupMenu,
    &WeaponDef,
    &ControlsDef,
    NULL
};

static menuitem_t ColorWidgetItems[] = {
    {ITT_LRFUNC, 0, "red :    ", M_WGCurrentColor, 0, NULL, &currentcolor[0] },
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "green :", M_WGCurrentColor, 0, NULL, &currentcolor[1] },
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "blue :  ", M_WGCurrentColor, 0, NULL, &currentcolor[2] },
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "alpha :", M_WGCurrentColor, 0, NULL, &currentcolor[3] },
};

static menu_t ColorWidgetMnu = {
    MNF_NOHOTKEYS,
    98, 60,
    NULL,
#if __JDOOM__ || __JDOOM64__
    4, ColorWidgetItems,
#else
    10, ColorWidgetItems,
#endif
    0, MENU_OPTIONS,
    huFontA,
    cfg.menuColor2,
    NULL, false,
    LINEHEIGHT_A,
#if __JDOOM__ || __JDOOM64__
    0, 4
#else
    0, 10
#endif
};

// Cvars for the menu
cvar_t menuCVars[] =
{
    {"menu-scale", 0, CVT_FLOAT, &cfg.menuScale, .1f, 1},
    {"menu-flash-r", 0, CVT_FLOAT, &cfg.flashColor[0], 0, 1},
    {"menu-flash-g", 0, CVT_FLOAT, &cfg.flashColor[1], 0, 1},
    {"menu-flash-b", 0, CVT_FLOAT, &cfg.flashColor[2], 0, 1},
    {"menu-flash-speed", 0, CVT_INT, &cfg.flashSpeed, 0, 50},
    {"menu-turningskull", 0, CVT_BYTE, &cfg.turningSkull, 0, 1},
    {"menu-effect", 0, CVT_INT, &cfg.menuEffects, 0, 2},
    {"menu-color-r", 0, CVT_FLOAT, &cfg.menuColor[0], 0, 1},
    {"menu-color-g", 0, CVT_FLOAT, &cfg.menuColor[1], 0, 1},
    {"menu-color-b", 0, CVT_FLOAT, &cfg.menuColor[2], 0, 1},
    {"menu-colorb-r", 0, CVT_FLOAT, &cfg.menuColor2[0], 0, 1},
    {"menu-colorb-g", 0, CVT_FLOAT, &cfg.menuColor2[1], 0, 1},
    {"menu-colorb-b", 0, CVT_FLOAT, &cfg.menuColor2[2], 0, 1},
    {"menu-glitter", 0, CVT_FLOAT, &cfg.menuGlitter, 0, 1},
    {"menu-fog", 0, CVT_INT, &cfg.menuFog, 0, 4},
    {"menu-shadow", 0, CVT_FLOAT, &cfg.menuShadow, 0, 1},
    {"menu-patch-replacement", 0, CVT_BYTE, &cfg.usePatchReplacement, 0, 2},
    {"menu-slam", 0, CVT_BYTE, &cfg.menuSlam, 0, 1},
    {"menu-quick-ask", 0, CVT_BYTE, &cfg.askQuickSaveLoad, 0, 1},
#if __JDOOM__ || __JDOOM64__
    {"menu-quitsound", 0, CVT_INT, &cfg.menuQuitSound, 0, 1},
#endif
    {NULL}
};

// Console commands for the menu
ccmd_t  menuCCmds[] = {
    {"menu",        "", CCmdMenuAction},
    {"menuup",      "", CCmdMenuAction},
    {"menudown",    "", CCmdMenuAction},
    {"menuleft",    "", CCmdMenuAction},
    {"menuright",   "", CCmdMenuAction},
    {"menuselect",  "", CCmdMenuAction},
    {"menudelete",  "", CCmdMenuAction},
    {"menucancel",  "", CCmdMenuAction},
    {"helpscreen",  "", CCmdMenuAction},
    {"savegame",    "", CCmdMenuAction},
    {"loadgame",    "", CCmdMenuAction},
    {"soundmenu",   "", CCmdMenuAction},
    {"quicksave",   "", CCmdMenuAction},
    {"endgame",     "", CCmdMenuAction},
    {"togglemsgs",  "", CCmdMenuAction},
    {"quickload",   "", CCmdMenuAction},
    {"quit",        "", CCmdMenuAction},
    {"togglegamma", "", CCmdMenuAction},
    {NULL}
};

// Code -------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up
 * Register Cvars and CCmds for the opperation/look of the menu.
 */
void Hu_MenuRegister(void)
{
    int             i;

    for(i = 0; menuCVars[i].name; ++i)
        Con_AddVariable(menuCVars + i);
    for(i = 0; menuCCmds[i].name; ++i)
        Con_AddCommand(menuCCmds + i);
}

/**
 * Load any resources the menu needs.
 */
void M_LoadData(void)
{
    int             i;
    char            buffer[9];

    // Load the cursor patches
    for(i = 0; i < cursors; ++i)
    {
        sprintf(buffer, CURSORPREF, i+1);
        R_CachePatch(&cursorst[i], buffer);
    }

#if __JDOOM__ || __JDOOM64__
    R_CachePatch(&m_doom, "M_DOOM");
    R_CachePatch(&m_newg, "M_NEWG");
    R_CachePatch(&m_skill, "M_SKILL");
    R_CachePatch(&m_episod, "M_EPISOD");
    R_CachePatch(&m_ngame, "M_NGAME");
    R_CachePatch(&m_option, "M_OPTION");
    R_CachePatch(&m_loadg, "M_LOADG");
    R_CachePatch(&m_saveg, "M_SAVEG");
    R_CachePatch(&m_rdthis, "M_RDTHIS");
    R_CachePatch(&m_quitg, "M_QUITG");
    R_CachePatch(&m_optttl, "M_OPTTTL");
    R_CachePatch(&dpLSLeft, "M_LSLEFT");
    R_CachePatch(&dpLSRight, "M_LSRGHT");
    R_CachePatch(&dpLSCntr, "M_LSCNTR");
# if __JDOOM__
    if(gameMode == retail || gameMode == commercial)
        R_CachePatch(&credit, "CREDIT");
    if(gameMode == commercial)
        R_CachePatch(&help, "HELP");
    if(gameMode == shareware || gameMode == registered || gameMode == retail)
        R_CachePatch(&help1, "HELP1");
    if(gameMode == shareware || gameMode == registered)
        R_CachePatch(&help2, "HELP2");
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    R_CachePatch(&m_htic, "M_HTIC");
    R_CachePatch(&dpFSlot, "M_FSLOT");
#endif

    if(!menuFogData.texture && !Get(DD_NOVIDEO))
    {
        menuFogData.texture =
            GL_NewTextureWithParams2(DGL_LUMINANCE, 64, 64,
                                     W_CacheLumpName("menufog", PU_CACHE),
                                     0, DGL_NEAREST, DGL_LINEAR,
                                     -1 /*best anisotropy*/,
                                     DGL_REPEAT, DGL_REPEAT);
    }
}

/**
 * The opposite of M_LoadData()
 */
void M_UnloadData(void)
{
    if(Get(DD_NOVIDEO))
        return;

    if(menuFogData.texture)
        DGL_DeleteTextures(1, (DGLuint*) &menuFogData.texture);
    menuFogData.texture = 0;
}

/**
 * Menu initialization.
 * Called during (post-engine) init and after updating game/engine state.
 *
 * Initializes the various vars, fonts, adjust the menu structs and
 * anything else that needs to be done before the menu can be used.
 */
void Hu_MenuInit(void)
{
#if !__JDOOM64__
    menuitem_t *item;
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int   i, w, maxw;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    R_GetGammaMessageStrings();
#endif

#if __JDOOM__ || __JDOOM64__
    // Quit messages.
    endmsg[0] = GET_TXT(TXT_QUITMSG);
    for(i = 1; i <= NUM_QUITMESSAGES; ++i)
        endmsg[i] = GET_TXT(TXT_QUITMESSAGE1 + i - 1);
#endif
#if __JHERETIC__
    // Episode names.
    for(i = 0, maxw = 0; i < 4; ++i)
    {
        EpisodeItems[i].text = GET_TXT(TXT_EPISODE1 + i);
        w = M_StringWidth(EpisodeItems[i].text, EpiDef.font);
        if(w > maxw)
            maxw = w;
    }
    // Center the episodes menu appropriately.
    EpiDef.x = 160 - maxw / 2 + 12;

#elif __JDOOM__
    // Episode names.
    for(i = 0, maxw = 0; i < 4; ++i)
    {
        EpisodeItems[i].text = GET_TXT(TXT_EPISODE1 + i);
        w = M_StringWidth(EpisodeItems[i].text, EpiDef.font);
        if(w > maxw)
            maxw = w;

        EpisodeItems[i].patch = &episodeNamePatches[i];
    }
    // Center the episodes menu appropriately.
    EpiDef.x = 160 - maxw / 2 + 12;
    // "Choose Episode"
    episodemsg = GET_TXT(TXT_ASK_EPISODE);
#endif

#if __JDOOM__ || __JDOOM64__
    // Skill names.
    for(i = 0, maxw = 0; i < NUM_SKILL_MODES; ++i)
    {
        SkillItems[i].text = GET_TXT(TXT_SKILL1 + i);
        w = M_StringWidth(SkillItems[i].text, SkillDef.font);
        if(w > maxw)
            maxw = w;
    }
    // Center the skill menu appropriately.
    SkillDef.x = 160 - maxw / 2 + 12;
#endif

    currentMenu = &MainDef;
    menuActive = false;
    DD_Execute(true, "deactivatebclass menu");
    menuAlpha = menuTargetAlpha = 0;

    menuFogData.texture = 0;
    menuFogData.alpha = menuFogData.targetAlpha = 0;
    menuFogData.joinY = 0.5f;
    menuFogData.scrollDir = true;
    menuFogData.layers[0].texOffset[VX] =
        menuFogData.layers[0].texOffset[VY] = 0;
    menuFogData.layers[0].texAngle = 93;
    menuFogData.layers[0].posAngle = 35;
    menuFogData.layers[1].texOffset[VX] =
        menuFogData.layers[1].texOffset[VY] = 0;
    menuFogData.layers[1].texAngle = 12;
    menuFogData.layers[1].posAngle = 77;

    M_LoadData();

    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = MENUCURSOR_TICSPERFRAME;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuActive;
    quickSaveSlot = -1;

#if __JDOOM__
    // Here we catch version dependencies, like HELP1/2, and four episodes.
    switch(gameMode)
    {
    case commercial:
        // This is used because DOOM 2 had only one HELP
        //  page. I use CREDIT as second page now, but
        //  kept this hack for educational purposes.
        item = &MainItems[READTHISID];
        item->func = M_QuitDOOM;
        item->text = "{case}Quit Game";
        item->patch = &m_quitg;
        MainDef.itemCount--;
        MainDef.y += 8;
        SkillDef.prevMenu = MENU_NEWGAME;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadDef1.background = "HELP";
        ReadDef1.backgroundIsRaw = false;
        ReadDef2.background = "CREDIT";
        ReadDef2.backgroundIsRaw = false;
        ReadItems1[0].func = M_FinishReadThis;
        break;
    case shareware:
        // Episode 2 and 3 are handled, branching to an ad screen.
    case registered:
        // We need to remove the fourth episode.
        EpiDef.itemCount = 3;
        item = &MainItems[READTHISID];
        item->func = M_ReadThis;
        item->text = "{case}Read This!";
        item->patch = &m_rdthis;
        MainDef.itemCount--;
        MainDef.y = 64;
        ReadDef1.background = "HELP1";
        ReadDef1.backgroundIsRaw = false;
        ReadDef2.background = "HELP2";
        ReadDef2.backgroundIsRaw = false;
        break;
    case retail:
        // We are fine.
        EpiDef.itemCount = 4;
        ReadDef1.background = "HELP1";
        ReadDef2.background = "CREDIT";
        break;

    default:
        break;
    }
#else
# if !__JDOOM64__
    item = &MainItems[READTHISID];
    item->func = M_ReadThis;
# endif
#endif

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    SkullBaseLump = W_GetNumForName(SKULLBASELMP);
#endif

#if __JHERETIC__
    if(gameMode == extended)
    {                            // Add episodes 4 and 5 to the menu
        EpiDef.itemCount = EpiDef.numVisItems = 5;
        EpiDef.y = 50 - ITEM_HEIGHT;
    }
#endif

    M_InitControlsMenu();
}

/**
 * @return              @c true, iff the menu is currently active (open).
 */
boolean Hu_MenuIsActive(void)
{
    return menuActive;
}

/**
 * Set the alpha level of the entire menu.
 *
 * @param alpha         Alpha level to set the menu too (0...1)
 */
void Hu_MenuSetAlpha(float alpha)
{
    // The menu's alpha will start moving towards this target value.
    menuTargetAlpha = alpha;
}

/**
 * @return              Current alpha level of the menu.
 */
float Hu_MenuAlpha(void)
{
    return menuAlpha;
}

/**
 * Updates on Game Tick.
 */
void Hu_MenuTicker(timespan_t time)
{
#define fog                 (&menuFogData)
#define MENUALPHA_FADE_STEP (.07f)
#define MENUFOGALPHA_FADE_STEP (.07f)

    int                 i;
    float               diff;
    static trigger_t    fixed = { 1 / 35.0 };

    if(!M_RunTrigger(&fixed, time))
        return;

    typeInTime++;

    // Check if there has been a response to a message
    if(messageToPrint)
    {
        if(messageNeedsInput == true)
        {
            if(messageRoutine)
                messageRoutine(0, NULL);
        }
    }

    // Move towards the target alpha level for the entire menu.
    diff = menuTargetAlpha - menuAlpha;
    if(fabs(diff) > MENUALPHA_FADE_STEP)
    {
        menuAlpha += MENUALPHA_FADE_STEP * (diff > 0? 1 : -1);
    }
    else
    {
        menuAlpha = menuTargetAlpha;
    }

    // Move towards the target alpha level for fog effect
    diff = menuFogData.targetAlpha - menuFogData.alpha;
    if(fabs(diff) > MENUFOGALPHA_FADE_STEP)
    {
        menuFogData.alpha += MENUFOGALPHA_FADE_STEP * (diff > 0? 1 : -1);
    }
    else
    {
        menuFogData.alpha = menuFogData.targetAlpha;
    }

    // menu zoom in/out
    if(!menuActive && fog->alpha > 0)
    {
        outFade += 1 / (float) slamInTicks;
        if(outFade > 1)
            fadingOut = false;
    }

    if(menuActive || fog->alpha > 0)
    {
        float               rewind = 20;

        for(i = 0; i < 2; ++i)
        {
            if(cfg.menuFog == 1)
            {
                fog->layers[i].texAngle += MENUFOGSPEED[i] / 4;
                fog->layers[i].posAngle -= MENUFOGSPEED[!i];
                fog->layers[i].texOffset[VX] =
                    160 + 120 * cos(fog->layers[i].posAngle / 180 * PI);
                fog->layers[i].texOffset[VY] =
                    100 + 100 * sin(fog->layers[i].posAngle / 180 * PI);
            }
            else
            {
                fog->layers[i].texAngle += MENUFOGSPEED[i] / 4;
                fog->layers[i].posAngle -= MENUFOGSPEED[!i] * 1.5f;
                fog->layers[i].texOffset[VX] =
                    320 + 320 * cos(fog->layers[i].posAngle / 180 * PI);
                fog->layers[i].texOffset[VY] =
                    240 + 240 * sin(fog->layers[i].posAngle / 180 * PI);
            }
        }

        // Fade in/out the widget background filter
        if(widgetEdit)
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

        // Calculate the height of the menuFog 3 Y join
        if(cfg.menuFog == 3)
        {
            if(fog->scrollDir && fog->joinY > 0.46f)
                fog->joinY = fog->joinY / 1.002f;
            else if(!fog->scrollDir && fog->joinY < 0.54f )
                fog->joinY = fog->joinY * 1.002f;

            if((fog->joinY < 0.46f || fog->joinY > 0.54f))
                fog->scrollDir = !fog->scrollDir;
        }

        // Animate the cursor patches
        if(--skullAnimCounter <= 0)
        {
            whichSkull++;
            skullAnimCounter = MENUCURSOR_TICSPERFRAME;
            if(whichSkull > cursors-1)
                whichSkull = 0;
        }

        menuTime++;

        menu_color += cfg.flashSpeed;
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
        frame = (menuTime / 3) % 18;
    }

    if(menuActive)
        MN_TickerEx();

#undef fog
#undef MENUALPHA_FADE_STEP
#undef MENUFOGALPHA_FADE_STEP
}

void M_SetupNextMenu(menu_t* menudef)
{
    if(!menudef)
        return;

    currentMenu = menudef;

    // Have we been to this menu before?
    // If so move the cursor to the last selected item
    if(currentMenu->lastOn)
    {
        itemOn = currentMenu->lastOn;
    }
    else
    {   // Select the first active item in this menu.
        int                     i;

        for(i = 0; i < menudef->itemCount; ++i)
        {
            if(menudef->items[i].type != ITT_EMPTY)
                break;
        }

        if(i > menudef->itemCount)
            itemOn = -1;
        else
            itemOn = i;
    }

    menu_color = 0;
    skull_angle = 0;
    typeInTime = 0;
}

/**
 * @return              @c true, if the menu is active and there is a
 *                      background for this page.
 */
boolean MN_CurrentMenuHasBackground(void)
{
    if(!menuActive)
        return false;

    return (currentMenu->background &&
            W_CheckNumForName(currentMenu->background) != -1);
}

/**
 * Sets the view matrix up for rendering the menu
 */
static void setMenuMatrix(float time)
{
    boolean             allowScaling = !(currentMenu->flags & MNF_NOSCALE);
    boolean             rendMenuEffect = true;

    // Use a plain 320x200 projection.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, 320, 200, -1, 1);

    // If there is a menu background raw lump, draw it instead of the
    // background effect.
    if(currentMenu->background)
    {
        lumpnum_t           lump =
            W_CheckNumForName(currentMenu->background);

        if(lump != -1)
        {
            DGL_Color4f(1, 1, 1, menuFogData.alpha);

            if(currentMenu->backgroundIsRaw)
                GL_DrawRawScreen_CS(lump, 0, 0, 1, 1);
            else
                GL_DrawPatch_CS(0, 0, lump);
            rendMenuEffect = false;
        }
    }

    // Draw a background effect?
    if(rendMenuEffect)
    {
#define mfd                 (&menuFogData)

        // Two layers.
        Hu_DrawFogEffect(cfg.menuFog, mfd->texture,
                         mfd->layers[0].texOffset, mfd->layers[0].texAngle,
                         mfd->alpha, menuFogData.joinY);
        Hu_DrawFogEffect(cfg.menuFog, mfd->texture,
                         mfd->layers[1].texOffset, mfd->layers[1].texAngle,
                         mfd->alpha, menuFogData.joinY);

#undef mfd
    }

    if(allowScaling)
    {
        // Scale by the menuScale.
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(160, 100, 0);

        if(cfg.menuSlam){
            if(time > 1 && time <= 2)
            {
                time = 2 - time;
                DGL_Scalef(cfg.menuScale * (.9f + time * .1f),
                      cfg.menuScale * (.9f + time * .1f), 1);
            } else {
                DGL_Scalef(cfg.menuScale * (2 - time),
                      cfg.menuScale * (2 - time), 1);
            }
        } else
            DGL_Scalef(cfg.menuScale, cfg.menuScale, 1);

        DGL_Translatef(-160, -100, 0);
    }
}

static void drawMessage(void)
{
#define BUFSIZE 80

    int             x, y;
    uint            i, start;
    char            string[BUFSIZE];

    start = 0;

    y = 100 - M_StringHeight(messageString, huFontA) / 2;
    while(*(messageString + start))
    {
        for(i = 0; i < strlen(messageString + start); ++i)
            if(*(messageString + start + i) == '\n' || i > BUFSIZE-1)
            {
                memset(string, 0, BUFSIZE);
                strncpy(string, messageString + start, MIN_OF(i, BUFSIZE));
                string[BUFSIZE - 1] = 0;
                start += i + 1;
                break;
            }

        if(i == strlen(messageString + start))
        {
            strncpy(string, messageString + start, BUFSIZE);
            string[BUFSIZE - 1] = 0;
            start += i;
        }

        x = 160 - M_StringWidth(string, huFontA) / 2;
        M_WriteText2(x, y, string, huFontA, cfg.menuColor2[0],
                     cfg.menuColor2[1], cfg.menuColor2[2], 1);

        y += huFontA[17].height;
    }

#undef BUFSIZE
}

/**
 * This is the main menu drawing routine (called every tic by the drawing
 * loop) Draws the current menu 'page' by calling the funcs attached to
 * each menu item.
 *
 * Also draws any current menu message 'Are you sure you want to quit?'
 */
void Hu_MenuDrawer(void)
{
    int             i;
    int             pos[2], offset[2], width, height;
    float           scale, temp;
    int             effTime;
    boolean         allowScaling =
        (!(currentMenu->flags & MNF_NOSCALE)? true : false);

    effTime = (menuTime > menuDarkTicks? menuDarkTicks : menuTime);
    temp = .5 * effTime / (float) menuDarkTicks;

    if(!menuActive && menuAlpha > 0.0125f)  // fading out
    {
        temp = outFade + 1;
    }
    else
    {
        effTime = (menuTime > slamInTicks) ? slamInTicks : menuTime;
        temp = (effTime / (float) slamInTicks);
    }

    // These are popped in the end of the function.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    // Setup matrix.
    if(messageToPrint || ( menuActive || (menuAlpha > 0 || menuFogData.alpha > 0)) )
        setMenuMatrix(messageToPrint? 1 : temp);    // don't slam messages

    // Don't change back to the menu after a canceled quit
    if(!menuActive && quitAsk)
        goto end_draw_menu;

    // Horiz. & Vertically center string and print it.
    if(messageToPrint)
    {
        drawMessage();
        goto end_draw_menu;
    }

    if(!menuActive && !(menuAlpha > 0) && !(menuFogData.alpha > 0))
        goto end_draw_menu;

    if(allowScaling && currentMenu->unscaled.numVisItems)
    {
        currentMenu->numVisItems = currentMenu->unscaled.numVisItems / cfg.menuScale;
        currentMenu->y = 110 - (110 - currentMenu->unscaled.y) / cfg.menuScale;

        if(currentMenu->firstItem && currentMenu->firstItem < currentMenu->numVisItems)
        {
            // Make sure all pages are divided correctly.
            currentMenu->firstItem = 0;
        }
        if(itemOn - currentMenu->firstItem >= currentMenu->numVisItems)
        {
            itemOn = currentMenu->firstItem + currentMenu->numVisItems - 1;
        }
    }

    if(currentMenu->drawFunc)
        currentMenu->drawFunc(); // Call Draw routine.

    pos[VX] = currentMenu->x;
    pos[VY] = currentMenu->y;

    if(menuAlpha > 0.0125f)
    {
        for(i = currentMenu->firstItem;
            i < currentMenu->itemCount && i < currentMenu->firstItem + currentMenu->numVisItems; ++i)
        {
            float           t, r, g, b;

            // Which color?
#if __JDOOM__ || __JDOOM64__
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
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
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
            else if(itemOn == i && !widgetEdit && cfg.usePatchReplacement)
            {
                // Selection!
                if(menu_color <= 50)
                    t = menu_color / 50.0f;
                else
                    t = (100 - menu_color) / 50.0f;
                r = currentMenu->color[0] * t + cfg.flashColor[0] * (1 - t);
                g = currentMenu->color[1] * t + cfg.flashColor[1] * (1 - t);
                b = currentMenu->color[2] * t + cfg.flashColor[2] * (1 - t);
            }
            else
            {
                r = currentMenu->color[0];
                g = currentMenu->color[1];
                b = currentMenu->color[2];
            }
#if __JDOOM__ || __JDOOM64__
            }
#endif
            if(currentMenu->items[i].patch)
            {
                WI_DrawPatch(pos[VX], pos[VY], r, g, b, menuAlpha,
                             currentMenu->items[i].patch,
                             (currentMenu->items[i].flags & MIF_NOTALTTXT)? NULL :
                             currentMenu->items[i].text, true, ALIGN_LEFT);
            }
            else if(currentMenu->items[i].text)
            {
                WI_DrawParamText(pos[VX], pos[VY],
                                currentMenu->items[i].text, currentMenu->font,
                                r, g, b, menuAlpha,
                                false,
                                cfg.usePatchReplacement? true : false,
                                ALIGN_LEFT);
            }

            pos[VY] += currentMenu->itemHeight;
        }

        // Draw the colour widget?
        if(widgetEdit)
        {
            Draw_BeginZoom(0.5f, 160, 100);
            DrawColorWidget();
        }

        // Draw the menu cursor.
        if(allowScaling)
        {
            if(itemOn >= 0)
            {
                menu_t* mn = (widgetEdit? &ColorWidgetMnu : currentMenu);

                scale = mn->itemHeight / (float) LINEHEIGHT;
                width = cursorst[whichSkull].width * scale;
                height = cursorst[whichSkull].height * scale;

                offset[VX] = mn->x;
                offset[VX] += MENUCURSOR_OFFSET_X * scale;

                offset[VY] = mn->y;
                offset[VY] += (itemOn - mn->firstItem + 1) * mn->itemHeight;
                offset[VY] -= (float) height / 2;
                offset[VY] += MENUCURSOR_OFFSET_Y * scale;

                GL_SetPatch(cursorst[whichSkull].lump, DGL_CLAMP, DGL_CLAMP);
                DGL_MatrixMode(DGL_MODELVIEW);
                DGL_PushMatrix();

                DGL_Translatef(offset[VX], offset[VY], 0);
                DGL_Scalef(1, 1.0f / 1.2f, 1);
                if(skull_angle)
                    DGL_Rotatef(skull_angle, 0, 0, 1);
                DGL_Scalef(1, 1.2f, 1);

                GL_DrawRect(-width/2.f, -height/2.f, width, height, 1, 1, 1, menuAlpha);

                DGL_MatrixMode(DGL_MODELVIEW);
                DGL_PopMatrix();
            }
        }

        if(widgetEdit)
        {
            Draw_EndZoom();
        }
    }

  end_draw_menu:

    // Restore original matrices.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    M_ControlGrabDrawer();
}

/**
 * Execute a menu navigation/action command.
 */
void Hu_MenuCommand(menucommand_e cmd)
{
    if(cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST)
    {
        if(cmd == MCMD_CLOSEFAST)
        {   // Hide the menu instantly.
            menuAlpha = menuTargetAlpha = 0;
            menuFogData.alpha = menuFogData.targetAlpha = 0;
            fadingOut = false;
        }
        else
        {
            menuTargetAlpha = 0;
            menuFogData.targetAlpha = 0;
            fadingOut = true;
        }

        menuActive = false;
        outFade = 0;

        // Disable the menu binding class
        DD_Execute(true, "deactivatebclass menu");
        return;
    }

    if(!menuActive)
    {
        if(cmd == MCMD_OPEN)
        {
            S_LocalSound(menusnds[2], NULL);

            Con_Open(false);

            Hu_MenuSetAlpha(1);
            menuFogData.targetAlpha = 1;
            menuActive = true;
            menu_color = 0;
            menuTime = 0;
            fadingOut = false;
            skull_angle = 0;
            currentMenu = &MainDef;
            itemOn = currentMenu->lastOn;
            typeInTime = 0;
            quitAsk = 0;

            // Enable the menu binding class
            DD_Execute(true, "activatebclass menu");
        }
    }
    else
    {
        int             i;
        int             firstVI, lastVI; // first and last visible item
        int             itemCountOffset = 0;
        const menuitem_t *item;
        menu_t         *menu = currentMenu;

        if(widgetEdit)
        {
            menu = &ColorWidgetMnu;

            if(!rgba)
                itemCountOffset = 1;
        }

        firstVI = menu->firstItem;
        lastVI = firstVI + menu->numVisItems - 1 - itemCountOffset;
        if(lastVI > menu->itemCount - 1 - itemCountOffset)
            lastVI = menu->itemCount - 1 - itemCountOffset;
        item = &menu->items[itemOn];
        menu->lastOn = itemOn;

        switch(cmd)
        {
        case MCMD_NAV_LEFT:
            if(item->type == ITT_LRFUNC && item->func != NULL)
            {
                item->func(LEFT_DIR | item->option, item->data);
                S_LocalSound(menusnds[4], NULL);
            }
            else
            {
                menu_nav_left:

                // Let's try to change to the previous page.
                if(menu->firstItem - menu->numVisItems >= 0)
                {
                    menu->firstItem -= menu->numVisItems;
                    itemOn -= menu->numVisItems;

                    // Ensure cursor points to an editable item
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

        case MCMD_NAV_RIGHT:
            if(item->type == ITT_LRFUNC && item->func != NULL)
            {
                item->func(RIGHT_DIR | item->option, item->data);
                S_LocalSound(menusnds[4], NULL);
            }
            else
            {
                menu_nav_right:

                // Move on to the next page, if possible.
                if(menu->firstItem + menu->numVisItems <
                   menu->itemCount)
                {
                    menu->firstItem += menu->numVisItems;
                    itemOn += menu->numVisItems;

                    // Ensure cursor points to an editable item
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

        case MCMD_NAV_DOWN:
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

        case MCMD_NAV_UP:
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

        case MCMD_NAV_OUT:
            menu->lastOn = itemOn;
            if(menu->prevMenu == MENU_NONE)
            {
                menu->lastOn = itemOn;
                S_LocalSound(menusnds[1], NULL);
                Hu_MenuCommand(MCMD_CLOSE);
            }
            else
            {
                M_SetupNextMenu(menulist[menu->prevMenu]);
                S_LocalSound(menusnds[2], NULL);
            }
            break;

        case MCMD_DELETE:
            if(menu->flags & MNF_DELETEFUNC)
            {
                if(item->func)
                {
                    item->func(-1, item->data);
                    S_LocalSound(menusnds[2], NULL);
                }
            }
            break;

        case MCMD_SELECT:
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
        }
    }
}

/**
 * Responds to alphanumeric input for edit fields.
 */
boolean M_EditResponder(event_t *ev)
{
    int         ch = -1;
    char       *ptr;

    if(!saveStringEnter && !ActiveEdit && !messageToPrint)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
        shiftdown = (ev->state == EVS_DOWN);

    if(ev->type == EV_KEY && (ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
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
                    if(saveCharIndex < SAVESTRINGSIZE &&
                        M_StringWidth(savegamestrings[saveSlot], huFontA)
                        < (SAVESTRINGSIZE - 1) * 8)
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

    return false;
}

void M_EndAnyKeyMsg(void)
{
    M_StopMessage();
    Hu_MenuCommand(MCMD_CLOSE);
    S_LocalSound(menusnds[1], NULL);
}

/**
 * This is the "fallback" responder, its the last stage in the event chain
 * so if an event reaches here it means there was no suitable binding for it.
 *
 * Handles the hotkey selection in the menu and "press any key" messages.
 *
 * @return              @c true, if it ate the event.
 */
boolean Hu_MenuResponder(event_t* ev)
{
    int                 ch = -1;
    int                 i;
    uint                cid;
    int                 firstVI, lastVI;    // first and last visible item
    boolean             skip;

    if(!menuActive)
    {
        // Any key/button down pops up menu if in demos.
        if(G_GetGameAction() == GA_NONE && !singledemo &&
           (Get(DD_PLAYBACK) || FI_IsMenuTrigger(ev)))
        {
            if(ev->state == EVS_DOWN &&
               (ev->type == EV_KEY || ev->type == EV_MOUSE_BUTTON ||
                ev->type == EV_JOY_BUTTON))
            {
                Hu_MenuCommand(MCMD_OPEN);
                return true;
            }
        }

        return false;
    }

    if(widgetEdit || (currentMenu->flags & MNF_NOHOTKEYS))
        return false;

    if(ev->type == EV_KEY && (ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
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
    for(i = firstVI; i <= lastVI; ++i)
    {
        if(currentMenu->items[i].text && currentMenu->items[i].type != ITT_EMPTY)
        {
            cid = 0;
            if(currentMenu->items[i].text[0] == '{')
            {   // A parameter string, skip till '}' is found
                for(cid = 0, skip = true;
                     cid < strlen(currentMenu->items[i].text) && skip; ++cid)
                    if(currentMenu->items[i].text[cid] == '}')
                        skip = false;
            }

            if(toupper(ch) == toupper(currentMenu->items[i].text[cid]))
            {
                itemOn = i;
                return true;
            }
        }
    }
    return false;
}

/**
 * The colour widget edits the "hot" currentcolour[]
 * The widget responder handles setting the specified vars to that of the
 * currentcolour.
 *
 * \fixme The global value rgba (fixme!) is used to control if rgb or rgba input
 * is needed, as defined in the widgetcolors array.
 */
void DrawColorWidget(void)
{
    int         w = 0;
    menu_t     *menu = &ColorWidgetMnu;

    if(widgetEdit)
    {
#if __JDOOM__ || __JDOOM64__
        w = 38;
#else
        w = 46;
#endif

        M_DrawBackgroundBox(menu->x -30, menu->y -40,
#if __JDOOM__ || __JDOOM64__
                        160, (rgba? 85 : 75),
#else
                        180, (rgba? 170 : 140),
#endif
                             1, 1, 1, menuAlpha, true, BORDERUP);

        GL_SetNoTexture();
        GL_DrawRect(menu->x+w, menu->y-30, 24, 22, currentcolor[0],
                    currentcolor[1], currentcolor[2], currentcolor[3]);
        M_DrawBackgroundBox(menu->x+w, menu->y-30, 24, 22, 1, 1, 1,
                            menuAlpha, false, BORDERDOWN);
#if __JDOOM__ || __JDOOM64__
        MN_DrawSlider(menu, 0, 11, currentcolor[0] * 10 + .25f);
        M_WriteText2(menu->x, menu->y, ColorWidgetItems[0].text,
                     huFontA, 1, 1, 1, menuAlpha);
        MN_DrawSlider(menu, 1, 11, currentcolor[1] * 10 + .25f);
        M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A),
                     ColorWidgetItems[1].text, huFontA, 1, 1, 1, menuAlpha);
        MN_DrawSlider(menu, 2, 11, currentcolor[2] * 10 + .25f);
        M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 2),
                     ColorWidgetItems[2].text, huFontA, 1, 1, 1, menuAlpha);
#else
        MN_DrawSlider(menu, 1, 11, currentcolor[0] * 10 + .25f);
        M_WriteText2(menu->x, menu->y, ColorWidgetItems[0].text,
                     huFontA, 1, 1, 1, menuAlpha);
        MN_DrawSlider(menu, 4, 11, currentcolor[1] * 10 + .25f);
        M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 3),
                     ColorWidgetItems[3].text, huFontA, 1, 1, 1, menuAlpha);
        MN_DrawSlider(menu, 7, 11, currentcolor[2] * 10 + .25f);
        M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 6),
                     ColorWidgetItems[6].text, huFontA, 1, 1, 1, menuAlpha);
#endif
        if(rgba)
        {
#if __JDOOM__ || __JDOOM64__
            MN_DrawSlider(menu, 3, 11, currentcolor[3] * 10 + .25f);
            M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 3),
                         ColorWidgetItems[3].text, huFontA, 1, 1, 1,
                         menuAlpha);
#else
            MN_DrawSlider(menu, 10, 11, currentcolor[3] * 10 + .25f);
            M_WriteText2(menu->x, menu->y + (LINEHEIGHT_A * 9),
                         ColorWidgetItems[9].text, huFontA, 1, 1, 1,
                         menuAlpha);
#endif
        }

    }
}

/**
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
    if(widgetcolors[index].a)
    {
        rgba = true;
        currentcolor[3] = *widgetcolors[index].a;
    }
    else
    {
        rgba = false;
        currentcolor[3] = 1.0f;
    }

    // Activate the widget
    widgetEdit = true;
}

void M_ToggleVar(int index, void *data)
{
    char       *cvarname = (char *) data;

    if(!data)
        return;

    DD_Executef(true, "toggle %s", cvarname);
    S_LocalSound(menusnds[0], NULL);
}

void M_DrawTitle(char *text, int y)
{
    WI_DrawParamText(160 - M_StringWidth(text, huFontB) / 2, y, text,
                     huFontB, cfg.menuColor[0], cfg.menuColor[1],
                     cfg.menuColor[2], menuAlpha, true, true, ALIGN_LEFT);
}

void M_WriteMenuText(const menu_t *menu, int index, const char *text)
{
    int         off = 0;

    if(menu->items[index].text)
        off = M_StringWidth(menu->items[index].text, menu->font) + 4;

    M_WriteText2(menu->x + off,
                 menu->y + menu->itemHeight * (index  - menu->firstItem),
                 text, menu->font, 1, 1, 1, menuAlpha);
}

/**
 * User wants to load this game
 */
void M_LoadSelect(int option, void* data)
{
    menu_t*             menu = &SaveDef;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    char        name[256];
#endif

    menu->lastOn = option;
    Hu_MenuCommand(MCMD_CLOSEFAST);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    SV_GetSaveGameFileName(option, name);
    G_LoadGame(name);
#else
    G_LoadGame(option);
#endif
}

/**
 * User wants to save. Start string input for Hu_MenuResponder
 */
void M_SaveSelect(int option, void* data)
{
    menu_t*             menu = &LoadDef;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    menu->lastOn = saveSlot = option;
    strcpy(saveOldString, savegamestrings[option]);
    if(!strcmp(savegamestrings[option], EMPTYSTRING))
        savegamestrings[option][0] = 0;
    saveCharIndex = strlen(savegamestrings[option]);
}

void M_StartMessage(char* string, void* routine, boolean input)
{
    messageResponse = 0;
    messageLastMenuActive = menuActive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuActive = true;
    typeInTime = 0;

    // Enable the message binding class
    DD_Executef(true, "activatebclass message");
}

void M_StopMessage(void)
{
    messageToPrint = 0;

    if(!messageLastMenuActive)
        Hu_MenuCommand(MCMD_CLOSEFAST);

    // Disable the message binding class
    DD_Executef(true, "deactivatebclass message");
}

void M_DrawMainMenu(void)
{
#if __JHEXEN__
    int         frame;

    frame = (menuTime / 5) % 7;

    DGL_Color4f( 1, 1, 1, menuAlpha);
    GL_DrawPatch_CS(88, 0, m_htic.lump);
    GL_DrawPatch_CS(37, 80, SkullBaseLump + (frame + 2) % 7);
    GL_DrawPatch_CS(278, 80, SkullBaseLump + frame);

#elif __JHERETIC__
    WI_DrawPatch(88, 0, 1, 1, 1, menuAlpha, &m_htic, NULL, false,
                 ALIGN_LEFT);

    DGL_Color4f( 1, 1, 1, menuAlpha);
    GL_DrawPatch_CS(40, 10, SkullBaseLump + (17 - frame));
    GL_DrawPatch_CS(232, 10, SkullBaseLump + frame);
#elif __JDOOM__ || __JDOOM64__
    WI_DrawPatch(94, 2, 1, 1, 1, menuAlpha, &m_doom,
                 NULL, false, ALIGN_LEFT);
#elif __JSTRIFE__
    menu_t     *menu = &MainDef;
    int         yoffset = 0;

    WI_DrawPatch(86, 2, 1, 1, 1, menuAlpha, W_GetNumForName("M_STRIFE"),
                 NULL, false, ALIGN_LEFT);

    WI_DrawPatch(menu->x, menu->y + yoffset, 1, 1, 1, menuAlpha,
                 W_GetNumForName("M_NGAME"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menuAlpha, W_GetNumForName("M_NGAME"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menuAlpha, W_GetNumForName("M_OPTION"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menuAlpha, W_GetNumForName("M_LOADG"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menuAlpha, W_GetNumForName("M_SAVEG"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menuAlpha, W_GetNumForName("M_RDTHIS"), NULL, false, ALIGN_LEFT);
    WI_DrawPatch(menu->x, menu->y + (yoffset+= menu->itemHeight), 1, 1, 1,
                 menuAlpha, W_GetNumForName("M_QUITG"), NULL, false, ALIGN_LEFT);
#endif
}

void M_DrawNewGameMenu(void)
{
    menu_t*             menu = &NewGameDef;
    M_DrawTitle("Choose Game Type", menu->y - 30);
}

void M_DrawReadThis(void)
{
#if __JDOOM__
    // The background is handled elsewhere, just draw the cursor.
    GL_DrawPatch(298, 160, cursorst[whichSkull].lump);
#endif
}

#if __JHEXEN__
void M_DrawClassMenu(void)
{
    menu_t     *menu = &ClassDef;
    playerclass_t    class;
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

    M_WriteText2(34, 24, "CHOOSE CLASS:", huFontB, menu->color[0],
                 menu->color[1], menu->color[2], menuAlpha);

    class = (playerclass_t) currentMenu->items[itemOn].option;

    DGL_Color4f( 1, 1, 1, menuAlpha);
    GL_DrawPatch_CS(174, 8, W_GetNumForName(boxLumpName[class]));
    GL_DrawPatch_CS(174 + 24, 8 + 12,
                    W_GetNumForName(walkLumpName[class]) +
                    ((menuTime >> 3) & 3));
}
#endif

#if __JDOOM__ || __JHERETIC__
void M_DrawEpisode(void)
{
#ifdef __JDOOM__
    menu_t             *menu = &EpiDef;
#endif

#ifdef __JHERETIC__
    M_DrawTitle("WHICH EPISODE?", 4);
#elif __JDOOM__
    WI_DrawPatch(50, 40, menu->color[0], menu->color[1], menu->color[2], menuAlpha,
                 &m_episod, "{case}Which Episode{scaley=1.25,y=-3}?",
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
#elif __JDOOM__ || __JDOOM64__
    menu_t *menu = &SkillDef;
    WI_DrawPatch(96, 14, menu->color[0], menu->color[1], menu->color[2], menuAlpha,
                 &m_newg, "{case}NEW GAME", true, ALIGN_LEFT);
    WI_DrawPatch(54, 38, menu->color[0], menu->color[1], menu->color[2], menuAlpha,
                 &m_skill, "{case}Choose Skill Level:", true,
                 ALIGN_LEFT);
#endif
}

void M_DrawFilesMenu(void)
{
    // clear out the quicksave/quickload stuff
    quicksave = 0;
    quickload = 0;
}

/**
 * Read the strings from the savegame files.
 */
void M_ReadSaveStrings(void)
{
    int         i;
    char        name[256];

    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        SV_GetSaveGameFileName(i, name);
        if(!SV_GetSaveDescription(name, savegamestrings[i]))
        {
            strcpy(savegamestrings[i], EMPTYSTRING);
            LoadItems[i].type = ITT_INERT;
        }
        else
        {
            LoadItems[i].type = ITT_EFUNC;
        }
    }
}

#if __JDOOM__ || __JDOOM64__
#define SAVEGAME_BOX_YOFFSET 3
#else
#define SAVEGAME_BOX_YOFFSET 5
#endif

void M_DrawLoad(void)
{
    int                 i;
    menu_t*             menu = &LoadDef;
    int                 width =
        M_StringWidth("a", menu->font) * (SAVESTRINGSIZE - 1);

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    M_DrawTitle("LOAD GAME", 4);
#else
    WI_DrawPatch(72, 24, menu->color[0], menu->color[1], menu->color[2], menuAlpha,
                 &m_loadg, "{case}LOAD GAME", true, ALIGN_LEFT);
#endif
    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        M_DrawSaveLoadBorder(LoadDef.x - 8, SAVEGAME_BOX_YOFFSET + LoadDef.y +
                             (menu->itemHeight * i), width + 8);
        M_WriteText2(LoadDef.x, SAVEGAME_BOX_YOFFSET + LoadDef.y + 1 +
                     (menu->itemHeight * i),
                     savegamestrings[i], menu->font, menu->color[0], menu->color[1],
                     menu->color[2], menuAlpha);
    }
}

void M_DrawSave(void)
{
    int                 i;
    menu_t*             menu = &SaveDef;
    int                 width =
        M_StringWidth("a", menu->font) * (SAVESTRINGSIZE - 1);

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    M_DrawTitle("SAVE GAME", 4);
#else
    WI_DrawPatch(72, 24, menu->color[0], menu->color[1], menu->color[2], menuAlpha,
                 &m_saveg, "{case}SAVE GAME", true, ALIGN_LEFT);
#endif

    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        M_DrawSaveLoadBorder(SaveDef.x - 8, SAVEGAME_BOX_YOFFSET + SaveDef.y +
                             (menu->itemHeight * i), width + 8);
        M_WriteText2(SaveDef.x, SAVEGAME_BOX_YOFFSET + SaveDef.y + 1 +
                     (menu->itemHeight * i),
                     savegamestrings[i], menu->font, menu->color[0], menu->color[1],
                     menu->color[2], menuAlpha);
    }

    if(saveStringEnter)
    {
        size_t              len = strlen(savegamestrings[saveSlot]);

        //if(len < SAVESTRINGSIZE)
        {
            i = M_StringWidth(savegamestrings[saveSlot], huFontA);
            M_WriteText2(SaveDef.x + i, SAVEGAME_BOX_YOFFSET + SaveDef.y + 1 +
                         (menu->itemHeight * saveSlot), "_", huFontA,
                         menu->color[0], menu->color[1], menu->color[2],
                         menuAlpha);
        }
    }
}

/**
 * Draw border for the savegame description
 */
void M_DrawSaveLoadBorder(int x, int y, int width)
{
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    DGL_Color4f(1, 1, 1, menuAlpha);
    GL_DrawPatch_CS(x - 8, y - 4, dpFSlot.lump);
#else
    DGL_Color4f(1, 1, 1, menuAlpha);

    GL_SetPatch(dpLSLeft.lump, DGL_CLAMP, DGL_CLAMP);
    GL_DrawRect(x, y - 3, dpLSLeft.width, dpLSLeft.height, 1, 1, 1, menuAlpha);
    GL_SetPatch(dpLSRight.lump, DGL_CLAMP, DGL_CLAMP);
    GL_DrawRect(x + width - dpLSRight.width, y - 3, dpLSRight.width,
                dpLSRight.height, 1, 1, 1, menuAlpha);

    GL_SetPatch(dpLSCntr.lump, DGL_REPEAT, DGL_REPEAT);
    GL_DrawRectTiled(x + dpLSLeft.width, y - 3,
                     width - dpLSLeft.width - dpLSRight.width,
                     14, 8, 14);
#endif
}

/**
 * \fixme Hu_MenuResponder calls this when user is finished.
 * not in jHexen it doesn't...
 */
void M_DoSave(int slot)
{
    G_SaveGame(slot, savegamestrings[slot]);
    Hu_MenuCommand(MCMD_CLOSE);

    // Picked a quicksave slot yet?
    if(quickSaveSlot == -2)
        quickSaveSlot = slot;
}

boolean M_QuickSaveResponse(int ch, void *data)
{
    if(messageResponse == 1) // Yes
    {
        M_DoSave(quickSaveSlot);
        S_LocalSound(menusnds[1], NULL);
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSE);
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSE);
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
    return false;
}

/**
 * Called via the bindings mechanism when a player wishes to save their
 * game to a preselected save slot.
 */
static void M_QuickSave(void)
{
    player_t*               player = &players[CONSOLEPLAYER];

    if(player->playerState == PST_DEAD || G_GetGameState() != GS_LEVEL ||
       Get(DD_PLAYBACK))
    {
        M_StartMessage(SAVEDEAD, NULL, false);
        return;
    }

    if(quickSaveSlot < 0)
    {
        Hu_MenuCommand(MCMD_OPEN);
        M_ReadSaveStrings();
        M_SetupNextMenu(&SaveDef);
        quickSaveSlot = -2; // Means to pick a slot now.
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

boolean M_QuickLoadResponse(int ch, void *data)
{
    if(messageResponse == 1) // Yes
    {
        M_LoadSelect(quickSaveSlot, NULL);
        S_LocalSound(menusnds[1], NULL);
        M_StopMessage();

        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSE);
        S_LocalSound(menusnds[1], NULL);

        return true;
    }

    return false;
}

static void M_QuickLoad(void)
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

#if !__JDOOM64__
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

# if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
void M_ReadThis3(int option, void *data)
{
    option = 0;
    M_SetupNextMenu(&ReadDef3);
}
# endif

void M_FinishReadThis(int option, void *data)
{
    option = 0;
    M_SetupNextMenu(&MainDef);
}
#endif

void M_DrawOptions(void)
{
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    WI_DrawPatch(88, 0, 1, 1, 1, menuAlpha, &m_htic, NULL, false,
                 ALIGN_LEFT);

    M_DrawTitle("OPTIONS", 44);
#else
# if __JDOOM64__
    WI_DrawPatch(160, 44, cfg.menuColor[0], cfg.menuColor[1], cfg.menuColor[2],
                 menuAlpha, 0, "{case}OPTIONS",
                 true, ALIGN_CENTER);
#else
    WI_DrawPatch(160, 44, cfg.menuColor[0], cfg.menuColor[1], cfg.menuColor[2],
                 menuAlpha, &m_optttl, "{case}OPTIONS",
                 true, ALIGN_CENTER);
# endif
#endif
}

void M_DrawOptions2(void)
{
    menu_t     *menu = &Options2Def;

#if __JDOOM__ || __JDOOM64__
    M_DrawTitle("SOUND OPTIONS", menu->y - 20);

    MN_DrawSlider(menu, 0, 16, SFXVOLUME);
    MN_DrawSlider(menu, 1, 16, MUSICVOLUME);
#else
    M_DrawTitle("SOUND OPTIONS", 0);

    MN_DrawSlider(menu, 1, 16, SFXVOLUME);
    MN_DrawSlider(menu, 4, 16, MUSICVOLUME);
#endif
}

void M_DrawGameplay(void)
{
    int         idx = 0;
    menu_t     *menu = &GameplayDef;

#if __JHEXEN__
    M_DrawTitle("GAMEPLAY", 0);
    M_WriteMenuText(menu, idx++, yesno[cfg.alwaysRun != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.lookSpring != 0]);
    M_WriteMenuText(menu, idx++, yesno[!cfg.noAutoAim]);
#else

# if __JHERETIC__
    M_DrawTitle("GAMEPLAY", 4);
# else
    M_DrawTitle("GAMEPLAY", menu->y - 20);
# endif

    M_WriteMenuText(menu, idx++, yesno[cfg.alwaysRun != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.lookSpring != 0]);
    M_WriteMenuText(menu, idx++, yesno[!cfg.noAutoAim]);
    M_WriteMenuText(menu, idx++, yesno[cfg.jumpEnabled != 0]);
# if __JDOOM64__
    M_WriteMenuText(menu, idx++, yesno[cfg.weaponRecoil != 0]);
    idx = 7;
# else
    idx = 6;
# endif
# if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, yesno[cfg.anyBossDeath != 0]);
#   if !__JDOOM64__
    M_WriteMenuText(menu, idx++, yesno[cfg.raiseGhosts != 0]);
#   endif
    M_WriteMenuText(menu, idx++, yesno[cfg.maxSkulls != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.allowSkullsInWalls != 0]);
# endif
# if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, yesno[cfg.monstersStuckInDoors != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.avoidDropoffs != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.fallOff != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.slidingCorpses != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.moveBlock != 0]);
    M_WriteMenuText(menu, idx++, yesno[cfg.wallRunNorthOnly != 0]);
# endif
# if __JDOOM__ || __JDOOM64__
    M_WriteMenuText(menu, idx++, yesno[cfg.zombiesCanExit != 0]);
# endif
# if __JDOOM__
#  if !__WOLFTC__
    M_WriteMenuText(menu, idx++, yesno[cfg.fixOuchFace != 0]);
#  endif
# endif
#endif
}

void M_DrawWeaponMenu(void)
{
    menu_t     *menu = &WeaponDef;
    int         i = 0;
    char       *autoswitch[] = { "NEVER", "IF BETTER", "ALWAYS" };
#if __JHEXEN__
    char       *weaponids[] = { "First", "Second", "Third", "Fourth"};
#endif

#if __JDOOM__ || __JDOOM64__
    byte berserkAutoSwitch = cfg.berserkAutoSwitch;
#endif

    M_DrawTitle("WEAPONS", menu->y - 20);

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        M_WriteMenuText(menu, 4+i, GET_TXT(TXT_WEAPON1 + cfg.weaponOrder[i]));
#elif __JHERETIC__
        /**
         * \fixme We should allow different weapon preferences per player
         * class. However, since the only other class in jHeretic is the
         * chicken which has only 1 weapon anyway -we'll just show the
         * names of the player's weapons for now.
         */
        M_WriteMenuText(menu, 4+i, GET_TXT(TXT_TXT_WPNSTAFF + cfg.weaponOrder[i]));
#elif __JHEXEN__
        /**
         * \fixme We should allow different weapon preferences per player
         * class. Then we can show the real names here.
         */
        M_WriteMenuText(menu, 4+i, weaponids[cfg.weaponOrder[i]]);
#endif
    }

#if __JHEXEN__
    M_WriteMenuText(menu, 8, yesno[cfg.weaponNextMode]);
    M_WriteMenuText(menu, 11, autoswitch[cfg.weaponAutoSwitch]);
    M_WriteMenuText(menu, 12, yesno[cfg.noWeaponAutoSwitchIfFiring]);
    M_WriteMenuText(menu, 13, autoswitch[cfg.ammoAutoSwitch]);
#elif __JHERETIC__
    M_WriteMenuText(menu, 12, yesno[cfg.weaponNextMode]);
    M_WriteMenuText(menu, 15, autoswitch[cfg.weaponAutoSwitch]);
    M_WriteMenuText(menu, 16, yesno[cfg.noWeaponAutoSwitchIfFiring]);
    M_WriteMenuText(menu, 17, autoswitch[cfg.ammoAutoSwitch]);
#elif __JDOOM64__
    M_WriteMenuText(menu, 14, yesno[cfg.weaponNextMode]);
    M_WriteMenuText(menu, 17, autoswitch[cfg.weaponAutoSwitch]);
    M_WriteMenuText(menu, 18, yesno[cfg.noWeaponAutoSwitchIfFiring]);
    M_WriteMenuText(menu, 19, autoswitch[cfg.ammoAutoSwitch]);
    M_WriteMenuText(menu, 20, yesno[berserkAutoSwitch != 0]);
#elif __JDOOM__
    M_WriteMenuText(menu, 13, yesno[cfg.weaponNextMode]);
    M_WriteMenuText(menu, 16, autoswitch[cfg.weaponAutoSwitch]);
    M_WriteMenuText(menu, 17, yesno[cfg.noWeaponAutoSwitchIfFiring]);
    M_WriteMenuText(menu, 18, autoswitch[cfg.ammoAutoSwitch]);
    M_WriteMenuText(menu, 19, yesno[berserkAutoSwitch != 0]);
#endif
}

void M_WeaponOrder(int option, void *data)
{
    int         choice = option >> NUM_WEAPON_TYPES;
    int         temp;

    if(option & RIGHT_DIR)
    {
        if(choice < NUM_WEAPON_TYPES-1)
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

void M_WeaponAutoSwitch(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.weaponAutoSwitch < 2)
            cfg.weaponAutoSwitch++;
    }
    else if(cfg.weaponAutoSwitch > 0)
        cfg.weaponAutoSwitch--;
}

void M_AmmoAutoSwitch(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.ammoAutoSwitch < 2)
            cfg.ammoAutoSwitch++;
    }
    else if(cfg.ammoAutoSwitch > 0)
        cfg.ammoAutoSwitch--;
}

void M_DrawHUDMenu(void)
{
#if __JDOOM__ || __JDOOM64__
    int                 idx;
#endif
    menu_t*             menu = &HUDDef;
    char*               xhairnames[7] = {
        "NONE", "CROSS", "ANGLES", "SQUARE", "OPEN SQUARE", "DIAMOND", "V"
    };

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    char       *token;

    M_DrawTitle("hud options", 4);

    // Draw the page arrows.
    DGL_Color4f( 1, 1, 1, menuAlpha);
    token = (!menu->firstItem || menuTime & 8) ? "invgeml2" : "invgeml1";
    GL_DrawPatch_CS(menu->x -20, menu->y - 16, W_GetNumForName(token));
    token = (menu->firstItem + menu->numVisItems >= menu->itemCount ||
             menuTime & 8) ? "invgemr2" : "invgemr1";
    GL_DrawPatch_CS(312 - (menu->x -20), menu->y - 16, W_GetNumForName(token));
#else
    M_DrawTitle("HUD OPTIONS", menu->y - 20);
#endif

#if __JHEXEN__ || __JSTRIFE__
    if(menu->firstItem < menu->numVisItems)
    {
        M_WriteMenuText(menu, 0, yesno[cfg.msgShow != 0]);
        M_WriteMenuText(menu, 1, xhairnames[cfg.xhair]);
        MN_DrawSlider(menu, 3, 9, cfg.xhairSize);
        MN_DrawSlider(menu, 6, 11, cfg.screenBlocks - 3);
        MN_DrawSlider(menu, 9, 20, cfg.statusbarScale - 1);
        MN_DrawSlider(menu, 12, 11, cfg.statusbarAlpha * 10 + .25f);
    }
    else
    {
        M_WriteMenuText(menu, 16, yesno[cfg.hudShown[HUD_MANA] != 0]);
        M_WriteMenuText(menu, 17, yesno[cfg.hudShown[HUD_HEALTH]]);
        M_WriteMenuText(menu, 18, yesno[cfg.hudShown[HUD_ARTI]]);
        MN_DrawColorBox(menu,19, cfg.hudColor[0], cfg.hudColor[1],
                        cfg.hudColor[2], menuAlpha);
        MN_DrawSlider(menu, 21, 10, cfg.hudScale * 10 - 3 + .5f);
    }
#elif __JHERETIC__
    if(menu->firstItem < menu->numVisItems)
    {
        M_WriteMenuText(menu, 0, yesno[cfg.msgShow != 0]);
        M_WriteMenuText(menu, 1, xhairnames[cfg.xhair]);
        MN_DrawSlider(menu, 3, 9, cfg.xhairSize);
        MN_DrawSlider(menu, 6, 11, cfg.screenBlocks - 3);
        MN_DrawSlider(menu, 9, 20, cfg.statusbarScale - 1);
        MN_DrawSlider(menu, 12, 11, cfg.statusbarAlpha * 10 + .25f);
    }
    else
    {
        M_WriteMenuText(menu, 16, yesno[cfg.hudShown[HUD_AMMO]]);
        M_WriteMenuText(menu, 17, yesno[cfg.hudShown[HUD_ARMOR]]);
        M_WriteMenuText(menu, 18, yesno[cfg.hudShown[HUD_ARTI]]);
        M_WriteMenuText(menu, 19, yesno[cfg.hudShown[HUD_HEALTH]]);
        M_WriteMenuText(menu, 20, yesno[cfg.hudShown[HUD_KEYS]]);
        MN_DrawColorBox(menu, 21, cfg.hudColor[0], cfg.hudColor[1],
                        cfg.hudColor[2], menuAlpha);
        MN_DrawSlider(menu, 23, 10, cfg.hudScale * 10 - 3 + .5f);
    }
#elif __JDOOM__ || __JDOOM64__
    idx = 0;
    M_WriteMenuText(menu, idx++, yesno[cfg.hudShown[HUD_AMMO]]);
    M_WriteMenuText(menu, idx++, yesno[cfg.hudShown[HUD_ARMOR]]);
# if __JDOOM64__
    M_WriteMenuText(menu, idx++, yesno[cfg.hudShown[HUD_POWER]]);
# else
    M_WriteMenuText(menu, idx++, yesno[cfg.hudShown[HUD_FACE]]);
# endif
    M_WriteMenuText(menu, idx++, yesno[cfg.hudShown[HUD_HEALTH]]);
    M_WriteMenuText(menu, idx++, yesno[cfg.hudShown[HUD_KEYS]]);
# if __JDOOM__
# if !__WOLFTC__
    M_WriteMenuText(menu, idx++, yesno[cfg.hudKeysCombine]);
# endif
# endif
    MN_DrawSlider(menu, idx++, 10, cfg.hudScale * 10 - 3 + .5f);
    MN_DrawColorBox(menu, idx++, cfg.hudColor[0], cfg.hudColor[1],
                    cfg.hudColor[2], menuAlpha);
    M_WriteMenuText(menu, idx++, yesno[cfg.msgShow != 0]);
    M_WriteMenuText(menu, idx++, xhairnames[cfg.xhair]);
    MN_DrawSlider(menu, idx++, 9, cfg.xhairSize);
    MN_DrawSlider(menu, idx++, 11, cfg.screenBlocks - 3 );
# if !__JDOOM64__
    MN_DrawSlider(menu, idx++, 20, cfg.statusbarScale - 1);
    MN_DrawSlider(menu, idx++, 11, cfg.statusbarAlpha * 10 + .25f);
# endif
#endif
}

void M_FloatMod10(float *variable, int option)
{
    int         val = (*variable + .05f) * 10;

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
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
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
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
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


#if __JDOOM__ || __JDOOM64__
void M_XhairR(int option, void *data)
{
    int         val = cfg.xhairColor[0];

    val += (option == RIGHT_DIR ? 17 : -17);
    if(val < 0)
        val = 0;
    if(val > 255)
        val = 255;
    cfg.xhairColor[0] = val;
}

void M_XhairG(int option, void *data)
{
    int         val = cfg.xhairColor[1];

    val += (option == RIGHT_DIR ? 17 : -17);
    if(val < 0)
        val = 0;
    if(val > 255)
        val = 255;
    cfg.xhairColor[1] = val;
}

void M_XhairB(int option, void *data)
{
    int         val = cfg.xhairColor[2];

    val += (option == RIGHT_DIR ? 17 : -17);
    if(val < 0)
        val = 0;
    if(val > 255)
        val = 255;
    cfg.xhairColor[2] = val;
}

void M_XhairAlpha(int option, void *data)
{
    int         val = cfg.xhairColor[3];

    val += (option == RIGHT_DIR ? 17 : -17);
    if(val < 0)
        val = 0;
    if(val > 255)
        val = 255;
    cfg.xhairColor[3] = val;
}
#endif

#if __JDOOM64__
void M_WeaponRecoil(int option, void *data)
{
    cfg.weaponRecoil = !cfg.weaponRecoil;
}
#endif

#if !__JDOOM64__
void M_SizeStatusBar(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.statusbarScale < 20)
            cfg.statusbarScale++;
    }
    else if(cfg.statusbarScale > 1)
        cfg.statusbarScale--;

    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE);

    R_SetViewSize(cfg.screenBlocks);
}

void M_StatusBarAlpha(int option, void *data)
{
    M_FloatMod10(&cfg.statusbarAlpha, option);

    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE);
}
#endif

void M_WGCurrentColor(int option, void *data)
{
    M_FloatMod10(data, option);
}

void M_NewGame(int option, void* data)
{
    if(IS_NETGAME)
    {
        M_StartMessage(NEWGAME, NULL, false);
        return;
    }

#if __JHEXEN__
    M_SetupNextMenu(&ClassDef);
#elif __JHERETIC__
    M_SetupNextMenu(&EpiDef);
#elif __JDOOM64__ || __JSTRIFE__
    M_SetupNextMenu(&SkillDef);
#else // __JDOOM__ || __WOLFTC__
    if(gameMode == commercial)
        M_SetupNextMenu(&SkillDef);
    else
        M_SetupNextMenu(&EpiDef);
#endif
}

boolean M_QuitResponse(int option, void *data)
{
#if __JDOOM__ || __JDOOM64__
    static int  quitsounds[8] = {
        SFX_PLDETH,
        SFX_DMPAIN,
        SFX_POPAIN,
        SFX_SLOP,
        SFX_TELEPT,
        SFX_POSIT1,
        SFX_POSIT3,
        SFX_SGTATK
    };
    static int  quitsounds2[8] = {
        SFX_VILACT,
        SFX_GETPOW,
# if __JDOOM64__
        SFX_PEPAIN,
# else
        SFX_BOSCUB,
# endif
        SFX_SLOP,
        SFX_SKESWG,
        SFX_KNTDTH,
        SFX_BSPACT,
        SFX_SGTATK
    };
#endif

    if(messageResponse == 1)
    {
        // No need to close down the menu question after this.
#if __JDOOM__ || __JDOOM64__
        // Play an exit sound if it is enabled.
        if(cfg.menuQuitSound && !IS_NETGAME)
        {
            if(!quitYet)
            {
                if(gameMode == commercial)
                    S_LocalSound(quitsounds2[(GAMETIC >> 2) & 7], NULL);
                else
                    S_LocalSound(quitsounds[(GAMETIC >> 2) & 7], NULL);

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
        Hu_MenuCommand(MCMD_CLOSE);
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
    return false;
}

void M_QuitDOOM(int option, void *data)
{
    Con_Open(false);

#if __JDOOM__ || __JDOOM64__
    sprintf(endstring, "%s\n\n%s",
            endmsg[(GAMETIC % (NUM_QUITMESSAGES + 1))], DOSY);
#else
    sprintf(endstring, "%s\n\n%s", endmsg[0], DOSY);
#endif

    quitAsk = 1;
    M_StartMessage(endstring, M_QuitResponse, true);
}

boolean M_EndGameResponse(int option, void *data)
{
    if(messageResponse == 1)
    {
        currentMenu->lastOn = itemOn;
        menuAlpha = menuTargetAlpha = 0;
        menuFogData.alpha = menuFogData.targetAlpha = 0;
        fadingOut = false;
        menuActive = false;
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSE);
        G_StartTitle();

        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSE);
        S_LocalSound(menusnds[1], NULL);

        return true;
    }

    return false;
}

void M_EndGame(int option, void *data)
{
    option = 0;
    if(!userGame)
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

void M_ChangeMessages(int option, void *data)
{
    cfg.msgShow = !cfg.msgShow;
    P_SetMessage(players + CONSOLEPLAYER, !cfg.msgShow ? MSGOFF : MSGON, true);
}

void M_HUDScale(int option, void *data)
{
    int         val = (cfg.hudScale + .05f) * 10;

    if(option == RIGHT_DIR)
    {
        if(val < 12)
            val++;
    }
    else if(val > 3)
        val--;

    cfg.hudScale = val / 10.0f;
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE);
}

#if __JDOOM__ || __JDOOM64__
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

/**
 * Called via the menu or the control bindings mechanism when the player
 * wishes to save their game.
 */
void M_SaveGame(int option, void *data)
{
    player_t *player = &players[CONSOLEPLAYER];

    if(player->playerState == PST_DEAD || G_GetGameState() != GS_LEVEL ||
       Get(DD_PLAYBACK))
    {
        M_StartMessage(SAVEDEAD, NULL, false);
        return;
    }

    if(IS_CLIENT)
    {
#if __JDOOM__ || __JDOOM64__
        M_StartMessage(GET_TXT(TXT_SAVENET), NULL, false);
#endif
        return;
    }

    Hu_MenuCommand(MCMD_OPEN);
    M_SetupNextMenu(&SaveDef);;
    M_ReadSaveStrings();
}

void M_ChooseClass(int option, void *data)
{
#if __JHEXEN__
    if(IS_NETGAME)
    {
        P_SetMessage(&players[CONSOLEPLAYER],
                     "YOU CAN'T START A NEW GAME FROM WITHIN A NETGAME!", false);
        return;
    }

    MenuPClass = option;
    switch(MenuPClass)
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

#if __JDOOM__ || __JHERETIC__ || __WOLFTC__
void M_Episode(int option, void *data)
{
#if __JHERETIC__
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
#else
    if((gameMode == shareware) && option)
    {
        M_StartMessage(SWSTRING, NULL, false);
        M_SetupNextMenu(&ReadDef1);
        return;
    }

    // Yet another hack...
    if((gameMode == registered) && (option > 2))
    {
        Con_Message("M_Episode: 4th episode requires Ultimate DOOM\n");
        option = 0;
    }

    epi = option;
    M_SetupNextMenu(&SkillDef);
#endif
}
#endif

#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__ || __WOLFTC__
boolean M_VerifyNightmare(int option, void *data)
{
#if __JDOOM__ || __JHERETIC__ || __JSTRIFE__
    if(messageResponse == 1) // Yes
    {
        M_StopMessage();
        Hu_MenuCommand(MCMD_CLOSEFAST);

#ifdef __JHERETIC__
        G_DeferedInitNew(SM_NIGHTMARE, MenuEpisode, 1);
#elif __JDOOM__
        G_DeferedInitNew(SM_NIGHTMARE, epi + 1, 1);
#elif __JSTRIFE__
        G_DeferredNewGame(SM_NIGHTMARE);
#endif
        return true;
    }
    else if(messageResponse == -1 || messageResponse == -2)
    {
        M_StopMessage();
        S_LocalSound(menusnds[1], NULL);
        return true;
    }
#endif
    return false;
}
#endif

void M_ChooseSkill(int option, void *data)
{
#if __JHEXEN__
    cfg.playerClass[CONSOLEPLAYER] = MenuPClass;
    G_DeferredNewGame(option);
#else
# if __JDOOM__ || __JSTRIFE__
    if(option == SM_NIGHTMARE)
    {
#  if __JSTRIFE__
        M_StartMessage("u nuts? FIXME!!!", M_VerifyNightmare, true);
#  else
        M_StartMessage(NIGHTMARE, M_VerifyNightmare, true);
#  endif
        return;
    }
# endif
#endif

    Hu_MenuCommand(MCMD_CLOSEFAST);

#if __JHERETIC__
    G_DeferedInitNew(option, MenuEpisode, 1);
#elif __JDOOM__
    G_DeferedInitNew(option, epi + 1, 1);
#elif __JDOOM64__
    G_DeferedInitNew(option, 1, 1);
#elif __JSTRIFE__
    G_DeferredNewGame(option);
#endif
}

void M_SfxVol(int option, void *data)
{
    int         vol = SFXVOLUME;

    if(option == RIGHT_DIR)
    {
        if(vol < 15)
            vol++;
    }
    else
    {
        if(vol > 0)
            vol--;
    }

    Set(DD_SFX_VOLUME, vol * 17);
}

void M_MusicVol(int option, void *data)
{
    int         vol = MUSICVOLUME;

    if(option == RIGHT_DIR)
    {
        if(vol < 15)
            vol++;
    }
    else
    {
        if(vol > 0)
            vol--;
    }

    Set(DD_MUSIC_VOLUME, vol * 17);
}

void M_SizeDisplay(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
#if __JDOOM64__
        if(cfg.screenBlocks < 11)
#else
        if(cfg.screenBlocks < 13)
#endif
        {
            cfg.screenBlocks++;
        }
    }
    else if(cfg.screenBlocks > 3)
    {
        cfg.screenBlocks--;
    }

    R_SetViewSize(cfg.screenBlocks);
}

void M_OpenDCP(int option, void *data)
{
#define NUM_PANEL_NAMES 3
    static const char *panelNames[] = {
        "panel",
        "panel audio",
        "panel input",
        "panel controls"
    };
    int         idx = option;

    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
        idx = 0;

    Hu_MenuCommand(MCMD_CLOSEFAST);
    DD_Execute(true, panelNames[idx]);

#undef NUM_PANEL_NAMES
}

void MN_DrawColorBox(const menu_t *menu, int index, float r, float g,
                     float b, float a)
{
    int         x = menu->x + 4;
    int         y = menu->y + menu->itemHeight * (index - menu->firstItem) + 3;

    M_DrawColorBox(x, y, r, g, b, a);
}

/**
 * Draws a menu slider control
 */
void MN_DrawSlider(const menu_t *menu, int item, int width, int slot)
{
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    int         x;
    int         y;

    x = menu->x + 24;
    y = menu->y + 2 + (menu->itemHeight * (item  - menu->firstItem));

    M_DrawSlider(x, y, width, slot, menuAlpha);
#else
    int         x =0, y =0;
    int         height = menu->itemHeight - 1;
    float       scale = height / 13.0f;

    if(menu->items[item].text)
        x = M_StringWidth(menu->items[item].text, menu->font);

    x += menu->x + 6;
    y = menu->y + menu->itemHeight * item;

    M_DrawSlider(x, y, width, height, slot, menuAlpha);
#endif
}

/**
 * Routes menu commands, actions and navigation.
 */
DEFCC(CCmdMenuAction)
{
    int         mode = 0;

    if(!menuActive)
    {
        if(!stricmp(argv[0], "menu") && !chatOn)   // Open menu
        {
            Hu_MenuCommand(MCMD_OPEN);
            return true;
        }
    }
    else
    {
        // Determine what state the menu is in currently
        if(ActiveEdit)
            mode = 1;
        else if(widgetEdit)
            mode = 2;
        else if(saveStringEnter)
            mode = 3;
#if !__JDOOM64__
        else
        {
            if(currentMenu == &ReadDef1 || currentMenu == &ReadDef2
# if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
               || currentMenu == &ReadDef3
# endif
               )
                mode = 4;
        }
#endif

        if(!stricmp(argv[0], "menuup"))
        {
            // If a message is being displayed, menuup does nothing
            if(messageToPrint)
                return true;

            switch(mode)
            {
            case 2: // Widget edit
                if(!widgetEdit)
                    break;
                // Fall through.

            case 0: // Menu nav
                Hu_MenuCommand(MCMD_NAV_UP);
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

            switch(mode)
            {
            case 2: // Widget edit
                if(!widgetEdit)
                    break;
                // Fall through.

            case 0: // Menu nav
                Hu_MenuCommand(MCMD_NAV_DOWN);
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

            switch(mode)
            {
            case 0: // Menu nav
            case 2: // Widget edit
                Hu_MenuCommand(MCMD_NAV_LEFT);
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

            switch(mode)
            {
            case 0: // Menu nav
            case 2: // Widget edit
                Hu_MenuCommand(MCMD_NAV_RIGHT);
                break;

            default:
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menudelete"))
        {
            if(messageToPrint)
                return true;

            if(!mode)
            {
                Hu_MenuCommand(MCMD_DELETE);
            }
            return true;
        }
        else if(!stricmp(argv[0], "menuselect"))
        {
            // If a message is being displayed, menuselect does nothing
            if(messageToPrint)
                return true;

            switch(mode)
            {
            case 4: // In helpscreens
            case 0: // Menu nav
                Hu_MenuCommand(MCMD_SELECT);
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

                widgetEdit = false;
                S_LocalSound(menusnds[5], NULL);
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
            int         c;

            // If a message is being displayed, menucancel does nothing
            if(messageToPrint)
                return true;

            switch(mode)
            {
            case 0: // Menu nav: Previous menu
                Hu_MenuCommand(MCMD_NAV_OUT);
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
                widgetEdit = false;
                S_LocalSound(menusnds[1], NULL);
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
            switch(mode)
            {
            case 0: // Menu nav: Close menu
                if(messageToPrint)
                    M_StopMessage();

                currentMenu->lastOn = itemOn;
                Hu_MenuCommand(MCMD_CLOSE);
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
                widgetEdit = false;
                S_LocalSound(menusnds[1], NULL);
                break;

            case 3: // Save string edit: Cancel
                saveStringEnter = 0;
                strcpy(&savegamestrings[saveSlot][0], saveOldString);
                break;

            case 4: // In helpscreens: Exit and close menu
                M_SetupNextMenu(&MainDef);
                Hu_MenuCommand(MCMD_CLOSEFAST);
                break;
            }

            return true;
        }
    }

    // If a message is being displayed, hotkeys don't work
    if(messageToPrint)
        return true;

    // Hotkey menu shortcuts
#if !__JDOOM64__
    if(!stricmp(argv[0], "helpscreen"))    // F1
    {
        Hu_MenuCommand(MCMD_OPEN);
        menuTime = 0;
# if __JDOOM__
        if(gameMode == retail)
            currentMenu = &ReadDef2;
        else
# endif
            currentMenu = &ReadDef1;

        itemOn = 0;
        //S_LocalSound(menusnds[2], NULL);
    }
    else
#endif
        if(!stricmp(argv[0], "SaveGame"))    // F2
    {
        menuTime = 0;
        //S_LocalSound(menusnds[2], NULL);
        M_SaveGame(0, NULL);
    }
    else if(!stricmp(argv[0], "LoadGame"))
    {
        Hu_MenuCommand(MCMD_OPEN);
        menuTime = 0;
        //S_LocalSound(menusnds[2], NULL);
        M_LoadGame(0, NULL);
    }
    else if(!stricmp(argv[0], "SoundMenu"))    // F4
    {
        Hu_MenuCommand(MCMD_OPEN);
        menuTime = 0;
        currentMenu = &Options2Def;
        itemOn = 0;                // sfx_vol
        //S_LocalSound(menusnds[2], NULL);
    }
    else if(!stricmp(argv[0], "QuickSave"))    // F6
    {
        S_LocalSound(menusnds[2], NULL);
        menuTime = 0;
        M_QuickSave();
    }
    else if(!stricmp(argv[0], "EndGame"))    // F7
    {
        S_LocalSound(menusnds[2], NULL);
        menuTime = 0;
        M_EndGame(0, NULL);
    }
    else if(!stricmp(argv[0], "ToggleMsgs"))    // F8
    {
        menuTime = 0;
        M_ChangeMessages(0, NULL);
        S_LocalSound(menusnds[2], NULL);
    }
    else if(!stricmp(argv[0], "QuickLoad"))    // F9
    {
        S_LocalSound(menusnds[2], NULL);
        menuTime = 0;
        M_QuickLoad();
    }
    else if(!stricmp(argv[0], "quit"))    // F10
    {
        if(IS_DEDICATED)
            DD_Execute(true, "quit!");
        else
        {
            S_LocalSound(menusnds[2], NULL);
            menuTime = 0;
            M_QuitDOOM(0, NULL);
        }
    }
    else if(!stricmp(argv[0], "ToggleGamma"))    // F11
    {
        R_CycleGammaLevel();
    }

    return true;
}
