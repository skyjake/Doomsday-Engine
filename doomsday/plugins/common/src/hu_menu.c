/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
void M_QuitDOOM(mn_object_t* obj, int option);

void M_OpenDCP(mn_object_t* obj, int option);
void M_ChangeMessages(mn_object_t* obj, int option);
void M_HUDHideTime(mn_object_t* obj, int option);
void M_MessageUptime(mn_object_t* obj, int option);
#if __JHERETIC__ || __JHEXEN__
void M_InventoryHideTime(mn_object_t* obj, int option);
void M_InventorySlotMaxVis(mn_object_t* obj, int option);
#endif
void M_HUDInfo(mn_object_t* obj, int option);
void M_WeaponOrder(mn_object_t* obj, int option);
void M_HUDRed(mn_object_t* obj, int option);
void M_HUDGreen(mn_object_t* obj, int option);
void M_HUDBlue(mn_object_t* obj, int option);
void M_LoadSelect(mn_object_t* obj, int option);

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void M_KillCounter(mn_object_t* obj, int option);
void M_ItemCounter(mn_object_t* obj, int option);
void M_SecretCounter(mn_object_t* obj, int option);
#endif

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
void M_DrawFilesMenu(const mn_page_t* page, int x, int y);

void M_DoSaveGame(const mndata_edit_t* ef);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void drawColorWidget(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char* weaponNames[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if __JDOOM__ || __JDOOM64__
/// The end message strings will be initialized in Hu_MenuInit().
char* endmsg[NUM_QUITMESSAGES + 1];
#endif

// -1 = no quicksave slot picked!
int quickSaveSlot;

mndata_edit_t saveGameDescriptions[NUMSAVESLOTS];

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

static char notDesignedForMessage[80];

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
    { MN_BUTTON,    0,  0,  "{case}New Game", GF_FONTB, &m_ngame.id, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "{case}Options", GF_FONTB, &m_option.id, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "{case}Load game", GF_FONTB, &m_loadg.id, MNButton_Drawer, MNButton_Dimensions, M_LoadGame },
    { MN_BUTTON,    0,  0,  "{case}Save game", GF_FONTB, &m_saveg.id, MNButton_Drawer, MNButton_Dimensions, M_SaveGame },
    { MN_BUTTON,    0,  0,  "{case}Read This!", GF_FONTB, &m_rdthis.id, MNButton_Drawer, MNButton_Dimensions, M_ReadThis },
    { MN_BUTTON,    0,  0,  "{case}Quit Game", GF_FONTB, &m_quitg.id, MNButton_Drawer, MNButton_Dimensions, M_QuitDOOM },
#elif __JDOOM64__
    { MN_BUTTON,    0,  0,  "{case}New Game", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "{case}Options", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "{case}Load Game", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_LoadGame },
    { MN_BUTTON,    0,  0,  "{case}Save Game", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_SaveGame },
    { MN_BUTTON,    0,  0,  "{case}Quit Game", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_QuitDOOM },
#else
    { MN_BUTTON,    0,  0,  "New Game", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "Options",  GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "Game Files", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &FilesMenu },
    { MN_BUTTON,    0,  0,  "Info",     GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ReadThis },
    { MN_BUTTON,    0,  0,  "Quit Game", GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_QuitDOOM },
#endif
    { MN_NONE }
};

#if __JHEXEN__
mn_page_t MainMenu = {
    MainItems, 5,
    0,
    { 110, 50 },
    M_DrawMainMenu,
    0, 0,
    0, 5
};
#elif __JHERETIC__
mn_page_t MainMenu = {
    MainItems, 5,
    0,
    { 110, 56 },
    M_DrawMainMenu,
    0, 0,
    0, 5
};
#elif __JDOOM64__
mn_page_t MainMenu = {
    MainItems, 5,
    0,
    { 97, 64 },
    M_DrawMainMenu,
    0, 0,
    0, 5
};
#elif __JDOOM__
mn_page_t MainMenu = {
    MainItems, 6,
    0,
    { 97, 64 },
    M_DrawMainMenu,
    0, 0,
    0, 6
};
#endif

mn_object_t NewGameItems[] = {
    { MN_BUTTON,    0,  0,  "S",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_NewGame },
    { MN_BUTTON,    0,  0,  "M",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, SCEnterMultiplayerMenu },
    { MN_NONE }
};

#if __JHEXEN__
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 110, 50 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    0, 2
};
#elif __JHERETIC__
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 110, 64 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    0, 2
};
#elif __JDOOM64__
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 97, 64 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    0, 2
};
#else
mn_page_t GameTypeMenu = {
    NewGameItems, 2,
    0,
    { 97, 64 },
    M_DrawNewGameMenu,
    0, &MainMenu,
    0, 2
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
    0, 0
};
#endif

#if __JDOOM__ || __JHERETIC__
static mn_object_t* EpisodeItems;
#endif

#if __JDOOM__ || __JHERETIC__
mn_page_t EpisodeMenu = {
    0, 0,
    0,
# if __JDOOM__
    { 48, 63 },
# else
    { 48, 50 },
# endif
    M_DrawEpisode,
    0, &GameTypeMenu,
    0, 0
};
#endif


#if __JHERETIC__ || __JHEXEN__
static mn_object_t FilesItems[] = {
    { MN_BUTTON,    0,  0,  "Load Game",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_LoadGame },
    { MN_BUTTON,    0,  0,  "Save Game",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_SaveGame },
    { MN_NONE }
};

mn_page_t FilesMenu = {
    FilesItems, 2,
    0,
    { 110, 60 },
    M_DrawFilesMenu,
    0, &MainMenu,
    0, 2
};
#endif

static mn_object_t LoadItems[] = {
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[0], 0 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[1], 1 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[2], 2 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[3], 3 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[4], 4 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[5], 5 },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[6], 6 },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_LoadSelect, &saveGameDescriptions[7], 7 },
#endif
    { MN_NONE }
};

mn_page_t LoadMenu = {
    LoadItems, NUMSAVESLOTS,
    0,
#if __JDOOM__ || __JDOOM64__
    { 64, 44 },
#else
    { 64, 30 },
#endif
    M_DrawLoad,
    0, &MainMenu,
    0, NUMSAVESLOTS
};

static mn_object_t SaveItems[] = {
#if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,              "{case}Save game", GF_FONTB, &m_saveg.id, MNText_Drawer, MNText_Dimensions },
#elif __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,              "Save Game", GF_FONTB, 0, MNText_Drawer, MNText_Dimensions },
#endif
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[0] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[1] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[2] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[3] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[4] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[5] },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[6] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         GF_FONTA, 0, MNEdit_Drawer, MNEdit_Dimensions, M_ActivateEditField, &saveGameDescriptions[7] },
#endif
    { MN_NONE }
};

mn_page_t SaveMenu = {
    SaveItems, 1+NUMSAVESLOTS,
    0,
#if __JDOOM__ || __JDOOM64__
    { 64, 24 },
#else
    { 64, 10 },
#endif
    NULL,
    1, &MainMenu,
    0, 1+NUMSAVESLOTS
};

#if __JHEXEN__
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_BABY },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_EASY },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_HARD },
    { MN_BUTTON,    0,  0,  "",     GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillLevelMenu = {
    SkillItems, 5,
    0,
    { 120, 44 },
    M_DrawSkillMenu,
    2, &PlayerClassMenu,
    0, 5
};
#elif __JHERETIC__
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,  "W",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_BABY },
    { MN_BUTTON,    0,  0,  "Y",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_EASY },
    { MN_BUTTON,    0,  0,  "B",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,  "S",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_HARD },
    { MN_BUTTON,    0,  0,  "P",    GF_FONTB, 0, MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillLevelMenu = {
    SkillItems, 5,
    0,
    { 38, 30 },
    M_DrawSkillMenu,
    2, &EpisodeMenu,
    0, 5
};
#elif __JDOOM64__
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,  "I",    GF_FONTB, &skillModeNames[0], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 0 },
    { MN_BUTTON,    0,  0,  "H",    GF_FONTB, &skillModeNames[1], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 1 },
    { MN_BUTTON,    0,  0,  "H",    GF_FONTB, &skillModeNames[2], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 2 },
    { MN_BUTTON,    0,  0,  "U",    GF_FONTB, &skillModeNames[3], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 3 },
    { MN_NONE }
};
static mn_page_t SkillLevelMenu = {
    SkillItems, 4,
    0,
    { 48, 63 },
    M_DrawSkillMenu,
    2, &GameTypeMenu,
    0, 4
};
#else
static mn_object_t SkillItems[] = {
    { MN_BUTTON,    0,  0,              "I",    GF_FONTB, &skillModeNames[0], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 0 },
    { MN_BUTTON,    0,  0,              "H",    GF_FONTB, &skillModeNames[1], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 1 },
    { MN_BUTTON,    0,  0,              "H",    GF_FONTB, &skillModeNames[2], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 2 },
    { MN_BUTTON,    0,  0,              "U",    GF_FONTB, &skillModeNames[3], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 3 },
    { MN_BUTTON,    0,  MNF_NO_ALTTEXT, "N",    GF_FONTB, &skillModeNames[4], MNButton_Drawer, MNButton_Dimensions, M_ChooseSkill, 0, 4 },
    { MN_NONE }
};

mn_page_t SkillLevelMenu = {
    SkillItems, 5,
    0,
    { 48, 63 },
    M_DrawSkillMenu,
    2, &EpisodeMenu,
    0, 5
};
#endif

static mn_object_t OptionsItems[] = {
    { MN_BUTTON,    0,  0,  "End Game", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_EndGame },
    { MN_BUTTON,    0,  0,  "Control Panel", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 0 },
    { MN_BUTTON,    0,  0,  "Controls", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &ControlsMenu },
    { MN_BUTTON,    0,  0,  "Gameplay", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &GameplayMenu },
    { MN_BUTTON,    0,  0,  "HUD",      GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &HUDMenu },
    { MN_BUTTON,    0,  0,  "Automap",  GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &AutomapMenu },
    { MN_BUTTON,    0,  0,  "Weapons",  GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &WeaponMenu },
#if __JHERETIC__ || __JHEXEN__
    { MN_BUTTON,    0,  0,  "Inventory", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &InventoryMenu },
#endif
    { MN_BUTTON,    0,  0,  "Sound",    GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_SetMenu, &SoundMenu },
    { MN_BUTTON,    0,  0,  "Mouse",    GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 2 },
    { MN_BUTTON,    0,  0,  "Joystick", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 2 },
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
    0,
#if __JHERETIC__ || __JHEXEN__
    11
#else
    10
#endif
};

mndata_slider_t sld_sound_volume = { 0, 255, 0, 1, false, "sound-volume" };
mndata_slider_t sld_music_volume = { 0, 255, 0, 1, false, "music-volume" };

mn_object_t SoundMenuItems[] = {
    { MN_TEXT,      0,  0,  "SFX Volume", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_sound_volume },
    { MN_TEXT,      0,  0,  "Music Volume", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_music_volume },
    { MN_BUTTON,    0,  0,  "Open Audio Panel", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_OpenDCP, 0, 1 },
    { MN_NONE }
};

mn_page_t SoundMenu = {
    SoundMenuItems, 5,
    0,
#if __JHEXEN__
    { 70, 25 },
#elif __JHERETIC__
    { 70, 30 },
#elif __JDOOM__ || __JDOOM64__
    { 70, 40 },
#endif
    M_DrawOptions2,
    0, &OptionsMenu,
    0, 5
};

#if __JDOOM64__
mndata_slider_t sld_hud_viewsize = { 0, 11, 0, 1, false, "view-size" };
#else
mndata_slider_t sld_hud_viewsize = { 0, 13, 0, 1, false, "view-size" };
#endif
mndata_slider_t sld_hud_wideoffset = { 0, 1, 0, .1f, true, "hud-wideoffset" };
mndata_slider_t sld_hud_xhair_size = { 0, 1, 0, .1f, true, "view-cross-size" };
mndata_slider_t sld_hud_xhair_opacity = { 0, 1, 0, .1f, true, "view-cross-a" };
mndata_slider_t sld_hud_size = { 0, 1, 0, .1f, true, "hud-scale" };
mndata_slider_t sld_hud_counter_size = { 0, 1, 0, .1f, true, "hud-cheat-counter-scale" };
mndata_slider_t sld_hud_statusbar_size = { 0, 1, 0, .1f, true, "hud-status-size" };
mndata_slider_t sld_hud_statusbar_opacity = { 0, 1, 0, .1f, true, "hud-status-alpha" };
mndata_slider_t sld_hud_messages_size = { 0, 1, 0, .1f, true, "msg-scale" };

mndata_listitem_t lstit_hud_xhair_symbols[] = {
    { "None", 0 },
    { "Cross", 1 },
    { "Angles", 2 },
    { "Square", 3 },
    { "Open Square", 4 },
    { "Diamond", 5 },
    { "V", 6 }
};
mndata_list_t lst_hud_xhair_symbol = {
    lstit_hud_xhair_symbols, NUMLISTITEMS(lstit_hud_xhair_symbols), "view-cross-type"
};

static mn_object_t HUDItems[] = {
    { MN_TEXT,      0,  0,  "View Size", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "", GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_viewsize },
    { MN_TEXT,      0,  0,  "Wide Offset", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "", GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_wideoffset },
#if __JDOOM__
    { MN_TEXT,      0,  0,  "Single Key Display", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-keys-combine", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_LIST,      0,  0,  "AutoHide", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, M_HUDHideTime },
    { MN_TEXT,      0,  0,  "UnHide Events", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_TEXT,      0,  0,  "Receive Damage", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-damage", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Health", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-health", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Armor", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-armor", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Powerup", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-powerup", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Weapon", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-weapon", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  "Pickup Mana", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-ammo", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#else
    { MN_TEXT,      0,  0,  "Pickup Ammo", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-ammo", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_TEXT,      0,  0,  "Pickup Key", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-key", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Pickup Item", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-invitem", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif

    { MN_TEXT,      0,  0,  "Messages", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_BUTTON2,   0,  0,  "Shown",    GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, M_ChangeMessages },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_messages_size },
    { MN_LIST,      0,  0,  "Uptime",   GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, M_MessageUptime },

    { MN_TEXT,      0,  0,  "Crosshair", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_TEXT,      0,  0,  "Symbol",   GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,  "",         GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, Hu_MenuCvarList, &lst_hud_xhair_symbol },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_xhair_size },
    { MN_TEXT,      0,  0,  "Opacity",  GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",  GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_xhair_opacity },
    { MN_TEXT,      0,  0,  "Vitality Color", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "view-cross-vitality", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Color",    GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_COLORBOX,  0,  MNF_INACTIVE, "", GF_FONTA, 0, MNColorBox_Drawer, MNColorBox_Dimensions, MN_ActivateColorBox, 0, 7 },

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Statusbar", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_statusbar_size },
    { MN_TEXT,      0,  0,  "Opacity",  GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_statusbar_opacity },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Counters", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_LIST,      0,  0,  "Kills",    GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, M_KillCounter },
    { MN_LIST,      0,  0,  "Items",    GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, M_ItemCounter },
    { MN_LIST,      0,  0,  "Secrets",  GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, M_SecretCounter },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_counter_size },
#endif

    { MN_TEXT,      0,  0,  "Fullscreen HUD", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_TEXT,      0,  0,  "Size",     GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, Hu_MenuCvarSlider, &sld_hud_size },
    { MN_TEXT,      0,  0,  "Text Color", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_COLORBOX,  0,  MNF_INACTIVE, "", GF_FONTA, 0, MNColorBox_Drawer, MNColorBox_Dimensions, MN_ActivateColorBox, 0, 5 },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  "Show Mana", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-mana", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Show Ammo", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-ammo", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Show Armor", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-armor", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0,  "Show Power Keys", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-power", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM__
    { MN_TEXT,      0,  0,  "Show Face", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-face", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_TEXT,      0,  0,  "Show Health", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-health", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Show Keys", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-keys", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Show Item", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-currentitem", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
    { MN_NONE }
};

mn_page_t HUDMenu = {
#if __JHEXEN__
    HUDItems, 54,
#elif __JHERETIC__
    HUDItems, 63,
#elif __JDOOM64__
    HUDItems, 59,
#elif __JDOOM__
    HUDItems, 63,
#endif
    0,
#if __JDOOM__ || __JDOOM64__
    { 80, 40 },
#else
    { 80, 28 },
#endif
    M_DrawHUDMenu,
    0, &OptionsMenu,
#if __JHEXEN__
    0, 15        // 21
#elif __JHERETIC__
    0, 15        // 23
#elif __JDOOM64__
    0, 19
#elif __JDOOM__
    0, 19
#endif
};

#if __JHERETIC__ || __JHEXEN__
mndata_button_t btn_inv_selectmode = { &cfg.inventorySelectMode, "Scroll", "Cursor" };

static mn_object_t InventoryItems[] = {
    { MN_BUTTON2EX, 0,  0,  "Select Mode", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton, &btn_inv_selectmode },
    { MN_TEXT,      0,  0,  "Wrap Around", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-wrap", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Choose And Use", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-use-immediate", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Select Next If Use Failed", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-use-next", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_LIST,      0,  0,  "AutoHide", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, M_InventoryHideTime },

    { MN_TEXT,      0,  0,  "Fullscreen HUD", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_LIST,      0,  0,  "Max Visible Slots", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, M_InventorySlotMaxVis, "hud-inventory-slot-max" },
    { MN_TEXT,      0,  0,  "Show Empty Slots", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-inventory-slot-showempty", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_NONE }
};

mn_page_t InventoryMenu = {
    InventoryItems, 12,
    0,
    { 78, 48 },
    M_DrawInventoryMenu,
    0, &OptionsMenu,
    0, 12, { 12, 48 }
};
#endif

mndata_listitem_t lstit_weapons_order[NUM_WEAPON_TYPES];
mndata_list_t lst_weapons_order = {
    lstit_weapons_order, NUMLISTITEMS(lstit_weapons_order)
};

mndata_listitem_t lstit_weapons_autoswitch_pickup[] = {
    { "Never", 0 },
    { "If Better", 1 },
    { "Always", 2 }
};
mndata_list_t lst_weapons_autoswitch_pickup = {
    lstit_weapons_autoswitch_pickup, NUMLISTITEMS(lstit_weapons_autoswitch_pickup), "player-autoswitch"
};

mndata_listitem_t lstit_weapons_autoswitch_pickupammo[] = {
    { "Never", 0 },
    { "If Better", 1 },
    { "Always", 2 }
};
mndata_list_t lst_weapons_autoswitch_pickupammo = {
    lstit_weapons_autoswitch_pickupammo, NUMLISTITEMS(lstit_weapons_autoswitch_pickupammo), "player-autoswitch-ammo"
};

static mn_object_t WeaponItems[] = {
    { MN_TEXT,      0,  0,  "Priority Order", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2},
    { MN_LIST,      0,  0,  "",         GF_FONTA, 0, MNList_Drawer, MNList_Dimensions, M_WeaponOrder, &lst_weapons_order },
    { MN_TEXT,      0,  0,  "Cycling",  GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_TEXT,      0,  0,  "Use Priority Order", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-weapon-nextmode", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Sequential", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-weapon-cycle-sequential", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },

    { MN_TEXT,      0,  0,  "Autoswitch", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
    { MN_TEXT,      0,  0,  "Pickup Weapon", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,  "", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, Hu_MenuCvarList, &lst_weapons_autoswitch_pickup },
    { MN_TEXT,      0,  0,  "   If Not Firing", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-autoswitch-notfiring", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Pickup Ammo", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_LIST,      0,  0,  "", GF_FONTA, 0, MNList_InlineDrawer, MNList_InlineDimensions, Hu_MenuCvarList, &lst_weapons_autoswitch_pickupammo },
#if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Pickup Beserk", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "player-autoswitch-berserk", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
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
    0, 12, { 12, 38 }
};

static mn_object_t GameplayItems[] = {
    { MN_TEXT,      0,  0, "Always Run",    GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "ctl-run",    GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0, "Use LookSpring", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "ctl-look-spring", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0, "Use AutoAim",   GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "ctl-aim-noauto",   GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0, "Allow Jumping", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0, "player-jump", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0, "Weapon Recoil", GF_FONTA },
    { MN_BUTTON2,   0,  0, "player-weapon-recoil", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Compatibility", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions, 0, 0, MENU_COLOR2 },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Any Boss Trigger 666", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-anybossdeath666", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#  if !__JDOOM64__
    { MN_TEXT,      0,  0,  "Av Resurrects Ghosts", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-raiseghosts", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
#  endif
    { MN_TEXT,      0,  0,  "PE Limited To 21 Lost Souls", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-maxskulls", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "LS Can Get Stuck Inside Walls", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-skullsinwalls", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
# endif
    { MN_TEXT,      0,  0,  "Monsters Can Get Stuck In Doors", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-monsters-stuckindoors", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Some Objects Never Hang Over Ledges", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-objects-neverhangoverledges", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Objects Fall Under Own Weight", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-objects-falloff", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Corpses Slide Down Stairs", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-corpse-sliding", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Use Exactly Doom's Clipping Code", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-objects-clipping", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "  ^If Not NorthOnly WallRunning", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-player-wallrun-northonly", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Zombie Players Can Exit Maps", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "game-zombiescanexit", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Fix Ouch Face", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-face-ouchfix", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
    { MN_TEXT,      0,  0,  "Fix Weapon Slot Display", GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_BUTTON2,   0,  0,  "hud-status-weaponslots-ownedfix", GF_FONTA, 0, MNButton_Drawer, MNButton_Dimensions, Hu_MenuCvarButton },
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
    0, 6, { 6, 25 }
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
    0, 16, { 16, 40 }
#elif __JDOOM__
    0, 18, { 18, 40 }
#else
    0, 11, { 11, 40 }
#endif
};
#endif

static mn_object_t ColorWidgetItems[] = {
    { MN_TEXT,      0,  0,  "Red",      GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[0] },
    { MN_TEXT,      0,  0,  "Green",    GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[1] },
    { MN_TEXT,      0,  0,  "Blue",     GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[2] },
    { MN_TEXT,      0,  0,  "Alpha",    GF_FONTA, 0, MNText_Drawer, MNText_Dimensions },
    { MN_SLIDER,    0,  0,  "",         GF_FONTA, 0, MNSlider_Drawer, MNSlider_Dimensions, M_WGCurrentColor, &currentcolor[3] },
    { MN_NONE }
};

static mn_page_t ColorWidgetMenu = {
    ColorWidgetItems, 8,
    MNPF_NOHOTKEYS,
    { 98, 60 },
    NULL,
    0, &OptionsMenu,
    0, 8
};

// Cvars for the menu:
cvar_t menuCVars[] = {
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
    { "menu-quick-ask", 0,  CVT_BYTE,   &cfg.askQuickSaveLoad, 0, 1 },
    { "menu-hotkeys",   0,  CVT_BYTE,   &cfg.menuHotkeys, 0, 1 },
#if __JDOOM__ || __JDOOM64__
    { "menu-quitsound", 0,  CVT_INT,    &cfg.menuQuitSound, 0, 1 },
#endif
    { NULL }
};

// Console commands for the menu:
ccmd_t menuCCmds[] = {
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
    { "savegame",       "",     CCmdMenuAction },
    { "loadgame",       "",     CCmdMenuAction },
    { "soundmenu",      "",     CCmdMenuAction },
    { "quicksave",      "",     CCmdMenuAction },
    { "endgame",        "",     CCmdMenuAction },
    { "togglemsgs",     "",     CCmdMenuAction },
    { "quickload",      "",     CCmdMenuAction },
    { "quit",           "",     CCmdMenuAction },
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
    if(!mnActive)
        return NULL;
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
        mndata_listitem_t* item = &((mndata_listitem_t*)lstit_weapons_order)[i];
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

    qsort(lstit_weapons_order, NUM_WEAPON_TYPES, sizeof(lstit_weapons_order[0]), compareWeaponPriority);
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
    EpisodeItems = Z_Calloc(sizeof(mn_object_t) * (numEpisodes+1), PU_STATIC, 0);

    for(i = 0, maxw = 0; i < numEpisodes; ++i)
    {
        mn_object_t* obj = &EpisodeItems[i];

        obj->type = MN_BUTTON;
        obj->drawer = MNButton_Drawer;
        obj->dimensions = MNButton_Dimensions;
        obj->action = M_Episode;
        obj->data2 = i;
        obj->text = GET_TXT(TXT_EPISODE1 + i);
        obj->font = GF_FONTB;
        w = GL_TextWidth(obj->text, obj->font);
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
    EpisodeMenu.numVisObjects = MIN_OF(EpisodeMenu._size, 10);
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
    ClassItems = Z_Calloc(sizeof(mn_object_t) * (count + 1), PU_STATIC, 0);

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
        obj->font = GF_FONTB;

        n++;
    }

    // Add the random class option.
    ClassItems[n].type = MN_BUTTON;
    ClassItems[n].drawer = MNButton_Drawer;
    ClassItems[n].dimensions = MNButton_Dimensions;
    ClassItems[n].action = M_ChooseClass;
    ClassItems[n].data2 = -1;
    ClassItems[n].text = GET_TXT(TXT_RANDOMPLAYERCLASS);
    ClassItems[n].font = GF_FONTB;

    // Finalize setup.
    PlayerClassMenu._objects = ClassItems;
    PlayerClassMenu._size = count + 1;
    PlayerClassMenu.numVisObjects = MIN_OF(PlayerClassMenu._size, 10);
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
    int i;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    R_GetGammaMessageStrings();
#endif

#if __JDOOM__ || __JDOOM64__
    // Quit messages.
    endmsg[0] = GET_TXT(TXT_QUITMSG);
    for(i = 1; i <= NUM_QUITMESSAGES; ++i)
        endmsg[i] = GET_TXT(TXT_QUITMESSAGE1 + i - 1);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Skill names.
    for(i = 0, maxw = 0; i < NUM_SKILL_MODES; ++i)
    {
        SkillItems[i].text = GET_TXT(TXT_SKILL1 + i);
        w = GL_TextWidth(SkillItems[i].text, SkillItems[i].font);
        if(w > maxw)
            maxw = w;
    }
    // Center the skill menu appropriately.
    SkillLevelMenu._offset[VX] = SCREENWIDTH/2 - maxw / 2 + 12;
#endif

    // Play modes.
    NewGameItems[0].text = GET_TXT(TXT_SINGLEPLAYER);
    NewGameItems[1].text = GET_TXT(TXT_MULTIPLAYER);

    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        mndata_edit_t* edit = &saveGameDescriptions[i];
        edit->data = i;
        edit->emptyString = EMPTYSTRING;
        edit->onChange = M_DoSaveGame;
    }

    mnCurrentPage = &MainMenu;
    mnActive = false;
    DD_Execute(true, "deactivatebcontext menu");
    mnAlpha = mnTargetAlpha = 0;

    M_LoadData();

    mnFocusObjectIndex = mnCurrentPage->focus;
    whichSkull = 0;
    skullAnimCounter = MN_CURSOR_TICSPERFRAME;
    quickSaveSlot = -1;

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
#define MENUALPHA_FADE_STEP (.0825)

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

    if(mnActive || mnAlpha > 0)
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

            if(focusObj && !(focusObj->flags & (MNF_DISABLED|MNF_INACTIVE)) && (focusObj->type == MN_LIST || focusObj->type == MN_SLIDER))
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

    if(mnActive || mnAlpha > 0)
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

        MN_TickerEx();
    }
}

void Hu_MenuPageString(char* str, const mn_page_t* page)
{
    sprintf(str, "PAGE %i/%i", (page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1,
            (int)ceil((float)page->_size/page->numVisObjects));
}

static void calcNumVisObjects(mn_page_t* page)
{
    page->firstObject = MAX_OF(0, mnFocusObjectIndex - page->numVisObjects/2);
    page->firstObject = MIN_OF(page->firstObject, page->_size - page->numVisObjects);
    page->firstObject = MAX_OF(0, page->firstObject);
}

mn_page_t* MN_CurrentPage(void)
{
    return mnCurrentPage;
}

void MN_GotoPage(mn_page_t* page)
{
    if(!mnActive)
        return;
    if(!page)
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

    calcNumVisObjects(mnCurrentPage);

    menu_color = 0;
    skull_angle = 0;
    R_ResetTextTypeInTimer();
}

/**
 * This is the main menu drawing routine (called every tic by the drawing
 * loop) Draws the current menu 'page' by calling the funcs attached to
 * each menu obj.
 */
void Hu_MenuDrawer(void)
{
    float pos[2], offset[2];
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

    if(mnCurrentPage->unscaled.numVisObjects)
    {
        mnCurrentPage->numVisObjects = mnCurrentPage->unscaled.numVisObjects / cfg.menuScale;
        mnCurrentPage->_offset[VY] = (SCREENHEIGHT/2) - ((SCREENHEIGHT/2) - mnCurrentPage->unscaled.y) / cfg.menuScale;
    }

    if(mnCurrentPage->drawer)
        mnCurrentPage->drawer(mnCurrentPage, mnCurrentPage->_offset[VX], mnCurrentPage->_offset[VY]);

    pos[VX] = mnCurrentPage->_offset[VX];
    pos[VY] = mnCurrentPage->_offset[VY];

    if(mnAlpha > 0.0125f)
    {
        const mn_object_t* focusObj = focusObject();
        for(i = mnCurrentPage->firstObject; i < mnCurrentPage->_size && i < mnCurrentPage->firstObject + mnCurrentPage->numVisObjects; ++i)
        {
            const mn_object_t* obj = &mnCurrentPage->_objects[i];
            int height = 0;

            if(obj->type == MN_NONE||(obj->flags & MNF_HIDDEN)||!obj->drawer)
                continue;

            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();
            DGL_Translatef(pos[VX], pos[VY], 0);

            obj->drawer(obj, 0, 0, mnAlpha);

            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();

            if(obj->dimensions)
                obj->dimensions(obj, NULL, &height);

            // Draw the cursor?
            if(obj == focusObj)
            {
#if __JDOOM__ || __JDOOM64__
# define MN_CURSOR_OFFSET_X         (-22)
# define MN_CURSOR_OFFSET_Y         (0)
#elif __JHERETIC__ || __JHEXEN__
# define MN_CURSOR_OFFSET_X         (-2)
# define MN_CURSOR_OFFSET_Y         (-1)
#endif

                mn_page_t* mn = ((focusObj && focusObj->type == MN_COLORBOX && !(focusObj->flags & MNF_INACTIVE))? &ColorWidgetMenu : mnCurrentPage);
                float scale = MIN_OF((float) height / cursorst[whichSkull].height, 1);

                offset[VX] = MN_CURSOR_OFFSET_X * scale;
                offset[VY] = height/2 + MN_CURSOR_OFFSET_Y * scale;

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

            pos[VY] += height * (1+.125f); // Leading.
        }

        // Draw the colour widget?
        if(focusObj && focusObj->type == MN_COLORBOX && !(focusObj->flags & MNF_INACTIVE))
        {
            Draw_BeginZoom(0.5f, SCREENWIDTH/2, SCREENHEIGHT/2);
            drawColorWidget();
            Draw_EndZoom();
        }
    }

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
}

/**
 * Execute a menu navigation/action command.
 */
void Hu_MenuCommand(menucommand_e cmd)
{
    if(cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST)
    {
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
            mnCurrentPage->focus = mnFocusObjectIndex;
            mnActive = false;

            if(cmd != MCMD_CLOSEFAST)
                S_LocalSound(SFX_MENU_CLOSE, NULL);

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
            R_ResetTextTypeInTimer();

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

        firstVisible = menu->firstObject;
        lastVisible = firstVisible + menu->numVisObjects - 1 - numVisObjectsOffset;
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
            if((obj->type == MN_SLIDER || obj->type == MN_LIST) && obj->action != NULL)
            {
                S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
                obj->action(obj, LEFT_DIR);
            }
            break;

        case MCMD_NAV_RIGHT:
            if((obj->type == MN_SLIDER || obj->type == MN_LIST) && obj->action != NULL)
            {
                S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
                obj->action(obj, RIGHT_DIR);
            }
            break;

        case MCMD_NAV_PAGEUP:
        case MCMD_NAV_PAGEDOWN:
            S_LocalSound(SFX_MENU_NAV_UP, NULL);
            Hu_MenuNavigatePage(menu, cmd == MCMD_NAV_PAGEUP? -1 : +1);
            break;

        case MCMD_NAV_DOWN:
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
            S_LocalSound(SFX_MENU_NAV_UP, NULL);
            calcNumVisObjects(mnCurrentPage);
            break;

        case MCMD_NAV_UP:
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
            break;

        case MCMD_NAV_OUT:
            menu->focus = hasFocus;
            if(!menu->previous)
            {
                menu->focus = hasFocus;
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
            if(obj->action)
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
                    S_LocalSound(SFX_MENU_CYCLE, NULL);
                    obj->action(obj, (obj->type == MN_LIST? RIGHT_DIR : obj->data2));
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

        first = last = page->firstObject;
        last += page->numVisObjects - 1;

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

void M_DrawMenuText5(const char* string, int x, int y, compositefontid_t font, short flags,
    float glitterStrength, float shadowStrength)
{
    if(cfg.menuEffects == 0)
    {
        flags |= DTF_NO_TYPEIN;
        glitterStrength = 0;
        shadowStrength = 0;
    }

    GL_DrawTextFragment7(string, x, y, font, flags, 0, 0, glitterStrength, shadowStrength);
}

void M_DrawMenuText4(const char* string, int x, int y, compositefontid_t font, short flags, float glitterStrength)
{
    M_DrawMenuText5(string, x, y, font, flags, glitterStrength, cfg.menuShadow);
}

void M_DrawMenuText3(const char* string, int x, int y, compositefontid_t font, short flags)
{
    M_DrawMenuText4(string, x, y, font, flags, cfg.menuGlitter);
}

void M_DrawMenuText2(const char* string, int x, int y, compositefontid_t font)
{
    M_DrawMenuText3(string, x, y, font, DTF_ALIGN_TOPLEFT);
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
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    filename_t name;
#endif

    SaveMenu.focus = option+1;

    Hu_MenuCommand(MCMD_CLOSEFAST);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    SV_GetSaveGameFileName(name, option, FILENAME_T_MAXLEN);
    G_LoadGame(name);
#else
    G_LoadGame(option);
#endif
}

void M_DrawMainMenu(const mn_page_t* page, int x, int y)
{
#if __JHEXEN__
    int frame = (mnTime / 5) % 7;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnAlpha);

    GL_DrawPatch(m_htic.id, 88, 0);
    GL_DrawPatch(dpBullWithFire[(frame + 2) % 7].id, 37, 80);
    GL_DrawPatch(dpBullWithFire[frame].id, 278, 80);

    DGL_Disable(DGL_TEXTURE_2D);

#elif __JHERETIC__
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(m_htic.id, 88, 0, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, mnAlpha);
    DGL_Color4f(1, 1, 1, mnAlpha);
    GL_DrawPatch(dpRotatingSkull[17 - frame].id, 40, 10);
    GL_DrawPatch(dpRotatingSkull[frame].id, 232, 10);

    DGL_Disable(DGL_TEXTURE_2D);
#elif __JDOOM__ || __JDOOM64__
    DGL_Enable(DGL_TEXTURE_2D);
    WI_DrawPatch4(m_doom.id, 94, 2, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, mnAlpha);
    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void M_DrawNewGameMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);

    M_DrawMenuText3(GET_TXT(TXT_PICKGAMETYPE), SCREENWIDTH/2, y-30, GF_FONTB, DTF_ALIGN_TOP);

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
#define BG_X            (174)
#define BG_Y            (8)

    float w, h, s, t;
    int pClass;
    spriteinfo_t sprInfo;
    int tmap = 1;

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText2("Choose class:", 34, 24, GF_FONTB);

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

    DGL_SetTranslatedSprite(sprInfo.material, 1, tmap);
    
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
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("WHICH EPISODE?", SCREENWIDTH/2, y-4, GF_FONTB, DTF_ALIGN_TOP);

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
    WI_DrawPatch4(m_episod.id, 50, 40, "{case}Which Episode{scaley=1.25,y=-3}?", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void M_DrawSkillMenu(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JDOOM__ || __JDOOM64__
    WI_DrawPatch4(m_newg.id, 96, 14, "{case}NEW GAME", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    WI_DrawPatch4(m_skill.id, 54, 38, "{case}Choose Skill Level:", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#else
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("CHOOSE SKILL LEVEL:", SCREENWIDTH/2, y-8, GF_FONTB, DTF_ALIGN_TOP);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawFilesMenu(const mn_page_t* page, int x, int y)
{
    // Clear out the quicksave/quickload stuff.
    quicksave = 0;
    quickload = 0;
}

static void updateSaveList(void)
{
    filename_t fileName;
    int i;

    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        mn_object_t* loadSlot = &LoadItems[i];

        SV_GetSaveGameFileName(fileName, i, FILENAME_T_MAXLEN);

        memset(saveGameDescriptions[i].text, 0, MNDATA_EDIT_TEXT_MAX_LENGTH+1);
        if(SV_GetSaveDescription(saveGameDescriptions[i].text, fileName, MNDATA_EDIT_TEXT_MAX_LENGTH+1))
        {
            loadSlot->flags &= ~MNF_DISABLED;
        }
        else
        {
            loadSlot->flags |= MNF_DISABLED;
        }
    }
}

/**
 * Called after an save game description has been modified to action the save.
 */
void M_DoSaveGame(const mndata_edit_t* ef)
{
    // Picked a quicksave slot yet?
    if(quickSaveSlot == -2)
        quickSaveSlot = ef->data;
    SaveMenu.focus = ef->data+1;
    LoadMenu.focus = ef->data;

    S_LocalSound(SFX_MENU_ACCEPT, NULL);
    G_SaveGame(ef->data, ef->text);
    Hu_MenuCommand(MCMD_CLOSEFAST);
}

void M_SetEditFieldText(mndata_edit_t* ef, const char* string)
{
    dd_snprintf(ef->text, MNDATA_EDIT_TEXT_MAX_LENGTH, "%s", string);
}

void M_ActivateEditField(mn_object_t* obj, int option)
{
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));
    obj->flags &= ~MNF_INACTIVE;
}

void MNText_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
    uint colorIndex = obj->data2 % NUM_MENU_COLORS;
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
        WI_DrawPatch4(*obj->patch, 0, 0, (obj->flags & MNF_NO_ALTTEXT)? NULL : obj->text, obj->font, true, DPF_ALIGN_TOPLEFT, color[CR], color[CG], color[CB], color[CA]);
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(color);

    M_DrawMenuText2(obj->text, 0, 0, obj->font);

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
    GL_TextFragmentDimensions(width, height, obj->text, obj->font);
}

void MNEdit_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
#if __JDOOM__ || __JDOOM64__
# define COLOR_IDX              (0)
# define OFFSET_Y               (4)
#else
# define COLOR_IDX              (2)
# define OFFSET_Y               (5)
#endif

    const mndata_edit_t* edit = (const mndata_edit_t*) obj->data;
    boolean isActive = obj == focusObject() && !(obj->flags & MNF_INACTIVE);
    char buf[MNDATA_EDIT_TEXT_MAX_LENGTH+1];
    const char* string;
    float color[4], light = 1;

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

    M_DrawSaveLoadBorder(x - 10, y, (edit->maxVisibleChars > 0? MIN_OF(edit->maxVisibleChars, MNDATA_EDIT_TEXT_MAX_LENGTH) : MNDATA_EDIT_TEXT_MAX_LENGTH) * GL_CharWidth('_', obj->font) + 20);

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
    if(string)
        M_DrawMenuText3(string, x, y, obj->font, DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);

#undef OFFSET_Y
#undef COLOR_IDX
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
    float color[4];
    int i;

    DGL_Enable(DGL_TEXTURE_2D);

    color[CR] = cfg.menuColors[2][CR];
    color[CG] = cfg.menuColors[2][CG];
    color[CB] = cfg.menuColors[2][CB];
    color[CA] = alpha;
    DGL_Color4fv(color);
    for(i = 0; i < list->count; ++i)
    {
        const mndata_listitem_t* item = &((const mndata_listitem_t*) list->items)[i];
        M_DrawMenuText2(item->text, x, y, GF_FONTA);
        y += GL_TextFragmentHeight(item->text, GF_FONTA) * (1+MNDATA_LIST_LEADING);
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

void MNList_InlineDrawer(const mn_object_t* obj, int x, int y, float alpha)
{
    const mndata_list_t* list = (const mndata_list_t*) obj->data;
    const mndata_listitem_t* item = ((const mndata_listitem_t*) list->items) + list->selection;
    float color[4];

    DGL_Enable(DGL_TEXTURE_2D);

    color[CR] = cfg.menuColors[2][CR];
    color[CG] = cfg.menuColors[2][CG];
    color[CB] = cfg.menuColors[2][CB];
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
    for(i = 0; i < list->count; ++i)
    {
        const mndata_listitem_t* item = &((const mndata_listitem_t*) list->items)[i];
        int w;
        if(width && (w = GL_TextFragmentWidth(item->text, GF_FONTA)) > *width)
            *width = w;
        if(height)
        {
            int h = GL_TextFragmentHeight(item->text, GF_FONTA);
            *height += h;
            if(i != list->count-1)
                *height += h * MNDATA_LIST_LEADING;
        }
    }
}

void MNList_InlineDimensions(const mn_object_t* obj, int* width, int* height)
{
    const mndata_list_t* list = (const mndata_list_t*) obj->data;
    const mndata_listitem_t* item = ((const mndata_listitem_t*) list->items) + list->selection;
    if(width)
        *width = GL_TextFragmentWidth(item->text, GF_FONTA);
    if(height)
        *height = GL_TextFragmentHeight(item->text, GF_FONTA);
}

void MNButton_Drawer(const mn_object_t* obj, int x, int y, float alpha)
{
    int dis = (obj->flags & MNF_DISABLED) != 0;
    int act = (obj->flags & MNF_INACTIVE) == 0;
    //int click = (obj->flags & MNF_CLICKED) != 0;
    boolean down = act /*|| click*/;
    uint colorIndex = (obj->type == MN_BUTTON? obj->data2 : 2) % NUM_MENU_COLORS;
    const char* text;
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
        WI_DrawPatch4(*obj->patch, 0, 0, (obj->flags & MNF_NO_ALTTEXT)? NULL : text, obj->font, true, DPF_ALIGN_TOPLEFT, color[CR], color[CG], color[CB], color[CA]);
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(color);

    M_DrawMenuText2(text, x, y, obj->font);

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
    GL_TextFragmentDimensions(width, height, text, obj->font);
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
#if __JHERETIC__ || __JHEXEN__
# define OFFSET_X               (24)
# define OFFSET_Y               (2)
#else   
# define OFFSET_X               (0)
# define OFFSET_Y               (0)
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

void MNSlider_Dimensions(const mn_object_t* obj, int* width, int* height)
{
    patchinfo_t middleInfo;
    if(!R_GetPatchInfo(dpSliderMiddle, &middleInfo))
        return;
    if(width)
        *width = (int) ceil(middleInfo.width * MNDATA_SLIDER_SLOTS * MNDATA_SLIDER_SCALE);
    if(height)
        *height = (int) ceil(middleInfo.height * MNDATA_SLIDER_SCALE);
}

void Hu_MenuCvarButton(mn_object_t* obj, int option)
{
    cvarbutton_t* cb = obj->data;
    cvar_t* var = Con_GetVariable(cb->cvarname);
    int value;

    //strcpy(obj->text, cb->active? cb->yes : cb->no);
    obj->text = cb->active? cb->yes : cb->no;

    if(!var)
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

    Con_SetInteger(cb->cvarname, value, true);
}

void Hu_MenuCvarList(mn_object_t* obj, int option)
{
    mndata_list_t* list = obj->data;
    cvar_t* var = Con_GetVariable(list->data);
    int value = ((mndata_listitem_t*) list->items)[list->selection].data;

    if(list->selection < 0)
        return; // Hmm?
    if(!var)
        return;
    Con_SetInteger(var->name, value, true);
}

void Hu_MenuCvarSlider(mn_object_t* obj, int option)
{
    mndata_slider_t* slider = obj->data;
    cvar_t* var = Con_GetVariable(slider->data);
    float value = slider->value;

    if(!var)
        return;

    if(!slider->floatMode)
    {
        value += (slider->value < 0? -.5f : .5f);
    }

    if(var->type == CVT_FLOAT)
    {
        if(slider->step >= .01f)
        {
            Con_SetFloat(var->name, (int) (100 * value) / 100.0f, true);
        }
        else
        {
            Con_SetFloat(var->name, value, true);
        }
    }
    else if(var->type == CVT_INT)
        Con_SetInteger(var->name, (int) value, true);
    else if(var->type == CVT_BYTE)
        Con_SetInteger(var->name, (byte) value, true);
}

void M_DrawLoad(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("Load Game", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);
#else
    WI_DrawPatch4(m_loadg.id, SCREENWIDTH/2, 24, "{case}Load game", GF_FONTB, true, DPF_ALIGN_TOP, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
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
        G_SaveGame(quickSaveSlot, saveGameDescriptions[quickSaveSlot].text);
    return true;
}

/**
 * Called via the bindings mechanism when a player wishes to save their
 * game to a preselected save slot.
 */
static void M_QuickSave(void)
{
    player_t* player = &players[CONSOLEPLAYER];
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

    if(quickSaveSlot < 0)
    {
        Hu_MenuCommand(MCMD_OPEN);
        updateSaveList();
        MN_GotoPage(&SaveMenu);
        quickSaveSlot = -2; // Means to pick a slot now.
        return;
    }
    sprintf(buf, QSPROMPT, saveGameDescriptions[quickSaveSlot].text);

    if(!cfg.askQuickSaveLoad)
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        G_SaveGame(quickSaveSlot, saveGameDescriptions[quickSaveSlot].text);
        return;
    }

    S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
    Hu_MsgStart(MSG_YESNO, buf, M_QuickSaveResponse, NULL);
}

int M_QuickLoadResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        filename_t name;

        SV_GetSaveGameFileName(name, quickSaveSlot, FILENAME_T_MAXLEN);
        G_LoadGame(name);
#else
        G_LoadGame(quickSaveSlot);
#endif
    }

    return true;
}

static void M_QuickLoad(void)
{
    char buf[80];

    if(IS_NETGAME)
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QLOADNET, NULL, NULL);
        return;
    }

    if(quickSaveSlot < 0)
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QSAVESPOT, NULL, NULL);
        return;
    }

    sprintf(buf, QLPROMPT, saveGameDescriptions[quickSaveSlot].text);

    if(!cfg.askQuickSaveLoad)
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        filename_t name;

        SV_GetSaveGameFileName(name, quickSaveSlot, FILENAME_T_MAXLEN);
        G_LoadGame(name);
#else
        G_LoadGame(quickSaveSlot);
#endif
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        return;
    }

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
    M_DrawMenuText3("OPTIONS", SCREENWIDTH/2, y-32, GF_FONTB, DTF_ALIGN_TOP);
#else
# if __JDOOM64__
    WI_DrawPatch4(0, 160, y - 20, "{case}OPTIONS", GF_FONTB, true, DPF_ALIGN_TOP, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
#else
    WI_DrawPatch4(m_optttl.id, 160, y - 20, "{case}OPTIONS", GF_FONTB, true, DPF_ALIGN_TOP, cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
# endif
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawOptions2(const mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("SOUND OPTIONS", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);
#if __JDOOM__ || __JDOOM64__
    //MN_DrawSlider(page, 0, x, y, 16, SFXVOLUME);
    //MN_DrawSlider(page, 1, x, y, 16, MUSICVOLUME);
#else
    //MN_DrawSlider(page, 1, x, y, 16, SFXVOLUME);
    //MN_DrawSlider(page, 4, x, y, 16, MUSICVOLUME);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawGameplay(const mn_page_t* page, int x, int y)
{
    int idx = 0;

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("GAMEPLAY", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);

#if __JHEXEN__
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.alwaysRun != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.lookSpring != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[!cfg.noAutoAim]);
#else
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.alwaysRun != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.lookSpring != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[!cfg.noAutoAim]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.jumpEnabled != 0]);
# if __JDOOM64__
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.weaponRecoil != 0]);
    idx = 7;
# else
    idx = 6;
# endif
# if __JDOOM__ || __JDOOM64__
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.anyBossDeath != 0]);
#   if !__JDOOM64__
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.raiseGhosts != 0]);
#   endif
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.maxSkulls != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.allowSkullsInWalls != 0]);
# endif
# if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.monstersStuckInDoors != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.avoidDropoffs != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.fallOff != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.slidingCorpses != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.moveBlock != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.wallRunNorthOnly != 0]);
# endif
# if __JDOOM__ || __JDOOM64__
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.zombiesCanExit != 0]);
# endif
# if __JDOOM__
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.fixOuchFace != 0]);
    //M_WriteMenuText(page, idx++, x, y, yesno[cfg.fixStatusbarOwnedWeapons != 0]);
# endif
#endif

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

#if __JDOOM__ || __JDOOM64__
    Hu_MenuPageString(buf, page);
    DGL_Color4f(cfg.menuColors[1][CR], cfg.menuColors[1][CG], cfg.menuColors[1][CB], Hu_MenuAlpha());
    M_DrawMenuText3(buf, SCREENWIDTH/2, y - 12, GF_FONTA, DTF_ALIGN_TOP);
#elif __JHERETIC__
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, Hu_MenuAlpha());
    GL_DrawPatch(dpInvPageLeft[!page->firstObject || (mnTime & 8)], x, y - 22);
    GL_DrawPatch(dpInvPageRight[page->firstObject + page->numVisObjects >= page->_size || (mnTime & 8)], 312 - x, y - 22);
#endif

    /**
     * \kludge Inform the user how to change the order.
     */
    if(mnFocusObjectIndex - 1 == 0)
    {
        const char* str = "Use left/right to move weapon up/down";
        DGL_Color4f(cfg.menuColors[1][CR], cfg.menuColors[1][CG], cfg.menuColors[1][CB], mnAlpha);
        M_DrawMenuText3(str, SCREENWIDTH/2, SCREENHEIGHT/2 + (95/cfg.menuScale), GF_FONTA, DTF_ALIGN_BOTTOM);
    }

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

    // Auto-hide option:
    {
    char secString[11];
    const char* str;
    uint seconds = MINMAX_OF(0, cfg.inventoryTimer, 30);
    if(seconds > 0)
    {
        memset(secString, 0, sizeof(secString));
        dd_snprintf(secString, 11, "%2u seconds", seconds);
        str = secString;
    }
    else
        str = "Disabled";
    //M_WriteMenuText(page, idx++, x, y, str);
    }

    { char buff[3];
    const char* str;
    uint val = MINMAX_OF(0, cfg.inventorySlotMaxVis, 16);

    if(val > 0)
    {
        memset(buff, 0, sizeof(buff));
        dd_snprintf(buff, 3, "%2u", val);
        str = buff;
    }
    else
        str = "Automatic";
    //M_WriteMenuText(page, idx++, x, y, str);
    }

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

/**
 * @todo This could use a cleanup.
 */
void M_DrawHUDMenu(const mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    static const char* countnames[4] = { "HIDDEN", "COUNT", "PERCENT", "COUNT+PCNT" };
#endif
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuColors[0][CR], cfg.menuColors[0][CG], cfg.menuColors[0][CB], mnAlpha);
    M_DrawMenuText3("HUD options", SCREENWIDTH/2, y-28, GF_FONTB, DTF_ALIGN_TOP);

#if __JDOOM__ || __JDOOM64__
    Hu_MenuPageString(buf, page);
    DGL_Color4f(1, .7f, .3f, Hu_MenuAlpha());
    M_DrawMenuText3(buf, SCREENWIDTH/2, y - 12, GF_FONTA, DTF_ALIGN_TOP);
#else
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, Hu_MenuAlpha());
    GL_DrawPatch(dpInvPageLeft[!page->firstObject || (mnTime & 8)], x, y - 22);
    GL_DrawPatch(dpInvPageRight[page->firstObject + page->numVisObjects >= page->_size || (mnTime & 8)], 312 - x, y - 22);
#endif

    // Auto-hide HUD options:
    {
    char secString[11];
    const char* str;
    uint seconds = MINMAX_OF(0, cfg.hudTimer, 30);
    if(seconds > 0)
    {
        memset(secString, 0, sizeof(secString));
        dd_snprintf(secString, 11, "%2u %s", seconds, seconds > 1? "seconds" : "second");
        str = secString;
    }
    else
        str = "Disabled";
    //M_WriteMenuText(page, 0, x, y, str);
    }

    {
    char secString[11];
    const char* str;
    uint seconds = MINMAX_OF(1, cfg.msgUptime, 30);

    memset(secString, 0, sizeof(secString));
    dd_snprintf(secString, 11, "%2u %s", seconds, seconds > 1? "seconds" : "second");
    str = secString;
    //M_WriteMenuText(page, 0, x, y, str);
    }

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Counters:
    //M_WriteMenuText(page, 0, x, y, countnames[(cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2)]);
    //M_WriteMenuText(page, 0, x, y, countnames[((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3)]);
    //M_WriteMenuText(page, 0, x, y, countnames[((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4)]);
#endif

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

/**
 * Set the show kills counter
 */
void M_KillCounter(mn_object_t* obj, int option)
{
    int op = (cfg.counterCheat & 0x1) | ((cfg.counterCheat & 0x8) >> 2);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x9;
    cfg.counterCheat |= (op & 0x1) | ((op & 0x2) << 2);
}

/**
 * Set the show objects counter
 */
void M_ItemCounter(mn_object_t* obj, int option)
{
    int op = ((cfg.counterCheat & 0x2) >> 1) | ((cfg.counterCheat & 0x10) >> 3);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x12;
    cfg.counterCheat |= ((op & 0x1) << 1) | ((op & 0x2) << 3);
}

/**
 * Set the show secrets counter
 */
void M_SecretCounter(mn_object_t* obj, int option)
{
    int op = ((cfg.counterCheat & 0x4) >> 2) | ((cfg.counterCheat & 0x20) >> 4);

    op += option == RIGHT_DIR ? 1 : -1;
    if(op < 0)
        op = 0;
    if(op > 3)
        op = 3;
    cfg.counterCheat &= ~0x24;
    cfg.counterCheat |= ((op & 0x1) << 2) | ((op & 0x2) << 4);
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

void M_HUDHideTime(mn_object_t* obj, int option)
{
    int val = cfg.hudTimer;

    if(option == RIGHT_DIR)
    {
        if(val < 30)
            val++;
    }
    else if(val > 0)
        val--;

    cfg.hudTimer = val;
}

void M_MessageUptime(mn_object_t* obj, int option)
{
    int                 val = cfg.msgUptime;

    if(option == RIGHT_DIR)
    {
        if(val < 30)
            val++;
    }
    else if(val > 1)
        val--;

    cfg.msgUptime = val;
}

#if __JHERETIC__ || __JHEXEN__
void M_InventoryHideTime(mn_object_t* obj, int option)
{
    int val = cfg.inventoryTimer;

    if(option == RIGHT_DIR)
    {
        if(val < 30)
            val++;
    }
    else if(val > 0)
        val--;

    cfg.inventoryTimer = val;
}

void M_InventorySlotMaxVis(mn_object_t* obj, int option)
{
    int val = cfg.inventorySlotMaxVis;
    char* cvarname;

    if(option == RIGHT_DIR)
    {
        if(val < 16)
            val++;
    }
    else if(val > 0)
        val--;

    if(!obj->data)
        return;
    cvarname = (char*) obj->data;

    Con_SetInteger(cvarname, val, false);
}
#endif

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
    if(IS_CLIENT && !Get(DD_PLAYBACK))
    {
        Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, NULL);
        return;
    }

    updateSaveList();
    MN_GotoPage(&LoadMenu);
}

void M_SaveGame(mn_object_t* obj, int option)
{
    player_t* player = &players[CONSOLEPLAYER];

    if(Get(DD_PLAYBACK))
        return;

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

    if(IS_CLIENT)
    {
#if __JDOOM__ || __JDOOM64__
        Hu_MsgStart(MSG_ANYKEY, SAVENET, NULL, NULL);
#endif
        return;
    }

    Hu_MenuCommand(MCMD_OPEN);
    updateSaveList();
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
DEFCC(CCmdMenuAction)
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
                {
                mndata_edit_t* edit = (mndata_edit_t*)focusObj->data;
                if(edit->onChange)
                {
                    edit->onChange(edit);
                }
                else
                {
                    S_LocalSound(SFX_MENU_ACCEPT, NULL);
                }
                focusObj->flags |= MNF_INACTIVE;
                break;
                }
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

    // Menu-related hotkey shortcuts.
    if(!stricmp(argv[0], "SaveGame"))
    {
        M_SaveGame(0, 0);
    }
    else if(!stricmp(argv[0], "LoadGame"))
    {
        Hu_MenuCommand(MCMD_OPEN);
        M_LoadGame(0, 0);
    }
    else if(!stricmp(argv[0], "SoundMenu"))
    {
        Hu_MenuCommand(MCMD_OPEN);
        MN_GotoPage(&SoundMenu);
    }
    else if(!stricmp(argv[0], "QuickSave"))
    {
        M_QuickSave();
    }
    else if(!stricmp(argv[0], "EndGame"))
    {
        M_EndGame(0, 0);
    }
    else if(!stricmp(argv[0], "ToggleMsgs"))
    {
        M_ChangeMessages(0, 0);
    }
    else if(!stricmp(argv[0], "QuickLoad"))
    {
        M_QuickLoad();
    }
    else if(!stricmp(argv[0], "quit"))
    {
        if(IS_DEDICATED)
            DD_Execute(true, "quit!");
        else
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            M_QuitDOOM(0, 0);
        }
    }

    return true;
}

DEFCC(CCmdShortcut)
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
    return false;
}

/**
 * @todo Remove this placeholder.
 */
void MN_DrawSlider(const mn_page_t* page, int index, int x, int y, int range, int pos)
{
    // Stub.
}
