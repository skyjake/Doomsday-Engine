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
#include "hu_chat.h"
#include "hu_log.h"
#include "hu_msg.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "x_hair.h"
#include "p_player.h"
#include "g_controls.h"
#include "p_saveg.h"
#include "g_common.h"
#include "r_common.h"
#include "m_ctrl.h"

#include "hu_menu.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct cvarbutton_s {
    char            active;
    const char*     cvarname;
    const char*     yes;
    const char*     no;
    int             mask;
} cvarbutton_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int Hu_MenuActionSetActivePage(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuActionInitNewGame(mn_object_t* obj, mn_actionid_t action, void* paramaters);

int Hu_MenuSelectLoadGame(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectSaveGame(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#if __JHEXEN__
int Hu_MenuSelectFiles(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#endif
int Hu_MenuSelectPlayerSetup(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectJoinGame(mn_object_t* obj, mn_actionid_t action, void* paramaters);

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
int Hu_MenuSelectHelp(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#endif
int Hu_MenuSelectControlPanelLink(mn_object_t* obj, mn_actionid_t action, void* paramaters);

int Hu_MenuSelectSingleplayer(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectMultiplayer(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#if __JDOOM__ || __JHERETIC__
int Hu_MenuFocusEpisode(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuActivateNotSharewareEpisode(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#endif
#if __JHEXEN__
int Hu_MenuFocusOnPlayerClass(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectPlayerClass(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#endif
int Hu_MenuFocusSkillMode(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectLoadSlot(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectQuitGame(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectEndGame(mn_object_t* obj, mn_actionid_t action, void* paramaters);
int Hu_MenuSelectAcceptPlayerSetup(mn_object_t* obj, mn_actionid_t action, void* paramaters);

int Hu_MenuSelectSaveSlot(mn_object_t* obj, mn_actionid_t action, void* paramaters);

int Hu_MenuChangeWeaponPriority(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#if __JHEXEN__
int Hu_MenuSelectPlayerSetupPlayerClass(mn_object_t* obj, mn_actionid_t action, void* paramaters);
#endif
int Hu_MenuSelectPlayerColor(mn_object_t* obj, mn_actionid_t action, void* paramaters);

void Hu_MenuDrawMainPage(mn_page_t* page, int x, int y);
void Hu_MenuDrawGameTypePage(mn_page_t* page, int x, int y);
void Hu_MenuDrawSkillPage(mn_page_t* page, int x, int y);
#if __JHEXEN__
void Hu_MenuDrawPlayerClassPage(mn_page_t* page, int x, int y);
#endif
#if __JDOOM__ || __JHERETIC__
void Hu_MenuDrawEpisodePage(mn_page_t* page, int x, int y);
#endif
void Hu_MenuDrawOptionsPage(mn_page_t* page, int x, int y);
void Hu_MenuDrawSoundPage(mn_page_t* page, int x, int y);
void Hu_MenuDrawGameplayPage(mn_page_t* page, int x, int y);
void Hu_MenuDrawHUDPage(mn_page_t* page, int x, int y);
#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawInventoryPage(mn_page_t* page, int x, int y);
#endif
void Hu_MenuDrawWeaponsPage(mn_page_t* page, int x, int y);
void Hu_MenuDrawLoadGamePage(mn_page_t* page, int x, int y);
void Hu_MenuDrawSaveGamePage(mn_page_t* page, int x, int y);
void Hu_MenuDrawMultiplayerPage(mn_page_t* page, int x, int y);
void Hu_MenuDrawPlayerSetupPage(mn_page_t* page, int x, int y);
void Hu_MenuDrawAutomapPage(mn_page_t* page, int x, int y);

int Hu_MenuColorWidgetCmdResponder(mn_page_t* page, menucommand_e cmd);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void initAllObjectsOnAllPages(void);

static void Hu_MenuUpdateCursorState(void);

static boolean Hu_MenuHasCursorRotation(mn_object_t* obj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

cvarbutton_t mnCVarButtons[] = {
    { 0, "ctl-aim-noauto" },
#if __JHERETIC__ || __JHEXEN__
    { 0, "ctl-inventory-mode", "Scroll", "Cursor" },
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

int menuTime = 0;
int menuFlashCounter = 0;
boolean menuNominatingQuickSaveSlot = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mn_page_t* menuActivePage = NULL;
static boolean menuActive = false;

static float mnAlpha = 0; // Alpha level for the entire menu.
static float mnTargetAlpha = 0; // Target alpha for the entire UI.

static skillmode_t mnSkillmode = SM_MEDIUM;
static int mnEpisode = 0;
#if __JHEXEN__
static int mnPlrClass = PCLASS_FIGHTER;
#endif

static int frame = 0; // Used by any graphic animations that need to be pumped.

static boolean colorWidgetActive = false;

// Present cursor state.
static boolean cursorHasRotation = false;
static float cursorAngle = 0;
static int cursorAnimCounter = 0;
static int cursorAnimFrame = 0;

#if __JHERETIC__
static char notDesignedForMessage[80];
#endif

static patchid_t pMainTitle;
#if __JDOOM__ || __JDOOM64__
static patchid_t pNewGame;
static patchid_t pSkill;
static patchid_t pEpisode;
static patchid_t pNGame;
static patchid_t pOptions;
static patchid_t pLoadGame;
static patchid_t pSaveGame;
static patchid_t pReadThis;
static patchid_t pQuitGame;
static patchid_t pOptionsTitle;

static patchid_t pSkillModeNames[NUM_SKILL_MODES];
#endif
#if __JDOOM__
static patchid_t pEpisodeNames[4];
#endif

#if __JHEXEN__
static patchid_t pPlayerClassBG[3];
static patchid_t pBullWithFire[8];
#endif

#if __JHERETIC__
static patchid_t pRotatingSkull[18];
#endif

static patchid_t pCursors[MENU_CURSOR_FRAMECOUNT];

#if __JDOOM__
mndata_button_t btn_main_new_game  = { false, NULL, "{case}New Game", &pNGame };
mndata_button_t btn_main_options   = { false, NULL, "{case}Options", &pOptions };
mndata_button_t btn_main_load_game = { false, NULL, "{case}Load game", &pLoadGame };
mndata_button_t btn_main_save_game = { false, NULL, "{case}Save game", &pSaveGame };
mndata_button_t btn_main_help      = { false, NULL, "{case}Read This!", &pReadThis };
mndata_button_t btn_main_quit_game = { false, NULL, "{case}Quit Game", &pQuitGame };
#elif __JDOOM64__
mndata_button_t btn_main_new_game  = { false, NULL, "{case}New Game" };
mndata_button_t btn_main_options   = { false, NULL, "{case}Options" };
mndata_button_t btn_main_load_game = { false, NULL, "{case}Load Game" };
mndata_button_t btn_main_save_game = { false, NULL, "{case}Save Game" };
mndata_button_t btn_main_quit_game = { false, NULL, "{case}Quit Game" };
#else
mndata_button_t btn_main_new_game   = { false, NULL, "New Game" };
mndata_button_t btn_main_options    = { false, NULL, "Options" };
mndata_button_t btn_main_game_files = { false, NULL, "Game Files" };
mndata_button_t btn_main_help       = { false, NULL, "Info" };
mndata_button_t btn_main_quit_game  = { false, NULL, "Quit Game" };
#endif

mn_object_t MainMenuObjects[] = {
#if __JDOOM__
    { MN_BUTTON,    0,  0,  'n', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_new_game, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  'o', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_options, &OptionsMenu },
    { MN_BUTTON,    0,  0,  'l', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectLoadGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_load_game },
    { MN_BUTTON,    0,  0,  's', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectSaveGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_save_game },
    { MN_BUTTON,    0,  MNF_ID0, 'r', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectHelp, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_help },
    { MN_BUTTON,    0,  0,  'q', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectQuitGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_quit_game },
#elif __JDOOM64__
    { MN_BUTTON,    0,  0,  'n', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_new_game, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  'o', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_options, &OptionsMenu },
    { MN_BUTTON,    0,  0,  'l', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectLoadGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_load_game },
    { MN_BUTTON,    0,  0,  's', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectSaveGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_save_game },
    { MN_BUTTON,    0,  0,  'q', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectQuitGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_quit_game },
#else
    { MN_BUTTON,    0,  0,  'n', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_new_game, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  'o', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_options, &OptionsMenu },
    { MN_BUTTON,    0,  0,  'f', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_game_files, &FilesMenu },
    { MN_BUTTON,    0,  0,  'i', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectHelp, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_help },
    { MN_BUTTON,    0,  0,  'q', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectQuitGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_main_quit_game },
#endif
    { MN_NONE }
};

mn_page_t MainMenu = {
    MainMenuObjects,
#if __JHEXEN__ || __JHERETIC__
    { 110, 56 },
#else
    {  97, 64 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawMainPage,
#if defined(__JDOOM__) && !defined(__JDOOM64__)
    //0, 0,
    //0, 6
#else
    //0, 0,
    //0, 5
#endif
};

mndata_button_t btn_gametype_singleplayer = { false, NULL, (const char*)TXT_SINGLEPLAYER };
mndata_button_t btn_gametype_multiplayer = { false, NULL, (const char*)TXT_MULTIPLAYER };

mn_object_t GameTypeMenuObjects[] = {
    { MN_BUTTON,    0,  0,  's', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectSingleplayer, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gametype_singleplayer },
    { MN_BUTTON,    0,  0,  'm', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectMultiplayer, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gametype_multiplayer },
    { MN_NONE }
};

mn_page_t GameTypeMenu = {
    GameTypeMenuObjects,
#if __JDOOM__ || __JDOOM64__
    {  97, 65 },
#else
    { 104, 65 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawGameTypePage, NULL,
    &MainMenu,
    //0, 2
};

#if __JHEXEN__
mndata_mobjpreview_t mop_playerclass_preview;

mn_object_t* PlayerClassMenuObjects;
mndata_button_t* PlayerClassMenuButtons;

mn_page_t PlayerClassMenu = {
    0, { 66, 66 }, { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawPlayerClassPage, NULL,
    &GameTypeMenu,
    //0, 0
};
#endif

#if __JDOOM__ || __JHERETIC__
mn_object_t* EpisodeMenuObjects;
mndata_button_t* EpisodeMenuButtons;

mn_page_t EpisodeMenu = {
    0,
# if __JDOOM__
    { 48, 63 },
# else
    { 80, 50 },
# endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawEpisodePage, NULL,
    &GameTypeMenu,
    //0, 0
};
#endif

#if __JDOOM__ || __JDOOM64__
mndata_button_t btn_skill_baby      = { false, NULL, "", &pSkillModeNames[0] };
mndata_button_t btn_skill_easy      = { false, NULL, "", &pSkillModeNames[1] };
mndata_button_t btn_skill_medium    = { false, NULL, "", &pSkillModeNames[2] };
mndata_button_t btn_skill_hard      = { false, NULL, "", &pSkillModeNames[3] };
#  if !__JDOOM64__
mndata_button_t btn_skill_nightmare = { false, NULL, "", &pSkillModeNames[4] };
#  endif
#else
mndata_button_t btn_skill_baby;
mndata_button_t btn_skill_easy;
mndata_button_t btn_skill_medium;
mndata_button_t btn_skill_hard;
mndata_button_t btn_skill_nightmare;
#endif

#if __JHEXEN__
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  MNF_ID0,                0, MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_baby, NULL, SM_BABY },
    { MN_BUTTON,    0,  MNF_ID1,                0, MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_easy, NULL, SM_EASY },
    { MN_BUTTON,    0,  MNF_ID2|MNF_DEFAULT,    0, MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_medium, NULL, SM_MEDIUM },
    { MN_BUTTON,    0,  MNF_ID3,                0, MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_hard, NULL, SM_HARD },
    { MN_BUTTON,    0,  MNF_ID4,                0, MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_nightmare, NULL, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillMenu = {
    SkillMenuObjects,
    { 120, 44 }, { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawSkillPage, NULL,
    &PlayerClassMenu,
    //0, 5
};
#elif __JHERETIC__
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  MNF_ID0,                'w', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_baby, NULL, SM_BABY },
    { MN_BUTTON,    0,  MNF_ID1,                'y', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_easy, NULL, SM_EASY },
    { MN_BUTTON,    0,  MNF_ID2|MNF_DEFAULT,    'b', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_medium, NULL, SM_MEDIUM },
    { MN_BUTTON,    0,  MNF_ID3,                's', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_hard, NULL, SM_HARD },
    { MN_BUTTON,    0,  MNF_ID4,                'p', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_nightmare, NULL, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillMenu = {
    SkillMenuObjects,
    { 38, 30 }, { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawSkillPage, NULL,
    &EpisodeMenu,
    //0, 5
};
#elif __JDOOM64__
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  MNF_ID0,                'g', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_baby, NULL, SM_BABY },
    { MN_BUTTON,    0,  MNF_ID1,                'b', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_easy, NULL, SM_EASY },
    { MN_BUTTON,    0,  MNF_ID2|MNF_DEFAULT,    'o', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_medium, NULL, SM_MEDIUM },
    { MN_BUTTON,    0,  MNF_ID3,                'w', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_hard, NULL, SM_HARD },
    { MN_NONE }
};
static mn_page_t SkillMenu = {
    SkillMenuObjects,
    { 48, 63 }, { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawSkillPage, NULL,
    &GameTypeMenu,
    //0, 4
};
#else
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  MNF_ID0,                'y', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_baby, NULL, SM_BABY },
    { MN_BUTTON,    0,  MNF_ID1,                'r', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_easy, NULL, SM_EASY },
    { MN_BUTTON,    0,  MNF_ID2|MNF_DEFAULT,    'h', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_medium, NULL, SM_MEDIUM },
    { MN_BUTTON,    0,  MNF_ID3,                'u', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_hard, NULL, SM_HARD },
    { MN_BUTTON,    0,  MNF_ID4|MNF_NO_ALTTEXT, 'n', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionInitNewGame, NULL, NULL, NULL, Hu_MenuFocusSkillMode }, MNButton_CommandResponder, NULL, NULL, &btn_skill_nightmare, NULL, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillMenu = {
    SkillMenuObjects,
    { 48, 63 }, { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawSkillPage, NULL,
    &EpisodeMenu,
    //0, 5
};
#endif

#if __JHERETIC__ || __JHEXEN__
mndata_button_t btn_files_load_game = { false, NULL, "Load Game" };
mndata_button_t btn_files_save_game = { false, NULL, "Save Game" };

static mn_object_t FilesMenuObjects[] = {
    { MN_BUTTON,    0,  0,  'l', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectLoadGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_files_load_game },
    { MN_BUTTON,    0,  0,  's', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectSaveGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_files_save_game },
    { MN_NONE }
};

mn_page_t FilesMenu = {
    FilesMenuObjects,
    { 110, 60 }, { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    NULL, NULL,
    &MainMenu,
    //0, 2
};
#endif

mndata_edit_t edit_saveslots[NUMSAVESLOTS] = {
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 0 },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 1 },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 2 },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 3 },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 4 },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 5 },
#if !__JHEXEN__
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 6 },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 7 }
#endif
};

static mn_object_t LoadMenuObjects[] = {
    { MN_EDIT,  0,  MNF_ID0|MNF_DISABLED,  '0', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[0], NULL, MNF_ID0 },
    { MN_EDIT,  0,  MNF_ID1|MNF_DISABLED,  '1', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[1], NULL, MNF_ID1 },
    { MN_EDIT,  0,  MNF_ID2|MNF_DISABLED,  '2', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[2], NULL, MNF_ID2 },
    { MN_EDIT,  0,  MNF_ID3|MNF_DISABLED,  '3', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[3], NULL, MNF_ID3 },
    { MN_EDIT,  0,  MNF_ID4|MNF_DISABLED,  '4', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[4], NULL, MNF_ID4 },
    { MN_EDIT,  0,  MNF_ID5|MNF_DISABLED,  '5', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[5], NULL, MNF_ID5 },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,  0,  MNF_ID6|MNF_DISABLED,  '6', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[6], NULL, MNF_ID6 },
    { MN_EDIT,  0,  MNF_ID7|MNF_DISABLED,  '7', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectLoadSlot, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNObject_DefaultCommandResponder, NULL, NULL, &edit_saveslots[7], NULL, MNF_ID7 },
#endif
    { MN_NONE }
};

mn_page_t LoadMenu = {
    LoadMenuObjects,
#if __JDOOM__ || __JDOOM64__
    { 80, 54 },
#else
    { 70, 30 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawLoadGamePage, NULL,
    &MainMenu,
    //0, NUMSAVESLOTS
};

static mn_object_t SaveMenuObjects[] = {
    { MN_EDIT,  0,  MNF_ID0,  '0', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[0], NULL, MNF_ID0 },
    { MN_EDIT,  0,  MNF_ID1,  '1', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[1], NULL, MNF_ID1 },
    { MN_EDIT,  0,  MNF_ID2,  '2', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[2], NULL, MNF_ID2 },
    { MN_EDIT,  0,  MNF_ID3,  '3', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[3], NULL, MNF_ID3 },
    { MN_EDIT,  0,  MNF_ID4,  '4', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[4], NULL, MNF_ID4 },
    { MN_EDIT,  0,  MNF_ID5,  '5', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[5], NULL, MNF_ID5 },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,  0,  MNF_ID6,  '6', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[6], NULL, MNF_ID6 },
    { MN_EDIT,  0,  MNF_ID7,  '7', MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, Hu_MenuSelectSaveSlot, Hu_MenuSaveSlotEdit, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[7], NULL, MNF_ID7 },
#endif
    { MN_NONE }
};

mn_page_t SaveMenu = {
    SaveMenuObjects,
#if __JDOOM__ || __JDOOM64__
    { 80, 54 },
#else
    { 64, 10 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawSaveGamePage, NULL,
    &MainMenu,
    //0, 1+NUMSAVESLOTS
};

mndata_button_t btn_options_end_game = { false, NULL, "End Game" };
mndata_button_t btn_options_control_panel = { false, NULL, "Control Panel" };
mndata_button_t btn_options_controls = { false, NULL, "Controls" };
mndata_button_t btn_options_gameplay = { false, NULL, "Gameplay" };
mndata_button_t btn_options_hud = { false, NULL, "HUD" };
mndata_button_t btn_options_automap = { false, NULL, "Automap" };
mndata_button_t btn_options_weapons = { false, NULL, "Weapons" };
#if __JHERETIC__ || __JHEXEN__
mndata_button_t btn_options_inventory = { false, NULL, "Inventory" };
#endif
mndata_button_t btn_options_sound = { false, NULL, "Sound" };
mndata_button_t btn_options_mouse = { false, NULL, "Mouse" };
mndata_button_t btn_options_joystick = { false, NULL, "Joystick" };

static mn_object_t OptionsMenuObjects[] = {
    { MN_BUTTON,    0,  0,  'e', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectEndGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_end_game },
    { MN_BUTTON,    0,  0,  'p', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectControlPanelLink, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_control_panel },
    { MN_BUTTON,    0,  0,  'c', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_controls, &ControlsMenu },
    { MN_BUTTON,    0,  0,  'g', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_gameplay, &GameplayMenu },
    { MN_BUTTON,    0,  0,  'h', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_hud, &HudMenu },
    { MN_BUTTON,    0,  0,  'a', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_automap, &AutomapMenu },
    { MN_BUTTON,    0,  0,  'w', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_weapons, &WeaponMenu },
#if __JHERETIC__ || __JHEXEN__
    { MN_BUTTON,    0,  0,  'i', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_inventory, &InventoryMenu },
#endif
    { MN_BUTTON,    0,  0,  's', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuActionSetActivePage, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_sound, &SoundMenu },
    { MN_BUTTON,    0,  0,  'm', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectControlPanelLink, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_mouse, NULL, 2 },
    { MN_BUTTON,    0,  0,  'j', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectControlPanelLink, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_options_joystick, NULL, 2 },
    { MN_NONE }
};

mn_page_t OptionsMenu = {
    OptionsMenuObjects,
#if __JHERETIC__ || __JHEXEN__
    { 110, 63 },
#else
    { 110, 63 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawOptionsPage, NULL,
    &MainMenu,
    //0,
#if __JHERETIC__ || __JHEXEN__
    //11
#else
    //10
#endif
};

mndata_slider_t sld_sound_sfx_volume = { 0, 255, 0, 5, false, "sound-volume" };
mndata_slider_t sld_sound_music_volume = { 0, 255, 0, 5, false, "music-volume" };

mndata_text_t txt_sound_sfx_volume = { "SFX Volume" };
mndata_text_t txt_sound_music_volume = { "Music Volume" };

mndata_button_t btn_sound_open_audio_panel = { false, NULL, "Open Audio Panel" };

mn_object_t SoundMenuObjects[] = {
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_sound_sfx_volume },
    { MN_SLIDER,    0,  0,  's', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_sound_sfx_volume },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_sound_music_volume },
    { MN_SLIDER,    0,  0,  'm', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_sound_music_volume },
    { MN_BUTTON,    0,  0,  'p', MENU_FONT1, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectControlPanelLink, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_sound_open_audio_panel, NULL, 1 },
    { MN_NONE }
};

mn_page_t SoundMenu = {
    SoundMenuObjects,
#if __JHEXEN__
    { 97, 25 },
#elif __JHERETIC__
    { 97, 30 },
#elif __JDOOM__ || __JDOOM64__
    { 97, 40 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawSoundPage, NULL,
    &OptionsMenu,
    //0, 5
};

#if __JDOOM64__
mndata_slider_t sld_hud_viewsize = { 3, 11, 0, 1, false, "view-size" };
#else
mndata_slider_t sld_hud_viewsize = { 3, 13, 0, 1, false, "view-size" };
#endif
mndata_slider_t sld_hud_wideoffset = { 0, 1, 0, .1f, true, "hud-wideoffset" };
mndata_slider_t sld_hud_uptime = { 0, 60, 0, 1.f, true, "hud-timer", "Disabled", NULL, " second", " seconds" };
mndata_slider_t sld_hud_xhair_size = { 0, 1, 0, .1f, true, "view-cross-size" };
mndata_slider_t sld_hud_xhair_opacity = { 0, 1, 0, .1f, true, "view-cross-a" };
mndata_slider_t sld_hud_size = { 0, 1, 0, .1f, true, "hud-scale" };
mndata_slider_t sld_hud_cntr_size = { 0, 1, 0, .1f, true, "hud-cheat-counter-scale" };
mndata_slider_t sld_hud_sbar_size = { 0, 1, 0, .1f, true, "hud-status-size" };
mndata_slider_t sld_hud_sbar_opacity = { 0, 1, 0, .1f, true, "hud-status-alpha" };
mndata_slider_t sld_hud_messages_size = { 0, 1, 0, .1f, true, "msg-scale" };
mndata_slider_t sld_hud_messages_uptime = { 0, 60, 0, 1.f, true, "msg-uptime", "Disabled", NULL, " second", " seconds" };

mndata_colorbox_t cbox_hud_color = {
    0, 0, 0, 0, 0, 0, true,
    "hud-color-r", "hud-color-g", "hud-color-b", "hud-color-a"
};

mndata_listitem_t listit_hud_xhair_symbols[] = {
    { "None", 0 },
    { "Cross", 1 },
    { "Angles", 2 },
    { "Square", 3 },
    { "Open Square", 4 },
    { "Diamond", 5 },
    { "V", 6 }
};
mndata_list_t list_hud_xhair_symbol = {
    listit_hud_xhair_symbols, NUMLISTITEMS(listit_hud_xhair_symbols), "view-cross-type"
};

mndata_colorbox_t cbox_hud_xhair_color = {
    0, 0, 0, 0, 0, 0, false,
    "view-cross-r", "view-cross-g", "view-cross-b",
};

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_listitem_t listit_hud_killscounter_displaymethods[] = {
    { "Hidden",         0 },
    { "Count",          CCH_KILLS },
    { "Percent",        CCH_KILLS_PRCNT },
    { "Count+Percent",  CCH_KILLS | CCH_KILLS_PRCNT },
};
mndata_list_t list_hud_cntr_kills = {
    listit_hud_killscounter_displaymethods,
    NUMLISTITEMS(listit_hud_killscounter_displaymethods),
    "hud-cheat-counter", CCH_KILLS | CCH_KILLS_PRCNT
};

mndata_listitem_t listit_hud_itemscounter_displaymethods[] = {
    { "Hidden",         0 },
    { "Count",          CCH_ITEMS },
    { "Percent",        CCH_ITEMS_PRCNT },
    { "Count+Percent",  CCH_ITEMS | CCH_ITEMS_PRCNT },
};
mndata_list_t list_hud_cntr_items = {
    listit_hud_itemscounter_displaymethods,
    NUMLISTITEMS(listit_hud_itemscounter_displaymethods),
    "hud-cheat-counter", CCH_ITEMS | CCH_ITEMS_PRCNT
};

mndata_listitem_t listit_hud_secretscounter_displaymethods[] = {
    { "Hidden",         0 },
    { "Count",          CCH_SECRETS },
    { "Percent",        CCH_SECRETS_PRCNT },
    { "Count+Percent",  CCH_SECRETS | CCH_SECRETS_PRCNT },
};
mndata_list_t list_hud_cntr_secrets = {
    listit_hud_secretscounter_displaymethods,
    NUMLISTITEMS(listit_hud_secretscounter_displaymethods),
    "hud-cheat-counter", CCH_SECRETS | CCH_SECRETS_PRCNT
};
#endif

mndata_text_t txt_hud_view_size = { "View Size" };
mndata_text_t txt_hud_wide_offset = { "Wide Offset" };
#if __JDOOM__
mndata_text_t txt_hud_single_key_display = { "Single Key Display" };
#endif
mndata_text_t txt_hud_autohide = { "AutoHide" };
mndata_text_t txt_hud_unhide_events = { "UnHide Events" };
mndata_text_t txt_hud_unhide_receive_damage = { "Receive Damage" };
mndata_text_t txt_hud_unhide_pickup_health = { "Pickup Health" };
mndata_text_t txt_hud_unhide_pickup_armor = { "Pickup Armor" };
mndata_text_t txt_hud_unhide_pickup_powerup = { "Pickup Powerup" };
mndata_text_t txt_hud_unhide_pickup_weapon = { "Pickup Weapon" };
#if __JHEXEN__
mndata_text_t txt_hud_unhide_pickup_ammo = { "Pickup Mana" };
#else
mndata_text_t txt_hud_unhide_pickup_ammo = { "Pickup Ammo" };
#endif
mndata_text_t txt_hud_unhide_pickup_key = { "Pickup Key" };
#if __JHERETIC__ || __JHEXEN__
mndata_text_t txt_hud_unhide_pickup_item = { "Pickup Item" };
#endif

mndata_text_t txt_hud_messages = { "Messages" };
mndata_text_t txt_hud_msg_shown = { "Shown" };
mndata_text_t txt_hud_msg_size = { "Size" };
mndata_text_t txt_hud_msg_uptime = { "Uptime" };

mndata_text_t txt_hud_crosshair = { "Crosshair" };
mndata_text_t txt_hud_xhair_symbol = { "Symbol" };
mndata_text_t txt_hud_xhair_size = { "Size" };
mndata_text_t txt_hud_xhair_opacity = { "Opacity" };
mndata_text_t txt_hud_xhair_vitality_color = { "Vitality Color" };
mndata_text_t txt_hud_xhair_color = { "Color" };

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
mndata_text_t txt_hud_statusbar = { "Statusbar" };
mndata_text_t txt_hud_sbar_size = { "Size" };
mndata_text_t txt_hud_sbar_opacity = { "Opacity" };
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_text_t txt_hud_counters = { "Counters" };
mndata_text_t txt_hud_cntr_kills = { "Kills" };
mndata_text_t txt_hud_cntr_items = { "Items" };
mndata_text_t txt_hud_cntr_secrets = { "Secrets" };
mndata_text_t txt_hud_cntr_size = { "Size" };
#endif

mndata_text_t txt_hud_fullscreen = { "Fullscreen" };
mndata_text_t txt_hud_full_size = { "Size" };
mndata_text_t txt_hud_full_text_color = { "Text Color" };
#if __JHEXEN__
mndata_text_t txt_hud_full_show_mana = { "Show Mana" };
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_text_t txt_hud_full_show_ammo = { "Show Ammo" };
mndata_text_t txt_hud_full_show_armor = { "Show Armor" };
#endif
#if __JDOOM64__
mndata_text_t txt_hud_full_show_powerkeys = { "Show PowerKeys" };
#endif
#if __JDOOM__
mndata_text_t txt_hud_full_show_face = { "Show Face" };
#endif
mndata_text_t txt_hud_full_show_health = { "Show Health" };
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_text_t txt_hud_full_show_keys = { "Show Keys" };
#endif
#if __JHERETIC__ || __JHEXEN__
mndata_text_t txt_hud_full_show_readyitem = { "Show Ready-Item" };
#endif

mndata_button_t btn_hud_single_key_display = { true, "hud-keys-combine" };
mndata_button_t btn_hud_unhide_receive_damage = { true, "hud-unhide-damage" };
mndata_button_t btn_hud_unhide_pickup_health = { true, "hud-unhide-pickup-health" };
mndata_button_t btn_hud_unhide_pickup_armor = { true, "hud-unhide-pickup-armor" };
mndata_button_t btn_hud_unhide_pickup_powerup = { true, "hud-unhide-pickup-powerup" };
mndata_button_t btn_hud_unhide_pickup_weapon = { true, "hud-unhide-pickup-weapon" };
mndata_button_t btn_hud_unhide_pickup_ammo = { true, "hud-unhide-pickup-ammo" };
mndata_button_t btn_hud_unhide_pickup_key = { true, "hud-unhide-pickup-key" };
#if __JHERETIC__ || __JHEXEN__
mndata_button_t btn_hud_unhide_pickup_item = { true, "hud-unhide-pickup-invitem" };
#endif
mndata_button_t btn_hud_msg_shown = { true, "msg-show" };
mndata_button_t btn_hud_xhair_vitality_color = { true, "view-cross-vitality" };
#if __JHEXEN__
mndata_button_t btn_hud_full_show_mana = { true, "hud-mana" };
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_button_t btn_hud_full_show_ammo = { true, "hud-ammo" };
mndata_button_t btn_hud_full_show_armor = { true, "hud-armor" };
#endif
#if __JDOOM64__
mndata_button_t btn_hud_full_show_powerkeys = { true, "hud-power" };
#endif
#if __JDOOM__
mndata_button_t btn_hud_full_show_face = { true, "hud-face" };
#endif
mndata_button_t btn_hud_full_show_health = { true, "hud-health" };
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_button_t btn_hud_full_show_keys = { true, "hud-keys" };
#endif
#if __JHERETIC__ || __JHEXEN__
mndata_button_t btn_hud_full_show_readyitem = { true, "hud-currentitem" };
#endif

static mn_object_t HudMenuObjects[] = {
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_view_size },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_viewsize },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_wide_offset },
    { MN_SLIDER,    0,  0,  'w',MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_wideoffset },
#if __JDOOM__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_single_key_display },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_single_key_display },
#endif
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_autohide },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNSlider_TextualValueUpdateDimensions, MNSlider_TextualValueDrawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_uptime },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_events },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_receive_damage },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_receive_damage },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_health },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_health },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_armor },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_armor },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_powerup },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_powerup },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_weapon },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_weapon },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_ammo },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_ammo },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_key },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_key },
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_item },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_item },
#endif

    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_messages },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_msg_shown },
    { MN_BUTTON,    0,  0,  'm',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_msg_shown },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_msg_size },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_messages_size },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_msg_uptime },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNSlider_TextualValueUpdateDimensions, MNSlider_TextualValueDrawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_messages_uptime },

    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_crosshair },
    { MN_TEXT,      0,  0,  'c',MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_symbol },
    { MN_LIST,      0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_hud_xhair_symbol },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_size },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_xhair_size },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_opacity },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_xhair_opacity },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_vitality_color },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_xhair_vitality_color },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_color },
    { MN_COLORBOX,  0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, Hu_MenuCvarColorBox, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_hud_xhair_color },

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_statusbar },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_sbar_size },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_sbar_size },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_sbar_opacity },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_sbar_opacity },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_counters },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_kills },
    { MN_LIST,      0,  0,  'k',MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_hud_cntr_kills },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_items },
    { MN_LIST,      0,  0,  'i',MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_hud_cntr_items },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_secrets },
    { MN_LIST,      0,  0,  's',MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_hud_cntr_secrets },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_size },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_cntr_size },
#endif

    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_fullscreen },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_size },
    { MN_SLIDER,    0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_size },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_text_color },
    { MN_COLORBOX,  0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, Hu_MenuCvarColorBox, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_hud_color },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_mana },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_mana },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_ammo },
    { MN_BUTTON,    0,  0,  'a',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_ammo },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_armor },
    { MN_BUTTON,    0,  0,  'r',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_armor },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_powerkeys },
    { MN_BUTTON,    0,  0,  'p',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_powerkeys },
#endif
#if __JDOOM__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_face },
    { MN_BUTTON,    0,  0,  'f',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_face },
#endif
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_health },
    { MN_BUTTON,    0,  0,  'h',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_health },
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_keys },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_keys },
#endif
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_readyitem },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_readyitem },
#endif
    { MN_NONE }
};

mn_page_t HudMenu = {
    HudMenuObjects,
#if __JDOOM__ || __JDOOM64__
    { 97, 40 },
#else
    { 97, 28 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawHUDPage, NULL,
    &OptionsMenu,
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
mndata_slider_t sld_inv_uptime = { 0, 30, 0, 1.f, true, "hud-inventory-timer", "Disabled", NULL, " second", " seconds" };
mndata_slider_t sld_inv_maxvisslots = { 0, 16, 0, 1, false, "hud-inventory-slot-max", "Automatic" };

mndata_text_t txt_inv_select_mode = { "Select Mode" };
mndata_text_t txt_inv_wrap_around = { "Wrap Around" };
mndata_text_t txt_inv_choose_and_use = { "Choose And Use" };
mndata_text_t txt_inv_next_if_failed = { "Select Next If Use Failed" };
mndata_text_t txt_inv_autohide = { "AutoHide" };
mndata_text_t txt_inv_fullscreen = { "Fullscreen HUD" };
mndata_text_t txt_inv_full_max_visible_slots = { "Max Visible Slots" };
mndata_text_t txt_inv_full_show_empty_slots = { "Show Empty Slots" };

mndata_button_t btn_inv_select_mode = { true, "ctl-inventory-mode" };
mndata_button_t btn_inv_wrap_around = { true, "ctl-inventory-wrap" };
mndata_button_t btn_inv_choose_and_use = { true, "ctl-inventory-use-immediate" };
mndata_button_t btn_inv_next_if_failed = { true, "ctl-inventory-use-next" };
mndata_button_t btn_inv_full_show_empty_slots = { true, "hud-inventory-slot-showempty" };

static mn_object_t InventoryMenuObjects[] = {
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_select_mode },
    { MN_BUTTON,    0,  0,  's',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_inv_select_mode },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_wrap_around },
    { MN_BUTTON,    0,  0,  'w',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_inv_wrap_around },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_choose_and_use },
    { MN_BUTTON,    0,  0,  'c',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_inv_choose_and_use },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_next_if_failed },
    { MN_BUTTON,    0,  0,  'n',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_inv_next_if_failed },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_autohide },
    { MN_SLIDER,    0,  0,  'h',MENU_FONT1, MENU_COLOR3, MNSlider_TextualValueUpdateDimensions, MNSlider_TextualValueDrawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_inv_uptime },

    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_fullscreen },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_full_max_visible_slots },
    { MN_SLIDER,    0,  0,  'v',MENU_FONT1, MENU_COLOR3, MNSlider_TextualValueUpdateDimensions, MNSlider_TextualValueDrawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_inv_maxvisslots },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_inv_full_show_empty_slots },
    { MN_BUTTON,    0,  0,  'e',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_inv_full_show_empty_slots },
    { MN_NONE }
};

mn_page_t InventoryMenu = {
    InventoryMenuObjects,
    { 78, 48 }, { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawInventoryPage, NULL,
    &OptionsMenu,
    //0, 12, { 12, 48 }
};
#endif

mndata_listitem_t listit_weapons_order[NUM_WEAPON_TYPES] = {
#if __JDOOM__ || __JDOOM64__
    { (const char*)TXT_WEAPON1,             WT_FIRST },
    { (const char*)TXT_WEAPON2,             WT_SECOND },
    { (const char*)TXT_WEAPON3,             WT_THIRD },
    { (const char*)TXT_WEAPON4,             WT_FOURTH },
    { (const char*)TXT_WEAPON5,             WT_FIFTH },
    { (const char*)TXT_WEAPON6,             WT_SIXTH },
    { (const char*)TXT_WEAPON7,             WT_SEVENTH },
    { (const char*)TXT_WEAPON8,             WT_EIGHTH },
    { (const char*)TXT_WEAPON9,             WT_NINETH },
#  if __JDOOM64__
    { (const char*)TXT_WEAPON10,            WT_TENTH }
#  endif
#elif __JHERETIC__
    { (const char*)TXT_TXT_WPNSTAFF,        WT_FIRST },
    { (const char*)TXT_TXT_WPNWAND,         WT_SECOND },
    { (const char*)TXT_TXT_WPNCROSSBOW,     WT_THIRD },
    { (const char*)TXT_TXT_WPNBLASTER,      WT_FOURTH },
    { (const char*)TXT_TXT_WPNSKULLROD,     WT_FIFTH },
    { (const char*)TXT_TXT_WPNPHOENIXROD,   WT_SIXTH },
    { (const char*)TXT_TXT_WPNMACE,         WT_SEVENTH },
    { (const char*)TXT_TXT_WPNGAUNTLETS,    WT_EIGHTH }
#elif __JHEXEN__
    /**
     * \fixme We should allow different weapon preferences per player-class.
     */
    { "First",  WT_FIRST },
    { "Second", WT_SECOND },
    { "Third",  WT_THIRD },
    { "Fourth", WT_FOURTH }
#endif
};

mndata_list_t list_weapons_order = {
    listit_weapons_order, NUMLISTITEMS(listit_weapons_order)
};

mndata_listitem_t listit_weapons_autoswitch_pickup[] = {
    { "Never", 0 },
    { "If Better", 1 },
    { "Always", 2 }
};
mndata_list_t list_weapons_autoswitch_pickup = {
    listit_weapons_autoswitch_pickup, NUMLISTITEMS(listit_weapons_autoswitch_pickup), "player-autoswitch"
};

mndata_listitem_t listit_weapons_autoswitch_pickupammo[] = {
    { "Never", 0 },
    { "If Better", 1 },
    { "Always", 2 }
};
mndata_list_t list_weapons_autoswitch_pickupammo = {
    listit_weapons_autoswitch_pickupammo, NUMLISTITEMS(listit_weapons_autoswitch_pickupammo), "player-autoswitch-ammo"
};

mndata_text_t txt_weapons_priority_order = { "Priority Order" };
mndata_text_t txt_weapons_cycling = { "Cycling" };
mndata_text_t txt_weapons_cycl_use_priority_order = { "Use Priority Order" };
mndata_text_t txt_weapons_cycl_sequential = { "Sequential" };
mndata_text_t txt_weapons_autoswitch = { "Autoswitch" };
mndata_text_t txt_weapons_aswch_pickup_weapon = { "Pickup Weapon" };
mndata_text_t txt_weapons_aswch_pickup_weapon_ifnot_firing = { "   If Not Firing" };
mndata_text_t txt_weapons_aswch_pickup_ammo = { "Pickup Ammo" };
#if __JDOOM__ || __JDOOM64__
mndata_text_t txt_weapons_aswch_pickup_berserk = { "Pickup Beserk" };
#endif

mndata_button_t btn_weapons_cycl_use_priority_order = { true, "player-weapon-nextmode" };
mndata_button_t btn_weapons_cycl_sequential = { true, "player-weapon-cycle-sequential" };
mndata_button_t btn_weapons_aswch_pickup_weapon_ifnot_firing = { true, "player-autoswitch-notfiring" };
#if __JDOOM__ || __JDOOM64__
mndata_button_t btn_weapons_aswch_pickup_berserk = { true, "player-autoswitch-berserk" };
#endif

static mn_object_t WeaponMenuObjects[] = {
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_priority_order },
    { MN_LIST,      0,  0,  'p',MENU_FONT1, MENU_COLOR3, MNList_UpdateDimensions, MNList_Drawer, { Hu_MenuChangeWeaponPriority, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_CommandResponder, NULL, NULL, &list_weapons_order },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_cycling },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_cycl_use_priority_order },
    { MN_BUTTON,    0,  0,  'o',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_weapons_cycl_use_priority_order },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_cycl_sequential },
    { MN_BUTTON,    0,  0,  's',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_weapons_cycl_sequential },

    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_autoswitch },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_aswch_pickup_weapon },
    { MN_LIST,      0,  0,  'w',MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_weapons_autoswitch_pickup },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_aswch_pickup_weapon_ifnot_firing },
    { MN_BUTTON,    0,  0,  'f',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_weapons_aswch_pickup_weapon_ifnot_firing },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_aswch_pickup_ammo },
    { MN_LIST,      0,  0,  'a',MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_weapons_autoswitch_pickupammo },
#if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_weapons_aswch_pickup_berserk },
    { MN_BUTTON,    0,  0,  'b',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_weapons_aswch_pickup_berserk },
#endif
    { MN_NONE }
};

mn_page_t WeaponMenu = {
    WeaponMenuObjects,
#if __JDOOM__ || __JDOOM64__
    { 78, 40 },
#elif __JHERETIC__
    { 78, 26 },
#elif __JHEXEN__
    { 78, 38 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawWeaponsPage, NULL,
    &OptionsMenu,
    //0, 12, { 12, 38 }
};

mndata_slider_t sld_map_opacity = { 0, 1, 0, 0.1f, true, "map-opacity" };
mndata_slider_t sld_map_lineopacity = { 0, 1, 0, 0.1f, true, "map-alpha-lines" };
mndata_slider_t sld_map_doorglow = { 0, 200, 0, 5, true, "map-door-glow" };

mndata_listitem_t lstit_map_statusbar[] = {
    { "NONE", 0 },
    { "CURRENT", 1 },
    { "STATUSBAR", 2 }
};
mndata_list_t lst_map_statusbar = {
    lstit_map_statusbar, NUMLISTITEMS(lstit_map_statusbar), "map-huddisplay"
};

mndata_listitem_t lstit_map_customcolors[] = {
    { "NEVER", 0 },
    { "AUTO", 1 },
    { "ALWAYS", 2 }
};
mndata_list_t lst_map_customcolors = {
    lstit_map_customcolors, NUMLISTITEMS(lstit_map_customcolors), "map-customcolors"
};

mndata_colorbox_t cbox_map_line_unseen_color = { 
    0, 0, 0, 0, 0, 0, false,
    "map-wall-unseen-r", "map-wall-unseen-g", "map-wall-unseen-b"
};
mndata_colorbox_t cbox_map_line_solid_color = {
    0, 0, 0, 0, 0, 0, false,
    "map-wall-r", "map-wall-g", "map-wall-b"
};
mndata_colorbox_t cbox_map_line_floor_color = {
    0, 0, 0, 0, 0, 0, false,
    "map-wall-floorchange-r", "map-wall-floorchange-g", "map-wall-floorchange-b"
};
mndata_colorbox_t cbox_map_line_ceiling_color = {
    0, 0, 0, 0, 0, 0, false,
    "map-wall-ceilingchange-r", "map-wall-ceilingchange-g", "map-wall-ceilingchange-b"
};
mndata_colorbox_t cbox_map_mobj_color = {
    0, 0, 0, 0, 0, 0, false,
    "map-mobj-r", "map-mobj-g", "map-mobj-b"
};
mndata_colorbox_t cbox_map_background_color = {
    0, 0, 0, 0, 0, 0, false,
    "map-background-r", "map-background-g", "map-background-b"
};

mndata_text_t txt_map_opacity = { "Background Opacity" };
mndata_text_t txt_map_line_opacity = { "Line Opacity" };
mndata_text_t txt_map_hud_display = { "HUD Display" };
mndata_text_t txt_map_door_colors = { "Door Colors" };
mndata_text_t txt_map_door_glow = { "Door Glow" };
mndata_text_t txt_map_use_custom_colors = { "Use Custom Colors" };
mndata_text_t txt_map_color_wall = { "Wall" };
mndata_text_t txt_map_color_floor_height_change = { "Floor Height Change" };
mndata_text_t txt_map_color_ceiling_height_change = { "Ceiling Height Change" };
mndata_text_t txt_map_color_unseen = { "Unseen" };
mndata_text_t txt_map_color_thing = { "Thing" };
mndata_text_t txt_map_color_background = { "Background" };

mndata_button_t btn_map_door_colors = { true, "map-door-colors" };

mn_object_t AutomapMenuObjects[] = {
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_opacity },
    { MN_SLIDER,    0,  0,  'o', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_map_opacity },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_line_opacity },
    { MN_SLIDER,    0,  0,  'l', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_map_lineopacity },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_hud_display },
    { MN_LIST,      0,  0,  'h', MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &lst_map_statusbar },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_door_colors },
    { MN_BUTTON,    0,  0,  'c', MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_map_door_colors },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_door_glow },
    { MN_SLIDER,    0,  0,  'g', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_map_doorglow },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_use_custom_colors },
    { MN_LIST,      0,  0,  0,   MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &lst_map_customcolors },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_color_wall },
    { MN_COLORBOX,  0,  0,  'w', MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, NULL, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_map_line_solid_color },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_color_floor_height_change },
    { MN_COLORBOX,  0,  0,  'f', MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, NULL, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_map_line_floor_color },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_color_ceiling_height_change },
    { MN_COLORBOX,  0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, NULL, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_map_line_ceiling_color },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_color_unseen },
    { MN_COLORBOX,  0,  0,  'u', MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, NULL, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_map_line_unseen_color },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_color_thing },
    { MN_COLORBOX,  0,  0,  't', MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, NULL, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_map_mobj_color },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_map_color_background },
    { MN_COLORBOX,  0,  0,  'b', MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { Hu_MenuCvarColorBox, NULL, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_map_background_color },
    { MN_NONE }
};

mn_page_t AutomapMenu = {
    AutomapMenuObjects,
#if __JHERETIC__ || __JHEXEN__
    { 64, 28 },
#else
    { 70, 40 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawAutomapPage, NULL,
    &OptionsMenu,
#if __JHERETIC__ || __JHEXEN__
    //0, 11, { 11, 28 }
#else
    //0, 13, { 13, 40 }
#endif
};

mndata_text_t txt_gameplay_always_run = { "Always Run" };
mndata_text_t txt_gameplay_use_lookspring = { "Use LookSpring" };
mndata_text_t txt_gameplay_use_autoaim = { "Use AutoAim" };
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
mndata_text_t txt_gameplay_allow_jumping = { "Allow Jumping" };
#endif
#if __JDOOM64__
mndata_text_t txt_gameplay_weapon_recoil = { "Weapon Recoil" };
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
mndata_text_t txt_gameplay_compatibility = { "Compatibility" };
# if __JDOOM__ || __JDOOM64__
mndata_text_t txt_gameplay_any_boss_trigger_666 = { "Any Boss Trigger 666" };
#  if !__JDOOM64__
mndata_text_t txt_gameplay_archvile_resurrects_ghosts = { "Av Resurrects Ghosts" };
#  endif
mndata_text_t txt_gameplay_painelemental_limit_lostsouls = { "PE Limited To 21 Lost Souls" };
mndata_text_t txt_gameplay_lostsouls_stuck = { "LS Can Get Stuck Inside Walls" };
# endif
mndata_text_t txt_gameplay_monsters_stuck_in_doors = { "Monsters Can Get Stuck In Doors" };
mndata_text_t txt_gameplay_never_hang_over_ledges = { "Some Objects Never Hang Over Ledges" };
mndata_text_t txt_gameplay_fall_under_own_weight = { "Objects Fall Under Own Weight" };
mndata_text_t txt_gameplay_corpse_stair_slide = { "Corpses Slide Down Stairs" };
mndata_text_t txt_gameplay_vanilla_exact_clipping = { "Use Exactly Doom's Clipping Code" };
mndata_text_t txt_gameplay_vanilla_exact_clipping_ifnot_wallrun = { "  ^If Not NorthOnly WallRunning" };
# if __JDOOM__ || __JDOOM64__
mndata_text_t txt_gameplay_zombie_exits = { "Zombie Players Can Exit Maps" };
mndata_text_t txt_gameplay_fix_ouch_face = { "Fix Ouch Face" };
mndata_text_t txt_gameplay_fix_weapon_slot = { "Fix Weapon Slot Display" };
# endif
#endif

mndata_button_t btn_gameplay_always_run = { true, "ctl-run" };
mndata_button_t btn_gameplay_use_lookspring = { true, "ctl-look-spring" };
mndata_button_t btn_gameplay_use_autoaim = { true, "ctl-aim-noauto" };
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
mndata_button_t btn_gameplay_allow_jumping = { true, "player-jump" };
#endif
#if __JDOOM64__
mndata_button_t btn_gameplay_weapon_recoil = { true, "player-weapon-recoil" };
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
# if __JDOOM__ || __JDOOM64__
mndata_button_t btn_gameplay_any_boss_trigger_666 = { true, "game-anybossdeath666" };
#  if !__JDOOM64__
mndata_button_t btn_gameplay_archvile_resurrects_ghosts = { true, "game-raiseghosts" };
#  endif
mndata_button_t btn_gameplay_painelemental_limit_lostsouls = { true, "game-maxskulls" };
mndata_button_t btn_gameplay_lostsouls_stuck = { true, "game-skullsinwalls" };
# endif
mndata_button_t btn_gameplay_monsters_stuck_in_doors = { true, "game-monsters-stuckindoors" };
mndata_button_t btn_gameplay_never_hang_over_ledges = { true, "game-objects-neverhangoverledges" };
mndata_button_t btn_gameplay_fall_under_own_weight = { true, "game-objects-falloff" };
mndata_button_t btn_gameplay_corpse_stair_slide = { true, "game-corpse-sliding" };
mndata_button_t btn_gameplay_vanilla_exact_clipping = { true, "game-objects-clipping" };
mndata_button_t btn_gameplay_vanilla_exact_clipping_ifnot_wallrun = { true, "game-player-wallrun-northonly" };
# if __JDOOM__ || __JDOOM64__
mndata_button_t btn_gameplay_zombie_exits = { true, "game-zombiescanexit" };
mndata_button_t btn_gameplay_fix_ouch_face = { true, "hud-face-ouchfix" };
mndata_button_t btn_gameplay_fix_weapon_slot = { true, "hud-status-weaponslots-ownedfix" };
# endif
#endif

static mn_object_t GameplayMenuObjects[] = {
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_always_run },
    { MN_BUTTON,    0,  0,  'r',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_always_run },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_use_lookspring },
    { MN_BUTTON,    0,  0,  'l',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_use_lookspring },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_use_autoaim },
    { MN_BUTTON,    0,  0,  'a',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_use_autoaim },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_allow_jumping },
    { MN_BUTTON,    0,  0,  'j',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_allow_jumping },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_weapon_recoil },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_weapon_recoil },
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR2, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_compatibility },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_any_boss_trigger_666 },
    { MN_BUTTON,    0,  0,  'b',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_any_boss_trigger_666 },
#  if !__JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_archvile_resurrects_ghosts },
    { MN_BUTTON,    0,  0,  'g',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_archvile_resurrects_ghosts },
#  endif
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_painelemental_limit_lostsouls },
    { MN_BUTTON,    0,  0,  'p',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_painelemental_limit_lostsouls },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_lostsouls_stuck },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_lostsouls_stuck },
# endif
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_monsters_stuck_in_doors },
    { MN_BUTTON,    0,  0,  'd',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_monsters_stuck_in_doors },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_never_hang_over_ledges },
    { MN_BUTTON,    0,  0,  'h',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_never_hang_over_ledges },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_fall_under_own_weight },
    { MN_BUTTON,    0,  0,  'f',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_fall_under_own_weight },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_corpse_stair_slide },
    { MN_BUTTON,    0,  0,  's',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_corpse_stair_slide },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_vanilla_exact_clipping },
    { MN_BUTTON,    0,  0,  'c',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_vanilla_exact_clipping },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_vanilla_exact_clipping_ifnot_wallrun },
    { MN_BUTTON,    0,  0,  'w',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_vanilla_exact_clipping_ifnot_wallrun },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_zombie_exits },
    { MN_BUTTON,    0,  0,  'e',MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_zombie_exits },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_fix_ouch_face },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_fix_ouch_face },
    { MN_TEXT,      0,  0,  0,  MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_gameplay_fix_weapon_slot },
    { MN_BUTTON,    0,  0,  0,  MENU_FONT1, MENU_COLOR3, MNButton_UpdateDimensions, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_gameplay_fix_weapon_slot },
# endif
#endif
    { MN_NONE }
};

mn_page_t GameplayMenu = {
    GameplayMenuObjects,
#if __JHEXEN__
    { 88, 25 },
#elif __JHERETIC__
    { 30, 40 },
#else
    { 30, 40 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawGameplayPage, NULL,
    &OptionsMenu,
#if __JHEXEN__
    //0, 6, { 6, 25 }
#elif __JDOOM64__
    //0, 16, { 16, 40 }
#elif __JDOOM__
    //0, 18, { 18, 40 }
#else
    //0, 11, { 11, 40 }
#endif
};

mndata_button_t btn_multiplayer_player_setup = { false, NULL, "Player Setup" };
mndata_button_t btn_multiplayer_join_game = { false, NULL, "Join Game" };

mn_object_t MultiplayerMenuObjects[] = {
    { MN_BUTTON,    0,  0,  's',MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectPlayerSetup, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_multiplayer_player_setup },
    { MN_BUTTON,    0,  0,  'j',MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectJoinGame, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_multiplayer_join_game },
    { MN_NONE }
};

mn_page_t MultiplayerMenu = {
    MultiplayerMenuObjects,
#if __JHERETIC__ || __JHEXEN__
    { 97, 65 },
#else
    { 97, 65 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawMultiplayerPage, NULL,
    &GameTypeMenu,
    //0, 2
};

mndata_mobjpreview_t mop_player_preview;
mndata_edit_t edit_player_name = { "", "", 0, NULL, "net-name" };

#if __JHEXEN__
mndata_listitem_t listit_player_class[3] = {
    { (const char*)TXT_PLAYERCLASS1, PCLASS_FIGHTER },
    { (const char*)TXT_PLAYERCLASS2, PCLASS_CLERIC },
    { (const char*)TXT_PLAYERCLASS3, PCLASS_MAGE }
};
mndata_list_t list_player_class = {
    listit_player_class, NUMLISTITEMS(listit_player_class)
};
#endif

/// \todo Read these names from Text definitions.
mndata_listitem_t listit_player_color[NUMPLAYERCOLORS+1] = {
#if __JHEXEN__
    { "Red",        0 },
    { "Blue",       1 },
    { "Yellow",     2 },
    { "Green",      3 },
    { "Jade",       4 },
    { "White",      5 },
    { "Hazel",      6 },
    { "Purple",     7 },
    { "Automatic",  8 }
#elif __JHERETIC__
    { "Green",      0 },
    { "Orange",     1 },
    { "Red",        2 },
    { "Blue",       3 },
    { "Automatic",  4 }
#else
    { "Green",      0 },
    { "Indigo",     1 },
    { "Brown",      2 },
    { "Red",        3 },
    { "Automatic",  4}
#endif
};
mndata_list_t list_player_color = {
    listit_player_color, NUMLISTITEMS(listit_player_color)
};

#if __JHEXEN__
mndata_text_t txt_player_class = { "Class" };
#endif
mndata_text_t txt_player_color = { "Color" };

mndata_button_t btn_player_save_changes = { false, NULL, "Save Changes" };

mn_object_t PlayerSetupMenuObjects[] = {
    { MN_MOBJPREVIEW, 0,MNF_ID0,    0,   0, 0, MNMobjPreview_UpdateDimensions, MNMobjPreview_Drawer, { NULL }, NULL, NULL, NULL, &mop_player_preview },
    { MN_EDIT,      0,  MNF_ID1,    0,   MENU_FONT1, MENU_COLOR1, MNEdit_UpdateDimensions, MNEdit_Drawer, { NULL, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_player_name },
#if __JHEXEN__
    { MN_TEXT,      0,  0,          0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_player_class },
    { MN_LIST,      0,  MNF_ID2,    'c', MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuSelectPlayerSetupPlayerClass, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_player_class },
#endif
    { MN_TEXT,      0,  0,          0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_player_color },
    { MN_LIST,      0,  MNF_ID3,    0,   MENU_FONT1, MENU_COLOR3, MNList_InlineUpdateDimensions, MNList_InlineDrawer, { Hu_MenuSelectPlayerColor, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNList_InlineCommandResponder, NULL, NULL, &list_player_color },
    { MN_BUTTON,    0,  0,          's', MENU_FONT2, MENU_COLOR1, MNButton_UpdateDimensions, MNButton_Drawer, { NULL, Hu_MenuSelectAcceptPlayerSetup, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_player_save_changes },
    { MN_NONE }
};

mn_page_t PlayerSetupMenu = {
    PlayerSetupMenuObjects,
#if __JHERETIC__ || __JHEXEN__
    { 70, 54 },
#else
    { 70, 54 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    Hu_MenuDrawPlayerSetupPage, NULL,
    &MultiplayerMenu,
#if __JHEXEN__
    //0, 7
#else
    //0, 5
#endif
};

mndata_slider_t sld_colorwidget_red = {
    0, 1, 0, .05f, true
};
mndata_slider_t sld_colorwidget_green = {
    0, 1, 0, .05f, true
};
mndata_slider_t sld_colorwidget_blue = {
    0, 1, 0, .05f, true
};
mndata_slider_t sld_colorwidget_alpha = {
    0, 1, 0, .05f, true
};

mndata_colorbox_t cbox_colorwidget_mixcolor = {
    SCREENHEIGHT/7, SCREENHEIGHT/7, 0, 0, 0, 0, true
};

mndata_text_t txt_colorwidget_red   = { "Red" };
mndata_text_t txt_colorwidget_green = { "Green" };
mndata_text_t txt_colorwidget_blue  = { "Blue" };
mndata_text_t txt_colorwidget_alpha = { "Alpha" };

static mn_object_t ColorWidgetMenuObjects[] = {
    { MN_COLORBOX,  0,  MNF_NO_FOCUS, 0, MENU_FONT1, MENU_COLOR1, MNColorBox_UpdateDimensions, MNColorBox_Drawer, { NULL }, NULL, NULL, NULL, &cbox_colorwidget_mixcolor },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_colorwidget_red },
    { MN_SLIDER,    0,  0,  'r', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuUpdateColorWidgetColor, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_red,   NULL, CR },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_colorwidget_green },
    { MN_SLIDER,    0,  0,  'g', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuUpdateColorWidgetColor, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_green, NULL, CG },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_colorwidget_blue },
    { MN_SLIDER,    0,  0,  'b', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuUpdateColorWidgetColor, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_blue,  NULL, CB },
    { MN_TEXT,      0,  0,  0,   MENU_FONT1, MENU_COLOR1, MNText_UpdateDimensions, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_colorwidget_alpha },
    { MN_SLIDER,    0,  0,  'a', MENU_FONT1, MENU_COLOR1, MNSlider_UpdateDimensions, MNSlider_Drawer, { Hu_MenuUpdateColorWidgetColor, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_alpha, NULL, CA },
    { MN_NONE }
};

static mn_page_t ColorWidgetMenu = {
    ColorWidgetMenuObjects,
#if __JHERETIC__ || __JHEXEN__
    { 98, 60 },
#else
    { 98, 60 },
#endif
    { (fontid_t)GF_FONTA, (fontid_t)GF_FONTB }, { 0, 1, 2 },
    NULL, Hu_MenuColorWidgetCmdResponder,
    &OptionsMenu,
    //0, 8
};

// Cvars for the menu:
cvartemplate_t menuCVars[] = {
    { "menu-scale",     0,  CVT_FLOAT,  &cfg.menuScale, .1f, 1 },
    { "menu-stretch",   0,  CVT_BYTE,   &cfg.menuScaleMode, SCALEMODE_FIRST, SCALEMODE_LAST },
    { "menu-flash-r",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CR], 0, 1 },
    { "menu-flash-g",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CG], 0, 1 },
    { "menu-flash-b",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CB], 0, 1 },
    { "menu-flash-speed", 0, CVT_INT,   &cfg.menuTextFlashSpeed, 0, 50 },
    { "menu-turningskull", 0, CVT_BYTE, &cfg.menuCursorRotate, 0, 1 },
    { "menu-effect",    0,  CVT_INT,    &cfg.menuEffectFlags, 0, MEF_EVERYTHING },
    { "menu-color-r",   0,  CVT_FLOAT,  &cfg.menuTextColors[0][CR], 0, 1 },
    { "menu-color-g",   0,  CVT_FLOAT,  &cfg.menuTextColors[0][CG], 0, 1 },
    { "menu-color-b",   0,  CVT_FLOAT,  &cfg.menuTextColors[0][CB], 0, 1 },
    { "menu-colorb-r",  0,  CVT_FLOAT,  &cfg.menuTextColors[1][CR], 0, 1 },
    { "menu-colorb-g",  0,  CVT_FLOAT,  &cfg.menuTextColors[1][CG], 0, 1 },
    { "menu-colorb-b",  0,  CVT_FLOAT,  &cfg.menuTextColors[1][CB], 0, 1 },
    { "menu-colorc-r",  0,  CVT_FLOAT,  &cfg.menuTextColors[2][CR], 0, 1 },
    { "menu-colorc-g",  0,  CVT_FLOAT,  &cfg.menuTextColors[2][CG], 0, 1 },
    { "menu-colorc-b",  0,  CVT_FLOAT,  &cfg.menuTextColors[2][CB], 0, 1 },
    { "menu-colord-r",  0,  CVT_FLOAT,  &cfg.menuTextColors[3][CR], 0, 1 },
    { "menu-colord-g",  0,  CVT_FLOAT,  &cfg.menuTextColors[3][CG], 0, 1 },
    { "menu-colord-b",  0,  CVT_FLOAT,  &cfg.menuTextColors[3][CB], 0, 1 },
    { "menu-glitter",   0,  CVT_FLOAT,  &cfg.menuTextGlitter, 0, 1 },
    { "menu-fog",       0,  CVT_INT,    &cfg.hudFog, 0, 5 },
    { "menu-shadow",    0,  CVT_FLOAT,  &cfg.menuShadow, 0, 1 },
    { "menu-patch-replacement", 0, CVT_BYTE, &cfg.usePatchReplacement, 0, 2 },
    { "menu-slam",      0,  CVT_BYTE,   &cfg.menuSlam,  0, 1 },
    { "menu-quick-ask", 0,  CVT_BYTE,   &cfg.confirmQuickGameSave, 0, 1 },
    { "menu-hotkeys",   0,  CVT_BYTE,   &cfg.menuShortcutsEnabled, 0, 1 },
#if __JDOOM__ || __JDOOM64__
    { "menu-quitsound", 0,  CVT_INT,    &cfg.menuQuitSound, 0, 1 },
#endif
    { "menu-save-suggestname", 0, CVT_BYTE, &cfg.menuGameSaveSuggestName, 0, 1 },
    { NULL }
};

// Console commands for the menu:
ccmdtemplate_t menuCCmds[] = {
    { "menu",           "s",    CCmdMenuOpen },
    { "menu",           "",     CCmdMenuOpen },
    { "menuup",         "",     CCmdMenuCommand },
    { "menudown",       "",     CCmdMenuCommand },
    { "menupageup",     "",     CCmdMenuCommand },
    { "menupagedown",   "",     CCmdMenuCommand },
    { "menuleft",       "",     CCmdMenuCommand },
    { "menuright",      "",     CCmdMenuCommand },
    { "menuselect",     "",     CCmdMenuCommand },
    { "menudelete",     "",     CCmdMenuCommand },
    { "menuback",       "",     CCmdMenuCommand },
    { NULL }
};

// Code -------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up
 * Register Cvars and CCmds for the operation/look of the menu.
 */
void Hu_MenuRegister(void)
{
    int i;
    for(i = 0; menuCVars[i].name; ++i)
        Con_AddVariable(menuCVars + i);
    for(i = 0; menuCCmds[i].name; ++i)
        Con_AddCommand(menuCCmds + i);
}

static boolean chooseCloseMethod(void)
{
    // If we aren't using a transition then we can close normally and allow our
    // own menu fade-out animation to be used instead.
    return Con_GetInteger("con-transition-tics") == 0? MCMD_CLOSE : MCMD_CLOSEFAST;
}

mn_page_t* Hu_MenuFindPageForName(const char* name)
{
    const struct pagename_pair_s {
        mn_page_t* page;
        const char* name;
    } pairs[] = {
        { &MainMenu,        "Main" },
        { &GameTypeMenu,    "GameType" },
#if __JHEXEN__
        { &PlayerClassMenu, "PlayerClass" },
#endif
#if __JDOOM__ || __JHERETIC__
        { &EpisodeMenu,     "Episode" },
#endif
        { &SkillMenu,       "Skill" },
        { &OptionsMenu,     "Options" },
        { &SoundMenu,       "SoundOptions" },
        { &GameplayMenu,    "GameplayOptions" },
        { &HudMenu,         "HudOptions" },
        { &AutomapMenu,     "AutomapOptions" },
#if __JHERETIC__ || __JHEXEN__
        { &FilesMenu,       "Files" },
#endif
        { &LoadMenu,        "LoadGame" },
        { &SaveMenu,        "SaveGame" },
        { &MultiplayerMenu, "Multiplayer" },
        { &PlayerSetupMenu, "PlayerSetup" },
#if __JHERETIC__ || __JHEXEN__
        { &InventoryMenu,   "InventoryOptions" },
#endif
        { &WeaponMenu,      "WeaponOptions" },
        { &ControlsMenu,    "ControlOptions" },
        { NULL }
    };
    size_t i;
    for(i = 0; NULL != pairs[i].page; ++i)
    {
        if(!stricmp(name, pairs[i].name))
        {
            return pairs[i].page;
        }
    }
    return NULL;
}

/// \todo Make this state an object property flag.
/// @return  @c true if the rotation of a cursor on this object should be animated.
static boolean Hu_MenuHasCursorRotation(mn_object_t* obj)
{
    assert(NULL != obj);
    return (!(MNObject_Flags(obj) & MNF_DISABLED) &&
       ((MNObject_Type(obj) == MN_LIST && obj->cmdResponder == MNList_InlineCommandResponder) ||
                 MNObject_Type(obj) == MN_SLIDER));
}

/// To be called to re-evaluate the state of the cursor (e.g., when focus changes).
static void Hu_MenuUpdateCursorState(void)
{
    if(menuActive)
    {
        mn_page_t* page;
        mn_object_t* obj;

        if(colorWidgetActive)
            page = &ColorWidgetMenu;
        else
            page = Hu_MenuActivePage();

        obj = MNPage_FocusObject(page);
        if(NULL != obj)
        {
            cursorHasRotation = Hu_MenuHasCursorRotation(obj);
            return;
        }
    }
    cursorHasRotation = false;
}

void Hu_MenuComposeSubpageString(mn_page_t* page, size_t bufSize, char* buf)
{
    assert(NULL != page);
    if(NULL == buf || 0 == bufSize) return;
/*    snprintf(buf, bufSize, "PAGE %i/%i", (page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1,
            (int)ceil((float)page->objectsCount/page->numVisObjects));*/
}

void Hu_MenuLoadResources(void)
{
    char buffer[9];

#if __JDOOM__ || __JDOOM64__
    pMainTitle = R_PrecachePatch("M_DOOM", NULL);
#elif __JHERETIC__ || __JHEXEN__
    pMainTitle = R_PrecachePatch("M_HTIC", NULL);
#endif

#if __JDOOM__ || __JDOOM64__
    pNewGame  = R_PrecachePatch("M_NEWG", NULL);
    pSkill    = R_PrecachePatch("M_SKILL", NULL);
    pEpisode  = R_PrecachePatch("M_EPISOD", NULL);
    pNGame    = R_PrecachePatch("M_NGAME", NULL);
    pOptions  = R_PrecachePatch("M_OPTION", NULL);
    pLoadGame = R_PrecachePatch("M_LOADG", NULL);
    pSaveGame = R_PrecachePatch("M_SAVEG", NULL);
    pReadThis = R_PrecachePatch("M_RDTHIS", NULL);
    pQuitGame = R_PrecachePatch("M_QUITG", NULL);
    pOptionsTitle = R_PrecachePatch("M_OPTTTL", NULL);
#endif

#if __JDOOM__ || __JDOOM64__
    pSkillModeNames[SM_BABY]      = R_PrecachePatch("M_JKILL", NULL);
    pSkillModeNames[SM_EASY]      = R_PrecachePatch("M_ROUGH", NULL);
    pSkillModeNames[SM_MEDIUM]    = R_PrecachePatch("M_HURT", NULL);
    pSkillModeNames[SM_HARD]      = R_PrecachePatch("M_ULTRA", NULL);
#  if __JDOOM__
    pSkillModeNames[SM_NIGHTMARE] = R_PrecachePatch("M_NMARE", NULL);
#  endif
#endif

#if __JDOOM__
    if(gameModeBits & (GM_DOOM_SHAREWARE|GM_DOOM|GM_DOOM_ULTIMATE))
    {
        pEpisodeNames[0] = R_PrecachePatch("M_EPI1", NULL);
        pEpisodeNames[1] = R_PrecachePatch("M_EPI2", NULL);
        pEpisodeNames[2] = R_PrecachePatch("M_EPI3", NULL);
    }
    if(gameModeBits & GM_DOOM_ULTIMATE)
    {
        pEpisodeNames[3] = R_PrecachePatch("M_EPI4", NULL);
    }
#endif

#if __JHERETIC__
    { int i;
    for(i = 0; i < 18; ++i)
    {
        dd_snprintf(buffer, 9, "M_SKL%02d", i);
        pRotatingSkull[i] = R_PrecachePatch(buffer, NULL);
    }}
#endif

#if __JHEXEN__
    { int i;
    for(i = 0; i < 7; ++i)
    {
        dd_snprintf(buffer, 9, "FBUL%c0", 'A'+i);
        pBullWithFire[i] = R_PrecachePatch(buffer, NULL);
    }}

    pPlayerClassBG[0] = R_PrecachePatch("M_FBOX", NULL);
    pPlayerClassBG[1] = R_PrecachePatch("M_CBOX", NULL);
    pPlayerClassBG[2] = R_PrecachePatch("M_MBOX", NULL);
#endif

    { int i;
    for(i = 0; i < MENU_CURSOR_FRAMECOUNT; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        dd_snprintf(buffer, 9, "M_SKULL%d", i+1);
#else
        dd_snprintf(buffer, 9, "M_SLCTR%d", i+1);
#endif
        pCursors[i] = R_PrecachePatch(buffer, NULL);
    }}
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

void Hu_MenuInitWeaponsMenu(void)
{
    qsort(listit_weapons_order, NUMLISTITEMS(listit_weapons_order),
        sizeof(listit_weapons_order[0]), compareWeaponPriority);
}

#if __JDOOM__ || __JHERETIC__
/**
 * Construct the episode selection menu.
 */
void Hu_MenuInitEpisodeMenu(void)
{
    int i, maxw, w, numEpisodes;
    mn_object_t* obj;
    mndata_button_t* btn;

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
    EpisodeMenuObjects = Z_Calloc(sizeof(mn_object_t) * (numEpisodes+1), PU_GAMESTATIC, 0);
    EpisodeMenuButtons = Z_Calloc(sizeof(mndata_button_t) * (numEpisodes), PU_GAMESTATIC, 0);
    obj = EpisodeMenuObjects;
    btn = EpisodeMenuButtons;
    for(i = 0, maxw = 0; i < numEpisodes; ++i)
    {
        const char* text = GET_TXT(TXT_EPISODE1 + i);

        obj->_type = MN_BUTTON;
        btn->text = text;
# if __JDOOM__
        btn->patch = &pEpisodeNames[i];
# endif
        obj->_typedata = btn;
        obj->drawer = MNButton_Drawer;
        obj->cmdResponder = MNButton_CommandResponder;
        obj->updateDimensions = MNButton_UpdateDimensions;
        if(isalnum(text[0]))
            obj->_shortcut = tolower(text[0]);

#if __JHERETIC__
        if(gameMode == heretic_shareware && i != 0)
        {
            obj->actions[MNA_ACTIVEOUT].callback = Hu_MenuActivateNotSharewareEpisode;
        }
        else
        {
            obj->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
            obj->data1 = &SkillMenu;
        }
#else
        if(gameMode == doom_shareware && i != 0)
        {
            obj->actions[MNA_ACTIVEOUT].callback = Hu_MenuActivateNotSharewareEpisode;
        }
        else
        {
            obj->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
            obj->data1 = &SkillMenu;
        }
#endif
        obj->actions[MNA_FOCUS].callback = Hu_MenuFocusEpisode;
        obj->data2 = i;
        obj->_pageFontIdx = MENU_FONT2;
        FR_SetFont(FID(MNPage_PredefinedFont(&EpisodeMenu, obj->_pageFontIdx)));
        FR_LoadDefaultAttrib();
        w = FR_TextWidth(text);
        if(w > maxw)
            maxw = w;
        obj++;
        btn++;
    }
    obj->_type = MN_NONE;

    // Finalize setup.
    EpisodeMenu.objects = EpisodeMenuObjects;
    EpisodeMenu.offset[VX] = SCREENWIDTH/2 - maxw / 2 + 18; // Center the menu appropriately.
}
#endif

#if __JHEXEN__
/**
 * Construct the player class selection menu.
 */
void Hu_MenuInitPlayerClassMenu(void)
{
    uint i, n, count;
    mn_object_t* obj;
    mndata_button_t* btn;

    // First determine the number of selectable player classes.
    count = 0;
    for(i = 0; i < NUM_PLAYER_CLASSES; ++i)
    {
        classinfo_t* info = PCLASS_INFO(i);
        if(info->userSelectable)
            ++count;
    }

    // Allocate the menu objects array.
    PlayerClassMenuObjects = Z_Calloc(sizeof(mn_object_t) * (count+3), PU_GAMESTATIC, 0);
    PlayerClassMenuButtons = Z_Calloc(sizeof(mndata_button_t) * (count+1), PU_GAMESTATIC, 0);

    // Add the selectable classes.
    n = 0;
    obj = PlayerClassMenuObjects;
    btn = PlayerClassMenuButtons;
    while(n < count)
    {
        classinfo_t* info = PCLASS_INFO(n++);
        if(!info->userSelectable)
            continue;

        obj->_type = MN_BUTTON;
        btn->text = info->niceName;
        obj->_typedata = btn;
        obj->drawer = MNButton_Drawer;
        obj->cmdResponder = MNButton_CommandResponder;
        obj->updateDimensions = MNButton_UpdateDimensions;
        obj->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
        obj->actions[MNA_FOCUS].callback = Hu_MenuFocusOnPlayerClass;
        obj->data2 = (int)info->plrClass;
        obj->_shortcut = tolower(btn->text[0]);
        obj->_pageFontIdx = MENU_FONT2;
        obj->_pageColorIdx = MENU_COLOR1;
        obj++;
        btn++;
    }

    // Random class button.
    obj->_type = MN_BUTTON;
    btn->text = GET_TXT(TXT_RANDOMPLAYERCLASS);
    obj->_typedata = btn;
    obj->drawer = MNButton_Drawer;
    obj->cmdResponder = MNButton_CommandResponder;
    obj->updateDimensions = MNButton_UpdateDimensions;
    obj->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
    obj->actions[MNA_FOCUS].callback = Hu_MenuFocusOnPlayerClass;
    obj->data2 = (int)PCLASS_NONE;
    obj->_shortcut = tolower(btn->text[0]);
    obj->_pageFontIdx = MENU_FONT2;
    obj->_pageColorIdx = MENU_COLOR1;
    obj++;

    // Mobj preview.
    obj->_type = MN_MOBJPREVIEW;
    obj->_flags = MNF_ID0;
    obj->updateDimensions = MNMobjPreview_UpdateDimensions;
    obj->drawer = MNMobjPreview_Drawer;
    obj->_typedata = &mop_playerclass_preview;
    obj++;

    // Terminate.
    obj->_type = MN_NONE;

    // Finalize setup.
    PlayerClassMenu.objects = PlayerClassMenuObjects;
}
#endif

void Hu_MenuInit(void)
{
    mnAlpha = mnTargetAlpha = 0;
    menuActivePage = NULL;
    menuActive = false;
    cursorHasRotation = false;
    cursorAngle = 0;
    cursorAnimFrame = 0;
    cursorAnimCounter = MENU_CURSOR_TICSPERFRAME;

    DD_Execute(true, "deactivatebcontext menu");

#if __JDOOM__ || __JHERETIC__
    Hu_MenuInitEpisodeMenu();
#endif
#if __JHEXEN__
    Hu_MenuInitPlayerClassMenu();
#endif
    Hu_MenuInitControlsPage();
    Hu_MenuInitWeaponsMenu();

    initAllObjectsOnAllPages();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Skill name init and auto-centering.
    /// \fixme Do this (optionally) during page initialization.
    { int i, w, maxw;
    for(i = 0, maxw = 0; i < NUM_SKILL_MODES; ++i)
    {
        /// \fixme Find objects by id.
        const char* text = GET_TXT(TXT_SKILL1 + i);
        mn_object_t* obj = &SkillMenu.objects[i];
        mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
        btn->text = text;
        MNObject_SetShortcut(obj, text[0]);
        FR_SetFont(MNPage_PredefinedFont(&SkillMenu, MNObject_Font(obj)));
        FR_LoadDefaultAttrib();
        w = FR_TextWidth(text);
        if(w > maxw)
            maxw = w;
    }
    SkillMenu.offset[VX] = SCREENWIDTH/2 - maxw / 2 + 14;
    }
#endif

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        mn_object_t* obj = MN_MustFindObjectOnPage(&MainMenu, 0, MNF_ID0); // Read This!
        MNObject_SetFlags(obj, FO_SET, MNF_DISABLED|MNF_HIDDEN|MNF_NO_FOCUS);
        MainMenu.offset[VY] += 8;
    }
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        SkillMenu.previous = &GameTypeMenu;
    }
#endif

    Hu_MenuLoadResources();
}

boolean Hu_MenuIsActive(void)
{
    return menuActive;
}

void Hu_MenuSetAlpha(float alpha)
{
    // The menu's alpha will start moving towards this target value.
    mnTargetAlpha = alpha;
}

float Hu_MenuAlpha(void)
{
    return mnAlpha;
}

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

    if(menuActive)
    {
        menuFlashCounter += (int)(cfg.menuTextFlashSpeed * ticLength * TICRATE);
        if(menuFlashCounter >= 100)
            menuFlashCounter -= 100;

        if(cfg.menuCursorRotate)
        {
            if(cursorHasRotation)
            {
                cursorAngle += (float)(5 * ticLength * TICRATE);
            }
            else if(cursorAngle != 0)
            {
                float rewind = (float)(MENU_CURSOR_REWIND_SPEED * ticLength * TICRATE);
                if(cursorAngle <= rewind || cursorAngle >= 360 - rewind)
                    cursorAngle = 0;
                else if(cursorAngle < 180)
                    cursorAngle -= rewind;
                else
                    cursorAngle += rewind;
            }

            if(cursorAngle >= 360)
                cursorAngle -= 360;
        }
    }

    // The following is restricted to fixed 35 Hz ticks.
    if(!M_RunTrigger(&fixed, ticLength))
        return; // It's too soon.

    if(menuActive)
    {
        menuTime++;

        // Animate the cursor patches.
        if(--cursorAnimCounter <= 0)
        {
            cursorAnimFrame++;
            cursorAnimCounter = MENU_CURSOR_TICSPERFRAME;
            if(cursorAnimFrame > MENU_CURSOR_FRAMECOUNT-1)
                cursorAnimFrame = 0;
        }

        // Used for Heretic's rotating skulls.
        frame = (menuTime / 3) % 18;
    }
}

mn_page_t* Hu_MenuActivePage(void)
{
    return menuActivePage;
}

void Hu_MenuSetActivePage(mn_page_t* page)
{
    if(!menuActive) return;
    if(NULL == page) return;

    FR_ResetTypeinTimer();
    menuFlashCounter = 0; // Restart selection flash animation.
    cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
    menuNominatingQuickSaveSlot = false;

    if(menuActivePage == page) return;

    // This is now the "active" page.
    menuActivePage = page;
    MNPage_Initialize(page);
}

boolean Hu_MenuIsVisible(void)
{
    return (menuActive || mnAlpha > .0001f);
}

int Hu_MenuDefaultFocusAction(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_FOCUS != action) return 1;
    menuFlashCounter = 0; // Restart selection flash animation.
    Hu_MenuUpdateCursorState();
    return 0;
}

void Hu_MenuDrawFocusCursor(int x, int y, int focusObjectHeight, float alpha)
{
#if __JDOOM__ || __JDOOM64__
# define OFFSET_X         (-22)
# define OFFSET_Y         (-2)
#elif __JHERETIC__ || __JHEXEN__
# define OFFSET_X         (-16)
# define OFFSET_Y         (3)
#endif

    const int cursorIdx = cursorAnimFrame;
    const float angle = cursorAngle;
    patchid_t pCursor = pCursors[cursorIdx % MENU_CURSOR_FRAMECOUNT];
    float scale, pos[2];
    patchinfo_t info;

    if(!R_GetPatchInfo(pCursor, &info))
        return;

    scale = MIN_OF((float) (focusObjectHeight * 1.267f) / info.height, 1);
    pos[VX] = x + OFFSET_X * scale;
    pos[VY] = y + OFFSET_Y * scale + focusObjectHeight/2;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(pos[VX], pos[VY], 0);
    DGL_Scalef(scale, scale, 1);
    DGL_Rotatef(angle, 0, 0, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, alpha);

    GL_DrawPatch3(pCursor, 0, 0, 0, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef OFFSET_Y
#undef OFFSET_X
}

static void drawOverlayBackground(float darken)
{
    DGL_SetNoMaterial();
    DGL_DrawRectColor(0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0, darken);
}

static void beginOverlayDraw(void)
{
#define SMALL_SCALE             .75f

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);

#undef SMALL_SCALE
}

static void endOverlayDraw(void)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Hu_MenuDrawer(void)
{
#define OVERLAY_DARKEN          .7f

    const int winWidth  = Get(DD_WINDOW_WIDTH);
    const int winHeight = Get(DD_WINDOW_HEIGHT);
    borderedprojectionstate_t bp;
    boolean showFocusCursor = true;
    mn_object_t* focusObj;

    if(!Hu_MenuIsVisible()) return;

    GL_ConfigureBorderedProjection(&bp, 0, SCREENWIDTH, SCREENHEIGHT, winWidth, winHeight, cfg.menuScaleMode);
    GL_BeginBorderedProjection(&bp);

    // First determine whether the focus cursor should be visible.
    focusObj = MNPage_FocusObject(Hu_MenuActivePage());
    if(NULL != focusObj && (MNObject_Flags(focusObj) & MNF_ACTIVE))
    {
        if(MNObject_Type(focusObj) == MN_COLORBOX || MNObject_Type(focusObj) == MN_BINDINGS)
        {
            showFocusCursor = false;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
    DGL_Scalef(cfg.menuScale, cfg.menuScale, 1);
    DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);

    MN_DrawPage(Hu_MenuActivePage(), mnAlpha, showFocusCursor);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    GL_EndBorderedProjection(&bp);

    // Drawing any overlays?
    if(NULL != focusObj && (MNObject_Flags(focusObj) & MNF_ACTIVE))
    {
        switch(MNObject_Type(focusObj))
        {
        case MN_COLORBOX:
        case MN_BINDINGS:
            drawOverlayBackground(OVERLAY_DARKEN);
            GL_BeginBorderedProjection(&bp);

            beginOverlayDraw();
            if(MNObject_Type(focusObj) == MN_BINDINGS)
            {
                Hu_MenuControlGrabDrawer(MNBindings_ControlName(focusObj), 1);
            }
            else
            {
                MN_DrawPage(&ColorWidgetMenu, 1, true);
            }
            endOverlayDraw();

            GL_EndBorderedProjection(&bp);
            break;
        default: break;
        }
    }

#undef OVERLAY_DARKEN
}

void Hu_MenuNavigatePage(mn_page_t* page, int pageDelta)
{
#if 0
    assert(NULL != page);
    {
    int index = MAX_OF(0, page->focus), oldIndex = index;

    if(pageDelta < 0)
    {
        index = MAX_OF(0, index - page->numVisObjects);
    }
    else
    {
        index = MIN_OF(page->objectsCount-1, index + page->numVisObjects);
    }

    // Don't land on empty objects.
    while((page->objects[index].flags & (MNF_DISABLED | MNF_NO_FOCUS)) && (index > 0))
        index--;
    while((page->objects[index].flags & (MNF_DISABLED | MNF_NO_FOCUS)) && index < page->objectsCount)
        index++;

    if(index != oldIndex)
    {
        S_LocalSound(SFX_MENU_NAV_RIGHT, NULL);
        MNPage_SetFocus(page, page->objects + index);
    }
    }
#endif
}

/**
 * \fixme This should be the job of the page-init logic.
 */
static void updateObjectsLinkedWithCvars(mn_object_t* objs)
{
    mn_object_t* obj;
    for(obj = objs; MNObject_Type(obj) != MN_NONE; obj++)
    {
        switch(MNObject_Type(obj))
        {
        case MN_BUTTON: {
            const mn_actioninfo_t* action = MNObject_Action(obj, MNA_MODIFIED);
            mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
            if(NULL != action && action->callback == Hu_MenuCvarButton)
            {
                cvarbutton_t* cvb;
                if(obj->data1)
                {
                    // This button has already been initialized.
                    cvb = (cvarbutton_t*) obj->data1;
                    cvb->active = (Con_GetByte(cvb->cvarname) & (cvb->mask? cvb->mask : ~0)) != 0;
                    //strcpy(obj->text, cvb->active ? cvb->yes : cvb->no);
                    btn->text = cvb->active ? cvb->yes : cvb->no;
                    continue;
                }
                // Find the cvarbutton representing this one.
                for(cvb = mnCVarButtons; cvb->cvarname; cvb++)
                {
                    if(!strcmp(btn->data, cvb->cvarname) && obj->data2 == cvb->mask)
                    {
                        cvb->active = (Con_GetByte(cvb->cvarname) & (cvb->mask? cvb->mask : ~0)) != 0;
                        obj->data1 = (void*) cvb;
                        btn->yes = cvb->yes;
                        btn->no  = cvb->no;
                        btn->text = (cvb->active ? btn->yes : btn->no);
                        break;
                    }
                }
                cvb = NULL;
            }
            break;
          }
        case MN_LIST: {
            const mn_actioninfo_t* action = MNObject_Action(obj, MNA_MODIFIED);
            mndata_list_t* list = (mndata_list_t*) obj->_typedata;
            if(NULL != action && action->callback == Hu_MenuCvarList)
            {
                MNList_SelectItemByValue(obj, MNLIST_SIF_NO_ACTION, Con_GetInteger(list->data));
            }
            break;
          }
        case MN_EDIT: {
            const mn_actioninfo_t* action = MNObject_Action(obj, MNA_MODIFIED);
            mndata_edit_t* edit = (mndata_edit_t*) obj->_typedata;
            if(NULL != action && action->callback == Hu_MenuCvarEdit)
            {
                MNEdit_SetText(obj, MNEDIT_STF_NO_ACTION, Con_GetString(edit->data1));
            }
            break;
          }
        case MN_SLIDER: {
            const mn_actioninfo_t* action = MNObject_Action(obj, MNA_MODIFIED);
            mndata_slider_t* sldr = (mndata_slider_t*) obj->_typedata;
            if(NULL != action && action->callback == Hu_MenuCvarSlider)
            {
                float value;
                if(sldr->floatMode)
                    value = Con_GetFloat(sldr->data1);
                else
                    value = Con_GetInteger(sldr->data1);
                MNSlider_SetValue(obj, MNSLIDER_SVF_NO_ACTION, value);
            }
            break;
          }
        case MN_COLORBOX: {
            const mn_actioninfo_t* action = MNObject_Action(obj, MNA_MODIFIED);
            mndata_colorbox_t* cbox = (mndata_colorbox_t*) obj->_typedata;
            if(NULL != action && action->callback == Hu_MenuCvarColorBox)
            {
                float rgba[4];
                rgba[CR] = Con_GetFloat(cbox->data1);
                rgba[CG] = Con_GetFloat(cbox->data2);
                rgba[CB] = Con_GetFloat(cbox->data3);
                rgba[CA] = (cbox->rgbaMode? Con_GetFloat(cbox->data4) : 1.f);
                MNColorBox_SetColor4fv(obj, MNCOLORBOX_SCF_NO_ACTION, rgba);
            }
            break;
          }
        default:
            break;
        }
    }
}

static void initPage(mn_page_t* page)
{
    assert(NULL != page);
    page->focus = -1; /// \fixme Make this a page flag.
    page->objectsCount = MN_CountObjects(page->objects);
    { int i;
    for(i = 0; i < (int)MENU_FONT_COUNT; ++i)
    {
        if(0 == (gamefontid_t)page->fonts[i]) continue;
        page->fonts[i] = FID((gamefontid_t)page->fonts[i]);
    }}
    // Init objects.
    { mn_object_t* obj;
    for(obj = page->objects; MNObject_Type(obj) != MN_NONE; obj++)
    {
        obj->_dimensions.x = 0;
        obj->_dimensions.y = 0;
        obj->_dimensions.width  = 0;
        obj->_dimensions.height = 0;
        MNObject_SetFlags(obj, FO_CLEAR, MNF_FOCUS);
        if(0 != obj->_shortcut)
        {
            int shortcut = obj->_shortcut;
            obj->_shortcut = 0; // Clear invalid defaults.
            MNObject_SetShortcut(obj, shortcut);
        }
    }}
    updateObjectsLinkedWithCvars(page->objects);
}

static void initAllObjectsOnAllPages(void)
{
    // Set default Yes/No strings.
    { cvarbutton_t* cvb;
    for(cvb = mnCVarButtons; cvb->cvarname; cvb++)
    {
        if(!cvb->yes)
            cvb->yes = "Yes";
        if(!cvb->no)
            cvb->no = "No";
    }}

    initPage(&MainMenu);
    initPage(&GameTypeMenu);
#if __JHEXEN__
    initPage(&PlayerClassMenu);
#endif
#if __JDOOM__ || __JHERETIC__
    initPage(&EpisodeMenu);
#endif
    initPage(&SkillMenu);
    initPage(&MultiplayerMenu);
    initPage(&PlayerSetupMenu);
#if __JHERETIC__ || __JHEXEN__
    initPage(&FilesMenu);
#endif
    initPage(&LoadMenu);
    initPage(&SaveMenu);
    initPage(&OptionsMenu);
    initPage(&ControlsMenu);
    initPage(&GameplayMenu);
    initPage(&HudMenu);
    initPage(&AutomapMenu);
    initPage(&WeaponMenu);
#if __JHERETIC__ || __JHEXEN__
    initPage(&InventoryMenu);
#endif
    initPage(&SoundMenu);
    initPage(&ColorWidgetMenu);
}

int Hu_MenuColorWidgetCmdResponder(mn_page_t* page, menucommand_e cmd)
{
    assert(NULL != page);
    switch(cmd)
    {
    case MCMD_NAV_OUT: {
        mn_object_t* obj = (mn_object_t*)page->data;
        MNObject_SetFlags(obj, FO_CLEAR, MNF_ACTIVE);
        S_LocalSound(SFX_MENU_CANCEL, NULL);
        colorWidgetActive = false;

        /// \kludge We should re-focus on the object instead.
        cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true;
      }
    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        return true; // Eat these.
    case MCMD_SELECT: {
        mn_object_t* obj = (mn_object_t*)page->data;
        MNObject_SetFlags(obj, FO_CLEAR, MNF_ACTIVE);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        colorWidgetActive = false;
        /// \fixme Find object by id.
        MNColorBox_CopyColor(obj, 0, &ColorWidgetMenu.objects[0]);

        /// \kludge We should re-focus on the object instead.
        cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true;
      }
    default:
        break;
    }
    return false;
}

static void fallbackCommandResponder(mn_page_t* page, menucommand_e cmd)
{
    assert(NULL != page);
    switch(cmd)
    {
    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        S_LocalSound(cmd == MCMD_NAV_PAGEUP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
        Hu_MenuNavigatePage(page, cmd == MCMD_NAV_PAGEUP? -1 : +1);
        break;

    case MCMD_NAV_UP:
    case MCMD_NAV_DOWN: {
        mn_object_t* obj = MNPage_FocusObject(page);
        // An object on this page must have focus in order to navigate.
        if(NULL != obj)
        {
            int i = 0, giveFocus = page->focus;
            do
            {
                giveFocus += (cmd == MCMD_NAV_UP? -1 : 1);
                if(giveFocus < 0)
                    giveFocus = page->objectsCount - 1;
                else if(giveFocus >= page->objectsCount)
                    giveFocus = 0;
            } while(++i < page->objectsCount && (MNObject_Flags(page->objects + giveFocus) & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN)));

            if(giveFocus != page->focus)
            {
                S_LocalSound(cmd == MCMD_NAV_UP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
                MNPage_SetFocus(page, page->objects + giveFocus);
            }
        }
        break;
      }
    case MCMD_NAV_OUT:
        if(!page->previous)
        {
            S_LocalSound(SFX_MENU_CLOSE, NULL);
            Hu_MenuCommand(MCMD_CLOSE);
        }
        else
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            Hu_MenuSetActivePage(page->previous);
        }
        break;
    default:
/*#if _DEBUG
        Con_Message("Warning:fallbackCommandResponder: Command %i not processed.\n", (int) cmd);
#endif*/
        break;
    }
}

/// Depending on the current menu state some commands require translating.
static menucommand_e translateCommand(menucommand_e cmd)
{
    // If a close command is received while currently working with a selected
    // "active" widget - interpret the command instead as "navigate out".
    if(menuActive && (cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST))
    {
        mn_object_t* obj = MNPage_FocusObject(Hu_MenuActivePage());
        if(NULL != obj)
        {
            switch(MNObject_Type(obj))
            {
            case MN_EDIT:
            case MN_LIST:
            case MN_COLORBOX:
                if(MNObject_Flags(obj) & MNF_ACTIVE)
                {
                    cmd = MCMD_NAV_OUT;
                }
                break;
            default:
                break;
            }
        }
    }
    return cmd;
}

void Hu_MenuCommand(menucommand_e cmd)
{
    //uint firstVisible, lastVisible; // first and last visible obj
    //uint numVisObjectsOffset = 0;
    mn_page_t* page;
    mn_object_t* obj;

    /*firstVisible = page->firstObject;
    lastVisible = firstVisible + page->numVisObjects - 1 - numVisObjectsOffset;
    if(lastVisible > page->objectsCount - 1 - numVisObjectsOffset)
        lastVisible = page->objectsCount - 1 - numVisObjectsOffset;*/

    cmd = translateCommand(cmd);

    // Determine the page which will respond to this command.
    if(colorWidgetActive)
        page = &ColorWidgetMenu;
    else
        page = Hu_MenuActivePage();

    if(cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST)
    {
        if(menuActive)
        {
            menuNominatingQuickSaveSlot = false;

            Hu_FogEffectSetAlphaTarget(0);

            if(cmd == MCMD_CLOSEFAST)
            {   // Hide the menu instantly.
                mnAlpha = mnTargetAlpha = 0;
            }
            else
            {
                mnTargetAlpha = 0;
            }

            if(cmd != MCMD_CLOSEFAST)
                S_LocalSound(SFX_MENU_CLOSE, NULL);

            menuActive = false;

            // Disable the menu binding context.
            DD_Execute(true, "deactivatebcontext menu");
        }
        return;
    }

    // No other commands are responded to once shutdown has begun.
    if(GA_QUIT == G_GetGameAction())
    {
        return;
    }

    if(!menuActive)
    {
        if(MCMD_OPEN == cmd)
        {
            // If anyone is currently chatting; the menu cannot be opened.
            int i;
            for(i = 0; i < MAXPLAYERS; ++i)
            {
                if(ST_ChatIsActive(i))
                    return;
            }

            S_LocalSound(SFX_MENU_OPEN, NULL);

            Con_Open(false);

            Hu_FogEffectSetAlphaTarget(1);
            Hu_MenuSetAlpha(1);
            menuActive = true;
            menuTime = 0;
            Hu_MenuSetActivePage(&MainMenu);

            // Enable the menu binding class
            DD_Execute(true, "activatebcontext menu");
            B_SetContextFallback("menu", Hu_MenuFallbackResponder);
        }
        return;
    }

    // Try the current focus object.
    obj = MNPage_FocusObject(page);
    if(NULL != obj && NULL != obj->cmdResponder)
    {
        if(obj->cmdResponder(obj, cmd))
            return;
    }

    // Try the page's cmd responder.
    if(NULL != page->cmdResponder)
    {
        if(page->cmdResponder(page, cmd))
            return;
    }

    fallbackCommandResponder(page, cmd);
}

int Hu_MenuPrivilegedResponder(event_t* ev)
{
    if(Hu_MenuIsActive())
    {
        mn_object_t* obj = MNPage_FocusObject(Hu_MenuActivePage());
        if(NULL != obj && !(MNObject_Flags(obj) & MNF_DISABLED))
        {
            if(NULL != obj->privilegedResponder)
            {
                return obj->privilegedResponder(obj, ev);
            }
        }
    }
    return false;
}

int Hu_MenuResponder(event_t* ev)
{
    if(Hu_MenuIsActive())
    {
        mn_object_t* obj = MNPage_FocusObject(Hu_MenuActivePage());
        if(NULL != obj && !(MNObject_Flags(obj) & MNF_DISABLED))
        {
            if(NULL != obj->responder)
            {
                return obj->responder(obj, ev);
            }
        }
    }
    return false; // Not eaten.
}

int Hu_MenuFallbackResponder(event_t* ev)
{
    mn_page_t* page = Hu_MenuActivePage();

    if(!Hu_MenuIsActive() || NULL == page) return false;

    if(cfg.menuShortcutsEnabled)
    {
        if(ev->type == EV_KEY && (ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        {
            int i;
            for(i = 0; i < page->objectsCount; ++i)
            {
                mn_object_t* obj = &page->objects[i];
                if(MNObject_Flags(obj) & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN))
                    continue;

                if(MNObject_Shortcut(obj) == ev->data1)
                {
                    MNPage_SetFocus(page, obj);
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * User wants to load this game
 */
int Hu_MenuSelectLoadSlot(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->_typedata;
    const int saveSlot = edit->data2;
    if(MNA_ACTIVEOUT != action) return 1;
    MNPage_SetFocus(&SaveMenu, MNPage_FindObject(&SaveMenu, 0, obj->data2));
    G_LoadGame(saveSlot);
    Hu_MenuCommand(chooseCloseMethod());
    return 0;
    }
}

void Hu_MenuDrawMainPage(mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JDOOM64__
#  define TITLEOFFSET_X         (-3)
#  define TITLEOFFSET_Y         (-62)
#else
#  define TITLEOFFSET_X         (-22)
#  define TITLEOFFSET_Y         (-56)
#endif

#if __JHEXEN__
    int frame = (menuTime / 5) % 7;
#endif

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(1, 1, 1, mnRendState->pageAlpha);

    WI_DrawPatch3(pMainTitle, Hu_ChoosePatchReplacement(pMainTitle), x + TITLEOFFSET_X, y + TITLEOFFSET_Y, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#if __JHEXEN__
    GL_DrawPatch(pBullWithFire[(frame + 2) % 7], x - 73, y + 24);
    GL_DrawPatch(pBullWithFire[frame], x + 168, y + 24);
#elif __JHERETIC__
    GL_DrawPatch(pRotatingSkull[17 - frame], x - 70, y - 46);
    GL_DrawPatch(pRotatingSkull[frame], x + 122, y - 46);
#endif

    DGL_Disable(DGL_TEXTURE_2D);

#undef TITLEOFFSET_Y
#undef TITLEOFFSET_X
}

void Hu_MenuDrawGameTypePage(mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JDOOM64__
#  define TITLEOFFSET_X         (67)
#else
#  define TITLEOFFSET_X         (60)
#endif

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);
    FR_DrawText3(GET_TXT(TXT_PICKGAMETYPE), x + TITLEOFFSET_X, y - 25, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);

#undef TITLEOFFSET_X
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
void Hu_MenuDrawPlayerClassPage(mn_page_t* page, int x, int y)
{
#define BG_X            (108)
#define BG_Y            (-58)

    assert(NULL != page);
    {
    mn_object_t* obj;
    int pClass;

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    FR_DrawText3("Choose class:", x - 32, y - 42, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

    obj = MNPage_FocusObject(page);
    assert(NULL != obj);
    pClass = obj->data2;
    if(pClass < 0)
    {   // Random class.
        // Number of user-selectable classes.
        pClass = (menuTime / 5);
    }

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    GL_DrawPatch(pPlayerClassBG[pClass % 3], x + BG_X, y + BG_Y);
    }

#undef BG_X
#undef BG_Y
}
#endif

#if __JDOOM__ || __JHERETIC__
void Hu_MenuDrawEpisodePage(mn_page_t* page, int x, int y)
{
#if __JHERETIC__
    mn_object_t* obj;
#endif

    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__
    /// \kludge Inform the user episode 6 is designed for deathmatch only.
    obj = MNPage_FocusObject(page);
    if(NULL != obj && obj->data2 == 5)
    {
        const char* str = notDesignedForMessage;
        composeNotDesignedForMessage(GET_TXT(TXT_SINGLEPLAYER));
        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(cfg.menuTextColors[1][CR], cfg.menuTextColors[1][CG], cfg.menuTextColors[1][CB], mnRendState->pageAlpha);
        FR_DrawText3(str, SCREENWIDTH/2, SCREENHEIGHT - 2, ALIGN_BOTTOM, MN_MergeMenuEffectWithDrawTextFlags(0));
    }
    // kludge end.
#else // __JDOOM__
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch3(pEpisode, Hu_ChoosePatchReplacement2(pEpisode, "{case}Which Episode{scaley=1.25,y=-3}?", true), x + 7, y - 25, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void Hu_MenuDrawSkillPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JDOOM__ || __JDOOM64__
    WI_DrawPatch3(pNewGame, Hu_ChoosePatchReplacement2(pNewGame, "{case}NEW GAME", true), x + 48, y - 49, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
    WI_DrawPatch3(pSkill, Hu_ChoosePatchReplacement2(pSkill, "{case}Choose Skill Level:", true), x + 6, y - 25, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#elif __JHEXEN__
    FR_DrawText3("Choose Skill Level:", x - 46, y - 28, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuUpdateGameSaveWidgets(void)
{
    if(!menuActive) return;

    // Prompt a refresh of the game-save info. We don't yet actively monitor
    // the contents of the game-save paths, so instead we settle for manual
    // updates whenever the save/load menu is opened.
    SV_UpdateGameSaveInfo();

    // Update widgets.
    { int i;
    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        /// \fixme Find object by id.
        mn_object_t* obj = &LoadMenu.objects[i];
        mndata_edit_t* edit = (mndata_edit_t*) obj->_typedata;
        const gamesaveinfo_t* info = SV_GetGameSaveInfoForSlot(edit->data2);
        const char* text = "";

        MNObject_SetFlags(obj, FO_SET, MNF_DISABLED);
        if(!Str_IsEmpty(&info->filePath))
        {
            text = Str_Text(&info->name);
            MNObject_SetFlags(obj, FO_CLEAR, MNF_DISABLED);
        }
        MNEdit_SetText(obj, MNEDIT_STF_NO_ACTION, text);
    }}
}

/**
 * Called after the save name has been modified and to action the game-save.
 */
int Hu_MenuSelectSaveSlot(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->_typedata;
    const int saveSlot = edit->data2;
    const char* saveName = MNEdit_Text(obj);

    if(MNA_ACTIVEOUT != action) return 1;

    if(menuNominatingQuickSaveSlot)
    {
        Con_SetInteger("game-save-quick-slot", saveSlot);
        menuNominatingQuickSaveSlot = false;
    }

    if(!G_SaveGame2(saveSlot, saveName))
        return 0;

    MNPage_SetFocus(&SaveMenu, MN_MustFindObjectOnPage(&SaveMenu, 0, obj->data2));
    MNPage_SetFocus(&LoadMenu, MN_MustFindObjectOnPage(&LoadMenu, 0, obj->data2));
    Hu_MenuCommand(chooseCloseMethod());
    return 0;
    }
}

int Hu_MenuCvarButton(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
    const cvarbutton_t* cb = obj->data1;
    cvartype_t varType = Con_GetVariableType(cb->cvarname);
    int value;

    if(MNA_MODIFIED != action) return 1;
 
    //strcpy(btn->text, cb->active? cb->yes : cb->no);
    btn->text = cb->active? cb->yes : cb->no;

    if(CVT_NULL == varType)
        return 0;

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
    return 0;
    }
}

int Hu_MenuCvarList(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    const mndata_list_t* list = (mndata_list_t*) obj->_typedata;
    const mndata_listitem_t* item;
    cvartype_t varType;
    int value;

    if(MNA_MODIFIED != action) return 1;

    if(MNList_Selection(obj) < 0)
        return 0; // Hmm?

    varType = Con_GetVariableType(list->data);
    if(CVT_NULL == varType)
        return 0;

    item = &((mndata_listitem_t*) list->items)[list->selection];
    if(list->mask)
    {
        value = Con_GetInteger(list->data);
        value = (value & ~list->mask) | (item->data & list->mask);
    }
    else
    {
        value = item->data;
    }

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
    return 0;
    }
}

int Hu_MenuSaveSlotEdit(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    if(MNA_ACTIVE != action) return 1;
    // Are we suggesting a new name?
    if(cfg.menuGameSaveSuggestName)
    {
        ddstring_t* suggestName = G_GenerateSaveGameName();
        MNEdit_SetText(obj, MNEDIT_STF_NO_ACTION, Str_Text(suggestName));
        Str_Free(suggestName);
    }
    return 0;
}

int Hu_MenuCvarEdit(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    const mndata_edit_t* edit = (mndata_edit_t*)obj->_typedata;
    cvartype_t varType = Con_GetVariableType(edit->data1);
    if(MNA_MODIFIED != action) return 1;
    if(CVT_CHARPTR != varType) return 0;
    Con_SetString2(edit->data1, MNEdit_Text(obj), SVF_WRITE_OVERRIDE);
    return 0;
    }
}

int Hu_MenuCvarSlider(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    const mndata_slider_t* sldr = obj->_typedata;
    cvartype_t varType = Con_GetVariableType(sldr->data1);
    float value = MNSlider_Value(obj);

    if(MNA_MODIFIED != action) return 1;

    if(CVT_NULL == varType)
        return 0;

    switch(varType)
    {
    case CVT_FLOAT:
        if(sldr->step >= .01f)
        {
            Con_SetFloat2(sldr->data1, (int) (100 * value) / 100.0f, SVF_WRITE_OVERRIDE);
        }
        else
        {
            Con_SetFloat2(sldr->data1, value, SVF_WRITE_OVERRIDE);
        }
        break;
    case CVT_INT:
        Con_SetInteger2(sldr->data1, (int) value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        Con_SetInteger2(sldr->data1, (byte) value, SVF_WRITE_OVERRIDE);
        break;
    default:
        break;
    }
    return 0;
    }
}

int Hu_MenuActivateColorWidget(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    mn_object_t* cboxMix, *sldrRed, *sldrGreen, *sldrBlue, *textAlpha, *sldrAlpha;

    if(action != MNA_ACTIVE) return 1;

    /// \fixme Find the objects by id.
    cboxMix   = &ColorWidgetMenu.objects[0];
    sldrRed   = &ColorWidgetMenu.objects[2];
    sldrGreen = &ColorWidgetMenu.objects[4];
    sldrBlue  = &ColorWidgetMenu.objects[6];
    textAlpha = &ColorWidgetMenu.objects[7];
    sldrAlpha = &ColorWidgetMenu.objects[8];

    colorWidgetActive = true;

    MNPage_Initialize(&ColorWidgetMenu);
    ColorWidgetMenu.data = obj;

    MNColorBox_CopyColor(cboxMix, 0, obj);
    MNSlider_SetValue(sldrRed,   MNSLIDER_SVF_NO_ACTION, MNColorBox_Redf(obj));
    MNSlider_SetValue(sldrGreen, MNSLIDER_SVF_NO_ACTION, MNColorBox_Greenf(obj));
    MNSlider_SetValue(sldrBlue,  MNSLIDER_SVF_NO_ACTION, MNColorBox_Bluef(obj));
    MNSlider_SetValue(sldrAlpha, MNSLIDER_SVF_NO_ACTION, MNColorBox_Alphaf(obj));

    MNObject_SetFlags(textAlpha, (MNColorBox_RGBAMode(obj)? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);
    MNObject_SetFlags(sldrAlpha, (MNColorBox_RGBAMode(obj)? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);

    return 0;
}

int Hu_MenuCvarColorBox(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    if(action != MNA_MODIFIED) return 1;
    // MNColorBox's current color has already been updated and we know
    // that at least one of the color components have changed.
    // So our job is to simply update the associated cvars.
    Con_SetFloat2(cbox->data1, MNColorBox_Redf(obj),   SVF_WRITE_OVERRIDE);
    Con_SetFloat2(cbox->data2, MNColorBox_Greenf(obj), SVF_WRITE_OVERRIDE);
    Con_SetFloat2(cbox->data3, MNColorBox_Bluef(obj),  SVF_WRITE_OVERRIDE);
    if(MNColorBox_RGBAMode(obj))
        Con_SetFloat2(cbox->data4, MNColorBox_Alphaf(obj), SVF_WRITE_OVERRIDE);
    return 0;
}

void Hu_MenuDrawLoadGamePage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JHERETIC__ || __JHEXEN__
    FR_DrawText3("Load Game", SCREENWIDTH/2, y-20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#else
    WI_DrawPatch3(pLoadGame, Hu_ChoosePatchReplacement2(pLoadGame, "{case}Load game", true), x - 8, y - 26, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawSaveGamePage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JHERETIC__ || __JHEXEN__
    FR_DrawText3("Save Game", SCREENWIDTH/2, y-20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#else
    WI_DrawPatch3(pSaveGame, Hu_ChoosePatchReplacement2(pSaveGame, "{case}Save game", true), x - 8, y - 26, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
int Hu_MenuSelectHelp(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;
    G_StartHelp();
    return 0;
}
#endif

void Hu_MenuDrawOptionsPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JHERETIC__ || __JHEXEN__
    FR_DrawText3("OPTIONS", x + 42, y - 38, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#else
    WI_DrawPatch3(pOptionsTitle, Hu_ChoosePatchReplacement2(pOptionsTitle, "{case}OPTIONS", true), x + 42, y - 20, ALIGN_TOP, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawSoundPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);
    FR_DrawText3("SOUND OPTIONS", SCREENWIDTH/2, y-20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawGameplayPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);
    FR_DrawText3("GAMEPLAY", SCREENWIDTH/2, y-20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawWeaponsPage(mn_page_t* page, int x, int y)
{
    int i = 0;
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);
    FR_DrawText3("WEAPONS", SCREENWIDTH/2, y-26, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

/*#if __JDOOM__ || __JDOOM64__
    Hu_MenuComposeSubpageString(page, 1024, buf);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorAndAlpha(cfg.menuTextColors[1][CR], cfg.menuTextColors[1][CG], cfg.menuTextColors[1][CB], mnRendState->pageAlpha);
    FR_DrawText3(buf, SCREENWIDTH/2, y - 12, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#elif __JHERETIC__
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    GL_DrawPatch(pInvPageLeft[!page->firstObject || (menuTime & 8)], x, y - 22);
    GL_DrawPatch(pInvPageRight[page->firstObject + page->numVisObjects >= page->objectsCount || (menuTime & 8)], 312 - x, y - 22);
#endif*/

    /// \kludge Inform the user how to change the order.
    /*{ mn_object_t* obj = MNPage_FocusObject(page);
    if(NULL != obj && obj == &page->objects[1])
    {
        const char* str = "Use left/right to move weapon up/down";
        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(cfg.menuTextColors[1][CR], cfg.menuTextColors[1][CG], cfg.menuTextColors[1][CB], mnRendState->pageAlpha);
        FR_DrawText3(str, SCREENWIDTH/2, SCREENHEIGHT/2 + (95/cfg.menuScale), ALIGN_BOTTOM, MN_MergeMenuEffectWithDrawTextFlags(0));
    }}*/
    // kludge end.

    DGL_Disable(DGL_TEXTURE_2D);
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawInventoryPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    FR_DrawText3("Inventory Options", SCREENWIDTH/2, y-28, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void Hu_MenuDrawHUDPage(mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);
    FR_DrawText3("HUD options", SCREENWIDTH/2, y - 20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

/*#if __JDOOM__ || __JDOOM64__
    Hu_MenuComposeSubpageString(page, 1024, buf);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorAndAlpha(1, .7f, .3f, mnRendState->pageAlpha);
    FR_DrawText3(buf, x + SCREENWIDTH/2, y + -12, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#else
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    GL_DrawPatch(pInvPageLeft[!page->firstObject || (menuTime & 8)], x, y + -22);
    GL_DrawPatch(pInvPageRight[page->firstObject + page->numVisObjects >= page->objectsCount || (menuTime & 8)], 312 - x, y + -22);
#endif*/

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawMultiplayerPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][0], cfg.menuTextColors[0][1], cfg.menuTextColors[0][2], mnRendState->pageAlpha);

    FR_DrawText3(GET_TXT(TXT_MULTIPLAYER), x + 60, y - 25, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawPlayerSetupPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][0], cfg.menuTextColors[0][1], cfg.menuTextColors[0][2], mnRendState->pageAlpha);

    FR_DrawText3(GET_TXT(TXT_PLAYERSETUP), x + 90, y - 25, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

int Hu_MenuActionSetActivePage(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MenuSetActivePage((mn_page_t*)obj->data1);
    return 0;
}

int Hu_MenuUpdateColorWidgetColor(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    float value = MNSlider_Value(obj);
    /// \fixme Find object by id.
    mn_object_t* cboxMix = &ColorWidgetMenu.objects[0];

    if(MNA_MODIFIED != action) return 1;

    switch(obj->data2)
    {
    case CR: MNColorBox_SetRedf(  cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CG: MNColorBox_SetGreenf(cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CB: MNColorBox_SetBluef( cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CA: MNColorBox_SetAlphaf(cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    default:
        Con_Error("Hu_MenuUpdateColorWidgetColor: Invalid value (%i) for data2.", obj->data2);
    }
    return 0;
    }
}

int Hu_MenuChangeWeaponPriority(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_MODIFIED != action) return 1;
    /*int         choice = option >> NUM_WEAPON_TYPES;
    int         temp;

    if(option & RIGHT_DIR)
    {
        if(choice < NUM_WEAPON_TYPES-1)
        {
            temp = cfg.weaponOrder[choice+1];
            cfg.weaponOrder[choice+1] = cfg.weaponOrder[choice];
            cfg.weaponOrder[choice] = temp;
        }
    }
    else
    {
        if(choice > 0)
        {
            temp = cfg.weaponOrder[choice];
            cfg.weaponOrder[choice] = cfg.weaponOrder[choice-1];
            cfg.weaponOrder[choice-1] = temp;
        }
    }*/
    return 0;
}

int Hu_MenuSelectSingleplayer(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;

    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NEWGAME, NULL, NULL);
        return 0;
    }

#if __JHEXEN__
    Hu_MenuSetActivePage(&PlayerClassMenu);
#elif __JHERETIC__
    Hu_MenuSetActivePage(&EpisodeMenu);
#elif __JDOOM64__
    Hu_MenuSetActivePage(&SkillMenu);
#else // __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        Hu_MenuSetActivePage(&SkillMenu);
    else
        Hu_MenuSetActivePage(&EpisodeMenu);
#endif
    return 0;
}

int Hu_MenuSelectMultiplayer(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    /// \fixme Find object by id.
    mn_object_t* labelObj = &MultiplayerMenu.objects[1];
    mndata_button_t* btn = (mndata_button_t*)labelObj->_typedata;
    if(MNA_ACTIVEOUT != action) return 1;
    // Set the appropriate label.
    if(IS_NETGAME)
    {
        btn->text = "Disconnect";
    }
    else
    {
        btn->text = "Join Game";
    }
    Hu_MenuSetActivePage(&MultiplayerMenu);
    return 0;
}

int Hu_MenuSelectJoinGame(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;
    if(IS_NETGAME)
    {
        DD_Execute(false, "net disconnect");
        Hu_MenuCommand(MCMD_CLOSE);
        return 0;
    }

    DD_Execute(false, "net setup client");
    return 0;
}

int Hu_MenuSelectPlayerSetup(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    mn_object_t* mop    = MN_MustFindObjectOnPage(&PlayerSetupMenu, 0, MNF_ID0);
    mn_object_t* name   = MN_MustFindObjectOnPage(&PlayerSetupMenu, 0, MNF_ID1);
    mn_object_t* color  = MN_MustFindObjectOnPage(&PlayerSetupMenu, 0, MNF_ID3);
#if __JHEXEN__
    mn_object_t* class_;
#endif

    if(MNA_ACTIVEOUT != action) return 1;

#if __JHEXEN__
    MNMobjPreview_SetMobjType(mop, PCLASS_INFO(cfg.netClass)->mobjType);
    MNMobjPreview_SetPlayerClass(mop, cfg.netClass);
#else
    MNMobjPreview_SetMobjType(mop, MT_PLAYER);
    MNMobjPreview_SetPlayerClass(mop, PCLASS_PLAYER);
#endif
    MNMobjPreview_SetTranslationClass(mop, 0);
    MNMobjPreview_SetTranslationMap(mop, cfg.netColor);

    MNList_SelectItemByValue(color, MNLIST_SIF_NO_ACTION, cfg.netColor);
#if __JHEXEN__
    class_ = MN_MustFindObjectOnPage(&PlayerSetupMenu, 0, MNF_ID2);
    MNList_SelectItemByValue(class_, MNLIST_SIF_NO_ACTION, cfg.netClass);
#endif

    MNEdit_SetText(name, MNEDIT_STF_NO_ACTION|MNEDIT_STF_REPLACEOLD, Con_GetString("net-name"));

    Hu_MenuSetActivePage(&PlayerSetupMenu);
    return 0;
}

#if __JHEXEN__
int Hu_MenuSelectPlayerSetupPlayerClass(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    int selection;
    if(MNA_MODIFIED != action) return 1;

    selection = MNList_Selection(obj);
    if(selection >= 0)
    {
        mn_object_t* mop = MN_MustFindObjectOnPage(&PlayerSetupMenu, 0, MNF_ID0);
        MNMobjPreview_SetPlayerClass(mop, selection);
        MNMobjPreview_SetMobjType(mop, PCLASS_INFO(selection)->mobjType);
    }
    return 0;
    }
}
#endif

int Hu_MenuSelectPlayerColor(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    int selection;
    if(MNA_MODIFIED != action) return 1;

    selection = MNList_Selection(obj);
    if(selection >= 0)
    {
        mn_object_t* mop = MN_MustFindObjectOnPage(&PlayerSetupMenu, 0, MNF_ID0);
        MNMobjPreview_SetTranslationMap(mop, selection);
    }
    return 0;
    }
}

int Hu_MenuSelectAcceptPlayerSetup(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    char buf[300];

    cfg.netColor = list_player_color.selection;
#if __JHEXEN__
    cfg.netClass = list_player_class.selection;
#endif

    if(MNA_ACTIVEOUT != action) return 1;

    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, edit_player_name.text, 300);
    DD_Execute(false, buf);

    if(IS_NETGAME)
    {
        strcpy(buf, "setname ");
        M_StrCatQuoted(buf, edit_player_name.text, 300);
        DD_Execute(false, buf);
#if __JHEXEN__
        // Must do 'setclass' first; the real class and color do not change
        // until the server sends us a notification -- this means if we do
        // 'setcolor' first, the 'setclass' after it will override the color
        // change (or such would appear to be the case).
        DD_Executef(false, "setclass %i", cfg.netClass);
#endif
        DD_Executef(false, "setcolor %i", cfg.netColor);
    }

    Hu_MenuSetActivePage(&MultiplayerMenu);
    return 0;
}

int Hu_MenuSelectQuitGame(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;
    G_QuitGame();
    return 0;
}

int Hu_MenuSelectEndGame(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;
    G_EndGame();
    return 0;
}

int Hu_MenuSelectLoadGame(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;
    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT && !Get(DD_PLAYBACK))
        {
            Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, NULL);
            return 0;
        }
    }

    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(&LoadMenu);
    return 0;
}

int Hu_MenuSelectSaveGame(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    player_t* player = &players[CONSOLEPLAYER];

    if(MNA_ACTIVEOUT != action) return 1;
    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT)
        {
#if __JDOOM__ || __JDOOM64__
            Hu_MsgStart(MSG_ANYKEY, SAVENET, NULL, NULL);
#endif
            return 0;
        }

        if(G_GetGameState() != GS_MAP)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, NULL, NULL);
            return 0;
        }

        if(player->playerState == PST_DEAD)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, NULL, NULL);
            return 0;
        }
    }

    Hu_MenuCommand(MCMD_OPEN);
    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(&SaveMenu);
    return 0;
}

#if __JHEXEN__
int Hu_MenuSelectPlayerClass(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    int option = obj->data2;
    mn_object_t* skillObj;
    const char* text;

    if(MNA_ACTIVEOUT != action) return 1;
    if(IS_NETGAME)
    {
        P_SetMessage(&players[CONSOLEPLAYER], "You can't start a new game from within a netgame!", false);
        return 0;
    }

    if(option < 0)
    {   // Random class.
        // Number of user-selectable classes.
        mnPlrClass = (menuTime / 5) % 3;
    }
    else
    {
        mnPlrClass = option;
    }

    skillObj = MN_MustFindObjectOnPage(&SkillMenu, 0, MNF_ID0);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_BABY]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(&SkillMenu, 0, MNF_ID1);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_EASY]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(&SkillMenu, 0, MNF_ID2);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_MEDIUM]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(&SkillMenu, 0, MNF_ID3);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_HARD]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(&SkillMenu, 0, MNF_ID4);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_NIGHTMARE]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    switch(mnPlrClass)
    {
    case PCLASS_FIGHTER:    SkillMenu.offset[VX] = 120; break;
    case PCLASS_CLERIC:     SkillMenu.offset[VX] = 116; break;
    case PCLASS_MAGE:       SkillMenu.offset[VX] = 112; break;
    }
    Hu_MenuSetActivePage(&SkillMenu);
    return 0;
}

int Hu_MenuFocusOnPlayerClass(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    {
    playerclass_t plrClass = (playerclass_t)obj->data2;
    mn_object_t* mop;

    if(MNA_FOCUS != action) return 1;

    mop = MN_MustFindObjectOnPage(&PlayerClassMenu, 0, MNF_ID0);
    MNMobjPreview_SetPlayerClass(mop, plrClass);
    MNMobjPreview_SetMobjType(mop, (PCLASS_NONE == plrClass? MT_NONE : PCLASS_INFO(plrClass)->mobjType));

    Hu_MenuDefaultFocusAction(obj, action, paramaters);
    return 0;
    }
}
#endif

#if __JDOOM__ || __JHERETIC__
int Hu_MenuFocusEpisode(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_FOCUS != action) return 1;
    mnEpisode = obj->data2;
    Hu_MenuDefaultFocusAction(obj, action, paramaters);
    return 0;
}

int Hu_MenuConfirmOrderCommericalVersion(msgresponse_t response, void* context)
{
    G_StartHelp();
    return true;
}

int Hu_MenuActivateNotSharewareEpisode(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MsgStart(MSG_ANYKEY, SWSTRING, Hu_MenuConfirmOrderCommericalVersion, NULL);
    return 0;
}
#endif

int Hu_MenuFocusSkillMode(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    assert(NULL != obj);
    if(MNA_FOCUS != action) return 1;
    mnSkillmode = (skillmode_t)obj->data2;
    Hu_MenuDefaultFocusAction(obj, action, paramaters);
    return 0;
}

#if __JDOOM__
int Hu_MenuConfirmInitNewGame(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        Hu_MenuInitNewGame(true);
    }
    return true;
}
#endif

void Hu_MenuInitNewGame(boolean confirmed)
{
#if __JDOOM__
    if(!confirmed && SM_NIGHTMARE == mnSkillmode)
    {
        Hu_MsgStart(MSG_YESNO, NIGHTMARE, Hu_MenuConfirmInitNewGame, NULL);
        return;
    }
#endif
    Hu_MenuCommand(chooseCloseMethod());
#if __JHEXEN__
    cfg.playerClass[CONSOLEPLAYER] = mnPlrClass;
    G_DeferredNewGame(mnSkillmode);
#else
    G_DeferedInitNew(mnSkillmode, mnEpisode, 0);
#endif
}

int Hu_MenuActionInitNewGame(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MenuInitNewGame(false);
    return 0;
}

int Hu_MenuSelectControlPanelLink(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
#define NUM_PANEL_NAMES         3

    static const char* panelNames[NUM_PANEL_NAMES] = {
        "panel",
        "panel audio",
        "panel input"
    };
    int idx = obj->data2;

    if(MNA_ACTIVEOUT != action) return 1;

    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
        idx = 0;

    Hu_MenuCommand(MCMD_CLOSEFAST);
    DD_Execute(true, panelNames[idx]);
    return 0;

#undef NUM_PANEL_NAMES
}

/**
 * Draws the automap options menu
 */
void Hu_MenuDrawAutomapPage(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][0], cfg.menuTextColors[0][1], cfg.menuTextColors[0][2], mnRendState->pageAlpha);
    FR_DrawText3("Automap OPTIONS", SCREENWIDTH/2, y-26, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

/*#if __JHERETIC__ || __JHEXEN__
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, mnRendState->page_alpha);
    GL_DrawPatch(pInvPageLeft[!page->firstObject || (menuTime & 8)], x, y - 22);
    GL_DrawPatch(pInvPageRight[page->firstObject + page->numVisObjects >= page->_size || (menuTime & 8)], 312 - x, y - 22);
#endif*/

    DGL_Disable(DGL_TEXTURE_2D);
}

D_CMD(MenuOpen)
{
    if(argc > 1)
    {
        mn_page_t* page = Hu_MenuFindPageForName(argv[1]);
        if(NULL != page)
        {
            Hu_MenuCommand(MCMD_OPEN);
            Hu_MenuSetActivePage(page);
            return true;
        }
        return false;
    }

    Hu_MenuCommand(!menuActive? MCMD_OPEN : MCMD_CLOSE);
    return true;
}

/**
 * Routes menu commands for actions and navigation into the menu.
 */
D_CMD(MenuCommand)
{
    if(menuActive)
    {
        const char* cmd = argv[0] + 4;
        if(!stricmp(cmd, "up"))
        {
            Hu_MenuCommand(MCMD_NAV_UP);
            return true;
        }
        if(!stricmp(cmd, "down"))
        {
            Hu_MenuCommand(MCMD_NAV_DOWN);
            return true;
        }
        if(!stricmp(cmd, "left"))
        {
            Hu_MenuCommand(MCMD_NAV_LEFT);
            return true;
        }
        if(!stricmp(cmd, "right"))
        {
            Hu_MenuCommand(MCMD_NAV_RIGHT);
            return true;
        }
        if(!stricmp(cmd, "back"))
        {
            Hu_MenuCommand(MCMD_NAV_OUT);
            return true;
        }
        if(!stricmp(cmd, "delete"))
        {
            Hu_MenuCommand(MCMD_DELETE);
            return true;
        }
        if(!stricmp(cmd, "select"))
        {
            Hu_MenuCommand(MCMD_SELECT);
            return true;
        }
        if(!stricmp(cmd, "pagedown"))
        {
            Hu_MenuCommand(MCMD_NAV_PAGEDOWN);
            return true;
        }
        if(!stricmp(cmd, "pageup"))
        {
            Hu_MenuCommand(MCMD_NAV_PAGEUP);
            return true;
        }
    }
    return false;
}
