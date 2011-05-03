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

void M_InitControlsMenu(void);
void M_ControlGrabDrawer(const char* niceName, float alpha);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void M_SetMenu(mn_object_t* obj);

void M_OpenLoadMenu(mn_object_t* obj);
void M_OpenSaveMenu(mn_object_t* obj);
#if __JHEXEN__
void M_OpenFilesMenu(mn_object_t* obj);
#endif
void M_OpenPlayerSetupMenu(mn_object_t* obj);
void M_OpenMultiplayerClientMenu(mn_object_t* obj);

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void M_OpenHelp(mn_object_t* obj);
#endif
void M_OpenControlPanel(mn_object_t* obj);

void M_SelectSingleplayer(mn_object_t* obj);
void M_SelectMultiplayer(mn_object_t* obj);
#if __JDOOM__ || __JHERETIC__
void M_SelectEpisode(mn_object_t* obj);
#endif
#if __JHEXEN__
void M_SelectPlayerClass(mn_object_t* obj);
#endif
void M_SelectSkillMode(mn_object_t* obj);
void M_SelectLoad(mn_object_t* obj);
void M_SelectQuitGame(mn_object_t* obj);
void M_SelectEndGame(mn_object_t* obj);
void M_AcceptPlayerSetup(mn_object_t* obj);

void M_SaveGame(mn_object_t* obj);

void M_WeaponOrder(mn_object_t* obj);
#if __JHEXEN__
void M_ChangePlayerClass(mn_object_t* obj);
#endif
void M_ChangePlayerColor(mn_object_t* obj);
void M_ChangePlayerName(mn_object_t* obj);

void M_DrawMainMenu(mn_page_t* page, int x, int y);
void M_DrawGameTypeMenu(mn_page_t* page, int x, int y);
void M_DrawSkillMenu(mn_page_t* page, int x, int y);
#if __JHEXEN__
void M_DrawPlayerClassMenu(mn_page_t* page, int x, int y);
#endif
#if __JDOOM__ || __JHERETIC__
void M_DrawEpisodeMenu(mn_page_t* page, int x, int y);
#endif
void M_DrawOptionsMenu(mn_page_t* page, int x, int y);
void M_DrawSoundMenu(mn_page_t* page, int x, int y);
void M_DrawGameplayMenu(mn_page_t* page, int x, int y);
void M_DrawHudMenu(mn_page_t* page, int x, int y);
#if __JHERETIC__ || __JHEXEN__
void M_DrawInventoryMenu(mn_page_t* page, int x, int y);
#endif
void M_DrawWeaponMenu(mn_page_t* page, int x, int y);
void M_DrawLoadMenu(mn_page_t* page, int x, int y);
void M_DrawSaveMenu(mn_page_t* page, int x, int y);
void M_DrawMultiplayerMenu(mn_page_t* page, int x, int y);
void M_DrawPlayerSetupMenu(mn_page_t* page, int x, int y);
void M_DrawColorWidgetMenu(mn_page_t* page, int x, int y);

int MN_ColorWidgetMenuCmdResponder(mn_page_t* page, menucommand_e cmd);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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

boolean menuNominatingQuickSaveSlot = false;

// Menu (page) render state.
static mn_rendstate_t rs;
const mn_rendstate_t* mnRendState = &rs;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mn_page_t* menuActivePage = NULL;
static boolean menuActive = false;

static float mnAlpha = 0; // Alpha level for the entire menu.
static float mnTargetAlpha = 0; // Target alpha for the entire UI.

#if __JDOOM__ || __JHERETIC__
static int epi;
#endif
#if __JHEXEN__
static int mnPlrClass;
#endif

static int frame = 0; // Used by any graphic animations that need to be pumped.
static int mnTime = 0;
static int flashCounter = 0;

static float cursorAngle = 0;
static int cursorAnimCounter = 0;
static int cursorAnimFrame = 0;

static boolean colorWidgetActive = false;
static float colorWidgetColor[4] = {0, 0, 0, 0};

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
#endif

#if __JHEXEN__
static patchid_t pPlayerClassBG[3];
static patchid_t pBullWithFire[8];
#endif

#if __JHERETIC__
static patchid_t pRotatingSkull[18];
#endif

static patchid_t pCursors[MENU_CURSOR_FRAMECOUNT];

static patchid_t pSliderLeft;
static patchid_t pSliderRight;
static patchid_t pSliderMiddle;
static patchid_t pSliderHandle;

#if __JDOOM__ || __JDOOM64__
static patchid_t pEditLeft;
static patchid_t pEditRight;
static patchid_t pEditMiddle;
#elif __JHERETIC__ || __JHEXEN__
static patchid_t pEditMiddle;
#endif

#if __JHERETIC__ || __JHEXEN__
# define READTHISID      3
#elif !__JDOOM64__
# define READTHISID      4
#endif

mn_object_t MainMenuObjects[] = {
#if __JDOOM__
    { MN_BUTTON,    0,  0,  "{case}New Game",  MENU_FONT2, MENU_COLOR1, &pNGame,    MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "{case}Options",   MENU_FONT2, MENU_COLOR1, &pOptions,  MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "{case}Load game", MENU_FONT2, MENU_COLOR1, &pLoadGame, MNButton_Dimensions, MNButton_Drawer, M_OpenLoadMenu, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "{case}Save game", MENU_FONT2, MENU_COLOR1, &pSaveGame, MNButton_Dimensions, MNButton_Drawer, M_OpenSaveMenu, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "{case}Read This!", MENU_FONT2, MENU_COLOR1,&pReadThis, MNButton_Dimensions, MNButton_Drawer, M_OpenHelp, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "{case}Quit Game", MENU_FONT2, MENU_COLOR1, &pQuitGame, MNButton_Dimensions, MNButton_Drawer, M_SelectQuitGame, MNButton_CommandResponder },
#elif __JDOOM64__
    { MN_BUTTON,    0,  0,  "{case}New Game",  MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "{case}Options",   MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "{case}Load Game", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenLoadMenu, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "{case}Save Game", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenSaveMenu, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "{case}Quit Game", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectQuitGame, MNButton_CommandResponder },
#else
    { MN_BUTTON,    0,  0,  "New Game", MENU_FONT2,   MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &GameTypeMenu },
    { MN_BUTTON,    0,  0,  "Options",  MENU_FONT2,   MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &OptionsMenu },
    { MN_BUTTON,    0,  0,  "Game Files", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &FilesMenu },
    { MN_BUTTON,    0,  0,  "Info",     MENU_FONT2,   MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenHelp, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "Quit Game", MENU_FONT2,  MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectQuitGame, MNButton_CommandResponder },
#endif
    { MN_NONE }
};

#if __JHEXEN__ || __JHERETIC__
mn_page_t MainMenu = {
    MainMenuObjects, 5, -1, 0,
    { 110, 56 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawMainMenu,
    //0, 0,
    //0, 5
};
#elif __JDOOM64__
mn_page_t MainMenu = {
    MainMenuObjects, 5, -1, 0,
    { 97, 64 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawMainMenu,
    //0, 0,
    //0, 5
};
#elif __JDOOM__
mn_page_t MainMenu = {
    MainMenuObjects, 6, -1, 0,
    { 97, 64 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawMainMenu,
    //0, 0,
    //0, 6
};
#endif

mn_object_t GameTypeMenuObjects[] = {
    { MN_BUTTON,    0,  0,  (const char*)TXT_SINGLEPLAYER,  MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSingleplayer, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  (const char*)TXT_MULTIPLAYER,   MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectMultiplayer, MNButton_CommandResponder },
    { MN_NONE }
};

mn_page_t GameTypeMenu = {
    GameTypeMenuObjects, 2, -1, 0,
#if __JDOOM__ || __JDOOM64__
    { 97, 65 },
#else
    { 104, 65 },
#endif
    { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawGameTypeMenu, NULL,
    &MainMenu,
    //0, 2
};

#if __JHEXEN__
static mn_object_t* PlayerClassMenuObjects;

mn_page_t PlayerClassMenu = {
    0, 0, -1, 0, { 66, 66 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawPlayerClassMenu, NULL,
    &GameTypeMenu,
    //0, 0
};
#endif

#if __JDOOM__ || __JHERETIC__
static mn_object_t* EpisodeMenuObjects;

mn_page_t EpisodeMenu = {
    0, 0, -1, 0,
# if __JDOOM__
    { 48, 63 },
# else
    { 80, 50 },
# endif
    { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawEpisodeMenu, NULL,
    &GameTypeMenu,
    //0, 0
};
#endif

#if __JHERETIC__ || __JHEXEN__
static mn_object_t FilesMenuObjects[] = {
    { MN_BUTTON,    0,  0,  "Load Game",    MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenLoadMenu, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "Save Game",    MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenSaveMenu, MNButton_CommandResponder },
    { MN_NONE }
};

mn_page_t FilesMenu = {
    FilesMenuObjects, 2, -1, 0,
    { 110, 60 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    NULL, NULL,
    &MainMenu,
    //0, 2
};
#endif

mndata_edit_t edit_saveslots[NUMSAVESLOTS] = {
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 0, M_SaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 1, M_SaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 2, M_SaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 3, M_SaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 4, M_SaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 5, M_SaveGame },
#if !__JHEXEN__
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 6, M_SaveGame },
    { "", "", 0, (const char*) TXT_EMPTYSTRING, NULL, 7, M_SaveGame }
#endif
};

static mn_object_t LoadMenuObjects[] = {
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[0] },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[1] },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[2] },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[3] },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[4] },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[5] },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[6] },
    { MN_EDIT,      0,  MNF_DISABLED|MNF_INACTIVE,  "",     MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_SelectLoad, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[7] },
#endif
    { MN_NONE }
};

mn_page_t LoadMenu = {
    LoadMenuObjects, NUMSAVESLOTS, -1, 0,
#if __JDOOM__ || __JDOOM64__
    { 80, 54 },
#else
    { 70, 30 },
#endif
    { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawLoadMenu, NULL,
    &MainMenu,
    //0, NUMSAVESLOTS
};

static mn_object_t SaveMenuObjects[] = {
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[0] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[1] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[2] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[3] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[4] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[5] },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[6] },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, Hu_MenuSaveSlotEdit, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_saveslots[7] },
#endif
    { MN_NONE }
};

mn_page_t SaveMenu = {
    SaveMenuObjects, NUMSAVESLOTS, -1, 0,
#if __JDOOM__ || __JDOOM64__
    { 80, 54 },
#else
    { 64, 10 },
#endif
    { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawSaveMenu, NULL,
    &MainMenu,
    //0, 1+NUMSAVESLOTS
};

#if __JHEXEN__
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  0,  "",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_BABY },
    { MN_BUTTON,    0,  0,  "",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_EASY },
    { MN_BUTTON,    0,  MNF_DEFAULT, "", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,  "",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_HARD },
    { MN_BUTTON,    0,  0,  "",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillMenu = {
    SkillMenuObjects, 5, -1, 0,
    { 120, 44 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawSkillMenu, NULL,
    &PlayerClassMenu,
    //0, 5
};
#elif __JHERETIC__
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  0,  "W",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_BABY },
    { MN_BUTTON,    0,  0,  "Y",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_EASY },
    { MN_BUTTON,    0,  MNF_DEFAULT, "B", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,  "S",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_HARD },
    { MN_BUTTON,    0,  0,  "P",          MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillMenu = {
    SkillMenuObjects, 5, -1, 0,
    { 38, 30 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawSkillMenu, NULL,
    &EpisodeMenu,
    //0, 5
};
#elif __JDOOM64__
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  0,  "I",          MENU_FONT2, MENU_COLOR1, &pSkillModeNames[0], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_BABY },
    { MN_BUTTON,    0,  0,  "H",          MENU_FONT2, MENU_COLOR1, &pSkillModeNames[1], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_EASY },
    { MN_BUTTON,    0,  MNF_DEFAULT, "H", MENU_FONT2, MENU_COLOR1, &pSkillModeNames[2], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,  "U",          MENU_FONT2, MENU_COLOR1, &pSkillModeNames[3], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_HARD },
    { MN_NONE }
};
static mn_page_t SkillMenu = {
    SkillMenuObjects, 4, -1, 0,
    { 48, 63 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawSkillMenu, NULL,
    &GameTypeMenu,
    //0, 4
};
#else
static mn_object_t SkillMenuObjects[] = {
    { MN_BUTTON,    0,  0,              "I",    MENU_FONT2, MENU_COLOR1, &pSkillModeNames[0], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_BABY },
    { MN_BUTTON,    0,  0,              "H",    MENU_FONT2, MENU_COLOR1, &pSkillModeNames[1], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_EASY },
    { MN_BUTTON,    0,  MNF_DEFAULT,    "H",    MENU_FONT2, MENU_COLOR1, &pSkillModeNames[2], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_MEDIUM },
    { MN_BUTTON,    0,  0,              "U",    MENU_FONT2, MENU_COLOR1, &pSkillModeNames[3], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_HARD },
    { MN_BUTTON,    0,  MNF_NO_ALTTEXT, "N",    MENU_FONT2, MENU_COLOR1, &pSkillModeNames[4], MNButton_Dimensions, MNButton_Drawer, M_SelectSkillMode, MNButton_CommandResponder, NULL, NULL, 0, SM_NIGHTMARE },
    { MN_NONE }
};

mn_page_t SkillMenu = {
    SkillMenuObjects, 5, -1, 0,
    { 48, 63 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawSkillMenu, NULL,
    &EpisodeMenu,
    //0, 5
};
#endif

static mn_object_t OptionsMenuObjects[] = {
    { MN_BUTTON,    0,  0,  "End Game", MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SelectEndGame, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "Control Panel", MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenControlPanel, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "Controls", MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &ControlsMenu },
    { MN_BUTTON,    0,  0,  "Gameplay", MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &GameplayMenu },
    { MN_BUTTON,    0,  0,  "HUD",      MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &HudMenu },
    { MN_BUTTON,    0,  0,  "Automap",  MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &AutomapMenu },
    { MN_BUTTON,    0,  0,  "Weapons",  MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &WeaponMenu },
#if __JHERETIC__ || __JHEXEN__
    { MN_BUTTON,    0,  0,  "Inventory", MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &InventoryMenu },
#endif
    { MN_BUTTON,    0,  0,  "Sound",    MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_SetMenu, MNButton_CommandResponder, NULL, NULL, &SoundMenu },
    { MN_BUTTON,    0,  0,  "Mouse",    MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenControlPanel, MNButton_CommandResponder, NULL, NULL, 0, 2 },
    { MN_BUTTON,    0,  0,  "Joystick", MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenControlPanel, MNButton_CommandResponder, NULL, NULL, 0, 2 },
    { MN_NONE }
};

mn_page_t OptionsMenu = {
    OptionsMenuObjects,
#if __JHERETIC__ || __JHEXEN__
    11,
#else
    10,
#endif
    -1, 0, { 110, 63 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawOptionsMenu, NULL,
    &MainMenu,
    //0,
#if __JHERETIC__ || __JHEXEN__
    //11
#else
    //10
#endif
};

mndata_slider_t sld_sound_volume = { 0, 255, 0, 5, false, "sound-volume" };
mndata_slider_t sld_music_volume = { 0, 255, 0, 5, false, "music-volume" };

mn_object_t SoundMenuObjects[] = {
    { MN_TEXT,      0,  0,  "SFX Volume",   MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",             MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_sound_volume },
    { MN_TEXT,      0,  0,  "Music Volume", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",             MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_music_volume },
    { MN_BUTTON,    0,  0,  "Open Audio Panel", MENU_FONT1, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenControlPanel, MNButton_CommandResponder, NULL, NULL, 0, 1 },
    { MN_NONE }
};

mn_page_t SoundMenu = {
    SoundMenuObjects, 5, -1, 0,
#if __JHEXEN__
    { 97, 25 },
#elif __JHERETIC__
    { 97, 30 },
#elif __JDOOM__ || __JDOOM64__
    { 97, 40 },
#endif
    { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawSoundMenu, NULL,
    &OptionsMenu,
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

mndata_colorbox_t cbox_hud_color = {
    &cfg.hudColor[0], &cfg.hudColor[1], &cfg.hudColor[2], &cfg.hudColor[3]
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
mndata_listinline_t list_hud_xhair_symbol = {
    listit_hud_xhair_symbols, NUMLISTITEMS(listit_hud_xhair_symbols), "view-cross-type"
};

mndata_colorbox_t cbox_hud_xhair_color = {
    &cfg.xhairColor[0], &cfg.xhairColor[1], &cfg.xhairColor[2], NULL
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

static mn_object_t HudMenuObjects[] = {
    { MN_TEXT,      0,  0,  "View Size", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",          MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_viewsize },
    { MN_TEXT,      0,  0,  "Wide Offset", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",          MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_wideoffset },
#if __JDOOM__
    { MN_TEXT,      0,  0,  "Single Key Display", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-keys-combine", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
    { MN_TEXT,      0,  0,  "AutoHide", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR3, 0, MNSlider_TextualValueDimensions, MNSlider_TextualValueDrawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_uptime },
    { MN_TEXT,      0,  0,  "UnHide Events", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Receive Damage", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer},
    { MN_BUTTON2,   0,  0,  "hud-unhide-damage", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Pickup Health", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-health", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Pickup Armor", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-armor", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Pickup Powerup", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-powerup", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Pickup Weapon", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-weapon", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  "Pickup Mana", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-ammo", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#else
    { MN_TEXT,      0,  0,  "Pickup Ammo", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-ammo", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
    { MN_TEXT,      0,  0,  "Pickup Key", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-key", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Pickup Item", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-unhide-pickup-invitem", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif

    { MN_TEXT,      0,  0,  "Messages", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Shown",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "msg-show", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Size",     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_messages_size },
    { MN_TEXT,      0,  0,  "Uptime",   MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR3, 0, MNSlider_TextualValueDimensions, MNSlider_TextualValueDrawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_messages_uptime },

    { MN_TEXT,      0,  0,  "Crosshair", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Symbol",   MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,  "",         MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, Hu_MenuCvarListInline, MNListInline_CommandResponder, NULL, NULL, &list_hud_xhair_symbol },
    { MN_TEXT,      0,  0,  "Size",     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_xhair_size },
    { MN_TEXT,      0,  0,  "Opacity",  MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",  MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_xhair_opacity },
    { MN_TEXT,      0,  0,  "Vitality Color", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "view-cross-vitality", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Color",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_COLORBOX,  0,  MNF_INACTIVE, "", MENU_FONT1, MENU_COLOR1, 0, MNColorBox_Dimensions, MNColorBox_Drawer, Hu_MenuActivateColorWidget, MNColorBox_CommandResponder, NULL, NULL, &cbox_hud_xhair_color },

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Statusbar", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Size",     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_statusbar_size },
    { MN_TEXT,      0,  0,  "Opacity",  MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_statusbar_opacity },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Counters", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Kills",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,  "",         MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, Hu_MenuCvarListInline, MNListInline_CommandResponder, NULL, NULL, &list_hud_killscounter },
    { MN_TEXT,      0,  0,  "Items",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,  "",         MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, Hu_MenuCvarListInline, MNListInline_CommandResponder, NULL, NULL, &list_hud_itemscounter },
    { MN_TEXT,      0,  0,  "Secrets",  MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,  "",         MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, Hu_MenuCvarListInline, MNListInline_CommandResponder, NULL, NULL, &list_hud_secretscounter },
    { MN_TEXT,      0,  0,  "Size",     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_counter_size },
#endif

    { MN_TEXT,      0,  0,  "Fullscreen HUD", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Size",     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_hud_size },
    { MN_TEXT,      0,  0,  "Text Color", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_COLORBOX,  0,  MNF_INACTIVE, "", MENU_FONT1, MENU_COLOR1, 0, MNColorBox_Dimensions, MNColorBox_Drawer, Hu_MenuActivateColorWidget, MNColorBox_CommandResponder, NULL, NULL, &cbox_hud_color },
#if __JHEXEN__
    { MN_TEXT,      0,  0,  "Show Mana", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-mana",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Show Ammo",  MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-ammo",   MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Show Armor", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-armor",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0,  "Show Power Keys", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-power",       MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
#if __JDOOM__
    { MN_TEXT,      0,  0,  "Show Face", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-face",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
    { MN_TEXT,      0,  0,  "Show Health", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-health",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      0,  0,  "Show Keys", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-keys",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      0,  0,  "Show Item",        MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-currentitem",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
    { MN_NONE }
};

mn_page_t HudMenu = {
#if __JHEXEN__
    HudMenuObjects, 57,
#elif __JHERETIC__
    HudMenuObjects, 69,
#elif __JDOOM64__
    HudMenuObjects, 65,
#elif __JDOOM__
    HudMenuObjects, 70,
#endif
    -1, 0,
#if __JDOOM__ || __JDOOM64__
    { 97, 40 },
#else
    { 97, 28 },
#endif
    { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawHudMenu, NULL,
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
mndata_button_t btn_inv_selectmode = { &cfg.inventorySelectMode, "Scroll", "Cursor" };
mndata_slider_t sld_inv_uptime = { 0, 30, 0, 1.f, true, "hud-inventory-timer", "Disabled", NULL, " second", " seconds" };
mndata_slider_t sld_inv_maxvisslots = { 0, 16, 0, 1, false, "hud-inventory-slot-max", "Automatic" };

static mn_object_t InventoryMenuObjects[] = {
    { MN_TEXT,      0,  0,  "Select Mode", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2EX, 0,  0,  "",            MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder, NULL, NULL, &btn_inv_selectmode },
    { MN_TEXT,      0,  0,  "Wrap Around",          MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-wrap",   MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Choose And Use",               MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-use-immediate",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Select Next If Use Failed", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "ctl-inventory-use-next",    MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "AutoHide", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR3, 0, MNSlider_TextualValueDimensions, MNSlider_TextualValueDrawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_inv_uptime },

    { MN_TEXT,      0,  0,  "Fullscreen HUD", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Max Visible Slots", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",                  MENU_FONT1, MENU_COLOR3, 0, MNSlider_TextualValueDimensions, MNSlider_TextualValueDrawer, Hu_MenuCvarSlider, MNSlider_CommandResponder, NULL, NULL, &sld_inv_maxvisslots },
    { MN_TEXT,      0,  0,  "Show Empty Slots",             MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-inventory-slot-showempty", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_NONE }
};

mn_page_t InventoryMenu = {
    InventoryMenuObjects, 15, -1, 0,
    { 78, 48 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawInventoryMenu, NULL,
    &OptionsMenu,
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

static mn_object_t WeaponMenuObjects[] = {
    { MN_TEXT,      0,  0,  "Priority Order", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LIST,      0,  MNF_INACTIVE,  "",    MENU_FONT1, MENU_COLOR3, 0, MNList_Dimensions, MNList_Drawer, M_WeaponOrder, MNList_CommandResponder, NULL, NULL, &list_weapons_order },
    { MN_TEXT,      0,  0,  "Cycling", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Use Priority Order",     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "player-weapon-nextmode", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Sequential",                     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "player-weapon-cycle-sequential", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },

    { MN_TEXT,      0,  0,  "Autoswitch", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
    { MN_TEXT,      0,  0,  "Pickup Weapon", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,  "",              MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, Hu_MenuCvarListInline, MNListInline_CommandResponder, NULL, NULL, &list_weapons_autoswitch_pickup },
    { MN_TEXT,      0,  0,  "   If Not Firing",             MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "player-autoswitch-notfiring",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Pickup Ammo", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,  "",            MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, Hu_MenuCvarListInline, MNListInline_CommandResponder, NULL, NULL, &list_weapons_autoswitch_pickupammo },
#if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Pickup Beserk",             MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "player-autoswitch-berserk", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
    { MN_NONE }
};

mn_page_t WeaponMenu = {
    WeaponMenuObjects,
#if __JDOOM__ || __JDOOM64__
    16,
#else
    14,
#endif
    -1, MNPF_NOHOTKEYS,
#if __JDOOM__ || __JDOOM64__
    { 78, 40 },
#elif __JHERETIC__
    { 78, 26 },
#elif __JHEXEN__
    { 78, 38 },
#endif
    { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawWeaponMenu, NULL,
    &OptionsMenu,
    //0, 12, { 12, 38 }
};

static mn_object_t GameplayMenuObjects[] = {
    { MN_TEXT,      0,  0, "Always Run", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0, "ctl-run",    MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0, "Use LookSpring",   MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0, "ctl-look-spring",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0, "Use AutoAim",      MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0, "ctl-aim-noauto",   MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0, "Allow Jumping", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0, "player-jump",   MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif
#if __JDOOM64__
    { MN_TEXT,      0,  0, "Weapon Recoil",         MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0, "player-weapon-recoil",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Compatibility", MENU_FONT1, MENU_COLOR2, 0, MNText_Dimensions, MNText_Drawer },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Any Boss Trigger 666", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-anybossdeath666", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#  if !__JDOOM64__
    { MN_TEXT,      0,  0,  "Av Resurrects Ghosts", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-raiseghosts",     MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
#  endif
    { MN_TEXT,      0,  0,  "PE Limited To 21 Lost Souls", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-maxskulls",              MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "LS Can Get Stuck Inside Walls", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-skullsinwalls",            MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
# endif
    { MN_TEXT,      0,  0,  "Monsters Can Get Stuck In Doors",  MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-monsters-stuckindoors",       MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Some Objects Never Hang Over Ledges",  MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-objects-neverhangoverledges",     MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Objects Fall Under Own Weight",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-objects-falloff",             MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Corpses Slide Down Stairs",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-corpse-sliding",          MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Use Exactly Doom's Clipping Code", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-objects-clipping",            MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "  ^If Not NorthOnly WallRunning",  MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-player-wallrun-northonly",    MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
# if __JDOOM__ || __JDOOM64__
    { MN_TEXT,      0,  0,  "Zombie Players Can Exit Maps", MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "game-zombiescanexit",          MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Fix Ouch Face",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-face-ouchfix", MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
    { MN_TEXT,      0,  0,  "Fix Weapon Slot Display",          MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_BUTTON2,   0,  0,  "hud-status-weaponslots-ownedfix",  MENU_FONT1, MENU_COLOR3, 0, MNButton_Dimensions, MNButton_Drawer, Hu_MenuCvarButton, MNButton_CommandResponder },
# endif
#endif
    { MN_NONE }
};

#if __JHEXEN__
mn_page_t GameplayMenu = {
    GameplayMenuObjects, 6, -1, 0,
    { 88, 25 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawGameplayMenu, NULL,
    &OptionsMenu,
    //0, 6, { 6, 25 }
};
#else
mn_page_t GameplayMenu = {
#if __JDOOM64__
    GameplayMenuObjects, 33,
#elif __JDOOM__
    GameplayMenuObjects, 35,
#else
    GameplayMenuObjects, 21,
#endif
    -1, 0, { 30, 40 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawGameplayMenu, NULL,
    &OptionsMenu,
#if __JDOOM64__
    //0, 16, { 16, 40 }
#elif __JDOOM__
    //0, 18, { 18, 40 }
#else
    //0, 11, { 11, 40 }
#endif
};
#endif

mn_object_t MultiplayerMenuObjects[] = {
    { MN_BUTTON,    0,  0,  "Player Setup", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenPlayerSetupMenu, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "Join Game",    MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenMultiplayerClientMenu, MNButton_CommandResponder },
    { MN_NONE }
};

mn_object_t MultiplayerClientMenuObjects[] = {
    { MN_BUTTON,    0,  0,  "Player Setup", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenPlayerSetupMenu, MNButton_CommandResponder },
    { MN_BUTTON,    0,  0,  "Disconnect",   MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_OpenMultiplayerClientMenu, MNButton_CommandResponder },
    { MN_NONE}
};

mn_page_t MultiplayerMenu = {
    MultiplayerMenuObjects, 2, -1, 0,
    { 97, 65 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawMultiplayerMenu, NULL,
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

mn_object_t PlayerSetupMenuObjects[] = {
    { MN_MOBJPREVIEW, 0,0,              "",         0, 0, 0, MNMobjPreview_Dimensions, MNMobjPreview_Drawer, NULL, NULL, NULL, NULL, &mop_player_preview },
    { MN_EDIT,      0,  MNF_INACTIVE,   "",         MENU_FONT1, MENU_COLOR1, 0, MNEdit_Dimensions, MNEdit_Drawer, M_ChangePlayerName, MNEdit_CommandResponder, MNEdit_Responder, NULL, &edit_player_name },
#if __JHEXEN__
    { MN_TEXT,      0,  0,              "Class",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,              "",         MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, M_ChangePlayerClass, MNListInline_CommandResponder, NULL, NULL, &list_player_class },
#endif
    { MN_TEXT,      0,  0,              "Color",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_LISTINLINE, 0, 0,              "",         MENU_FONT1, MENU_COLOR3, 0, MNListInline_Dimensions, MNListInline_Drawer, M_ChangePlayerColor, MNListInline_CommandResponder, NULL, NULL, &list_player_color },
    { MN_BUTTON,    0,  0,              "Accept Changes", MENU_FONT2, MENU_COLOR1, 0, MNButton_Dimensions, MNButton_Drawer, M_AcceptPlayerSetup, MNButton_CommandResponder },
    { MN_NONE }
};

mn_page_t PlayerSetupMenu = {
#if __JHEXEN__
    PlayerSetupMenuObjects, 7,
#else
    PlayerSetupMenuObjects, 5,
#endif
    -1, 0, { 70, 54 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawPlayerSetupMenu, NULL,
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

static mn_object_t ColorWidgetMenuObjects[] = {
    { MN_TEXT,      0,  0,  "Red",      MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuUpdateColorWidgetColor, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_red, 0 },
    { MN_TEXT,      0,  0,  "Green",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuUpdateColorWidgetColor, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_green, 1 },
    { MN_TEXT,      0,  0,  "Blue",     MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuUpdateColorWidgetColor, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_blue, 2 },
    { MN_TEXT,      0,  0,  "Alpha",    MENU_FONT1, MENU_COLOR1, 0, MNText_Dimensions, MNText_Drawer },
    { MN_SLIDER,    0,  0,  "",         MENU_FONT1, MENU_COLOR1, 0, MNSlider_Dimensions, MNSlider_Drawer, Hu_MenuUpdateColorWidgetColor, MNSlider_CommandResponder, NULL, NULL, &sld_colorwidget_alpha, 3 },
    { MN_NONE }
};

static mn_page_t ColorWidgetMenu = {
    ColorWidgetMenuObjects, 8, -1, MNPF_NOHOTKEYS,
    { 98, 60 }, { GF_FONTA, GF_FONTB }, { 0, 1, 2 },
    M_DrawColorWidgetMenu, MN_ColorWidgetMenuCmdResponder,
    &OptionsMenu,
    //0, 8
};

// Cvars for the menu:
cvartemplate_t menuCVars[] = {
    { "menu-scale",     0,  CVT_FLOAT,  &cfg.menuScale, .1f, 1 },
    { "menu-nostretch", 0,  CVT_BYTE,   &cfg.menuNoStretch, 0, 1 },
    { "menu-flash-r",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CR], 0, 1 },
    { "menu-flash-g",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CG], 0, 1 },
    { "menu-flash-b",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CB], 0, 1 },
    { "menu-flash-speed", 0, CVT_INT,   &cfg.menuTextFlashSpeed, 0, 50 },
    { "menu-turningskull", 0, CVT_BYTE, &cfg.menuCursorRotate, 0, 1 },
    { "menu-effect",    0,  CVT_INT,    &cfg.menuEffects, 0, 1 },
    { "menu-color-r",   0,  CVT_FLOAT,  &cfg.menuTextColors[0][CR], 0, 1 },
    { "menu-color-g",   0,  CVT_FLOAT,  &cfg.menuTextColors[0][CG], 0, 1 },
    { "menu-color-b",   0,  CVT_FLOAT,  &cfg.menuTextColors[0][CB], 0, 1 },
    { "menu-colorb-r",  0,  CVT_FLOAT,  &cfg.menuTextColors[1][CR], 0, 1 },
    { "menu-colorb-g",  0,  CVT_FLOAT,  &cfg.menuTextColors[1][CG], 0, 1 },
    { "menu-colorb-b",  0,  CVT_FLOAT,  &cfg.menuTextColors[1][CB], 0, 1 },
    { "menu-colorc-r",  0,  CVT_FLOAT,  &cfg.menuTextColors[2][CR], 0, 1 },
    { "menu-colorc-g",  0,  CVT_FLOAT,  &cfg.menuTextColors[2][CG], 0, 1 },
    { "menu-colorc-b",  0,  CVT_FLOAT,  &cfg.menuTextColors[2][CB], 0, 1 },
    { "menu-glitter",   0,  CVT_FLOAT,  &cfg.menuTextGlitter, 0, 1 },
    { "menu-fog",       0,  CVT_INT,    &cfg.hudFog, 0, 5 },
    { "menu-shadow",    0,  CVT_FLOAT,  &cfg.menuShadow, 0, 1 },
    { "menu-patch-replacement", 0, CVT_BYTE, &cfg.usePatchReplacement, 0, 2 },
    { "menu-slam",      0,  CVT_BYTE,   &cfg.menuSlam,  0, 1 },
    { "menu-quick-ask", 0,  CVT_BYTE,   &cfg.confirmQuickGameSave, 0, 1 },
    { "menu-hotkeys",   0,  CVT_BYTE,   &cfg.menuHotkeys, 0, 1 },
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

#if __JDOOM__ || __JDOOM64__
    pSliderLeft   = R_PrecachePatch("M_THERML", NULL);
    pSliderRight  = R_PrecachePatch("M_THERMR", NULL);
    pSliderMiddle = R_PrecachePatch("M_THERM2", NULL);
    pSliderHandle = R_PrecachePatch("M_THERMO", NULL);
#elif __JHERETIC__ || __JHEXEN__
    pSliderLeft   = R_PrecachePatch("M_SLDLT", NULL);
    pSliderRight  = R_PrecachePatch("M_SLDRT", NULL);
    pSliderMiddle = R_PrecachePatch("M_SLDMD1", NULL);
    pSliderHandle = R_PrecachePatch("M_SLDKB", NULL);
#endif

#if __JDOOM__ || __JDOOM64__
    pEditLeft   = R_PrecachePatch("M_LSLEFT", NULL);
    pEditRight  = R_PrecachePatch("M_LSRGHT", NULL);
    pEditMiddle = R_PrecachePatch("M_LSCNTR", NULL);
#elif __JHERETIC__ || __JHEXEN__
    pEditMiddle = R_PrecachePatch("M_FSLOT", NULL);
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
        item->text = name;
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
    EpisodeMenuObjects = Z_Calloc(sizeof(mn_object_t) * (numEpisodes+1), PU_GAMESTATIC, 0);

    for(i = 0, maxw = 0; i < numEpisodes; ++i)
    {
        mn_object_t* obj = &EpisodeMenuObjects[i];

        obj->type = MN_BUTTON;
        obj->drawer = MNButton_Drawer;
        obj->cmdResponder = MNButton_CommandResponder;
        obj->dimensions = MNButton_Dimensions;
        obj->action = M_SelectEpisode;
        obj->data2 = i;
        obj->text = GET_TXT(TXT_EPISODE1 + i);
        obj->pageFontIdx = MENU_FONT2;
        FR_SetFont(FID(MNPage_PredefinedFont(&EpisodeMenu, obj->pageFontIdx)));
        w = FR_TextFragmentWidth(obj->text);
        if(w > maxw)
            maxw = w;
# if __JDOOM__
        obj->patch = &pEpisodeNames[i];
# endif
    }
    EpisodeMenuObjects[i].type = MN_NONE;

    // Finalize setup.
    EpisodeMenu.objects = EpisodeMenuObjects;
    EpisodeMenu.objectsCount = numEpisodes;
    //EpisodeMenu.numVisObjects = MIN_OF(EpisodeMenu.objectsCount, 10);
    EpisodeMenu.offset[VX] = SCREENWIDTH/2 - maxw / 2 + 18; // Center the menu appropriately.
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
    PlayerClassMenuObjects = Z_Calloc(sizeof(mn_object_t) * (count + 1), PU_GAMESTATIC, 0);

    // Add the selectable classes.
    n = i = 0;
    while(n < count)
    {
        classinfo_t* info = PCLASS_INFO(i++);
        mn_object_t* obj;

        if(!info->userSelectable)
            continue;

        obj = &PlayerClassMenuObjects[n];
        obj->type = MN_BUTTON;
        obj->drawer = MNButton_Drawer;
        obj->cmdResponder = MNButton_CommandResponder;
        obj->dimensions = MNButton_Dimensions;
        obj->action = M_SelectPlayerClass;
        obj->data2 = n;
        obj->text = info->niceName;
        obj->pageFontIdx = MENU_FONT2;

        ++n;
    }

    // Add the random class option.
    PlayerClassMenuObjects[n].type = MN_BUTTON;
    PlayerClassMenuObjects[n].drawer = MNButton_Drawer;
    PlayerClassMenuObjects[n].cmdResponder = MNButton_CommandResponder;
    PlayerClassMenuObjects[n].dimensions = MNButton_Dimensions;
    PlayerClassMenuObjects[n].action = M_SelectPlayerClass;
    PlayerClassMenuObjects[n].data2 = -1;
    PlayerClassMenuObjects[n].text = GET_TXT(TXT_RANDOMPLAYERCLASS);
    PlayerClassMenuObjects[n].pageFontIdx = MENU_FONT2;

    // Finalize setup.
    PlayerClassMenu.objects = PlayerClassMenuObjects;
    PlayerClassMenu.objectsCount = count + 1;
    //PlayerClassMenu.numVisObjects = MIN_OF(PlayerClassMenu.objectsCount, 10);
}
#endif

void Hu_MenuInit(void)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    // Skill name init and auto-centering.
    /// \fixme Do this (optionally) during page initialization.
    { int i, w, maxw;
    for(i = 0, maxw = 0; i < NUM_SKILL_MODES; ++i)
    {
        SkillMenuObjects[i].text = GET_TXT(TXT_SKILL1 + i);
        FR_SetFont(FID(MNPage_PredefinedFont(&SkillMenu, SkillMenuObjects[i].pageFontIdx)));
        w = FR_TextFragmentWidth(SkillMenuObjects[i].text);
        if(w > maxw)
            maxw = w;
    }
    SkillMenu.offset[VX] = SCREENWIDTH/2 - maxw / 2 + 14;
    }
#endif

    mnAlpha = mnTargetAlpha = 0;
    menuActivePage = NULL;
    menuActive = false;
    cursorAnimFrame = 0;
    cursorAnimCounter = MENU_CURSOR_TICSPERFRAME;

    DD_Execute(true, "deactivatebcontext menu");

    Hu_MenuLoadResources();

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        mn_object_t* obj = &MainMenuObjects[4]; // Read This!
        obj->action = M_SelectQuitGame;
        obj->text = "{case}Quit Game";
        obj->patch = &pQuitGame;
        MainMenu.objectsCount = 5;
        MainMenu.offset[VY] += 8;
    }
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        SkillMenu.previous = &GameTypeMenu;
    }
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
        mn_page_t* page = Hu_MenuActivePage();
        mn_object_t* focusObj = MNPage_FocusObject(page);

        flashCounter += (int)(cfg.menuTextFlashSpeed * ticLength * TICRATE);
        if(flashCounter >= 100)
            flashCounter -= 100;

        if(cfg.menuCursorRotate)
        {
            if(NULL != focusObj && !(focusObj->flags & (MNF_DISABLED|MNF_INACTIVE)) &&
               (focusObj->type == MN_LISTINLINE || focusObj->type == MN_SLIDER))
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
        mnTime++;

        // Animate the cursor patches.
        if(--cursorAnimCounter <= 0)
        {
            cursorAnimFrame++;
            cursorAnimCounter = MENU_CURSOR_TICSPERFRAME;
            if(cursorAnimFrame > MENU_CURSOR_FRAMECOUNT-1)
                cursorAnimFrame = 0;
        }

        // Used for Heretic's rotating skulls.
        frame = (mnTime / 3) % 18;
    }
}

mn_object_t* MNPage_FocusObject(mn_page_t* page)
{
    assert(NULL != page);
    return &page->objects[page->focus];
}

void MNPage_ComposeSubpageString(mn_page_t* page, size_t bufSize, char* buf)
{
    assert(NULL != page);
    if(NULL == buf || 0 == bufSize) return;
/*    snprintf(buf, bufSize, "PAGE %i/%i", (page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1,
            (int)ceil((float)page->objectsCount/page->numVisObjects));*/
}

static void calcNumVisObjects(mn_page_t* page)
{
/*    page->firstObject = MAX_OF(0, page->focus - page->numVisObjects/2);
    page->firstObject = MIN_OF(page->firstObject, page->objectsCount - page->numVisObjects);
    page->firstObject = MAX_OF(0, page->firstObject);*/
}

void MNPage_Initialize(mn_page_t* page)
{
    assert(NULL != page);
    // Init objects.
    { uint i; mn_object_t* obj;
    for(i = 0, obj = page->objects; i < page->objectsCount; ++i, obj++)
    {
        // Calculate real coordinates.
        /*obj->x = UI_ScreenX(obj->relx);
        obj->w = UI_ScreenW(obj->relw);
        obj->y = UI_ScreenY(obj->rely);
        obj->h = UI_ScreenH(obj->relh);*/

        switch(obj->type)
        {
        case MN_TEXT:
        case MN_MOBJPREVIEW:
            obj->flags |= MNF_NO_FOCUS;
            break;
        case MN_BUTTON:
        case MN_BUTTON2:
        case MN_BUTTON2EX:
            if(NULL != obj->text && ((unsigned int)obj->text < NUMTEXT))
            {
                obj->text = GET_TXT((unsigned int)obj->text);
            }

            if(obj->type == MN_BUTTON2)
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
            break;

        case MN_EDIT: {
            mndata_edit_t* edit = (mndata_edit_t*) obj->data;
            if(NULL != edit->emptyString && ((unsigned int)edit->emptyString < NUMTEXT))
            {
                edit->emptyString = GET_TXT((unsigned int)edit->emptyString);
            }
            // Update text.
            //memset(obj->text, 0, sizeof(obj->text));
            //strncpy(obj->text, edit->ptr, 255);
            break;
          }
        case MN_LIST:
        case MN_LISTINLINE: {
            mndata_list_t* list = obj->data;
            int i;
            for(i = 0; i < list->count; ++i)
            {
                mndata_listitem_t* item = &((mndata_listitem_t*)list->items)[i];
                if(NULL != item->text && ((unsigned int)item->text < NUMTEXT))
                {
                    item->text = GET_TXT((unsigned int)item->text);
                }
            }

            // Determine number of visible items.
            //list->numvis = (obj->h - 2 * UI_BORDER) / listItemHeight(list);
            if(list->selection >= 0)
            {
                if(list->selection < list->first)
                    list->first = list->selection;
                //if(list->selection > list->first + list->numvis - 1)
                //    list->first = list->selection - list->numvis + 1;
            }
            //UI_InitColumns(obj);
            break;
          }
        default:
            break;
        }
    }}

    // If we haven't yet visited this page and no initial focus object is
    // specified then find the first focusable object and select it.
    if(0 > page->focus)
    {
        uint i;
        // First look for a default focus object. There should only be one
        // but find the last with this flag...
        for(i = 0; i < page->objectsCount; ++i)
        {
            mn_object_t* obj = &page->objects[i];
            obj->flags &= ~MNF_FOCUS;
            if((obj->flags & MNF_DEFAULT) && !(obj->flags & (MNF_DISABLED|MNF_NO_FOCUS)))
                page->focus = i;
        }

        // No default focus? Find the first focusable object.
        if(0 > page->focus)
        for(i = 0; i < page->objectsCount; ++i)
        {
            mn_object_t* obj = &page->objects[i];
            if(!(obj->flags & (MNF_DISABLED | MNF_NO_FOCUS)))
            {
                obj->flags |= MNF_FOCUS;
                page->focus = i;
                break;
            }
        }

        if(0 > page->focus)
        {
#if _DEBUG
            Con_Message("Warning:MNPage::Initialize: No focusable object on page.");
#endif
            page->focus = 0; // Keep this within valid range!
        }
    }
    calcNumVisObjects(page);
}

mn_page_t* Hu_MenuActivePage(void)
{
    return menuActivePage;
}

void Hu_MenuSetActivePage(mn_page_t* page)
{
    if(!menuActive)
        return;
    if(!page || menuActivePage == page)
        return;

    MNPage_Initialize(page);

    FR_ResetTypeInTimer();
    flashCounter = 0;
    cursorAngle = 0;

    menuNominatingQuickSaveSlot = false;

    // This is now the active page.
    menuActivePage = page;
}

static void drawFocusCursor(int x, int y, int cursorIdx, int focusObjectHeight,
    float angle, float alpha)
{
#if __JDOOM__ || __JDOOM64__
# define OFFSET_X         (-22)
# define OFFSET_Y         (-2)
#elif __JHERETIC__ || __JHEXEN__
# define OFFSET_X         (-16)
# define OFFSET_Y         (3)
#endif

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

    GL_DrawPatch2(pCursor, 0, 0, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef OFFSET_Y
#undef OFFSET_X
}

void MN_DrawPage(mn_page_t* page, float alpha, boolean showFocusCursor)
{
    assert(NULL != page);
    {
    int pos[2] = { 0, 0 };
    uint i;

    if(!(alpha > .0001f)) return;

    // Configure default render state:
    rs.pageAlpha = alpha;
    rs.textGlitter = cfg.menuTextGlitter;
    rs.textShadow = cfg.menuShadow;
    for(i = 0; i < MENU_FONT_COUNT; ++i)
    {
        rs.textFonts[i] = MNPage_PredefinedFont(page, i);
    }
    for(i = 0; i < MENU_COLOR_COUNT; ++i)
    {
        MNPage_PredefinedColor(page, i, rs.textColors[i]);
        rs.textColors[i][CA] = alpha; // For convenience.
    }

    /*if(page->unscaled.numVisObjects)
    {
        page->numVisObjects = page->unscaled.numVisObjects / cfg.menuScale;
        page->offset[VY] = (SCREENHEIGHT/2) - ((SCREENHEIGHT/2) - page->unscaled.y) / cfg.menuScale;
    }*/

    if(NULL != page->drawer)
    {
        page->drawer(page, page->offset[VX], page->offset[VY]);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(page->offset[VX], page->offset[VY], 0);

    for(i = 0/*page->firstObject*/;
        i < page->objectsCount /*&& i < page->firstObject + page->numVisObjects*/; ++i)
    {
        mn_object_t* obj = &page->objects[i];
        int height = 0;

        if(obj->type == MN_NONE || !obj->drawer || (obj->flags & MNF_HIDDEN))
            continue;

        obj->drawer(obj, pos[VX], pos[VY]);

        if(obj->dimensions)
        {
            obj->dimensions(obj, page, NULL, &height);
        }

        if(showFocusCursor && (obj->flags & MNF_FOCUS))
        {
            drawFocusCursor(pos[VX], pos[VY], cursorAnimFrame, height, cursorAngle, alpha);
        }

        pos[VY] += height * (1.08f); // Leading.
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-page->offset[VX], -page->offset[VY], 0);
    }
}

boolean Hu_MenuIsVisible(void)
{
    return (menuActive || mnAlpha > .0001f);
}

static void beginOverlayDraw(float darken)
{
#define SMALL_SCALE             .75f

    DGL_SetNoMaterial();
    DGL_DrawRect(0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0, darken);

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

    boolean showFocusCursor = true;
    mn_object_t* focusObj;

    if(!Hu_MenuIsVisible()) return;

    // First determine whether the focus cursor should be visible.
    focusObj = MNPage_FocusObject(Hu_MenuActivePage());
    if(NULL != focusObj && !(focusObj->flags & MNF_INACTIVE))
    {
        if(focusObj->type == MN_COLORBOX || focusObj->type == MN_BINDINGS)
        {
            showFocusCursor = false;
        }
    }

    // Draw the active menu page.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_LoadIdentity();

    DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
    DGL_Scalef(cfg.menuScale, cfg.menuScale, 1);
    DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);

    MN_DrawPage(Hu_MenuActivePage(), mnAlpha, showFocusCursor);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    // Drawing any overlays?
    if(NULL != focusObj && !(focusObj->flags & MNF_INACTIVE))
    {
        if(focusObj->type == MN_COLORBOX)
        {
            // Draw the color widget.
            beginOverlayDraw(OVERLAY_DARKEN);
            MN_DrawPage(&ColorWidgetMenu, 1, true);
            endOverlayDraw();
        }
        else if(focusObj->type == MN_BINDINGS)
        {
            // Draw the control-grab visual.
            mndata_bindings_t* binds = (mndata_bindings_t*) focusObj->data;

            beginOverlayDraw(OVERLAY_DARKEN);
            M_ControlGrabDrawer(binds->text, 1);
            endOverlayDraw();
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
        uint i;
        for(i = 0; i < page->objectsCount; ++i)
        {
            page->objects[i].flags &= ~MNF_FOCUS;
        }
        page->objects[index].flags |= MNF_FOCUS;
        page->focus = index;
        calcNumVisObjects(page);
        S_LocalSound(SFX_MENU_NAV_RIGHT, NULL);
    }
    }
#endif
}

/// \note This is only done the first time this object collection is processed.
static void updateObjectsLinkedWithCvars(mn_object_t* objs)
{
    mn_object_t* obj;
    for(obj = objs; obj->type; obj++)
    {
        switch(obj->type)
        {
        case MN_BUTTON:
        case MN_BUTTON2:
        case MN_BUTTON2EX:
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
            break;

        case MN_LIST:
        case MN_LISTINLINE: {
            mndata_list_t* list = obj->data;
            if(obj->action == Hu_MenuCvarListInline)
            {
                // Choose the correct list item based on the value of the cvar.
                list->selection = MNList_FindItem(obj, Con_GetInteger(list->data));
            }
            break;
          }
        case MN_EDIT: {
            mndata_edit_t* edit = obj->data;
            if(obj->action == Hu_MenuCvarEdit)
            {
                MNEdit_SetText(obj, Con_GetString(edit->data1));
                //strncpy(edit->ptr, Con_GetString(edit->data), edit->maxLen);
            }
            break;
          }
        case MN_SLIDER: {
            mndata_slider_t* sldr = obj->data;
            if(obj->action == Hu_MenuCvarSlider)
            {
                if(sldr->floatMode)
                    sldr->value = Con_GetFloat(sldr->data1);
                else
                    sldr->value = Con_GetInteger(sldr->data1);
            }
            break;
          }
        default:
            break;
        }
    }
}

static void initObjects(mn_object_t* objs)
{
    updateObjectsLinkedWithCvars(objs);
}

static void initAllObjectsOnAllPages(void)
{
#if __JHEXEN__
    initObjects(PlayerClassMenuObjects);
#endif
#if __JDOOM__ || __JHERETIC__
    initObjects(EpisodeMenuObjects);
#endif
#if __JHERETIC__ || __JHEXEN__
    initObjects(FilesMenuObjects);
#endif
    initObjects(LoadMenuObjects);
    initObjects(SaveMenuObjects);
    initObjects(SkillMenuObjects);
    initObjects(OptionsMenuObjects);
    initObjects(SoundMenuObjects);
    initObjects(SoundMenuObjects);
    initObjects(HudMenuObjects);
#if __JHERETIC__ || __JHEXEN__
    initObjects(InventoryMenuObjects);
#endif
    initObjects(WeaponMenuObjects);
    initObjects(GameplayMenuObjects);
    initObjects(AutomapMenuObjects);
    initObjects(MultiplayerMenuObjects);
    initObjects(MultiplayerClientMenuObjects);
    initObjects(PlayerSetupMenuObjects);

    initObjects(ColorWidgetMenuObjects);

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

int MN_ColorWidgetMenuCmdResponder(mn_page_t* page, menucommand_e cmd)
{
    assert(NULL != page);
    switch(cmd)
    {
    case MCMD_NAV_OUT: {
        mn_object_t* cboxObj = (mn_object_t*)page->data;
        cboxObj->flags |= MNF_INACTIVE;
        S_LocalSound(SFX_MENU_CANCEL, NULL);
        colorWidgetActive = false;
        return true;
      }
    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        return true; // Eat these.
    case MCMD_SELECT: {
        mn_object_t* cboxObj = (mn_object_t*)page->data;
        mndata_colorbox_t* cbox = (mndata_colorbox_t*)cboxObj->data;

        // Set the new color
        *cbox->r = colorWidgetColor[CR];
        *cbox->g = colorWidgetColor[CG];
        *cbox->b = colorWidgetColor[CB];
        if(NULL != cbox->a)
            *cbox->a = colorWidgetColor[CA];

        cboxObj->flags |= MNF_INACTIVE;
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        colorWidgetActive = false;
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
        uint i = 0;
        int hasFocus = page->focus;
        do
        {
            hasFocus += (cmd == MCMD_NAV_UP? -1 : 1);
            if(hasFocus < 0)
                hasFocus = page->objectsCount - 1;
            else if((unsigned) hasFocus >= page->objectsCount)
                hasFocus = 0;
        } while(++i < page->objectsCount && (page->objects[hasFocus].flags & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN)));

        if(hasFocus != page->focus)
        {
            page->objects[page->focus].flags &= ~MNF_FOCUS;
            page->focus = hasFocus;
            page->objects[page->focus].flags |= MNF_FOCUS;
            flashCounter = 0;
            S_LocalSound(cmd == MCMD_NAV_UP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
            calcNumVisObjects(page);
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
            switch(obj->type)
            {
            case MN_EDIT:
            case MN_LIST:
            case MN_COLORBOX:
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

            // Disable the menu binding class
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
            if(Chat_IsActive(CONSOLEPLAYER))
            {
                return;
            }

            S_LocalSound(SFX_MENU_OPEN, NULL);

            Con_Open(false);

            Hu_FogEffectSetAlphaTarget(1);
            Hu_MenuSetAlpha(1);
            menuActive = true;
            flashCounter = 0;
            mnTime = 0;
            cursorAngle = 0;
            Hu_MenuSetActivePage(&MainMenu);
            FR_ResetTypeInTimer();

            initAllObjectsOnAllPages();

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
        if(NULL != obj && !(obj->flags & (MNF_DISABLED | MNF_INACTIVE)))
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
        if(NULL != obj && !(obj->flags & (MNF_DISABLED | MNF_INACTIVE)))
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
#if 0
    /// \fixme This can no longer work this way. A shortcut should be defined in mn_object_t
    mn_page_t* page;

    if(!Hu_MenuIsActive()) return false;

    page = Hu_MenuActivePage();

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
        last += page->objectsCount/*page->numVisObjects*/ - 1;

        if(last > page->objectsCount - 1)
            last = page->objectsCount - 1;

        for(i = first; i <= last; ++i)
        {
            const mn_object_t* obj = &page->objects[i];
            if(obj->text && obj->text[0] && !(obj->flags & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN)))
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
                    page->objects[page->focus].flags &= ~MNF_FOCUS;
                    page->focus = i;
                    page->objects[page->focus].flags |= MNF_FOCUS;
                    return true;
                }
            }
        }
    }
#endif
    return false;
}

void M_DrawMenuText5(const char* string, int x, int y, int fontIdx, short flags,
    float glitterStrength, float shadowStrength)
{
    if(NULL == string || !string[0]) return;

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
    M_DrawMenuText5(string, x, y, fontIdx, flags, glitterStrength, rs.textShadow);
}

void M_DrawMenuText3(const char* string, int x, int y, int fontIdx, short flags)
{
    M_DrawMenuText4(string, x, y, fontIdx, flags, rs.textGlitter);
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
 * The color widget edits the "hot" currentcolour[]
 * The widget responder handles setting the specified vars to that of the
 * currentcolor.
 */
void M_DrawColorWidgetMenu(mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JDOOM64__
# define BGWIDTH            (160)
# define BGHEIGHT           (rgbaMode? 85 : 75)
#elif __JHERETIC__ || __JHEXEN__
# define BGWIDTH            (180)
# define BGHEIGHT           (rgbaMode? 170 : 140)
#endif

    mn_object_t* obj = (mn_object_t*) page->data;
    mndata_colorbox_t* cbox = (mndata_colorbox_t*) obj->data;
    boolean rgbaMode = (NULL != cbox->a);

    DGL_Enable(DGL_TEXTURE_2D);

    //M_DrawBackgroundBox(x-24, y-40, BGWIDTH, BGHEIGHT, true, BORDERUP, 1, 1, 1, rs.pageAlpha);
    //DGL_SetNoMaterial();

    DGL_DrawRect(x + BGWIDTH/2 - 24/2 - 24, y + 10 - 40, 24, 22, colorWidgetColor[0], colorWidgetColor[1], colorWidgetColor[2], colorWidgetColor[3]);
    M_DrawBackgroundBox(x + BGWIDTH/2 - 24/2 - 24, y + 10 - 40, 24, 22, false, BORDERDOWN, 1, 1, 1, rs.pageAlpha);

    DGL_Disable(DGL_TEXTURE_2D);

#undef BGWIDTH
#undef BGHEIGHT
}

/**
 * Inform the menu to activate the color widget
 * An intermediate step. Used to copy the existing rgba values pointed
 * to by the index (these match an index in the widgetColors array) into
 * the "hot" colorWidgetColor[] slots.
 */
void Hu_MenuActivateColorWidget(mn_object_t* obj)
{
    /// \fixme Find the objects by id.
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->data;
    mndata_slider_t* sldrRed   = (mndata_slider_t*)ColorWidgetMenu.objects[1].data;
    mndata_slider_t* sldrGreen = (mndata_slider_t*)ColorWidgetMenu.objects[3].data;
    mndata_slider_t* sldrBlue  = (mndata_slider_t*)ColorWidgetMenu.objects[5].data;
    mndata_slider_t* sldrAlpha = (mndata_slider_t*)ColorWidgetMenu.objects[7].data;
    boolean rgbaMode = (NULL != cbox->a);

    MNPage_Initialize(&ColorWidgetMenu);
    ColorWidgetMenu.data = obj;

    colorWidgetColor[CR] = *cbox->r;
    colorWidgetColor[CG] = *cbox->g;
    colorWidgetColor[CB] = *cbox->b;
    colorWidgetColor[CA] = (rgbaMode? *cbox->a : 1.0f);

    sldrRed->value   = *cbox->r;
    sldrGreen->value = *cbox->g;
    sldrBlue->value  = *cbox->b;
    sldrAlpha->value = (rgbaMode? *cbox->a : 1.0f);

    // Hide the alpha slider and label if not in rgbaMode.
    if(rgbaMode)
    {
        ColorWidgetMenu.objects[6].flags &= ~(MNF_DISABLED | MNF_HIDDEN);
        ColorWidgetMenu.objects[7].flags &= ~(MNF_DISABLED | MNF_HIDDEN);
    }
    else
    {
        ColorWidgetMenu.objects[6].flags |= MNF_DISABLED | MNF_HIDDEN;
        ColorWidgetMenu.objects[7].flags |= MNF_DISABLED | MNF_HIDDEN;
    }

    obj->flags &= ~MNF_INACTIVE; 
    colorWidgetActive = true; // Activate the widget.
}

/**
 * User wants to load this game
 */
void M_SelectLoad(mn_object_t* obj)
{
    const int saveSlot = obj->data2;

    SaveMenu.focus = saveSlot;
    Hu_MenuCommand(MCMD_CLOSEFAST);

    G_LoadGame(saveSlot);
}

void M_DrawMainMenu(mn_page_t* page, int x, int y)
{
#if __JHEXEN__
    int frame = (mnTime / 5) % 7;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, rs.pageAlpha);

    GL_DrawPatch(pMainTitle, x - 22, y - 56);
    GL_DrawPatch(pBullWithFire[(frame + 2) % 7], x - 73, y + 24);
    GL_DrawPatch(pBullWithFire[frame], x + 168, y + 24);

    DGL_Disable(DGL_TEXTURE_2D);

#elif __JHERETIC__
    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch5(pMainTitle, x - 22, y - 56, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, rs.pageAlpha, rs.textGlitter, rs.textShadow);
    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    GL_DrawPatch(pRotatingSkull[17 - frame], x - 70, y - 46);
    GL_DrawPatch(pRotatingSkull[frame], x + 122, y - 46);

    DGL_Disable(DGL_TEXTURE_2D);
#elif __JDOOM__ || __JDOOM64__
    DGL_Enable(DGL_TEXTURE_2D);
    WI_DrawPatch5(pMainTitle, x - 3, y - 62, NULL, GF_FONTB, false, DPF_ALIGN_TOPLEFT, 1, 1, 1, rs.pageAlpha, rs.textGlitter, rs.textShadow);
    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void M_DrawGameTypeMenu(mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JDOOM64__
#  define TITLEOFFSET_X       (67)
#else
#  define TITLEOFFSET_X       (60)
#endif

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);

    M_DrawMenuText3(GET_TXT(TXT_PICKGAMETYPE), x + TITLEOFFSET_X, y - 25, GF_FONTB, DTF_ALIGN_TOP);

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
void M_DrawPlayerClassMenu(mn_page_t* page, int x, int y)
{
#define BG_X            (108)
#define BG_Y            (-58)

    assert(NULL != page);
    {
    float w, h, s, t;
    int pClass;
    spriteinfo_t sprInfo;
    int tmap = 1;

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText2("Choose class:", x - 32, y - 42, GF_FONTB);

    pClass = MNPage_FocusObject(page)->data2;
    if(pClass < 0)
    {   // Random class.
        // Number of user-selectable classes.
        pClass = (mnTime / 5) % (page->objectsCount - 1);
    }

    R_GetSpriteInfo(STATES[PCLASS_INFO(pClass)->normalState].sprite, ((mnTime >> 3) & 3), &sprInfo);

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    GL_DrawPatch(pPlayerClassBG[pClass % 3], x + BG_X, y + BG_Y);

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

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
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

#undef BG_X
#undef BG_Y
}
#endif

#if __JDOOM__ || __JHERETIC__
void M_DrawEpisodeMenu(mn_page_t* page, int x, int y)
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
        DGL_Color4f(cfg.menuTextColors[1][CR], cfg.menuTextColors[1][CG], cfg.menuTextColors[1][CB], rs.pageAlpha);
        M_DrawMenuText3(str, SCREENWIDTH/2, SCREENHEIGHT - 2, GF_FONTA, DTF_ALIGN_BOTTOM);
    }
    // kludge end.
#else // __JDOOM__
    WI_DrawPatch5(pEpisode, x + 7, y - 25, "{case}Which Episode{scaley=1.25,y=-3}?", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha, rs.textGlitter, rs.textShadow);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void M_DrawSkillMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JDOOM__ || __JDOOM64__
    WI_DrawPatch5(pNewGame, x + 48, y - 49, "{case}NEW GAME", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha, rs.textGlitter, rs.textShadow);
    WI_DrawPatch5(pSkill, x + 6, y - 25, "{case}Choose Skill Level:", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha, rs.textGlitter, rs.textShadow);
#elif __JHEXEN__
    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("Choose Skill Level:", x - 46, y - 28, GF_FONTB, DTF_ALIGN_TOPLEFT);
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
        mn_object_t* obj = &LoadMenuObjects[i];
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
void M_SaveGame(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    const int saveSlot = edit->data2;
    const char* saveName = edit->text;

    if(menuNominatingQuickSaveSlot)
    {
        Con_SetInteger("game-save-quick-slot", saveSlot);
        menuNominatingQuickSaveSlot = false;
    }

    if(!G_SaveGame2(saveSlot, saveName))
        return;

    SaveMenu.focus = edit->data2;
    LoadMenu.focus = edit->data2;
    Hu_MenuCommand(MCMD_CLOSEFAST);
    }
}

int MNPage_PredefinedFont(mn_page_t* page, mn_page_fontid_t id)
{
    assert(NULL != page);
    if(!VALID_MNPAGE_FONTID(id))
    {
#if _DEBUG
        Con_Error("MNPage::PredefinedFont: Invalid font id '%i'.", (int)id);
        exit(1); // Unreachable.
#endif
        return 0; // Not a valid font id.
    }
    return (int)page->fonts[id];
}

void MNPage_PredefinedColor(mn_page_t* page, mn_page_colorid_t id, float rgb[3])
{
    assert(NULL != page);
    {
    uint colorIndex;
    if(NULL == rgb)
    {
#if _DEBUG
        Con_Error("MNPage::PredefinedColor: Invalid 'rgb' reference.");
        exit(1); // Unreachable.
#endif
        return;
    }
    if(!VALID_MNPAGE_COLORID(id))
    {
#if _DEBUG
        Con_Error("MNPage::PredefinedColor: Invalid color id '%i'.", (int)id);
        exit(1); // Unreachable.
#endif
        rgb[CR] = rgb[CG] = rgb[CB] = 1;
        return;
    }
    colorIndex = page->colors[id];
    rgb[CR] = cfg.menuTextColors[colorIndex][CR];
    rgb[CG] = cfg.menuTextColors[colorIndex][CG];
    rgb[CB] = cfg.menuTextColors[colorIndex][CB];
    }
}

void MNText_Drawer(mn_object_t* obj, int x, int y)
{
    gamefontid_t fontIdx = rs.textFonts[obj->pageFontIdx];
    float color[4];

    memcpy(color, rs.textColors[obj->pageColorIdx], sizeof(color));

    // Flash the focused object?
    if(obj->flags & MNF_FOCUS)
    {
        float t = (flashCounter <= 50? flashCounter / 50.0f : (100 - flashCounter) / 50.0f);
        color[CR] *= t; color[CG] *= t; color[CB] *= t;
        color[CR] += cfg.menuTextFlashColor[CR] * (1 - t); color[CG] += cfg.menuTextFlashColor[CG] * (1 - t); color[CB] += cfg.menuTextFlashColor[CB] * (1 - t);
    }

    if(obj->patch)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch5(*obj->patch, x, y, (obj->flags & MNF_NO_ALTTEXT)? NULL : obj->text, fontIdx, true, DPF_ALIGN_TOPLEFT, color[CR], color[CG], color[CB], color[CA], rs.textGlitter, rs.textShadow);
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(color);

    M_DrawMenuText2(obj->text, x, y, fontIdx);

    DGL_Disable(DGL_TEXTURE_2D);
}

void MNText_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
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
    FR_SetFont(FID(MNPage_PredefinedFont(page, obj->pageFontIdx)));
    FR_TextFragmentDimensions(width, height, obj->text);
}

static void drawEditBackground(const mn_object_t* obj, int x, int y, int width, float alpha)
{
#if __JHERETIC__ || __JHEXEN__
    //patchinfo_t middleInfo;
    //if(R_GetPatchInfo(pEditMiddle, &middleInfo))
    {
        DGL_Color4f(1, 1, 1, alpha);
        GL_DrawPatch(pEditMiddle, x - 8, y - 4);
    }
#else
    patchinfo_t leftInfo, rightInfo, middleInfo;
    int leftOffset = 0, rightOffset = 0;

    if(R_GetPatchInfo(pEditLeft, &leftInfo))
    {
        DGL_SetPatch(pEditLeft, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x, y - 3, leftInfo.width, leftInfo.height, 1, 1, 1, alpha);
        leftOffset = leftInfo.width;
    }

    if(R_GetPatchInfo(pEditRight, &rightInfo))
    {
        DGL_SetPatch(pEditRight, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x + width - rightInfo.width, y - 3, rightInfo.width, rightInfo.height, 1, 1, 1, alpha);
        rightOffset = rightInfo.width;
    }

    if(R_GetPatchInfo(pEditMiddle, &middleInfo))
    {
        DGL_SetPatch(pEditMiddle, DGL_REPEAT, DGL_REPEAT);
        DGL_Color4f(1, 1, 1, alpha);
        DGL_DrawRectTiled(x + leftOffset, y - 3, width - leftOffset - rightOffset, 14, 8, 14);
    }
#endif
}

void MNEdit_Drawer(mn_object_t* obj, int x, int y)
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
    gamefontid_t fontIdx = rs.textFonts[obj->pageFontIdx];
    char buf[MNDATA_EDIT_TEXT_MAX_LENGTH+1];
    float light = 1, textAlpha = rs.pageAlpha;
    const char* string;
    boolean isActive = (!(obj->flags & MNF_INACTIVE) && (obj->flags & MNF_FOCUS));

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
            textAlpha = rs.pageAlpha * .75f;
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(fontIdx));
    { int width, numVisCharacters;
    if(edit->maxVisibleChars > 0)
        numVisCharacters = MIN_OF(edit->maxVisibleChars, MNDATA_EDIT_TEXT_MAX_LENGTH);
    else
        numVisCharacters = MNDATA_EDIT_TEXT_MAX_LENGTH;
    width = numVisCharacters * FR_CharWidth('_') + 20;
    drawEditBackground(obj, x + BG_OFFSET_X, y - 1, width, rs.pageAlpha);
    }

    if(string)
    {
        float color[4];
        color[CR] = cfg.menuTextColors[COLOR_IDX][CR];
        color[CG] = cfg.menuTextColors[COLOR_IDX][CG];
        color[CB] = cfg.menuTextColors[COLOR_IDX][CB];

        if(isActive)
        {
            float t = (flashCounter <= 50? (flashCounter / 50.0f) : ((100 - flashCounter) / 50.0f));

            color[CR] *= t;
            color[CG] *= t;
            color[CB] *= t;

            color[CR] += cfg.menuTextFlashColor[CR] * (1 - t);
            color[CG] += cfg.menuTextFlashColor[CG] * (1 - t);
            color[CB] += cfg.menuTextFlashColor[CB] * (1 - t);
        }
        color[CA] = textAlpha;

        color[CR] *= light;
        color[CG] *= light;
        color[CB] *= light;

        DGL_Color4fv(color);
        M_DrawMenuText3(string, x, y, fontIdx, DTF_ALIGN_TOPLEFT);
    }

    DGL_Disable(DGL_TEXTURE_2D);

#undef BG_OFFSET_X
#undef OFFSET_Y
#undef OFFSET_X
#undef COLOR_IDX
}

int MNEdit_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    switch(cmd)
    {
    case MCMD_SELECT:
        if(obj->flags & MNF_INACTIVE)
        {
            if(NULL != obj->action)
            {
                S_LocalSound(SFX_MENU_CYCLE, NULL);
                obj->action(obj);
                return true;
            }
        }
        else
        {
            obj->flags |= MNF_INACTIVE;
            if(NULL != edit->onChange)
            {
                edit->onChange(obj);
            }
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            return true;
        }
        break;
    case MCMD_NAV_OUT:
        if(!(obj->flags & MNF_INACTIVE))
        {
            memcpy(edit->text, edit->oldtext, sizeof(edit->text));
            obj->flags |= MNF_INACTIVE;
            return true;
        }
        break;
    default:
        break;
    }
    return false; // Not eaten.
    }
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
int MNEdit_Responder(mn_object_t* obj, const event_t* ev)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*) obj->data;
    int ch = -1;
    char* ptr;

    if(ev->type != EV_KEY)
        return false;

    if(DDKEY_RSHIFT == ev->data1)
    {
        shiftdown = (EVS_DOWN == ev->state || EVS_REPEAT == ev->state);
        return true;
    }

    if(!(EVS_DOWN == ev->state || EVS_REPEAT == ev->state))
        return false;

    if(DDKEY_BACKSPACE == ev->data1)
    {
        size_t len = strlen(edit->text);
        if(0 != len)
            edit->text[len - 1] = '\0';
        return true;
    }

    ch = ev->data1;
    if(ch >= ' ' && ch <= 'z')
    {
        if(shiftdown)
            ch = shiftXForm[ch];

        // Filter out nasty characters.
        if(ch == '%')
            return true;

        if(strlen(edit->text) < MNDATA_EDIT_TEXT_MAX_LENGTH)
        {
            ptr = edit->text + strlen(edit->text);
            ptr[0] = ch;
            ptr[1] = '\0';
        }
        return true;
    }

    return false;
    }
}

void MNEdit_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
{
    // @fixme calculate visible dimensions properly.
    if(width)
        *width = 170;
    if(height)
        *height = 14;
}

void MNList_Drawer(mn_object_t* obj, int x, int y)
{
    const mndata_list_t* list = (const mndata_list_t*) obj->data;
    gamefontid_t fontIdx = rs.textFonts[obj->pageFontIdx];
    int i;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(rs.textColors[obj->pageColorIdx]);
    for(i = 0; i < list->count; ++i)
    {
        const mndata_listitem_t* item = &((const mndata_listitem_t*) list->items)[i];
        M_DrawMenuText2(item->text, x, y, fontIdx);
        FR_SetFont(FID(fontIdx));
        y += FR_TextFragmentHeight(item->text) * (1+MNDATA_LIST_LEADING);
    }
    DGL_Disable(DGL_TEXTURE_2D);
}

int MNList_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    assert(NULL != obj);
    switch(cmd)
    {
    case MCMD_NAV_DOWN:
        if(!(obj->flags & MNF_INACTIVE))
        {
            S_LocalSound(SFX_MENU_NAV_DOWN, NULL);
            return true;
        }
        break;
    case MCMD_NAV_UP:
        if(!(obj->flags & MNF_INACTIVE))
        {
            S_LocalSound(SFX_MENU_NAV_UP, NULL);
            return true;
        }
        break;
    case MCMD_NAV_OUT:
        if(!(obj->flags & MNF_INACTIVE))
        {
            obj->flags |= MNF_INACTIVE;
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            return true;
        }
        break;
    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        if(!(obj->flags & MNF_INACTIVE))
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            return true;
        }
        break;
    case MCMD_SELECT:
        if(NULL != obj->action)
        {
            if(obj->flags & MNF_INACTIVE)
            {
                S_LocalSound(SFX_MENU_CYCLE, NULL);
                obj->action(obj);
                obj->flags &= ~MNF_INACTIVE;
            }
            else
            {
                S_LocalSound(SFX_MENU_CYCLE, NULL);
                obj->action(obj);
            }
            return true;
        }
        break;
    default:
        break;
    }
    return false; // Not eaten.
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

void MNListInline_Drawer(mn_object_t* obj, int x, int y)
{
    const mndata_listinline_t* list = (const mndata_listinline_t*) obj->data;
    const mndata_listitem_t* item = ((const mndata_listitem_t*) list->items) + list->selection;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(rs.textColors[obj->pageColorIdx]);

    M_DrawMenuText2(item->text, x, y, rs.textFonts[obj->pageFontIdx]);

    DGL_Disable(DGL_TEXTURE_2D);
}

int MNListInline_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    assert(NULL != obj);
    {
    mndata_listinline_t* list = obj->data;
    switch(cmd)
    {
    case MCMD_SELECT: // Treat as @c MCMD_NAV_RIGHT
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        int oldsel = list->selection;

        if(MCMD_NAV_LEFT == cmd)
        {
            if(list->selection > 0)
                --list->selection;
        }
        else
        {
            if(list->selection < list->count - 1)
                ++list->selection;
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
        if(oldsel != list->selection && NULL != obj->action)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            obj->action(obj);
        }
        return true;
      }
    default:
        break;
    }
    return false; // Not eaten.
    }
}

void MNList_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
{
    const mndata_list_t* list = (const mndata_list_t*) obj->data;
    int i;
    if(!width && !height)
        return;
    if(width)
        *width = 0;
    if(height)
        *height = 0;
    FR_SetFont(FID(MNPage_PredefinedFont(page, obj->pageFontIdx)));
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

void MNListInline_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
{
    const mndata_listinline_t* list = (const mndata_listinline_t*) obj->data;
    const mndata_listitem_t* item = ((const mndata_listitem_t*) list->items) + list->selection;
    FR_SetFont(FID(MNPage_PredefinedFont(page, obj->pageFontIdx)));
    if(width)
        *width = FR_TextFragmentWidth(item->text);
    if(height)
        *height = FR_TextFragmentHeight(item->text);
}

void MNButton_Drawer(mn_object_t* obj, int x, int y)
{
    int dis = (obj->flags & MNF_DISABLED) != 0;
    int act = (obj->flags & MNF_INACTIVE) == 0;
    int click = (obj->flags & MNF_CLICKED) != 0;
    boolean down = act || click;
    const gamefontid_t fontIdx = rs.textFonts[obj->pageFontIdx];
    const char* text;
    float color[4];

    memcpy(color, rs.textColors[obj->pageColorIdx], sizeof(color));

    // Flash the focused object?
    if(obj->flags & MNF_FOCUS)
    {
        float t = (flashCounter <= 50? flashCounter / 50.0f : (100 - flashCounter) / 50.0f);
        color[CR] *= t; color[CG] *= t; color[CB] *= t;
        color[CR] += cfg.menuTextFlashColor[CR] * (1 - t); color[CG] += cfg.menuTextFlashColor[CG] * (1 - t); color[CB] += cfg.menuTextFlashColor[CB] * (1 - t);
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

    if(obj->patch)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch5(*obj->patch, x, y, (obj->flags & MNF_NO_ALTTEXT)? NULL : text, fontIdx, true, DPF_ALIGN_TOPLEFT, color[CR], color[CG], color[CB], color[CA], rs.textGlitter, rs.textShadow);
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(color);

    M_DrawMenuText2(text, x, y, fontIdx);

    DGL_Disable(DGL_TEXTURE_2D);
}

int MNButton_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    assert(NULL != obj);
    if(MCMD_SELECT == cmd)
    {
        if(obj->flags & MNF_CLICKED)
        {
            //if((ev->device == IDEV_KEYBOARD || ev->device == IDEV_MOUSE) && IS_TOGGLE_UP(ev))
            {
                //UI_Capture(0);
                obj->flags &= ~MNF_CLICKED;
                //if(UI_MouseInside(obj) || ev->device == IDEV_KEYBOARD)
                {
                    // Activate?
                    if(NULL != obj->action)
                    {
                        S_LocalSound(SFX_MENU_CYCLE, NULL);
                        obj->action(obj);
                    }
                }
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
            }

            // Call the action function.
            if(NULL != obj->action)
            {
                S_LocalSound(SFX_MENU_CYCLE, NULL);
                obj->action(obj);
            }
            //obj->timer = 0;
        }
        return true;
    }
    return false; // Not eaten.
}

void MNButton_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
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
    FR_SetFont(FID(MNPage_PredefinedFont(page, obj->pageFontIdx)));
    FR_TextFragmentDimensions(width, height, text);
}

void MNColorBox_Drawer(mn_object_t* obj, int x, int y)
{
    const mndata_colorbox_t* cbox = (const mndata_colorbox_t*) obj->data;
    const int width = MNDATA_COLORBOX_WIDTH;
    const int height = MNDATA_COLORBOX_HEIGHT;

    x += MNDATA_COLORBOX_PADDING_X;
    y += MNDATA_COLORBOX_PADDING_Y;

    DGL_Enable(DGL_TEXTURE_2D);
    M_DrawBackgroundBox(x, y, width, height, true, BORDERDOWN, 1, 1, 1, rs.pageAlpha);
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_SetNoMaterial();
    DGL_DrawRect(x, y, width, height, *cbox->r, *cbox->g, *cbox->b, cbox->a? *cbox->a : 1 * rs.pageAlpha);
}

int MNColorBox_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    assert(NULL != obj);
    {
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->data;
    switch(cmd)
    {
    case MCMD_SELECT:
        if(obj->flags & MNF_INACTIVE)
        {
            if(NULL != obj->action)
            {
                S_LocalSound(SFX_MENU_CYCLE, NULL);
                obj->action(obj);
                return true;
            }
        }
        break;
    default:
        break;
    }
    return false; // Not eaten.
    }
}

void MNColorBox_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
{
    if(width)  *width  = MNDATA_COLORBOX_WIDTH  + MNDATA_COLORBOX_PADDING_X*2;
    if(height) *height = MNDATA_COLORBOX_HEIGHT + MNDATA_COLORBOX_PADDING_Y*2;
}

int MNSlider_ThumbPos(const mn_object_t* obj)
{
#define WIDTH           (middleInfo.width)

    mndata_slider_t* data = obj->data;
    float range = data->max - data->min, useVal;
    patchinfo_t middleInfo;

    if(!R_GetPatchInfo(pSliderMiddle, &middleInfo))
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

void MNSlider_Drawer(mn_object_t* obj, int inX, int inY)
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

    if(!R_GetPatchInfo(pSliderMiddle, &middleInfo))
        return;
    if(!R_GetPatchInfo(pSliderLeft, &leftInfo))
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
        M_DrawGlowBar(from, to, HEIGHT*1.1f, true, true, true, 0, 0, 0, rs.pageAlpha * rs.textShadow);
    }

    DGL_Color4f(1, 1, 1, rs.pageAlpha);

    GL_DrawPatch2(pSliderLeft, 0, 0, DPF_ALIGN_RIGHT|DPF_ALIGN_TOP|DPF_NO_OFFSETX);
    GL_DrawPatch(pSliderRight, MNDATA_SLIDER_SLOTS * WIDTH, 0);

    DGL_SetPatch(pSliderMiddle, DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectTiled(0, middleInfo.topOffset, MNDATA_SLIDER_SLOTS * WIDTH, HEIGHT, middleInfo.width, middleInfo.height);

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    GL_DrawPatch2(pSliderHandle, MNSlider_ThumbPos(obj), 1, DPF_ALIGN_TOP|DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef HEIGHT
#undef WIDTH
#undef OFFSET_Y
#undef OFFSET_X
}

int MNSlider_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    assert(NULL != obj);
    {
    mndata_slider_t* sldr = (mndata_slider_t*)obj->data;
    switch(cmd)
    {
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        float oldvalue = sldr->value;

        if(MCMD_NAV_LEFT == cmd)
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

        // Did the value change?
        if(oldvalue != sldr->value && NULL != obj->action)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            obj->action(obj);
        }
        return true;
      }
    default:
        break;
    }
    return false; // Not eaten.
    }
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

void MNSlider_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
{
    patchinfo_t info;
    if(!R_GetPatchInfo(pSliderMiddle, &info))
        return;
    if(width)
        *width = (int) (info.width * MNDATA_SLIDER_SLOTS * MNDATA_SLIDER_SCALE + .5f);
    if(height)
    {
        int max = info.height;
        if(R_GetPatchInfo(pSliderLeft, &info))
            max = MAX_OF(max, info.height);
        if(R_GetPatchInfo(pSliderRight, &info))
            max = MAX_OF(max, info.height);
        *height = (int) ((max + MNDATA_SLIDER_PADDING_Y * 2) * MNDATA_SLIDER_SCALE + .5f);
    }
}

void MNSlider_TextualValueDrawer(mn_object_t* obj, int x, int y)
{
    assert(NULL != obj);
    {
    const mndata_slider_t* sldr = (const mndata_slider_t*) obj->data;
    const float value = MINMAX_OF(sldr->min, sldr->value, sldr->max);
    const gamefontid_t fontIdx = rs.textFonts[obj->pageFontIdx];
    char textualValue[41];
    const char* str = composeValueString(value, 0, sldr->floatMode, 0, 
        sldr->data2, sldr->data3, sldr->data4, sldr->data5, 40, textualValue);

    DGL_Translatef(x, y, 0);
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4fv(rs.textColors[obj->pageColorIdx]);

    M_DrawMenuText2(str, 0, 0, fontIdx);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_Translatef(-x, -y, 0);
    }
}

void MNSlider_TextualValueDimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
{
    assert(NULL != obj);
    {
    const mndata_slider_t* sldr = (const mndata_slider_t*) obj->data;
    if(NULL != width || NULL != height)
    {
        const gamefontid_t fontIdx = MNPage_PredefinedFont(page, obj->pageFontIdx);
        const float value = MINMAX_OF(sldr->min, sldr->value, sldr->max);
        char textualValue[41];
        const char* str = composeValueString(value, 0, sldr->floatMode, 0,
            sldr->data2, sldr->data3, sldr->data4, sldr->data5, 40, textualValue);

        FR_SetFont(FID(fontIdx));
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
void MNMobjPreview_Drawer(mn_object_t* obj, int inX, int inY)
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

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
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

void MNMobjPreview_Dimensions(const mn_object_t* obj, mn_page_t* page, int* width, int* height)
{
    // @fixme calculate visible dimensions properly!
    if(width)
        *width = MNDATA_MOBJPREVIEW_WIDTH;
    if(height)
        *height = MNDATA_MOBJPREVIEW_HEIGHT;
}

void Hu_MenuCvarButton(mn_object_t* obj)
{
    cvarbutton_t* cb = obj->data;
    cvartype_t varType = Con_GetVariableType(cb->cvarname);
    int value;

    //strcpy(obj->text, cb->active? cb->yes : cb->no);
    obj->text = cb->active? cb->yes : cb->no;

    if(CVT_NULL == varType)
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

void Hu_MenuCvarListInline(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_listinline_t* list = obj->data;
    cvartype_t varType;
    int value;

    if(list->selection < 0)
        return; // Hmm?

    varType = Con_GetVariableType(list->data);
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
        Con_Error("Hu_MenuCvarListInline: Unsupported variable type %i", (int)varType);
        break;
    }
    }
}

void Hu_MenuSaveSlotEdit(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    // Store a copy of the present text value so we can restore it.
    memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));

    // Are we suggesting a new name?
    if(cfg.menuGameSaveSuggestName)
    {
        ddstring_t* suggestName = G_GenerateSaveGameName();
        strncpy(edit->text, Str_Text(suggestName), MNDATA_EDIT_TEXT_MAX_LENGTH);
        edit->text[MNDATA_EDIT_TEXT_MAX_LENGTH] = '\0';
        Str_Free(suggestName);
    }
    obj->flags &= ~MNF_INACTIVE;
    }
}

void Hu_MenuCvarEdit(mn_object_t* obj)
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

void Hu_MenuCvarSlider(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_slider_t* sldr = obj->data;
    cvartype_t varType = Con_GetVariableType(sldr->data1);
    float value = sldr->value;

    if(CVT_NULL == varType)
        return;

    if(!sldr->floatMode)
    {
        value += (sldr->value < 0? -.5f : .5f);
    }

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
    }
}

void M_DrawLoadMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("Load Game", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);
#else
    WI_DrawPatch5(pLoadGame, x - 8, y - 26, "{case}Load game", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha, rs.textGlitter, rs.textShadow);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawSaveMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("Save Game", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);
#else
    WI_DrawPatch5(pSaveGame, x - 8, y - 26, "{case}Save game", GF_FONTB, true, DPF_ALIGN_TOPLEFT, cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha, rs.textGlitter, rs.textShadow);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void M_OpenHelp(mn_object_t* obj)
{
    G_StartHelp();
}
#endif

void M_DrawOptionsMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

#if __JHERETIC__ || __JHEXEN__
    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("OPTIONS", x + 42, y - 38, GF_FONTB, DTF_ALIGN_TOP);
#else
#  if __JDOOM64__
    WI_DrawPatch5(pOptionsTitle, x + 42, y - 20, "{case}OPTIONS", GF_FONTB, true, DPF_ALIGN_TOP, cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha, rs.textGlitter, rs.textShadow);
#  else
    WI_DrawPatch5(pOptionsTitle, x + 42, y - 20, "{case}OPTIONS", GF_FONTB, true, DPF_ALIGN_TOP, cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha, rs.textGlitter, rs.textShadow);
#  endif
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawSoundMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("SOUND OPTIONS", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawGameplayMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("GAMEPLAY", SCREENWIDTH/2, y-20, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawWeaponMenu(mn_page_t* page, int x, int y)
{
    int i = 0;
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("WEAPONS", SCREENWIDTH/2, y-26, GF_FONTB, DTF_ALIGN_TOP);

/*#if __JDOOM__ || __JDOOM64__
    MNPage_ComposeSubpageString(page, 1024, buf);
    DGL_Color4f(cfg.menuTextColors[1][CR], cfg.menuTextColors[1][CG], cfg.menuTextColors[1][CB], rs.pageAlpha);
    M_DrawMenuText3(buf, SCREENWIDTH/2, y - 12, GF_FONTA, DTF_ALIGN_TOP);
#elif __JHERETIC__
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    GL_DrawPatch(pInvPageLeft[!page->firstObject || (mnTime & 8)], x, y - 22);
    GL_DrawPatch(pInvPageRight[page->firstObject + page->numVisObjects >= page->objectsCount || (mnTime & 8)], 312 - x, y - 22);
#endif*/

    /// \kludge Inform the user how to change the order.
    /*{ mn_object_t* obj = MNPage_FocusObject(page);
    if(NULL != obj && obj == &page->objects[1])
    {
        const char* str = "Use left/right to move weapon up/down";
        DGL_Color4f(cfg.menuTextColors[1][CR], cfg.menuTextColors[1][CG], cfg.menuTextColors[1][CB], rs.pageAlpha);
        M_DrawMenuText3(str, SCREENWIDTH/2, SCREENHEIGHT/2 + (95/cfg.menuScale), GF_FONTA, DTF_ALIGN_BOTTOM);
    }}*/
    // kludge end.

    DGL_Disable(DGL_TEXTURE_2D);
}

#if __JHERETIC__ || __JHEXEN__
void M_DrawInventoryMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("Inventory Options", SCREENWIDTH/2, y-28, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void M_DrawHudMenu(mn_page_t* page, int x, int y)
{
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], rs.pageAlpha);
    M_DrawMenuText3("HUD options", SCREENWIDTH/2, y - 20, GF_FONTB, DTF_ALIGN_TOP);

/*#if __JDOOM__ || __JDOOM64__
    MNPage_ComposeSubpageString(page, 1024, buf);
    DGL_Color4f(1, .7f, .3f, rs.pageAlpha);
    M_DrawMenuText3(buf, x + SCREENWIDTH/2, y + -12, GF_FONTA, DTF_ALIGN_TOP);
#else
    // Draw the page arrows.
    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    GL_DrawPatch(pInvPageLeft[!page->firstObject || (mnTime & 8)], x, y + -22);
    GL_DrawPatch(pInvPageRight[page->firstObject + page->numVisObjects >= page->objectsCount || (mnTime & 8)], 312 - x, y + -22);
#endif*/

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawMultiplayerMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuTextColors[0][0], cfg.menuTextColors[0][1], cfg.menuTextColors[0][2], rs.pageAlpha);

    M_DrawMenuText3(GET_TXT(TXT_MULTIPLAYER), x + 60, y - 25, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_DrawPlayerSetupMenu(mn_page_t* page, int x, int y)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(cfg.menuTextColors[0][0], cfg.menuTextColors[0][1], cfg.menuTextColors[0][2], rs.pageAlpha);

    M_DrawMenuText3(GET_TXT(TXT_PLAYERSETUP), x + 90, y - 25, GF_FONTB, DTF_ALIGN_TOP);

    DGL_Disable(DGL_TEXTURE_2D);
}

void M_SetMenu(mn_object_t* obj)
{
    assert(NULL != obj);
    S_LocalSound(SFX_MENU_ACCEPT, NULL);
    Hu_MenuSetActivePage((mn_page_t*)obj->data);
}

void Hu_MenuUpdateColorWidgetColor(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_slider_t* sldr = (mndata_slider_t*)obj->data;
    colorWidgetColor[obj->data2] = sldr->value;
    }
}

void M_WeaponOrder(mn_object_t* obj)
{
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
}

void M_SelectSingleplayer(mn_object_t* obj)
{
    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NEWGAME, NULL, NULL);
        return;
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
}

void M_SelectMultiplayer(mn_object_t* obj)
{
    // Show the appropriate menu.
    if(IS_NETGAME)
    {
        MultiplayerMenu.objects = MultiplayerClientMenuObjects;
    }
    else
    {
        MultiplayerMenu.objects = MultiplayerMenuObjects;
    }

    Hu_MenuSetActivePage(&MultiplayerMenu);
}

void M_OpenMultiplayerClientMenu(mn_object_t* obj)
{
    if(IS_NETGAME)
    {
        DD_Execute(false, "net disconnect");
        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    DD_Execute(false, "net setup client");
}

void M_OpenPlayerSetupMenu(mn_object_t* obj)
{
    /// \fixme Find objects by id.
    mndata_mobjpreview_t* mop = &mop_player_preview;
    mndata_edit_t* name = &edit_player_name;

#if __JHEXEN__
    mop->mobjType = PCLASS_INFO(cfg.netClass)->mobjType;
    mop->plrClass = cfg.netClass;
#else
    mop->mobjType = MT_PLAYER;
#endif
    mop->tClass = 0;
    mop->tMap = cfg.netColor;

    list_player_color.selection = cfg.netColor;
#if __JHEXEN__
    list_player_class.selection = cfg.netClass;
#endif

    strncpy(name->text, Con_GetString("net-name"), MNDATA_EDIT_TEXT_MAX_LENGTH);
    name->text[MNDATA_EDIT_TEXT_MAX_LENGTH] = '\0';
    memcpy(name->oldtext, name->text, sizeof(name->oldtext));

    Hu_MenuSetActivePage(&PlayerSetupMenu);
}

#if __JHEXEN__
void M_ChangePlayerClass(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_listinline_t* lil = (mndata_listinline_t*) obj->data;
    mndata_mobjpreview_t* mop;

    if(lil->selection < 0) return;

    /// \fixme Find the object by id.
    mop = &mop_player_preview;
    mop->mobjType = PCLASS_INFO(lil->selection)->mobjType;
    mop->plrClass = lil->selection;
    }
}
#endif

void M_ChangePlayerColor(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    mndata_listinline_t* lil = (mndata_listinline_t*) obj->data;
    mndata_mobjpreview_t* mop;

    if(lil->selection < 0) return;

    /// \fixme Find the object by id.
    mop = &mop_player_preview;
    mop->tMap = lil->selection;
    }
}

void M_ChangePlayerName(mn_object_t* obj)
{
    mndata_edit_t* edit = (mndata_edit_t*)obj->data;
    memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));
    // Activate this.
    obj->flags &= ~MNF_INACTIVE;
}

void M_AcceptPlayerSetup(mn_object_t* obj)
{
    char buf[300];

    cfg.netColor = list_player_color.selection;
#if __JHEXEN__
    cfg.netClass = list_player_class.selection;
#endif

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
}

void M_SelectQuitGame(mn_object_t* obj)
{
    G_QuitGame();
}

void M_SelectEndGame(mn_object_t* obj)
{
    G_EndGame();
}

void M_OpenLoadMenu(mn_object_t* obj)
{
    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT && !Get(DD_PLAYBACK))
        {
            Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, NULL);
            return;
        }
    }

    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(&LoadMenu);
}

void M_OpenSaveMenu(mn_object_t* obj)
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
    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(&SaveMenu);
}

void M_SelectPlayerClass(mn_object_t* obj)
{
#if __JHEXEN__
    int option = obj->data2;

    if(IS_NETGAME)
    {
        P_SetMessage(&players[CONSOLEPLAYER], "You can't start a new game from within a netgame!", false);
        return;
    }

    if(option < 0)
    {   // Random class.
        // Number of user-selectable classes.
        mnPlrClass = (mnTime / 5) % (PlayerClassMenu.objectsCount - 1);
    }
    else
    {
        mnPlrClass = option;
    }

    switch(mnPlrClass)
    {
    case PCLASS_FIGHTER:
        SkillMenu.offset[VX] = 120;
        SkillMenuObjects[0].text = GET_TXT(TXT_SKILLF1);
        SkillMenuObjects[1].text = GET_TXT(TXT_SKILLF2);
        SkillMenuObjects[2].text = GET_TXT(TXT_SKILLF3);
        SkillMenuObjects[3].text = GET_TXT(TXT_SKILLF4);
        SkillMenuObjects[4].text = GET_TXT(TXT_SKILLF5);
        break;

    case PCLASS_CLERIC:
        SkillMenu.offset[VX] = 116;
        SkillMenuObjects[0].text = GET_TXT(TXT_SKILLC1);
        SkillMenuObjects[1].text = GET_TXT(TXT_SKILLC2);
        SkillMenuObjects[2].text = GET_TXT(TXT_SKILLC3);
        SkillMenuObjects[3].text = GET_TXT(TXT_SKILLC4);
        SkillMenuObjects[4].text = GET_TXT(TXT_SKILLC5);
        break;

    case PCLASS_MAGE:
        SkillMenu.offset[VX] = 112;
        SkillMenuObjects[0].text = GET_TXT(TXT_SKILLM1);
        SkillMenuObjects[1].text = GET_TXT(TXT_SKILLM2);
        SkillMenuObjects[2].text = GET_TXT(TXT_SKILLM3);
        SkillMenuObjects[3].text = GET_TXT(TXT_SKILLM4);
        SkillMenuObjects[4].text = GET_TXT(TXT_SKILLM5);
        break;
    }
    Hu_MenuSetActivePage(&SkillMenu);
#endif
}

#if __JDOOM__ || __JHERETIC__
void M_SelectEpisode(mn_object_t* obj)
{
#if __JHERETIC__
    if(gameMode == heretic_shareware && obj->data2)
    {
        Hu_MsgStart(MSG_ANYKEY, SWSTRING, NULL, NULL);
        G_StartHelp();
        return;
    }
#else
    if(gameMode == doom_shareware && obj->data2)
    {
        Hu_MsgStart(MSG_ANYKEY, SWSTRING, NULL, NULL);
        G_StartHelp();
        return;
    }
#endif

    epi = obj->data2;
    Hu_MenuSetActivePage(&SkillMenu);
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

void M_SelectSkillMode(mn_object_t* obj)
{
    assert(NULL != obj);
    {
    skillmode_t skillmode = (skillmode_t)obj->data2;

#if __JHEXEN__
    Hu_MenuCommand(MCMD_CLOSEFAST);
    cfg.playerClass[CONSOLEPLAYER] = mnPlrClass;
    G_DeferredNewGame(skillmode);
#else
# if __JDOOM__
    if(SM_NIGHTMARE == obj->data2)
    {
        Hu_MsgStart(MSG_YESNO, NIGHTMARE, M_VerifyNightmare, NULL);
        return;
    }
# endif

    Hu_MenuCommand(MCMD_CLOSEFAST);

# if __JDOOM64__
    G_DeferedInitNew(skillmode, 0, 0);
# else
    G_DeferedInitNew(skillmode, epi, 0);
# endif
#endif
    }
}

void M_OpenControlPanel(mn_object_t* obj)
{
#define NUM_PANEL_NAMES 3
    static const char* panelNames[] = {
        "panel",
        "panel audio",
        "panel input"
    };
    int idx = obj->data2;
    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
        idx = 0;

    Hu_MenuCommand(MCMD_CLOSEFAST);
    DD_Execute(true, panelNames[idx]);

#undef NUM_PANEL_NAMES
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
