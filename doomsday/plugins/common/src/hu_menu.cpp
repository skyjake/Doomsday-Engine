/** @file hu_menu.cpp  Menu widget stuff, episode selection and such.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "hu_menu.h"

#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <QMap>
#include <QtAlgorithms>
#include <de/memory.h>
#include "am_map.h"
#include "g_common.h"
#include "g_controls.h"
#include "gamesession.h"
#include "hu_chat.h"
#include "hu_log.h"
#include "hu_msg.h"
#include "hu_stuff.h"
#include "m_argv.h"
#include "m_ctrl.h"
#include "mapinfo.h"
#include "p_savedef.h"
#include "player.h"
#include "r_common.h"
#include "saveslots.h"
#include "x_hair.h"

using namespace de;
using namespace common;

/// Original game line height for pages that employ the fixed layout (in 320x200 pixels).
#if __JDOOM__
#  define FIXED_LINE_HEIGHT (15+1)
#else
#  define FIXED_LINE_HEIGHT (19+1)
#endif

int Hu_MenuActionSetActivePage(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuActionInitNewGame(mn_object_t *ob, mn_actionid_t action, void *parameters);

int Hu_MenuSelectLoadGame(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectSaveGame(mn_object_t *ob, mn_actionid_t action, void *parameters);
#if __JHEXEN__
int Hu_MenuSelectFiles(mn_object_t *ob, mn_actionid_t action, void *parameters);
#endif
int Hu_MenuSelectPlayerSetup(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectJoinGame(mn_object_t *ob, mn_actionid_t action, void *parameters);

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
int Hu_MenuSelectHelp(mn_object_t *ob, mn_actionid_t action, void *parameters);
#endif
int Hu_MenuSelectControlPanelLink(mn_object_t *ob, mn_actionid_t action, void *parameters);

int Hu_MenuSelectSingleplayer(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectMultiplayer(mn_object_t *ob, mn_actionid_t action, void *parameters);
#if __JDOOM__ || __JHERETIC__
int Hu_MenuFocusEpisode(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuActivateNotSharewareEpisode(mn_object_t *ob, mn_actionid_t action, void *parameters);
#endif
#if __JHEXEN__
int Hu_MenuFocusOnPlayerClass(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectPlayerClass(mn_object_t *ob, mn_actionid_t action, void *parameters);
#endif
int Hu_MenuFocusSkillMode(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectLoadSlot(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectQuitGame(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectEndGame(mn_object_t *ob, mn_actionid_t action, void *parameters);
int Hu_MenuSelectAcceptPlayerSetup(mn_object_t *ob, mn_actionid_t action, void *parameters);

int Hu_MenuSelectSaveSlot(mn_object_t *ob, mn_actionid_t action, void *parameters);

int Hu_MenuChangeWeaponPriority(mn_object_t *ob, mn_actionid_t action, void *parameters);
#if __JHEXEN__
int Hu_MenuSelectPlayerSetupPlayerClass(mn_object_t *ob, mn_actionid_t action, void *parameters);
#endif
int Hu_MenuSelectPlayerColor(mn_object_t *ob, mn_actionid_t action, void *parameters);

#if __JHEXEN__
void Hu_MenuPlayerClassBackgroundTicker(mn_object_t *ob);
void Hu_MenuPlayerClassPreviewTicker(mn_object_t *ob);
#endif

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(mn_page_t *page, Point2Raw const *origin);
#endif

void Hu_MenuDrawGameTypePage(mn_page_t *page, Point2Raw const *origin);
void Hu_MenuDrawSkillPage(mn_page_t *page, Point2Raw const *origin);
#if __JHEXEN__
void Hu_MenuDrawPlayerClassPage(mn_page_t *page, Point2Raw const *origin);
#endif
#if __JDOOM__ || __JHERETIC__
void Hu_MenuDrawEpisodePage(mn_page_t *page, Point2Raw const *origin);
#endif
void Hu_MenuDrawOptionsPage(mn_page_t *page, Point2Raw const *origin);
void Hu_MenuDrawWeaponsPage(mn_page_t *page, Point2Raw const *origin);
void Hu_MenuDrawLoadGamePage(mn_page_t *page, Point2Raw const *origin);
void Hu_MenuDrawSaveGamePage(mn_page_t *page, Point2Raw const *origin);
void Hu_MenuDrawMultiplayerPage(mn_page_t *page, Point2Raw const *origin);
void Hu_MenuDrawPlayerSetupPage(mn_page_t *page, Point2Raw const *origin);

int Hu_MenuColorWidgetCmdResponder(mn_page_t *page, menucommand_e cmd);

static void initAllPages();
static void destroyAllPages();

static void initAllObjectsOnAllPages();

static void Hu_MenuUpdateCursorState();

static dd_bool Hu_MenuHasCursorRotation(mn_object_t *obj);

cvarbutton_t mnCVarButtons[] = {
    cvarbutton_t(0, "ctl-aim-noauto"),
#if __JHERETIC__ || __JHEXEN__
    cvarbutton_t(0, "ctl-inventory-mode", "Scroll", "Cursor"),
    cvarbutton_t(0, "ctl-inventory-use-immediate"),
    cvarbutton_t(0, "ctl-inventory-use-next"),
    cvarbutton_t(0, "ctl-inventory-wrap"),
#endif
    cvarbutton_t(0, "ctl-look-spring"),
    cvarbutton_t(0, "ctl-run"),
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "game-anybossdeath666"),
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    cvarbutton_t(0, "game-corpse-sliding"),
#endif
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "game-maxskulls"),
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    cvarbutton_t(0, "game-monsters-stuckindoors"),
    cvarbutton_t(0, "game-monsters-floatoverblocking"),
    cvarbutton_t(0, "game-objects-clipping"),
    cvarbutton_t(0, "game-objects-falloff"),
#endif
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "game-objects-gibcrushednonbleeders"),
#endif
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    cvarbutton_t(0, "game-objects-neverhangoverledges"),
    cvarbutton_t(0, "game-player-wallrun-northonly"),
#endif
#if __JDOOM__
    cvarbutton_t(0, "game-raiseghosts"),
#endif
    cvarbutton_t(0, "game-save-confirm"),
    cvarbutton_t(0, "game-save-confirm-loadonreborn"),
    cvarbutton_t(0, "game-save-last-loadonreborn"),
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "game-skullsinwalls"),
#if __JDOOM__
    cvarbutton_t(0, "game-vilechase-usevileradius"),
#endif
    cvarbutton_t(0, "game-zombiescanexit"),
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    cvarbutton_t(0, "hud-ammo"),
    cvarbutton_t(0, "hud-armor"),
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    cvarbutton_t(0, "hud-cheat-counter-show-mapopen"),
#endif
#if __JHERETIC__ || __JHEXEN__
    cvarbutton_t(0, "hud-currentitem"),
#endif
#if __JDOOM__
    cvarbutton_t(0, "hud-face"),
    cvarbutton_t(0, "hud-face-ouchfix"),
#endif
    cvarbutton_t(0, "hud-health"),
#if __JHERETIC__ || __JHEXEN__
    cvarbutton_t(0, "hud-inventory-slot-showempty"),
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    cvarbutton_t(0, "hud-keys"),
#endif
#if __JDOOM__
    cvarbutton_t(0, "hud-keys-combine"),
#endif
#if __JHEXEN__
    cvarbutton_t(0, "hud-mana"),
#endif
#if __JDOOM64__
    cvarbutton_t(0, "hud-power"),
#endif
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "hud-status-weaponslots-ownedfix"),
#endif
    cvarbutton_t(0, "hud-unhide-damage"),
    cvarbutton_t(0, "hud-unhide-pickup-ammo"),
    cvarbutton_t(0, "hud-unhide-pickup-armor"),
    cvarbutton_t(0, "hud-unhide-pickup-health"),
#if __JHERETIC__ || __JHEXEN__
    cvarbutton_t(0, "hud-unhide-pickup-invitem"),
#endif
    cvarbutton_t(0, "hud-unhide-pickup-powerup"),
    cvarbutton_t(0, "hud-unhide-pickup-key"),
    cvarbutton_t(0, "hud-unhide-pickup-weapon"),
    cvarbutton_t(0, "map-door-colors"),
    cvarbutton_t(0, "msg-show"),
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "player-autoswitch-berserk"),
#endif
    cvarbutton_t(0, "player-autoswitch-notfiring"),
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    cvarbutton_t(0, "player-jump"),
#endif
    cvarbutton_t(0, "player-weapon-cycle-sequential"),
    cvarbutton_t(0, "player-weapon-nextmode"),
#if __JDOOM64__
    cvarbutton_t(0, "player-weapon-recoil"),
#endif
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "server-game-bfg-freeaim"),
#endif
    cvarbutton_t(0, "server-game-coop-nodamage"),
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "server-game-coop-nothing"),
    cvarbutton_t(0, "server-game-coop-noweapons"),
    cvarbutton_t(0, "server-game-coop-respawn-items"),
#endif
#if __JHEXEN__
    cvarbutton_t(0, "server-game-deathmatch"),
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    cvarbutton_t(0, "server-game-jump"),
#endif
#if __JDOOM__ || __JDOOM64__
    cvarbutton_t(0, "server-game-nobfg"),
#endif
    cvarbutton_t(0, "server-game-nomonsters"),
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    cvarbutton_t(0, "server-game-noteamdamage"),
#endif
    cvarbutton_t(0, "server-game-radiusattack-nomaxz"),
#if __JHEXEN__
    cvarbutton_t(0, "server-game-randclass"),
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    cvarbutton_t(0, "server-game-respawn"),
#endif
    cvarbutton_t(0, "view-cross-vitality"),
    cvarbutton_t()
};

int menuTime;
dd_bool menuNominatingQuickSaveSlot;

static mn_page_t *menuActivePage;
static dd_bool menuActive;

static float mnAlpha; // Alpha level for the entire menu.
static float mnTargetAlpha; // Target alpha for the entire UI.

static skillmode_t mnSkillmode = SM_MEDIUM;
static int mnEpisode;
#if __JHEXEN__
static int mnPlrClass = PCLASS_FIGHTER;
#endif

static int frame; // Used by any graphic animations that need to be pumped.

static dd_bool colorWidgetActive;

// Present cursor state.
static dd_bool cursorHasRotation;
static float cursorAngle;
static int cursorAnimCounter;
static int cursorAnimFrame;

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

static dd_bool inited;

typedef QMap<String, mn_page_t *> Pages;
static Pages pages;

static menucommand_e chooseCloseMethod()
{
    // If we aren't using a transition then we can close normally and allow our
    // own menu fade-out animation to be used instead.
    return Con_GetInteger("con-transition-tics") == 0? MCMD_CLOSE : MCMD_CLOSEFAST;
}

mn_page_t *Hu_MenuFindPageByName(char const *name)
{
    if(name && name[0])
    {
        Pages::iterator found = pages.find(String(name).toLower());
        if(found != pages.end())
        {
            return *found;
        }
    }
    return 0; // Not found.
}

/// @todo Make this state an object property flag.
/// @return  @c true if the rotation of a cursor on this object should be animated.
static dd_bool Hu_MenuHasCursorRotation(mn_object_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return (!(ob->flags() & MNF_DISABLED) &&
              (ob->type() == MN_LISTINLINE || ob->type() == MN_SLIDER));
}

/// To be called to re-evaluate the state of the cursor (e.g., when focus changes).
static void Hu_MenuUpdateCursorState()
{
    if(menuActive)
    {
        mn_page_t *page = colorWidgetActive? Hu_MenuFindPageByName("ColorWidget") : Hu_MenuActivePage();
        if(mn_object_t *ob = page->focusObject())
        {
            cursorHasRotation = Hu_MenuHasCursorRotation(ob);
            return;
        }
    }
    cursorHasRotation = false;
}

void Hu_MenuLoadResources()
{
    char buf[9];

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
    for(int i = 0; i < 18; ++i)
    {
        dd_snprintf(buf, 9, "M_SKL%02d", i);
        pRotatingSkull[i] = R_DeclarePatch(buf);
    }
#endif

#if __JHEXEN__
    for(int i = 0; i < 7; ++i)
    {
        dd_snprintf(buf, 9, "FBUL%c0", 'A'+i);
        pBullWithFire[i] = R_DeclarePatch(buf);
    }

    pPlayerClassBG[0] = R_DeclarePatch("M_FBOX");
    pPlayerClassBG[1] = R_DeclarePatch("M_CBOX");
    pPlayerClassBG[2] = R_DeclarePatch("M_MBOX");
#endif

    for(int i = 0; i < MENU_CURSOR_FRAMECOUNT; ++i)
    {
#if __JDOOM__ || __JDOOM64__
        dd_snprintf(buf, 9, "M_SKULL%d", i+1);
#else
        dd_snprintf(buf, 9, "M_SLCTR%d", i+1);
#endif
        pCursors[i] = R_DeclarePatch(buf);
    }
}

void Hu_MenuInitColorWidgetPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(98, 60);
#else
    Point2Raw const origin(124, 60);
#endif
    uint const numObjects = 10;

    mn_page_t *page = Hu_MenuNewPage("ColorWidget", &origin, MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, Hu_MenuColorWidgetCmdResponder, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_COLORBOX;
    ob->_flags         = MNF_ID0 | MNF_NO_FOCUS;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->width    = SCREENHEIGHT / 7;
        cbox->height   = SCREENHEIGHT / 7;
        cbox->rgbaMode = true;
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Red";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_flags         = MNF_ID1;
    ob->_shortcut      = 'r';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2          = CR;
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Green";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_flags         = MNF_ID2;
    ob->_shortcut      = 'g';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2 = CG;
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Blue";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_flags         = MNF_ID3;
    ob->_shortcut      = 'b';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2          = CB;
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_flags         = MNF_ID4;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Opacity";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_flags         = MNF_ID5;
    ob->_shortcut      = 'o';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    ob->data2          = CA;
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
    }
    ob++;

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitMainPage()
{
#if __JHEXEN__ || __JHERETIC__
    Point2Raw origin(110, 56);
    uint numObjects = 6;
#else
    Point2Raw origin(97, 64);
# if __JDOOM64__
    uint numObjects = 7;
# else
    uint numObjects = 8;
# endif
#endif

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        origin.y += 8;
    }
#endif

#if __JDOOM__ || __JDOOM64__
    mn_page_t *page = Hu_MenuNewPage("Main", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, NULL, NULL);
#else
    mn_page_t *page = Hu_MenuNewPage("Main", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawMainPage, NULL, NULL);
#endif
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    int y = 0;

#if __JDOOM__ || __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_origin.x      = -3;
    ob->_origin.y      = -70;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->patch = &pMainTitle;
    }
    ob++;
#endif

    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 'n';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"GameType";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
#if defined(__JDOOM__) && !defined(__JDOOM64__)
        btn->patch = &pNGame;
#else
        btn->text  = "New Game";
#endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 'o';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"Options";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
#if defined(__JDOOM__) && !defined(__JDOOM64__)
        btn->patch = &pOptions;
#else
        btn->text  = "Options";
#endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

#if __JDOOM__ || __JDOOM64__
    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 'l';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadGame;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
# if __JDOOM64__
        btn->text  = "Load Game";
# else
        btn->patch = &pLoadGame;
# endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveGame;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
# if __JDOOM64__
        btn->text  = "Save Game";
# else
        btn->patch = &pSaveGame;
# endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

#else
    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 'f';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"Files";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Game Files";
    }
    ob++; y += FIXED_LINE_HEIGHT;
#endif

#if !__JDOOM64__
    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
# if __JDOOM__
    ob->_flags         = MNF_ID0;
    ob->_shortcut      = 'r';
# else
    ob->_shortcut      = 'i';
# endif
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectHelp;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
# if defined(__JDOOM__)
        btn->patch = &pReadThis;
# else
        btn->text  = "Info";
# endif
    }
    ob++; y += FIXED_LINE_HEIGHT;
#endif

    ob->_type          = MN_BUTTON;
#if __JDOOM__
    ob->_flags         = MNF_ID1;
#endif
    ob->_origin.y      = y;
    ob->_shortcut      = 'q';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectQuitGame;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
#if defined(__JDOOM__) && !defined(__JDOOM64__)
        btn->patch = &pQuitGame;
#else
        btn->text  = "Quit Game";
#endif
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitGameTypePage()
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw origin(97, 65);
#else
    Point2Raw origin(104, 65);
#endif
    uint const numObjects = 3;

    mn_page_t *page = Hu_MenuNewPage("GameType", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawGameTypePage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    int y = 0;

    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSingleplayer;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = (char const *)TXT_SINGLEPLAYER;
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 'm';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectMultiplayer;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = (char const *)TXT_MULTIPLAYER;
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitSkillPage()
{
#if __JHEXEN__
    Point2Raw const origin(120, 44);
#elif __JHERETIC__
    Point2Raw const origin(38, 30);
#else
    Point2Raw const origin(48, 63);
#endif
    uint skillButtonFlags[NUM_SKILL_MODES] = {
        MNF_ID0,
        MNF_ID1,
        MNF_ID2 | MNF_DEFAULT,
        MNF_ID3,
#  if !__JDOOM64__
        MNF_ID4
#  endif
    };
#if !__JHEXEN__
    int skillButtonTexts[NUM_SKILL_MODES] = {
        TXT_SKILL1,
        TXT_SKILL2,
        TXT_SKILL3,
        TXT_SKILL4,
#  if !__JDOOM64__
        TXT_SKILL5
#  endif
    };
#endif
    uint const numObjects = NUM_SKILL_MODES + 1;

    mn_page_t *page = Hu_MenuNewPage("Skill", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawSkillPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
#if __JHEXEN__
    page->setPreviousPage(Hu_MenuFindPageByName("PlayerClass"));
#elif __JHERETIC__
    page->setPreviousPage(Hu_MenuFindPageByName("Episode"));
#elif __JDOOM64__
    page->setPreviousPage(Hu_MenuFindPageByName("GameType"));
#else // __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        page->setPreviousPage(Hu_MenuFindPageByName("GameType"));
    }
    else
    {
        page->setPreviousPage(Hu_MenuFindPageByName("Episode"));
    }
#endif

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    int y = 0;

    for(uint i = 0; i < NUM_SKILL_MODES; ++i, ob++, y += FIXED_LINE_HEIGHT)
    {
        ob->_type          = MN_BUTTON;
        ob->_flags         = skillButtonFlags[i];
#if !__JHEXEN__
        ob->_shortcut      = GET_TXT(skillButtonTexts[i])[0];
#endif
        ob->_origin.y      = y;
        ob->_pageFontIdx   = MENU_FONT1;
        ob->_pageColorIdx  = MENU_COLOR1;
        ob->ticker         = MNButton_Ticker;
        ob->updateGeometry = MNButton_UpdateGeometry;
        ob->drawer         = MNButton_Drawer;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionInitNewGame;
        ob->actions[MNA_FOCUS    ].callback = Hu_MenuFocusSkillMode;
        ob->cmdResponder   = MNButton_CommandResponder;
        ob->data2          = (int)(SM_BABY + i);
        ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
#if !__JHEXEN__
        {
            mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
            btn->text  = INT2PTR(const char, skillButtonTexts[i]);
# if __JDOOM__ || __JDOOM64__
            btn->patch = &pSkillModeNames[i];
# endif
        }
#endif
    }

    ob->_type = MN_NONE;

    page->_objects = objects;

#if __JDOOM__
    if(gameMode != doom2_hacx && gameMode != doom_chex)
    {
        mn_object_t *ob = MN_MustFindObjectOnPage(page, 0, MNF_ID4);
        MNButton_SetFlags(ob, FO_SET, MNBUTTON_NO_ALTTEXT);
    }
#endif
}

void Hu_MenuInitMultiplayerPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(97, 65);
#else
    Point2Raw const origin(97, 65);
#endif
    uint const numObjects = 3;

    mn_page_t *page = Hu_MenuNewPage("Multiplayer", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawMultiplayerPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("GameType"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_BUTTON;
    ob->_flags         = MNF_ID0;
    ob->_shortcut      = 'j';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectJoinGame;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Join Game";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerSetup;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Player Setup";
    }
    ob++;

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitPlayerSetupPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(70, 44);
#else
    Point2Raw const origin(70, 54);
#endif
#if __JHEXEN__
    uint numObjects = 8;
#else
    uint numObjects = 6;
#endif

    mn_page_t *page = Hu_MenuNewPage("PlayerSetup", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawPlayerSetupPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPredefinedFont(MENU_FONT2, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("Multiplayer"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_MOBJPREVIEW;
    ob->_origin.x      = SCREENWIDTH/2 - origin.x;
    ob->_origin.y      = 60;
    ob->_flags         = MNF_ID0 | MNF_POSITION_FIXED;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNMobjPreview_Ticker;
    ob->updateGeometry = MNMobjPreview_UpdateGeometry;
    ob->drawer         = MNMobjPreview_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_mobjpreview_t), PU_GAMESTATIC, 0);
    ob++;

    ob->_type          = MN_EDIT;
    ob->_flags         = MNF_ID1 | MNF_LAYOUT_OFFSET;
    ob->_origin.y      = 75;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNEdit_Ticker;
    ob->updateGeometry = MNEdit_UpdateGeometry;
    ob->drawer         = MNEdit_Drawer;
    ob->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNEdit_CommandResponder;
    ob->responder      = MNEdit_Responder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_edit_t), PU_GAMESTATIC, 0);
    {
        mndata_edit_t *edit = (mndata_edit_t *)ob->_typedata;
        Str_Init(&edit->text);
        Str_Init(&edit->oldtext);
        edit->data1     = (void *)"net-name";
        edit->maxLength = 24;
    }
    ob++;

#if __JHEXEN__
    ob->_type          = MN_TEXT;
    ob->_flags         = MNF_LAYOUT_OFFSET;
    ob->_origin.y      = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Class";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_flags         = MNF_ID2;
    ob->_shortcut      = 'c';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuSelectPlayerSetupPlayerClass;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;
        list->count = 3;
        list->items = (mndata_listitem_t*)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        {
            mndata_listitem_t *item = (mndata_listitem_t *)list->items;
            item->text = (char const *)TXT_PLAYERCLASS1;
            item->data = PCLASS_FIGHTER;
            item++;

            item->text = (char const *)TXT_PLAYERCLASS2;
            item->data = PCLASS_CLERIC;
            item++;

            item->text = (char const *)TXT_PLAYERCLASS3;
            item->data = PCLASS_MAGE;
        }
    }
    ob++;
#endif

    ob->_type          = MN_TEXT;
#ifdef __JHERETIC__
    ob->_flags         = MNF_LAYOUT_OFFSET;
    ob->_origin.y      = 5;
#endif
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t*)ob->_typedata;
        text->text = "Color";
    }
    ob++;

    // Setup the player color selection list.
    ob->_type          = MN_LISTINLINE;
    ob->_flags         = MNF_ID3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuSelectPlayerColor;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;
#if __JHEXEN__
        // Hexen v1.0 has only four player colors.
        list->count = (gameMode == hexen_v10? 4 : NUMPLAYERCOLORS) + 1/*auto*/;
#else
        list->count = NUMPLAYERCOLORS + 1/*auto*/;
#endif
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        /// @todo Read these names from Text definitions.
        {
            mndata_listitem_t *item = (mndata_listitem_t *)list->items;
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

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT2;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectAcceptPlayerSetup;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Save Changes";
    }
    ob++;

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitSaveOptionsPage()
{
    Point2Raw const origin(60, 50);
    uint const numObjects = 8;

    mn_page_t *page = Hu_MenuNewPage("SaveOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Save Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t*)ob->_typedata;
        text->text = "Confirm quick load/save";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'q';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-save-confirm";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Confirm reborn load";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'r';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-save-confirm-loadonreborn";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Reborn preferences";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Load last save";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'a';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-save-last-loadonreborn";
    }
    ob++;

    ob->_type = MN_NONE;

    page->_objects = objects;
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitFilesPage()
{
    Point2Raw origin(110, 60);
    uint const numObjects = 3;

    mn_page_t *page = Hu_MenuNewPage("Files", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    int y = 0;

    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 'l';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadGame;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Load Game";
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type          = MN_BUTTON;
    ob->_origin.y      = y;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveGame;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Save Game";
    }
    ob++; y += FIXED_LINE_HEIGHT;

    ob->_type = MN_NONE;

    page->_objects = objects;
}
#endif

static void deleteGameSave(String slotId)
{
    DD_Executef(true, "deletegamesave %s", slotId.toLatin1().constData());
}

int Hu_MenuLoadSlotCommandResponder(mn_object_t *ob, menucommand_e cmd)
{
    DENG2_ASSERT(ob != 0 && ob->_type == MN_EDIT);
    if(MCMD_DELETE == cmd &&
       (ob->_flags & MNF_FOCUS) && !(ob->_flags & MNF_ACTIVE) && !(ob->_flags & MNF_DISABLED))
    {
        mndata_edit_t *edit = (mndata_edit_t*)ob->_typedata;
        deleteGameSave((char *)edit->data1);
        return true;
    }
    return MNObject_DefaultCommandResponder(ob, cmd);
}

int Hu_MenuSaveSlotCommandResponder(mn_object_t *ob, menucommand_e cmd)
{
    DENG2_ASSERT(ob != 0);
    if(MCMD_DELETE == cmd &&
       (ob->_flags & MNF_FOCUS) && !(ob->_flags & MNF_ACTIVE) && !(ob->_flags & MNF_DISABLED))
    {
        mndata_edit_t *edit = (mndata_edit_t *)ob->_typedata;
        deleteGameSave((char *)edit->data1);
        return true;
    }
    return MNEdit_CommandResponder(ob, cmd);
}

void Hu_MenuInitLoadGameAndSaveGamePages()
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw const origin(80, 54);
#else
    Point2Raw const origin(70, 30);
#endif
    uint const saveSlotObjectIds[NUMSAVESLOTS] = {
        MNF_ID0, MNF_ID1, MNF_ID2, MNF_ID3, MNF_ID4, MNF_ID5,
#if !__JHEXEN__
        MNF_ID6, MNF_ID7
#endif
    };

    mndata_edit_t *saveSlots = (mndata_edit_t *)Z_Calloc(sizeof(*saveSlots) * NUMSAVESLOTS, PU_GAMESTATIC, 0);

    for(int i = 0; i < NUMSAVESLOTS; ++i)
    {
        mndata_edit_t *slot = saveSlots + i;
        slot->emptyString = (char const *) TXT_EMPTYSTRING;
        slot->data1       = Str_Text(Str_Appendf(Str_New(), "%i", i));
        slot->maxLength   = 24;
    }

    mn_object_t *loadMenuObjects = (mn_object_t *)Z_Calloc(sizeof(*loadMenuObjects) * (NUMSAVESLOTS+1), PU_GAMESTATIC, 0);

    int y = 0;

    int i = 0;
    for(; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        mn_object_t *ob     = loadMenuObjects + i;
        mndata_edit_t *edit = saveSlots + i;

        ob->_type          = MN_EDIT;
        ob->_origin.x      = 0;
        ob->_origin.y      = y;
        ob->_flags         = saveSlotObjectIds[i] | MNF_DISABLED;
        ob->_shortcut      = '0' + i;
        ob->_pageFontIdx   = MENU_FONT1;
        ob->_pageColorIdx  = MENU_COLOR1;
        ob->updateGeometry = MNEdit_UpdateGeometry;
        ob->drawer         = MNEdit_Drawer;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadSlot;
        ob->actions[MNA_FOCUSOUT ].callback = Hu_MenuDefaultFocusAction;
        ob->cmdResponder   = Hu_MenuLoadSlotCommandResponder;
        ob->_typedata      = edit;
        ob->data2          = saveSlotObjectIds[i];
        Str_Init(&edit->text);
        Str_Init(&edit->oldtext);
    }
    loadMenuObjects[i]._type = MN_NONE;

    mn_object_t *saveMenuObjects = (mn_object_t *)Z_Calloc(sizeof(*saveMenuObjects) * (NUMSAVESLOTS+1), PU_GAMESTATIC, 0);

    y = 0;

    i = 0;
    for(; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        mn_object_t *ob     = saveMenuObjects + i;
        mndata_edit_t *edit = saveSlots + i;

        ob->_type          = MN_EDIT;
        ob->_origin.x      = 0;
        ob->_origin.y      = y;
        ob->_flags         = saveSlotObjectIds[i];
        ob->_shortcut      = '0' + i;
        ob->_pageFontIdx   = MENU_FONT1;
        ob->_pageColorIdx  = MENU_COLOR1;
        ob->updateGeometry = MNEdit_UpdateGeometry;
        ob->drawer         = MNEdit_Drawer;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveSlot;
        ob->actions[MNA_ACTIVE   ].callback = Hu_MenuSaveSlotEdit;
        ob->actions[MNA_FOCUSOUT ].callback = Hu_MenuDefaultFocusAction;
        ob->cmdResponder   = Hu_MenuSaveSlotCommandResponder;
        ob->responder      = MNEdit_Responder;
        ob->_typedata      = edit;
        ob->data2          = saveSlotObjectIds[i];
    }
    saveMenuObjects[i]._type = MN_NONE;

    mn_page_t *page = Hu_MenuNewPage("LoadGame", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawLoadGamePage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));
    page->_objects = loadMenuObjects;

    page = Hu_MenuNewPage("SaveGame", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawSaveGamePage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));
    page->_objects = saveMenuObjects;
}

void Hu_MenuInitOptionsPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(110, 63);
#else
    Point2Raw const origin(110, 63);
#endif
#if __JHERETIC__ || __JHEXEN__
    uint const numObjects = 13;
#else
    uint const numObjects = 12;
#endif

    mn_page_t *page = Hu_MenuNewPage("Options", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawOptionsPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'e';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectEndGame;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "End Game";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 't';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Show Taskbar";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'c';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"ControlOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Controls";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'g';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"GameplayOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Gameplay";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"SaveOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Game saves";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'h';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"HUDOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "HUD";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'a';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"AutomapOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Automap";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'w';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"WeaponOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Weapons";
    }
    ob++;

#if __JHERETIC__ || __JHEXEN__
    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'i';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"InventoryOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Inventory";
    }
    ob++;
#endif

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data1          = (void *)"SoundOptions";
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Sound";
    }
    ob++;

    /*
    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'm';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data2          = 2;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Mouse";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'j';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->data2          = 2;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Joystick";
    }
    ob++;*/

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitGameplayOptionsPage()
{
#if __JHEXEN__
    Point2Raw const origin(88, 25);
#elif __JHERETIC__
    Point2Raw const origin(30, 40);
#else
    Point2Raw const origin(30, 40);
#endif
#if __JDOOM64__
    uint const numObjects = 40;
#elif __JDOOM__
    uint const numObjects = 42;
#elif __JHERETIC__
    uint const numObjects = 24;
#elif __JHEXEN__
    uint const numObjects = 7;
#endif

    mn_page_t *page = Hu_MenuNewPage("GameplayOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Gameplay Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Always Run";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'r';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t*)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"ctl-run";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t*)ob->_typedata;
        text->text = "Use LookSpring";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'l';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t*)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"ctl-look-spring";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Disable AutoAim";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'a';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"ctl-aim-noauto";
    }
    ob++;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Allow Jumping";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'j';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"player-jump";
    }
    ob++;
#endif

#if __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Weapon Recoil";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"player-weapon-recoil";
    }
    ob++;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Compatibility";
    }
    ob++;

# if __JDOOM__ || __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Any Boss Trigger 666";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'b';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-anybossdeath666";
    }
    ob++;

#  if !__JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Av Resurrects Ghosts";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'g';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-raiseghosts";
    }
    ob++;

# if __JDOOM__
    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "VileChase uses Av radius";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'g';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-vilechase-usevileradius";
    }
    ob++;
# endif
#  endif // !__JDOOM64__

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "PE Limited To 21 Lost Souls";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'p';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-maxskulls";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "LS Can Get Stuck Inside Walls";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-skullsinwalls";
    }
    ob++;
# endif // __JDOOM__ || __JDOOM64__

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Monsters Fly Over Obstacles";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-monsters-floatoverblocking";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Monsters Can Get Stuck In Doors";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'd';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-monsters-stuckindoors";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Some Objects Never Hang Over Ledges";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'h';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-objects-neverhangoverledges";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Objects Fall Under Own Weight";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'f';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-objects-falloff";
    }
    ob++;

#if __JDOOM__ || __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "All Crushed Objects Become A Pile Of Gibs";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'g';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-objects-gibcrushednonbleeders";
    }
    ob++;
#endif

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Corpses Slide Down Stairs";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-corpse-sliding";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Use Exactly Doom's Clipping Code";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'c';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-objects-clipping";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "  ^If Not NorthOnly WallRunning";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'w';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-player-wallrun-northonly";
    }
    ob++;

# if __JDOOM__ || __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Zombie Players Can Exit Maps";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'e';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"game-zombiescanexit";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Fix Ouch Face";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-face-ouchfix";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Fix Weapon Slot Display";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-status-weaponslots-ownedfix";
    }
    ob++;
# endif // __JDOOM__ || __JDOOM64__
#endif // __JDOOM__ || __JHERETIC__ || __JDOOM64__

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitHUDOptionsPage()
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw const origin(97, 40);
#else
    Point2Raw const origin(97, 28);
#endif
#if __JDOOM64__
    uint const numObjects = 68;
#elif __JDOOM__
    uint const numObjects = 75;
#elif __JHERETIC__
    uint const numObjects = 75;
#elif __JHEXEN__
    uint const numObjects = 60;
#endif

    mn_page_t *page = Hu_MenuNewPage("HudOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("HUD Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "View Size";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 3;
#if __JDOOM64__
        sld->max       = 11;
#else
        sld->max       = 13;
#endif
        sld->value     = 0;
        sld->step      = 1;
        sld->floatMode = false;
        sld->data1     = (void *)"view-size";
    }
    ob++;

#if __JDOOM__
    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Single Key Display";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data = (void *)"hud-keys-combine";
    }
    ob++;
#endif

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "AutoHide";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_TextualValueUpdateGeometry;
    ob->drawer         = MNSlider_TextualValueDrawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 60;
        sld->value     = 0;
        sld->step      = 1;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-timer";
        sld->data2     = (void *)"Disabled";
        sld->data4     = (void *)" second";
        sld->data5     = (void *)" seconds";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "UnHide Events";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Receive Damage";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-damage";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Health";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-pickup-health";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Armor";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-pickup-armor";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Powerup";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-pickup-powerup";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Weapon";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-pickup-weapon";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
#if __JHEXEN__
        text->text = "Pickup Mana";
#else
        text->text = "Pickup Ammo";
#endif
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-pickup-ammo";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Key";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-pickup-key";
    }
    ob++;

#if __JHERETIC__ || __JHEXEN__
    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Item";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-unhide-pickup-invitem";
    }
    ob++;
#endif // __JHERETIC__ || __JHEXEN__

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Messages";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Shown";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 2;
    ob->_shortcut      = 'm';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"msg-show";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Uptime";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_TextualValueUpdateGeometry;
    ob->drawer         = MNSlider_TextualValueDrawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 60;
        sld->value     = 0;
        sld->step      = 1;
        sld->floatMode = true;
        sld->data1     = (void *)"msg-uptime";
        sld->data2     = (void *)"Disabled";
        sld->data4     = (void *)" second";
        sld->data5     = (void *)" seconds";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Size";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = .1f;
        sld->floatMode = true;
        sld->data1     = (void *)"msg-scale";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Color";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED ].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE   ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->rgbaMode = false;
        cbox->data1    = (void *)"msg-color-r";
        cbox->data2    = (void *)"msg-color-g";
        cbox->data3    = (void *)"msg-color-b";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Crosshair";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 3;
    ob->_shortcut      = 'c';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Symbol";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;
        list->data  = (void *)"view-cross-type";
        list->count = 6;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        {
            mndata_listitem_t *item = (mndata_listitem_t *)list->items;
            item->text = "None";
            item->data = 0;
            item++;

            item->text = "Cross";
            item->data = 1;
            item++;

            item->text = "Twin Angles";
            item->data = 2;
            item++;

            item->text = "Square";
            item->data = 3;
            item++;

            item->text = "Open Square";
            item->data = 4;
            item++;

            item->text = "Angle";
            item->data = 5;
        }
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Size";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = .1f;
        sld->floatMode = true;
        sld->data1     = (void *)"view-cross-size";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Angle";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.0625f;
        sld->floatMode = true;
        sld->data1     = (void *)"view-cross-angle";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Opacity";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"view-cross-a";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Vitality Color";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"view-cross-vitality";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Color";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_group         = 3;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED ].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE   ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->rgbaMode = false;
        cbox->data1    = (void *)"view-cross-r";
        cbox->data2    = (void *)"view-cross-g";
        cbox->data3    = (void *)"view-cross-b";
    }
    ob++;

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    ob->_type          = MN_TEXT;
    ob->_group         = 4;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Statusbar";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 4;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Size";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 4;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-status-size";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 4;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Opacity";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 4;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-status-alpha";
    }
    ob++;

#endif // __JDOOM__ || __JHERETIC__ || __JHEXEN__

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    ob->_type          = MN_TEXT;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Counters";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Items";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_group         = 5;
    ob->_shortcut      = 'i';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;
        list->data  = (void *)"hud-cheat-counter";
        list->mask  = CCH_ITEMS | CCH_ITEMS_PRCNT;
        list->count = 4;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        {
            mndata_listitem_t *item = (mndata_listitem_t *)list->items;
            item->text = "Hidden";
            item->data = 0;
            item++;

            item->text = "Count";
            item->data = CCH_ITEMS;
            item++;

            item->text = "Percent";
            item->data = CCH_ITEMS_PRCNT;
            item++;

            item->text = "Count+Percent";
            item->data = CCH_ITEMS | CCH_ITEMS_PRCNT;
        }
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Kills";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_group         = 5;
    ob->_shortcut      = 'k';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;
        list->data  = (void *)"hud-cheat-counter";
        list->mask  = CCH_KILLS | CCH_KILLS_PRCNT;
        list->count = 4;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        {
            mndata_listitem_t *item = (mndata_listitem_t *)list->items;
            item->text = "Hidden";
            item->data = 0;
            item++;

            item->text = "Count";
            item->data = CCH_KILLS;
            item++;

            item->text = "Percent";
            item->data = CCH_KILLS_PRCNT;
            item++;

            item->text = "Count+Percent";
            item->data = CCH_KILLS | CCH_KILLS_PRCNT;
        }
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Secrets";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_group         = 5;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;
        list->data  = (void *)"hud-cheat-counter";
        list->mask  = CCH_SECRETS | CCH_SECRETS_PRCNT;
        list->count = 4;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        {
            mndata_listitem_t *item = (mndata_listitem_t *)list->items;
            item->text = "Hidden";
            item->data = 0;
            item++;

            item->text = "Count";
            item->data = CCH_SECRETS;
            item++;

            item->text = "Percent";
            item->data = CCH_SECRETS_PRCNT;
            item++;

            item->text = "Count+Percent";
            item->data = CCH_SECRETS | CCH_SECRETS_PRCNT;
        }
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Automap Only";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-cheat-counter-show-mapopen";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Size";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 5;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-cheat-counter-scale";
    }
    ob++;

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Fullscreen";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Size";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-scale";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Text Color";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED ].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE   ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->rgbaMode = true;
        cbox->data1    = (void *)"hud-color-r";
        cbox->data2    = (void *)"hud-color-g";
        cbox->data3    = (void *)"hud-color-b";
        cbox->data4    = (void *)"hud-color-a";
    }
    ob++;

#if __JHEXEN__
    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Mana";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-mana";
    }
    ob++;

#endif // __JHEXEN__

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Ammo";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_shortcut      = 'a';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-ammo";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Armor";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_shortcut      = 'r';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-armor";
    }
    ob++;

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

#if __JDOOM64__
    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show PowerKeys";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_shortcut      = 'p';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-power";
    }
    ob++;

#endif // __JDOOM64__

#if __JDOOM__
    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Status";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_shortcut      = 'f';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-face";
    }
    ob++;

#endif // __JDOOM__

    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Health";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_shortcut      = 'h';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-health";
    }
    ob++;

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Keys";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-keys";
    }
    ob++;

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

#if __JHERETIC__ || __JHEXEN__
    ob->_type          = MN_TEXT;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Ready-Item";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 6;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-currentitem";
    }
    ob++;

#endif // __JHERETIC__ || __JHEXEN__

    ob->_type = MN_NONE;

    page->_objects = objects;
}

void Hu_MenuInitAutomapOptionsPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(64, 28);
#else
    Point2Raw const origin(70, 40);
#endif
#if __JDOOM64__
    uint const numObjects = 26;
#else
    uint const numObjects = 27;
#endif

    mn_page_t *page = Hu_MenuNewPage("AutomapOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Automap Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Background Opacity";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_shortcut      = 'o';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"map-opacity";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Line Opacity";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_shortcut      = 'l';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 1;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"map-line-opacity";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Line Width";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = .1f;
        sld->max       = 2;
        sld->value     = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"map-line-width";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "HUD Display";
    }
    ob++;

#if !__JDOOM64__
    ob->_type          = MN_LISTINLINE;
    ob->_shortcut      = 'h';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;

        list->data  = (void *)"map-huddisplay";
        list->count = 3;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        mndata_listitem_t *item = (mndata_listitem_t *)list->items;
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

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Door Colors";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'd';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"map-door-colors";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
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
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 200;
        sld->value     = 0;
        sld->step      = 5;
        sld->floatMode = true;
        sld->data1     = (void *)"map-door-glow";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Use Custom Colors";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;

        list->count = 3;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);
        list->data  = (void *)"map-customcolors";

        mndata_listitem_t *item = (mndata_listitem_t *)list->items;
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

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Wall";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_shortcut      = 'w';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->data1 = (void *)"map-wall-r";
        cbox->data2 = (void *)"map-wall-g";
        cbox->data3 = (void *)"map-wall-b";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Floor Height Change";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_shortcut      = 'f';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->data1 = (void *)"map-wall-floorchange-r";
        cbox->data2 = (void *)"map-wall-floorchange-g";
        cbox->data3 = (void *)"map-wall-floorchange-b";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Ceiling Height Change";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->data1 = (void *)"map-wall-ceilingchange-r";
        cbox->data2 = (void *)"map-wall-ceilingchange-g";
        cbox->data3 = (void *)"map-wall-ceilingchange-b";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Unseen";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_shortcut      = 'u';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->data1 = (void *)"map-wall-unseen-r";
        cbox->data2 = (void *)"map-wall-unseen-g";
        cbox->data3 = (void *)"map-wall-unseen-b";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Thing";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_shortcut      = 't';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->data1 = (void *)"map-mobj-r";
        cbox->data2 = (void *)"map-mobj-g";
        cbox->data3 = (void *)"map-mobj-b";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Background";
    }
    ob++;

    ob->_type          = MN_COLORBOX;
    ob->_shortcut      = 'b';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNColorBox_Ticker;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->drawer         = MNColorBox_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarColorBox;
    ob->actions[MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNColorBox_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    {
        mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;
        cbox->data1 = (void *)"map-background-r";
        cbox->data2 = (void *)"map-background-g";
        cbox->data3 = (void *)"map-background-b";
    }
    ob++;

    ob->_type = MN_NONE;

    page->_objects = objects;
}

static int compareWeaponPriority(void const *_a, void const *_b)
{
    mndata_listitem_t const *a = (mndata_listitem_t const *)_a;
    mndata_listitem_t const *b = (mndata_listitem_t const *)_b;
    int i = 0, aIndex = -1, bIndex = -1;

    do
    {
        if(cfg.weaponOrder[i] == a->data)
        {
            aIndex = i;
        }
        if(cfg.weaponOrder[i] == b->data)
        {
            bIndex = i;
        }
    } while(!(aIndex != -1 && bIndex != -1) && ++i < NUM_WEAPON_TYPES);

    if(aIndex > bIndex) return 1;
    if(aIndex < bIndex) return -1;
    return 0; // Should never happen.
}

void Hu_MenuInitWeaponsPage()
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw const origin(78, 40);
#elif __JHERETIC__
    Point2Raw const origin(78, 26);
#elif __JHEXEN__
    Point2Raw const origin(78, 38);
#endif
#if __JDOOM__ || __JDOOM64__
    uint const numObjects = 17;
#elif __JHERETIC__ || __JHEXEN__
    uint const numObjects = 15;
#endif
    const struct {
        char const *text;
        weapontype_t data;
    } weaponOrder[NUM_WEAPON_TYPES+1] = {
#if __JDOOM__ || __JDOOM64__
        { (char const *)TXT_WEAPON1,             WT_FIRST },
        { (char const *)TXT_WEAPON2,             WT_SECOND },
        { (char const *)TXT_WEAPON3,             WT_THIRD },
        { (char const *)TXT_WEAPON4,             WT_FOURTH },
        { (char const *)TXT_WEAPON5,             WT_FIFTH },
        { (char const *)TXT_WEAPON6,             WT_SIXTH },
        { (char const *)TXT_WEAPON7,             WT_SEVENTH },
        { (char const *)TXT_WEAPON8,             WT_EIGHTH },
        { (char const *)TXT_WEAPON9,             WT_NINETH },
#  if __JDOOM64__
        { (char const *)TXT_WEAPON10,            WT_TENTH },
#  endif
#elif __JHERETIC__
        { (char const *)TXT_TXT_WPNSTAFF,        WT_FIRST },
        { (char const *)TXT_TXT_WPNWAND,         WT_SECOND },
        { (char const *)TXT_TXT_WPNCROSSBOW,     WT_THIRD },
        { (char const *)TXT_TXT_WPNBLASTER,      WT_FOURTH },
        { (char const *)TXT_TXT_WPNSKULLROD,     WT_FIFTH },
        { (char const *)TXT_TXT_WPNPHOENIXROD,   WT_SIXTH },
        { (char const *)TXT_TXT_WPNMACE,         WT_SEVENTH },
        { (char const *)TXT_TXT_WPNGAUNTLETS,    WT_EIGHTH },
#elif __JHEXEN__
        /// @todo We should allow different weapon preferences per player-class.
        { "First",  WT_FIRST },
        { "Second", WT_SECOND },
        { "Third",  WT_THIRD },
        { "Fourth", WT_FOURTH },
#endif
        { "", WT_NOCHANGE}
    };

    mn_page_t *page = Hu_MenuNewPage("WeaponOptions", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawWeaponsPage, NULL, NULL);
    page->setTitle("Weapons Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Priority Order";
    }
    ob++;

    ob->_type          = MN_LIST;
    ob->_flags         = MNF_ID0;
    ob->_shortcut      = 'p';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNList_Ticker;
    ob->updateGeometry = MNList_UpdateGeometry;
    ob->drawer         = MNList_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuChangeWeaponPriority;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNList_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;

        list->count = NUM_WEAPON_TYPES;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);

        mndata_listitem_t *item = (mndata_listitem_t *)list->items;
        for(int i = 0; weaponOrder[i].data < NUM_WEAPON_TYPES; ++i, item++)
        {
            item->text = weaponOrder[i].text;
            item->data = weaponOrder[i].data;
        }
        qsort(list->items, list->count, sizeof(mndata_listitem_t), compareWeaponPriority);
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Cycling";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Use Priority Order";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'o';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"player-weapon-nextmode";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Sequential";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"player-weapon-cycle-sequential";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Autoswitch";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Weapon";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_group         = 2;
    ob->_shortcut      = 'w';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;

        list->count = 3;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);
        list->data  = (void *)"player-autoswitch";

        mndata_listitem_t *item = (mndata_listitem_t *)list->items;
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

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "   If Not Firing";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 2;
    ob->_shortcut      = 'f';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"player-autoswitch-notfiring";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Ammo";
    }
    ob++;

    ob->_type          = MN_LISTINLINE;
    ob->_group         = 2;
    ob->_shortcut      = 'a';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNListInline_Ticker;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->drawer         = MNListInline_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarList;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNListInline_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    {
        mndata_list_t *list = (mndata_list_t *)ob->_typedata;

        list->count = 3;
        list->items = (mndata_listitem_t *)Z_Calloc(sizeof(mndata_listitem_t) * list->count, PU_GAMESTATIC, 0);
        list->data  = (void *)"player-autoswitch-ammo";

        mndata_listitem_t *item = (mndata_listitem_t *)list->items;
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
    ob->_type          = MN_TEXT;
    ob->_group         = 2;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Pickup Beserk";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 2;
    ob->_shortcut      = 'b';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"player-autoswitch-berserk";
    }
    ob++;
#endif

    ob->_type = MN_NONE;

    page->_objects = objects;
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitInventoryOptionsPage()
{
    Point2Raw const origin(78, 48);
    uint const numObjects = 16;

    mn_page_t *page = Hu_MenuNewPage("InventoryOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Inventory Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Select Mode";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"ctl-inventory-mode";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Wrap Around";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'w';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"ctl-inventory-wrap";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Choose And Use";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'c';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"ctl-inventory-use-immediate";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Select Next If Use Failed";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'n';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"ctl-inventory-use-next";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "AutoHide";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_shortcut      = 'h';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_TextualValueUpdateGeometry;
    ob->drawer         = MNSlider_TextualValueDrawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 30;
        sld->value     = 0;
        sld->step      = 1.f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-inventory-timer";
        sld->data2     = (void *)"Disabled";
        sld->data4     = (void *)" second";
        sld->data5     = (void *)" seconds";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR2;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Fullscreen HUD";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Max Visible Slots";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_group         = 1;
    ob->_shortcut      = 'v';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_TextualValueUpdateGeometry;
    ob->drawer         = MNSlider_TextualValueDrawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 16;
        sld->value     = 0;
        sld->step      = 1;
        sld->floatMode = false;
        sld->data1     = (void *)"hud-inventory-slot-max";
        sld->data2     = (void *)"Automatic";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_group         = 1;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "Show Empty Slots";
    }
    ob++;

    ob->_type          = MN_BUTTON;
    ob->_group         = 1;
    ob->_shortcut      = 'e';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR3;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarButton;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->staydownMode = true;
        btn->data         = (void *)"hud-inventory-slot-showempty";
    }
    ob++;

    ob->_type = MN_NONE;

    page->_objects = objects;
}
#endif

void Hu_MenuInitSoundOptionsPage()
{
#if __JHEXEN__
    Point2Raw const origin(97, 25);
#elif __JHERETIC__
    Point2Raw const origin(97, 30);
#elif __JDOOM__ || __JDOOM64__
    Point2Raw const origin(97, 40);
#endif
    uint const numObjects = 6;

    mn_page_t *page = Hu_MenuNewPage("SoundOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Sound Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    mn_object_t *objects = (mn_object_t *)Z_Calloc(sizeof(*objects) * numObjects, PU_GAMESTATIC, 0);
    mn_object_t *ob = objects;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
        text->text = "SFX Volume";
    }
    ob++;

    ob->_type          = MN_SLIDER;
    ob->_shortcut      = 's';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNSlider_Ticker;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->drawer         = MNSlider_Drawer;
    ob->actions[MNA_MODIFIED].callback = Hu_MenuCvarSlider;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNSlider_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 255;
        sld->value     = 0;
        sld->step      = 5;
        sld->floatMode = false;
        sld->data1     = (void *)"sound-volume";
    }
    ob++;

    ob->_type          = MN_TEXT;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNText_Ticker;
    ob->updateGeometry = MNText_UpdateGeometry;
    ob->drawer         = MNText_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    {
        mndata_text_t *text = (mndata_text_t *)ob->_typedata;
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
    {
        mndata_slider_t *sld = (mndata_slider_t *)ob->_typedata;
        sld->min       = 0;
        sld->max       = 255;
        sld->value     = 0;
        sld->step      = 5;
        sld->floatMode = false;
        sld->data1     = (void *)"music-volume";
    }
    ob++;

    /*
    ob->_type          = MN_BUTTON;
    ob->_shortcut      = 'p';
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->ticker         = MNButton_Ticker;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->drawer         = MNButton_Drawer;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
    ob->actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->_typedata      = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    ob->data2          = 1;
    {
        mndata_button_t *btn = (mndata_button_t *)ob->_typedata;
        btn->text = "Open Audio Panel";
    }
    ob++;
    */

    ob->_type = MN_NONE;

    page->_objects = objects;
}

#if __JDOOM__ || __JHERETIC__
/**
 * Construct the episode selection menu.
 */
void Hu_MenuInitEpisodePage()
{
#if __JDOOM__
    Point2Raw const origin(48, 63);
#else
    Point2Raw const origin(80, 50);
#endif

    int numEpisodes;
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
    mn_object_t *objects     =     (mn_object_t *)Z_Calloc(    sizeof(mn_object_t) * (numEpisodes + 1), PU_GAMESTATIC, 0);
    mndata_button_t *buttons = (mndata_button_t *)Z_Calloc(sizeof(mndata_button_t) *     (numEpisodes), PU_GAMESTATIC, 0);

    mn_object_t *ob      = objects;
    mndata_button_t *btn = buttons;

    int y = 0;

    for(int i = 0; i < numEpisodes; ++i)
    {
        ob->_type         = MN_BUTTON;
        ob->_origin.x     = 0;
        ob->_origin.y     = y;

        btn->text = GET_TXT(TXT_EPISODE1 + i);
        if(isalnum(btn->text[0]))
        {
            ob->_shortcut = tolower(btn->text[0]);
        }
#if __JDOOM__
        btn->patch = &pEpisodeNames[i];
#endif

        ob->_typedata      = btn;
        ob->ticker         = MNButton_Ticker;
        ob->drawer         = MNButton_Drawer;
        ob->cmdResponder   = MNButton_CommandResponder;
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
            ob->data1 = (void *)"Skill";
#if __JHERETIC__
            if(gameMode == heretic_extended && i == 5)
            {
                ob->_flags |= MNF_ID0;
            }
#endif
        }

        ob->actions[MNA_FOCUS].callback = Hu_MenuFocusEpisode;
        ob->data2           = i;
        ob->_pageFontIdx    = MENU_FONT1;
        ob++;
        btn++;
        y += FIXED_LINE_HEIGHT;
    }
    ob->_type = MN_NONE;

    mn_page_t *page = Hu_MenuNewPage("Episode", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawEpisodePage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("GameType"));

    page->_objects = objects;
}
#endif

#if __JHEXEN__
/**
 * Construct the player class selection menu.
 */
void Hu_MenuInitPlayerClassPage()
{
    Point2Raw const pageOrigin(66, 66);

    // First determine the number of selectable player classes.
    int count = 0;
    for(int i = 0; i < NUM_PLAYER_CLASSES; ++i)
    {
        classinfo_t *info = PCLASS_INFO(i);
        if(info->userSelectable)
        {
            ++count;
        }
    }

    // Allocate the menu objects.
    mn_object_t *objects     =     (mn_object_t *)Z_Calloc(    sizeof(mn_object_t) * (count+4), PU_GAMESTATIC, 0);
    mndata_button_t *buttons = (mndata_button_t *)Z_Calloc(sizeof(mndata_button_t) * (count+1), PU_GAMESTATIC, 0);

    uint y = 0;

    // Add the selectable classes.
    mn_object_t *ob      = objects;
    mndata_button_t *btn = buttons;

    int n = 0;
    while(n < count)
    {
        classinfo_t *info = PCLASS_INFO(n++);

        if(!info->userSelectable) continue;

        ob->_type          = MN_BUTTON;

        btn->text = info->niceName;

        ob->_typedata      = btn;
        ob->_origin.x      = 0;
        ob->_origin.y      = y;
        ob->drawer         = MNButton_Drawer;
        ob->ticker         = MNButton_Ticker;
        ob->cmdResponder   = MNButton_CommandResponder;
        ob->updateGeometry = MNButton_UpdateGeometry;
        ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
        ob->actions[MNA_FOCUS    ].callback = Hu_MenuFocusOnPlayerClass;
        ob->data2          = (int)info->plrClass;
        ob->_shortcut      = tolower(btn->text[0]);
        ob->_pageFontIdx   = MENU_FONT1;
        ob->_pageColorIdx  = MENU_COLOR1;

        ob++;
        btn++;
        y += FIXED_LINE_HEIGHT;
    }

    // Random class button.
    ob->_type          = MN_BUTTON;

    btn->text = GET_TXT(TXT_RANDOMPLAYERCLASS);

    ob->_typedata      = btn;
    ob->_origin.x      = 0;
    ob->_origin.y      = y;
    ob->drawer         = MNButton_Drawer;
    ob->ticker         = MNButton_Ticker;
    ob->cmdResponder   = MNButton_CommandResponder;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->actions[MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
    ob->actions[MNA_FOCUS    ].callback = Hu_MenuFocusOnPlayerClass;
    ob->data2          = (int)PCLASS_NONE;
    ob->_shortcut      = tolower(btn->text[0]);
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob++;

    // Mobj preview background.
    ob->_type          = MN_RECT;
    ob->_flags         = MNF_NO_FOCUS|MNF_ID1;
    ob->_origin.x      = 108;
    ob->_origin.y      = -58;
    ob->drawer         = MNRect_Drawer;
    ob->ticker         = Hu_MenuPlayerClassBackgroundTicker;
    ob->updateGeometry = MNRect_UpdateGeometry;
    ob->_pageFontIdx   = MENU_FONT1;
    ob->_pageColorIdx  = MENU_COLOR1;
    ob->_typedata      = Z_Calloc(sizeof(mndata_rect_t), PU_GAMESTATIC, 0);
    ob++;

    // Mobj preview.
    ob->_type          = MN_MOBJPREVIEW;
    ob->_flags         = MNF_ID0;
    ob->_origin.x      = 108 + 55;
    ob->_origin.y      = -58 + 76;
    ob->ticker         = Hu_MenuPlayerClassPreviewTicker;
    ob->updateGeometry = MNMobjPreview_UpdateGeometry;
    ob->drawer         = MNMobjPreview_Drawer;
    ob->_typedata      = Z_Calloc(sizeof(mndata_mobjpreview_t), PU_GAMESTATIC, 0);
    ob++;

    // Terminate.
    ob->_type = MN_NONE;

    mn_page_t *page = Hu_MenuNewPage("PlayerClass", &pageOrigin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawPlayerClassPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("GameType"));

    page->_objects = objects;
}
#endif

static mn_page_t *addPageToCollection(mn_page_t *page, String name)
{
    String nameInIndex = name.toLower();
    if(pages.contains(nameInIndex))
    {
        delete pages[nameInIndex];
    }
    pages.insert(nameInIndex, page);
    return page;
}

mn_page_t *Hu_MenuNewPage(char const *name, Point2Raw const *origin, int flags,
    void (*ticker) (mn_page_t *page),
    void (*drawer) (mn_page_t *page, Point2Raw const *origin),
    int (*cmdResponder) (mn_page_t *page, menucommand_e cmd),
    void *userData)
{
    DENG2_ASSERT(origin != 0);
    DENG2_ASSERT(name != 0 && name[0]);
    return addPageToCollection(new mn_page_t(*origin, flags, ticker, drawer, cmdResponder, userData), name);
}

void Hu_MenuInit()
{
    cvarbutton_t *cvb;

    if(inited) return;

    mnAlpha = mnTargetAlpha = 0;
    menuActivePage    = 0;
    menuActive        = false;
    cursorHasRotation = false;
    cursorAngle       = 0;
    cursorAnimFrame   = 0;
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
        mn_object_t *ob = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("Main"), 0, MNF_ID0); // Read This!
        ob->setFlags(FO_SET, MNF_DISABLED|MNF_HIDDEN|MNF_NO_FOCUS);

        ob = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("Main"), 0, MNF_ID1); // Quit Game
        ob->setFixedY(ob->fixedY() - FIXED_LINE_HEIGHT);
    }
#endif

    inited = true;
}

void Hu_MenuShutdown()
{
    if(!inited) return;

    destroyAllPages();
    inited = false;
}

dd_bool Hu_MenuIsActive()
{
    return menuActive;
}

void Hu_MenuSetAlpha(float alpha)
{
    // The menu's alpha will start moving towards this target value.
    mnTargetAlpha = alpha;
}

float Hu_MenuAlpha()
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

mn_page_t* Hu_MenuActivePage()
{
    return menuActivePage;
}

void Hu_MenuSetActivePage2(mn_page_t *page, dd_bool canReactivate)
{
    if(!menuActive) return;
    if(!page) return;

    if(!(Get(DD_DEDICATED) || Get(DD_NOVIDEO)))
    {
        FR_ResetTypeinTimer();
    }

    cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
    menuNominatingQuickSaveSlot = false;

    if(menuActivePage == page)
    {
        if(!canReactivate) return;
        page->clearFocusObject();
    }

    page->updateObjects();

    // This is now the "active" page.
    menuActivePage = page;
    page->initialize();
}

void Hu_MenuSetActivePage(mn_page_t *page)
{
    Hu_MenuSetActivePage2(page, false/*don't reactivate*/);
}

dd_bool Hu_MenuIsVisible()
{
    return (menuActive || mnAlpha > .0001f);
}

int Hu_MenuDefaultFocusAction(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
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

static void beginOverlayDraw()
{
#define SMALL_SCALE             .75f

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);

#undef SMALL_SCALE
}

static void endOverlayDraw()
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Hu_MenuDrawer()
{
#define OVERLAY_DARKEN          .7f

    dgl_borderedprojectionstate_t bp;

    if(!Hu_MenuIsVisible()) return;

    GL_ConfigureBorderedProjection(&bp, 0, SCREENWIDTH, SCREENHEIGHT,
        Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.menuScaleMode));
    GL_BeginBorderedProjection(&bp);

    // First determine whether the focus cursor should be visible.
    mn_object_t *focusOb   = Hu_MenuActivePage()->focusObject();
    dd_bool showFocusCursor = true;
    if(focusOb && (focusOb->flags() & MNF_ACTIVE))
    {
        if(focusOb->type() == MN_COLORBOX || focusOb->type() == MN_BINDINGS)
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
    if(focusOb && (focusOb->flags() & MNF_ACTIVE))
    {
        switch(focusOb->type())
        {
        case MN_COLORBOX:
        case MN_BINDINGS:
            drawOverlayBackground(OVERLAY_DARKEN);
            GL_BeginBorderedProjection(&bp);

            beginOverlayDraw();
            if(focusOb->type() == MN_BINDINGS)
            {
                Hu_MenuControlGrabDrawer(MNBindings_ControlName(focusOb), 1);
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

void Hu_MenuPageTicker(mn_page_t *page)
{
    // Normal ticker actions first.
    page->tick();

    /// @todo Move game-menu specific page tick functionality here.
}

void Hu_MenuNavigatePage(mn_page_t * /*page*/, int /*pageDelta*/)
{
#if 0
    DENG2_ASSERT(page != 0);

    int index = MAX_OF(0, page->focus);

    oldIndex = index;

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
        page->setFocus(page->objects + index);
    }
#endif
}

static void initAllPages()
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
    Hu_MenuInitSaveOptionsPage();
    Hu_MenuInitHUDOptionsPage();
    Hu_MenuInitAutomapOptionsPage();
    Hu_MenuInitWeaponsPage();
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuInitInventoryOptionsPage();
#endif
    Hu_MenuInitSoundOptionsPage();
    Hu_MenuInitControlsPage();
}

static void destroyAllPages()
{
    qDeleteAll(pages);
    pages.clear();
}

static void initAllObjectsOnAllPages()
{
    foreach(mn_page_t *page, pages)
    {
        page->initObjects();
    }
}

int Hu_MenuColorWidgetCmdResponder(mn_page_t *page, menucommand_e cmd)
{
    DENG2_ASSERT(page != 0);
    switch(cmd)
    {
    case MCMD_NAV_OUT: {
        mn_object_t *ob = (mn_object_t *)page->userData;
        ob->setFlags(FO_CLEAR, MNF_ACTIVE);
        S_LocalSound(SFX_MENU_CANCEL, NULL);
        colorWidgetActive = false;

        /// @kludge We should re-focus on the object instead.
        cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true; }

    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        return true; // Eat these.

    case MCMD_SELECT: {
        mn_object_t *ob = (mn_object_t *)page->userData;
        ob->setFlags(FO_CLEAR, MNF_ACTIVE);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        colorWidgetActive = false;
        MNColorBox_CopyColor(ob, 0, MN_MustFindObjectOnPage(page, 0, MNF_ID0));

        /// @kludge We should re-focus on the object instead.
        cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true; }

    default: break;
    }

    return false;
}

static void fallbackCommandResponder(mn_page_t *page, menucommand_e cmd)
{
    DENG2_ASSERT(page != 0);
    switch(cmd)
    {
    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        S_LocalSound(cmd == MCMD_NAV_PAGEUP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
        Hu_MenuNavigatePage(page, cmd == MCMD_NAV_PAGEUP? -1 : +1);
        break;

    case MCMD_NAV_UP:
    case MCMD_NAV_DOWN:
        // An object on this page must have focus in order to navigate.
        if(page->focusObject())
        {
            int i = 0, giveFocus = page->focus;
            do
            {
                giveFocus += (cmd == MCMD_NAV_UP? -1 : 1);
                if(giveFocus < 0)
                    giveFocus = page->objectsCount() - 1;
                else if(giveFocus >= page->objectsCount())
                    giveFocus = 0;
            } while(++i < page->objectsCount() && (page->objects()[giveFocus].flags() & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN)));

            if(giveFocus != page->focus)
            {
                S_LocalSound(cmd == MCMD_NAV_UP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
                page->setFocus(&page->objects()[giveFocus]);
            }
        }
        break;

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
        if(mn_object_t *ob = Hu_MenuActivePage()->focusObject())
        {
            switch(ob->type())
            {
            case MN_EDIT:
            case MN_LIST:
            case MN_COLORBOX:
                if(ob->flags() & MNF_ACTIVE)
                {
                    cmd = MCMD_NAV_OUT;
                }
                break;

            default: break;
            }
        }
    }

    return cmd;
}

void Hu_MenuCommand(menucommand_e cmd)
{
    cmd = translateCommand(cmd);

    // Determine the page which will respond to this command.
    mn_page_t *page = colorWidgetActive? Hu_MenuFindPageByName("ColorWidget") : Hu_MenuActivePage();

    if(cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST)
    {
        if(menuActive)
        {
            //BusyMode_FreezeGameForBusyMode();

            menuNominatingQuickSaveSlot = false;

            Hu_FogEffectSetAlphaTarget(0);

            if(cmd == MCMD_CLOSEFAST)
            {
                // Hide the menu instantly.
                mnAlpha = mnTargetAlpha = 0;
            }
            else
            {
                mnTargetAlpha = 0;
            }

            if(cmd != MCMD_CLOSEFAST)
            {
                S_LocalSound(SFX_MENU_CLOSE, NULL);
            }

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
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                if(ST_ChatIsActive(i))
                    return;
            }

            S_LocalSound(SFX_MENU_OPEN, NULL);

            //Con_Open(false);

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
    mn_object_t *ob = page->focusObject();
    if(ob && ob->cmdResponder)
    {
        if(ob->cmdResponder(ob, cmd))
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

int Hu_MenuPrivilegedResponder(event_t *ev)
{
    if(Hu_MenuIsActive())
    {
        mn_object_t *ob = Hu_MenuActivePage()->focusObject();
        if(ob && !(ob->flags() & MNF_DISABLED))
        {
            if(ob->privilegedResponder)
            {
                return ob->privilegedResponder(ob, ev);
            }
        }
    }
    return false;
}

int Hu_MenuResponder(event_t *ev)
{
    if(Hu_MenuIsActive())
    {
        mn_object_t *ob = Hu_MenuActivePage()->focusObject();
        if(ob && !(ob->flags() & MNF_DISABLED))
        {
            if(ob->responder)
            {
                return ob->responder(ob, ev);
            }
        }
    }
    return false; // Not eaten.
}

int Hu_MenuFallbackResponder(event_t *ev)
{
    mn_page_t *page = Hu_MenuActivePage();

    if(!Hu_MenuIsActive() || !page) return false;

    if(cfg.menuShortcutsEnabled)
    {
        if(ev->type == EV_KEY && (ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        {
            for(int i = 0; i < page->objectsCount(); ++i)
            {
                mn_object_t *ob = &page->objects()[i];
                if(ob->flags() & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN))
                    continue;

                if(ob->shortcut() == ev->data1)
                {
                    page->setFocus(ob);
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
int Hu_MenuSelectLoadSlot(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mndata_edit_t *edit = (mndata_edit_t *)ob->_typedata;

    if(MNA_ACTIVEOUT != action) return 1;

    mn_page_t *saveGamePage = Hu_MenuFindPageByName("SaveGame");
    saveGamePage->setFocus(saveGamePage->findObject(0, ob->data2));

    G_SetGameActionLoadSession((char *)edit->data1);
    Hu_MenuCommand(chooseCloseMethod());
    return 0;
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(mn_page_t * /*page*/, Point2Raw const *origin)
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

    WI_DrawPatch(pMainTitle, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pMainTitle),
                 de::Vector2i(origin->x + TITLEOFFSET_X, origin->y + TITLEOFFSET_Y), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
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

void Hu_MenuDrawGameTypePage(mn_page_t * /*page*/, Point2Raw const *origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PICKGAMETYPE), SCREENWIDTH/2, origin->y - 28);
}

#if __JHERETIC__
static void composeNotDesignedForMessage(char const *str)
{
    char *buf = notDesignedForMessage;
    char tmp[2];

    buf[0] = 0;
    tmp[1] = 0;

    // Get the message template.
    char const *in = GET_TXT(TXT_NOTDESIGNEDFOR);

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
void Hu_MenuPlayerClassBackgroundTicker(mn_object_t *ob)
{
    DENG2_ASSERT(ob != 0);

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    if(mn_object_t *mop = ob->page()->focusObject())
    {
        playerclass_t pClass = (playerclass_t) mop->data2;
        if(pClass == PCLASS_NONE)
        {
            // Random class.
            /// @todo Use this object's timer instead of menuTime.
            pClass = playerclass_t(menuTime / 5);
        }

        /// @todo Only change here if in the "random" state.
        pClass = playerclass_t(int(pClass) % 3); // Number of user-selectable classes.

        MNRect_SetBackgroundPatch(ob, pPlayerClassBG[pClass]);
    }

    // Call MNRect's ticker now we've done our own processing.
    MNRect_Ticker(ob);
}

/**
 * A specialization of MNMobjPreview_Ticker() which implements the animation
 * logic for the player class selection page's player visual.
 */
void Hu_MenuPlayerClassPreviewTicker(mn_object_t *ob)
{
    DENG2_ASSERT(ob != 0);

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    if(mn_object_t *mop = ob->page()->focusObject())
    {
        playerclass_t pClass = (playerclass_t) mop->data2;
        if(pClass == PCLASS_NONE)
        {
            // Random class.
            /// @todo Use this object's timer instead of menuTime.
            pClass = playerclass_t(PCLASS_FIRST + (menuTime / 5));
            pClass = playerclass_t(int(pClass) % 3); // Number of user-selectable classes.

            MNMobjPreview_SetPlayerClass(ob, pClass);
            MNMobjPreview_SetMobjType(ob, PCLASS_INFO(pClass)->mobjType);
        }

        // Fighter is Yellow, others Red by default.
        MNMobjPreview_SetTranslationClass(ob, pClass);
        MNMobjPreview_SetTranslationMap(ob, pClass == PCLASS_FIGHTER? 2 : 0);
    }

    // Call MNMobjPreview's ticker now we've done our own processing.
    MNMobjPreview_Ticker(ob);
}

void Hu_MenuDrawPlayerClassPage(mn_page_t * /*page*/, Point2Raw const *origin)
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
void Hu_MenuDrawEpisodePage(mn_page_t *page, Point2Raw const *origin)
{
#if __JHERETIC__
    DENG2_UNUSED(origin);

    // Inform the user episode 6 is designed for deathmatch only.
    mn_object_t *obj = page->findObject(0, MNF_ID0);
    if(obj && obj == page->focusObject())
    {
        Point2Raw origin;

        composeNotDesignedForMessage(GET_TXT(TXT_SINGLEPLAYER));

        origin.x = SCREENWIDTH/2;
        origin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);

        Hu_MenuDrawPageHelp(notDesignedForMessage, origin.x, origin.y);
    }
#else // __JDOOM__
    DENG2_UNUSED(page);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorv(cfg.menuTextColors[0]);
    FR_SetAlpha(mnRendState->pageAlpha);

    WI_DrawPatch(pEpisode, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pEpisode),
                 de::Vector2i(origin->x + 7, origin->y - 25), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}
#endif

void Hu_MenuDrawSkillPage(mn_page_t * /*page*/, Point2Raw const *origin)
{
#if __JDOOM__ || __JDOOM64__
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pNewGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pNewGame),
                 de::Vector2i(origin->x + 48, origin->y - 49), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
    WI_DrawPatch(pSkill, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pSkill),
                 de::Vector2i(origin->x + 6, origin->y - 25), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#elif __JHEXEN__
    Hu_MenuDrawPageTitle("Choose Skill Level:", origin->x + 36, origin->y - 28);
#else
    DENG2_UNUSED(origin);
#endif
}

/**
 * Called after the save name has been modified and to action the game-save.
 */
int Hu_MenuSelectSaveSlot(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mndata_edit_t *edit = (mndata_edit_t *)ob->_typedata;
    char const *saveSlotId = (char *)edit->data1;

    if(MNA_ACTIVEOUT != action) return 1;

    if(menuNominatingQuickSaveSlot)
    {
        Con_SetInteger("game-save-quick-slot", de::String(saveSlotId).toInt());
        menuNominatingQuickSaveSlot = false;
    }

    de::String userDescription = Str_Text(MNEdit_Text(ob));
    if(!G_SetGameActionSaveSession(saveSlotId, &userDescription))
    {
        return 0;
    }

    mn_page_t *page = Hu_MenuFindPageByName("SaveGame");
    page->setFocus(MN_MustFindObjectOnPage(page, 0, ob->data2));

    page = Hu_MenuFindPageByName("LoadGame");
    page->setFocus(MN_MustFindObjectOnPage(page, 0, ob->data2));

    Hu_MenuCommand(chooseCloseMethod());
    return 0;
}

int Hu_MenuCvarButton(mn_object_t *obj, mn_actionid_t action, void * /*context*/)
{
    mndata_button_t *btn = (mndata_button_t *)obj->_typedata;
    cvarbutton_t const *cb = (cvarbutton_t *)obj->data1;
    cvartype_t varType = Con_GetVariableType(cb->cvarname);
    int value;

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

int Hu_MenuCvarList(mn_object_t *obj, mn_actionid_t action, void *parameters)
{
    mndata_list_t const *list = (mndata_list_t *) obj->_typedata;
    mndata_listitem_t const *item;
    cvartype_t varType;
    int value;

    DENG_UNUSED(parameters);
    if(MNA_MODIFIED != action) return 1;

    if(MNList_Selection(obj) < 0) return 0; // Hmm?

    varType = Con_GetVariableType((char const *)list->data);
    if(CVT_NULL == varType) return 0;

    item = &((mndata_listitem_t *) list->items)[list->selection];
    if(list->mask)
    {
        value = Con_GetInteger((char const *)list->data);
        value = (value & ~list->mask) | (item->data & list->mask);
    }
    else
    {
        value = item->data;
    }

    switch(varType)
    {
    case CVT_INT:
        Con_SetInteger2((char const *)list->data, value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        Con_SetInteger2((char const *)list->data, (byte) value, SVF_WRITE_OVERRIDE);
        break;
    default:
        Con_Error("Hu_MenuCvarList: Unsupported variable type %i", (int)varType);
        break;
    }
    return 0;
}

int Hu_MenuSaveSlotEdit(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    if(MNA_ACTIVE != action) return 1;
    if(cfg.menuGameSaveSuggestDescription)
    {
        de::String const description = G_DefaultSavedSessionUserDescription("" /*don't reuse an existing description*/);
        MNEdit_SetText(ob, MNEDIT_STF_NO_ACTION, description.toLatin1().constData());
    }
    return 0;
}

int Hu_MenuCvarEdit(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mndata_edit_t const *edit = (mndata_edit_t *)ob->_typedata;
    cvartype_t varType = Con_GetVariableType((char const *)edit->data1);

    if(MNA_MODIFIED != action) return 1;

    switch(varType)
    {
    case CVT_CHARPTR:
        Con_SetString2((char const *)edit->data1, Str_Text(MNEdit_Text(ob)), SVF_WRITE_OVERRIDE);
        break;

    case CVT_URIPTR: {
        /// @todo Sanitize and validate against known schemas.
        de::Uri uri(Str_Text(MNEdit_Text(ob)), RC_NULL);
        Con_SetUri2((char const *)edit->data1, reinterpret_cast<uri_s *>(&uri), SVF_WRITE_OVERRIDE);
        break; }

    default: break;
    }
    return 0;
}

int Hu_MenuCvarSlider(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mndata_slider_t const *sldr = (mndata_slider_t *)ob->_typedata;
    cvartype_t varType = Con_GetVariableType((char const *)sldr->data1);
    float value = MNSlider_Value(ob);

    if(MNA_MODIFIED != action) return 1;

    if(CVT_NULL == varType) return 0;

    switch(varType)
    {
    case CVT_FLOAT:
        if(sldr->step >= .01f)
        {
            Con_SetFloat2((char const *)sldr->data1, (int) (100 * value) / 100.0f, SVF_WRITE_OVERRIDE);
        }
        else
        {
            Con_SetFloat2((char const *)sldr->data1, value, SVF_WRITE_OVERRIDE);
        }
        break;
    case CVT_INT:
        Con_SetInteger2((char const *)sldr->data1, (int) value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        Con_SetInteger2((char const *)sldr->data1, (byte) value, SVF_WRITE_OVERRIDE);
        break;
    default:
        break;
    }
    return 0;
}

int Hu_MenuActivateColorWidget(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mn_object_t *cboxMix, *sldrRed, *sldrGreen, *sldrBlue, *textAlpha, *sldrAlpha;
    mn_page_t *colorWidgetPage = Hu_MenuFindPageByName("ColorWidget");

    if(action != MNA_ACTIVE) return 1;

    cboxMix   = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID0);
    sldrRed   = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID1);
    sldrGreen = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID2);
    sldrBlue  = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID3);
    textAlpha = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID4);
    sldrAlpha = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID5);

    colorWidgetActive = true;

    colorWidgetPage->initialize();
    colorWidgetPage->userData = ob;

    MNColorBox_CopyColor(cboxMix, 0, ob);
    MNSlider_SetValue(sldrRed,   MNSLIDER_SVF_NO_ACTION, MNColorBox_Redf(ob));
    MNSlider_SetValue(sldrGreen, MNSLIDER_SVF_NO_ACTION, MNColorBox_Greenf(ob));
    MNSlider_SetValue(sldrBlue,  MNSLIDER_SVF_NO_ACTION, MNColorBox_Bluef(ob));
    MNSlider_SetValue(sldrAlpha, MNSLIDER_SVF_NO_ACTION, MNColorBox_Alphaf(ob));

    textAlpha->setFlags((MNColorBox_RGBAMode(ob)? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);
    sldrAlpha->setFlags((MNColorBox_RGBAMode(ob)? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);

    return 0;
}

int Hu_MenuCvarColorBox(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mndata_colorbox_t *cbox = (mndata_colorbox_t *)ob->_typedata;

    if(action != MNA_MODIFIED) return 1;

    // MNColorBox's current color has already been updated and we know
    // that at least one of the color components have changed.
    // So our job is to simply update the associated cvars.
    Con_SetFloat2((char const *)cbox->data1, MNColorBox_Redf(ob),   SVF_WRITE_OVERRIDE);
    Con_SetFloat2((char const *)cbox->data2, MNColorBox_Greenf(ob), SVF_WRITE_OVERRIDE);
    Con_SetFloat2((char const *)cbox->data3, MNColorBox_Bluef(ob),  SVF_WRITE_OVERRIDE);
    if(MNColorBox_RGBAMode(ob))
    {
        Con_SetFloat2((char const *)cbox->data4, MNColorBox_Alphaf(ob), SVF_WRITE_OVERRIDE);
    }

    return 0;
}

void Hu_MenuDrawLoadGamePage(mn_page_t * /*page*/, Point2Raw const *origin)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JHERETIC__ || __JHEXEN__
    FR_DrawTextXY3("Load Game", SCREENWIDTH/2, origin->y-20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#else
    WI_DrawPatch(pLoadGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pLoadGame),
                 de::Vector2i(origin->x - 8, origin->y - 26), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif
    DGL_Disable(DGL_TEXTURE_2D);

    Point2Raw helpOrigin;
    helpOrigin.x = SCREENWIDTH/2;
    helpOrigin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);
    Hu_MenuDrawPageHelp("Select to load, [Del] to clear", helpOrigin.x, helpOrigin.y);
}

void Hu_MenuDrawSaveGamePage(mn_page_t * /*page*/, Point2Raw const *origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Save Game", SCREENWIDTH/2, origin->y - 20);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pSaveGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pSaveGame),
                 de::Vector2i(origin->x - 8, origin->y - 26), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif

    Point2Raw helpOrigin;
    helpOrigin.x = SCREENWIDTH/2;
    helpOrigin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);
    Hu_MenuDrawPageHelp("Select to save, [Del] to clear", helpOrigin.x, helpOrigin.y);
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
int Hu_MenuSelectHelp(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    if(MNA_ACTIVEOUT != action) return 1;
    G_StartHelp();
    return 0;
}
#endif

void Hu_MenuDrawOptionsPage(mn_page_t * /*page*/, Point2Raw const *origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Options", origin->x + 42, origin->y - 38);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pOptionsTitle, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pOptionsTitle),
                 de::Vector2i(origin->x + 42, origin->y - 20), ALIGN_TOP, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void Hu_MenuDrawWeaponsPage(mn_page_t *page, Point2Raw const * /*offset*/)
{
    // Inform the user how to change the order.
    if(page->focusObject() == MN_MustFindObjectOnPage(page, 0, MNF_ID0))
    {
        char const *helpText = "Use left/right to move weapon up/down";
        Point2Raw origin;
        origin.x = SCREENWIDTH/2;
        origin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);
        Hu_MenuDrawPageHelp(helpText, origin.x, origin.y);
    }
}

void Hu_MenuDrawMultiplayerPage(mn_page_t * /*page*/, Point2Raw const *origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_MULTIPLAYER), SCREENWIDTH/2, origin->y - 28);
}

void Hu_MenuDrawPlayerSetupPage(mn_page_t * /*page*/, Point2Raw const *origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PLAYERSETUP), SCREENWIDTH/2, origin->y - 28);
}

int Hu_MenuActionSetActivePage(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    DENG2_ASSERT(ob != 0);
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MenuSetActivePage(Hu_MenuFindPageByName((char *)ob->data1));
    return 0;
}

int Hu_MenuUpdateColorWidgetColor(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    float value = MNSlider_Value(ob);
    mn_object_t *cboxMix = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("ColorWidget"), 0, MNF_ID0);

    if(MNA_MODIFIED != action) return 1;

    switch(ob->data2)
    {
    case CR: MNColorBox_SetRedf(  cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CG: MNColorBox_SetGreenf(cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CB: MNColorBox_SetBluef( cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CA: MNColorBox_SetAlphaf(cboxMix, MNCOLORBOX_SCF_NO_ACTION, value); break;
    default:
        Con_Error("Hu_MenuUpdateColorWidgetColor: Invalid value (%i) for data2.", ob->data2);
    }

    return 0;
}

int Hu_MenuChangeWeaponPriority(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    if(MNA_MODIFIED != action) return 1;

    /*int choice = option >> NUM_WEAPON_TYPES;

    if(option & RIGHT_DIR)
    {
        if(choice < NUM_WEAPON_TYPES-1)
        {
            int temp = cfg.weaponOrder[choice+1];
            cfg.weaponOrder[choice+1] = cfg.weaponOrder[choice];
            cfg.weaponOrder[choice] = temp;
        }
    }
    else
    {
        if(choice > 0)
        {
            int temp = cfg.weaponOrder[choice];
            cfg.weaponOrder[choice] = cfg.weaponOrder[choice-1];
            cfg.weaponOrder[choice-1] = temp;
        }
    }*/
    return 0;
}

int Hu_MenuSelectSingleplayer(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
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

int Hu_MenuSelectMultiplayer(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    mn_page_t *multiplayerPage = Hu_MenuFindPageByName("Multiplayer");
    mn_object_t *labelObj = MN_MustFindObjectOnPage(multiplayerPage, 0, MNF_ID0);
    mndata_button_t *btn  = (mndata_button_t *)labelObj->_typedata;

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

int Hu_MenuSelectJoinGame(mn_object_t * /*ob*/, mn_actionid_t action, void * /*parameters*/)
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

int Hu_MenuSelectPlayerSetup(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    mn_page_t *playerSetupPage = Hu_MenuFindPageByName("PlayerSetup");
    mn_object_t *mop    = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID0);
    mn_object_t *name   = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID1);
    mn_object_t *color  = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID3);
#if __JHEXEN__
    mn_object_t *class_;
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
    class_ = MN_MustFindObjectOnPage(playerSetupPage, 0, MNF_ID2);
    MNList_SelectItemByValue(class_, MNLIST_SIF_NO_ACTION, cfg.netClass);
#endif

    MNEdit_SetText(name, MNEDIT_STF_NO_ACTION|MNEDIT_STF_REPLACEOLD, Con_GetString("net-name"));

    Hu_MenuSetActivePage(playerSetupPage);
    return 0;
}

#if __JHEXEN__
int Hu_MenuSelectPlayerSetupPlayerClass(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    if(MNA_MODIFIED != action) return 1;

    int selection = MNList_Selection(ob);
    if(selection >= 0)
    {
        mn_object_t *mop = MN_MustFindObjectOnPage(ob->page(), 0, MNF_ID0);
        MNMobjPreview_SetPlayerClass(mop, selection);
        MNMobjPreview_SetMobjType(mop, PCLASS_INFO(selection)->mobjType);
    }
    return 0;
}
#endif

int Hu_MenuSelectPlayerColor(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    if(MNA_MODIFIED != action) return 1;

    // The color translation map is stored in the list item data member.
    int selection = MNList_ItemData(ob, MNList_Selection(ob));
    if(selection >= 0)
    {
        mn_object_t *mop = MN_MustFindObjectOnPage(ob->page(), 0, MNF_ID0);
        MNMobjPreview_SetTranslationMap(mop, selection);
    }
    return 0;
}

int Hu_MenuSelectAcceptPlayerSetup(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mn_object_t *plrNameEdit = MN_MustFindObjectOnPage(ob->page(), 0, MNF_ID1);
#if __JHEXEN__
    mn_object_t *plrClassList = MN_MustFindObjectOnPage(ob->page(), 0, MNF_ID2);
#endif
    mn_object_t *plrColorList = MN_MustFindObjectOnPage(ob->page(), 0, MNF_ID3);

#if __JHEXEN__
    cfg.netClass = MNList_Selection(plrClassList);
#endif
    // The color translation map is stored in the list item data member.
    cfg.netColor = MNList_ItemData(plrColorList, MNList_Selection(plrColorList));

    if(MNA_ACTIVEOUT != action) return 1;

    char buf[300];
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

int Hu_MenuSelectQuitGame(mn_object_t * /*ob*/, mn_actionid_t action, void * /*parameters*/)
{
    if(MNA_ACTIVEOUT != action) return 1;
    G_QuitGame();
    return 0;
}

int Hu_MenuSelectEndGame(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    if(MNA_ACTIVEOUT != action) return 1;
    DD_Executef(true, "endgame");
    return 0;
}

int Hu_MenuSelectLoadGame(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    if(MNA_ACTIVEOUT != action) return 1;

    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT && !Get(DD_PLAYBACK))
        {
            Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, 0, NULL);
            return 0;
        }
    }

    Hu_MenuSetActivePage(Hu_MenuFindPageByName("LoadGame"));
    return 0;
}

int Hu_MenuSelectSaveGame(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    player_t *player = &players[CONSOLEPLAYER];

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
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("SaveGame"));
    return 0;
}

#if __JHEXEN__
int Hu_MenuSelectPlayerClass(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
    mn_page_t *skillPage = Hu_MenuFindPageByName("Skill");
    int option = ob->data2;
    mn_object_t *skillObj;
    char const *text;

    if(MNA_ACTIVEOUT != action) return 1;

    if(IS_NETGAME)
    {
        P_SetMessage(&players[CONSOLEPLAYER], LMF_NO_HIDE, "You can't start a new game from within a netgame!");
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
    text     = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_BABY]);
    ((mndata_button_t *)skillObj->_typedata)->text = text;
    skillObj->setShortcut(text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID1);
    text     = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_EASY]);
    ((mndata_button_t *)skillObj->_typedata)->text = text;
    skillObj->setShortcut(text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID2);
    text     = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_MEDIUM]);
    ((mndata_button_t *)skillObj->_typedata)->text = text;
    skillObj->setShortcut(text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID3);
    text     = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_HARD]);
    ((mndata_button_t *)skillObj->_typedata)->text = text;
    skillObj->setShortcut(text[0]);

    skillObj = MN_MustFindObjectOnPage(skillPage, 0, MNF_ID4);
    text     = GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_NIGHTMARE]);
    ((mndata_button_t *)skillObj->_typedata)->text = text;
    skillObj->setShortcut(text[0]);

    switch(mnPlrClass)
    {
    case PCLASS_FIGHTER:    skillPage->setX(120); break;
    case PCLASS_CLERIC:     skillPage->setX(116); break;
    case PCLASS_MAGE:       skillPage->setX(112); break;
    }
    Hu_MenuSetActivePage(skillPage);
    return 0;
}

int Hu_MenuFocusOnPlayerClass(mn_object_t *ob, mn_actionid_t action, void *context)
{
    playerclass_t plrClass = (playerclass_t)ob->data2;

    if(MNA_FOCUS != action) return 1;

    mn_object_t *mop = MN_MustFindObjectOnPage(ob->page(), 0, MNF_ID0);
    MNMobjPreview_SetPlayerClass(mop, plrClass);
    MNMobjPreview_SetMobjType(mop, (PCLASS_NONE == plrClass? MT_NONE : PCLASS_INFO(plrClass)->mobjType));

    Hu_MenuDefaultFocusAction(ob, action, context);
    return 0;
}
#endif

#if __JDOOM__ || __JHERETIC__
int Hu_MenuFocusEpisode(mn_object_t *ob, mn_actionid_t action, void *context)
{
    if(MNA_FOCUS != action) return 1;
    mnEpisode = ob->data2;
    Hu_MenuDefaultFocusAction(ob, action, context);
    return 0;
}

int Hu_MenuConfirmOrderCommericalVersion(msgresponse_t /*response*/, int /*userValue*/, void * /*context*/)
{
    G_StartHelp();
    return true;
}

int Hu_MenuActivateNotSharewareEpisode(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MsgStart(MSG_ANYKEY, SWSTRING, Hu_MenuConfirmOrderCommericalVersion, 0, NULL);
    return 0;
}
#endif

int Hu_MenuFocusSkillMode(mn_object_t *ob, mn_actionid_t action, void *context)
{
    DENG2_ASSERT(ob != 0);

    if(MNA_FOCUS != action) return 1;
    mnSkillmode = (skillmode_t)ob->data2;
    Hu_MenuDefaultFocusAction(ob, action, context);
    return 0;
}

#if __JDOOM__
int Hu_MenuConfirmInitNewGame(msgresponse_t response, int /*userValue*/, void * /*context*/)
{
    if(response == MSG_YES)
    {
        Hu_MenuInitNewGame(true);
    }
    return true;
}
#endif

void Hu_MenuInitNewGame(dd_bool confirmed)
{
#if __JDOOM__
    if(!confirmed && SM_NIGHTMARE == mnSkillmode)
    {
        Hu_MsgStart(MSG_YESNO, NIGHTMARE, Hu_MenuConfirmInitNewGame, 0, NULL);
        return;
    }
#else
    DENG2_UNUSED(confirmed);
#endif

    Hu_MenuCommand(chooseCloseMethod());

#if __JHEXEN__
    cfg.playerClass[CONSOLEPLAYER] = playerclass_t(mnPlrClass);
#endif

    GameRuleset newRules(defaultGameRules);
    newRules.skill = mnSkillmode;

#if __JHEXEN__
    de::Uri newMapUri = P_TranslateMap(0);
#else
    de::Uri newMapUri = G_ComposeMapUri(mnEpisode, 0);
#endif
    G_SetGameActionNewSession(newMapUri, 0/*default*/, newRules);
}

int Hu_MenuActionInitNewGame(mn_object_t * /*ob*/, mn_actionid_t action, void * /*context*/)
{
    if(MNA_ACTIVEOUT != action) return 1;
    Hu_MenuInitNewGame(false);
    return 0;
}

int Hu_MenuSelectControlPanelLink(mn_object_t *ob, mn_actionid_t action, void * /*context*/)
{
#define NUM_PANEL_NAMES         1

    static char const *panelNames[NUM_PANEL_NAMES] = {
        "taskbar" //,
        //"panel audio",
        //"panel input"
    };

    if(MNA_ACTIVEOUT != action) return 1;

    int idx = ob->data2;
    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
    {
        idx = 0;
    }

    DD_Execute(true, panelNames[idx]);
    return 0;

#undef NUM_PANEL_NAMES
}

D_CMD(MenuOpen)
{
    DENG2_UNUSED(src);

    if(argc > 1)
    {
        if(!stricmp(argv[1], "open"))
        {
            Hu_MenuCommand(MCMD_OPEN);
            return true;
        }
        if(!stricmp(argv[1], "close"))
        {
            Hu_MenuCommand(MCMD_CLOSE);
            return true;
        }

        if(mn_page_t *page = Hu_MenuFindPageByName(argv[1]))
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
    DENG2_UNUSED2(src, argc);

    if(menuActive)
    {
        char const *cmd = argv[0] + 4;
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

void Hu_MenuRegister()
{
    C_VAR_FLOAT("menu-scale",               &cfg.menuScale,              0, .1f, 1);
    C_VAR_BYTE ("menu-stretch",             &cfg.menuScaleMode,          0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_FLOAT("menu-flash-r",             &cfg.menuTextFlashColor[CR], 0, 0, 1);
    C_VAR_FLOAT("menu-flash-g",             &cfg.menuTextFlashColor[CG], 0, 0, 1);
    C_VAR_FLOAT("menu-flash-b",             &cfg.menuTextFlashColor[CB], 0, 0, 1);
    C_VAR_INT  ("menu-flash-speed",         &cfg.menuTextFlashSpeed,     0, 0, 50);
    C_VAR_BYTE ("menu-cursor-rotate",       &cfg.menuCursorRotate,       0, 0, 1);
    C_VAR_INT  ("menu-effect",              &cfg.menuEffectFlags,        0, 0, MEF_EVERYTHING);
    C_VAR_FLOAT("menu-color-r",             &cfg.menuTextColors[0][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-color-g",             &cfg.menuTextColors[0][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-color-b",             &cfg.menuTextColors[0][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-colorb-r",            &cfg.menuTextColors[1][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-colorb-g",            &cfg.menuTextColors[1][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-colorb-b",            &cfg.menuTextColors[1][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-colorc-r",            &cfg.menuTextColors[2][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-colorc-g",            &cfg.menuTextColors[2][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-colorc-b",            &cfg.menuTextColors[2][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-colord-r",            &cfg.menuTextColors[3][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-colord-g",            &cfg.menuTextColors[3][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-colord-b",            &cfg.menuTextColors[3][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-glitter",             &cfg.menuTextGlitter,        0, 0, 1);
    C_VAR_INT  ("menu-fog",                 &cfg.hudFog,                 0, 0, 5);
    C_VAR_FLOAT("menu-shadow",              &cfg.menuShadow,             0, 0, 1);
    C_VAR_INT  ("menu-patch-replacement",   &cfg.menuPatchReplaceMode,   0, PRM_FIRST, PRM_LAST);
    C_VAR_BYTE ("menu-slam",                &cfg.menuSlam,               0, 0, 1);
    C_VAR_BYTE ("menu-hotkeys",             &cfg.menuShortcutsEnabled,   0, 0, 1);
#if __JDOOM__ || __JDOOM64__
    C_VAR_INT  ("menu-quitsound",           &cfg.menuQuitSound,          0, 0, 1);
#endif
    C_VAR_BYTE ("menu-save-suggestname",    &cfg.menuGameSaveSuggestDescription, 0, 0, 1);

    // Aliases for obsolete cvars:
    C_VAR_BYTE ("menu-turningskull",        &cfg.menuCursorRotate,       0, 0, 1);


    C_CMD("menu",           "s",    MenuOpen);
    C_CMD("menu",           "",     MenuOpen);
    C_CMD("menuup",         "",     MenuCommand);
    C_CMD("menudown",       "",     MenuCommand);
    C_CMD("menupageup",     "",     MenuCommand);
    C_CMD("menupagedown",   "",     MenuCommand);
    C_CMD("menuleft",       "",     MenuCommand);
    C_CMD("menuright",      "",     MenuCommand);
    C_CMD("menuselect",     "",     MenuCommand);
    C_CMD("menudelete",     "",     MenuCommand);
    C_CMD("menuback",       "",     MenuCommand);
}
