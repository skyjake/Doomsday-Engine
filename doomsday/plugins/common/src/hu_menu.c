/**\file hu_menu.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Common interactive menu: options, episode etc.
 * Sliders and icons. Kinda widget stuff.
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "m_argv.h"
#include "hu_log.h"
#include "hu_msg.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "x_hair.h"
#include "p_player.h"
#include "g_controls.h"
#include "p_saveg.h"
#include "g_common.h"
#include "hu_menu.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct cvarbutton_s {
    char            active;
    const char*     cvarname;
    const char*     yes;
    const char*     no;
    int             mask;
} cvarbutton_t;

typedef struct rgba_s {
    float*          r, *g, *b, *a;
} rgba_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void M_InitControlsMenu(void);
void M_ControlGrabDrawer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void M_SetMenu(mn_object_t* obj, int option);
void M_NewGame(mn_object_t* obj, int option);
#if __JDOOM__ || __JHERETIC__
void M_Episode(mn_object_t* obj, int option);
#endif
#if __JHEXEN__
void M_ChooseClass(mn_object_t* obj, int option);
#endif
void M_ChooseSkill(mn_object_t* obj, int option);
void M_LoadGame(mn_object_t* obj, int option);
void M_SaveGame(mn_object_t* obj, int option);
#if __JHEXEN__
void M_GameFiles(mn_object_t* obj, int option);
#endif
void M_EndGame(mn_object_t* obj, int option);
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void M_ReadThis(mn_object_t* obj, int option);
#endif

void M_OpenDCP(mn_object_t* obj, int option);
void M_ChangeMessages(mn_object_t* obj, int option);
void M_HUDInfo(mn_object_t* obj, int option);
void M_WeaponOrder(mn_object_t* obj, int option);
void M_HUDRed(mn_object_t* obj, int option);
void M_HUDGreen(mn_object_t* obj, int option);
void M_HUDBlue(mn_object_t* obj, int option);
void M_LoadSelect(mn_object_t* obj, int option);

static void M_QuickSave(void);
static void M_QuickLoad(void);

void M_DrawMainMenu(const mn_page_t* page, int x, int y);
void M_DrawNewGameMenu(const mn_page_t* page, int x, int y);
void M_DrawSkillMenu(const mn_page_t* page, int x, int y);
#if __JHEXEN__
void M_DrawClassMenu(const mn_page_t* page, int x, int y);
#endif
#if __JDOOM__ || __JHERETIC__
void M_DrawEpisode(const mn_page_t* page, int x, int y);
#endif
void M_DrawOptions(const mn_page_t* page, int x, int y);
void M_DrawOptions2(const mn_page_t* page, int x, int y);
void M_DrawGameplay(const mn_page_t* page, int x, int y);
void M_DrawHUDMenu(const mn_page_t* page, int x, int y);
#if __JHERETIC__ || __JHEXEN__
void M_DrawInventoryMenu(const mn_page_t* page, int x, int y);
#endif
void M_DrawWeaponMenu(const mn_page_t* page, int x, int y);
void M_DrawLoad(const mn_page_t* page, int x, int y);
void M_DrawSave(const mn_page_t* page, int x, int y);
void M_DrawFilesMenu(const mn_page_t* page, int x, int y);

void M_DoSaveGame(mn_object_t* obj);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void drawColorWidget(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char* weaponNames[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if __JDOOM__ || __JDOOM64__
/// The end message strings will be initialized in Hu_MenuInit().
char* endmsg[NUM_QUITMESSAGES + 1];
#endif

char endstring[160];

static char* yesno[2] = {"NO", "YES"};

#if __JDOOM__ || __JHERETIC__
int epi;
#endif

int menu_color = 0;
float menu_glitter = 0;
float menu_shadow = 0;
float skull_angle = 0;

int frame; // Used by any graphic animations that need to be pumped.
int mnTime;

short mnFocusObjectIndex; // Menu obj skull is on.
short mnPreviousFocusObjectIndex; // Menu obj skull was last on (for restoring when leaving widget control).
short skullAnimCounter; // Skull animation counter.
short whichSkull; // Which skull to draw.

cvarbutton_t mnCVarButtons[] = {
    { 0, "ctl-aim-noauto" },
#if __JHERETIC__ || __JHEXEN__
    { 0, "ctl-inventory-mode" },
    { 0, "ctl-inventory-use-immediate" },
    { 0, "ctl-inventory-use-next" },
    { 0, "ctl-inventory-wrap" },
#endif
    { 0, "ctl-look-spring" },
    { 0, "ctl-run" },
#if __JDOOM__ || __JDOOM64__
    { 0, "game-anybossdeath666" },
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { 0, "game-corpse-sliding" },
#endif
#if __JDOOM__ || __JDOOM64__
    { 0, "game-maxskulls" },
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { 0, "game-monsters-stuckindoors" },
    { 0, "game-objects-clipping" },
    { 0, "game-objects-falloff" },
    { 0, "game-objects-neverhangoverledges" },
    { 0, "game-player-wallrun-northonly" },
#endif
#if __JDOOM__
    { 0, "game-raiseghosts" },
#endif
#if __JDOOM__ || __JDOOM64__
    { 0, "game-skullsinwalls" },
    { 0, "game-zombiescanexit" },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { 0, "hud-ammo" },
    { 0, "hud-armor" },
#endif
#if __JHERETIC__ || __JHEXEN__
    { 0, "hud-currentitem" },
#endif
#if __JDOOM__
    { 0, "hud-face" },
    { 0, "hud-face-ouchfix" },
#endif
    { 0, "hud-health" },
#if __JHERETIC__ || __JHEXEN__
    { 0, "hud-inventory-slot-showempty" },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { 0, "hud-keys" },
#endif
#if __JDOOM__
    { 0, "hud-keys-combine" },
#endif
#if __JHEXEN__
    { 0, "hud-mana" },
#endif
#if __JDOOM64__
    { 0, "hud-power" },
#endif
#if __JDOOM__ || __JDOOM64__
    { 0, "hud-status-weaponslots-ownedfix" },
#endif
    { 0, "hud-unhide-damage" },
    { 0, "hud-unhide-pickup-ammo" },
    { 0, "hud-unhide-pickup-armor" },
    { 0, "hud-unhide-pickup-health" },
#if __JHERETIC__ || __JHEXEN__
    { 0, "hud-unhide-pickup-invitem" },
#endif
    { 0, "hud-unhide-pickup-powerup" },
    { 0, "hud-unhide-pickup-key" },
    { 0, "hud-unhide-pickup-weapon" },
    { 0, "map-door-colors" },
    { 0, "msg-show" },
#if __JDOOM__ || __JDOOM64__
    { 0, "player-autoswitch-berserk" },
#endif
    { 0, "player-autoswitch-notfiring" },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { 0, "player-jump" },
#endif
    { 0, "player-weapon-cycle-sequential" },
    { 0, "player-weapon-nextmode" },
#if __JDOOM64__
    { 0, "player-weapon-recoil" },
#endif
#if __JDOOM__ || __JDOOM64__
    { 0, "server-game-bfg-freeaim" },
#endif
    { 0, "server-game-coop-nodamage" },
#if __JDOOM__ || __JDOOM64__
    { 0, "server-game-coop-nothing" },
    { 0, "server-game-coop-noweapons" },
    { 0, "server-game-coop-respawn-items" },
#endif
#if __JHEXEN__
    { 0, "server-game-deathmatch" },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { 0, "server-game-jump" },
#endif
#if __JDOOM__ || __JDOOM64__
    { 0, "server-game-nobfg" },
#endif
    { 0, "server-game-nomonsters" },
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { 0, "server-game-noteamdamage" },
#endif
    { 0, "server-game-radiusattack-nomaxz" },
#if __JHEXEN__
    { 0, "server-game-randclass" },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { 0, "server-game-respawn" },
#endif
    { 0, "view-cross-vitality" },
    { 0, 0 }
};

mn_page_t* mnCurrentPage;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean mnActive = false;
static float mnAlpha = 0; // Alpha level for the entire menu.
static float mnTargetAlpha = 0; // Target alpha for the entire UI.

static boolean nominatingQuickGameSaveSlot = false;

#if __JHERETIC__
static patchinfo_t dpRotatingSkull[18];
#elif __JHEXEN__
static patchinfo_t dpBullWithFire[8];
#endif

static patchinfo_t cursorst[MN_CURSOR_COUNT];

#if __JHEXEN__
static int MenuPClass;
#endif

static rgba_t widgetColors[] = { // Ptrs to colors editable with the colour widget
    { &cfg.automapL0[0], &cfg.automapL0[1], &cfg.automapL0[2], NULL },
    { &cfg.automapL1[0], &cfg.automapL1[1], &cfg.automapL1[2], NULL },
    { &cfg.automapL2[0], &cfg.automapL2[1], &cfg.automapL2[2], NULL },
    { &cfg.automapL3[0], &cfg.automapL3[1], &cfg.automapL3[2], NULL },
    { &cfg.automapBack[0], &cfg.automapBack[1], &cfg.automapBack[2], NULL },
    { &cfg.hudColor[0], &cfg.hudColor[1], &cfg.hudColor[2], &cfg.hudColor[3] },
    { &cfg.automapMobj[0], &cfg.automapMobj[1], &cfg.automapMobj[2], NULL },
    { &cfg.xhairColor[0], &cfg.xhairColor[1], &cfg.xhairColor[2], NULL }
};
static boolean rgba = false; // Used to swap between rgb / rgba modes for the color widget.

static int editcolorindex = 0; // The index of the widgetColors array of the obj being currently edited.

static float currentcolor[4] = {0, 0, 0, 0}; // Used by the widget as temporay values.

// Used to fade out the background a little when a widget is active.
static float menu_calpha = 0;

static int quicksave;
static int quickload;

#if __JHERETIC__
static char notDesignedForMessage[80];
#endif

#if __JDOOM__ || __JDOOM64__
static patchinfo_t m_doom;
static patchinfo_t m_newg;
static patchinfo_t m_skill;
static patchinfo_t m_episod;
static patchinfo_t m_ngame;
static patchinfo_t m_option;
static patchinfo_t m_loadg;
static patchinfo_t m_saveg;
static patchinfo_t m_rdthis;
static patchinfo_t m_quitg;
static patchinfo_t m_optttl;
static patchinfo_t dpLSLeft;
static patchinfo_t dpLSRight;
static patchinfo_t dpLSCntr;
#endif

static patchid_t dpSliderLeft;
static patchid_t dpSliderMiddle;
static patchid_t dpSliderRight;
static patchid_t dpSliderHandle;

#if __JHERETIC__ || __JHEXEN__
static patchinfo_t m_htic;
static patchinfo_t dpFSlot;
#endif
#if __JHEXEN__
static patchinfo_t dpPlayerClassBG[3];
#endif

#if __JHERETIC__ || __JHEXEN__
# define READTHISID      3
#elif !__JDOOM64__
# define READTHISID      4
#endif

mn_object_t MainItems[] = {
#if __JDOOM__
    { MN_BUTTON,    0,  0,  "{case}New Game", GF_FONTB, MENU_COLOR, &m_ngame.id, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "{case}Options", GF_FONTB, MENU_COLOR, &m_option.id, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "{case}Load game", GF_FONTB, MENU_COLOR, &m_loadg.id, MNButton_Drawer, MNButton_Dimensions, M_LoadGame },
    { MN_BUTTON,    0,  0,  "{case}Save game", GF_FONTB, MENU_COLOR, &m_saveg.id, MNButton_Drawer, MNButton_Dimensions, M_SaveGame },
    { MN_BUTTON,    0,  0,  "{case}Read This!", GF_FONTB, MENU_COLOR, &m_rdthis.id, MNButton_Drawer, MNButton_Dimensions, M_ReadThis },
    { MN_BUTTON,    0,  0,  "{case}Quit Game", GF_FONTB, MENU_COLOR, &m_quitg.id, MNButton_Drawer, MNButton_Dimensions, M_QuitDOOM },
#elif __JDOOM64__
    { MN_BUTTON,    0,  0,  "{case}New Game", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "{case}Options", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "{case}Load Game", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_LoadGame },
    { MN_BUTTON,    0,  0,  "{case}Save Game", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SaveGame },
    { MN_BUTTON,    0,  0,  "{case}Quit Game", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_QuitDOOM },
#else
    { MN_BUTTON,    0,  0,  "New Game", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "Options",  GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "Game Files", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &FilesMenu },
    { MN_BUTTON,    0,  0,  "Info",     GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ReadThis },
    { MN_BUTTON,    0,  0,  "Quit Game", GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_QuitDOOM },
#endif
    { MN_NONE }
};

#if __JHEXEN__
mn_page_t MainMenu = {
    MainItems, 5,
    0,
    { 110, 56 },
    M_DrawMainMenu,
    //0, 0,
    //0, 5
};
#elif __JHERETIC__
mn_page_t MainMenu = {
    MainItems, 5,
    0,
    { 110, 56 },
    M_DrawMainMenu,
    //0, 0,
    //0, 5
};
#elif __JDOOM64__
mn_page_t MainMenu = {
    MainItems, 5,
    0,
    { 97, 64 },
    M_DrawMainMenu,
    //0, 0,
    //0, 5
};
#elif __JDOOM__
mn_page_t MainMenu = {
    MainItems, 6,
    0,
    { 97, 64 },
    M_DrawMainMenu,
    //0, 0,
    //0, 6
};
#endif

mn_object_t NewGameItems[] = {
    { MN_BUTTON,    0,  0,  "{case}Singleplayer",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_NewGame },
    { MN_BUTTON,    0,  0,  "{case}Multiplayer",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_EnterMultiplayerMenu },
    { MN_NONE }
};

#if __JHEXEN__
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 110, 50 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    //0, 2
};
#elif __JHERETIC__
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 97, 65 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    //0, 2
};
#elif __JDOOM64__
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 97, 65 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    //0, 2
};
#else
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 97, 65 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    //0, 2
};
#endif

#if __JHEXEN__
static mn_object_t* ClassItems;

mn_page_t PlayerClassMenu = {
    0, 0,
    0,
    { 66, 66 },
    M_DrawClassMenu,
    0, &GameTypeMenu,
    //0, 0
};
#endif

#if __JDOOM__ || __JHERETIC__
static mn_object_t* EpisodeItems;

mn_page_t EpisodeMenu = {
    0, 0,
    0,
# if __JDOOM__
    { 48, 63 },
# else
    { 80, 50 },
# endif
    M_DrawEpisode,
    0, &GameTypeMenu,
    //0, 0
};
#endif

#if __JHERETIC__ || __JHEXEN__
static mn_object_t FilesItems[] = {
    { MN_BUTTON,    0,  0,  "Load Game",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_LoadGame },
    { MN_BUTTON,    0,  0,  "Save Game",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SaveGame },
    { MN_NONE }
};

mn_page_t FilesMenu = {
    FilesItems, 2,
    0,
    { 110, 60 },
    M_DrawFilesMenu,
    0, &MainMenu,
    //0, 2
};
#endif

mndata_edit_t edit_saveslots[NUMSAVESLOTS] = {
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 0, M_DoSaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 1, M_DoSaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 2, M_DoSaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 3, M_DoSaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 4, M_DoSaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 5, M_DoSaveGame },
#if !__JHEXEN__
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 6, M_DoSaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 7, M_DoSaveGame }
#endif
};

static mn_object_t LoadItems[] = {
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[0], 0 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[1], 1 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[2], 2 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[3], 3 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[4], 4 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[5], 5 },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[6], 6 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &edit_saveslots[7], 7 },
#endif
    { MN_NONE }
};

mn_page_t LoadMenu = {
    LoadItems, NUMSAVESLOTS,
    0,
#if __JDOOM__ || __JDOOM64__
    { 80, 54 },
#else
    { 70, 30 },
#endif
    M_DrawLoad,
    0, &MainMenu,
    //0, NUMSAVESLOTS
};

static mn_object_t SaveItems[] = {
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[0] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[1] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[2] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[3] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[4] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[5] },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[6] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, MENU_COLOR, 0, MNEdit_Drawer, MNEdit_Dimensions, Hu_MenuSaveSlotEdit, &edit_saveslots[7] },
#endif
    { MN_NONE }
};

mn_page_t SaveMenu = {
    SaveItems, NUMSAVESLOTS,
    0,
#if __JDOOM__ || __JDOOM64__
    { 80, 54 },
#else
    { 64, 10 },
#endif
    M_DrawSave,
    0, &MainMenu,
    //0, 1+NUMSAVESLOTS
};

#if __JHEXEN__
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_BABY },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_EASY },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_HARD },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillLevelMenu = {
    SkillItems, 5,
    0,
    { 120, 44 },
    M_DrawSkillMenu,
    2, &PlayerClassMenu,
    //0, 5
};
#elif __JHERETIC__
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,  "W",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_BABY },
    { MN_BUTTON,    0,  0,  "Y",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_EASY },
    { MN_BUTTON,    0,  0,  "B",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,  "S",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_HARD },
    { MN_BUTTON,    0,  0,  "P",    GF_FONTB, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillLevelMenu = {
    SkillItems, 5,
    0,
    { 38, 30 },
    M_DrawSkillMenu,
    2, &EpisodeMenu,
    //0, 5
};
#elif __JDOOM64__
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,  "I",    GF_FONTB, MENU_COLOR, &skillModeNames[0], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 0 },
    { MN_BUTTON,    0,  0,  "H",    GF_FONTB, MENU_COLOR, &skillModeNames[1], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 1 },
    { MN_BUTTON,    0,  0,  "H",    GF_FONTB, MENU_COLOR, &skillModeNames[2], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 2 },
    { MN_BUTTON,    0,  0,  "U",    GF_FONTB, MENU_COLOR, &skillModeNames[3], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 3 },
    { MN_NONE }
};
static mn_page_t SkillLevelMenu = {
    SkillItems, 4,
    0,
    { 48, 63 },
    M_DrawSkillMenu,
    2, &GameTypeMenu,
    //0, 4
};
#else
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,              "I",    GF_FONTB, MENU_COLOR, &skillModeNames[0], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 0 },
    { MN_BUTTON,    0,  0,              "H",    GF_FONTB, MENU_COLOR, &skillModeNames[1], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 1 },
    { MN_BUTTON,    0,  0,              "H",    GF_FONTB, MENU_COLOR, &skillModeNames[2], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 2 },
    { MN_BUTTON,    0,  0,              "U",    GF_FONTB, MENU_COLOR, &skillModeNames[3], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 3 },
    { MN_BUTTON,    0,  MNF_NO_ALTTEXT, "N",    GF_FONTB, MENU_COLOR, &skillModeNames[4], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 4 },
    { MN_NONE }
};

mn_page_t SkillLevelMenu = {
    SkillItems, 5,
    0,
    { 48, 63 },
    M_DrawSkillMenu,
    2, &EpisodeMenu,
    //0, 5
};
#endif

static mn_object_t OptionsItems[] = {
    { MN_BUTTON,    0,  0,  "End Game", GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_EndGame },
    { MN_BUTTON,    0,  0,  "Control Panel", GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 0 },
    { MN_BUTTON,    0,  0,  "Controls", GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &ControlsMenu },
    { MN_BUTTON,    0,  0,  "Gameplay", GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameplayMenu },
    { MN_BUTTON,    0,  0,  "HUD",      GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &HUDMenu },
    { MN_BUTTON,    0,  0,  "Automap",  GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &AutomapMenu },
    { MN_BUTTON,    0,  0,  "Weapons",  GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &WeaponMenu },
#if __JHERETIC__ || __JHEXEN__
    { MN_BUTTON,    0,  0,  "Inventory", GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &InventoryMenu },
#endif
    { MN_BUTTON,    0,  0,  "Sound",    GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &SoundMenu },
    { MN_BUTTON,    0,  0,  "Mouse",    GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 2 },
    { MN_BUTTON,    0,  0,  "Joystick", GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 2 },
    { MN_NONE }
};

mn_page_t OptionsMenu = {
    OptionsItems,
#if __JHERETIC__ || __JHEXEN__
    11,
#else
    10,
#endif
    0,
    { 110, 63 },
    M_DrawOptions,
    0, &MainMenu,
    //0,
#if __JHERETIC__ || __JHEXEN__
    //11
#else
    //10
#endif
};

mndata_slider_t sld_sound_volume = { 0, 255, 0, 1, false, "sound-volume" };
mndata_slider_t sld_music_volume = { 0, 255, 0, 1, false, "music-volume" };

mn_object_t SoundMenuItems[] = {
    { MN_TEXT,      0,  0,  "SFX Volume", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_sound_volume },
    { MN_TEXT,      0,  0,  "Music Volume", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_music_volume },
    { MN_BUTTON,    0,  0,  "Open Audio Panel", GF_FONTA, MENU_COLOR, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 1 },
    { MN_NONE }
};

mn_page_t SoundMenu = {
    SoundMenuItems, 5,
    0,
#if __JHEXEN__
    { 97, 25 },
#elif __JHERETIC__
    { 97, 30 },
#elif __JDOOM__ || __JDOOM64__
    { 97, 40 },
#endif
    M_DrawOptions2,
    0, &OptionsMenu,
    //0, 5
};

#if __JDOOM64__
mndata_slider_t sld_hud_viewsize = { 0, 11, 0, 1, false, "view-size" };
#else
mndata_slider_t sld_hud_viewsize = { 0, 13, 0, 1, false, "view-size" };
#endif
mndata_slider_t sld_hud_wideoffset = { 0, 1, 0, .1f, true, "hud-wideoffset" };
mndata_slider_t sld_hud_uptime = { 0, 60, 0, 1.f, true, "hud-timer", "Disabled", NULL, " second", " seconds" };
mndata_slider_t sld_hud_xhair_size = { 0, 1, 0, .1f, true, "view-cross-size" };
mndata_slider_t sld_hud_xhair_opacity = { 0, 1, 0, .1f, true, "view-cross-a" };
mndata_slider_t sld_hud_size = { 0, 1, 0, .1f, true, "hud-scale" };
mndata_slider_t sld_hud_counter_size = { 0, 1, 0, .1f, true, "hud-cheat-counter-scale" };
mndata_slider_t sld_hud_statusbar_size = { 0, 1, 0, .1f, true, "hud-status-size" };
mndata_slider_t sld_hud_statusbar_opacity = { 0, 1, 0, .1f, true, "hud-status-alpha" };
mndata_slider_t sld_hud_messages_size = { 0, 1, 0, .1f, true, "msg-scale" };
mndata_slider_t sld_hud_messages_uptime = { 0, 60, 0, 1.f, true, "msg-uptime", "Disabled", NULL, " second", " seconds" };

mndata_listitem_t listit_hud_xhair_symbols[] = {
    { "None", 0 },
    { "Cross", 1 },
    { "Angles", 2 },
    { "Square", 3 },
    { "Open Square", 4 },
    { "Diamond", 5 },
    { "V", 6 }
};
mndata_listinline_t list_hud_xhair_symbol = {
    listit_hud_xhair_symbols, NUMLISTITEMS(listit_hud_xhair_symbols), "view-cross-type"
};

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_listitem_t listit_hud_killscounter_displaymethods[] = {
    { "Hidden", 0 },
    { "Count", 1 },
    { "Percent", 2 },
    { "Count+Percent", 3 },
};
mndata_listinline_t list_hud_killscounter = {
    listit_hud_killscounter_displaymethods,
    NUMLISTITEMS(listit_hud_killscounter_displaymethods),
    "hud-cheat-counter"
};

mndata_listitem_t listit_hud_itemscounter_displaymethods[] = {
    { "Hidden", 0 },
    { "Count", 1 },
    { "Percent", 2 },
    { "Count+Percent", 3 },
};
mndata_listinline_t list_hud_itemscounter = {
    listit_hud_itemscounter_displaymethods,
    NUMLISTITEMS(listit_hud_itemscounter_displaymethods),
    "hud-cheat-counter"
};

mndata_listitem_t listit_hud_secretscounter_displaymethods[] = {
    { "Hidden", 0 },
    { "Count", 1 },
    { "Percent", 2 },
    { "Count+Percent", 3 },
};
mndata_listinline_t list_hud_secretscounter = {
    listit_hud_secretscounter_displaymethods,
    NUMLISTITEMS(listit_hud_secretscounter_displaymethods),
    "hud-cheat-counter"
};
#endif

static mn_object_t HUDItems[] = {
    { MN_TEXT,      0,  0,  "View Size", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "", GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_viewsize },
    { MN_TEXT,      0,  0,  "Wide Offset", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "", GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_wideoffset },
#if __JDOOM__
    { MN_TEXT,      0,  0,  "Single Key Display", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-keys-combine", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_TEXT,      0,  0,  "AutoHide", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "", GF_FONTA, MENU_COLOR3, 0, MNSlider_TextualValueDrawer, MNSlider_TextualValueDimensions, Hu_MenuCvarSlider, &sld_hud_uptime },
    { MN_TEXT,      0,  0,  "UnHide Events", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Receive Damage", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-damage", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Health", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-health", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Armor", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-armor", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Powerup", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-powerup", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Weapon", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-weapon", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  "Pickup Mana", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-ammo", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#else
    { MN_TEXT,      0,  0,  "Pickup Ammo", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-ammo", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_TEXT,      0,  0,  "Pickup Key", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-key", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Pickup Item", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-invitem", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif

    { MN_TEXT,      0,  0,  "Messages", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Shown",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "msg-show", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton }, //M_ChangeMessages },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_messages_size },
    { MN_TEXT,      0,  0,  "Uptime",   GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR3, 0, MNSlider_TextualValueDrawer, MNSlider_TextualValueDimensions, Hu_MenuCvarSlider, &sld_hud_messages_uptime },

    { MN_TEXT,      0,  0,  "Crosshair", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Symbol",   GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,  "",         GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, Hu_MenuCvarList, &list_hud_xhair_symbol },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_xhair_size },
    { MN_TEXT,      0,  0,  "Opacity",  GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",  GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_xhair_opacity },
    { MN_TEXT,      0,  0,  "Vitality Color", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "view-cross-vitality", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Color",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_COLORBOX,  0,  MNF_INACTIVE, "", GF_FONTA, MENU_COLOR, 0, MNColorBox_Drawer, MNColorBox_Dimensions, MN_ActivateColorBox, 0, 7 },

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Statusbar", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_statusbar_size },
    { MN_TEXT,      0,  0,  "Opacity",  GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_statusbar_opacity },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Counters", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Kills",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,  "",         GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, Hu_MenuCvarList, &list_hud_killscounter },
    { MN_TEXT,      0,  0,  "Items",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,  "",         GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, Hu_MenuCvarList, &list_hud_itemscounter },
    { MN_TEXT,      0,  0,  "Secrets",  GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,  "",         GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, Hu_MenuCvarList, &list_hud_secretscounter },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_counter_size },
#endif

    { MN_TEXT,      0,  0,  "Fullscreen HUD", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_size },
    { MN_TEXT,      0,  0,  "Text Color", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_COLORBOX,  0,  MNF_INACTIVE, "", GF_FONTA, MENU_COLOR, 0, MNColorBox_Drawer, MNColorBox_Dimensions, MN_ActivateColorBox, 0, 5 },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  "Show Mana", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-mana", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Show Ammo", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-ammo", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Show Armor", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-armor", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0,  "Show Power Keys", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-power", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__
    { MN_TEXT,      0,  0,  "Show Face", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-face", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_TEXT,      0,  0,  "Show Health", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-health", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Show Keys", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-keys", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Show Item", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-currentitem", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_NONE }
};

mn_page_t HUDMenu = {
#if __JHEXEN__
    HUDItems, 57,
#elif __JHERETIC__
    HUDItems, 69,
#elif __JDOOM64__
    HUDItems, 65,
#elif __JDOOM__
    HUDItems, 70,
#endif
    0,
#if __JDOOM__ || __JDOOM64__
    { 97, 40 },
#else
    { 97, 28 },
#endif
    M_DrawHUDMenu,
    0, &OptionsMenu,
#if __JHEXEN__
    //0, 15        // 21
#elif __JHERETIC__
    //0, 15        // 23
#elif __JDOOM64__
    //0, 19
#elif __JDOOM__
    //0, 19
#endif
};

#if __JHERETIC__ || __JHEXEN__
mndata_button_t btn_inv_selectmode = { &cfg.inventorySelectMode, "Scroll", "Cursor" };
mndata_slider_t sld_inv_uptime = { 0, 30, 0, 1.f, true, "hud-inventory-timer", "Disabled", NULL, " second", " seconds" };
mndata_slider_t sld_inv_maxvisslots = { 0, 16, 0, 1, false, "hud-inventory-slot-max", "Automatic" };

static mn_object_t InventoryItems[] = {
    { MN_TEXT,      0,  0,  "Select Mode", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2EX, 0,  0,  "", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton, &btn_inv_selectmode },
    { MN_TEXT,      0,  0,  "Wrap Around", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-wrap", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Choose And Use", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-use-immediate", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Select Next If Use Failed", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-use-next", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "AutoHide", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "", GF_FONTA, MENU_COLOR3, 0, MNSlider_TextualValueDrawer, MNSlider_TextualValueDimensions, Hu_MenuCvarSlider, &sld_inv_uptime },

    { MN_TEXT,      0,  0,  "Fullscreen HUD", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Max Visible Slots", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "", GF_FONTA, MENU_COLOR3, 0, MNSlider_TextualValueDrawer, MNSlider_TextualValueDimensions, Hu_MenuCvarSlider, &sld_inv_maxvisslots },
    { MN_TEXT,      0,  0,  "Show Empty Slots", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-inventory-slot-showempty", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_NONE }
};

mn_page_t InventoryMenu = {
    InventoryItems, 15,
    0,
    { 78, 48 },
    M_DrawInventoryMenu,
    0, &OptionsMenu,
    //0, 12, { 12, 48 }
};
#endif

mndata_listitem_t listit_weapons_order[NUM_WEAPON_TYPES];
mndata_list_t list_weapons_order = {
    listit_weapons_order, NUMLISTITEMS(listit_weapons_order)
};

mndata_listitem_t listit_weapons_autoswitch_pickup[] = {
    { "Never", 0 },
    { "If Better", 1 },
    { "Always", 2 }
};
mndata_listinline_t list_weapons_autoswitch_pickup = {
    listit_weapons_autoswitch_pickup, NUMLISTITEMS(listit_weapons_autoswitch_pickup), "player-autoswitch"
};

mndata_listitem_t listit_weapons_autoswitch_pickupammo[] = {
    { "Never", 0 },
    { "If Better", 1 },
    { "Always", 2 }
};
mndata_listinline_t list_weapons_autoswitch_pickupammo = {
    listit_weapons_autoswitch_pickupammo, NUMLISTITEMS(listit_weapons_autoswitch_pickupammo), "player-autoswitch-ammo"
};

static mn_object_t WeaponItems[] = {
    { MN_TEXT,      0,  0,  "Priority Order", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  MNF_INACTIVE,  "",         GF_FONTA, MENU_COLOR3, 0, MNList_Drawer, MNList_Dimensions, M_WeaponOrder, &list_weapons_order },
    { MN_TEXT,      0,  0,  "Cycling",  GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Use Priority Order", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-weapon-nextmode", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Sequential", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-weapon-cycle-sequential", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },

    { MN_TEXT,      0,  0,  "Autoswitch", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
    { MN_TEXT,      0,  0,  "Pickup Weapon", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,  "", GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, Hu_MenuCvarList, &list_weapons_autoswitch_pickup },
    { MN_TEXT,      0,  0,  "   If Not Firing", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-autoswitch-notfiring", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Ammo", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LISTINLINE, 0, 0,  "", GF_FONTA, MENU_COLOR3, 0, MNListInline_Drawer, MNListInline_Dimensions, Hu_MenuCvarList, &list_weapons_autoswitch_pickupammo },
#if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Pickup Beserk", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-autoswitch-berserk", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_NONE }
};

mn_page_t WeaponMenu = {
    WeaponItems,
#if __JDOOM__ || __JDOOM64__
    16,
#else
    14,
#endif
    MNPF_NOHOTKEYS,
#if __JDOOM__ || __JDOOM64__
    { 78, 40 },
#elif __JHERETIC__
    { 78, 26 },
#elif __JHEXEN__
    { 78, 38 },
#endif
    M_DrawWeaponMenu,
    1, &OptionsMenu,
    //0, 12, { 12, 38 }
};

static mn_object_t GameplayItems[] = {
    { MN_TEXT,      0,  0, "Always Run",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "ctl-run",    GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0, "Use LookSpring", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "ctl-look-spring", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0, "Use AutoAim",   GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "ctl-aim-noauto",   GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0, "Allow Jumping", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "player-jump", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0, "Weapon Recoil", GF_FONTA, MENU_COLOR },
    { MN_BUTTON2,   0,  0, "player-weapon-recoil", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Compatibility", GF_FONTA, MENU_COLOR2, 0, MNText_Drawer, MNText_Dimensions },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Any Boss Trigger 666", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-anybossdeath666", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#  if !__JDOOM64__
    { MN_TEXT,      0,  0,  "Av Resurrects Ghosts", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-raiseghosts", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#  endif
    { MN_TEXT,      0,  0,  "PE Limited To 21 Lost Souls", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-maxskulls", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "LS Can Get Stuck Inside Walls", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-skullsinwalls", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
# endif
    { MN_TEXT,      0,  0,  "Monsters Can Get Stuck In Doors", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-monsters-stuckindoors", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Some Objects Never Hang Over Ledges", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-objects-neverhangoverledges", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Objects Fall Under Own Weight", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-objects-falloff", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Corpses Slide Down Stairs", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-corpse-sliding", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Use Exactly Doom's Clipping Code", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-objects-clipping", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "  ^If Not NorthOnly WallRunning", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-player-wallrun-northonly", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Zombie Players Can Exit Maps", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-zombiescanexit", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Fix Ouch Face", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-face-ouchfix", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Fix Weapon Slot Display", GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-status-weaponslots-ownedfix", GF_FONTA, MENU_COLOR3, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
# endif
#endif
    { MN_NONE }
};

#if __JHEXEN__
mn_page_t GameplayMenu = {
    GameplayItems, 6,
    0,
    { 88, 25 },
    M_DrawGameplay,
    0, &OptionsMenu,
    //0, 6, { 6, 25 }
};
#else
mn_page_t GameplayMenu = {
#if __JDOOM64__
    GameplayItems, 33,
#elif __JDOOM__
    GameplayItems, 35,
#else
    GameplayItems, 21,
#endif
    0,
    { 30, 40 },
    M_DrawGameplay,
    0, &OptionsMenu,
#if __JDOOM64__
    //0, 16, { 16, 40 }
#elif __JDOOM__
    //0, 18, { 18, 40 }
#else
    //0, 11, { 11, 40 }
#endif
};
#endif

static mn_object_t ColorWidgetItems[] = {
    { MN_TEXT,      0,  0,  "Red",      GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[0] },
    { MN_TEXT,      0,  0,  "Green",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[1] },
    { MN_TEXT,      0,  0,  "Blue",     GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[2] },
    { MN_TEXT,      0,  0,  "Alpha",    GF_FONTA, MENU_COLOR, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, MENU_COLOR, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[3] },
    { MN_NONE }
};

static mn_page_t ColorWidgetMenu = {
    ColorWidgetItems, 8,
    MNPF_NOHOTKEYS,
    { 98, 60 },
    NULL,
    0, &OptionsMenu,
    //0, 8
};

// Cvars for the menu:
cvartemplate_t menuCVars[] = {
    { "menu-scale",     0,  CVT_FLOAT,  &cfg.menuScale, .1f, 1 },
    { "menu-nostretch", 0,  CVT_BYTE,   &cfg.menuNoStretch, 0, 1 },
    { "menu-flash-r",   0,  CVT_FLOAT,  &cfg.flashColor[CR], 0, 1 },
    { "menu-flash-g",   0,  CVT_FLOAT,  &cfg.flashColor[CG], 0, 1 },
    { "menu-flash-b",   0,  CVT_FLOAT,  &cfg.flashColor[CB], 0, 1 },
    { "menu-flash-speed", 0, CVT_INT,   &cfg.flashSpeed, 0, 50 },
    { "menu-turningskull", 0, CVT_BYTE, &cfg.turningSkull, 0, 1 },
    { "menu-effect",    0,  CVT_INT,    &cfg.menuEffects, 0, 1 },
    { "menu-color-r",   0,  CVT_FLOAT,  &cfg.menuColors[0][CR], 0, 1 },
    { "menu-color-g",   0,  CVT_FLOAT,  &cfg.menuColors[0][CG], 0, 1 },
    { "menu-color-b",   0,  CVT_FLOAT,  &cfg.menuColors[0][CB], 0, 1 },
    { "menu-colorb-r",  0,  CVT_FLOAT,  &cfg.menuColors[1][CR], 0, 1 },
    { "menu-colorb-g",  0,  CVT_FLOAT,  &cfg.menuColors[1][CG], 0, 1 },
    { "menu-colorb-b",  0,  CVT_FLOAT,  &cfg.menuColors[1][CB], 0, 1 },
    { "menu-colorc-r",  0,  CVT_FLOAT,  &cfg.menuColors[2][CR], 0, 1 },
    { "menu-colorc-g",  0,  CVT_FLOAT,  &cfg.menuColors[2][CG], 0, 1 },
    { "menu-colorc-b",  0,  CVT_FLOAT,  &cfg.menuColors[2][CB], 0, 1 },
    { "menu-glitter",   0,  CVT_FLOAT,  &cfg.menuGlitter, 0, 1 },
    { "menu-fog",       0,  CVT_INT,    &cfg.hudFog,    0, 5 },
    { "menu-shadow",    0,  CVT_FLOAT,  &cfg.menuShadow, 0, 1 },
    { "menu-patch-replacement", 0, CVT_BYTE, &cfg.usePatchReplacement, 0, 2 },
    { "menu-slam",      0,  CVT_BYTE,   &cfg.menuSlam,  0, 1 },
    { "menu-quick-ask", 0,  CVT_BYTE,   &cfg.confirmQuickGameSave, 0, 1 },
    { "menu-hotkeys",   0,  CVT_BYTE,   &cfg.menuHotkeys, 0, 1 },
#if __JDOOM__ || __JDOOM64__
    { "menu-quitsound", 0,  CVT_INT,    &cfg.menuQuitSound, 0, 1 },
#endif
    { NULL }
};

// Console commands for the menu:
ccmdtemplate_t menuCCmds[] = {
    { "menu",           "",     CCmdMenuAction },
    { "menuup",         "",     CCmdMenuAction },
    { "menudown",       "",     CCmdMenuAction },
    { "menupageup",     "",     CCmdMenuAction },
    { "menupagedown",   "",     CCmdMenuAction },
    { "menuleft",       "",     CCmdMenuAction },
    { "menuright",      "",     CCmdMenuAction },
    { "menuselect",     "",     CCmdMenuAction },
    { "menudelete",     "",     CCmdMenuAction },
    { "menuback",       "",     CCmdMenuAction },
    { "soundmenu",      "",     CCmdShortcut },
    { "quicksave",      "",     CCmdShortcut },
    { "endgame",        "",     CCmdShortcut },
    { "togglemsgs",     "",     CCmdShortcut },
    { "quickload",      "",     CCmdShortcut },
    { "helpscreen",     "",     CCmdShortcut   },
    { "togglegamma",    "",     CCmdShortcut   },
    { NULL }
};

// Code -------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up
 * Register Cvars and CCmds for the opperation/look of the menu.
 */
void Hu_MenuRegister(void)
{
    int i;
    for(i = 0; menuCVars[i].name; ++i)
        Con_AddVariable(menuCVars + i);
    for(i = 0; menuCCmds[i].name; ++i)
        Con_AddCommand(menuCCmds + i);
}

static __inline mn_object_t* focusObject(void)
{
    //if(!mnActive)
    //    return NULL;
    return &mnCurrentPage->_objects[mnFocusObjectIndex];
}

/**
 * Load any resources the menu needs.
 */
void M_LoadData(void)
{
    char buffer[9];
    int i;

    // Load the cursor patches
    for(i = 0; i < MN_CURSOR_COUNT; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        dd_snprintf(buffer, 9, "M_SKULL%d", i+1);
#else
        dd_snprintf(buffer, 9, "M_SLCTR%d", i+1);
#endif
        R_PrecachePatch(buffer, &cursorst[i]);
    }

#if __JDOOM__ || __JDOOM64__
    R_PrecachePatch("M_DOOM", &m_doom);
    R_PrecachePatch("M_NEWG", &m_newg);
    R_PrecachePatch("M_SKILL", &m_skill);
    R_PrecachePatch("M_EPISOD", &m_episod);
    R_PrecachePatch("M_NGAME", &m_ngame);
    R_PrecachePatch("M_OPTION", &m_option);
    R_PrecachePatch("M_LOADG", &m_loadg);
    R_PrecachePatch("M_SAVEG", &m_saveg);
    R_PrecachePatch("M_RDTHIS", &m_rdthis);
    R_PrecachePatch("M_QUITG", &m_quitg);
    R_PrecachePatch("M_OPTTTL", &m_optttl);
    R_PrecachePatch("M_LSLEFT", &dpLSLeft);
    R_PrecachePatch("M_LSRGHT", &dpLSRight);
    R_PrecachePatch("M_LSCNTR", &dpLSCntr);
#endif

#if __JDOOM__ || __JDOOM64__
    dpSliderLeft = R_PrecachePatch("M_THERML", NULL);
    dpSliderMiddle = R_PrecachePatch("M_THERM2", NULL);
    dpSliderRight = R_PrecachePatch("M_THERMR", NULL);
    dpSliderHandle = R_PrecachePatch("M_THERMO", NULL);
#elif __JHERETIC__ || __JHEXEN__
    dpSliderLeft = R_PrecachePatch("M_SLDLT", NULL);
    dpSliderMiddle = R_PrecachePatch("M_SLDMD1", NULL);
    dpSliderRight = R_PrecachePatch("M_SLDRT", NULL);
    dpSliderHandle = R_PrecachePatch("M_SLDKB", NULL);
#endif

#if __JHERETIC__ || __JHEXEN__
    R_PrecachePatch("M_HTIC", &m_htic);
    R_PrecachePatch("M_FSLOT", &dpFSlot);
#endif

#if __JHERETIC__
    for(i = 0; i < 18; ++i)
    {
        dd_snprintf(buffer, 9, "M_SKL%02d", i);
        R_PrecachePatch(buffer, &dpRotatingSkull[i]);
    }
#endif

#if __JHEXEN__
    for(i = 0; i < 7; ++i)
    {
        dd_snprintf(buffer, 9, "FBUL%c0", 'A'+i);
        R_PrecachePatch(buffer, &dpBullWithFire[i]);
    }

    R_PrecachePatch("M_FBOX", &dpPlayerClassBG[0]);
    R_PrecachePatch("M_CBOX", &dpPlayerClassBG[1]);
    R_PrecachePatch("M_MBOX", &dpPlayerClassBG[2]);
#endif
}

static int compareWeaponPriority(const void* _a, const void* _b)
{
    const mndata_listitem_t* a = (const mndata_listitem_t*)_a;
    const mndata_listitem_t* b = (const mndata_listitem_t*)_b;
    int i = 0, aIndex = -1, bIndex = -1;
    do
    {
        if(cfg.weaponOrder[i] == a->data)
            aIndex = i;
        if(cfg.weaponOrder[i] == b->data)
            bIndex = i;
    } while(!(aIndex != -1 && bIndex != -1) && i++ < NUM_WEAPON_TYPES);
    if(aIndex > bIndex)
        return 1;
    if(aIndex < bIndex)
        return -1;
    return 0; // Should never happen.
}

void M_InitWeaponsMenu(void)
{
#if __JHEXEN__
    const char* weaponids[] = { "First", "Second", "Third", "Fourth"};
#endif
    int i;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        mndata_listitem_t* item = &((mndata_listitem_t*)listit_weapons_order)[i];
#if __JDOOM__ || __JDOOM64__
        const char* name = GET_TXT(TXT_WEAPON1 + i);
#elif __JHERETIC__
        /**
         * \fixme We should allow different weapon preferences per player
         * class. However, since the only other class in jHeretic is the
         * chicken which has only 1 weapon anyway -we'll just show the
         * names of the player's weapons for now.
         */
        const char* name = GET_TXT(TXT_TXT_WPNSTAFF + i);
#elif __JHEXEN__
        /**
         * \fixme We should allow different weapon preferences per player
         * class. Then we can show the real names here.
         */
        const char* name = weaponids[i];
#endif
        dd_snprintf(item->text, 256, "%s", name);
        item->data = i;
    }

    qsort(listit_weapons_order, NUM_WEAPON_TYPES, sizeof(listit_weapons_order[0]), compareWeaponPriority);
}

#if __JDOOM__ || __JHERETIC__
/**
 * Construct the episode selection menu.
 */
void M_InitEpisodeMenu(void)
{
    int i, maxw, w, numEpisodes;

#if __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        numEpisodes = 0;
    else if(gameMode == doom_ultimate)
        numEpisodes = 4;
    else
        numEpisodes = 3;
#else // __JHERETIC__
    if(gameMode == heretic_extended)
        numEpisodes = 6;
    else
        numEpisodes = 3;
#endif

    // Allocate the menu objects array.
    EpisodeItems = Z_Calloc(sizeof(mn_object_t) * (numEpisodes+1), PU_GAMESTATIC, 0);

    for(i = 0, maxw = 0; i < numEpisodes; ++i)
    {
        mn_object_t* obj = &EpisodeItems[i];

        obj->type = MN_BUTTON;
        obj->drawer = MNButton_Drawer;
        obj->dimensions = MNButton_Dimensions;
        obj->action = M_Episode;
        obj->data2 = i;
        obj->text = GET_TXT(TXT_EPISODE1 + i);
        obj->fontIdx = GF_FONTB;
        FR_SetFont(FID(obj->fontIdx));
        w = FR_TextFragmentWidth(obj->text);
        if(w > maxw)
            maxw = w;
# if __JDOOM__
        obj->patch = &episodeNamePatches[i];
# endif
    }
    EpisodeItems[i].type = MN_NONE;

    // Finalize setup.
    EpisodeMenu._objects = EpisodeItems;
    EpisodeMenu._size = numEpisodes;
    //EpisodeMenu.numVisObjects = MIN_OF(EpisodeMenu._size, 10);
    EpisodeMenu._offset[VX] = SCREENWIDTH/2 - maxw / 2 + 18; // Center the menu appropriately.
}
#endif

#if __JHEXEN__
/**
 * Construct the player class selection menu.
 */
void M_InitPlayerClassMenu(void)
{
    uint i, n, count;

    // First determine the number of selectable player classes.
    count = 0;
    for(i = 0; i < NUM_PLAYER_CLASSES; ++i)
    {
        classinfo_t* info = PCLASS_INFO(i);

        if(info->userSelectable)
            count++;
    }

    // Allocate the menu objects array.
    ClassItems = Z_Calloc(sizeof(mn_object_t) * (count + 1), PU_GAMESTATIC, 0);

    // Add the selectable classes.
    n = i = 0;
    while(n < count)
    {
        classinfo_t* info = PCLASS_INFO(i++);
        mn_object_t* obj;

        if(!info->userSelectable)
            continue;

        obj = &ClassItems[n];
        obj->type = MN_BUTTON;
        obj->drawer = MNButton_Drawer;
        obj->dimensions = MNButton_Dimensions;
        obj->action = M_ChooseClass;
        obj->data2 = n;
        obj->text = info->niceName;
        obj->fontIdx = GF_FONTB;

        ++n;
    }

    // Add the random class option.
    ClassItems[n].type = MN_BUTTON;
    ClassItems[n].drawer = MNButton_Drawer;
    ClassItems[n].dimensions = MNButton_Dimensions;
    ClassItems[n].action = M_ChooseClass;
    ClassItems[n].data2 = -1;
    ClassItems[n].text = GET_TXT(TXT_RANDOMPLAYERCLASS);
    ClassItems[n].fontIdx = GF_FONTB;

    // Finalize setup.
    PlayerClassMenu._objects = ClassItems;
    PlayerClassMenu._size = count + 1;
    //PlayerClassMenu.numVisObjects = MIN_OF(PlayerClassMenu._size, 10);
}
#endif

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
    mn_object_t *obj;
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int w, maxw;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    R_GetGammaMessageStrings();
#endif

#if __JDOOM__ || __JDOOM64__
    // Quit messages.
    endmsg[0] = GET_TXT(TXT_QUITMSG);
    { int i;
    for(i = 1; i <= NUM_QUITMESSAGES; ++i)
        endmsg[i] = GET_TXT(TXT_QUITMESSAGE1 + i - 1);
    }
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Skill names.
    { int i;
    for(i = 0, maxw = 0; i < NUM_SKILL_MODES; ++i)
    {
        SkillItems[i].text = GET_TXT(TXT_SKILL1 + i);
        FR_SetFont(FID(SkillItems[i].fontIdx));
        w = FR_TextFragmentWidth(SkillItems[i].text);
        if(w > maxw)
            maxw = w;
    }}
    // Center the skill menu appropriately.
    SkillLevelMenu._offset[VX] = SCREENWIDTH/2 - maxw / 2 + 14;
#endif

    { int i;
    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        mndata_edit_t* edit = &edit_saveslots[i];
        if(NULL != edit->emptyString && ((unsigned int)edit->emptyString < NUMTEXT))
        {
            edit->emptyString = GET_TXT((unsigned int)edit->emptyString);
        }
    }}

    // Play modes.
    NewGameItems[0].text = GET_TXT(TXT_SINGLEPLAYER);
    NewGameItems[1].text = GET_TXT(TXT_MULTIPLAYER);

    mnCurrentPage = &MainMenu;
    mnActive = false;
    DD_Execute(true, "deactivatebcontext menu");
    mnAlpha = mnTargetAlpha = 0;

    M_LoadData();

    mnFocusObjectIndex = mnCurrentPage->focus;
    whichSkull = 0;
    skullAnimCounter = MN_CURSOR_TICSPERFRAME;

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        obj = &MainItems[4]; // Read This!
        obj->action = M_QuitDOOM;
        obj->text = "{case}Quit Game";
        obj->patch = &m_quitg.id;
        MainMenu._size = 5;
        MainMenu._offset[VY] += 8;
    }

    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        SkillLevelMenu.previous = &GameTypeMenu;
    }
#elif __JHERETIC__ || __JHEXEN__
    obj = &MainItems[READTHISID]; // Read This!
    obj->action = M_ReadThis;
#endif

#if __JDOOM__ || __JHERETIC__
    M_InitEpisodeMenu();
#endif
#if __JHEXEN__
    M_InitPlayerClassMenu();
#endif
    M_InitControlsMenu();
    M_InitWeaponsMenu();
}

/**
 * @return              @c true, iff the menu is currently active (open).
 */
boolean Hu_MenuIsActive(void)
{
    return mnActive;
}

/**
 * Set the alpha level of the entire menu.
 *
 * @param alpha         Alpha level to set the menu too (0...1)
 */
void Hu_MenuSetAlpha(float alpha)
{
    // The menu's alpha will start moving towards this target value.
    mnTargetAlpha = alpha;
}

/**
 * @return              Current alpha level of the menu.
 */
float Hu_MenuAlpha(void)
{
    return mnAlpha;
}

/**
 * Updates on Game Tick.
 */
void Hu_MenuTicker(timespan_t ticLength)
{
    static trigger_t fixed = { 1 / 35.0 };

    // Move towards the target alpha level for the entire menu.
    if(mnAlpha != mnTargetAlpha)
    {
#define MENUALPHA_FADE_STEP (.07)

        float diff = mnTargetAlpha - mnAlpha;
        if(fabs(diff) > MENUALPHA_FADE_STEP)
        {
            mnAlpha += (float)(MENUALPHA_FADE_STEP * ticLength * TICRATE * (diff > 0? 1 : -1));
        }
        else
        {
            mnAlpha = mnTargetAlpha;
        }

#undef MENUALPHA_FADE_STEP
    }

    if(mnActive)// || mnAlpha > 0)
    {
        mn_object_t* focusObj = focusObject();
        // Fade in/out the widget background filter
        if(focusObj && focusObj->type == MN_COLORBOX && !(focusObj->flags & MNF_INACTIVE))
        {
            if(menu_calpha < 0.5f)
                menu_calpha += (float)(.1 * ticLength * TICRATE);
            if(menu_calpha > 0.5f)
                menu_calpha = 0.5f;
        }
        else
        {
            if(menu_calpha > 0)
                menu_calpha -= (float)(.1 * ticLength * TICRATE);
            if(menu_calpha < 0)
                menu_calpha = 0;
        }

        menu_color += (int)(cfg.flashSpeed * ticLength * TICRATE);
        if(menu_color >= 100)
            menu_color -= 100;

        if(cfg.turningSkull)
        {
#define SKULL_REWIND_SPEED 20
            const mn_object_t* focusObj = focusObject();

            if(focusObj && !(focusObj->flags & (MNF_DISABLED|MNF_INACTIVE)) &&
               (focusObj->type == MN_LISTINLINE || focusObj->type == MN_SLIDER))
            {
                skull_angle += (float)(5 * ticLength * TICRATE);
            }
            else if(skull_angle != 0)
            {
                float rewind = (float)(SKULL_REWIND_SPEED * ticLength * TICRATE);
                if(skull_angle <= rewind || skull_angle >= 360 - rewind)
                    skull_angle = 0;
                else if(skull_angle < 180)
                    skull_angle -= rewind;
                else
                    skull_angle += rewind;
            }

            if(skull_angle >= 360)
                skull_angle -= 360;

#undef SKULL_REWIND_SPEED
        }
    }

    // The following is restricted to fixed 35 Hz ticks.
    if(!M_RunTrigger(&fixed, ticLength))
        return; // It's too soon.

    if(mnActive)// || mnAlpha > 0)
    {
        mnTime++;

        // Animate the cursor patches.
        if(--skullAnimCounter <= 0)
        {
            whichSkull++;
            skullAnimCounter = MN_CURSOR_TICSPERFRAME;
            if(whichSkull > MN_CURSOR_COUNT-1)
                whichSkull = 0;
        }

        // Used for Heretic's rotating skulls.
        frame = (mnTime / 3) % 18;
    }
}

void Hu_MenuPageString(char* str, const mn_page_t* page)
{
/*    sprintf(str, "PAGE %i/%i", (page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1,
            (int)ceil((float)page->_size/page->numVisObjects));*/
}

static void calcNumVisObjects(mn_page_t* page)
{
/*    page->firstObject = MAX_OF(0, mnFocusObjectIndex - page->numVisObjects/2);
    page->firstObject = MIN_OF(page->firstObject, page->_size - page->numVisObjects);
    page->firstObject = MAX_OF(0, page->firstObject);*/
}

mn_page_t* MN_CurrentPage(void)
{
    return mnCurrentPage;
}

void MN_GotoPage(mn_page_t* page)
{
    if(!mnActive)
        return;
    if(!page || mnCurrentPage == page)
        return;

    mnCurrentPage = page;

    // Have we been to this menu before?
    // If so move the cursor to the last selected obj
    if(mnCurrentPage->focus >= 0)
    {
        mnFocusObjectIndex = mnCurrentPage->focus;
    }
    else
    {   // Select the first active obj in this menu.
        uint i;
        for(i = 0; i < page->_size; ++i)
        {
            const mn_object_t* obj = &page->_objects[i];
            if(obj->action && !(obj->flags & (MNF_DISABLED|MNF_HIDDEN)))
                break;
        }

        if(i >= page->_size)
            mnFocusObjectIndex = -1;
        else
            mnFocusObjectIndex = i;
    }

    // Init objects.
    { uint i; mn_object_t* obj;
    for(i = 0, obj = page->_objects; i < page->_size; ++i, obj++)
    {
        // Calculate real coordinates.
        /*obj->x = UI_ScreenX(obj->relx);
        obj->w = UI_ScreenW(obj->relw);
        obj->y = UI_ScreenY(obj->rely);
        obj->h = UI_ScreenH(obj->relh);*/

        // Update objects on page
        /*if(obj->type == MN_EDIT)
        {
            // Update edit box text.
            memset(obj->text, 0, sizeof(obj->text));
            strncpy(obj->text, ((mndata_edit_t*) obj->data)->ptr, 255);
        }
        else*/ if(obj->type == MN_BUTTON2)
        {
            // Stay-down button state.
            if(*(char*) obj->data)
                obj->flags &= ~MNF_INACTIVE;
            else
                obj->flags |= MNF_INACTIVE;
        }
        else if(obj->type == MN_BUTTON2EX)
        {
            // Stay-down button state, with extended data.
            if(*(char*) ((mndata_button_t*)obj->data)->data)
                obj->flags &= ~MNF_INACTIVE;
            else
                obj->flags |= MNF_INACTIVE;
        }
        else if(obj->type == MN_LIST || obj->type == MN_LISTINLINE)
        {
            // List box number of visible items.
            mndata_list_t* dat = obj->data;

            //dat->numvis = (obj->h - 2 * UI_BORDER) / listItemHeight(dat);
            if(dat->selection >= 0)
            {
                if(dat->selection < dat->first)
                    dat->first = dat->selection;
                //if(dat->selection > dat->first + dat->numvis - 1)
                //    dat->first = dat->selection - dat->numvis + 1;
            }
            //UI_InitColumns(obj);
        }
    }}
    // The mouse has not yet been moved on this page.
    //uiMoved = false;

    calcNumVisObjects(mnCurrentPage);

    menu_color = 0;
    skull_angle = 0;
    FR_ResetTypeInTimer();

    nominatingQuickGameSaveSlot = false;
}

/**
 * This is the main menu drawing routine (called every tic by the drawing
 * loop) Draws the current menu 'page' by calling the funcs attached to
 * each menu obj.
 */
void Hu_MenuDrawer(void)
{
    mn_page_t* page = mnCurrentPage;
    int offset[2];
    uint i;

    // Popped at the end of the function.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_LoadIdentity();

    // Setup matrix.
    if(mnActive || mnAlpha > 0)
    {
        // Scale by the menuScale.
        DGL_MatrixMode(DGL_MODELVIEW);

        DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
        DGL_Scalef(cfg.menuScale, cfg.menuScale, 1);
        DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);
    }

    if(!mnActive && !(mnAlpha > 0))
        goto end_draw_menu;

    /*if(page->unscaled.numVisObjects)
    {
        page->numVisObjects = page->unscaled.numVisObjects / cfg.menuScale;
        page->_offset[VY] = (SCREENHEIGHT/2) - ((SCREENHEIGHT/2) - page->unscaled.y) / cfg.menuScale;
    }*/

    if(page->drawer)
    {
        page->drawer(page, page->_offset[VX], page->_offset[VY]);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(page->_offset[VX], page->_offset[VY], 0);

    if(mnAlpha > 0.0125f)
    {
        const mn_object_t* focusObj = focusObject();
        int pos[2] = { 0, 0 };

        for(i = 0/*page->firstObject*/;
            i < page->_size /*&& i < page->firstObject + page->numVisObjects*/; ++i)
        {
            const mn_object_t* obj = &page->_objects[i];
            int height = 0;

            if(obj->type == MN_NONE||(obj->flags & MNF_HIDDEN)||!obj->drawer)
                continue;

            obj->drawer(obj, pos[VX], pos[VY], mnAlpha);

            if(obj->dimensions)
                obj->dimensions(obj, NULL, &height);

            // Draw the cursor?
            if(obj == focusObj)
            {
#if __JDOOM__ || __JDOOM64__
# define MN_CURSOR_OFFSET_X         (-22)
# define MN_CURSOR_OFFSET_Y         (-2)
#elif __JHERETIC__ || __JHEXEN__
# define MN_CURSOR_OFFSET_X         (-16)
# define MN_CURSOR_OFFSET_Y         (3)
#endif

                mn_page_t* mn = ((focusObj && focusObj->type == MN_COLORBOX && !(focusObj->flags & MNF_INACTIVE))? &ColorWidgetMenu : page);
                float scale = MIN_OF((float) (height * 1.267f) / cursorst[whichSkull].height, 1);

                offset[VX] = MN_CURSOR_OFFSET_X * scale;
                offset[VY] = MN_CURSOR_OFFSET_Y * scale + height/2;

                DGL_MatrixMode(DGL_MODELVIEW);
                DGL_PushMatrix();

                DGL_Translatef(pos[VX] + offset[VX], pos[VY] + offset[VY], 0);
                DGL_Scalef(scale, scale, 1);
                if(skull_angle)
                    DGL_Rotatef(skull_angle, 0, 0, 1);

                DGL_Enable(DGL_TEXTURE_2D);
                DGL_Color4f(1, 1, 1, mnAlpha);

                GL_DrawPatch2(cursorst[whichSkull].id, 0, 0, DPF_NO_OFFSET);

                DGL_Disable(DGL_TEXTURE_2D);

                DGL_MatrixMode(DGL_MODELVIEW);
                DGL_PopMatrix();

#undef MN_CURSOR_OFFSET_Y
#undef MN_CURSOR_OFFSET_X
            }

            pos[VY] += height * (1.08f); // Leading.
        }

        // Draw the colour widget?
        if(focusObj && focusObj->type == MN_COLORBOX && !(focusObj->flags & MNF_INACTIVE))
        {
            Draw_BeginZoom(0.5f, SCREENWIDTH/2, SCREENHEIGHT/2);
            drawColorWidget();
            Draw_EndZoom();
        }
    }

    // Unnecessary given the matrix will be popped imminently.
    //DGL_MatrixMode(DGL_MODELVIEW);
    //DGL_Translatef(-page->_offset[VX], -page->_offset[VY], 0);

  end_draw_menu:

    // Restore original matrices.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    if(mnAlpha > 0.0125f)
    {
        const mn_object_t* focusObj = focusObject();
        if(focusObj && focusObj->type == MN_BINDINGS && !(focusObj->flags & MNF_INACTIVE))
        {
            M_ControlGrabDrawer();
        }
    }
}

void Hu_MenuNavigatePage(mn_page_t* page, int pageDelta)
{
#if 0
    uint index = MAX_OF(0, mnFocusObjectIndex), oldIndex = index;

    if(pageDelta < 0)
    {
        index = MAX_OF(0, index - page->numVisObjects);
    }
    else
    {
        index = MIN_OF(page->_size-1, index + page->numVisObjects);
    }

    // Don't land on empty objects.
    while((!page->_objects[index].action || (page->_objects[index].flags & (MNF_DISABLED|MNF_HIDDEN))) && (index > 0))
        index--;
    while((!page->_objects[index].action || (page->_objects[index].flags & (MNF_DISABLED|MNF_HIDDEN))) && index < page->_size)
        index++;

    if(index != oldIndex)
    {
        mnFocusObjectIndex = index;
        // Make a sound, too.
        S_LocalSound(SFX_MENU_NAV_RIGHT, NULL);
    }

    calcNumVisObjects(mnCurrentPage);
#endif
}

/**
 * Initializes all slider objects.
 */
void MN_InitCvarSliders(mn_object_t* obj)
{
    for(; obj->type; obj++)
    {
        if(obj->action == Hu_MenuCvarSlider)
        {
            mndata_slider_t* sldr = obj->data;

            if(sldr->floatMode)
                sldr->value = Con_GetFloat(sldr->data1);
            else
                sldr->value = Con_GetInteger(sldr->data1);
        }
    }
}

// Set cvarbutton data pointers.
// This is only done the first time this object collection is processed.
void MN_InitCvarButtons(mn_object_t* obj)
{
    for(; obj->type; obj++)
    {
        if(obj->action == Hu_MenuCvarButton)
        {
            cvarbutton_t* cvb;
            if(obj->data)
            {
                // This button has already been initialized.
                cvb = obj->data;
                cvb->active = (Con_GetByte(cvb->cvarname) & (obj->data2? obj->data2 : ~0)) != 0;
                //strcpy(obj->text, cvb->active ? cvb->yes : cvb->no);
                obj->text = cvb->active ? cvb->yes : cvb->no;
                continue;
            }
            // Find the cvarbutton representing this one.
            for(cvb = mnCVarButtons; cvb->cvarname; cvb++)
            {
                if(!strcmp(obj->text, cvb->cvarname) && obj->data2 == cvb->mask)
                {
                    cvb->active = (Con_GetByte(cvb->cvarname) & (obj->data2? obj->data2 : ~0)) != 0;
                    obj->data = cvb;
                    //strcpy(obj->text, cvb->active ? cvb->yes : cvb->no);
                    obj->text = (cvb->active ? cvb->yes : cvb->no);
                    break;
                }
            }
            cvb = NULL;
        }
        else if(obj->action == Hu_MenuCvarList)
        {
            mndata_list_t* list = obj->data;
            // Choose the correct list item based on the value of the cvar.
            list->selection = MNList_FindItem(obj, Con_GetInteger(list->data));
        }
        else if(obj->action == Hu_MenuCvarEdit)
        {
            mndata_edit_t* edit = obj->data;
            MNEdit_SetText(obj, Con_GetString(edit->data1));
            //strncpy(edit->ptr, Con_GetString(edit->data), edit->maxLen);
        }
    }
}

static void initObjectsInCollection(mn_object_t* objCollection)
{
    MN_InitCvarButtons(objCollection);
    MN_InitCvarSliders(objCollection);
}

static void initAllObjectsOnAllPages(void)
{
#if __JHEXEN__
    initObjectsInCollection(ClassItems);
#endif
#if __JDOOM__ || __JHERETIC__
    initObjectsInCollection(EpisodeItems);
#endif
#if __JHERETIC__ || __JHEXEN__
    initObjectsInCollection(FilesItems);
#endif
    initObjectsInCollection(LoadItems);
    initObjectsInCollection(SaveItems);
    initObjectsInCollection(SkillItems);
    initObjectsInCollection(OptionsItems);
    initObjectsInCollection(SoundMenuItems);
    initObjectsInCollection(SoundMenuItems);
    initObjectsInCollection(HUDItems);
#if __JHERETIC__ || __JHEXEN__
    initObjectsInCollection(InventoryItems);
#endif
    initObjectsInCollection(WeaponItems);
    initObjectsInCollection(GameplayItems);
    initObjectsInCollection(ColorWidgetItems);
    initObjectsInCollection(MAPItems);
    initObjectsInCollection(MultiplayerItems);
    initObjectsInCollection(MultiplayerClientItems);
    initObjectsInCollection(PlayerSetupItems);

    // Set default Yes/No strings.
    { cvarbutton_t* cvb;
    for(cvb = mnCVarButtons; cvb->cvarname; cvb++)
    {
        if(!cvb->yes)
            cvb->yes = "Yes";
        if(!cvb->no)
            cvb->no = "No";
    }}
}

/**
 * Execute a menu navigation/action command.
 */
void Hu_MenuCommand(menucommand_e cmd)
{
    // If a close command is received while currently working with a selected
    // "active" widget - interpret the command instead as "navigate out".
    if(cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST)
    {
        mn_object_t* obj = focusObject();
        if(mnActive && NULL != obj)
        {
            switch(obj->type)
            {
            case MN_EDIT:
            case MN_LIST:
                if(!(obj->flags & MNF_INACTIVE))
                {
                    cmd = MCMD_NAV_OUT;
                }
                break;
            default:
                break;
            }
        }
    }

    if(cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST)
    {
        nominatingQuickGameSaveSlot = false;

        Hu_FogEffectSetAlphaTarget(0);

        if(cmd == MCMD_CLOSEFAST)
        {   // Hide the menu instantly.
            mnAlpha = mnTargetAlpha = 0;
        }
        else
        {
            mnTargetAlpha = 0;
        }

        if(mnActive)
        {
            if(NULL != mnCurrentPage)
            {
                mnCurrentPage->focus = mnFocusObjectIndex;
            }

            if(cmd != MCMD_CLOSEFAST)
                S_LocalSound(SFX_MENU_CLOSE, NULL);

            mnActive = false;

            // Disable the menu binding class
            DD_Execute(true, "deactivatebcontext menu");
        }

        return;
    }

    if(!mnActive)
    {
        if(cmd == MCMD_OPEN)
        {
            S_LocalSound(SFX_MENU_OPEN, NULL);

            Con_Open(false);

            Hu_FogEffectSetAlphaTarget(1);
            Hu_MenuSetAlpha(1);
            mnActive = true;
            menu_color = 0;
            mnTime = 0;
            skull_angle = 0;
            mnCurrentPage = &MainMenu;
            mnFocusObjectIndex = mnCurrentPage->focus;
            FR_ResetTypeInTimer();

            initAllObjectsOnAllPages();

            // Enable the menu binding class
            DD_Execute(true, "activatebcontext menu");
            B_SetContextFallback("menu", Hu_MenuResponder);
        }
    }
    else
    {
        uint i, hasFocus;
        uint firstVisible, lastVisible; // first and last visible obj
        uint numVisObjectsOffset = 0;
        mn_object_t* obj, *focusObj = focusObject();
        mn_page_t* menu = mnCurrentPage;
        boolean updateFocus = true;

        if(focusObj && focusObj->type == MN_COLORBOX && !(focusObj->flags & MNF_INACTIVE))
        {
            menu = &ColorWidgetMenu;
            if(!rgba)
                numVisObjectsOffset = 1;
        }

        if(mnFocusObjectIndex < 0)
            updateFocus = false;

        hasFocus = MAX_OF(0, mnFocusObjectIndex);

        firstVisible = 0;//menu->firstObject;
        lastVisible = firstVisible + menu->_size/*numVisObjects*/ - 1 - numVisObjectsOffset;
        if(lastVisible > menu->_size - 1 - numVisObjectsOffset)
            lastVisible = menu->_size - 1 - numVisObjectsOffset;
        obj = &menu->_objects[hasFocus];

        if(updateFocus)
            menu->focus = mnFocusObjectIndex;

        switch(cmd)
        {
        default:
            Con_Error("Internal Error: Menu cmd %i not handled in Hu_MenuCommand.", (int) cmd);
            break; // Unreachable.

        case MCMD_OPEN: // Ignore.
            break;

        case MCMD_NAV_LEFT:
            if((obj->type == MN_SLIDER || obj->type == MN_LISTINLINE) && obj->action != NULL)
            {
                S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
                obj->action(obj, LEFT_DIR);
            }
            break;

        case MCMD_NAV_RIGHT:
            if((obj->type == MN_SLIDER || obj->type == MN_LISTINLINE) && obj->action != NULL)
            {
                S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
                obj->action(obj, RIGHT_DIR);
            }
            break;

        case MCMD_NAV_PAGEUP:
        case MCMD_NAV_PAGEDOWN:
            if(!(MN_LIST == obj->type && !(obj->flags & MNF_INACTIVE)))
            {
                S_LocalSound(SFX_MENU_NAV_UP, NULL);
                Hu_MenuNavigatePage(menu, cmd == MCMD_NAV_PAGEUP? -1 : +1);
            }
            else
            {
                S_LocalSound(SFX_MENU_CANCEL, NULL);
            }
            break;

        case MCMD_NAV_DOWN:
            if(MN_LIST == obj->type && !(obj->flags & MNF_INACTIVE))
            {
                S_LocalSound(SFX_MENU_NAV_DOWN, NULL);
            }
            else
            {
                i = 0;
                do
                {
                    if(hasFocus + 1 > menu->_size - 1)
                        hasFocus = 0;
                    else
                        hasFocus++;
                } while((!menu->_objects[hasFocus].action || (menu->_objects[hasFocus].flags & (MNF_DISABLED|MNF_HIDDEN))) && i++ < menu->_size);
                mnFocusObjectIndex = hasFocus;
                menu_color = 0;
                S_LocalSound(SFX_MENU_NAV_DOWN, NULL);
                calcNumVisObjects(mnCurrentPage);
            }
            break;

        case MCMD_NAV_UP:
            if(MN_LIST == obj->type && !(obj->flags & MNF_INACTIVE))
            {
                S_LocalSound(SFX_MENU_NAV_UP, NULL);
            }
            else
            {
                i = 0;
                do
                {
                    if(hasFocus <= 0)
                        hasFocus = menu->_size - 1;
                    else
                        hasFocus--;
                } while((!menu->_objects[hasFocus].action || (menu->_objects[hasFocus].flags & (MNF_DISABLED|MNF_HIDDEN))) && i++ < menu->_size);
                mnFocusObjectIndex = hasFocus;
                menu_color = 0;
                S_LocalSound(SFX_MENU_NAV_UP, NULL);
                calcNumVisObjects(mnCurrentPage);
            }
            break;

        case MCMD_NAV_OUT:
            menu->focus = hasFocus;
            if(MN_LIST == obj->type && !(obj->flags & MNF_INACTIVE))
            {
                obj->flags |= MNF_INACTIVE;
                S_LocalSound(SFX_MENU_CANCEL, NULL);
            }
            else if(!menu->previous)
            {
                S_LocalSound(SFX_MENU_CLOSE, NULL);
                Hu_MenuCommand(MCMD_CLOSE);
            }
            else
            {
                S_LocalSound(SFX_MENU_CANCEL, NULL);
                MN_GotoPage(menu->previous);
            }
            break;

        case MCMD_DELETE:
            if(obj->action && obj->type == MN_BINDINGS)
            {
                S_LocalSound(SFX_MENU_CANCEL, NULL);
                obj->action(obj, -1);
            }
            break;

        case MCMD_SELECT:
            if(MN_LIST == obj->type && (obj->flags & MNF_INACTIVE))
            {
                menu->focus = hasFocus;
                S_LocalSound(SFX_MENU_CYCLE, NULL);
                obj->action(obj, obj->data2);
                obj->flags &= ~MNF_INACTIVE;
            }
            else if(obj->action)
            {
                menu->focus = hasFocus;
                switch(obj->type)
                {
                case MN_BUTTON:
                case MN_BUTTON2:
                case MN_BUTTON2EX:
                case MN_EDIT:
                case MN_COLORBOX:
                case MN_BINDINGS:
                case MN_LIST:
                case MN_LISTINLINE:
                    S_LocalSound(SFX_MENU_CYCLE, NULL);
                    obj->action(obj, (obj->type == MN_LISTINLINE? RIGHT_DIR : obj->data2));
                    break;
                default:
                    break;
                }
            }
            break;
        }
    }
}

int Hu_MenuObjectResponder(event_t* ev)
{
    mn_object_t* focusObj;
    if(!Hu_MenuIsActive())
        return false;
    focusObj = &mnCurrentPage->_objects[mnFocusObjectIndex];
    if(focusObj->type != MN_EDIT || (focusObj->flags & (MNF_DISABLED|MNF_INACTIVE|MNF_HIDDEN)))
        return false;
    return MNEdit_Responder(focusObj, ev);
}

/**
 * Handles the hotkey selection in the menu.
 *
 * @return              @c true, if it ate the event.
 */
int Hu_MenuResponder(event_t* ev)
{
    mn_page_t* page;
    mn_object_t* focusObj;

    if(!mnActive)
        return false;

    page = mnCurrentPage;
    focusObj = focusObject();

    if(focusObj && focusObj->type == MN_COLORBOX && !(focusObj->flags & MNF_INACTIVE))
        return false; // Not while using the color widget.

    /**
     * Handle navigation by "hotkeys", if enabled.
     *
     * The first ASCII character of a page obj's text string is used
     * as a "hotkey" shortcut to allow navigating directly to that obj.
     */
    if(cfg.menuHotkeys && !(page->flags & MNPF_NOHOTKEYS) &&
       ev->type == EV_KEY && (ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
    {
        uint i, first, last; // First and last, visible page objects.
        int cand = toupper(ev->data1);

        first = last = 0;//page->firstObject;
        last += page->_size/*page->numVisObjects*/ - 1;

        if(last > page->_size - 1)
            last = page->_size - 1;
        page->focus = mnFocusObjectIndex;

        for(i = first; i <= last; ++i)
        {
            const mn_object_t* obj = &page->_objects[i];

            if(obj->text && obj->text[0] && obj->action && !(obj->flags & (MNF_DISABLED|MNF_HIDDEN)))
            {
                const char* ch = obj->text;
                boolean inParamBlock = false;

                /**
                 * Skip over any paramater blocks, we are only interested
                 * in the first (drawable) ASCII character.
                 *
                 * \assume Item text strings are '\0' terminated.
                 */
                do
                {
                    if(!ch)
                        break;

                    if(inParamBlock)
                    {
                        if(*ch == '}')
                            inParamBlock = false;
                    }
                    else
                    {
                        if(*ch == '{')
                        {
                            inParamBlock = true;
                        }
                        else if(!(*ch == ' ' || *ch == '\n'))
                        {
                            break; // First drawable character found.
                        }
                    }
                } while(*ch++);

                if(ch && *ch == cand)
                {
                    mnFocusObjectIndex = i;
                    return true;
                }
            }
        }
    }

    return false;
}

void M_DrawMenuText5(const char* string, int x, int y, int fontIdx, short flags,
    float glitterStrength, float shadowStrength)
{
    if(cfg.menuEffects == 0)
    {
        flags |= DTF_NO_TYPEIN|DTF_NO_SHADOW;
        glitterStrength = 0;
        shadowStrength = 0;
    }

    FR_SetFont(FID(fontIdx));
    FR_DrawTextFragment7(string, x, y, flags, 0, 0, glitterStrength, shadowStrength, 0, 0);
}

void M_DrawMenuText4(const char* string, int x, int y, int fontIdx, short flags, float glitterStrength)
{
    M_DrawMenuText5(string, x, y, fontIdx, flags, glitterStrength, cfg.menuShadow);
}

void M_DrawMenuText3(const char* string, int x, int y, int fontIdx, short flags)
{
    M_DrawMenuText4(string, x, y, fontIdx, flags, cfg.menuGlitter);
}

void M_DrawMenuText2(const char* string, int x, int y, int fontIdx)
{
    M_DrawMenuText3(string, x, y, fontIdx, DTF_ALIGN_TOPLEFT);
}

void M_DrawMenuText(const char* string, int x, int y)
{
    M_DrawMenuText2(string, x, y, GF_FONTA);
}

/**
 * The colour widget edits the "hot" currentcolour[]
 * The widget responder handles setting the specified vars to that of the
 * currentcolour.
 *
 * \fixme The global value rgba (fixme!) is used to control if rgb or rgba input
 * is needed, as defined in the widgetColors array.
 */
static void drawColorWidget(void)
{
#if __JDOOM__ || __JDOOM64__
# define BGWIDTH            (160)
# define BGHEIGHT           (rgba? 85 : 75)
#elif __JHERETIC__ || __JHEXEN__
# define BGWIDTH            (180)
# define BGHEIGHT           (rgba? 170 : 140)
#endif

    const mn_page_t* page = &ColorWidgetMenu;
    int x = ColorWidgetMenu._offset[VX];
    int y = ColorWidgetMenu._offset[VY];

    M_DrawBackgroundBox(x-24, y-40, BGWIDTH, BGHEIGHT, true, BORDERUP, 1, 1, 1, mnAlpha);

    DGL_SetNoMaterial();
    DGL_DrawRect(x + BGWIDTH/2 - 24/2 - 24, y + 10 - 40, 24, 22, currentcolor[0], currentcolor[1], currentcolor[2], currentcolor[3]);
    M_DrawBackgroundBox(x + BGWIDTH/2 - 24/2 - 24, y + 10 - 40, 24, 22, false, BORDERDOWN, 1, 1, 1, mnAlpha);
#if 0
    MN_DrawSlider(page, 0, x, y, 11, currentcolor[0] * 10 + .25f);
    MN_DrawSlider(page, 1, x, y, 11, currentcolor[1] * 10 + .25f);
    MN_DrawSlider(page, 2, x, y, 11, currentcolor[2] * 10 + .25f);
    if(rgba)
        MN_DrawSlider(page, 3, x, y, 11, currentcolor[3] * 10 + .25f);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText(ColorWidgetItems[0].text, x, y);
    M_DrawMenuText(ColorWidgetItems[1].text, x, y + LINEHEIGHT_A);
    M_DrawMenuText(ColorWidgetItems[2].text, x, y + (LINEHEIGHT_A * 2));
    if(rgba)
        M_DrawMenuText(ColorWidgetItems[3].text, x, y + (LINEHEIGHT_A * 3));
#endif

#undef BGWIDTH
#undef BGHEIGHT
}

/**
 * Inform the menu to activate the color widget
 * An intermediate step. Used to copy the existing rgba values pointed
 * to by the index (these match an index in the widgetColors array) into
 * the "hot" currentcolor[] slots. Also switches between rgb/rgba input.
 */
void MN_ActivateColorBox(mn_object_t* obj, int option)
{
    currentcolor[0] = *widgetColors[option].r;
    currentcolor[1] = *widgetColors[option].g;
    currentcolor[2] = *widgetColors[option].b;

    // Set the option of the colour being edited
    editcolorindex = option;

    // Remember the focus object on the current page.
    mnPreviousFocusObjectIndex = mnFocusObjectIndex;

    // Set the start position to 0;
    mnFocusObjectIndex = 0;

    // Do we want rgb or rgba sliders?
    if(widgetColors[option].a)
    {
        rgba = true;
        currentcolor[3] = *widgetColors[option].a;
#if __JHERETIC__ || __JHEXEN__
        ColorWidgetMenu._size = 12;
#else
        ColorWidgetMenu._size = 4;
#endif
    }
    else
    {
        rgba = false;
        currentcolor[3] = 1.0f;
#if __JHERETIC__ || __JHEXEN__
        ColorWidgetMenu._size = 9;
#else
        ColorWidgetMenu._size = 3;
#endif
    }

    obj->flags &= ~MNF_INACTIVE; // Activate the widget.
}

/**
 * User wants to load this game
 */
void M_LoadSelect(mn_object_t* obj, int option)
{
    const int saveSlot = option;

    SaveMenu.focus = option;
    Hu_MenuCommand(MCMD_CLOSEFAST);

    G_LoadGame(saveSlot);
}

void M_DrawMainMenu(const mn_page_t* page, int x, int y)
{
#if __JHEXEN__
    int frame = (mnTime / 5) % 7;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnAlpha);

    GL_DrawPatch(m_htic.id, x - 22, y - 56);
    GL_DrawPatch(dpBullWithFire[(frame + 2) % 7].id, x - 73, y + 24);
    GL_DrawPatch(dpBullWithFire[frame].id, x + 168, y + 24);

    DGL_Disable(DGL_TEXTURE_2D);

#elif __JHERETIC__
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(m_htic.id, x - 22, y - 56, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, mnAlpha);
    DGL_Color4f(1, 1, 1, mnAlpha);
    GL_DrawPatch(dpRotatingSkull[17 - frame].id, x - 70, y - 46);
    GL_DrawPatch(dpRotatingSkull[frame].id, x + 122, y - 46);

    DGL_Disable(DGL_TEXTURE_2D);
#elif __JDOOM__ || __JDOOM64__
    DGL_Enable(DGL_TEXTURE_2D);
    WI_DrawPatch4(m_doom.id, x - 3, y - 62, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, mnAlpha);
    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void M_DrawNewGameMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);

    M_DrawMenuText3(GET_TXT(TXT_PICKGAMETYPE), x + 60, y - 25, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

#if __JHERETIC__
static void composeNotDesignedForMessage(const char* str)
{
    char* buf = notDesignedForMessage, *in, tmp[2];

    buf[0] = 0;
    tmp[1] = 0;

    // Get the message template.
    in = GET_TXT(TXT_NOTDESIGNEDFOR);

    for(; *in; in++)
    {
        if(in[0] == '%')
        {
            if(in[1] == '1')
            {
                strcat(buf, str);
                in++;
                continue;
            }

            if(in[1] == '%')
                in++;
        }
        tmp[0] = *in;
        strcat(buf, tmp);
    }
}
#endif

#if __JHEXEN__
void M_DrawClassMenu(const mn_page_t* page, int x, int y)
{
#define BG_X            (108)
#define BG_Y            (-58)

    float w, h, s, t;
    int pClass;
    spriteinfo_t sprInfo;
    int tmap = 1;

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText2("Choose class:", x - 32, y - 42, GF_FONTB);

    pClass = focusObject()->data2;
    if(pClass < 0)
    {   // Random class.
        // Number of user-selectable classes.
        pClass = (mnTime / 5) % (page->_size - 1);
    }

    R_GetSpriteInfo(STATES[PCLASS_INFO(pClass)->normalState].sprite, ((mnTime >> 3) & 3), &sprInfo);

    DGL_Color4f(1, 1, 1, mnAlpha);
    GL_DrawPatch(dpPlayerClassBG[pClass % 3].id, x + BG_X, y + BG_Y);

    // Fighter's colors are a bit different.
    if(pClass == PCLASS_FIGHTER)
        tmap = 2;

    x += BG_X + 56 - sprInfo.offset;
    y += BG_Y + 78 - sprInfo.topOffset;
    w = sprInfo.width;
    h = sprInfo.height;

    s = sprInfo.texCoord[0];
    t = sprInfo.texCoord[1],

    DGL_SetPSprite2(sprInfo.material, 1, tmap);

    DGL_Color4f(1, 1, 1, mnAlpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(x, y + h);
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);

#undef BG_X
#undef BG_Y
}
#endif

#if __JDOOM__ || __JHERETIC__
void M_DrawEpisode(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__
    /**
     * \kludge Inform the user episode 6 is designed for deathmatch only.
     */
    if(mnFocusObjectIndex >= 0 && page->_objects[mnFocusObjectIndex].data2 == 5)
    {
        const char* str = notDesignedForMessage;
        composeNotDesignedForMessage(GET_TXT(TXT_SINGLEPLAYER));
        DGL_Color4f(cfg.menuColors[1][CR], cfg.menuColors[1][CG], cfg.menuColors[1][CB], mnAlpha);
        M_DrawMenuText3(str, SCREENWIDTH/2, SCREENHEIGHT - 2, GF_FONTA, DTF_ALIGN_BOTTOM);
    }
#else // __JDOOM__
    WI_DrawPatch4(m_episod.id, x + 7, y - 25, "{case}Which Episode{scaley=1.25,y=-3}?", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void M_DrawSkillMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JDOOM__ || __JDOOM64__
    WI_DrawPatch4(m_newg.id, x + 48, y - 49, "{case}NEW GAME", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    WI_DrawPatch4(m_skill.id, x + 6, y - 25, "{case}Choose Skill Level:", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#elif __JHEXEN__
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("Choose Skill Level:", x + 4, y - 28, GF_FONTB, DTF_ALIGN_TOPLEFT);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawFilesMenu(const mn_page_t* page, int x, int y)
{
    // Clear out the quicksave/quickload stuff.
    quicksave = 0;
    quickload = 0;
}

void MN_UpdateGameSaveWidgets(void)
{
    // Prompt a refresh of the game-save info. We don't yet actively monitor
    // the contents of the game-save paths, so instead we settle for manual
    // updates whenever the save/load menu is opened.
    SV_UpdateGameSaveInfo();

    // Update widgets.
    { int i;
    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        mn_object_t* obj = &LoadItems[i];
        mndata_edit_t* edit = &edit_saveslots[i];
        const gamesaveinfo_t* info = SV_GetGameSaveInfoForSlot(i);

        obj->flags |= MNF_DISABLED;
        memset(edit->text, 0, sizeof(edit->text));

        if(!Str_IsEmpty(&info->filePath))
        {
            strncpy(edit->text, Str_Text(&info->name), sizeof(edit->text)-1);
            obj->flags &= ~MNF_DISABLED;
        }
    }}
}

/**
 * Called after the save name has been modified and to action the game-save.
 */
void M_DoSaveGame(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    const int saveSlot = edit->data2;
    const char* saveName = edit->text;

    if(nominatingQuickGameSaveSlot)
    {
        Con_SetInteger("game-save-quick-slot", saveSlot);
        nominatingQuickGameSaveSlot = false;
    }

    if(!G_SaveGame(saveSlot, saveName))
        return;

    SaveMenu.focus = edit->data2;
    LoadMenu.focus = edit->data2;

    S_LocalSound(SFX_MENU_ACCEPT, NULL);
    Hu_MenuCommand(MCMD_CLOSEFAST);
    }
}

void MNText_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
    uint colorIndex = (obj->colorIdx % NUM_MENU_COLORS);
    float color[4];

    color[CR] = cfg.menuColors[colorIndex][CR];
    color[CG] = cfg.menuColors[colorIndex][CG];
    color[CB] = cfg.menuColors[colorIndex][CB];
    // Flash the focused object?
    if(obj == focusObject())
    {
        float t = (menu_color <= 50? menu_color / 50.0f : (100 - menu_color) / 50.0f);
        color[CR] *= t; color[CG] *= t; color[CB] *= t;
        color[CR] += cfg.flashColor[CR] * (1 - t); color[CG] += cfg.flashColor[CG] * (1 - t); color[CB] += cfg.flashColor[CB] * (1 - t);
    }
    color[CA] = alpha;

    if(obj->patch)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch4(*obj->patch, x, y, (obj->flags & MNF_NO_ALTTEXT)? NULL : obj->text, obj->fontIdx, true, DPF_ALIGN_TOPLEFT, color[CR], color[CG], color[CB], color[CA]);
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(color);

    M_DrawMenuText2(obj->text, x, y, obj->fontIdx);

    DGL_Disable(DGL_TEXTURE_2D);
}

void MNText_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    // @fixme What if patch replacement is disabled?
    if(obj->patch)
    {
        patchinfo_t info;
        R_GetPatchInfo(*obj->patch, &info);
        if(width)
            *width = info.width;
        if(height)
            *height = info.height;
        return;
    }
    FR_SetFont(FID(obj->fontIdx));
    FR_TextFragmentDimensions(width, height, obj->text);
}

void MNEdit_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
#if __JDOOM__ || __JDOOM64__
# define COLOR_IDX              (0)
# define OFFSET_X               (0)
# define OFFSET_Y               (0)
# define BG_OFFSET_X            (-11)
#else
# define COLOR_IDX              (2)
# define OFFSET_X               (13)
# define OFFSET_Y               (5)
# define BG_OFFSET_X            (-5)
#endif

    const mndata_edit_t* edit = (const mndata_edit_t*) obj->data;
    boolean isActive = obj == focusObject() && !(obj->flags & MNF_INACTIVE);
    char buf[MNDATA_EDIT_TEXT_MAX_LENGTH+1];
    const char* string;
    float light = 1;

    x += OFFSET_X;
    y += OFFSET_Y;

    if(isActive)
    {
        if((mnTime & 8) && strlen(edit->text) < MNDATA_EDIT_TEXT_MAX_LENGTH)
        {
            dd_snprintf(buf, MNDATA_EDIT_TEXT_MAX_LENGTH+1, "%s_", edit->text);
            string = buf;
        }
        else
            string = edit->text;
    }
    else
    {
        if(edit->text[0])
        {
            string = edit->text;
        }
        else
        {
            string = edit->emptyString;
            light *= .5f;
            alpha *= .75f;
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(obj->fontIdx));
    { int width, numVisCharacters;
    if(edit->maxVisibleChars > 0)
        numVisCharacters = MIN_OF(edit->maxVisibleChars, MNDATA_EDIT_TEXT_MAX_LENGTH);
    else
        numVisCharacters = MNDATA_EDIT_TEXT_MAX_LENGTH;
    width = numVisCharacters * FR_CharWidth('_') + 20;
    M_DrawSaveLoadBorder(x + BG_OFFSET_X, y - 1, width);
    }

    if(string)
    {
        float color[4];
        color[CR] = cfg.menuColors[COLOR_IDX][CR];
        color[CG] = cfg.menuColors[COLOR_IDX][CG];
        color[CB] = cfg.menuColors[COLOR_IDX][CB];

        if(isActive)
        {
            float t = (menu_color <= 50? (menu_color / 50.0f) : ((100 - menu_color) / 50.0f));

            color[CR] *= t; color[CG] *= t; color[CB] *= t;
            color[CR] += cfg.flashColor[CR] * (1 - t); color[CG] += cfg.flashColor[CG] * (1 - t); color[CB] += cfg.flashColor[CB] * (1 - t);
        }
        color[CA] = alpha;

        color[CR] *= light; color[CG] *= light; color[CB] *= light;

        DGL_Color4fv(color);
        M_DrawMenuText3(string, x, y, obj->fontIdx, DTF_ALIGN_TOPLEFT|DTF_NO_TYPEIN);
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef BG_OFFSET_X
#undef OFFSET_Y
#undef OFFSET_X
#undef COLOR_IDX
}

void MNEdit_SetText(mn_object_t* obj, const char* string)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    dd_snprintf(edit->text, MNDATA_EDIT_TEXT_MAX_LENGTH, "%s", string);
    }
}

/**
 * Responds to alphanumeric input for edit fields.
 */
boolean MNEdit_Responder(mn_object_t* obj, const event_t* ev)
{
    int ch = -1;
    char* ptr;

    if(ev->type != EV_KEY)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
    {
        shiftdown = (ev->state == EVS_DOWN || ev->state == EVS_REPEAT);
        return true;
    }

    if(!(ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        return false;

    ch = ev->data1;

    if(ch >= ' ' && ch <= 'z')
    {
        mndata_edit_t* edit = (mndata_edit_t*) obj->data;
        if(shiftdown)
            ch = shiftXForm[ch];

        // Filter out nasty characters.
        if(ch == '%')
            return true;

        if(strlen(edit->text) < MNDATA_EDIT_TEXT_MAX_LENGTH)
        {
            ptr = edit->text + strlen(edit->text);
            ptr[0] = ch;
            ptr[1] = 0;
        }

        return true;
    }

    return false;
}

void MNEdit_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    // @fixme calculate visible dimensions properly.
    if(width)
        *width = 170;
    if(height)
        *height = 14;
}

void MNList_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
    const mndata_list_t* list = (const mndata_list_t*) obj->data;
    const uint colorIdx = (obj->colorIdx % NUM_MENU_COLORS);
    float color[4];
    int i;

    DGL_Enable(DGL_TEXTURE_2D);

    color[CR] = cfg.menuColors[colorIdx][CR];
    color[CG] = cfg.menuColors[colorIdx][CG];
    color[CB] = cfg.menuColors[colorIdx][CB];
    color[CA] = alpha;
    DGL_Color4fv(color);
    for(i = 0; i < list->count; ++i)
    {
        const mndata_listitem_t* item = &((const mndata_listitem_t*) list->items)[i];
        M_DrawMenuText2(item->text, x, y, GF_FONTA);
        FR_SetFont(FID(GF_FONTA));
        y += FR_TextFragmentHeight(item->text) * (1+MNDATA_LIST_LEADING);
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

int MNList_FindItem(const mn_object_t* obj, int dataValue)
{
    assert(NULL != obj);
    {
    mndata_list_t* list = obj->data;
    int i;
    for(i = 0; i < list->count; ++i)
        if(((mndata_listitem_t*) list->items)[i].data == dataValue)
            return i;
    return -1;
    }
}

void MNListInline_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
    const mndata_listinline_t* list = (const mndata_listinline_t*) obj->data;
    const mndata_listitem_t* item = ((const mndata_listitem_t*) list->items) + list->selection;
    const uint colorIdx = (obj->colorIdx % NUM_MENU_COLORS);
    float color[4];

    DGL_Enable(DGL_TEXTURE_2D);

    color[CR] = cfg.menuColors[colorIdx][CR];
    color[CG] = cfg.menuColors[colorIdx][CG];
    color[CB] = cfg.menuColors[colorIdx][CB];
    color[CA] = alpha;
    DGL_Color4fv(color);
    M_DrawMenuText2(item->text, x, y, GF_FONTA);

    DGL_Disable(DGL_TEXTURE_2D);
}

void MNList_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    const mndata_list_t* list = (const mndata_list_t*) obj->data;
    int i;
    if(!width && !height)
        return;
    if(width)
        *width = 0;
    if(height)
        *height = 0;
    FR_SetFont(FID(GF_FONTA));
    for(i = 0; i < list->count; ++i)
    {
        const mndata_listitem_t* item = &((const mndata_listitem_t*) list->items)[i];
        int w;
        if(width && (w = FR_TextFragmentWidth(item->text)) > *width)
            *width = w;
        if(height)
        {
            int h = FR_TextFragmentHeight(item->text);
            *height += h;
            if(i != list->count-1)
                *height += h * MNDATA_LIST_LEADING;
        }
    }
}

void MNListInline_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    const mndata_listinline_t* list = (const mndata_listinline_t*) obj->data;
    const mndata_listitem_t* item = ((const mndata_listitem_t*) list->items) + list->selection;
    FR_SetFont(FID(GF_FONTA));
    if(width)
        *width = FR_TextFragmentWidth(item->text);
    if(height)
        *height = FR_TextFragmentHeight(item->text);
}

void MNButton_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
    int dis = (obj->flags & MNF_DISABLED) != 0;
    int act = (obj->flags & MNF_INACTIVE) == 0;
    int click = (obj->flags & MNF_CLICKED) != 0;
    boolean down = act || click;
    const uint colorIdx = (obj->colorIdx % NUM_MENU_COLORS);
    const char* text;
    float color[4];

    color[CR] = cfg.menuColors[colorIdx][CR];
    color[CG] = cfg.menuColors[colorIdx][CG];
    color[CB] = cfg.menuColors[colorIdx][CB];
    // Flash the focused object?
    if(obj == focusObject())
    {
        float t = (menu_color <= 50? menu_color / 50.0f : (100 - menu_color) / 50.0f);
        color[CR] *= t; color[CG] *= t; color[CB] *= t;
        color[CR] += cfg.flashColor[CR] * (1 - t); color[CG] += cfg.flashColor[CG] * (1 - t); color[CB] += cfg.flashColor[CB] * (1 - t);
    }
    color[CA] = alpha;

    if(obj->type == MN_BUTTON2EX)
    {
        const mndata_button_t* data = (const mndata_button_t*) obj->data;
        if(down)
            text = data->yes;
        else
            text = data->no;
    }
    else
    {
        text = obj->text;
    }

    if(obj->patch)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch4(*obj->patch, x, y, (obj->flags & MNF_NO_ALTTEXT)? NULL : text, obj->fontIdx, true, DPF_ALIGN_TOPLEFT, color[CR], color[CG], color[CB], color[CA]);
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(color);

    M_DrawMenuText2(text, x, y, obj->fontIdx);

    DGL_Disable(DGL_TEXTURE_2D);
}

void MNButton_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    int dis = (obj->flags & MNF_DISABLED) != 0;
    int act = (obj->flags & MNF_INACTIVE) == 0;
    //int click = (obj->flags & MNF_CLICKED) != 0;
    boolean down = act /*|| click*/;
    const char* text;

    // @fixme What if patch replacement is disabled?
    if(obj->patch)
    {
        patchinfo_t info;
        R_GetPatchInfo(*obj->patch, &info);
        if(width)
            *width = info.width;
        if(height)
            *height = info.height;
        return;
    }

    if(obj->type == MN_BUTTON2EX)
    {
        const mndata_button_t* data = (const mndata_button_t*) obj->data;
        if(down)
            text = data->yes;
        else
            text = data->no;
    }
    else
    {
        text = obj->text;
    }
    FR_SetFont(FID(obj->fontIdx));
    FR_TextFragmentDimensions(width, height, text);
}

void MNColorBox_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
#define WIDTH                   ((float)MNDATA_COLORBOX_WIDTH)
#define HEIGHT                  ((float)MNDATA_COLORBOX_HEIGHT)

    const rgba_t* color = &widgetColors[obj->data2];

    x += 3;
    y += 3;

    DGL_Enable(DGL_TEXTURE_2D);
    M_DrawBackgroundBox(x, y, WIDTH, HEIGHT, true, 1, 1, 1, 1, alpha);
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_SetNoMaterial();
    DGL_DrawRect(x, y, WIDTH, HEIGHT, *color->r, *color->g, *color->b, color->a? *color->a : 1 * alpha);

#undef HEIGHT
#undef WIDTH
}

void MNColorBox_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    if(width)
        *width = MNDATA_COLORBOX_WIDTH+6;
    if(height)
        *height = MNDATA_COLORBOX_HEIGHT+6;
}

int MNSlider_ThumbPos(const mn_object_t* obj)
{
#define WIDTH           (middleInfo.width)

    mndata_slider_t* data = obj->data;
    float range = data->max - data->min, useVal;
    patchinfo_t middleInfo;

    if(!R_GetPatchInfo(dpSliderMiddle, &middleInfo))
        return 0;

    if(!range)
        range = 1; // Should never happen.
    if(data->floatMode)
        useVal = data->value;
    else
    {
        if(data->value >= 0)
            useVal = (int) (data->value + .5f);
        else
            useVal = (int) (data->value - .5f);
    }
    useVal -= data->min;
    //return obj->x + UI_BAR_BORDER + butw + useVal / range * (obj->w - UI_BAR_BORDER * 2 - butw * 3);
    return useVal / range * MNDATA_SLIDER_SLOTS * WIDTH;

#undef WIDTH
}

void MNSlider_Drawer(const mn_object_t* obj, int inX, int inY, float alpha)
{
#define OFFSET_X                (0)
#if __JHERETIC__ || __JHEXEN__
#  define OFFSET_Y              (MNDATA_SLIDER_PADDING_Y + 1)
#else
#  define OFFSET_Y              (MNDATA_SLIDER_PADDING_Y)
#endif
#define WIDTH                   (middleInfo.width)
#define HEIGHT                  (middleInfo.height)

    const mndata_slider_t* slider = (const mndata_slider_t*) obj->data;
    float x = inX, y = inY, range = slider->max - slider->min;
    patchinfo_t middleInfo, leftInfo;

    if(!R_GetPatchInfo(dpSliderMiddle, &middleInfo))
        return;
    if(!R_GetPatchInfo(dpSliderLeft, &leftInfo))
        return;
    if(WIDTH <= 0 || HEIGHT <= 0)
        return;

    x += (leftInfo.width + OFFSET_X) * MNDATA_SLIDER_SCALE;
    y += (OFFSET_Y) * MNDATA_SLIDER_SCALE;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(MNDATA_SLIDER_SCALE, MNDATA_SLIDER_SCALE, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    if(cfg.menuShadow > 0)
    {
        float from[2], to[2];
        from[0] = 2;
        from[1] = 1+HEIGHT/2;
        to[0] = (MNDATA_SLIDER_SLOTS * WIDTH) - 2;
        to[1] = 1+HEIGHT/2;
        M_DrawGlowBar(from, to, HEIGHT*1.1f, true, true, true, 0, 0, 0, alpha * cfg.menuShadow);
    }

    DGL_Color4f(1, 1, 1, alpha);

    GL_DrawPatch2(dpSliderLeft, 0, 0, DPF_ALIGN_RIGHT|DPF_ALIGN_TOP|DPF_NO_OFFSETX);
    GL_DrawPatch(dpSliderRight, MNDATA_SLIDER_SLOTS * WIDTH, 0);

    DGL_SetPatch(dpSliderMiddle, DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectTiled(0, middleInfo.topOffset, MNDATA_SLIDER_SLOTS * WIDTH, HEIGHT, middleInfo.width, middleInfo.height);

    DGL_Color4f(1, 1, 1, alpha);
    GL_DrawPatch2(dpSliderHandle, MNSlider_ThumbPos(obj), 1, DPF_ALIGN_TOP|DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef HEIGHT
#undef WIDTH
#undef OFFSET_Y
#undef OFFSET_X
}

static __inline boolean valueIsOne(float value, boolean floatMode)
{
    if(floatMode)
    {
        return INRANGE_OF(1, value, .0001f);
    }
    return (value > 0 && 1 == (int)(value + .5f));
}

static char* composeTextualValue(float value, boolean floatMode, int precision,
    size_t bufSize, char* buf)
{
    assert(0 != bufSize && NULL != buf);
    precision = MAX_OF(0, precision);
    if(floatMode && !valueIsOne(value, floatMode))
    {
        dd_snprintf(buf, bufSize, "%.*f", precision, value);
    }
    else
    {
        dd_snprintf(buf, bufSize, "%.*i", precision, (int)value);
    }
    return buf;
}

static char* composeValueString(float value, float defaultValue, boolean floatMode,
    int precision, const char* defaultString, const char* templateString,
    const char* onethSuffix, const char* nthSuffix, size_t bufSize, char* buf)
{
    assert(0 != bufSize && NULL != buf);
    {
    boolean haveTemplateString = (NULL != templateString && templateString[0]);
    boolean haveDefaultString  = (NULL != defaultString && defaultString[0]);
    boolean haveOnethSuffix    = (NULL != onethSuffix && onethSuffix[0]);
    boolean haveNthSuffix      = (NULL != nthSuffix && nthSuffix[0]);
    const char* suffix = NULL;
    char textualValue[11];

    // Is the default-value-string in use?
    if(haveDefaultString && INRANGE_OF(value, defaultValue, .0001f))
    {
        strncpy(buf, defaultString, bufSize);
        buf[bufSize] = '\0';
        return buf;
    }

    composeTextualValue(value, floatMode, precision, 10, textualValue);

    // Choose a suffix.
    if(haveOnethSuffix && valueIsOne(value, floatMode))
    {
        suffix = onethSuffix;
    }
    else if(haveNthSuffix)
    {
        suffix = nthSuffix;
    }
    else
    {
        suffix = "";
    }

    // Are we substituting the textual value into a template? 
    if(haveTemplateString)
    {
        size_t textualValueLen = strlen(textualValue);
        const char* c, *beginSubstring = NULL;
        ddstring_t compStr;

        // Reserve a conservative amount of storage, we assume the caller
        // knows best and take the value given as the output buffer size.
        Str_Init(&compStr);
        Str_Reserve(&compStr, bufSize);

        // Composite the final string.
        beginSubstring = templateString;
        for(c = beginSubstring; *c; c++)
        {
            if(c[0] == '%' && c[1] == '1')
            {
                Str_PartAppend(&compStr, beginSubstring, 0, c - beginSubstring);
                Str_Appendf(&compStr, "%s%s", textualValue, suffix);
                // Next substring will begin from here.
                beginSubstring = c + 2;
                c += 1;
            }
        }
        // Anything remaining?
        if(beginSubstring != c)
            Str_Append(&compStr, beginSubstring);

        strncpy(buf, Str_Text(&compStr), bufSize);
        buf[bufSize] = '\0';
        Str_Free(&compStr);
    }
    else
    {
        dd_snprintf(buf, bufSize, "%s%s", textualValue, suffix);
    }

    return buf;
    }
}

void MNSlider_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    patchinfo_t info;
    if(!R_GetPatchInfo(dpSliderMiddle, &info))
        return;
    if(width)
        *width = (int) (info.width * MNDATA_SLIDER_SLOTS * MNDATA_SLIDER_SCALE + .5f);
    if(height)
    {
        int max = info.height;
        if(R_GetPatchInfo(dpSliderLeft, &info))
            max = MAX_OF(max, info.height);
        if(R_GetPatchInfo(dpSliderRight, &info))
            max = MAX_OF(max, info.height);
        *height = (int) ((max + MNDATA_SLIDER_PADDING_Y * 2) * MNDATA_SLIDER_SCALE + .5f);
    }
}

void MNSlider_TextualValueDrawer(const mn_object_t* obj, int x, int y, float alpha)
{
    assert(NULL != obj);
    {
    const mndata_slider_t* sldr = (const mndata_slider_t*) obj->data;
    const float value = MINMAX_OF(sldr->min, sldr->value, sldr->max);
    const uint colorIdx = (obj->colorIdx % NUM_MENU_COLORS);
    char textualValue[41];
    const char* str = composeValueString(value, 0, sldr->floatMode, 0, 
        sldr->data2, sldr->data3, sldr->data4, sldr->data5, 40, textualValue);

    DGL_Translatef(x, y, 0);
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuColors[colorIdx][CR],
                cfg.menuColors[colorIdx][CG],
                cfg.menuColors[colorIdx][CB], alpha);

    M_DrawMenuText2(str, 0, 0, obj->fontIdx);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_Translatef(-x, -y, 0);
    }
}

void MNSlider_TextualValueDimensions(const mn_object_t* obj, int* width, int* height)
{
    assert(NULL != obj);
    {
    const mndata_slider_t* sldr = (const mndata_slider_t*) obj->data;
    if(NULL != width || NULL != height)
    {
        const float value = MINMAX_OF(sldr->min, sldr->value, sldr->max);
        char textualValue[41];
        const char* str = composeValueString(value, 0, sldr->floatMode, 0,
            sldr->data2, sldr->data3, sldr->data4, sldr->data5, 40, textualValue);

        FR_TextFragmentDimensions(width, height, str);
    }
    }
}

static void findSpriteForMobjType(mobjtype_t mobjType, spritetype_e* sprite, int* frame)
{
    assert(mobjType >= MT_FIRST && mobjType < NUMMOBJTYPES && NULL != sprite && NULL != frame);
    {
    mobjinfo_t* info = &MOBJINFO[mobjType];
    int stateNum = info->states[SN_SPAWN];
    *sprite = STATES[stateNum].sprite;
    *frame = ((mnTime >> 3) & 3);
    }
}

/// \todo We can do better - the engine should be able to render this visual for us.
void MNMobjPreview_Drawer(const mn_object_t* obj, int inX, int inY, float alpha)
{
    assert(NULL != obj);
    {
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*) obj->data;
    float x = inX, y = inY, w, h, s, t, scale;
    int tClass, tMap, spriteFrame;
    spritetype_e sprite;
    spriteinfo_t info;

    findSpriteForMobjType(mop->mobjType, &sprite, &spriteFrame);
    if(!R_GetSpriteInfo(sprite, spriteFrame, &info))
        return;

    w = info.width;
    h = info.height;
    scale = (h > w? MNDATA_MOBJPREVIEW_HEIGHT / h : MNDATA_MOBJPREVIEW_WIDTH / w);
    w *= scale;
    h *= scale;

    x += MNDATA_MOBJPREVIEW_WIDTH/2 - info.width/2 * scale;
    y += MNDATA_MOBJPREVIEW_HEIGHT  - info.height  * scale;

    tClass = mop->tClass;
    tMap = mop->tMap;
    // Are we cycling the translation map?
    if(tMap == NUMPLAYERCOLORS)
        tMap = mnTime / 5 % NUMPLAYERCOLORS;
#if __JHEXEN__
    if(mop->plrClass >= PCLASS_FIGHTER)
        R_GetTranslation(mop->plrClass, tMap, &tClass, &tMap);
#endif

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_SetPSprite2(info.material, tClass, tMap);

    s = info.texCoord[0];
    t = info.texCoord[1];

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(x, y + h);
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);
    }
}

void MNMobjPreview_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    // @fixme calculate visible dimensions properly!
    if(width)
        *width = MNDATA_MOBJPREVIEW_WIDTH;
    if(height)
        *height = MNDATA_MOBJPREVIEW_HEIGHT;
}

void Hu_MenuCvarButton(mn_object_t* obj, int option)
{
    cvarbutton_t* cb = obj->data;
    int value;

    // Respond to input event.
    if(obj->flags & MNF_CLICKED)
    {
        //if((ev->device == IDEV_KEYBOARD || ev->device == IDEV_MOUSE) && IS_TOGGLE_UP(ev))
        {
            //UI_Capture(0);
            obj->flags &= ~MNF_CLICKED;
            /*if(UI_MouseInside(obj) || ev->device == IDEV_KEYBOARD)
            {
                // Activate?
                if(obj->action)
                    obj->action(obj);
            }
            return true;*/
        }
    }
    else /*if(IS_TOGGLE_DOWN(ev) &&
            ((ev->device == IDEV_MOUSE && UI_MouseInside(obj)) ||
             (ev->device == IDEV_KEYBOARD && IS_ACTKEY(ev->toggle.id))))*/
    {
        if(obj->type == MN_BUTTON)
        {
            // Capture input.
            //UI_Capture(obj);
            obj->flags |= MNF_CLICKED;
        }
        else
        {
            // Stay-down buttons change state.
            obj->flags ^= MNF_INACTIVE;

            if(obj->data)
            {
                void* data;

                if(obj->type == MN_BUTTON2EX)
                    data = ((mndata_button_t*) obj->data)->data;
                else
                    data = obj->data;

                *(char*) data = (obj->flags & MNF_INACTIVE) == 0;
            }

            // Call the action function.
            //if(obj->action)
            //    obj->action(obj);
        }
        obj->timer = 0;
        //return true;
    }
    // End.

    //strcpy(obj->text, cb->active? cb->yes : cb->no);
    obj->text = cb->active? cb->yes : cb->no;

    if(CVT_NULL == Con_GetVariableType(cb->cvarname))
        return;

    if(cb->mask)
    {
        value = Con_GetInteger(cb->cvarname);
        if(cb->active)
        {
            value |= cb->mask;
        }
        else
        {
            value &= ~cb->mask;
        }
    }
    else
    {
        value = cb->active;
    }

    Con_SetInteger2(cb->cvarname, value, SVF_WRITE_OVERRIDE);
}

void Hu_MenuCvarList(mn_object_t* obj, int option)
{
    mndata_listinline_t* list = obj->data;
    cvartype_t varType = Con_GetVariableType(list->data);
    int oldsel = list->selection, value;

    // Respond to input event.
    if(0 == option)
    {
        if(list->selection > 0)
            list->selection--;
    }
    else
    {
        if(list->selection < list->count - 1)
            list->selection++;
    }

    // Adjust the first visible item.
    if(list->selection >= 0)
    {
        if(list->selection < list->first)
            list->first = list->selection;
        //if(list->selection > list->first + list->numvis - 1)
        //    list->first = list->selection - list->numvis + 1;
    }
    // Call action function?
    //if(oldsel != list->selection && obj->action)
    //    obj->action(obj);
    // End

    if(list->selection < 0)
        return; // Hmm?

    if(CVT_NULL == varType)
        return;

    value = ((mndata_listitem_t*) list->items)[list->selection].data;
    switch(varType)
    {
    case CVT_INT:
        Con_SetInteger2(list->data, value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        Con_SetInteger2(list->data, (byte) value, SVF_WRITE_OVERRIDE);
        break;
    default:
        Con_Error("Hu_MenuCvarList: Unsupported variable type %i", (int)varType);
        break;
    }
}

void Hu_MenuSaveSlotEdit(mn_object_t* obj, int option)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));
    obj->flags &= ~MNF_INACTIVE;
    }
}

void Hu_MenuCvarEdit(mn_object_t* obj, int option)
{
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    //cvartype_t varType;

    // Activate this.
    memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));
    obj->flags &= ~MNF_INACTIVE;

    /*varType = Con_GetVariableType(edit->data1);
    if(CVT_CHARPTR != varType) return;
    Con_SetString2(edit->data1, edit->text, SVF_WRITE_OVERRIDE);*/
}

void Hu_MenuCvarSlider(mn_object_t* obj, int option)
{
    mndata_slider_t* sldr = obj->data;
    cvartype_t varType = Con_GetVariableType(sldr->data1);
    float value;

    if(CVT_NULL == varType)
        return;

    // Respond to input event. 
    if(0 == option)
    {
        sldr->value -= sldr->step;
        if(sldr->value < sldr->min)
            sldr->value = sldr->min;
    }
    else
    {
        sldr->value += sldr->step;
        if(sldr->value > sldr->max)
            sldr->value = sldr->max;
    }
    value = sldr->value;
    // End.

    if(!sldr->floatMode)
    {
        value += (sldr->value < 0? -.5f : .5f);
    }

    if(varType == CVT_FLOAT)
    {
        if(sldr->step >= .01f)
        {
            Con_SetFloat2(sldr->data1, (int) (100 * value) / 100.0f, SVF_WRITE_OVERRIDE);
        }
        else
        {
            Con_SetFloat2(sldr->data1, value, SVF_WRITE_OVERRIDE);
        }
    }
    else if(varType == CVT_INT)
    {
        Con_SetInteger2(sldr->data1, (int) value, SVF_WRITE_OVERRIDE);
    }
    else if(varType == CVT_BYTE)
    {
        Con_SetInteger2(sldr->data1, (byte) value, SVF_WRITE_OVERRIDE);
    }
}

void M_DrawLoad(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("Load Game", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);
#else
    WI_DrawPatch4(m_loadg.id, x - 8, y - 26, "{case}Load game", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawSave(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("Save Game", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);
#else
    WI_DrawPatch4(m_loadg.id, x - 8, y - 26, "{case}Save game", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

/**
 * Draw border for the savegame description
 */
void M_DrawSaveLoadBorder(int x, int y, int width)
{
#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(1, 1, 1, mnAlpha);
    GL_DrawPatch(dpFSlot.id, x - 8, y - 4);
#else
    DGL_Color4f(1, 1, 1, mnAlpha);

    DGL_SetPatch(dpLSLeft.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_DrawRect(x, y - 3, dpLSLeft.width, dpLSLeft.height, 1, 1, 1, mnAlpha);
    DGL_SetPatch(dpLSRight.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_DrawRect(x + width - dpLSRight.width, y - 3, dpLSRight.width, dpLSRight.height, 1, 1, 1, mnAlpha);

    DGL_SetPatch(dpLSCntr.id, DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectTiled(x + dpLSLeft.width, y - 3, width - dpLSLeft.width - dpLSRight.width, 14, 8, 14);
#endif
}

int M_QuickSaveResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        const int slot = Con_GetInteger("game-save-quick-slot");
        if(0 > slot || slot >= NUMSAVESLOTS)
        {
            Con_Message("Warning:M_QuickSaveResponse: Nominated \"quicksave\" slot #%i "
                "is not valid, aborting...", slot);
            return true;
        }
        G_SaveGame(slot, edit_saveslots[slot].text);
    }
    return true;
}

/**
 * Called via the bindings mechanism when a player wishes to save their
 * game to a preselected save slot.
 */
static void M_QuickSave(void)
{
    player_t* player = &players[CONSOLEPLAYER];
    const int slot = Con_GetInteger("game-save-quick-slot");
    boolean slotIsUsed;
    char buf[80];

    if(player->playerState == PST_DEAD || Get(DD_PLAYBACK))
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, NULL, NULL);
        return;
    }

    if(G_GetGameState() != GS_MAP)
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, NULL, NULL);
        return;
    }

    // If no quick-save slot has been nominated - allow doing so now.
    if(0 > slot)
    {
        Hu_MenuCommand(MCMD_OPEN);
        MN_UpdateGameSaveWidgets();
        MN_GotoPage(&SaveMenu);
        nominatingQuickGameSaveSlot = true;
        return;
    }

    slotIsUsed = SV_IsGameSaveSlotUsed(slot);
    if(!slotIsUsed || !cfg.confirmQuickGameSave)
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        G_SaveGame(slot, edit_saveslots[slot].text);
        return;
    }

    if(slotIsUsed)
    {
        const gamesaveinfo_t* info = SV_GetGameSaveInfoForSlot(slot);
        sprintf(buf, QSPROMPT, Str_Text(&info->name));
    }
    else
    {
        char identifier[11];
        dd_snprintf(identifier, 10, "#%10.i", slot);
        dd_snprintf(buf, 80, QLPROMPT, identifier);
    }

    S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
    Hu_MsgStart(MSG_YESNO, buf, M_QuickSaveResponse, NULL);
}

int M_QuickLoadResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        const int slot = Con_GetInteger("game-save-quick-slot");
        G_LoadGame(slot);
    }
    return true;
}

static void M_QuickLoad(void)
{
    const int slot = Con_GetInteger("game-save-quick-slot");
    const gamesaveinfo_t* info;
    char buf[80];

    if(IS_NETGAME)
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QLOADNET, NULL, NULL);
        return;
    }

    if(0 > slot || !SV_IsGameSaveSlotUsed(slot))
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QSAVESPOT, NULL, NULL);
        return;
    }

    if(!cfg.confirmQuickGameSave)
    {
        G_LoadGame(slot);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        return;
    }

    info = SV_GetGameSaveInfoForSlot(slot);
    dd_snprintf(buf, 80, QLPROMPT, Str_Text(&info->name));

    S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
    Hu_MsgStart(MSG_YESNO, buf, M_QuickLoadResponse, NULL);
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void M_ReadThis(mn_object_t* obj, int option)
{
    G_StartHelp();
}
#endif

void M_DrawOptions(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("OPTIONS", x + 42, y - 38, GF_FONTB, DTF_ALIGN_TOP);
#else
# if __JDOOM64__
    WI_DrawPatch4(m_optttl.id, x + 42, y - 20, "{case}OPTIONS", GF_FONTB, true, DPF_ALIGN_TOP, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#else
    WI_DrawPatch4(m_optttl.id, x + 42, y - 20, "{case}OPTIONS", GF_FONTB, true, DPF_ALIGN_TOP, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
# endif
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawOptions2(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("SOUND OPTIONS", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawGameplay(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("GAMEPLAY", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawWeaponMenu(const mn_page_t* page, int x, int y)
{
    int i = 0;
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("WEAPONS", SCREENWIDTH/2, y-26, GF_FONTB, DTF_ALIGN_TOP);

/*#if __JDOOM__ || __JDOOM64__
    Hu_MenuPageString(buf, page);
    DGL_Color4f(cfg.menuColors[1][CR], cfg.menuColors[1][CG], cfg.menuColors[1][CB], Hu_MenuAlpha());
    M_DrawMenuText3(buf, SCREENWIDTH/2, y - 12, GF_FONTA, DTF_ALIGN_TOP);
#elif __JHERETIC__
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, Hu_MenuAlpha());
    GL_DrawPatch(dpInvPageLeft[!page->firstObject || (mnTime & 8)], x, y - 22);
    GL_DrawPatch(dpInvPageRight[page->firstObject + page->numVisObjects >= page->_size || (mnTime & 8)], 312 - x, y - 22);
#endif*/

    /**
     * \kludge Inform the user how to change the order.
     */
    /*if(mnFocusObjectIndex - 1 == 0)
    {
        const char* str = "Use left/right to move weapon up/down";
        DGL_Color4f(cfg.menuColors[1][CR], cfg.menuColors[1][CG], cfg.menuColors[1][CB], mnAlpha);
        M_DrawMenuText3(str, SCREENWIDTH/2, SCREENHEIGHT/2 + (95/cfg.menuScale), GF_FONTA, DTF_ALIGN_BOTTOM);
    }*/

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_WeaponOrder(mn_object_t* obj, int option)
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

            mnFocusObjectIndex++;
        }
    }
    else
    {
        if(choice > 0)
        {
            temp = cfg.weaponOrder[choice];
            cfg.weaponOrder[choice] = cfg.weaponOrder[choice-1];
            cfg.weaponOrder[choice-1] = temp;

            mnFocusObjectIndex--;
        }
    }
}

#if __JHERETIC__ || __JHEXEN__
void M_DrawInventoryMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("Inventory Options", SCREENWIDTH/2, y-28, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void M_DrawHUDMenu(const mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("HUD options", SCREENWIDTH/2, y - 20, GF_FONTB, DTF_ALIGN_TOP);

/*#if __JDOOM__ || __JDOOM64__
    Hu_MenuPageString(buf, page);
    DGL_Color4f(1, .7f, .3f, Hu_MenuAlpha());
    M_DrawMenuText3(buf, x + SCREENWIDTH/2, y + -12, GF_FONTA, DTF_ALIGN_TOP);
#else
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, Hu_MenuAlpha());
    GL_DrawPatch(dpInvPageLeft[!page->firstObject || (mnTime & 8)], x, y + -22);
    GL_DrawPatch(dpInvPageRight[page->firstObject + page->numVisObjects >= page->_size || (mnTime & 8)], 312 - x, y + -22);
#endif*/

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_FloatMod10(float* variable, int option)
{
    int val = (*variable + .05f) * 10;

    if(option == RIGHT_DIR)
    {
        if(val < 10)
            val++;
    }
    else if(val > 0)
        val--;
    *variable = val / 10.0f;
}

void M_WGCurrentColor(mn_object_t* obj, int option)
{
    M_FloatMod10(obj->data, option);
}

void M_SetMenu(mn_object_t* obj, int option)
{
    S_LocalSound(SFX_MENU_ACCEPT, NULL);
    MN_GotoPage((mn_page_t*)obj->data);
}

void M_NewGame(mn_object_t* obj, int option)
{
    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NEWGAME, NULL, NULL);
        return;
    }

#if __JHEXEN__
    MN_GotoPage(&PlayerClassMenu);
#elif __JHERETIC__
    MN_GotoPage(&EpisodeMenu);
#elif __JDOOM64__
    MN_GotoPage(&SkillLevelMenu);
#else // __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        MN_GotoPage(&SkillLevelMenu);
    else
        MN_GotoPage(&EpisodeMenu);
#endif
}

int M_QuitResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        G_SetGameAction(GA_QUIT);
    }

    return true;
}

void M_QuitDOOM(mn_object_t* obj, int option)
{
    const char* endString;
#if __JDOOM__ || __JDOOM64__
    endString = endmsg[((int) GAMETIC % (NUM_QUITMESSAGES + 1))];
#else
    endString = GET_TXT(TXT_QUITMSG);
#endif
    Con_Open(false);
    Hu_MsgStart(MSG_YESNO, endString, M_QuitResponse, NULL);
}

int M_EndGameResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        G_StartTitle();
        return true;
    }

    return true;
}

void M_EndGame(mn_object_t* obj, int option)
{
    if(!userGame)
    {
        Hu_MsgStart(MSG_ANYKEY, ENDNOGAME, NULL, NULL);
        return;
    }

    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NETEND, NULL, NULL);
        return;
    }

    Hu_MsgStart(MSG_YESNO, ENDGAME, M_EndGameResponse, NULL);
}

void M_ChangeMessages(mn_object_t* obj, int option)
{
    cfg.hudShown[HUD_LOG] = !cfg.hudShown[HUD_LOG];
    P_SetMessage(players + CONSOLEPLAYER, !cfg.hudShown[HUD_LOG] ? MSGOFF : MSGON, true);
}

#if __JDOOM__ || __JDOOM64__
void M_HUDRed(mn_object_t* obj, int option)
{
    M_FloatMod10(&cfg.hudColor[0], option);
}

void M_HUDGreen(mn_object_t* obj, int option)
{
    M_FloatMod10(&cfg.hudColor[1], option);
}

void M_HUDBlue(mn_object_t* obj, int option)
{
    M_FloatMod10(&cfg.hudColor[2], option);
}
#endif

void M_LoadGame(mn_object_t* obj, int option)
{
    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT && !Get(DD_PLAYBACK))
        {
            Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, NULL);
            return;
        }
    }

    MN_UpdateGameSaveWidgets();
    MN_GotoPage(&LoadMenu);
}

void M_SaveGame(mn_object_t* obj, int option)
{
    player_t* player = &players[CONSOLEPLAYER];

    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT)
        {
#if __JDOOM__ || __JDOOM64__
            Hu_MsgStart(MSG_ANYKEY, SAVENET, NULL, NULL);
#endif
            return;
        }

        if(G_GetGameState() != GS_MAP)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, NULL, NULL);
            return;
        }

        if(player->playerState == PST_DEAD)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, NULL, NULL);
            return;
        }
    }

    Hu_MenuCommand(MCMD_OPEN);
    MN_UpdateGameSaveWidgets();
    MN_GotoPage(&SaveMenu);
}

void M_ChooseClass(mn_object_t* obj, int option)
{
#if __JHEXEN__
    if(IS_NETGAME)
    {
        P_SetMessage(&players[CONSOLEPLAYER],
                     "YOU CAN'T START A NEW GAME FROM WITHIN A NETGAME!", false);
        return;
    }

    if(option < 0)
    {   // Random class.
        // Number of user-selectable classes.
        MenuPClass = (mnTime / 5) % (PlayerClassMenu._size - 1);
    }
    else
    {
        MenuPClass = option;
    }

    switch(MenuPClass)
    {
    case PCLASS_FIGHTER:
        SkillLevelMenu._offset[VX] = 120;
        SkillItems[0].text = GET_TXT(TXT_SKILLF1);
        SkillItems[1].text = GET_TXT(TXT_SKILLF2);
        SkillItems[2].text = GET_TXT(TXT_SKILLF3);
        SkillItems[3].text = GET_TXT(TXT_SKILLF4);
        SkillItems[4].text = GET_TXT(TXT_SKILLF5);
        break;

    case PCLASS_CLERIC:
        SkillLevelMenu._offset[VX] = 116;
        SkillItems[0].text = GET_TXT(TXT_SKILLC1);
        SkillItems[1].text = GET_TXT(TXT_SKILLC2);
        SkillItems[2].text = GET_TXT(TXT_SKILLC3);
        SkillItems[3].text = GET_TXT(TXT_SKILLC4);
        SkillItems[4].text = GET_TXT(TXT_SKILLC5);
        break;

    case PCLASS_MAGE:
        SkillLevelMenu._offset[VX] = 112;
        SkillItems[0].text = GET_TXT(TXT_SKILLM1);
        SkillItems[1].text = GET_TXT(TXT_SKILLM2);
        SkillItems[2].text = GET_TXT(TXT_SKILLM3);
        SkillItems[3].text = GET_TXT(TXT_SKILLM4);
        SkillItems[4].text = GET_TXT(TXT_SKILLM5);
        break;
    }
    MN_GotoPage(&SkillLevelMenu);
#endif
}

#if __JDOOM__ || __JHERETIC__
void M_Episode(mn_object_t* obj, int option)
{
#if __JHERETIC__
    if(gameMode == heretic_shareware && option)
    {
        Hu_MsgStart(MSG_ANYKEY, SWSTRING, NULL, NULL);
        G_StartHelp();
        return;
    }
#else
    if(gameMode == doom_shareware && option)
    {
        Hu_MsgStart(MSG_ANYKEY, SWSTRING, NULL, NULL);
        G_StartHelp();
        return;
    }
#endif

    epi = option;
    MN_GotoPage(&SkillLevelMenu);
}
#endif

#if __JDOOM__ || __JHERETIC__
int M_VerifyNightmare(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        Hu_MenuCommand(MCMD_CLOSEFAST);
        G_DeferedInitNew(SM_NIGHTMARE, epi, 0);
    }

    return true;
}
#endif

void M_ChooseSkill(mn_object_t* obj, int option)
{
#if __JHEXEN__
    Hu_MenuCommand(MCMD_CLOSEFAST);
    cfg.playerClass[CONSOLEPLAYER] = MenuPClass;
    G_DeferredNewGame(option);
#else
# if __JDOOM__
    if(option == SM_NIGHTMARE)
    {
        Hu_MsgStart(MSG_YESNO, NIGHTMARE, M_VerifyNightmare, NULL);
        return;
    }
# endif

    Hu_MenuCommand(MCMD_CLOSEFAST);

# if __JDOOM64__
    G_DeferedInitNew(option, 0, 0);
# else
    G_DeferedInitNew(option, epi, 0);
# endif
#endif
}

void M_OpenDCP(mn_object_t* obj, int option)
{
#define NUM_PANEL_NAMES 3
    static const char *panelNames[] = {
        "panel",
        "panel audio",
        "panel input"
    };
    int                 idx = option;

    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
        idx = 0;

    Hu_MenuCommand(MCMD_CLOSEFAST);
    DD_Execute(true, panelNames[idx]);

#undef NUM_PANEL_NAMES
}

/**
 * Routes menu commands, actions and navigation.
 */
D_CMD(MenuAction)
{
    if(G_GetGameAction() == GA_QUIT)
        return false;

    if(!mnActive)
    {
        if(!stricmp(argv[0], "menu") && !Chat_IsActive(CONSOLEPLAYER)) // Open menu.
        {
            Hu_MenuCommand(MCMD_OPEN);
            return true;
        }
    }
    else
    {
        mn_object_t* focusObj = focusObject();
        int mode = 0;

        // Determine what "mode" the menu is in currently.
        if(focusObj && !(focusObj->flags & MNF_INACTIVE))
        {
            switch(focusObj->type)
            {
            case MN_EDIT:       mode = 1; break;
            case MN_COLORBOX:   mode = 2; break;
            default: break;
            }
        }

        if(!stricmp(argv[0], "menuup"))
        {
            switch(mode)
            {
            case 2: // Widget edit
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
            switch(mode)
            {
            case 2: // Widget edit
            case 0: // Menu nav
                Hu_MenuCommand(MCMD_NAV_DOWN);
                break;

            default:
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menupagedown"))
        {
            switch(mode)
            {
            case 0: // Menu nav
            case 2: // Widget edit
                Hu_MenuCommand(MCMD_NAV_PAGEDOWN);
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menupageup"))
        {
            switch(mode)
            {
            case 0: // Menu nav
            case 2: // Widget edit
                Hu_MenuCommand(MCMD_NAV_PAGEUP);
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menuleft"))
        {
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
            if(!mode)
            {
                Hu_MenuCommand(MCMD_DELETE);
            }
            return true;
        }
        else if(!stricmp(argv[0], "menuselect"))
        {
            switch(mode)
            {
            case 0: // Menu nav
                Hu_MenuCommand(MCMD_SELECT);
                break;

            case 1: // Edit Field
                if(NULL != focusObj)
                {
                    mndata_edit_t* edit = (mndata_edit_t*)focusObj->data;
                    S_LocalSound(SFX_MENU_ACCEPT, NULL);
                    if(edit->onChange)
                    {
                        edit->onChange(focusObj);
                    }
                    focusObj->flags |= MNF_INACTIVE;
                }
                break;
            case 2: // Widget edit
                // Set the new color
                *widgetColors[editcolorindex].r = currentcolor[0];
                *widgetColors[editcolorindex].g = currentcolor[1];
                *widgetColors[editcolorindex].b = currentcolor[2];

                if(rgba)
                    *widgetColors[editcolorindex].a = currentcolor[3];

                // Restore the position of the cursor.
                mnFocusObjectIndex = mnPreviousFocusObjectIndex;

                focusObj->flags |= MNF_INACTIVE;
                S_LocalSound(SFX_MENU_ACCEPT, NULL);
                break;
            }
            return true;
        }
        else if(!stricmp(argv[0], "menuback"))
        {
            int c;

            switch(mode)
            {
            case 0: // Menu nav: Previous menu.
                Hu_MenuCommand(MCMD_NAV_OUT);
                break;

            case 1: // Edit Field: Del char.
                {
                mndata_edit_t* edit = (mndata_edit_t*) focusObj->data;
                c = strlen(edit->text);
                if(c > 0)
                    edit->text[c - 1] = 0;
                break;
                }
            case 2: // Widget edit: Close widget.
                // Restore the position of the cursor.
                mnFocusObjectIndex = mnPreviousFocusObjectIndex;
                focusObj->flags |= MNF_INACTIVE;
                S_LocalSound(SFX_MENU_CANCEL, NULL);
                break;
            }

            return true;
        }
        else if(!stricmp(argv[0], "menu"))
        {
            switch(mode)
            {
            case 0: // Menu nav: Close menu.
                Hu_MenuCommand(MCMD_CLOSE);
                break;

            case 1: // Edit Field.
                {
                mndata_edit_t* edit = (mndata_edit_t*) focusObj->data;
                memcpy(edit->text, edit->oldtext, sizeof(edit->text));
                focusObj->flags |= MNF_INACTIVE;
                break;
                }

            case 2: // Widget edit: Close widget.
                // Restore the position of the cursor.
                mnFocusObjectIndex = mnPreviousFocusObjectIndex;
                focusObj->flags |= MNF_INACTIVE;
                S_LocalSound(SFX_MENU_CLOSE, NULL);
                break;
            }

            return true;
        }
    }

    return true;
}

D_CMD(Shortcut)
{
    if(G_GetGameAction() == GA_QUIT)
        return false;

#if !__JDOOM64__
    if(!stricmp(argv[0], "helpscreen"))
    {
        G_StartHelp();
        return true;
    }
#endif
    if(!stricmp(argv[0], "ToggleGamma"))
    {
        R_CycleGammaLevel();
        return true;
    }
    if(!stricmp(argv[0], "ToggleMsgs"))
    {
        M_ChangeMessages(0, 0);
        return true;
    }
    // Menu-related hotkey shortcuts.
    if(!stricmp(argv[0], "SoundMenu"))
    {
        Hu_MenuCommand(MCMD_OPEN);
        MN_GotoPage(&SoundMenu);
        return true;
    }
    if(!stricmp(argv[0], "EndGame"))
    {
        M_EndGame(0, 0);
        return true;
    }
    if(!stricmp(argv[0], "QuickSave"))
    {
        M_QuickSave();
        return true;
    }
    if(!stricmp(argv[0], "QuickLoad"))
    {
        M_QuickLoad();
        return true;
    }

    return false;
}
