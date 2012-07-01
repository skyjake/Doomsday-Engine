/**\file hu_menu.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

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

/// Original game line height for pages that employ the fixed layout (in 320x200 pixels).
#if __JDOOM__
#  define FIXED_LINE_HEIGHT             (15+1)
#else
#  define FIXED_LINE_HEIGHT             (19+1)
#endif

typedef struct cvarbutton_s {
    char            active;
    const char*     cvarname;
    const char*     yes;
    const char*     no;
    int             mask;
} cvarbutton_t;

int Hu_MenuActionSetActivePage(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuActionInitNewGame(mn_object_t* ob, mn_actionid_t action, void* parameters);

int Hu_MenuSelectLoadGame(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectSaveGame(mn_object_t* ob, mn_actionid_t action, void* parameters);
#if __JHEXEN__
int Hu_MenuSelectFiles(mn_object_t* ob, mn_actionid_t action, void* parameters);
#endif
int Hu_MenuSelectPlayerSetup(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectJoinGame(mn_object_t* ob, mn_actionid_t action, void* parameters);

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
int Hu_MenuSelectHelp(mn_object_t* ob, mn_actionid_t action, void* parameters);
#endif
int Hu_MenuSelectControlPanelLink(mn_object_t* ob, mn_actionid_t action, void* parameters);

int Hu_MenuSelectSingleplayer(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectMultiplayer(mn_object_t* ob, mn_actionid_t action, void* parameters);
#if __JDOOM__ || __JHERETIC__
int Hu_MenuFocusEpisode(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuActivateNotSharewareEpisode(mn_object_t* ob, mn_actionid_t action, void* parameters);
#endif
#if __JHEXEN__
int Hu_MenuFocusOnPlayerClass(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectPlayerClass(mn_object_t* ob, mn_actionid_t action, void* parameters);
#endif
int Hu_MenuFocusSkillMode(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectLoadSlot(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectQuitGame(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectEndGame(mn_object_t* ob, mn_actionid_t action, void* parameters);
int Hu_MenuSelectAcceptPlayerSetup(mn_object_t* ob, mn_actionid_t action, void* parameters);

int Hu_MenuSelectSaveSlot(mn_object_t* ob, mn_actionid_t action, void* parameters);

int Hu_MenuChangeWeaponPriority(mn_object_t* ob, mn_actionid_t action, void* parameters);
#if __JHEXEN__
int Hu_MenuSelectPlayerSetupPlayerClass(mn_object_t* ob, mn_actionid_t action, void* parameters);
#endif
int Hu_MenuSelectPlayerColor(mn_object_t* ob, mn_actionid_t action, void* parameters);

#if __JHEXEN__
void Hu_MenuPlayerClassBackgroundTicker(mn_object_t* ob);
void Hu_MenuPlayerClassPreviewTicker(mn_object_t* ob);
#endif

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(mn_page_t* page, const Point2Raw* origin);
#endif

void Hu_MenuDrawGameTypePage(mn_page_t* page, const Point2Raw* origin);
void Hu_MenuDrawSkillPage(mn_page_t* page, const Point2Raw* origin);
#if __JHEXEN__
void Hu_MenuDrawPlayerClassPage(mn_page_t* page, const Point2Raw* origin);
#endif
#if __JDOOM__ || __JHERETIC__
void Hu_MenuDrawEpisodePage(mn_page_t* page, const Point2Raw* origin);
#endif
void Hu_MenuDrawOptionsPage(mn_page_t* page, const Point2Raw* origin);
void Hu_MenuDrawWeaponsPage(mn_page_t* page, const Point2Raw* origin);
void Hu_MenuDrawLoadGamePage(mn_page_t* page, const Point2Raw* origin);
void Hu_MenuDrawSaveGamePage(mn_page_t* page, const Point2Raw* origin);
void Hu_MenuDrawMultiplayerPage(mn_page_t* page, const Point2Raw* origin);
void Hu_MenuDrawPlayerSetupPage(mn_page_t* page, const Point2Raw* origin);

int Hu_MenuColorWidgetCmdResponder(mn_page_t* page, menucommand_e cmd);

static void initAllPages(void);
static void destroyAllPages(void);

static void initAllObjectsOnAllPages(void);
static void updatePageObjects(mn_page_t* page);

static void Hu_MenuUpdateCursorState(void);

static boolean Hu_MenuHasCursorRotation(mn_object_t* obj);

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
    { 0, "game-monsters-floatoverblocking" },
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
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { 0, "hud-cheat-counter-show-mapopen" },
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
boolean menuNominatingQuickSaveSlot = false;

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

#if __JDOOM64__
mndata_slider_t sld_hud_viewsize = { 3, 11, 0, 1, false, "view-size" };
#else
mndata_slider_t sld_hud_viewsize = { 3, 13, 0, 1, false, "view-size" };
#endif
mndata_slider_t sld_hud_uptime = { 0, 60, 0, 1.f, true, "hud-timer", "Disabled", NULL, " second", " seconds" };
mndata_slider_t sld_hud_xhair_size = { 0, 1, 0, .1f, true, "view-cross-size" };
mndata_slider_t sld_hud_xhair_angle = { 0, 1, 0, .0625f, true, "view-cross-angle" };
mndata_slider_t sld_hud_xhair_opacity = { 0, 1, 0, .1f, true, "view-cross-a" };
mndata_slider_t sld_hud_size = { 0, 1, 0, .1f, true, "hud-scale" };
mndata_slider_t sld_hud_cntr_size = { 0, 1, 0, .1f, true, "hud-cheat-counter-scale" };
mndata_slider_t sld_hud_sbar_size = { 0, 1, 0, .1f, true, "hud-status-size" };
mndata_slider_t sld_hud_sbar_opacity = { 0, 1, 0, .1f, true, "hud-status-alpha" };
mndata_slider_t sld_hud_msg_size = { 0, 1, 0, .1f, true, "msg-scale" };
mndata_slider_t sld_hud_msg_uptime = { 0, 60, 0, 1.f, true, "msg-uptime", "Disabled", NULL, " second", " seconds" };

mndata_colorbox_t cbox_hud_color = {
    0, 0, 0, 0, 0, 0, true,
    "hud-color-r", "hud-color-g", "hud-color-b", "hud-color-a"
};

mndata_colorbox_t cbox_hud_msg_color = {
    0, 0, 0, 0, 0, 0, false,
    "msg-color-r", "msg-color-g", "msg-color-b"
};

mndata_listitem_t listit_hud_xhair_symbols[] = {
    { "None", 0 },
    { "Cross", 1 },
    { "Twin Angles", 2 },
    { "Square", 3 },
    { "Open Square", 4 },
    { "Angle", 5 }
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
mndata_text_t txt_hud_msg_color = { "Color" };
mndata_text_t txt_hud_msg_uptime = { "Uptime" };

mndata_text_t txt_hud_crosshair = { "Crosshair" };
mndata_text_t txt_hud_xhair_symbol = { "Symbol" };
mndata_text_t txt_hud_xhair_size = { "Size" };
mndata_text_t txt_hud_xhair_angle = { "Angle" };
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
mndata_text_t txt_hud_full_show_status = { "Show Status" };
#endif
mndata_text_t txt_hud_full_show_health = { "Show Health" };
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_text_t txt_hud_full_show_keys = { "Show Keys" };
#endif
#if __JHERETIC__ || __JHEXEN__
mndata_text_t txt_hud_full_show_readyitem = { "Show Ready-Item" };
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_text_t txt_hud_cntr_mapopen = { "Automap Only" };
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
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
mndata_button_t btn_hud_cntr_mapopen = { true, "hud-cheat-counter-show-mapopen" };
#endif

static mn_object_t HudMenuObjects[] = {
    { MN_TEXT,      0,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_view_size },
    { MN_SLIDER,    0,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_viewsize },
#if __JDOOM__
    { MN_TEXT,      0,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_single_key_display },
    { MN_BUTTON,    0,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_single_key_display },
#endif
    { MN_TEXT,      0,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_autohide },
    { MN_SLIDER,    0,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNSlider_Ticker, MNSlider_TextualValueUpdateGeometry, MNSlider_TextualValueDrawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_uptime },

    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR2, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_events },
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_receive_damage },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_receive_damage },
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_health },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_health },
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_armor },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_armor },
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_powerup },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_powerup },
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_weapon },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_weapon },
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_ammo },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_ammo },
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_key },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_key },
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_unhide_pickup_item },
    { MN_BUTTON,    1,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_unhide_pickup_item },
#endif

    { MN_TEXT,      2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR2, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_messages },
    { MN_TEXT,      2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_msg_shown },
    { MN_BUTTON,    2,  0,  { 0, 0 }, 'm',MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_msg_shown },
    { MN_TEXT,      2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_msg_uptime },
    { MN_SLIDER,    2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNSlider_Ticker, MNSlider_TextualValueUpdateGeometry, MNSlider_TextualValueDrawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_msg_uptime },
    { MN_TEXT,      2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_msg_size },
    { MN_SLIDER,    2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_msg_size },
    { MN_TEXT,      2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_msg_color },
    { MN_COLORBOX,  2,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNColorBox_Ticker, MNColorBox_UpdateGeometry, MNColorBox_Drawer, { Hu_MenuCvarColorBox, Hu_MenuCvarColorBox, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_hud_msg_color },

    { MN_TEXT,      3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR2, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_crosshair },
    { MN_TEXT,      3,  0,  { 0, 0 }, 'c',MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_symbol },
    { MN_LISTINLINE, 3, 0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNListInline_Ticker, MNListInline_UpdateGeometry, MNListInline_Drawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNListInline_CommandResponder, NULL, NULL, &list_hud_xhair_symbol },
    { MN_TEXT,      3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_size },
    { MN_SLIDER,    3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_xhair_size },
    { MN_TEXT,      3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_angle },
    { MN_SLIDER,    3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_xhair_angle },
    { MN_TEXT,      3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_opacity },
    { MN_SLIDER,    3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_xhair_opacity },
    { MN_TEXT,      3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_vitality_color },
    { MN_BUTTON,    3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_xhair_vitality_color },
    { MN_TEXT,      3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_xhair_color },
    { MN_COLORBOX,  3,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNColorBox_Ticker, MNColorBox_UpdateGeometry, MNColorBox_Drawer, { Hu_MenuCvarColorBox, Hu_MenuCvarColorBox, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_hud_xhair_color },

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      4,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR2, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_statusbar },
    { MN_TEXT,      4,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_sbar_size },
    { MN_SLIDER,    4,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_sbar_size },
    { MN_TEXT,      4,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_sbar_opacity },
    { MN_SLIDER,    4,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_sbar_opacity },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR2, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_counters },
    { MN_TEXT,      5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_items },
    { MN_LISTINLINE, 5, 0,  { 0, 0 }, 'i',MENU_FONT1, MENU_COLOR3, MNListInline_Ticker, MNListInline_UpdateGeometry, MNListInline_Drawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNListInline_CommandResponder, NULL, NULL, &list_hud_cntr_items },
    { MN_TEXT,      5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_kills },
    { MN_LISTINLINE, 5, 0,  { 0, 0 }, 'k',MENU_FONT1, MENU_COLOR3, MNListInline_Ticker, MNListInline_UpdateGeometry, MNListInline_Drawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNListInline_CommandResponder, NULL, NULL, &list_hud_cntr_kills },
    { MN_TEXT,      5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_secrets },
    { MN_LISTINLINE, 5, 0,  { 0, 0 }, 's',MENU_FONT1, MENU_COLOR3, MNListInline_Ticker, MNListInline_UpdateGeometry, MNListInline_Drawer, { Hu_MenuCvarList, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNListInline_CommandResponder, NULL, NULL, &list_hud_cntr_secrets },
    { MN_TEXT,      5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_mapopen },
    { MN_BUTTON,    5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_cntr_mapopen },
    { MN_TEXT,      5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_cntr_size },
    { MN_SLIDER,    5,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_cntr_size },
#endif

    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR2, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_fullscreen },
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_size },
    { MN_SLIDER,    6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNSlider_Ticker, MNSlider_UpdateGeometry, MNSlider_Drawer, { Hu_MenuCvarSlider, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNSlider_CommandResponder, NULL, NULL, &sld_hud_size },
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_text_color },
    { MN_COLORBOX,  6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNColorBox_Ticker, MNColorBox_UpdateGeometry, MNColorBox_Drawer, { Hu_MenuCvarColorBox, Hu_MenuCvarColorBox, Hu_MenuActivateColorWidget, NULL, NULL, Hu_MenuDefaultFocusAction }, MNColorBox_CommandResponder, NULL, NULL, &cbox_hud_color },
#if __JHEXEN__
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_mana },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_mana },
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_ammo },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 'a',MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_ammo },
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_armor },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 'r',MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_armor },
#endif
#if __JDOOM64__
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_powerkeys },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 'p',MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_powerkeys },
#endif
#if __JDOOM__
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_status },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 'f',MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_face },
#endif
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_health },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 'h',MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_health },
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_keys },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_keys },
#endif
#if __JHERETIC__ || __JHEXEN__
    { MN_TEXT,      6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR1, MNText_Ticker,   MNText_UpdateGeometry, MNText_Drawer, { NULL }, NULL, NULL, NULL, &txt_hud_full_show_readyitem },
    { MN_BUTTON,    6,  0,  { 0, 0 }, 0,  MENU_FONT1, MENU_COLOR3, MNButton_Ticker, MNButton_UpdateGeometry, MNButton_Drawer, { Hu_MenuCvarButton, NULL, NULL, NULL, NULL, Hu_MenuDefaultFocusAction }, MNButton_CommandResponder, NULL, NULL, &btn_hud_full_show_readyitem },
#endif
    { MN_NONE }
};

static boolean inited = false;

typedef struct {
    mn_page_t* page;
    ddstring_t name; // Symbolic name.
} pagerecord_t;

static int pageCount;
static pagerecord_t* pages;

// Cvars for the menu:
cvartemplate_t menuCVars[] = {
    { "menu-scale",     0,  CVT_FLOAT,  &cfg.menuScale, .1f, 1 },
    { "menu-stretch",   0,  CVT_BYTE,   &cfg.menuScaleMode, SCALEMODE_FIRST, SCALEMODE_LAST },
    { "menu-flash-r",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CR], 0, 1 },
    { "menu-flash-g",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CG], 0, 1 },
    { "menu-flash-b",   0,  CVT_FLOAT,  &cfg.menuTextFlashColor[CB], 0, 1 },
    { "menu-flash-speed", 0, CVT_INT,   &cfg.menuTextFlashSpeed, 0, 50 },
    { "menu-cursor-rotate", 0, CVT_BYTE, &cfg.menuCursorRotate, 0, 1 },
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
    { "menu-patch-replacement", 0, CVT_INT, &cfg.menuPatchReplaceMode, PRM_FIRST, PRM_LAST },
    { "menu-slam",      0,  CVT_BYTE,   &cfg.menuSlam,  0, 1 },
    { "menu-hotkeys",   0,  CVT_BYTE,   &cfg.menuShortcutsEnabled, 0, 1 },
#if __JDOOM__ || __JDOOM64__
    { "menu-quitsound", 0,  CVT_INT,    &cfg.menuQuitSound, 0, 1 },
#endif
    { "menu-save-suggestname", 0, CVT_BYTE, &cfg.menuGameSaveSuggestName, 0, 1 },

    // Aliases for obsolete cvars:
    { "menu-turningskull", 0, CVT_BYTE, &cfg.menuCursorRotate, 0, 1 },
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

void Hu_MenuRegister(void)
{
    int i;
    for(i = 0; menuCVars[i].path; ++i)
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

mn_page_t* Hu_MenuFindPageByName(const char* name)
{
    if(name && name[0])
    {
        int i;
        for(i = 0; i < pageCount; ++i)
        {
            pagerecord_t* rec = pages + i;
            if(!stricmp(name, Str_Text(&rec->name)))
            {
                return rec->page;
            }
        }
    }
    return NULL;
}

/// \todo Make this state an object property flag.
/// @return  @c true if the rotation of a cursor on this object should be animated.
static boolean Hu_MenuHasCursorRotation(mn_object_t* obj)
{
    assert(obj);
    return (!(MNObject_Flags(obj) & MNF_DISABLED) &&
              (MNObject_Type(obj) == MN_LISTINLINE || MNObject_Type(obj) == MN_SLIDER));
}

/// To be called to re-evaluate the state of the cursor (e.g., when focus changes).
static void Hu_MenuUpdateCursorState(void)
{
    if(menuActive)
    {
        mn_page_t* page;
        mn_object_t* obj;

        if(colorWidgetActive)
            page = Hu_MenuFindPageByName("ColorWidget");
        else
            page = Hu_MenuActivePage();

        obj = MNPage_FocusObject(page);
        if(obj)
        {
            cursorHasRotation = Hu_MenuHasCursorRotation(obj);
            return;
        }
    }
    cursorHasRotation = false;
}

void Hu_MenuLoadResources(void)
{
    char buffer[9];
    int i;

#if __JDOOM__ || __JDOOM64__
    pMainTitle = R_DeclarePatch("M_DOOM");
#elif __JHERETIC__ || __JHEXEN__
    pMainTitle = R_DeclarePatch("M_HTIC");
#endif

#if __JDOOM__ || __JDOOM64__
    pNewGame  = R_DeclarePatch("M_NEWG");
    pSkill    = R_DeclarePatch("M_SKILL");
    pEpisode  = R_DeclarePatch("M_EPISOD");
    pNGame    = R_DeclarePatch("M_NGAME");
    pOptions  = R_DeclarePatch("M_OPTION");
    pLoadGame = R_DeclarePatch("M_LOADG");
    pSaveGame = R_DeclarePatch("M_SAVEG");
    pReadThis = R_DeclarePatch("M_RDTHIS");
    pQuitGame = R_DeclarePatch("M_QUITG");
    pOptionsTitle = R_DeclarePatch("M_OPTTTL");
#endif

#if __JDOOM__ || __JDOOM64__
    pSkillModeNames[SM_BABY]      = R_DeclarePatch("M_JKILL");
    pSkillModeNames[SM_EASY]      = R_DeclarePatch("M_ROUGH");
    pSkillModeNames[SM_MEDIUM]    = R_DeclarePatch("M_HURT");
    pSkillModeNames[SM_HARD]      = R_DeclarePatch("M_ULTRA");
#  if __JDOOM__
    pSkillModeNames[SM_NIGHTMARE] = R_DeclarePatch("M_NMARE");
#  endif
#endif

#if __JDOOM__
    if(gameModeBits & (GM_DOOM_SHAREWARE|GM_DOOM|GM_DOOM_ULTIMATE))
    {
        pEpisodeNames[0] = R_DeclarePatch("M_EPI1");
        pEpisodeNames[1] = R_DeclarePatch("M_EPI2");
        pEpisodeNames[2] = R_DeclarePatch("M_EPI3");
    }
    if(gameModeBits & GM_DOOM_ULTIMATE)
    {
        pEpisodeNames[3] = R_DeclarePatch("M_EPI4");
    }
#endif

#if __JHERETIC__
    for(i = 0; i < 18; ++i)
    {
        dd_snprintf(buffer, 9, "M_SKL%02d", i);
        pRotatingSkull[i] = R_DeclarePatch(buffer);
    }
#endif

#if __JHEXEN__
    for(i = 0; i < 7; ++i)
    {
        dd_snprintf(buffer, 9, "FBUL%c0", 'A'+i);
        pBullWithFire[i] = R_DeclarePatch(buffer);
    }

    pPlayerClassBG[0] = R_DeclarePatch("M_FBOX");
    pPlayerClassBG[1] = R_DeclarePatch("M_CBOX");
    pPlayerClassBG[2] = R_DeclarePatch("M_MBOX");
#endif

    for(i = 0; i < MENU_CURSOR_FRAMECOUNT; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        dd_snprintf(buffer, 9, "M_SKULL%d", i+1);
#else
        dd_snprintf(buffer, 9, "M_SLCTR%d", i+1);
#endif
        pCursors[i] = R_DeclarePatch(buffer);
    }
}

void Hu_MenuInitColorWidgetPage(void)
{
#if __JHERETIC__ || __JHEXEN__
    const Point2Raw origin = { 98, 60 };
#else
    const Point2Raw origin = { 124, 60 };
#endif
    mn_object_t* objects, *ob;
    const uint numObjects = 10;
    mn_page_t* page;

    page = Hu_MenuNewPage("ColorWidget", &origin, MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, Hu_MenuColorWidgetCmdResponder, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitColorWidgetPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_COLORBOX;
    ob->_flags = MNF_ID0|MNF_NO_FOCUS;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer = MNColorBox_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    { mndata_colorbox_t* cbox = (mndata_colorbox_t*)ob->_typedata;
    cbox->width  = SCREENHEIGHT/7;
    cbox->height = SCREENHEIGHT/7;
    cbox->rgbaMode = true;
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Red";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_flags = MNF_ID1;
    ob->_shortcut = 'r';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2 = CR;
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 1;
    sld->value = 0;
    sld->step = .05f;
    sld->floatMode = true;
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Green";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_flags = MNF_ID2;
    ob->_shortcut = 'g';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2 = CG;
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 1;
    sld->value = 0;
    sld->step = .05f;
    sld->floatMode = true;
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Blue";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_flags = MNF_ID3;
    ob->_shortcut = 'b';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2 = CB;
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 1;
    sld->value = 0;
    sld->step = .05f;
    sld->floatMode = true;
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_flags = MNF_ID4;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Opacity";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_flags = MNF_ID5;
    ob->_shortcut = 'o';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2 = CA;
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 1;
    sld->value = 0;
    sld->step = .05f;
    sld->floatMode = true;
    }
    ob++;

    ob->_type = MN_NONE;

    page->objects = objects;
}

void Hu_MenuInitMainPage(void)
{
#if __JHEXEN__ || __JHERETIC__
    Point2Raw origin = { 110, 56 };
    uint numObjects = 6;
#else
    Point2Raw origin = {  97, 64 };
# if __JDOOM64__
    uint numObjects = 7;
# else
    uint numObjects = 8;
# endif
#endif
    mn_object_t* objects, *ob;
    mn_page_t* page;
    int y;

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        origin.y += 8;
#endif

#if __JDOOM__ || __JDOOM64__
    page = Hu_MenuNewPage("Main", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, NULL, NULL);
#else
    page = Hu_MenuNewPage("Main", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawMainPage, NULL, NULL);
#endif
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTB));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitMainPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;
    y = 0;

#if __JDOOM__ || __JDOOM64__
    ob->_type = MN_TEXT;
    ob->_origin.x = -3;
    ob->_origin.y = -70;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->patch = &pMainTitle;
    }
    ob++;
#endif

    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 'n';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "GameType";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
#if defined(__JDOOM__) && !defined(__JDOOM64__)
    btn->patch = &pNGame;
#else
    btn->text = "New Game";
#endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 'o';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "Options";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
#if defined(__JDOOM__) && !defined(__JDOOM64__)
    btn->patch = &pOptions;
#else
    btn->text = "Options";
#endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

#if __JDOOM__ || __JDOOM64__
    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 'l';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadGame;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
# if __JDOOM64__
    btn->text = "Load Game";
# else
    btn->patch = &pLoadGame;
# endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveGame;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
# if __JDOOM64__
    btn->text = "Save Game";
# else
    btn->patch = &pSaveGame;
# endif
    }
    ob++; y += FIXED_LINE_HEIGHT;
#else
    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 'f';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "Files";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Game Files";
    }
    ob++; y += FIXED_LINE_HEIGHT;
#endif

#if !__JDOOM64__
    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
# if __JDOOM__
    ob->_flags = MNF_ID0;
    ob->_shortcut = 'r';
# else
    ob->_shortcut = 'i';
# endif
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectHelp;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
# if defined(__JDOOM__)
    btn->patch = &pReadThis;
# else
    btn->text = "Info";
# endif
    }
    ob++; y += FIXED_LINE_HEIGHT;
#endif

    ob->_type = MN_BUTTON;
#if __JDOOM__
    ob->_flags = MNF_ID1;
#endif
    ob->_origin.y = y;
    ob->_shortcut = 'q';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectQuitGame;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
#if defined(__JDOOM__) && !defined(__JDOOM64__)
    btn->patch = &pQuitGame;
#else
    btn->text = "Quit Game";
#endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_NONE;

    page->objects = objects;
}

void Hu_MenuInitGameTypePage(void)
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw origin = { 97, 65 };
#else
    Point2Raw origin = { 104, 65 };
#endif
    mn_object_t* objects, *ob;
    const uint numObjects = 3;
    mn_page_t* page;
    int y;

    page = Hu_MenuNewPage("GameType", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawGameTypePage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTB));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Main"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitGameTypeMenu: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;
    y = 0;

    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSingleplayer;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = (const char*)TXT_SINGLEPLAYER;
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 'm';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectMultiplayer;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = (const char*)TXT_MULTIPLAYER;
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_NONE;

    page->objects = objects;
}

void Hu_MenuInitSkillPage(void)
{
#if __JHEXEN__
    const Point2Raw origin = { 120, 44 };
#elif __JHERETIC__
    const Point2Raw origin = { 38, 30 };
#else
    const Point2Raw origin = { 48, 63 };
#endif
    int skillButtonFlags[NUM_SKILL_MODES] = {
        { MNF_ID0 },
        { MNF_ID1 },
        { MNF_ID2|MNF_DEFAULT },
        { MNF_ID3 },
#  if !__JDOOM64__
        { MNF_ID4 }
#  endif
    };
#if !__JHEXEN__
    int skillButtonTexts[NUM_SKILL_MODES] = {
        { TXT_SKILL1 },
        { TXT_SKILL2 },
        { TXT_SKILL3 },
        { TXT_SKILL4 },
#  if !__JDOOM64__
        { TXT_SKILL5 }
#  endif
    };
#endif
    mn_object_t* objects, *ob;
    const uint numObjects = NUM_SKILL_MODES + 1;
    mn_page_t* page;
    int y;

    page = Hu_MenuNewPage("Skill", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawSkillPage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTB));
#if __JHEXEN__
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("PlayerClass"));
#elif __JHERETIC__
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Episode"));
#elif __JDOOM64__
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("GameType"));
#else // __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("GameType"));
    }
    else
    {
        MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Episode"));
    }
#endif

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitSkillPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;
    y = 0;

    { uint i;
    for(i = 0; i < NUM_SKILL_MODES; ++i, ob++, y += FIXED_LINE_HEIGHT)
    {
        ob->_type = MN_BUTTON;
        ob->_flags = skillButtonFlags[i];
#if !__JHEXEN__
        ob->_shortcut = GET_TXT(skillButtonTexts[i])[0];
#endif
        ob->_origin.y = y;
        ob->_pageFontIdx = MENU_FONT1;
        ob->_pageColorIdx = MENU_COLOR1;
        ob->ticker = MNButton_Ticker;
        ob->updateGeometry = MNButton_UpdateGeometry;
        ob->drawer = MNButton_Drawer;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionInitNewGame;
        ob->actions[MNA_FOCUS].callback = Hu_MenuFocusSkillMode;
        ob->cmdResponder = MNButton_CommandResponder;
        ob->data2 = (int)(SM_BABY + i);
        ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
#if !__JHEXEN__
        { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
        btn->text = (const char*) skillButtonTexts[i];
# if __JDOOM__ || __JDOOM64__
        btn->patch = &pSkillModeNames[i];
# endif
        }
#endif
    }}

    ob->_type = MN_NONE;

    page->objects = objects;

#if __JDOOM__
    if(gameMode != doom2_hacx && gameMode != doom_chex)
    {
        mn_object_t* ob = MN_MustFindObjectOnPage(page, 0, MNF_ID4);
        MNButton_SetFlags(ob, FO_SET, MNBUTTON_NO_ALTTEXT);
    }
#endif
}

void Hu_MenuInitMultiplayerPage(void)
{
#if __JHERETIC__ || __JHEXEN__
    const Point2Raw origin = { 97, 65 };
#else
    const Point2Raw origin = { 97, 65 };
#endif
    mn_object_t* objects, *ob;
    const uint numObjects = 3;
    mn_page_t* page;

    page = Hu_MenuNewPage("Multiplayer", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawMultiplayerPage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTB));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("GameType"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitMultiplayerMenu: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_BUTTON;
    ob->_flags = MNF_ID0;
    ob->_shortcut = 'j';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectJoinGame;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Join Game";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerSetup;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Player Setup";
    }
    ob++;

    ob->_type = MN_NONE;

    page->objects = objects;
}

void Hu_MenuInitPlayerSetupPage(void)
{
#if __JHERETIC__ || __JHEXEN__
    const Point2Raw origin = { 70, 54 };
#else
    const Point2Raw origin = { 70, 54 };
#endif
    mn_object_t* objects, *ob;
    mn_page_t* page;
    uint numObjects;

    page = Hu_MenuNewPage("PlayerSetup", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawPlayerSetupPage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPredefinedFont(page, MENU_FONT2, FID(GF_FONTB));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Multiplayer"));

#if __JHEXEN__
    numObjects = 8;
#else
    numObjects = 6;
#endif
    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitPlayerSetupMenu: Failed on allocation of %lu bytes for player setup menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_MOBJPREVIEW;
    ob->_flags = MNF_ID0;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNMobjPreview_Ticker;
    ob->updateGeometry = MNMobjPreview_UpdateGeometry;
    ob->drawer = MNMobjPreview_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_mobjpreview_t), PU_GAMESTATIC, 0);
    ob++;

    ob->_type = MN_EDIT;
    ob->_flags = MNF_ID1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNEdit_Ticker;
    ob->updateGeometry = MNEdit_UpdateGeometry;
    ob->drawer = MNEdit_Drawer;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNEdit_CommandResponder;
    ob->responder = MNEdit_Responder;
    ob->_typedata = Z_Calloc(sizeof(mndata_edit_t), PU_GAMESTATIC, 0);
    { mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    Str_Init(&edit->text);
    Str_Init(&edit->oldtext);
    edit->data1 = "net-name";
    edit->maxLength = 24;
    }
    ob++;

#if __JHEXEN__
    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Class";
    }
    ob++;

    ob->_type = MN_LISTINLINE;
    ob->_flags = MNF_ID2;
    ob->_shortcut = 'c';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuSelectPlayerSetupPlayerClass;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNListInline_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    { mndata_list_t* list = (mndata_list_t*)ob->_typedata;
    list->count = 3;
    list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

    { mndata_listitem_t* item = list->items;
    item->text = (const char*)TXT_PLAYERCLASS1;
    item->data = PCLASS_FIGHTER;
    item++;

    item->text = (const char*)TXT_PLAYERCLASS2;
    item->data = PCLASS_CLERIC;
    item++;

    item->text = (const char*)TXT_PLAYERCLASS3;
    item->data = PCLASS_MAGE;
    }
    }
    ob++;
#endif

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Color";
    }
    ob++;

    // Setup the player color selection list.
    ob->_type = MN_LISTINLINE;
    ob->_flags = MNF_ID3;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuSelectPlayerColor;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNListInline_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    { mndata_list_t* list = (mndata_list_t*)ob->_typedata;
#if __JHEXEN__
    // Hexen v1.0 has only four player colors.
    list->count = (gameMode == hexen_v10? 4 : NUMPLAYERCOLORS) + 1/*auto*/;
#else
    list->count = NUMPLAYERCOLORS + 1/*auto*/;
#endif
    list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

    /// @todo Read these names from Text definitions.
    { mndata_listitem_t* item = list->items;
#if __JHEXEN__
    item->text = "Red";         item->data = 0;
    item++;

    item->text = "Blue";        item->data = 1;
    item++;

    item->text = "Yellow";      item->data = 2;
    item++;

    item->text = "Green";       item->data = 3;
    item++;

    // Hexen v1.0 has only four player colors.
    if(gameMode != hexen_v10)
    {
        item->text = "Jade";    item->data = 4;
        item++;

        item->text = "White";   item->data = 5;
        item++;

        item->text = "Hazel";   item->data = 6;
        item++;

        item->text = "Purple";  item->data = 7;
        item++;
    }

    item->text = "Automatic";   item->data = 8;
#elif __JHERETIC__
    item->text = "Green";       item->data = 0;
    item++;

    item->text = "Orange";      item->data = 1;
    item++;

    item->text = "Red";         item->data = 2;
    item++;

    item->text = "Blue";        item->data = 3;
    item++;

    item->text = "Automatic";   item->data = 4;
#else
    item->text = "Green";       item->data = 0;
    item++;

    item->text = "Indigo";      item->data = 1;
    item++;

    item->text = "Brown";       item->data = 2;
    item++;

    item->text = "Red";         item->data = 3;
    item++;

    item->text = "Automatic";   item->data = 4;
#endif
    }
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT2;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectAcceptPlayerSetup;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Save Changes";
    }
    ob++;

    ob->_type = MN_NONE;

    page->objects = objects;
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitFilesPage(void)
{
    Point2Raw origin = { 110, 60 };
    mn_object_t* objects, *ob;
    const uint numObjects = 3;
    mn_page_t* page;
    int y;

    page = Hu_MenuNewPage("Files", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTB));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Main"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitFilesMenu: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;
    y = 0;

    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 'l';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadGame;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Load Game";
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_BUTTON;
    ob->_origin.y = y;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveGame;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Save Game";
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_NONE;

    page->objects = objects;
}
#endif

void Hu_MenuInitLoadGameAndSaveGamePages(void)
{
#if __JDOOM__ || __JDOOM64__
    const Point2Raw origin = { 80, 54 };
#else
    const Point2Raw origin = { 70, 30 };
#endif
    mn_object_t* loadMenuObjects, *saveMenuObjects;
    mndata_edit_t* saveSlots;
    mn_page_t* page;
    const int saveSlotObjectIds[NUMSAVESLOTS] = {
        MNF_ID0, MNF_ID1, MNF_ID2, MNF_ID3, MNF_ID4, MNF_ID5,
#if !__JHEXEN__
        MNF_ID6, MNF_ID7
#endif
    };
    int i, y;

    saveSlots = Z_Calloc(sizeof(*saveSlots) * NUMSAVESLOTS, PU_GAMESTATIC, 0);
    if(!saveSlots) Con_Error("Hu_MenuInitLoadGameAndSaveGamePages: Failed on allocation of %lu bytes for load/save menu edit fields.", (unsigned long) (sizeof(*saveSlots) * NUMSAVESLOTS));

    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        mndata_edit_t* slot = saveSlots + i;
        slot->emptyString = (const char*) TXT_EMPTYSTRING;
        slot->data2 = i;
        slot->maxLength = 24;
    }

    loadMenuObjects = Z_Calloc(sizeof(*loadMenuObjects) * (NUMSAVESLOTS+1), PU_GAMESTATIC, 0);
    if(!loadMenuObjects) Con_Error("Hu_MenuInitLoadGameAndSaveGamePages: Failed on allocation of %lu bytes for load menu objects.", (unsigned long) (sizeof(*loadMenuObjects) * (NUMSAVESLOTS+1)));

    y = 0;
    for(i = 0; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        mn_object_t* ob = loadMenuObjects + i;
        mndata_edit_t* edit = saveSlots + i;
        ob->_type = MN_EDIT;
        ob->_origin.x = 0;
        ob->_origin.y = y;
        ob->_flags = saveSlotObjectIds[i] | MNF_DISABLED;
        ob->_shortcut = '0' + i;
        ob->_pageFontIdx = MENU_FONT1;
        ob->_pageColorIdx = MENU_COLOR1;
        ob->updateGeometry = MNEdit_UpdateGeometry;
        ob->drawer = MNEdit_Drawer;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadSlot;
        ob->actions[MNA_FOCUSOUT].callback = Hu_MenuDefaultFocusAction;
        ob->cmdResponder = MNObject_DefaultCommandResponder;
        ob->_typedata = edit;
        ob->data2 = saveSlotObjectIds[i];
        Str_Init(&edit->text);
        Str_Init(&edit->oldtext);
    }
    loadMenuObjects[i]._type = MN_NONE;

    saveMenuObjects = Z_Calloc(sizeof(*saveMenuObjects) * (NUMSAVESLOTS+1), PU_GAMESTATIC, 0);
    if(!saveMenuObjects) Con_Error("initAllPages: Failed on allocation of %lu bytes for save menu objects.", (unsigned long) (sizeof(*saveMenuObjects) * (NUMSAVESLOTS+1)));

    y = 0;
    for(i = 0; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        mn_object_t* ob = saveMenuObjects + i;
        mndata_edit_t* edit = saveSlots + i;
        ob->_type = MN_EDIT;
        ob->_origin.x = 0;
        ob->_origin.y = y;
        ob->_flags = saveSlotObjectIds[i];
        ob->_shortcut = '0' + i;
        ob->_pageFontIdx = MENU_FONT1;
        ob->_pageColorIdx = MENU_COLOR1;
        ob->updateGeometry = MNEdit_UpdateGeometry;
        ob->drawer = MNEdit_Drawer;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveSlot;
        ob->actions[MNA_ACTIVE].callback = Hu_MenuSaveSlotEdit;
        ob->actions[MNA_FOCUSOUT].callback = Hu_MenuDefaultFocusAction;
        ob->cmdResponder = MNEdit_CommandResponder;
        ob->responder = MNEdit_Responder;
        ob->_typedata = edit;
        ob->data2 = saveSlotObjectIds[i];
    }
    saveMenuObjects[i]._type = MN_NONE;

    page = Hu_MenuNewPage("LoadGame", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawLoadGamePage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Main"));
    page->objects = loadMenuObjects;

    page = Hu_MenuNewPage("SaveGame", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawSaveGamePage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Main"));
    page->objects = saveMenuObjects;
}

void Hu_MenuInitOptionsPage(void)
{
#if __JHERETIC__ || __JHEXEN__
    const Point2Raw origin = { 110, 63 };
#else
    const Point2Raw origin = { 110, 63 };
#endif
    mn_object_t* objects, *ob;
#if __JHERETIC__ || __JHEXEN__
    const uint numObjects = 12;
#else
    const uint numObjects = 11;
#endif
    mn_page_t* page;

    page = Hu_MenuNewPage("Options", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawOptionsPage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Main"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitOptionsPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'e';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectEndGame;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "End Game";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'p';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Control Panel";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'c';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "ControlOptions";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Controls";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'g';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "GameplayOptions";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Gameplay";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'h';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "HUDOptions";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "HUD";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'a';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "AutomapOptions";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Automap";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'w';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "WeaponOptions";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Weapons";
    }
    ob++;

#if __JHERETIC__ || __JHEXEN__
    ob->_type = MN_BUTTON;
    ob->_shortcut = 'i';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "InventoryOptions";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Inventory";
    }
    ob++;
#endif

    ob->_type = MN_BUTTON;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data1 = "SoundOptions";
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Sound";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'm';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data2 = 2;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Mouse";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'j';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->data2 = 2;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Joystick";
    }
    ob++;

    ob->_type = MN_NONE;

    page->objects = objects;
}

void Hu_MenuInitGameplayOptionsPage(void)
{
#if __JHEXEN__
    const Point2Raw origin = { 88, 25 };
#elif __JHERETIC__
    const Point2Raw origin = { 30, 40 };
#else
    const Point2Raw origin = { 30, 40 };
#endif
    mn_object_t* objects, *ob;
#if __JDOOM64__
    const uint numObjects = 38;
#elif __JDOOM__
    const uint numObjects = 38;
#elif __JHERETIC__
    const uint numObjects = 24;
#elif __JHEXEN__
    const uint numObjects = 7;
#endif
    mn_page_t* page;

    page = Hu_MenuNewPage("GameplayOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    MNPage_SetTitle(page, "Gameplay Options");
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Options"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitGameplayOptionsPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Always Run";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'r';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "ctl-run";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Use LookSpring";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'l';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "ctl-look-spring";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Disable AutoAim";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'a';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "ctl-aim-noauto";
    }
    ob++;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Allow Jumping";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'j';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "player-jump";
    }
    ob++;
#endif

#if __JDOOM64__
    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Weapon Recoil";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "player-weapon-recoil";
    }
    ob++;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR2;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Compatibility";
    }
    ob++;

# if __JDOOM__ || __JDOOM64__
    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Any Boss Trigger 666";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'b';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-anybossdeath666";
    }
    ob++;

#  if !__JDOOM64__
    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Av Resurrects Ghosts";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'g';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-raiseghosts";
    }
    ob++;
#  endif // !__JDOOM64__

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "PE Limited To 21 Lost Souls";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'p';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-maxskulls";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "LS Can Get Stuck Inside Walls";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-skullsinwalls";
    }
    ob++;
# endif // __JDOOM__ || __JDOOM64__

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Monsters Fly Over Obstacles";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-monsters-floatoverblocking";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Monsters Can Get Stuck In Doors";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'd';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-monsters-stuckindoors";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Some Objects Never Hang Over Ledges";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'h';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-objects-neverhangoverledges";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Objects Fall Under Own Weight";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'f';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-objects-falloff";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Corpses Slide Down Stairs";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-corpse-sliding";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Use Exactly Doom's Clipping Code";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'c';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-objects-clipping";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "  ^If Not NorthOnly WallRunning";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'w';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-player-wallrun-northonly";
    }
    ob++;

# if __JDOOM__ || __JDOOM64__
    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Zombie Players Can Exit Maps";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'e';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "game-zombiescanexit";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Fix Ouch Face";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "hud-face-ouchfix";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Fix Weapon Slot Display";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "hud-status-weaponslots-ownedfix";
    }
    ob++;
# endif // __JDOOM__ || __JDOOM64__
#endif // __JDOOM__ || __JHERETIC__ || __JDOOM64__

    ob->_type = MN_NONE;

    page->objects = objects;
}

void Hu_MenuInitHUDOptionsPage(void)
{
#if __JDOOM__ || __JDOOM64__
    const Point2Raw origin = { 97, 40 };
#else
    const Point2Raw origin = { 97, 28 };
#endif
    mn_page_t* page;

    page = Hu_MenuNewPage("HudOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    MNPage_SetTitle(page, "HUD Options");
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Options"));
    page->objects = HudMenuObjects;
}

void Hu_MenuInitAutomapOptionsPage(void)
{
#if __JHERETIC__ || __JHEXEN__
    const Point2Raw origin = { 64, 28 };
#else
    const Point2Raw origin = { 70, 40 };
#endif
    mn_object_t* objects, *ob;
#if __JDOOM64__
    const uint numObjects = 26;
#else
    const uint numObjects = 27;
#endif
    mn_page_t* page;

    page = Hu_MenuNewPage("AutomapOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    MNPage_SetTitle(page, "Automap Options");
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Options"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitAutomapOptionsPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Background Opacity";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_shortcut = 'o';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 1;
    sld->value = 0;
    sld->step = 0.1f;
    sld->floatMode = true;
    sld->data1 = "map-opacity";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Line Opacity";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_shortcut = 'l';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 1;
    sld->value = 0;
    sld->step = 0.1f;
    sld->floatMode = true;
    sld->data1 = "map-line-opacity";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Line Width";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = .1f;
    sld->max = 2;
    sld->value = 0;
    sld->step = 0.1f;
    sld->floatMode = true;
    sld->data1 = "map-line-width";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "HUD Display";
    }
    ob++;

#if !__JDOOM64__
    ob->_type = MN_LISTINLINE;
    ob->_shortcut = 'h';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNListInline_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    { mndata_list_t* list = (mndata_list_t*)ob->_typedata;
    mndata_listitem_t* item;

    list->count = 3;
    item = list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

    item->text = "None";
    item->data = 0;
    item++;

    item->text = "Current";
    item->data = 1;
    item++;

    item->text = "Statusbar";
    item->data = 2;
    }
    ob++;
#endif

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Door Colors";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'd';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "map-door-colors";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Door Glow";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_shortcut = 'g';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 200;
    sld->value = 0;
    sld->step = 5;
    sld->floatMode = true;
    sld->data1 = "map-door-glow";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Use Custom Colors";
    }
    ob++;

    ob->_type = MN_LISTINLINE;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNListInline_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    { mndata_list_t* list = (mndata_list_t*)ob->_typedata;
    mndata_listitem_t* item;

    list->count = 3;
    item = list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

    item->text = "Never";
    item->data = 0;
    item++;

    item->text = "Auto";
    item->data = 1;
    item++;

    item->text = "Always";
    item->data = 2;
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Wall";
    }
    ob++;

    ob->_type = MN_COLORBOX;
    ob->_shortcut = 'w';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNColorBox_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    { mndata_colorbox_t* cbox = (mndata_colorbox_t*)ob->_typedata;
    cbox->data1 = "map-wall-r";
    cbox->data2 = "map-wall-g";
    cbox->data3 = "map-wall-b";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Floor Height Change";
    }
    ob++;

    ob->_type = MN_COLORBOX;
    ob->_shortcut = 'f';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNColorBox_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    { mndata_colorbox_t* cbox = (mndata_colorbox_t*)ob->_typedata;
    cbox->data1 = "map-wall-floorchange-r";
    cbox->data2 = "map-wall-floorchange-g";
    cbox->data3 = "map-wall-floorchange-b";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Ceiling Height Change";
    }
    ob++;

    ob->_type = MN_COLORBOX;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNColorBox_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    { mndata_colorbox_t* cbox = (mndata_colorbox_t*)ob->_typedata;
    cbox->data1 = "map-wall-ceilingchange-r";
    cbox->data2 = "map-wall-ceilingchange-g";
    cbox->data3 = "map-wall-ceilingchange-b";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Unseen";
    }
    ob++;

    ob->_type = MN_COLORBOX;
    ob->_shortcut = 'u';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNColorBox_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    { mndata_colorbox_t* cbox = (mndata_colorbox_t*)ob->_typedata;
    cbox->data1 = "map-wall-unseen-r";
    cbox->data2 = "map-wall-unseen-g";
    cbox->data3 = "map-wall-unseen-b";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Thing";
    }
    ob++;

    ob->_type = MN_COLORBOX;
    ob->_shortcut = 't';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNColorBox_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    { mndata_colorbox_t* cbox = (mndata_colorbox_t*)ob->_typedata;
    cbox->data1 = "map-mobj-r";
    cbox->data2 = "map-mobj-g";
    cbox->data3 = "map-mobj-b";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Background";
    }
    ob++;

    ob->_type = MN_COLORBOX;
    ob->_shortcut = 'b';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNColorBox_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    { mndata_colorbox_t* cbox = (mndata_colorbox_t*)ob->_typedata;
    cbox->data1 = "map-background-r";
    cbox->data2 = "map-background-g";
    cbox->data3 = "map-background-b";
    }
    ob++;

    ob->_type = MN_NONE;

    page->objects = objects;
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
    } while(!(aIndex != -1 && bIndex != -1) && ++i < NUM_WEAPON_TYPES);

    if(aIndex > bIndex) return 1;
    if(aIndex < bIndex) return -1;
    return 0; // Should never happen.
}

void Hu_MenuInitWeaponsPage(void)
{
#if __JDOOM__ || __JDOOM64__
    const Point2Raw origin = { 78, 40 };
#elif __JHERETIC__
    const Point2Raw origin = { 78, 26 };
#elif __JHEXEN__
    const Point2Raw origin = { 78, 38 };
#endif
    mn_object_t* objects, *ob;
#if __JDOOM__ || __JDOOM64__
    const uint numObjects = 17;
#elif __JHERETIC__ || __JHEXEN__
    const uint numObjects = 15;
#endif
    const struct {
        const char* text;
        int data;
    } weaponOrder[NUM_WEAPON_TYPES+1] = {
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
        { (const char*)TXT_WEAPON10,            WT_TENTH },
#  endif
#elif __JHERETIC__
        { (const char*)TXT_TXT_WPNSTAFF,        WT_FIRST },
        { (const char*)TXT_TXT_WPNWAND,         WT_SECOND },
        { (const char*)TXT_TXT_WPNCROSSBOW,     WT_THIRD },
        { (const char*)TXT_TXT_WPNBLASTER,      WT_FOURTH },
        { (const char*)TXT_TXT_WPNSKULLROD,     WT_FIFTH },
        { (const char*)TXT_TXT_WPNPHOENIXROD,   WT_SIXTH },
        { (const char*)TXT_TXT_WPNMACE,         WT_SEVENTH },
        { (const char*)TXT_TXT_WPNGAUNTLETS,    WT_EIGHTH },
#elif __JHEXEN__
        /**
         * @todo We should allow different weapon preferences per player-class.
         */
        { "First",  WT_FIRST },
        { "Second", WT_SECOND },
        { "Third",  WT_THIRD },
        { "Fourth", WT_FOURTH },
#endif
        { 0 }
    };
    mn_page_t* page;

    page = Hu_MenuNewPage("WeaponOptions", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawWeaponsPage, NULL, NULL);
    MNPage_SetTitle(page, "Weapons Options");
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Options"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitWeaponOptionsPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR2;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Priority Order";
    }
    ob++;

    ob->_type = MN_LIST;
    ob->_flags = MNF_ID0;
    ob->_shortcut = 'p';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNList_Ticker;
    ob->updateGeometry = MNList_UpdateGeometry;
    ob->drawer = MNList_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuChangeWeaponPriority;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNList_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    { mndata_list_t* list = (mndata_list_t*)ob->_typedata;
    mndata_listitem_t* item;
    uint i;

    list->count = NUM_WEAPON_TYPES;
    item = list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

    for(i = 0; weaponOrder[i].text; ++i, item++)
    {
        item->text = weaponOrder[i].text;
        item->data = weaponOrder[i].data;
    }
    qsort(list->items, list->count, sizeof(mndata_listitem_t), compareWeaponPriority);
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR2;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Cycling";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Use Priority Order";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'o';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "player-weapon-nextmode";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Sequential";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "player-weapon-cycle-sequential";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 2;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR2;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Autoswitch";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 2;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Pickup Weapon";
    }
    ob++;

    ob->_type = MN_LISTINLINE;
    ob->_group = 2;
    ob->_shortcut = 'w';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNListInline_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    { mndata_list_t* list = (mndata_list_t*)ob->_typedata;
    mndata_listitem_t* item;

    list->count = 3;
    item = list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

    item->text = "Never";
    item->data = 0;
    item++;

    item->text = "If Better";
    item->data = 1;
    item++;

    item->text = "Always";
    item->data = 2;
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 2;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "   If Not Firing";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 2;
    ob->_shortcut = 'f';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "player-autoswitch-notfiring";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 2;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Pickup Ammo";
    }
    ob++;

    ob->_type = MN_LISTINLINE;
    ob->_group = 2;
    ob->_shortcut = 'a';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNListInline_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    { mndata_list_t* list = (mndata_list_t*)ob->_typedata;
    mndata_listitem_t* item;

    list->count = 3;
    item = list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

    item->text = "Never";
    item->data = 0;
    item++;

    item->text = "If Better";
    item->data = 1;
    item++;

    item->text = "Always";
    item->data = 2;
    }
    ob++;

#if __JDOOM__ || __JDOOM64__
    ob->_type = MN_TEXT;
    ob->_group = 2;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Pickup Beserk";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 2;
    ob->_shortcut = 'b';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "player-autoswitch-berserk";
    }
    ob++;
#endif

    ob->_type = MN_NONE;

    page->objects = objects;
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitInventoryOptionsPage(void)
{
    const Point2Raw origin = { 78, 48 };
    mn_object_t* objects, *ob;
    const uint numObjects = 16;
    mn_page_t* page;

    page = Hu_MenuNewPage("InventoryOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    MNPage_SetTitle(page, "Inventory Options");
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Options"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);

    if(!objects) Con_Error("Hu_MenuInitInventoryPage: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));
    ob = objects;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Select Mode";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "ctl-inventory-mode";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Wrap Around";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'w';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "ctl-inventory-wrap";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Choose And Use";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'c';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "ctl-inventory-use-immediate";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Select Next If Use Failed";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'n';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "ctl-inventory-use-next";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "AutoHide";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_shortcut = 'h';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_TextualValueUpdateGeometry;
    ob->drawer = MNSlider_TextualValueDrawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 30;
    sld->value = 0;
    sld->step = 1.f;
    sld->floatMode = true;
    sld->data1 = "hud-inventory-timer";
    sld->data2 = "Disabled";
    sld->data4 = " second";
    sld->data5 = " seconds";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR2;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Fullscreen HUD";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Max Visible Slots";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_group = 1;
    ob->_shortcut = 'v';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_TextualValueUpdateGeometry;
    ob->drawer = MNSlider_TextualValueDrawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 16;
    sld->value = 0;
    sld->step = 1;
    sld->floatMode = false;
    sld->data1 = "hud-inventory-slot-max";
    sld->data2 = "Automatic";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_group = 1;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Show Empty Slots";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_group = 1;
    ob->_shortcut = 'e';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR3;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->staydownMode = true;
    btn->data = "hud-inventory-slot-showempty";
    }
    ob++;

    ob->_type = MN_NONE;

    page->objects = objects;
}
#endif

void Hu_MenuInitSoundOptionsPage(void)
{
#if __JHEXEN__
    const Point2Raw origin = { 97, 25 };
#elif __JHERETIC__
    const Point2Raw origin = { 97, 30 };
#elif __JDOOM__ || __JDOOM64__
    const Point2Raw origin = { 97, 40 };
#endif
    mn_object_t* objects, *ob;
    const uint numObjects = 6;
    mn_page_t* page;

    page = Hu_MenuNewPage("SoundOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    MNPage_SetTitle(page, "Sound Options");
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Options"));

    objects = Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    if(!objects) Con_Error("Hu_MenuInitFilesMenu: Failed on allocation of %lu bytes for menu objects.", (unsigned long) (sizeof(*objects) * numObjects));

    ob = objects;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "SFX Volume";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_shortcut = 's';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 255;
    sld->value = 0;
    sld->step = 5;
    sld->floatMode = false;
    sld->data1 = "sound-volume";
    }
    ob++;

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer = MNText_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    { mndata_text_t* text = (mndata_text_t*)ob->_typedata;
    text->text = "Music Volume";
    }
    ob++;

    ob->_type = MN_SLIDER;
    ob->_shortcut = 'm';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNSlider_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    { mndata_slider_t* sld = (mndata_slider_t*)ob->_typedata;
    sld->min = 0;
    sld->max = 255;
    sld->value = 0;
    sld->step = 5;
    sld->floatMode = false;
    sld->data1 = "music-volume";
    }
    ob++;

    ob->_type = MN_BUTTON;
    ob->_shortcut = 'p';
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    ob->data2 = 1;
    { mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    btn->text = "Open Audio Panel";
    }
    ob++;

    ob->_type = MN_NONE;

    page->objects = objects;
}

#if __JDOOM__ || __JHERETIC__
/**
 * Construct the episode selection menu.
 */
void Hu_MenuInitEpisodePage(void)
{
#if __JDOOM__
    const Point2Raw origin = { 48, 63 };
#else
    const Point2Raw origin = { 80, 50 };
#endif
    int i, y, numEpisodes;
    mn_object_t* objects, *ob;
    mndata_button_t* buttons, *btn;
    mn_page_t* page;

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
    objects = Z_Calloc(sizeof(mn_object_t) * (numEpisodes+1), PU_GAMESTATIC, 0);
    buttons = Z_Calloc(sizeof(mndata_button_t) * (numEpisodes), PU_GAMESTATIC, 0);

    ob = objects;
    btn = buttons;
    y = 0;
    for(i = 0; i < numEpisodes; ++i)
    {
        ob->_type = MN_BUTTON;
        ob->_origin.x = 0;
        ob->_origin.y = y;
        btn->text = GET_TXT(TXT_EPISODE1 + i);
        if(isalnum(btn->text[0]))
            ob->_shortcut = tolower(btn->text[0]);
#if __JDOOM__
        btn->patch = &pEpisodeNames[i];
#endif
        ob->_typedata = btn;
        ob->ticker = MNButton_Ticker;
        ob->drawer = MNButton_Drawer;
        ob->cmdResponder = MNButton_CommandResponder;
        ob->updateGeometry = MNButton_UpdateGeometry;

        if(i != 0
#if __JHERETIC__
           && gameMode == heretic_shareware
#else
           && gameMode == doom_shareware
#endif
           )
        {
            ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActivateNotSharewareEpisode;
        }
        else
        {
            ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
            ob->data1 = "Skill";
#if __JHERETIC__
            if(gameMode == heretic_extended && i == 5)
            {
                ob->_flags |= MNF_ID0;
            }
#endif
        }

        ob->actions[MNA_FOCUS].callback = Hu_MenuFocusEpisode;
        ob->data2 = i;
        ob->_pageFontIdx = MENU_FONT1;
        ob++;
        btn++;
        y += FIXED_LINE_HEIGHT;
    }
    ob->_type = MN_NONE;

    page = Hu_MenuNewPage("Episode", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawEpisodePage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTB));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("GameType"));

    page->objects = objects;
}
#endif

#if __JHEXEN__
/**
 * Construct the player class selection menu.
 */
void Hu_MenuInitPlayerClassPage(void)
{
    const Point2Raw pageOrigin = { 66, 66 };
    mndata_button_t* buttons, *btn;
    uint i, n, count;
    mn_object_t* objects, *ob;
    mn_page_t* page;
    int y;

    // First determine the number of selectable player classes.
    count = 0;
    for(i = 0; i < NUM_PLAYER_CLASSES; ++i)
    {
        classinfo_t* info = PCLASS_INFO(i);
        if(info->userSelectable)
            ++count;
    }

    // Allocate the menu objects.
    objects = Z_Calloc(sizeof(mn_object_t) * (count+4), PU_GAMESTATIC, 0);
    buttons = Z_Calloc(sizeof(mndata_button_t) * (count+1), PU_GAMESTATIC, 0);

    // Add the selectable classes.
    y = 0;
    n = 0;
    ob = objects;
    btn = buttons;
    while(n < count)
    {
        classinfo_t* info = PCLASS_INFO(n++);

        if(!info->userSelectable) continue;

        ob->_type = MN_BUTTON;
        btn->text = info->niceName;
        ob->_typedata = btn;
        ob->_origin.x = 0;
        ob->_origin.y = y;
        ob->drawer = MNButton_Drawer;
        ob->ticker = MNButton_Ticker;
        ob->cmdResponder = MNButton_CommandResponder;
        ob->updateGeometry = MNButton_UpdateGeometry;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
        ob->actions[MNA_FOCUS].callback = Hu_MenuFocusOnPlayerClass;
        ob->data2 = (int)info->plrClass;
        ob->_shortcut = tolower(btn->text[0]);
        ob->_pageFontIdx = MENU_FONT1;
        ob->_pageColorIdx = MENU_COLOR1;
        ob++;
        btn++;
        y += FIXED_LINE_HEIGHT;
    }

    // Random class button.
    ob->_type = MN_BUTTON;
    btn->text = GET_TXT(TXT_RANDOMPLAYERCLASS);
    ob->_typedata = btn;
    ob->_origin.x = 0;
    ob->_origin.y = y;
    ob->drawer = MNButton_Drawer;
    ob->ticker = MNButton_Ticker;
    ob->cmdResponder = MNButton_CommandResponder;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
    ob->actions[MNA_FOCUS].callback = Hu_MenuFocusOnPlayerClass;
    ob->data2 = (int)PCLASS_NONE;
    ob->_shortcut = tolower(btn->text[0]);
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob++;

    // Mobj preview background.
    ob->_type = MN_RECT;
    ob->_flags = MNF_NO_FOCUS|MNF_ID1;
    ob->_origin.x = 108;
    ob->_origin.y = -58;
    ob->drawer = MNRect_Drawer;
    ob->ticker = Hu_MenuPlayerClassBackgroundTicker;
    ob->updateGeometry = MNRect_UpdateGeometry;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->_typedata = Z_Calloc(sizeof(mndata_rect_t), PU_GAMESTATIC, 0);
    ob++;

    // Mobj preview.
    ob->_type = MN_MOBJPREVIEW;
    ob->_flags = MNF_ID0;
    ob->_origin.x = 108 + 55;
    ob->_origin.y = -58 + 76;
    ob->ticker = Hu_MenuPlayerClassPreviewTicker;
    ob->updateGeometry = MNMobjPreview_UpdateGeometry;
    ob->drawer = MNMobjPreview_Drawer;
    ob->_typedata = Z_Calloc(sizeof(mndata_mobjpreview_t), PU_GAMESTATIC, 0);
    ob++;

    // Terminate.
    ob->_type = MN_NONE;

    page = Hu_MenuNewPage("PlayerClass", &pageOrigin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawPlayerClassPage, NULL, NULL);
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTB));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("GameType"));

    page->objects = objects;
}
#endif

mn_page_t* MNPage_New(const Point2Raw* origin, int flags,
    void (*ticker) (struct mn_page_s* page),
    void (*drawer) (struct mn_page_s* page, const Point2Raw* origin),
    int (*cmdResponder) (struct mn_page_s* page, menucommand_e cmd),
    void* userData)
{
    mn_page_t* page = (mn_page_t*) malloc(sizeof(*page));
    fontid_t fontId;
    int i;

    if(!page) Con_Error("MNPage::New: Failed on allocation of %lu bytes for new MenuPage.", (unsigned long) sizeof(*page));

    page->origin.x = origin? origin->x : 0;
    page->origin.y = origin? origin->y : 0;

    page->flags = flags;

    page->objects = NULL;
    page->objectsCount = 0;

    page->ticker = ticker;
    page->drawer = drawer;
    page->cmdResponder = cmdResponder;
    page->previous = NULL;
    page->userData = userData;

    Str_Init(&page->title);

    fontId = FID(GF_FONTA);
    for(i = 0; i < MENU_FONT_COUNT; ++i)
        page->fonts[i] = fontId;

    memset(page->colors, 0, sizeof(page->colors));
    page->colors[0] = 0;
    page->colors[1] = 1;
    page->colors[2] = 2;

    page->focus = -1; /// @todo Make this a page flag.
    page->geometry = Rect_New();

    return page;
}

static mn_page_t* addPageToCollection(mn_page_t* page, const char* name)
{
    if(page)
    {
        pagerecord_t* rec;

        pages = (pagerecord_t*)realloc(pages, sizeof(*pages) * ++pageCount);
        if(!pages) Con_Error("addPageToCollection: Failed on (re)allocation of %lu bytes enlarging Pages collection.", (unsigned long) sizeof(*pages) * pageCount);

        rec = &pages[pageCount-1];
        rec->page = page;
        Str_Init(&rec->name); Str_Set(&rec->name, name);
    }
    return page;
}

mn_page_t* Hu_MenuNewPage(const char* name, const Point2Raw* origin, int flags,
    void (*ticker) (struct mn_page_s* page),
    void (*drawer) (struct mn_page_s* page, const Point2Raw* origin),
    int (*cmdResponder) (struct mn_page_s* page, menucommand_e cmd),
    void* userData)
{
    if(!name || !name[0])
    {
#if _DEBUG
        Con_Message("Warning: Hu_MenuNewPage: Attempt to create page with invalid name '%s', ignoring.\n", name);
#endif
        return NULL;
    }

    return addPageToCollection(MNPage_New(origin, flags, ticker, drawer, cmdResponder, userData), name);
}

void Hu_MenuInit(void)
{
    cvarbutton_t* cvb;

    if(inited) return;

    pageCount = 0;
    pages = NULL;

    mnAlpha = mnTargetAlpha = 0;
    menuActivePage = NULL;
    menuActive = false;
    cursorHasRotation = false;
    cursorAngle = 0;
    cursorAnimFrame = 0;
    cursorAnimCounter = MENU_CURSOR_TICSPERFRAME;

    DD_Execute(true, "deactivatebcontext menu");

    Hu_MenuLoadResources();

    // Set default Yes/No strings.
    for(cvb = mnCVarButtons; cvb->cvarname; cvb++)
    {
        if(!cvb->yes) cvb->yes = "Yes";
        if(!cvb->no) cvb->no = "No";
    }

    initAllPages();
    initAllObjectsOnAllPages();

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        mn_object_t* ob = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("Main"), 0, MNF_ID0); // Read This!
        MNObject_SetFlags(ob, FO_SET, MNF_DISABLED|MNF_HIDDEN|MNF_NO_FOCUS);

        ob = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("Main"), 0, MNF_ID1); // Quit Game
        MNObject_SetFixedY(ob, MNObject_FixedY(ob) - FIXED_LINE_HEIGHT);
    }
#endif

    inited = true;
}

void Hu_MenuShutdown(void)
{
    if(!inited) return;

    destroyAllPages();
    inited = false;
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
#define MENUALPHA_FADE_STEP (.07f)

    float diff = 0;

    // Move towards the target alpha level for the entire menu.
    diff = mnTargetAlpha - mnAlpha;
    if(fabs(diff) > MENUALPHA_FADE_STEP)
    {
        mnAlpha += (float)(MENUALPHA_FADE_STEP * ticLength * TICRATE * (diff > 0? 1 : -1));
    }
    else
    {
        mnAlpha = mnTargetAlpha;
    }

    if(!menuActive) return;

    // Animate cursor rotation?
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

    // Time to think? Updates on 35Hz game ticks.
    if(!DD_IsSharpTick()) return;

    // Advance menu time.
    menuTime++;

    // Animate the cursor graphic?
    if(--cursorAnimCounter <= 0)
    {
        cursorAnimFrame++;
        cursorAnimCounter = MENU_CURSOR_TICSPERFRAME;
        if(cursorAnimFrame > MENU_CURSOR_FRAMECOUNT-1)
            cursorAnimFrame = 0;
    }

    // Used for Heretic's rotating skulls.
    frame = (menuTime / 3) % 18;

    // Call the active page's ticker.
    menuActivePage->ticker(menuActivePage);

#undef MENUALPHA_FADE_STEP
}

mn_page_t* Hu_MenuActivePage(void)
{
    return menuActivePage;
}

void Hu_MenuSetActivePage(mn_page_t* page)
{
    if(!menuActive) return;
    if(!page) return;

    if(!(Get(DD_DEDICATED) || Get(DD_NOVIDEO)))
    {
        FR_ResetTypeinTimer();
    }

    cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
    menuNominatingQuickSaveSlot = false;

    if(menuActivePage == page) return;

    updatePageObjects(page);

    // This is now the "active" page.
    menuActivePage = page;
    MNPage_Initialize(page);
}

boolean Hu_MenuIsVisible(void)
{
    return (menuActive || mnAlpha > .0001f);
}

int Hu_MenuDefaultFocusAction(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(parameters);
    if(MNA_FOCUS != action) return 1;
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

    scale = MIN_OF((float) (focusObjectHeight * 1.267f) / info.geometry.size.height, 1);
    pos[VX] = x + OFFSET_X * scale;
    pos[VY] = y + OFFSET_Y * scale + focusObjectHeight/2;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(pos[VX], pos[VY], 0);
    DGL_Scalef(scale, scale, 1);
    DGL_Rotatef(angle, 0, 0, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, alpha);

    GL_DrawPatchXY3(pCursor, 0, 0, 0, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef OFFSET_Y
#undef OFFSET_X
}

void Hu_MenuDrawPageTitle(const char* title, int x, int y)
{
    if(!title || !title[0]) return;

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorv(cfg.menuTextColors[0]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(title, x, y, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawPageHelp(const char* help, int x, int y)
{
    if(!help || !help[0]) return;

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorv(cfg.menuTextColors[1]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(help, x, y, ALIGN_BOTTOM, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawOverlayBackground(float darken)
{
    DGL_SetNoMaterial();
    DGL_DrawRectf2Color(0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0, darken);
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

    borderedprojectionstate_t bp;
    boolean showFocusCursor = true;
    mn_object_t* focusObj;

    if(!Hu_MenuIsVisible()) return;

    GL_ConfigureBorderedProjection(&bp, 0, SCREENWIDTH, SCREENHEIGHT,
        Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), cfg.menuScaleMode);
    GL_BeginBorderedProjection(&bp);

    // First determine whether the focus cursor should be visible.
    focusObj = MNPage_FocusObject(Hu_MenuActivePage());
    if(focusObj && (MNObject_Flags(focusObj) & MNF_ACTIVE))
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
    if(focusObj && (MNObject_Flags(focusObj) & MNF_ACTIVE))
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
                MN_DrawPage(Hu_MenuFindPageByName("ColorWidget"), 1, true);
            }
            endOverlayDraw();

            GL_EndBorderedProjection(&bp);
            break;
        default: break;
        }
    }

#undef OVERLAY_DARKEN
}

void Hu_MenuPageTicker(mn_page_t* page)
{
    // Normal ticker actions first.
    MNPage_Ticker(page);

    /// @todo Move game-menu specific page tick functionality here.
}

void Hu_MenuNavigatePage(mn_page_t* page, int pageDelta)
{
#if 0
    int index;
    assert(page);

    oldIndex = index = MAX_OF(0, page->focus);

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
#endif
}

static void initPageObjects(mn_page_t* page)
{
    mn_object_t* ob;
    assert(page);

    page->objectsCount = 0;

    for(ob = page->objects; MNObject_Type(ob) != MN_NONE; ob++)
    {
        page->objectsCount += 1;

        ob->_page = page;
        ob->_geometry = Rect_New();

        ob->timer = 0;
        MNObject_SetFlags(ob, FO_CLEAR, MNF_FOCUS);

        if(0 != ob->_shortcut)
        {
            int shortcut = ob->_shortcut;
            ob->_shortcut = 0; // Clear invalid defaults.
            MNObject_SetShortcut(ob, shortcut);
        }

        switch(MNObject_Type(ob))
        {
        case MN_TEXT: {
            mndata_text_t* txt = (mndata_text_t*)ob->_typedata;
            MNObject_SetFlags(ob, FO_SET, MNF_NO_FOCUS);

            if(txt->text && (PTR2INT(txt->text) > 0 && PTR2INT(txt->text) < NUMTEXT))
            {
                txt->text = GET_TXT(PTR2INT(txt->text));
            }
            break; }

        case MN_BUTTON: {
            const mn_actioninfo_t* action = MNObject_Action(ob, MNA_MODIFIED);
            mndata_button_t* btn = (mndata_button_t*)ob->_typedata;

            if(btn->text && (PTR2INT(btn->text) > 0 && PTR2INT(btn->text) < NUMTEXT))
            {
                btn->text = GET_TXT(PTR2INT(btn->text));
                /// @todo Should not be done here.
                MNObject_SetShortcut(ob, btn->text[0]);
            }
            break; }

        case MN_EDIT: {
            mndata_edit_t* edit = (mndata_edit_t*) ob->_typedata;

            if(edit->emptyString && (PTR2INT(edit->emptyString) > 0 && PTR2INT(edit->emptyString) < NUMTEXT))
            {
                edit->emptyString = GET_TXT(PTR2INT(edit->emptyString));
            }
            break; }

        case MN_LIST:
        case MN_LISTINLINE: {
            mndata_list_t* list = (mndata_list_t*) ob->_typedata;
            int i;

            for(i = 0; i < list->count; ++i)
            {
                mndata_listitem_t* item = &((mndata_listitem_t*)list->items)[i];
                if(item->text && (PTR2INT(item->text) > 0 && PTR2INT(item->text) < NUMTEXT))
                {
                    item->text = GET_TXT(PTR2INT(item->text));
                }
            }
            break; }

        case MN_COLORBOX: {
            mndata_colorbox_t* cbox = (mndata_colorbox_t*) ob->_typedata;

            if(!cbox->rgbaMode)
                cbox->a = 1.f;
            if(0 >= cbox->width)
                cbox->width = MNDATA_COLORBOX_WIDTH;
            if(0 >= cbox->height)
                cbox->height = MNDATA_COLORBOX_HEIGHT;
            break; }

        case MN_MOBJPREVIEW:
            MNObject_SetFlags(ob, FO_SET, MNF_NO_FOCUS);
            break;

        default: break;
        }
    }
}

/**
 * Main task is to update objects linked to cvars.
 */
static void updatePageObjects(mn_page_t* page)
{
    mn_object_t* ob;
    assert(page);

    for(ob = page->objects; MNObject_Type(ob) != MN_NONE; ob++)
    {
        switch(MNObject_Type(ob))
        {
        case MN_TEXT:
        case MN_MOBJPREVIEW:
            MNObject_SetFlags(ob, FO_SET, MNF_NO_FOCUS);
            break;

        case MN_BUTTON: {
            const mn_actioninfo_t* action = MNObject_Action(ob, MNA_MODIFIED);
            mndata_button_t* btn = (mndata_button_t*)ob->_typedata;

            if(action && action->callback == Hu_MenuCvarButton)
            {
                cvarbutton_t* cvb;
                if(ob->data1)
                {
                    // This button has already been initialized.
                    cvb = (cvarbutton_t*) ob->data1;
                    cvb->active = (Con_GetByte(cvb->cvarname) & (cvb->mask? cvb->mask : ~0)) != 0;
                    //strcpy(obj->text, cvb->active ? cvb->yes : cvb->no);
                    btn->text = cvb->active ? cvb->yes : cvb->no;
                    continue;
                }

                // Find the cvarbutton representing this one.
                for(cvb = mnCVarButtons; cvb->cvarname; cvb++)
                {
                    if(!strcmp(btn->data, cvb->cvarname) && ob->data2 == cvb->mask)
                    {
                        cvb->active = (Con_GetByte(cvb->cvarname) & (cvb->mask? cvb->mask : ~0)) != 0;
                        ob->data1 = (void*) cvb;
                        btn->yes = cvb->yes;
                        btn->no  = cvb->no;
                        btn->text = (cvb->active ? btn->yes : btn->no);
                        break;
                    }
                }
                cvb = NULL;
            }
            break; }

        case MN_LIST:
        case MN_LISTINLINE: {
            const mn_actioninfo_t* action = MNObject_Action(ob, MNA_MODIFIED);
            mndata_list_t* list = (mndata_list_t*) ob->_typedata;

            if(action && action->callback == Hu_MenuCvarList)
            {
                MNList_SelectItemByValue(ob, MNLIST_SIF_NO_ACTION, Con_GetInteger(list->data));
            }
            break; }

        case MN_EDIT: {
            const mn_actioninfo_t* action = MNObject_Action(ob, MNA_MODIFIED);
            mndata_edit_t* edit = (mndata_edit_t*) ob->_typedata;

            if(action && action->callback == Hu_MenuCvarEdit)
            {
                MNEdit_SetText(ob, MNEDIT_STF_NO_ACTION, Con_GetString(edit->data1));
            }
            break; }

        case MN_SLIDER: {
            const mn_actioninfo_t* action = MNObject_Action(ob, MNA_MODIFIED);
            mndata_slider_t* sldr = (mndata_slider_t*) ob->_typedata;
            if(action && action->callback == Hu_MenuCvarSlider)
            {
                float value;
                if(sldr->floatMode)
                    value = Con_GetFloat(sldr->data1);
                else
                    value = Con_GetInteger(sldr->data1);
                MNSlider_SetValue(ob, MNSLIDER_SVF_NO_ACTION, value);
            }
            break; }

        case MN_COLORBOX: {
            mndata_colorbox_t* cbox = (mndata_colorbox_t*) ob->_typedata;
            const mn_actioninfo_t* action = MNObject_Action(ob, MNA_MODIFIED);

            if(action && action->callback == Hu_MenuCvarColorBox)
            {
                float rgba[4];
                rgba[CR] = Con_GetFloat(cbox->data1);
                rgba[CG] = Con_GetFloat(cbox->data2);
                rgba[CB] = Con_GetFloat(cbox->data3);
                rgba[CA] = (cbox->rgbaMode? Con_GetFloat(cbox->data4) : 1.f);
                MNColorBox_SetColor4fv(ob, MNCOLORBOX_SCF_NO_ACTION, rgba);
            }
            break; }

        default: break;
        }
    }
}

static void destroyPageObjects(mn_page_t* page)
{
    mn_object_t* obj;
    if(!page) return;
    for(obj = page->objects; MNObject_Type(obj) != MN_NONE; obj++)
    {
        if(obj->_geometry)
        {
            Rect_Delete(obj->_geometry);
            obj->_geometry = NULL;
        }
    }
}

static void destroyPage(mn_page_t* page)
{
    if(!page) return;

    destroyPageObjects(page);

    Str_Free(&page->title);

    if(page->geometry)
    {
        Rect_Delete(page->geometry);
        page->geometry = NULL;
    }

    free(page);
}

static void initAllPages(void)
{
    Hu_MenuInitColorWidgetPage();
    Hu_MenuInitMainPage();
    Hu_MenuInitGameTypePage();
#if __JDOOM__ || __JHERETIC__
    Hu_MenuInitEpisodePage();
#endif
#if __JHEXEN__
    Hu_MenuInitPlayerClassPage();
#endif
    Hu_MenuInitSkillPage();
    Hu_MenuInitMultiplayerPage();
    Hu_MenuInitPlayerSetupPage();
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuInitFilesPage();
#endif
    Hu_MenuInitLoadGameAndSaveGamePages();
    Hu_MenuInitOptionsPage();
    Hu_MenuInitGameplayOptionsPage();
    Hu_MenuInitHUDOptionsPage();
    Hu_MenuInitAutomapOptionsPage();
    Hu_MenuInitWeaponsPage();
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuInitInventoryOptionsPage();
#endif
    Hu_MenuInitSoundOptionsPage();
    Hu_MenuInitControlsPage();
}

static void destroyAllPages(void)
{
    int i;
    if(!pages) return;
    for(i = 0; i < pageCount; ++i)
    {
        pagerecord_t* rec = pages + i;
        destroyPage(rec->page);
        Str_Free(&rec->name);
    }
    free(pages);
}

static void initAllObjectsOnAllPages(void)
{
    int i;
    for(i = 0; i < pageCount; ++i)
    {
        pagerecord_t* rec = pages + i;
        initPageObjects(rec->page);
    }
}

int Hu_MenuColorWidgetCmdResponder(mn_page_t* page, menucommand_e cmd)
{
    assert(page);
    switch(cmd)
    {
    case MCMD_NAV_OUT: {
        mn_object_t* obj = (mn_object_t*)page->userData;
        MNObject_SetFlags(obj, FO_CLEAR, MNF_ACTIVE);
        S_LocalSound(SFX_MENU_CANCEL, NULL);
        colorWidgetActive = false;

        /// @kludge We should re-focus on the object instead.
        cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true;
      }
    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        return true; // Eat these.
    case MCMD_SELECT: {
        mn_object_t* obj = (mn_object_t*)page->userData;
        MNObject_SetFlags(obj, FO_CLEAR, MNF_ACTIVE);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        colorWidgetActive = false;
        MNColorBox_CopyColor(obj, 0, MN_MustFindObjectOnPage(page, 0, MNF_ID0));

        /// @kludge We should re-focus on the object instead.
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
    assert(page);
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
        if(obj)
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
//        DEBUG_Message("Warning: fallbackCommandResponder: Command %i not processed, ignoring.\n", (int) cmd);
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
        if(obj)
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
    mn_page_t* page;
    mn_object_t* obj;

    cmd = translateCommand(cmd);

    // Determine the page which will respond to this command.
    if(colorWidgetActive)
        page = Hu_MenuFindPageByName("ColorWidget");
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
    if(G_QuitInProgress())
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

            menuActivePage = NULL; // Always re-activate this page.
            Hu_MenuSetActivePage(Hu_MenuFindPageByName("Main"));

            // Enable the menu binding class
            DD_Execute(true, "activatebcontext menu");
            B_SetContextFallback("menu", Hu_MenuFallbackResponder);
        }
        return;
    }

    // Try the current focus object.
    obj = MNPage_FocusObject(page);
    if(obj && obj->cmdResponder)
    {
        if(obj->cmdResponder(obj, cmd))
            return;
    }

    // Try the page's cmd responder.
    if(page->cmdResponder)
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
        if(obj && !(MNObject_Flags(obj) & MNF_DISABLED))
        {
            if(obj->privilegedResponder)
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
        if(obj && !(MNObject_Flags(obj) & MNF_DISABLED))
        {
            if(obj->responder)
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

    if(!Hu_MenuIsActive() || !page) return false;

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
int Hu_MenuSelectLoadSlot(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    mndata_edit_t* edit = (mndata_edit_t*)obj->_typedata;
    const int saveSlot = edit->data2;
    mn_page_t* saveGamePage;

    DENG_UNUSED(parameters);

    if(MNA_ACTIVEOUT != action) return 1;

    saveGamePage = Hu_MenuFindPageByName("SaveGame");
    MNPage_SetFocus(saveGamePage, MNPage_FindObject(saveGamePage, 0, obj->data2));

    G_LoadGame(saveSlot);
    Hu_MenuCommand(chooseCloseMethod());
    return 0;
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(mn_page_t* page, const Point2Raw* origin)
{
#define TITLEOFFSET_X         (-22)
#define TITLEOFFSET_Y         (-56)

#if __JHEXEN__
    int frame = (menuTime / 5) % 7;
#endif

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(1, 1, 1, mnRendState->pageAlpha);

    WI_DrawPatchXY3(pMainTitle, Hu_ChoosePatchReplacement(cfg.menuPatchReplaceMode, pMainTitle),
        origin->x + TITLEOFFSET_X, origin->y + TITLEOFFSET_Y, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#if __JHEXEN__
    GL_DrawPatchXY(pBullWithFire[(frame + 2) % 7], origin->x - 73, origin->y + 24);
    GL_DrawPatchXY(pBullWithFire[frame], origin->x + 168, origin->y + 24);
#elif __JHERETIC__
    GL_DrawPatchXY(pRotatingSkull[17 - frame], origin->x - 70, origin->y - 46);
    GL_DrawPatchXY(pRotatingSkull[frame], origin->x + 122, origin->y - 46);
#endif

    DGL_Disable(DGL_TEXTURE_2D);

#undef TITLEOFFSET_Y
#undef TITLEOFFSET_X
}
#endif

void Hu_MenuDrawGameTypePage(mn_page_t* page, const Point2Raw* origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PICKGAMETYPE), SCREENWIDTH/2, origin->y - 28);
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
/**
 * A specialization of MNRect_Ticker() which implements the animation logic
 * for the player class selection page's player visual background.
 */
void Hu_MenuPlayerClassBackgroundTicker(mn_object_t* ob)
{
    mn_object_t* mop;
    assert(ob);

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    mop = MNPage_FocusObject(MNObject_Page(ob));
    if(mop)
    {
        playerclass_t pClass = (playerclass_t) mop->data2;
        if(pClass == PCLASS_NONE)
        {
            // Random class.
            /// @todo Use this object's timer instead of menuTime.
            pClass = (menuTime / 5);
        }

        /// @todo Only change here if in the "random" state.
        pClass %= 3; // Number of user-selectable classes.

        MNRect_SetBackgroundPatch(ob, pPlayerClassBG[pClass]);
    }

    // Call MNRect's ticker now we've done our own processing.
    MNRect_Ticker(ob);
}

/**
 * A specialization of MNMobjPreview_Ticker() which implements the animation
 * logic for the player class selection page's player visual.
 */
void Hu_MenuPlayerClassPreviewTicker(mn_object_t* ob)
{
    mn_object_t* mop;
    assert(ob);

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    mop = MNPage_FocusObject(MNObject_Page(ob));
    if(mop)
    {
        playerclass_t pClass = (playerclass_t) mop->data2;
        if(pClass == PCLASS_NONE)
        {
            // Random class.
            /// @todo Use this object's timer instead of menuTime.
            pClass = PCLASS_FIRST + (menuTime / 5);
            pClass %= 3; // Number of user-selectable classes.

            MNMobjPreview_SetPlayerClass(ob, pClass);
            MNMobjPreview_SetMobjType(ob, (PCLASS_NONE == pClass? MT_NONE : PCLASS_INFO(pClass)->mobjType));
        }
    }

    // Call MNMobjPreview's ticker now we've done our own processing.
    MNMobjPreview_Ticker(ob);
}

void Hu_MenuDrawPlayerClassPage(mn_page_t* page, const Point2Raw* origin)
{
    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    FR_DrawTextXY3("Choose class:", origin->x - 32, origin->y - 42, ALIGN_TOPLEFT,
                   MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

#if __JDOOM__ || __JHERETIC__
void Hu_MenuDrawEpisodePage(mn_page_t* page, const Point2Raw* origin)
{
#if __JHERETIC__
    // Inform the user episode 6 is designed for deathmatch only.
    mn_object_t* obj = MNPage_FindObject(page, 0, MNF_ID0);
    if(obj && obj == MNPage_FocusObject(page))
    {
        const char* str = notDesignedForMessage;
        Point2Raw origin;

        composeNotDesignedForMessage(GET_TXT(TXT_SINGLEPLAYER));

        origin.x = SCREENWIDTH/2;
        origin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);

        Hu_MenuDrawPageHelp(notDesignedForMessage, origin.x, origin.y);
    }
#else // __JDOOM__
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorv(cfg.menuTextColors[0]);
    FR_SetAlpha(mnRendState->pageAlpha);

    WI_DrawPatchXY3(pEpisode, Hu_ChoosePatchReplacement(cfg.menuPatchReplaceMode, pEpisode),
        origin->x + 7, origin->y - 25, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}
#endif

void Hu_MenuDrawSkillPage(mn_page_t* page, const Point2Raw* origin)
{
#if __JDOOM__ || __JDOOM64__
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatchXY3(pNewGame, Hu_ChoosePatchReplacement(cfg.menuPatchReplaceMode, pNewGame),
        origin->x + 48, origin->y - 49, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
    WI_DrawPatchXY3(pSkill, Hu_ChoosePatchReplacement(cfg.menuPatchReplaceMode, pSkill),
        origin->x + 6, origin->y - 25, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#elif __JHEXEN__
    Hu_MenuDrawPageTitle("Choose Skill Level:", origin->x + 36, origin->y - 28);
#endif
}

void Hu_MenuUpdateGameSaveWidgets(void)
{
    const int saveSlotObjectIds[NUMSAVESLOTS] = {
        MNF_ID0, MNF_ID1, MNF_ID2, MNF_ID3, MNF_ID4, MNF_ID5,
#if !__JHEXEN__
        MNF_ID6, MNF_ID7
#endif
    };
    mn_page_t* page;
    int i;

    if(!menuActive) return;

    // Prompt a refresh of the game-save info. We don't yet actively monitor
    // the contents of the game-save paths, so instead we settle for manual
    // updates whenever the save/load menu is opened.
    SV_UpdateAllSaveInfo();

    // Update widgets.
    page = Hu_MenuFindPageByName("LoadGame");
    for(i = 0; i < NUMSAVESLOTS; ++i)
    {
        mn_object_t* obj = MN_MustFindObjectOnPage(page, 0, saveSlotObjectIds[i]);
        mndata_edit_t* edit = (mndata_edit_t*) obj->_typedata;
        SaveInfo* info = SV_SaveInfoForSlot(edit->data2);
        const char* text = "";

        MNObject_SetFlags(obj, FO_SET, MNF_DISABLED);
        if(SaveInfo_IsLoadable(info))
        {
            text = Str_Text(SaveInfo_Name(info));
            MNObject_SetFlags(obj, FO_CLEAR, MNF_DISABLED);
        }
        MNEdit_SetText(obj, MNEDIT_STF_NO_ACTION, text);
    }
}

/**
 * Called after the save name has been modified and to action the game-save.
 */
int Hu_MenuSelectSaveSlot(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    const int saveSlot = edit->data2;
    mn_page_t* page;

    DENG_UNUSED(parameters);

    if(MNA_ACTIVEOUT != action) return 1;

    if(menuNominatingQuickSaveSlot)
    {
        Con_SetInteger("game-save-quick-slot", saveSlot);
        menuNominatingQuickSaveSlot = false;
    }

    if(!G_SaveGame2(saveSlot, Str_Text(MNEdit_Text(ob)))) return 0;

    page = Hu_MenuFindPageByName("SaveGame");
    MNPage_SetFocus(page, MN_MustFindObjectOnPage(page, 0, ob->data2));

    page = Hu_MenuFindPageByName("LoadGame");
    MNPage_SetFocus(page, MN_MustFindObjectOnPage(page, 0, ob->data2));

    Hu_MenuCommand(chooseCloseMethod());
    return 0;
}

int Hu_MenuCvarButton(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
    const cvarbutton_t* cb = obj->data1;
    cvartype_t varType = Con_GetVariableType(cb->cvarname);
    int value;

    DENG_UNUSED(parameters);
    if(MNA_MODIFIED != action) return 1;

    //strcpy(btn->text, cb->active? cb->yes : cb->no);
    btn->text = cb->active? cb->yes : cb->no;

    if(CVT_NULL == varType) return 0;

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

int Hu_MenuCvarList(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    const mndata_list_t* list = (mndata_list_t*) obj->_typedata;
    const mndata_listitem_t* item;
    cvartype_t varType;
    int value;

    DENG_UNUSED(parameters);
    if(MNA_MODIFIED != action) return 1;

    if(MNList_Selection(obj) < 0) return 0; // Hmm?

    varType = Con_GetVariableType(list->data);
    if(CVT_NULL == varType) return 0;

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

int Hu_MenuSaveSlotEdit(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(parameters);

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

int Hu_MenuCvarEdit(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    const mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    cvartype_t varType = Con_GetVariableType(edit->data1);

    DENG_UNUSED(parameters);

    if(MNA_MODIFIED != action) return 1;

    switch(varType)
    {
    case CVT_CHARPTR:
        Con_SetString2(edit->data1, Str_Text(MNEdit_Text(ob)), SVF_WRITE_OVERRIDE);
        break;
    case CVT_URIPTR: {
        /// @todo Sanitize and validate against known schemas.
        Uri* uri = Uri_NewWithPath2(Str_Text(MNEdit_Text(ob)), RC_NULL);
        Con_SetUri2(edit->data1, uri, SVF_WRITE_OVERRIDE);
        Uri_Delete(uri);
        break;
      }
    default: break;
    }
    return 0;
}

int Hu_MenuCvarSlider(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    const mndata_slider_t* sldr = obj->_typedata;
    cvartype_t varType = Con_GetVariableType(sldr->data1);
    float value = MNSlider_Value(obj);

    DENG_UNUSED(parameters);
    if(MNA_MODIFIED != action) return 1;

    if(CVT_NULL == varType) return 0;

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

int Hu_MenuActivateColorWidget(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    mn_object_t* cboxMix, *sldrRed, *sldrGreen, *sldrBlue, *textAlpha, *sldrAlpha;
    mn_page_t* colorWidgetPage = Hu_MenuFindPageByName("ColorWidget");

    DENG_UNUSED(parameters);
    if(action != MNA_ACTIVE) return 1;

    cboxMix   = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID0);
    sldrRed   = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID1);
    sldrGreen = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID2);
    sldrBlue  = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID3);
    textAlpha = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID4);
    sldrAlpha = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID5);

    colorWidgetActive = true;

    MNPage_Initialize(colorWidgetPage);
    colorWidgetPage->userData = obj;

    MNColorBox_CopyColor(cboxMix, 0, obj);
    MNSlider_SetValue(sldrRed,   MNSLIDER_SVF_NO_ACTION, MNColorBox_Redf(obj));
    MNSlider_SetValue(sldrGreen, MNSLIDER_SVF_NO_ACTION, MNColorBox_Greenf(obj));
    MNSlider_SetValue(sldrBlue,  MNSLIDER_SVF_NO_ACTION, MNColorBox_Bluef(obj));
    MNSlider_SetValue(sldrAlpha, MNSLIDER_SVF_NO_ACTION, MNColorBox_Alphaf(obj));

    MNObject_SetFlags(textAlpha, (MNColorBox_RGBAMode(obj)? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);
    MNObject_SetFlags(sldrAlpha, (MNColorBox_RGBAMode(obj)? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);

    return 0;
}

int Hu_MenuCvarColorBox(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    DENG_UNUSED(parameters);
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

void Hu_MenuDrawLoadGamePage(mn_page_t* page, const Point2Raw* origin)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JHERETIC__ || __JHEXEN__
    FR_DrawTextXY3("Load Game", SCREENWIDTH/2, origin->y-20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#else
    WI_DrawPatchXY3(pLoadGame, Hu_ChoosePatchReplacement(cfg.menuPatchReplaceMode, pLoadGame),
        origin->x - 8, origin->y - 26, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawSaveGamePage(mn_page_t* page, const Point2Raw* origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Save Game", SCREENWIDTH/2, origin->y - 20);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatchXY3(pSaveGame, Hu_ChoosePatchReplacement(cfg.menuPatchReplaceMode, pSaveGame),
        origin->x - 8, origin->y - 26, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
int Hu_MenuSelectHelp(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(parameters);
    if(MNA_ACTIVEOUT != action) return 1;
    G_StartHelp();
    return 0;
}
#endif

void Hu_MenuDrawOptionsPage(mn_page_t* page, const Point2Raw* origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Options", origin->x + 42, origin->y - 38);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatchXY3(pOptionsTitle, Hu_ChoosePatchReplacement(cfg.menuPatchReplaceMode, pOptionsTitle),
        origin->x + 42, origin->y - 20, ALIGN_TOP, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void Hu_MenuDrawWeaponsPage(mn_page_t* page, const Point2Raw* offset)
{
    // Inform the user how to change the order.
    if(MNPage_FocusObject(page) == MN_MustFindObjectOnPage(page, 0, MNF_ID0))
    {
        const char* helpText = "Use left/right to move weapon up/down";
        Point2Raw origin;
        origin.x = SCREENWIDTH/2;
        origin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);
        Hu_MenuDrawPageHelp(helpText, origin.x, origin.y);
    }
}

void Hu_MenuDrawMultiplayerPage(mn_page_t* page, const Point2Raw* origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_MULTIPLAYER), SCREENWIDTH/2, origin->y - 28);
}

void Hu_MenuDrawPlayerSetupPage(mn_page_t* page, const Point2Raw* origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PLAYERSETUP), SCREENWIDTH/2, origin->y - 28);
}

int Hu_MenuActionSetActivePage(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    assert(obj);
    DENG_UNUSED(parameters);
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MenuSetActivePage(Hu_MenuFindPageByName((char*)obj->data1));
    return 0;
}

int Hu_MenuUpdateColorWidgetColor(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    float value = MNSlider_Value(obj);
    mn_object_t* cboxMix = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("ColorWidget"), 0, MNF_ID0);

    DENG_UNUSED(parameters);
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

int Hu_MenuChangeWeaponPriority(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(parameters);
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

int Hu_MenuSelectSingleplayer(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(parameters);

    if(MNA_ACTIVEOUT != action) return 1;

    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NEWGAME, NULL, 0, NULL);
        return 0;
    }

#if __JHEXEN__
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("PlayerClass"));
#elif __JHERETIC__
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("Episode"));
#elif __JDOOM64__
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("Skill"));
#else // __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        Hu_MenuSetActivePage(Hu_MenuFindPageByName("Skill"));
    else
        Hu_MenuSetActivePage(Hu_MenuFindPageByName("Episode"));
#endif
    return 0;
}

int Hu_MenuSelectMultiplayer(mn_object_t* obj, mn_actionid_t action, void* parameters)
{
    mn_page_t* multiplayerPage = Hu_MenuFindPageByName("Multiplayer");
    mn_object_t* labelObj = MN_MustFindObjectOnPage(multiplayerPage, 0, MNF_ID0);
    mndata_button_t* btn = (mndata_button_t*)labelObj->_typedata;

    DENG_UNUSED(parameters);
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
    Hu_MenuSetActivePage(multiplayerPage);
    return 0;
}

int Hu_MenuSelectJoinGame(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);

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

int Hu_MenuSelectPlayerSetup(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    mn_page_t* playerSetupPage = Hu_MenuFindPageByName("PlayerSetup");
    mn_object_t* mop    = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID0);
    mn_object_t* name   = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID1);
    mn_object_t* color  = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID3);
#if __JHEXEN__
    mn_object_t* class_;
#endif

    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);
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
    class_ = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID2);
    MNList_SelectItemByValue(class_, MNLIST_SIF_NO_ACTION, cfg.netClass);
#endif

    MNEdit_SetText(name, MNEDIT_STF_NO_ACTION|MNEDIT_STF_REPLACEOLD, Con_GetString("net-name"));

    Hu_MenuSetActivePage(playerSetupPage);
    return 0;
}

#if __JHEXEN__
int Hu_MenuSelectPlayerSetupPlayerClass(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    int selection;

    DENG_UNUSED(parameters);
    if(MNA_MODIFIED != action) return 1;

    selection = MNList_Selection(ob);
    if(selection >= 0)
    {
        mn_object_t* mop = MN_MustFindObjectOnPage(MNObject_Page(ob), 0, MNF_ID0);
        MNMobjPreview_SetPlayerClass(mop, selection);
        MNMobjPreview_SetMobjType(mop, PCLASS_INFO(selection)->mobjType);
    }
    return 0;
}
#endif

int Hu_MenuSelectPlayerColor(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    int selection;

    DENG_UNUSED(parameters);
    if(MNA_MODIFIED != action) return 1;

    // The color translation map is stored in the list item data member.
    selection = MNList_ItemData(ob, MNList_Selection(ob));
    if(selection >= 0)
    {
        mn_object_t* mop = MN_MustFindObjectOnPage(MNObject_Page(ob), 0, MNF_ID0);
        MNMobjPreview_SetTranslationMap(mop, selection);
    }
    return 0;
}

int Hu_MenuSelectAcceptPlayerSetup(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    mn_object_t* plrNameEdit = MN_MustFindObjectOnPage(MNObject_Page(ob), 0, MNF_ID1);
#if __JHEXEN__
    mn_object_t* plrClassList = MN_MustFindObjectOnPage(MNObject_Page(ob), 0, MNF_ID2);
#endif
    mn_object_t* plrColorList = MN_MustFindObjectOnPage(MNObject_Page(ob), 0, MNF_ID3);
    char buf[300];

    DENG_UNUSED(parameters);

#if __JHEXEN__
    cfg.netClass = MNList_Selection(plrClassList);
#endif
    // The color translation map is stored in the list item data member.
    cfg.netColor = MNList_ItemData(plrColorList, MNList_Selection(plrColorList));

    if(MNA_ACTIVEOUT != action) return 1;

    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, Str_Text(MNEdit_Text(plrNameEdit)), 300);
    DD_Execute(false, buf);

    if(IS_NETGAME)
    {
        strcpy(buf, "setname ");
        M_StrCatQuoted(buf, Str_Text(MNEdit_Text(plrNameEdit)), 300);
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

    Hu_MenuSetActivePage(Hu_MenuFindPageByName("Multiplayer"));
    return 0;
}

int Hu_MenuSelectQuitGame(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);
    if(MNA_ACTIVEOUT != action) return 1;
    G_QuitGame();
    return 0;
}

int Hu_MenuSelectEndGame(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);
    if(MNA_ACTIVEOUT != action) return 1;
    G_EndGame();
    return 0;
}

int Hu_MenuSelectLoadGame(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);

    if(MNA_ACTIVEOUT != action) return 1;

    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT && !Get(DD_PLAYBACK))
        {
            Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, 0, NULL);
            return 0;
        }
    }

    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("LoadGame"));
    return 0;
}

int Hu_MenuSelectSaveGame(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    player_t* player = &players[CONSOLEPLAYER];

    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);

    if(MNA_ACTIVEOUT != action) return 1;
    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT)
        {
#if __JDOOM__ || __JDOOM64__
            Hu_MsgStart(MSG_ANYKEY, SAVENET, NULL, 0, NULL);
#endif
            return 0;
        }

        if(G_GameState() != GS_MAP)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, NULL, 0, NULL);
            return 0;
        }

        if(player->playerState == PST_DEAD)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, NULL, 0, NULL);
            return 0;
        }
    }

    Hu_MenuCommand(MCMD_OPEN);
    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("SaveGame"));
    return 0;
}

#if __JHEXEN__
int Hu_MenuSelectPlayerClass(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    mn_page_t* skillPage = Hu_MenuFindPageByName("Skill");
    int option = ob->data2;
    mn_object_t* skillObj;
    const char* text;

    DENG_UNUSED(parameters);

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

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID0);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_BABY]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID1);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_EASY]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID2);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_MEDIUM]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID3);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_HARD]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID4);
    text = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_NIGHTMARE]);
    ((mndata_button_t*)skillObj->_typedata)->text = text;
    MNObject_SetShortcut(skillObj, text[0]);

    switch(mnPlrClass)
    {
    case PCLASS_FIGHTER:    MNPage_SetX(skillPage, 120); break;
    case PCLASS_CLERIC:     MNPage_SetX(skillPage, 116); break;
    case PCLASS_MAGE:       MNPage_SetX(skillPage, 112); break;
    }
    Hu_MenuSetActivePage(skillPage);
    return 0;
}

int Hu_MenuFocusOnPlayerClass(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    playerclass_t plrClass = (playerclass_t)ob->data2;
    mn_object_t* mop;

    DENG_UNUSED(parameters);
    if(MNA_FOCUS != action) return 1;

    mop = MN_MustFindObjectOnPage(MNObject_Page(ob), 0, MNF_ID0);
    MNMobjPreview_SetPlayerClass(mop, plrClass);
    MNMobjPreview_SetMobjType(mop, (PCLASS_NONE == plrClass? MT_NONE : PCLASS_INFO(plrClass)->mobjType));

    Hu_MenuDefaultFocusAction(ob, action, parameters);
    return 0;
}
#endif

#if __JDOOM__ || __JHERETIC__
int Hu_MenuFocusEpisode(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(parameters);
    if(MNA_FOCUS != action) return 1;
    mnEpisode = ob->data2;
    Hu_MenuDefaultFocusAction(ob, action, parameters);
    return 0;
}

int Hu_MenuConfirmOrderCommericalVersion(msgresponse_t response, int userValue, void* userPointer)
{
    G_StartHelp();
    return true;
}

int Hu_MenuActivateNotSharewareEpisode(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MsgStart(MSG_ANYKEY, SWSTRING, Hu_MenuConfirmOrderCommericalVersion, 0, NULL);
    return 0;
}
#endif

int Hu_MenuFocusSkillMode(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    assert(ob);
    DENG_UNUSED(parameters);
    if(MNA_FOCUS != action) return 1;
    mnSkillmode = (skillmode_t)ob->data2;
    Hu_MenuDefaultFocusAction(ob, action, parameters);
    return 0;
}

#if __JDOOM__
int Hu_MenuConfirmInitNewGame(msgresponse_t response, int userValue, void* userPointer)
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
        Hu_MsgStart(MSG_YESNO, NIGHTMARE, Hu_MenuConfirmInitNewGame, 0, NULL);
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

int Hu_MenuActionInitNewGame(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
    DENG_UNUSED(ob);
    DENG_UNUSED(parameters);
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MenuInitNewGame(false);
    return 0;
}

int Hu_MenuSelectControlPanelLink(mn_object_t* ob, mn_actionid_t action, void* parameters)
{
#define NUM_PANEL_NAMES         3

    static const char* panelNames[NUM_PANEL_NAMES] = {
        "panel",
        "panel audio",
        "panel input"
    };
    int idx = ob->data2;

    DENG_UNUSED(parameters);
    if(MNA_ACTIVEOUT != action) return 1;

    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
        idx = 0;

    Hu_MenuCommand(MCMD_CLOSEFAST);
    DD_Execute(true, panelNames[idx]);
    return 0;

#undef NUM_PANEL_NAMES
}

D_CMD(MenuOpen)
{
    if(argc > 1)
    {
        mn_page_t* page = Hu_MenuFindPageByName(argv[1]);
        if(page)
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
 * Routes console commands for menu actions and navigation into the menu subsystem.
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
