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

namespace common {

using namespace common::menu;

/// Original game line height for pages that employ the fixed layout (in 320x200 pixels).
#if __JDOOM__
#  define FIXED_LINE_HEIGHT (15+1)
#else
#  define FIXED_LINE_HEIGHT (19+1)
#endif

void Hu_MenuActivatePlayerSetup(Page *page);

void Hu_MenuActionSetActivePage(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuActionInitNewGame(Widget *wi, Widget::mn_actionid_t action);

void Hu_MenuSelectLoadGame(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuSelectSaveGame(Widget *wi, Widget::mn_actionid_t action);
#if __JHEXEN__
void Hu_MenuSelectFiles(Widget *wi, Widget::mn_actionid_t action);
#endif
void Hu_MenuSelectJoinGame(Widget *wi, Widget::mn_actionid_t action);

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void Hu_MenuSelectHelp(Widget *wi, Widget::mn_actionid_t action);
#endif
void Hu_MenuSelectControlPanelLink(Widget *wi, Widget::mn_actionid_t action);

void Hu_MenuSelectSingleplayer(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuSelectMultiplayer(Widget *wi, Widget::mn_actionid_t action);
#if __JDOOM__ || __JHERETIC__
void Hu_MenuFocusEpisode(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuActivateNotSharewareEpisode(Widget *wi, Widget::mn_actionid_t action);
#endif
#if __JHEXEN__
void Hu_MenuFocusOnPlayerClass(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuSelectPlayerClass(Widget *wi, Widget::mn_actionid_t action);
#endif
void Hu_MenuFocusSkillMode(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuSelectLoadSlot(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuSelectQuitGame(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuSelectEndGame(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuSelectAcceptPlayerSetup(Widget *wi, Widget::mn_actionid_t action);

void Hu_MenuSelectSaveSlot(Widget *wi, Widget::mn_actionid_t action);

void Hu_MenuChangeWeaponPriority(Widget *wi, Widget::mn_actionid_t action);
#if __JHEXEN__
void Hu_MenuSelectPlayerSetupPlayerClass(Widget *wi, Widget::mn_actionid_t action);
#endif
void Hu_MenuSelectPlayerColor(Widget *wi, Widget::mn_actionid_t action);

#if __JHEXEN__
void Hu_MenuPlayerClassBackgroundTicker(Widget *wi);
void Hu_MenuPlayerClassPreviewTicker(Widget *wi);
#endif

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(Page *page, Point2Raw const *origin);
#endif

void Hu_MenuDrawGameTypePage(Page *page, Point2Raw const *origin);
void Hu_MenuDrawSkillPage(Page *page, Point2Raw const *origin);
#if __JHEXEN__
void Hu_MenuDrawPlayerClassPage(Page *page, Point2Raw const *origin);
#endif
#if __JDOOM__ || __JHERETIC__
void Hu_MenuDrawEpisodePage(Page *page, Point2Raw const *origin);
#endif
void Hu_MenuDrawOptionsPage(Page *page, Point2Raw const *origin);
void Hu_MenuDrawLoadGamePage(Page *page, Point2Raw const *origin);
void Hu_MenuDrawSaveGamePage(Page *page, Point2Raw const *origin);
void Hu_MenuDrawMultiplayerPage(Page *page, Point2Raw const *origin);
void Hu_MenuDrawPlayerSetupPage(Page *page, Point2Raw const *origin);

int Hu_MenuColorWidgetCmdResponder(Page *page, menucommand_e cmd);

void Hu_MenuSaveSlotEdit(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuBindings(Widget *wi, Widget::mn_actionid_t action);

void Hu_MenuActivateColorWidget(Widget *wi, Widget::mn_actionid_t action);
void Hu_MenuUpdateColorWidgetColor(Widget *wi, Widget::mn_actionid_t action);

static void initAllPages();
static void destroyAllPages();

static void initAllObjectsOnAllPages();

static void Hu_MenuUpdateCursorState();

static dd_bool Hu_MenuHasCursorRotation(Widget *wi);

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

static Page *menuActivePage;
static dd_bool menuActive;

static float mnAlpha; // Alpha level for the entire menu.
static float mnTargetAlpha; // Target alpha for the entire UI.

static skillmode_t mnSkillmode = SM_MEDIUM;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
static int mnEpisode;
#endif
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

#if __JHEXEN__
static patchid_t pPlayerClassBG[3];
static patchid_t pBullWithFire[8];
#endif

#if __JHERETIC__
static patchid_t pRotatingSkull[18];
#endif

static patchid_t pCursors[MENU_CURSOR_FRAMECOUNT];

static dd_bool inited;

typedef QMap<String, Page *> Pages;
static Pages pages;

static menucommand_e chooseCloseMethod()
{
    // If we aren't using a transition then we can close normally and allow our
    // own menu fade-out animation to be used instead.
    return Con_GetInteger("con-transition-tics") == 0? MCMD_CLOSE : MCMD_CLOSEFAST;
}

Page *Hu_MenuFindPageByName(String name)
{
    if(!name.isEmpty())
    {
        Pages::iterator found = pages.find(name.toLower());
        if(found != pages.end())
        {
            return *found;
        }
    }
    return 0; // Not found.
}

String Hu_MenuFindPageName(Page const *page)
{
    if(page)
    {
        Pages::const_iterator i = pages.constBegin();
        while(i != pages.constEnd())
        {
            if(i.value() == page)
            {
                return i.key();
            }
            i++;
        }
    }
    return "";
}

/// @todo Make this state an object property flag.
/// @return  @c true if the rotation of a cursor on this object should be animated.
static dd_bool Hu_MenuHasCursorRotation(Widget *wi)
{
    DENG2_ASSERT(wi != 0);
    return (!(wi->flags() & MNF_DISABLED) &&
              (wi->is<InlineListWidget>() || wi->is<SliderWidget>()));
}

/// To be called to re-evaluate the state of the cursor (e.g., when focus changes).
static void Hu_MenuUpdateCursorState()
{
    if(menuActive)
    {
        Page *page = colorWidgetActive? Hu_MenuFindPageByName("ColorWidget") : Hu_MenuActivePage();
        if(Widget *wi = page->focusObject())
        {
            cursorHasRotation = Hu_MenuHasCursorRotation(wi);
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

    Page *page = Hu_MenuNewPage("ColorWidget", &origin, MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, Hu_MenuColorWidgetCmdResponder, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->_flags   = MNF_ID0 | MNF_NO_FOCUS;
        cbox->width    = SCREENHEIGHT / 7;
        cbox->height   = SCREENHEIGHT / 7;
        cbox->_rgbaMode = true;
        page->_widgets << cbox;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Red";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->_flags    = MNF_ID1;
        sld->setShortcut('r');
        sld->data2     = (void*)CR;
        sld->min       = 0;
        sld->max       = 1;
        sld->_value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
        sld->actions[Widget::MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Green";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->_flags    = MNF_ID2;
        sld->setShortcut('g');
        sld->data2     = (void*)CG;
        sld->min       = 0;
        sld->max       = 1;
        sld->_value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        sld->actions[Widget::MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Blue";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->_flags    = MNF_ID3;
        sld->setShortcut('b');
        sld->data2     = (void*)CB;
        sld->min       = 0;
        sld->max       = 1;
        sld->_value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
        sld->actions[Widget::MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->_flags = MNF_ID4;
        text->text   = "Opacity";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->_flags    = MNF_ID5;
        sld->setShortcut('o');
        sld->data2     = (void*)CA;
        sld->min       = 0;
        sld->max       = 1;
        sld->_value     = 0;
        sld->step      = .05f;
        sld->floatMode = true;
        sld->actions[Widget::MNA_MODIFIED].callback = Hu_MenuUpdateColorWidgetColor;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }
}

void Hu_MenuInitMainPage()
{
#if __JHEXEN__ || __JHERETIC__
    Point2Raw origin(110, 56);
#else
    Point2Raw origin(97, 64);
#endif

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        origin.y += 8;
    }
#endif

#if __JDOOM__ || __JDOOM64__
    Page *page = Hu_MenuNewPage("Main", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, NULL, NULL);
#else
    Page *page = Hu_MenuNewPage("Main", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawMainPage, NULL, NULL);
#endif
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));

    int y = 0;

#if __JDOOM__ || __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->_origin.x = -3;
        text->_origin.y = -70;
        text->patch     = &pMainTitle;
        page->_widgets << text;
    }
#endif

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->setShortcut('n');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("GameType"));
#if defined(__JDOOM__) && !defined(__JDOOM64__)
        btn->setPatch(pNGame);
#else
        btn->setText("New Game");
#endif
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    y += FIXED_LINE_HEIGHT;

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->setShortcut('o');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("Options"));
#if defined(__JDOOM__) && !defined(__JDOOM64__)
        btn->setPatch(pOptions);
#else
        btn->setText("Options");
#endif
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    y += FIXED_LINE_HEIGHT;

#if __JDOOM__ || __JDOOM64__
    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->setShortcut('l');
        btn->_pageFontIdx = MENU_FONT1;
# if __JDOOM64__
        btn->setText("Load Game");
# else
        btn->setPatch(pLoadGame);
# endif
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    y += FIXED_LINE_HEIGHT;

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->setShortcut('s');
        btn->_pageFontIdx = MENU_FONT1;
# if __JDOOM64__
        btn->setText("Save Game");
# else
        btn->setPatch(pSaveGame);
# endif
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    y += FIXED_LINE_HEIGHT;

#else
    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->setShortcut('f');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("Files"));
        btn->setText("Game Files");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    y += FIXED_LINE_HEIGHT;
#endif

#if !__JDOOM64__
    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
# if __JDOOM__
        btn->_flags       = MNF_ID0;
        btn->setShortcut('r');
# else
        btn->setShortcut('i');
# endif
        btn->_pageFontIdx = MENU_FONT1;
# if defined(__JDOOM__)
        btn->setPatch(pReadThis);
# else
        btn->setText("Info");
# endif
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectHelp;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    y += FIXED_LINE_HEIGHT;
#endif

    {
        ButtonWidget *btn = new ButtonWidget;
#if __JDOOM__
        btn->_flags       = MNF_ID1;
#endif
        btn->_origin.y    = y;
        btn->setShortcut('q');
        btn->_pageFontIdx = MENU_FONT1;
#if defined(__JDOOM__) && !defined(__JDOOM64__)
        btn->setPatch(pQuitGame);
#else
        btn->setText("Quit Game");
#endif
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectQuitGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
}

void Hu_MenuInitGameTypePage()
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw origin(97, 65);
#else
    Point2Raw origin(104, 65);
#endif

    Page *page = Hu_MenuNewPage("GameType", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawGameTypePage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));

    int y = 0;

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText(GET_TXT(TXT_SINGLEPLAYER));
        if(!btn->text().isEmpty() && btn->text().first().isLetterOrNumber()) btn->setShortcut(btn->text().first().toLatin1());
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectSingleplayer;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    y += FIXED_LINE_HEIGHT;

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText(GET_TXT(TXT_MULTIPLAYER));
        if(!btn->text().isEmpty() && btn->text().first().isLetterOrNumber()) btn->setShortcut(btn->text().first().toLatin1());
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectMultiplayer;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
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

    Page *page = Hu_MenuNewPage("Skill", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawSkillPage, NULL, NULL);
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

    int y = 0;

    for(uint i = 0; i < NUM_SKILL_MODES; ++i, y += FIXED_LINE_HEIGHT)
    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_flags         = skillButtonFlags[i];
        btn->_origin.y      = y;
        btn->_pageFontIdx   = MENU_FONT1;
        btn->data2          = (int)(SM_BABY + i);
#if !__JHEXEN__
        btn->setText(GET_TXT(skillButtonTexts[i]));
        if(!btn->text().isEmpty() && btn->text().first().isLetterOrNumber()) btn->setShortcut(btn->text().first().toLatin1());
# if __JDOOM__ || __JDOOM64__
        btn->setPatch(pSkillModeNames[i]);
# endif
#endif
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionInitNewGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuFocusSkillMode;
        page->_widgets << btn;
    }

#if __JDOOM__
    if(gameMode != doom2_hacx && gameMode != doom_chex)
    {
        MN_MustFindObjectOnPage(page, 0, MNF_ID4)->as<ButtonWidget>().setNoAltText();
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

    Page *page = Hu_MenuNewPage("Multiplayer", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawMultiplayerPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("GameType"));

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_flags       = MNF_ID0;
        btn->setShortcut('j');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText("Join Game");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectJoinGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('p');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText("Player Setup");
        btn->setData(String("PlayerSetup"));
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
}

void Hu_MenuInitPlayerSetupPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(70, 44);
#else
    Point2Raw const origin(70, 54);
#endif

    Page *page = Hu_MenuNewPage("PlayerSetup", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawPlayerSetupPage, NULL, NULL);
    page->setOnActiveCallback(Hu_MenuActivatePlayerSetup);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPredefinedFont(MENU_FONT2, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("Multiplayer"));

    {
        MobjPreviewWidget *mprev = new MobjPreviewWidget;
        mprev->_origin.x = SCREENWIDTH/2 - origin.x;
        mprev->_origin.y = 60;
        mprev->_flags    = MNF_ID0 | MNF_POSITION_FIXED;
        page->_widgets << mprev;
    }

    {
        LineEditWidget *edit = new LineEditWidget;
        edit->_flags    = MNF_ID1 | MNF_LAYOUT_OFFSET;
        edit->_origin.y = 75;
        edit->data1     = (void *)"net-name";
        edit->_maxLength = 24;
        edit->actions[Widget::MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << edit;
    }

#if __JHEXEN__
    {
        LabelWidget *text = new LabelWidget;
        text->_flags    = MNF_LAYOUT_OFFSET;
        text->_origin.y = 5;
        text->text      = "Class";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->_flags         = MNF_ID2;
        list->setShortcut('c');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = Hu_MenuSelectPlayerSetupPlayerClass;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;

        list->_items << new mndata_listitem_t((char const *)TXT_PLAYERCLASS1, PCLASS_FIGHTER);
        list->_items << new mndata_listitem_t((char const *)TXT_PLAYERCLASS2, PCLASS_CLERIC);
        list->_items << new mndata_listitem_t((char const *)TXT_PLAYERCLASS3, PCLASS_MAGE);

        page->_widgets << list;
    }
#endif

    {
        LabelWidget *text = new LabelWidget;
#ifdef __JHERETIC__
        text->_flags    = MNF_LAYOUT_OFFSET;
        text->_origin.y = 5;
#endif
        text->text      = "Color";
        page->_widgets << text;
    }

    // Setup the player color selection list.
    {
        InlineListWidget *list = new InlineListWidget;
        list->_flags         = MNF_ID3;
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = Hu_MenuSelectPlayerColor;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;

        /// @todo Read these names from Text definitions.
        int colorIdx = 0;
#if __JHEXEN__
        list->_items << new mndata_listitem_t("Red",    colorIdx++);
        list->_items << new mndata_listitem_t("Blue",   colorIdx++);
        list->_items << new mndata_listitem_t("Yellow", colorIdx++);
        list->_items << new mndata_listitem_t("Green",  colorIdx++);
        if(gameMode != hexen_v10) // Hexen v1.0 has only four player colors.
        {
            list->_items << new mndata_listitem_t("Jade",   colorIdx++);
            list->_items << new mndata_listitem_t("White",  colorIdx++);
            list->_items << new mndata_listitem_t("Hazel",  colorIdx++);
            list->_items << new mndata_listitem_t("Purple", colorIdx++);
        }
#elif __JHERETIC__
        list->_items << new mndata_listitem_t("Green",  colorIdx++);
        list->_items << new mndata_listitem_t("Orange", colorIdx++);
        list->_items << new mndata_listitem_t("Red",    colorIdx++);
        list->_items << new mndata_listitem_t("Blue",   colorIdx++);
#else
        list->_items << new mndata_listitem_t("Green",  colorIdx++);
        list->_items << new mndata_listitem_t("Indigo", colorIdx++);
        list->_items << new mndata_listitem_t("Brown",  colorIdx++);
        list->_items << new mndata_listitem_t("Red",    colorIdx++);

#endif
        list->_items << new mndata_listitem_t("Automatic", colorIdx++);

        page->_widgets << list;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('s');
        btn->setText("Save Changes");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectAcceptPlayerSetup;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
}

void Hu_MenuInitSaveOptionsPage()
{
    Point2Raw const origin(60, 50);

    Page *page = Hu_MenuNewPage("SaveOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Save Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Confirm quick load/save";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-save-confirm");
        btn->setShortcut('q');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Confirm reborn load";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-save-confirm-loadonreborn");
        btn->setShortcut('r');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Reborn preferences";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Load last save";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-save-last-loadonreborn");
        btn->setGroup(1)
            .setShortcut('a');
        page->_widgets << btn;
    }
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitFilesPage()
{
    Point2Raw origin(110, 60);

    Page *page = Hu_MenuNewPage("Files", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));

    int y = 0;

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->setShortcut('l');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText("Load Game");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
    y += FIXED_LINE_HEIGHT;

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->_origin.y    = y;
        btn->setShortcut('s');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText("Save Game");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
}
#endif

static void deleteGameSave(String slotId)
{
    DD_Executef(true, "deletegamesave %s", slotId.toLatin1().constData());
}

int Hu_MenuLoadSlotCommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    if(MCMD_DELETE == cmd &&
       (wi->_flags & MNF_FOCUS) && !(wi->_flags & MNF_ACTIVE) && !(wi->_flags & MNF_DISABLED))
    {
        LineEditWidget &edit = wi->as<LineEditWidget>();
        deleteGameSave((char *)edit.data1);
        return true;
    }
    return Widget_DefaultCommandResponder(wi, cmd);
}

int Hu_MenuSaveSlotCommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    if(MCMD_DELETE == cmd &&
       (wi->_flags & MNF_FOCUS) && !(wi->_flags & MNF_ACTIVE) && !(wi->_flags & MNF_DISABLED))
    {
        LineEditWidget &edit = wi->as<LineEditWidget>();
        deleteGameSave((char *)edit.data1);
        return true;
    }
    return LineEditWidget_CommandResponder(wi, cmd);
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

    Page *loadPage = Hu_MenuNewPage("LoadGame", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawLoadGamePage, NULL, NULL);
    loadPage->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    loadPage->setPreviousPage(Hu_MenuFindPageByName("Main"));

    int y = 0;
    int i = 0;
    for(; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        LineEditWidget *edit = new LineEditWidget;
        edit->_origin.x      = 0;
        edit->_origin.y      = y;
        edit->_flags         = saveSlotObjectIds[i] | MNF_DISABLED;
        edit->setShortcut('0' + i);
        edit->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectLoadSlot;
        edit->actions[Widget::MNA_FOCUSOUT ].callback = Hu_MenuDefaultFocusAction;
        edit->cmdResponder   = Hu_MenuLoadSlotCommandResponder;
        edit->_maxLength      = 24;
        edit->data1          = Str_Text(Str_Appendf(Str_New(), "%i", i));
        edit->data2          = saveSlotObjectIds[i];
        edit->emptyString    = (char const *) TXT_EMPTYSTRING;
        loadPage->_widgets << edit;
    }

    Page *savePage = Hu_MenuNewPage("SaveGame", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawSaveGamePage, NULL, NULL);
    savePage->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    savePage->setPreviousPage(Hu_MenuFindPageByName("Main"));

    y = 0;
    i = 0;
    for(; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        LineEditWidget *edit = new LineEditWidget;
        edit->_origin.x      = 0;
        edit->_origin.y      = y;
        edit->_flags         = saveSlotObjectIds[i];
        edit->setShortcut('0' + i);
        edit->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectSaveSlot;
        edit->actions[Widget::MNA_ACTIVE   ].callback = Hu_MenuSaveSlotEdit;
        edit->actions[Widget::MNA_FOCUSOUT ].callback = Hu_MenuDefaultFocusAction;
        edit->cmdResponder   = Hu_MenuSaveSlotCommandResponder;
        edit->emptyString    = (char const *) TXT_EMPTYSTRING;
        edit->_maxLength     = 24;
        edit->data1          = Str_Text(Str_Appendf(Str_New(), "%i", i));
        edit->data2          = saveSlotObjectIds[i];
        savePage->_widgets << edit;
    }
}

void Hu_MenuInitOptionsPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(110, 63);
#else
    Point2Raw const origin(110, 63);
#endif

    Page *page = Hu_MenuNewPage("Options", &origin, 0, Hu_MenuPageTicker, Hu_MenuDrawOptionsPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Main"));


    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('e');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText("End Game");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectEndGame;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('t');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setText("Show Taskbar");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectControlPanelLink;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('c');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("ControlOptions"));
        btn->setText("Controls");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('g');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("GameplayOptions"));
        btn->setText("Gameplay");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('s');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("SaveOptions"));
        btn->setText("Game saves");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('h');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("HUDOptions"));
        btn->setText("HUD");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('a');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("AutomapOptions"));
        btn->setText("Automap");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('w');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("WeaponOptions"));
        btn->setText("Weapons");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }

#if __JHERETIC__ || __JHEXEN__
    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('i');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("InventoryOptions"));
        btn->setText("Inventory");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
#endif

    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setShortcut('s');
        btn->_pageFontIdx = MENU_FONT1;
        btn->setData(String("SoundOptions"));
        btn->setText("Sound");
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << btn;
    }
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

    Page *page = Hu_MenuNewPage("GameplayOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Gameplay Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Always Run";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("ctl-run");
        btn->setShortcut('r');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Use LookSpring";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("ctl-look-spring");
        btn->setShortcut('l');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Disable AutoAim";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("ctl-aim-noauto");
        btn->setShortcut('a');
        page->_widgets << btn;
    }

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->text = "Allow Jumping";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("player-jump");
        btn->setShortcut('j');
        page->_widgets << btn;
    }
#endif

#if __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->text = "Weapon Recoil";
        page->_widgets << text;
    }

    page->_widgets << new CVarToggleWidget("player-weapon-recoil");
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Compatibility";
        page->_widgets << text;
    }

# if __JDOOM__ || __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text   = "Any Boss Trigger 666";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-anybossdeath666");
        btn->setGroup(1)
            .setShortcut('b');
        page->_widgets << btn;
    }

#  if !__JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Av Resurrects Ghosts";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-raiseghosts");
        btn->setGroup(1)
            .setShortcut('g');
        page->_widgets << btn;
    }

# if __JDOOM__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "VileChase uses Av radius";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-vilechase-usevileradius");
        btn->setGroup(1)
            .setShortcut('g');
        page->_widgets << btn;
    }
# endif
#  endif // !__JDOOM64__

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "PE Limited To 21 Lost Souls";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-maxskulls");
        btn->setShortcut('p');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "LS Can Get Stuck Inside Walls";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-skullsinwalls");
        btn->setGroup(1);
        page->_widgets << btn;
    }
# endif // __JDOOM__ || __JDOOM64__

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Monsters Fly Over Obstacles";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-monsters-floatoverblocking");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Monsters Can Get Stuck In Doors";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-monsters-stuckindoors");
        btn->setGroup(1);
        btn->setShortcut('d');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Some Objects Never Hang Over Ledges";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-objects-neverhangoverledges");
        btn->setGroup(1);
        btn->setShortcut('h');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Objects Fall Under Own Weight";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-objects-falloff");
        btn->setGroup(1);
        btn->setShortcut('f');
        page->_widgets << btn;
    }

#if __JDOOM__ || __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "All Crushed Objects Become A Pile Of Gibs";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-objects-gibcrushednonbleeders");
        btn->setGroup(1);
        btn->setShortcut('g');
        page->_widgets << btn;
    }
#endif

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Corpses Slide Down Stairs";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-corpse-sliding");
        btn->setGroup(1);
        btn->setShortcut('s');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Use Exactly Doom's Clipping Code";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-objects-clipping");
        btn->setGroup(1)
            .setShortcut('c');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "  ^If Not NorthOnly WallRunning";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-player-wallrun-northonly");
        btn->setGroup(1)
            .setShortcut('w');
        page->_widgets << btn;
    }

# if __JDOOM__ || __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Zombie Players Can Exit Maps";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("game-zombiescanexit");
        btn->setGroup(1);
        btn->setShortcut('e');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Fix Ouch Face";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-face-ouchfix");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Fix Weapon Slot Display";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-status-weaponslots-ownedfix");
        btn->setGroup(1);
        page->_widgets << btn;
    }
# endif // __JDOOM__ || __JDOOM64__
#endif // __JDOOM__ || __JHERETIC__ || __JDOOM64__
}

void Hu_MenuInitHUDOptionsPage()
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw const origin(97, 40);
#else
    Point2Raw const origin(97, 28);
#endif

    Page *page = Hu_MenuNewPage("HudOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("HUD Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    {
        LabelWidget *text = new LabelWidget;
        text->text = "View Size";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->min       = 3;
#if __JDOOM64__
        sld->max       = 11;
#else
        sld->max       = 13;
#endif
        sld->_value     = 0;
        sld->step      = 1;
        sld->floatMode = false;
        sld->data1     = (void *)"view-size";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

#if __JDOOM__
    {
        LabelWidget *text = new LabelWidget;
        text->text = "Single Key Display";
        page->_widgets << text;
    }

    page->_widgets << new CVarToggleWidget("hud-keys-combine");
#endif

    {
        LabelWidget *text = new LabelWidget;
        text->text = "AutoHide";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new TextualSliderWidget;
        sld->_pageColorIdx  = MENU_COLOR3;
        sld->min            = 0;
        sld->max            = 60;
        sld->_value         = 0;
        sld->step           = 1;
        sld->floatMode      = true;
        sld->data1          = (void *)"hud-timer";
        sld->data2          = (void *)"Disabled";
        sld->data4          = (void *)" second";
        sld->data5          = (void *)" seconds";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "UnHide Events";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Receive Damage";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-damage");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Pickup Health";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-pickup-health");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Pickup Armor";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-pickup-armor");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Pickup Powerup";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-pickup-powerup");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Pickup Weapon";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-pickup-weapon");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
#if __JHEXEN__
        text->text = "Pickup Mana";
#else
        text->text = "Pickup Ammo";
#endif
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-pickup-ammo");
        btn->setGroup(1);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Pickup Key";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-pickup-key");
        btn->setGroup(1);
        page->_widgets << btn;
    }

#if __JHERETIC__ || __JHEXEN__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Pickup Item";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-unhide-pickup-invitem");
        btn->setGroup(1);
        page->_widgets << btn;
    }
#endif // __JHERETIC__ || __JHEXEN__

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Messages";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text = "Shown";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("msg-show");
        btn->setGroup(2)
            .setShortcut('m');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text   = "Uptime";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new TextualSliderWidget;
        sld->setGroup(2);
        sld->_pageColorIdx  = MENU_COLOR3;
        sld->min            = 0;
        sld->max            = 60;
        sld->_value         = 0;
        sld->step           = 1;
        sld->floatMode      = true;
        sld->data1          = (void *)"msg-uptime";
        sld->data2          = (void *)"Disabled";
        sld->data4          = (void *)" second";
        sld->data5          = (void *)" seconds";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text = "Size";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(2);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = .1f;
        sld->floatMode = true;
        sld->data1     = (void *)"msg-scale";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text = "Color";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setGroup(2);
        cbox->_rgbaMode = false;
        cbox->data1     = (void *)"msg-color-r";
        cbox->data2     = (void *)"msg-color-g";
        cbox->data3     = (void *)"msg-color-b";
        cbox->actions[Widget::MNA_MODIFIED ].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVEOUT].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE   ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(3);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Crosshair";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(3);
        text->setShortcut('c');
        text->text = "Symbol";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->setGroup(3);
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"view-cross-type";

        int xhairIdx = 0;
        list->_items << new mndata_listitem_t("None",        xhairIdx++);
        list->_items << new mndata_listitem_t("Cross",       xhairIdx++);
        list->_items << new mndata_listitem_t("Twin Angles", xhairIdx++);
        list->_items << new mndata_listitem_t("Square",      xhairIdx++);
        list->_items << new mndata_listitem_t("Open Square", xhairIdx++);
        list->_items << new mndata_listitem_t("Angle",       xhairIdx++);

        page->_widgets << list;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(3);
        text->text   = "Size";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(3);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = .1f;
        sld->floatMode = true;
        sld->data1     = (void *)"view-cross-size";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(3);
        text->text = "Angle";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(3);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.0625f;
        sld->floatMode = true;
        sld->data1     = (void *)"view-cross-angle";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(3);
        text->text = "Opacity";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(3);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"view-cross-a";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(3);
        text->text = "Vitality Color";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("view-cross-vitality");
        btn->setGroup(3);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(3);
        text->text = "Color";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setGroup(3);
        cbox->_rgbaMode = false;
        cbox->data1     = (void *)"view-cross-r";
        cbox->data2     = (void *)"view-cross-g";
        cbox->data3     = (void *)"view-cross-b";
        cbox->actions[Widget::MNA_MODIFIED ].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVEOUT].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE   ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(4);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Statusbar";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(4);
        text->text = "Size";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(4);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-status-size";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(4);
        text->text = "Opacity";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(4);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-status-alpha";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

#endif // __JDOOM__ || __JHERETIC__ || __JHEXEN__

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(5);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Counters";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(5);
        text->text = "Items";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->setGroup(5);
        list->setShortcut('i');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"hud-cheat-counter";
        list->mask  = CCH_ITEMS | CCH_ITEMS_PRCNT;

        list->_items << new mndata_listitem_t("Hidden",        0);
        list->_items << new mndata_listitem_t("Count",         CCH_ITEMS);
        list->_items << new mndata_listitem_t("Percent",       CCH_ITEMS_PRCNT);
        list->_items << new mndata_listitem_t("Count+Percent", CCH_ITEMS | CCH_ITEMS_PRCNT);

        page->_widgets << list;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(5);
        text->text = "Kills";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->setGroup(5);
        list->setShortcut('k');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"hud-cheat-counter";
        list->mask  = CCH_KILLS | CCH_KILLS_PRCNT;

        list->_items << new mndata_listitem_t("Hidden",        0);
        list->_items << new mndata_listitem_t("Count",         CCH_KILLS);
        list->_items << new mndata_listitem_t("Percent",       CCH_KILLS_PRCNT);
        list->_items << new mndata_listitem_t("Count+Percent", CCH_KILLS | CCH_KILLS_PRCNT);

        page->_widgets << list;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(5);
        text->text = "Secrets";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->setGroup(5);
        list->setShortcut('s');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"hud-cheat-counter";
        list->mask  = CCH_SECRETS | CCH_SECRETS_PRCNT;

        list->_items << new mndata_listitem_t("Hidden",        0);
        list->_items << new mndata_listitem_t("Count",         CCH_SECRETS);
        list->_items << new mndata_listitem_t("Percent",       CCH_SECRETS_PRCNT);
        list->_items << new mndata_listitem_t("Count+Percent", CCH_SECRETS | CCH_SECRETS_PRCNT);

        page->_widgets << list;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(5);
        text->text = "Automap Only";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-cheat-counter-show-mapopen");
        btn->setGroup(5);
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(5);
        text->text = "Size";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(5);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-cheat-counter-scale";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Fullscreen";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Size";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setGroup(6);
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"hud-scale";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Text Color";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setGroup(6);
        cbox->_rgbaMode = true;
        cbox->data1     = (void *)"hud-color-r";
        cbox->data2     = (void *)"hud-color-g";
        cbox->data3     = (void *)"hud-color-b";
        cbox->data4     = (void *)"hud-color-a";
        cbox->actions[Widget::MNA_MODIFIED ].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVEOUT].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE   ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

#if __JHEXEN__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show Mana";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-mana");
        btn->setGroup(6);
        page->_widgets << btn;
    }

#endif // __JHEXEN__

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show Ammo";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-ammo");
        btn->setGroup(6)
            .setShortcut('a');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show Armor";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-armor");
        btn->setGroup(6)
            .setShortcut('r');
        page->_widgets << btn;
    }

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

#if __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show PowerKeys";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-power");
        btn->setGroup(6)
            .setShortcut('p');
        page->_widgets << btn;
    }

#endif // __JDOOM64__

#if __JDOOM__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show Status";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-face");
        btn->setGroup(6)
            .setShortcut('f');
        page->_widgets << btn;
    }

#endif // __JDOOM__

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show Health";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-health");
        btn->setGroup(6)
            .setShortcut('h');
        page->_widgets << btn;
    }

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show Keys";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-keys");
        btn->setGroup(6);
        page->_widgets << btn;
    }

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

#if __JHERETIC__ || __JHEXEN__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(6);
        text->text = "Show Ready-Item";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-currentitem");
        btn->setGroup(6);
        page->_widgets << btn;
    }

#endif // __JHERETIC__ || __JHEXEN__
}

void Hu_MenuInitAutomapOptionsPage()
{
#if __JHERETIC__ || __JHEXEN__
    Point2Raw const origin(64, 28);
#else
    Point2Raw const origin(70, 40);
#endif

    Page *page = Hu_MenuNewPage("AutomapOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Automap Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Background Opacity";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setShortcut('o');
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"map-opacity";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Line Opacity";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setShortcut('l');
        sld->min       = 0;
        sld->max       = 1;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"map-line-opacity";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Line Width";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->min       = .1f;
        sld->max       = 2;
        sld->_value    = 0;
        sld->step      = 0.1f;
        sld->floatMode = true;
        sld->data1     = (void *)"map-line-width";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "HUD Display";
        page->_widgets << text;
    }

#if !__JDOOM64__
    {
        InlineListWidget *list = new InlineListWidget;
        list->setShortcut('h');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"map-huddisplay";

        list->_items << new mndata_listitem_t("None",      0);
        list->_items << new mndata_listitem_t("Current",   1);
        list->_items << new mndata_listitem_t("Statusbar", 2);

        page->_widgets << list;
    }
#endif

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Door Colors";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("map-door-colors");
        btn->setShortcut('d');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Door Glow";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setShortcut('g');
        sld->min       = 0;
        sld->max       = 200;
        sld->_value    = 0;
        sld->step      = 5;
        sld->floatMode = true;
        sld->data1     = (void *)"map-door-glow";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Use Custom Colors";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"map-customcolors";

        list->_items << new mndata_listitem_t("Never",  0);
        list->_items << new mndata_listitem_t("Auto",   1);
        list->_items << new mndata_listitem_t("Always", 2);

        page->_widgets << list;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Wall";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setShortcut('w');
        cbox->data1     = (void *)"map-wall-r";
        cbox->data2     = (void *)"map-wall-g";
        cbox->data3     = (void *)"map-wall-b";
        cbox->actions[Widget::MNA_MODIFIED].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Floor Height Change";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setShortcut('f');
        cbox->data1     = (void *)"map-wall-floorchange-r";
        cbox->data2     = (void *)"map-wall-floorchange-g";
        cbox->data3     = (void *)"map-wall-floorchange-b";
        cbox->actions[Widget::MNA_MODIFIED].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Ceiling Height Change";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->data1 = (void *)"map-wall-ceilingchange-r";
        cbox->data2 = (void *)"map-wall-ceilingchange-g";
        cbox->data3 = (void *)"map-wall-ceilingchange-b";
        cbox->actions[Widget::MNA_MODIFIED].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Unseen";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setShortcut('u');
        cbox->data1     = (void *)"map-wall-unseen-r";
        cbox->data2     = (void *)"map-wall-unseen-g";
        cbox->data3     = (void *)"map-wall-unseen-b";
        cbox->actions[Widget::MNA_MODIFIED].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Thing";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setShortcut('t');
        cbox->data1     = (void *)"map-mobj-r";
        cbox->data2     = (void *)"map-mobj-g";
        cbox->data3     = (void *)"map-mobj-b";
        cbox->actions[Widget::MNA_MODIFIED].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Background";
        page->_widgets << text;
    }

    {
        ColorPreviewWidget *cbox = new ColorPreviewWidget;
        cbox->setShortcut('b');
        cbox->data1     = (void *)"map-background-r";
        cbox->data2     = (void *)"map-background-g";
        cbox->data3     = (void *)"map-background-b";
        cbox->actions[Widget::MNA_MODIFIED].callback = CvarColorPreviewWidget_UpdateCvar;
        cbox->actions[Widget::MNA_ACTIVE  ].callback = Hu_MenuActivateColorWidget;
        cbox->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << cbox;
    }
}

static bool compareWeaponPriority(mndata_listitem_t const *a, mndata_listitem_t const *b)
{
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

    return aIndex < bIndex;
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

    Page *page = Hu_MenuNewPage("WeaponOptions", &origin, 0, Hu_MenuPageTicker);
    page->setTitle("Weapons Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    {
        LabelWidget *text = new LabelWidget;
        text->_pageColorIdx = MENU_COLOR2;
        text->text = "Priority Order";
        page->_widgets << text;
    }

    {
        ListWidget *list = new ListWidget;
        list->setHelpInfo("Use left/right to move weapon up/down");
        list->setShortcut('p');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = Hu_MenuChangeWeaponPriority;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;

        for(int i = 0; weaponOrder[i].data < NUM_WEAPON_TYPES; ++i)
        {
            list->_items << new mndata_listitem_t(weaponOrder[i].text, weaponOrder[i].data);
        }
        qSort(list->_items.begin(), list->_items.end(), compareWeaponPriority);

        page->_widgets << list;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Cycling";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Use Priority Order";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("player-weapon-nextmode");
        btn->setGroup(1);
        btn->setShortcut('o');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Sequential";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("player-weapon-cycle-sequential");
        btn->setGroup(1);
        btn->setShortcut('s');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Autoswitch";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text = "Pickup Weapon";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->setGroup(2);
        list->setShortcut('w');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"player-autoswitch";

        list->_items << new mndata_listitem_t("Never",     0);
        list->_items << new mndata_listitem_t("If Better", 1);
        list->_items << new mndata_listitem_t("Always",    2);

        page->_widgets << list;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text = "   If Not Firing";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("player-autoswitch-notfiring");
        btn->setGroup(2)
            .setShortcut('f');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text = "Pickup Ammo";
        page->_widgets << text;
    }

    {
        InlineListWidget *list = new InlineListWidget;
        list->setGroup(2);
        list->setShortcut('a');
        list->_pageColorIdx  = MENU_COLOR3;
        list->actions[Widget::MNA_MODIFIED].callback = CvarListWidget_UpdateCvar;
        list->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        list->data  = (void *)"player-autoswitch-ammo";

        list->_items << new mndata_listitem_t("Never",     0);
        list->_items << new mndata_listitem_t("If Better", 1);
        list->_items << new mndata_listitem_t("Always",    2);

        page->_widgets << list;
    }

#if __JDOOM__ || __JDOOM64__
    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(2);
        text->text = "Pickup Beserk";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("player-autoswitch-berserk");
        btn->setGroup(2)
            .setShortcut('b');
        page->_widgets << btn;
    }
#endif
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitInventoryOptionsPage()
{
    Point2Raw const origin(78, 48);

    Page *page = Hu_MenuNewPage("InventoryOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Inventory Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Select Mode";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("ctl-inventory-mode");
        btn->setShortcut('s');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Wrap Around";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("ctl-inventory-wrap");
        btn->setShortcut('w');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Choose And Use";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("ctl-inventory-use-immediate");
        btn->setShortcut('c');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Select Next If Use Failed";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("ctl-inventory-use-next");
        btn->setShortcut('n');
        page->_widgets << btn;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "AutoHide";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new TextualSliderWidget;
        sld->setShortcut('h');
        sld->_pageColorIdx = MENU_COLOR3;
        sld->min           = 0;
        sld->max           = 30;
        sld->_value        = 0;
        sld->step          = 1.f;
        sld->floatMode     = true;
        sld->data1         = (void *)"hud-inventory-timer";
        sld->data2         = (void *)"Disabled";
        sld->data4         = (void *)" second";
        sld->data5         = (void *)" seconds";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->_pageColorIdx = MENU_COLOR2;
        text->text          = "Fullscreen HUD";
        page->_widgets << text;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Max Visible Slots";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new TextualSliderWidget;
        sld->setGroup(1);
        sld->setShortcut('v');
        sld->_pageColorIdx  = MENU_COLOR3;
        sld->min            = 0;
        sld->max            = 16;
        sld->_value         = 0;
        sld->step           = 1;
        sld->floatMode      = false;
        sld->data1          = (void *)"hud-inventory-slot-max";
        sld->data2          = (void *)"Automatic";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->setGroup(1);
        text->text = "Show Empty Slots";
        page->_widgets << text;
    }

    {
        CVarToggleWidget *btn = new CVarToggleWidget("hud-inventory-slot-showempty");
        btn->setGroup(1)
            .setShortcut('e');
        page->_widgets << btn;
    }
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

    Page *page = Hu_MenuNewPage("SoundOptions", &origin, 0, Hu_MenuPageTicker, NULL, NULL, NULL);
    page->setTitle("Sound Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuFindPageByName("Options"));

    {
        LabelWidget *text = new LabelWidget;
        text->text = "SFX Volume";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setShortcut('s');
        sld->min       = 0;
        sld->max       = 255;
        sld->_value    = 0;
        sld->step      = 5;
        sld->floatMode = false;
        sld->data1     = (void *)"sound-volume";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }

    {
        LabelWidget *text = new LabelWidget;
        text->text = "Music Volume";
        page->_widgets << text;
    }

    {
        SliderWidget *sld = new SliderWidget;
        sld->setShortcut('m');
        sld->min       = 0;
        sld->max       = 255;
        sld->_value    = 0;
        sld->step      = 5;
        sld->floatMode = false;
        sld->data1     = (void *)"music-volume";
        sld->actions[Widget::MNA_MODIFIED].callback = CvarSliderWidget_UpdateCvar;
        sld->actions[Widget::MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
        page->_widgets << sld;
    }
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

    Page *page = Hu_MenuNewPage("Episode", &origin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawEpisodePage);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("GameType"));

    int y = 0;
    int n = 0;
    DENG2_FOR_EACH_CONST(HexDefs::EpisodeInfos, it, hexDefs.episodeInfos)
    {
        EpisodeInfo const &info = it->second;

        ButtonWidget *btn = new ButtonWidget;

        btn->setText(info.gets("title"))
            .setFixedY(y);

        // Has a menu image been specified?
        de::Uri image(info.gets("menuImage"), RC_NULL);
        if(!image.path().isEmpty())
        {
            // Presently only patches are supported.
            if(!image.scheme().compareWithoutCase("Patches"))
            {
                btn->setPatch(R_DeclarePatch(image.path().toUtf8().constData()));
            }
        }

        // Has a menu shortcut/hotkey been specified?
        /// @todo Validate symbolic dday key names.
        String const shortcut = info.gets("menuShortcut");
        if(!shortcut.isEmpty() && shortcut.first().isLetterOrNumber())
        {
            btn->setShortcut(shortcut.first().toLower().toLatin1());
        }

        // Has a menu help/info text been specified?
        String const helpInfo = info.gets("menuHelpInfo");
        if(!helpInfo.isEmpty())
        {
            btn->setHelpInfo(helpInfo);
        }

        de::Uri startMap(info.gets("startMap"), RC_NULL);
        if(
#if __JHERETIC__
           gameMode == heretic_shareware
#else // __JDOOM__
           gameMode == doom_shareware
#endif
           && startMap.path() != "E1M1")
        {
            btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActivateNotSharewareEpisode;
        }
        else
        {
            btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuActionSetActivePage;
            btn->setData(String("Skill"));
        }

        btn->actions[Widget::MNA_FOCUS].callback = Hu_MenuFocusEpisode;
        btn->data2           = n;
        btn->_pageFontIdx    = MENU_FONT1;
        page->_widgets << btn;

        y += FIXED_LINE_HEIGHT;
        n += 1;
    }
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

    Page *page = Hu_MenuNewPage("PlayerClass", &pageOrigin, MPF_LAYOUT_FIXED|MPF_NEVER_SCROLL, Hu_MenuPageTicker, Hu_MenuDrawPlayerClassPage, NULL, NULL);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuFindPageByName("GameType"));

    uint y = 0;

    // Add the selectable classes.
    int n = 0;
    while(n < count)
    {
        classinfo_t *info = PCLASS_INFO(n++);

        if(!info->userSelectable) continue;

        ButtonWidget *btn = new ButtonWidget;

        if(info->niceName && (PTR2INT(info->niceName) > 0 && PTR2INT(info->niceName) < NUMTEXT))
        {
            btn->setText(GET_TXT(PTR2INT(info->niceName)));
        }
        else
        {
            btn->setText(info->niceName);
        }
        if(!btn->text().isEmpty() && btn->text().first().isLetterOrNumber()) btn->setShortcut(btn->text().first().toLatin1());
        btn->_origin.x      = 0;
        btn->_origin.y      = y;
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuFocusOnPlayerClass;
        btn->data2          = (int)info->plrClass;
        btn->_pageFontIdx   = MENU_FONT1;

        page->_widgets << btn;
        y += FIXED_LINE_HEIGHT;
    }

    // Random class button.
    {
        ButtonWidget *btn = new ButtonWidget;
        btn->setText(GET_TXT(TXT_RANDOMPLAYERCLASS));
        if(!btn->text().isEmpty() && btn->text().first().isLetterOrNumber()) btn->setShortcut(btn->text().first().toLatin1());
        btn->_origin.x      = 0;
        btn->_origin.y      = y;
        btn->actions[Widget::MNA_ACTIVEOUT].callback = Hu_MenuSelectPlayerClass;
        btn->actions[Widget::MNA_FOCUS    ].callback = Hu_MenuFocusOnPlayerClass;
        btn->data2          = (int)PCLASS_NONE;
        btn->_pageFontIdx   = MENU_FONT1;
        btn->_pageColorIdx  = MENU_COLOR1;
        page->_widgets << btn;
    }

    // Mobj preview background.
    {
        RectWidget *rect = new RectWidget;
        rect->_flags         = MNF_NO_FOCUS|MNF_ID1;
        rect->_origin.x      = 108;
        rect->_origin.y      = -58;
        rect->onTickCallback = Hu_MenuPlayerClassBackgroundTicker;
        rect->_pageFontIdx   = MENU_FONT1;
        rect->_pageColorIdx  = MENU_COLOR1;
        page->_widgets << rect;
    }

    // Mobj preview.
    {
        MobjPreviewWidget *mprev = new MobjPreviewWidget;
        mprev->_flags         = MNF_ID0;
        mprev->_origin.x      = 108 + 55;
        mprev->_origin.y      = -58 + 76;
        mprev->onTickCallback = Hu_MenuPlayerClassPreviewTicker;
        page->_widgets << mprev;
    }
}
#endif

static Page *addPageToCollection(Page *page, String name)
{
    String nameInIndex = name.toLower();
    if(pages.contains(nameInIndex))
    {
        delete pages[nameInIndex];
    }
    pages.insert(nameInIndex, page);
    return page;
}

Page *Hu_MenuNewPage(char const *name, Point2Raw const *origin, int flags,
    void (*ticker) (Page *page),
    void (*drawer) (Page *page, Point2Raw const *origin),
    int (*cmdResponder) (Page *page, menucommand_e cmd),
    void *userData)
{
    DENG2_ASSERT(origin != 0);
    DENG2_ASSERT(name != 0 && name[0]);
    return addPageToCollection(new Page(*origin, flags, ticker, drawer, cmdResponder, userData), name);
}

void Hu_MenuInit()
{
    // Close the menu (if open) and shutdown (if initialized - we're reinitializing).
    Hu_MenuShutdown();

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
    for(cvarbutton_t *cvb = mnCVarButtons; cvb->cvarname; cvb++)
    {
        if(!cvb->yes) cvb->yes = "Yes";
        if(!cvb->no) cvb->no = "No";
    }

    initAllPages();
    initAllObjectsOnAllPages();

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        Widget *wi = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("Main"), 0, MNF_ID0); // Read This!
        wi->setFlags(FO_SET, MNF_DISABLED|MNF_HIDDEN|MNF_NO_FOCUS);

        wi = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("Main"), 0, MNF_ID1); // Quit Game
        wi->setFixedY(wi->fixedY() - FIXED_LINE_HEIGHT);
    }
#endif

    inited = true;
}

void Hu_MenuShutdown()
{
    if(!inited) return;

    Hu_MenuCommand(MCMD_CLOSEFAST);
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

Page *Hu_MenuActivePage()
{
    return menuActivePage;
}

void Hu_MenuSetActivePage2(Page *page, dd_bool canReactivate)
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

void Hu_MenuSetActivePage(Page *page)
{
    Hu_MenuSetActivePage2(page, false/*don't reactivate*/);
}

dd_bool Hu_MenuIsVisible()
{
    return (menuActive || mnAlpha > .0001f);
}

void Hu_MenuDefaultFocusAction(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_FOCUS != action) return;
    Hu_MenuUpdateCursorState();
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

    int const cursorIdx = cursorAnimFrame;
    float const angle = cursorAngle;
    patchid_t pCursor = pCursors[cursorIdx % MENU_CURSOR_FRAMECOUNT];

    patchinfo_t info;
    if(!R_GetPatchInfo(pCursor, &info))
        return;

    float scale = de::min((focusObjectHeight * 1.267f) / info.geometry.size.height, 1.f);
    float pos[2];
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
    Widget *focusOb   = Hu_MenuActivePage()->focusObject();
    dd_bool showFocusCursor = true;
    if(focusOb && (focusOb->flags() & MNF_ACTIVE))
    {
        if(focusOb->is<ColorPreviewWidget>() || focusOb->is<InputBindingWidget>())
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
        if(focusOb->is<ColorPreviewWidget>())
        {
            drawOverlayBackground(OVERLAY_DARKEN);
            GL_BeginBorderedProjection(&bp);

            beginOverlayDraw();
                MN_DrawPage(Hu_MenuFindPageByName("ColorWidget"), 1, true);
            endOverlayDraw();

            GL_EndBorderedProjection(&bp);
        }
        if(InputBindingWidget *binds = focusOb->maybeAs<InputBindingWidget>())
        {
            drawOverlayBackground(OVERLAY_DARKEN);
            GL_BeginBorderedProjection(&bp);

            beginOverlayDraw();
                Hu_MenuControlGrabDrawer(binds->controlName(), 1);
            endOverlayDraw();

            GL_EndBorderedProjection(&bp);
        }
    }

#undef OVERLAY_DARKEN
}

void Hu_MenuPageTicker(Page *page)
{
    // Normal ticker actions first.
    page->tick();

    /// @todo Move game-menu specific page tick functionality here.
}

void Hu_MenuNavigatePage(Page * /*page*/, int /*pageDelta*/)
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
    foreach(Page *page, pages)
    {
        page->initObjects();
    }
}

int Hu_MenuColorWidgetCmdResponder(Page *page, menucommand_e cmd)
{
    DENG2_ASSERT(page != 0);
    switch(cmd)
    {
    case MCMD_NAV_OUT: {
        Widget *wi = (Widget *)page->userData;
        wi->setFlags(FO_CLEAR, MNF_ACTIVE);
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
        ColorPreviewWidget &cbox = ((Widget *)page->userData)->as<ColorPreviewWidget>();
        cbox.setFlags(FO_CLEAR, MNF_ACTIVE);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        colorWidgetActive = false;
        cbox.copyColor(0, MN_MustFindObjectOnPage(page, 0, MNF_ID0)->as<ColorPreviewWidget>());

        /// @kludge We should re-focus on the object instead.
        cursorAngle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true; }

    default: break;
    }

    return false;
}

static void fallbackCommandResponder(Page *page, menucommand_e cmd)
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
                    giveFocus = page->widgetCount() - 1;
                else if(giveFocus >= page->widgetCount())
                    giveFocus = 0;
            } while(++i < page->widgetCount() && (page->widgets()[giveFocus]->flags() & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN)));

            if(giveFocus != page->focus)
            {
                S_LocalSound(cmd == MCMD_NAV_UP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
                page->setFocus(page->widgets()[giveFocus]);
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
        if(Widget *wi = Hu_MenuActivePage()->focusObject())
        {
            if((wi->flags() & MNF_ACTIVE) &&
               (wi->is<LineEditWidget>() || wi->is<ListWidget>() || wi->is<ColorPreviewWidget>()))
            {
                cmd = MCMD_NAV_OUT;
            }
        }
    }

    return cmd;
}

void Hu_MenuCommand(menucommand_e cmd)
{
    cmd = translateCommand(cmd);

    // Determine the page which will respond to this command.
    Page *page = colorWidgetActive? Hu_MenuFindPageByName("ColorWidget") : Hu_MenuActivePage();

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
    Widget *wi = page->focusObject();
    if(wi && wi->cmdResponder)
    {
        if(wi->cmdResponder(wi, cmd))
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
        Widget *wi = Hu_MenuActivePage()->focusObject();
        if(wi && !(wi->flags() & MNF_DISABLED))
        {
            return wi->handleEvent_Privileged(ev);
        }
    }
    return false;
}

int Hu_MenuResponder(event_t *ev)
{
    if(Hu_MenuIsActive())
    {
        Widget *wi = Hu_MenuActivePage()->focusObject();
        if(wi && !(wi->flags() & MNF_DISABLED))
        {
            return wi->handleEvent(ev);
        }
    }
    return false; // Not eaten.
}

int Hu_MenuFallbackResponder(event_t *ev)
{
    Page *page = Hu_MenuActivePage();

    if(!Hu_MenuIsActive() || !page) return false;

    if(cfg.menuShortcutsEnabled)
    {
        if(ev->type == EV_KEY && (ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        {
            foreach(Widget *wi, page->widgets())
            {
                if(wi->flags() & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN))
                    continue;

                if(wi->shortcut() == ev->data1)
                {
                    page->setFocus(wi);
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
void Hu_MenuSelectLoadSlot(Widget *wi, Widget::mn_actionid_t action)
{
    LineEditWidget *edit = &wi->as<LineEditWidget>();

    if(Widget::MNA_ACTIVEOUT != action) return;

    // Linked focus between LoadGame and SaveGame pages.
    Page *page = Hu_MenuFindPageByName("SaveGame");
    page->setFocus(page->findObject(0, wi->data2));

    page = Hu_MenuFindPageByName("LoadGame");
    page->setFocus(page->findObject(0, wi->data2));

    G_SetGameActionLoadSession((char *)edit->data1);
    Hu_MenuCommand(chooseCloseMethod());
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(Page * /*page*/, Point2Raw const *origin)
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
                 Vector2i(origin->x + TITLEOFFSET_X, origin->y + TITLEOFFSET_Y), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
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

void Hu_MenuDrawGameTypePage(Page * /*page*/, Point2Raw const *origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PICKGAMETYPE), SCREENWIDTH/2, origin->y - 28);
}

#if __JHEXEN__
/**
 * A specialization of MNRect_Ticker() which implements the animation logic
 * for the player class selection page's player visual background.
 */
void Hu_MenuPlayerClassBackgroundTicker(Widget *wi)
{
    DENG2_ASSERT(wi != 0);
    RectWidget &bg = wi->as<RectWidget>();

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    if(Widget *mop = wi->page()->focusObject())
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

        bg.setBackgroundPatch(pPlayerClassBG[pClass]);
    }
}

/**
 * A specialization of MNMobjPreview_Ticker() which implements the animation
 * logic for the player class selection page's player visual.
 */
void Hu_MenuPlayerClassPreviewTicker(Widget *wi)
{
    DENG2_ASSERT(wi != 0);
    MobjPreviewWidget &mprev = wi->as<MobjPreviewWidget>();

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    if(Widget *mop = wi->page()->focusObject())
    {
        playerclass_t pClass = (playerclass_t) mop->data2;
        if(pClass == PCLASS_NONE)
        {
            // Random class.
            /// @todo Use this object's timer instead of menuTime.
            pClass = playerclass_t(PCLASS_FIRST + (menuTime / 5));
            pClass = playerclass_t(int(pClass) % 3); // Number of user-selectable classes.

            mprev.setPlayerClass(pClass);
            mprev.setMobjType(PCLASS_INFO(pClass)->mobjType);
        }

        // Fighter is Yellow, others Red by default.
        mprev.setTranslationClass(pClass);
        mprev.setTranslationMap(pClass == PCLASS_FIGHTER? 2 : 0);
    }
}

void Hu_MenuDrawPlayerClassPage(Page * /*page*/, Point2Raw const *origin)
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
void Hu_MenuDrawEpisodePage(Page *page, Point2Raw const *origin)
{
#if __JDOOM__
    DENG2_UNUSED(page);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorv(cfg.menuTextColors[0]);
    FR_SetAlpha(mnRendState->pageAlpha);

    WI_DrawPatch(pEpisode, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pEpisode),
                 Vector2i(origin->x + 7, origin->y - 25), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#else
    DENG2_UNUSED2(page, origin);
#endif
}
#endif

void Hu_MenuDrawSkillPage(Page * /*page*/, Point2Raw const *origin)
{
#if __JDOOM__ || __JDOOM64__
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pNewGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pNewGame),
                 Vector2i(origin->x + 48, origin->y - 49), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
    WI_DrawPatch(pSkill, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pSkill),
                 Vector2i(origin->x + 6, origin->y - 25), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

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
void Hu_MenuSelectSaveSlot(Widget *wi, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;

    LineEditWidget &edit = wi->as<LineEditWidget>();
    char const *saveSlotId = (char *)edit.data1;

    if(menuNominatingQuickSaveSlot)
    {
        Con_SetInteger("game-save-quick-slot", String(saveSlotId).toInt());
        menuNominatingQuickSaveSlot = false;
    }

    String userDescription = Str_Text(edit.text());
    if(!G_SetGameActionSaveSession(saveSlotId, &userDescription))
    {
        return;
    }

    Page *page = Hu_MenuFindPageByName("SaveGame");
    page->setFocus(MN_MustFindObjectOnPage(page, 0, wi->data2));

    page = Hu_MenuFindPageByName("LoadGame");
    page->setFocus(MN_MustFindObjectOnPage(page, 0, wi->data2));

    Hu_MenuCommand(chooseCloseMethod());
}

void Hu_MenuSaveSlotEdit(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    if(Widget::MNA_ACTIVE != action) return;
    if(cfg.menuGameSaveSuggestDescription)
    {
        String const description = G_DefaultSavedSessionUserDescription("" /*don't reuse an existing description*/);
        wi->as<LineEditWidget>().setText(MNEDIT_STF_NO_ACTION, description.toLatin1().constData());
    }
}

void Hu_MenuActivateColorWidget(Widget *wi, Widget::mn_actionid_t action)
{
    if(action != Widget::MNA_ACTIVE) return;

    ColorPreviewWidget &cbox = wi->as<ColorPreviewWidget>();
    Page *colorWidgetPage = Hu_MenuFindPageByName("ColorWidget");

    ColorPreviewWidget &cboxMix = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID0)->as<ColorPreviewWidget>();
    SliderWidget &sldrRed   = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID1)->as<SliderWidget>();
    SliderWidget &sldrGreen = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID2)->as<SliderWidget>();
    SliderWidget &sldrBlue  = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID3)->as<SliderWidget>();
    SliderWidget &textAlpha = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID4)->as<SliderWidget>();
    SliderWidget &sldrAlpha = MN_MustFindObjectOnPage(colorWidgetPage, 0, MNF_ID5)->as<SliderWidget>();

    colorWidgetActive = true;

    colorWidgetPage->initialize();
    colorWidgetPage->userData = wi;

    cboxMix.copyColor(0, cbox);

    sldrRed  .setValue(MNSLIDER_SVF_NO_ACTION, cbox.redf());
    sldrGreen.setValue(MNSLIDER_SVF_NO_ACTION, cbox.greenf());
    sldrBlue .setValue(MNSLIDER_SVF_NO_ACTION, cbox.bluef());
    sldrAlpha.setValue(MNSLIDER_SVF_NO_ACTION, cbox.alphaf());

    textAlpha.setFlags((cbox.rgbaMode()? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);
    sldrAlpha.setFlags((cbox.rgbaMode()? FO_CLEAR : FO_SET), MNF_DISABLED|MNF_HIDDEN);
}

void Hu_MenuDrawLoadGamePage(Page * /*page*/, Point2Raw const *origin)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JHERETIC__ || __JHEXEN__
    FR_DrawTextXY3("Load Game", SCREENWIDTH/2, origin->y-20, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));
#else
    WI_DrawPatch(pLoadGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pLoadGame),
                 Vector2i(origin->x - 8, origin->y - 26), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
#endif
    DGL_Disable(DGL_TEXTURE_2D);

    Point2Raw helpOrigin(SCREENWIDTH/2, (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale));
    Hu_MenuDrawPageHelp("Select to load, [Del] to clear", helpOrigin.x, helpOrigin.y);
}

void Hu_MenuDrawSaveGamePage(Page * /*page*/, Point2Raw const *origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Save Game", SCREENWIDTH/2, origin->y - 20);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pSaveGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pSaveGame),
                 Vector2i(origin->x - 8, origin->y - 26), ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif

    Point2Raw helpOrigin(SCREENWIDTH/2, (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale));
    Hu_MenuDrawPageHelp("Select to save, [Del] to clear", helpOrigin.x, helpOrigin.y);
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void Hu_MenuSelectHelp(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;
    G_StartHelp();
}
#endif

void Hu_MenuDrawOptionsPage(Page * /*page*/, Point2Raw const *origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Options", origin->x + 42, origin->y - 38);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[0][CR], cfg.menuTextColors[0][CG], cfg.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pOptionsTitle, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), pOptionsTitle),
                 Vector2i(origin->x + 42, origin->y - 20), ALIGN_TOP, 0, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void Hu_MenuDrawMultiplayerPage(Page * /*page*/, Point2Raw const *origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_MULTIPLAYER), SCREENWIDTH/2, origin->y - 28);
}

void Hu_MenuDrawPlayerSetupPage(Page * /*page*/, Point2Raw const *origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PLAYERSETUP), SCREENWIDTH/2, origin->y - 28);
}

void Hu_MenuActionSetActivePage(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    if(Widget::MNA_ACTIVEOUT != action) return;
    Hu_MenuSetActivePage(Hu_MenuFindPageByName(wi->as<ButtonWidget>().data().toString()));
}

void Hu_MenuUpdateColorWidgetColor(Widget *wi, Widget::mn_actionid_t action)
{
    if(Widget::MNA_MODIFIED != action) return;

    SliderWidget &sldr = wi->as<SliderWidget>();
    float value = sldr.value();
    ColorPreviewWidget &cboxMix = MN_MustFindObjectOnPage(Hu_MenuFindPageByName("ColorWidget"), 0, MNF_ID0)->as<ColorPreviewWidget>();

    switch(wi->data2)
    {
    case CR: cboxMix.setRedf  (MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CG: cboxMix.setGreenf(MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CB: cboxMix.setBluef (MNCOLORBOX_SCF_NO_ACTION, value); break;
    case CA: cboxMix.setAlphaf(MNCOLORBOX_SCF_NO_ACTION, value); break;

    default: DENG2_ASSERT(!"Hu_MenuUpdateColorWidgetColor: Invalid value for data2.");
    }
}

void Hu_MenuChangeWeaponPriority(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_MODIFIED != action) return;

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
}

void Hu_MenuSelectSingleplayer(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;

    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NEWGAME, NULL, 0, NULL);
        return;
    }

#if __JHEXEN__
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("PlayerClass"));
#elif __JHERETIC__
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("Episode"));
#elif __JDOOM64__
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("Skill"));
#else // __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        Hu_MenuSetActivePage(Hu_MenuFindPageByName("Skill"));
    }
    else
    {
        Hu_MenuSetActivePage(Hu_MenuFindPageByName("Episode"));
    }
#endif
}

void Hu_MenuSelectMultiplayer(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    Page *multiplayerPage = Hu_MenuFindPageByName("Multiplayer");
    Widget *label = MN_MustFindObjectOnPage(multiplayerPage, 0, MNF_ID0);
    ButtonWidget *btn = &label->as<ButtonWidget>();

    if(Widget::MNA_ACTIVEOUT != action) return;

    // Set the appropriate label.
    if(IS_NETGAME)
    {
        btn->setText("Disconnect");
    }
    else
    {
        btn->setText("Join Game");
    }
    Hu_MenuSetActivePage(multiplayerPage);
}

void Hu_MenuSelectJoinGame(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;

    if(IS_NETGAME)
    {
        DD_Execute(false, "net disconnect");
        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    DD_Execute(false, "net setup client");
}

void Hu_MenuActivatePlayerSetup(Page *page)
{
    DENG2_ASSERT(page != 0);

    MobjPreviewWidget &mop = MN_MustFindObjectOnPage(page, 0, MNF_ID0)->as<MobjPreviewWidget>();
    LineEditWidget &name   = MN_MustFindObjectOnPage(page, 0, MNF_ID1)->as<LineEditWidget>();
    ListWidget &color      = MN_MustFindObjectOnPage(page, 0, MNF_ID3)->as<ListWidget>();

#if __JHEXEN__
    mop.setMobjType(PCLASS_INFO(cfg.netClass)->mobjType);
    mop.setPlayerClass(cfg.netClass);
#else
    mop.setMobjType(MT_PLAYER);
    mop.setPlayerClass(PCLASS_PLAYER);
#endif
    mop.setTranslationClass(0);
    mop.setTranslationMap(cfg.netColor);

    color.selectItemByValue(MNLIST_SIF_NO_ACTION, cfg.netColor);
#if __JHEXEN__
    ListWidget &class_ = MN_MustFindObjectOnPage(page, 0, MNF_ID2)->as<ListWidget>();
    class_.selectItemByValue(MNLIST_SIF_NO_ACTION, cfg.netClass);
#endif

    name.setText(MNEDIT_STF_NO_ACTION|MNEDIT_STF_REPLACEOLD, Con_GetString("net-name"));
}

#if __JHEXEN__
void Hu_MenuSelectPlayerSetupPlayerClass(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    if(Widget::MNA_MODIFIED != action) return;

    ListWidget &list = wi->as<ListWidget>();
    int selection = list.selection();
    if(selection >= 0)
    {
        MobjPreviewWidget &mop = MN_MustFindObjectOnPage(wi->page(), 0, MNF_ID0)->as<MobjPreviewWidget>();
        mop.setPlayerClass(selection);
        mop.setMobjType(PCLASS_INFO(selection)->mobjType);
    }
}
#endif

void Hu_MenuSelectPlayerColor(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    if(Widget::MNA_MODIFIED != action) return;

    // The color translation map is stored in the list item data member.
    ListWidget &list = wi->as<ListWidget>();
    int selection = list.itemData(list.selection());
    if(selection >= 0)
    {
        MobjPreviewWidget &mop = MN_MustFindObjectOnPage(wi->page(), 0, MNF_ID0)->as<MobjPreviewWidget>();
        mop.setTranslationMap(selection);
    }
}

void Hu_MenuSelectAcceptPlayerSetup(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    LineEditWidget &plrNameEdit = MN_MustFindObjectOnPage(wi->page(), 0, MNF_ID1)->as<LineEditWidget>();
#if __JHEXEN__
    ListWidget &plrClassList = MN_MustFindObjectOnPage(wi->page(), 0, MNF_ID2)->as<ListWidget>();
#endif
    ListWidget &plrColorList = MN_MustFindObjectOnPage(wi->page(), 0, MNF_ID3)->as<ListWidget>();

#if __JHEXEN__
    cfg.netClass = plrClassList.selection();
#endif
    // The color translation map is stored in the list item data member.
    cfg.netColor = plrColorList.itemData(plrColorList.selection());

    if(Widget::MNA_ACTIVEOUT != action) return;

    char buf[300];
    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, Str_Text(plrNameEdit.text()), 300);
    DD_Execute(false, buf);

    if(IS_NETGAME)
    {
        strcpy(buf, "setname ");
        M_StrCatQuoted(buf, Str_Text(plrNameEdit.text()), 300);
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
}

void Hu_MenuSelectQuitGame(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;
    G_QuitGame();
}

void Hu_MenuSelectEndGame(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;
    DD_Executef(true, "endgame");
}

void Hu_MenuSelectLoadGame(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;

    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT && !Get(DD_PLAYBACK))
        {
            Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, 0, NULL);
            return;
        }
    }

    Hu_MenuSetActivePage(Hu_MenuFindPageByName("LoadGame"));
}

void Hu_MenuSelectSaveGame(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    player_t *player = &players[CONSOLEPLAYER];

    if(Widget::MNA_ACTIVEOUT != action) return;

    if(!Get(DD_DEDICATED))
    {
        if(IS_CLIENT)
        {
#if __JDOOM__ || __JDOOM64__
            Hu_MsgStart(MSG_ANYKEY, SAVENET, NULL, 0, NULL);
#endif
            return;
        }

        if(G_GameState() != GS_MAP)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, NULL, 0, NULL);
            return;
        }

        if(player->playerState == PST_DEAD)
        {
            Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, NULL, 0, NULL);
            return;
        }
    }

    Hu_MenuCommand(MCMD_OPEN);
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("SaveGame"));
}

#if __JHEXEN__
void Hu_MenuSelectPlayerClass(Widget *wi, Widget::mn_actionid_t action)
{
    Page *skillPage = Hu_MenuFindPageByName("Skill");
    int option = wi->data2;
    ButtonWidget *skillObj;

    if(Widget::MNA_ACTIVEOUT != action) return;

    if(IS_NETGAME)
    {
        P_SetMessage(&players[CONSOLEPLAYER], LMF_NO_HIDE, "You can't start a new game from within a netgame!");
        return;
    }

    if(option < 0)
    {
        // Random class.
        // Number of user-selectable classes.
        mnPlrClass = (menuTime / 5) % 3;
    }
    else
    {
        mnPlrClass = option;
    }

    skillObj = &MN_MustFindObjectOnPage(skillPage, 0, MNF_ID0)->as<ButtonWidget>();
    skillObj->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_BABY]));
    if(!skillObj->text().isEmpty() && skillObj->text().first().isLetterOrNumber()) skillObj->setShortcut(skillObj->text().first().toLatin1());

    skillObj = &MN_MustFindObjectOnPage(skillPage, 0, MNF_ID1)->as<ButtonWidget>();
    skillObj->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_EASY]));
    if(!skillObj->text().isEmpty() && skillObj->text().first().isLetterOrNumber()) skillObj->setShortcut(skillObj->text().first().toLatin1());

    skillObj = &MN_MustFindObjectOnPage(skillPage, 0, MNF_ID2)->as<ButtonWidget>();
    skillObj->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_MEDIUM]));
    if(!skillObj->text().isEmpty() && skillObj->text().first().isLetterOrNumber()) skillObj->setShortcut(skillObj->text().first().toLatin1());

    skillObj = &MN_MustFindObjectOnPage(skillPage, 0, MNF_ID3)->as<ButtonWidget>();
    skillObj->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_HARD]));
    if(!skillObj->text().isEmpty() && skillObj->text().first().isLetterOrNumber()) skillObj->setShortcut(skillObj->text().first().toLatin1());

    skillObj = &MN_MustFindObjectOnPage(skillPage, 0, MNF_ID4)->as<ButtonWidget>();
    skillObj->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeNames[SM_NIGHTMARE]));
    if(!skillObj->text().isEmpty() && skillObj->text().first().isLetterOrNumber()) skillObj->setShortcut(skillObj->text().first().toLatin1());

    switch(mnPlrClass)
    {
    case PCLASS_FIGHTER:    skillPage->setX(120); break;
    case PCLASS_CLERIC:     skillPage->setX(116); break;
    case PCLASS_MAGE:       skillPage->setX(112); break;
    }
    Hu_MenuSetActivePage(skillPage);
}

void Hu_MenuFocusOnPlayerClass(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);

    if(Widget::MNA_FOCUS != action) return;

    playerclass_t plrClass = (playerclass_t)wi->data2;
    MobjPreviewWidget &mop = MN_MustFindObjectOnPage(wi->page(), 0, MNF_ID0)->as<MobjPreviewWidget>();
    mop.setPlayerClass(plrClass);
    mop.setMobjType((PCLASS_NONE == plrClass? MT_NONE : PCLASS_INFO(plrClass)->mobjType));

    Hu_MenuDefaultFocusAction(wi, action);
}
#endif

#if __JDOOM__ || __JHERETIC__
void Hu_MenuFocusEpisode(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    if(Widget::MNA_FOCUS != action) return;
    mnEpisode = wi->data2;
    Hu_MenuDefaultFocusAction(wi, action);
}

int Hu_MenuConfirmOrderCommericalVersion(msgresponse_t /*response*/, int /*userValue*/, void * /*context*/)
{
    G_StartHelp();
    return true;
}

void Hu_MenuActivateNotSharewareEpisode(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;
    Hu_MsgStart(MSG_ANYKEY, SWSTRING, Hu_MenuConfirmOrderCommericalVersion, 0, NULL);
}
#endif

void Hu_MenuFocusSkillMode(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);

    if(Widget::MNA_FOCUS != action) return;
    mnSkillmode = (skillmode_t)wi->data2;
    Hu_MenuDefaultFocusAction(wi, action);
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

void Hu_MenuActionInitNewGame(Widget * /*wi*/, Widget::mn_actionid_t action)
{
    if(Widget::MNA_ACTIVEOUT != action) return;
    Hu_MenuInitNewGame(false);
}

void Hu_MenuSelectControlPanelLink(Widget *wi, Widget::mn_actionid_t action)
{
#define NUM_PANEL_NAMES         1

    static char const *panelNames[NUM_PANEL_NAMES] = {
        "taskbar" //,
        //"panel audio",
        //"panel input"
    };

    if(Widget::MNA_ACTIVEOUT != action) return;

    int idx = wi->data2;
    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
    {
        idx = 0;
    }

    DD_Execute(true, panelNames[idx]);

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

        if(Page *page = Hu_MenuFindPageByName(argv[1]))
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
    C_VAR_INT  ("menu-patch-replacement",   &cfg.menuPatchReplaceMode,   0, 0, 1);
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

} // namespace common
