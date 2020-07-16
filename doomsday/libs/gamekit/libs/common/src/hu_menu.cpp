/** @file hu_menu.cpp  Menu widget stuff, episode selection and such.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
#include <de/keymap.h>
#include <de/legacy/memory.h>
#include <de/recordvalue.h>
#include <de/textvalue.h>
#include <de/nativepointervalue.h>
#include "g_common.h"
#include "g_controls.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_msg.h"
#include "hu_stuff.h"
#include "m_argv.h"
#include "m_ctrl.h"
#include "p_savedef.h"
#include "player.h"
#include "r_common.h"
#include "saveslots.h"
#include "x_hair.h"

#include "menu/page.h"
#include "menu/widgets/coloreditwidget.h"
#include "menu/widgets/cvarcoloreditwidget.h"
#include "menu/widgets/cvarinlinelistwidget.h"
#include "menu/widgets/cvarlineeditwidget.h"
#include "menu/widgets/cvarsliderwidget.h"
#include "menu/widgets/cvartextualsliderwidget.h"
#include "menu/widgets/cvartogglewidget.h"
#include "menu/widgets/inputbindingwidget.h"
#include "menu/widgets/labelwidget.h"
#include "menu/widgets/mobjpreviewwidget.h"
#include "menu/widgets/rectwidget.h"
#include "menu/widgets/sliderwidget.h"

using namespace de;

namespace common {

using namespace common::menu;

/// Original game line height for pages that employ the fixed layout (in 320x200 pixels).
#if __JDOOM__
#  define FIXED_LINE_HEIGHT (15+1)
#else
#  define FIXED_LINE_HEIGHT (19+1)
#endif

void Hu_MenuActivatePlayerSetup(Page &page);

void Hu_MenuActionSetActivePage(Widget &wi, Widget::Action action);
void Hu_MenuActionInitNewGame(Widget &wi, Widget::Action action);

void Hu_MenuSelectLoadGame(Widget &wi, Widget::Action action);
void Hu_MenuSelectSaveGame(Widget &wi, Widget::Action action);
void Hu_MenuSelectJoinGame(Widget &wi, Widget::Action action);

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void Hu_MenuSelectHelp(Widget &wi, Widget::Action action);
#endif
void Hu_MenuSelectControlPanelLink(Widget &wi, Widget::Action action);

void Hu_MenuSelectSingleplayer(Widget &wi, Widget::Action action);
//void Hu_MenuSelectMultiplayer(Widget &wi, Widget::Action action);
void Hu_MenuSelectEpisode(Widget &wi, Widget::Action action);
#if __JDOOM__ || __JHERETIC__
void Hu_MenuActivateNotSharewareEpisode(Widget &wi, Widget::Action action);
#endif
#if __JHEXEN__
void Hu_MenuFocusOnPlayerClass(Widget &wi, Widget::Action action);
void Hu_MenuSelectPlayerClass(Widget &wi, Widget::Action action);
#endif
void Hu_MenuFocusSkillMode(Widget &wi, Widget::Action action);
void Hu_MenuSelectLoadSlot(Widget &wi, Widget::Action action);
void Hu_MenuSelectQuitGame(Widget &wi, Widget::Action action);
void Hu_MenuSelectEndGame(Widget &wi, Widget::Action action);
void Hu_MenuSelectAcceptPlayerSetup(Widget &wi, Widget::Action action);

void Hu_MenuSelectSaveSlot(Widget &wi, Widget::Action action);

void Hu_MenuChangeWeaponPriority(Widget &wi, Widget::Action action);
#if __JHEXEN__
void Hu_MenuSelectPlayerSetupPlayerClass(Widget &wi, Widget::Action action);
#endif
void Hu_MenuSelectPlayerColor(Widget &wi, Widget::Action action);

#if __JHEXEN__
void Hu_MenuPlayerClassBackgroundTicker(Widget &wi);
void Hu_MenuPlayerClassPreviewTicker(Widget &wi);
#endif

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(const Page &page, const Vec2i &origin);
#endif

//void Hu_MenuDrawGameTypePage(const Page &page, const Vec2i &origin);
void Hu_MenuDrawSkillPage(const Page &page, const Vec2i &origin);
#if __JHEXEN__
void Hu_MenuDrawPlayerClassPage(const Page &page, const Vec2i &origin);
#endif
void Hu_MenuDrawEpisodePage(const Page &page, const Vec2i &origin);
void Hu_MenuDrawOptionsPage(const Page &page, const Vec2i &origin);
void Hu_MenuDrawLoadGamePage(const Page &page, const Vec2i &origin);
void Hu_MenuDrawSaveGamePage(const Page &page, const Vec2i &origin);
void Hu_MenuDrawMultiplayerPage(const Page &page, const Vec2i &origin);
void Hu_MenuDrawPlayerSetupPage(const Page &page, const Vec2i &origin);

int Hu_MenuColorWidgetCmdResponder(Page &page, menucommand_e cmd);
int Hu_MenuSkipPreviousPageIfSkippingEpisodeSelection(Page &page, menucommand_e cmd);

void Hu_MenuSaveSlotEdit(Widget &wi, Widget::Action action);

void Hu_MenuActivateColorWidget(Widget &wi, Widget::Action action);
void Hu_MenuUpdateColorWidgetColor(Widget &wi, Widget::Action action);

static void Hu_MenuInitNewGame(bool confirmed);

static void initAllPages();
static void destroyAllPages();

static void Hu_MenuUpdateCursorState();

static bool Hu_MenuHasCursorRotation(Widget *wi);

int menuTime;
dd_bool menuNominatingQuickSaveSlot;

static Page *currentPage;
static bool menuActive;

static float mnAlpha; // Alpha level for the entire menu.
static float mnTargetAlpha; // Target alpha for the entire UI.

static skillmode_t mnSkillmode = SM_MEDIUM;
static String mnEpisode;
#if __JHEXEN__
static int mnPlrClass = PCLASS_FIGHTER;
#endif

static int frame; // Used by any graphic animations that need to be pumped.

static bool colorWidgetActive;

// Present cursor state.
struct Cursor
{
    bool hasRotation = false;
    float angle      = 0;
    int animCounter  = 0;
    int animFrame    = 0;
};
static Cursor cursor;

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

static bool inited;

typedef KeyMap<String, Page *> Pages;
static Pages pages;

static menucommand_e chooseCloseMethod()
{
    // If we aren't using a transition then we can close normally and allow our
    // own menu fade-out animation to be used instead.
    return Con_GetInteger("con-transition-tics") == 0? MCMD_CLOSE : MCMD_CLOSEFAST;
}

bool Hu_MenuHasPage(String name)
{
    if(!name.isEmpty())
    {
        return pages.contains(name.lower());
    }
    return false;
}

Page &Hu_MenuPage(String name)
{
    if(!name.isEmpty())
    {
        Pages::iterator found = pages.find(name.lower());
        if(found != pages.end())
        {
            return *found->second;
        }
    }
    /// @throw Error No Page exists with the name specified.
    throw Error("Hu_MenuPage", "Unknown page '" + name + "'");
}

/// @todo Make this state an object property flag.
/// @return  @c true if the rotation of a cursor on this object should be animated.
static bool Hu_MenuHasCursorRotation(Widget *wi)
{
    DE_ASSERT(wi != 0);
    return (!wi->isDisabled() && (is<InlineListWidget>(wi) || is<SliderWidget>(wi)));
}

/// To be called to re-evaluate the state of the cursor (e.g., when focus changes).
static void Hu_MenuUpdateCursorState()
{
    if(menuActive)
    {
        Page *page = colorWidgetActive? Hu_MenuPagePtr("ColorWidget") : Hu_MenuPagePtr();
        if(Widget *wi = page->focusWidget())
        {
            cursor.hasRotation = Hu_MenuHasCursorRotation(wi);
            return;
        }
    }
    cursor.hasRotation = false;
}

static void Hu_MenuLoadResources()
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
    Vec2i const origin(98, 60);
#else
    Vec2i const origin(124, 60);
#endif

    Page *page = Hu_MenuAddPage(new Page("ColorWidget", origin, Page::NoScroll, NULL, Hu_MenuColorWidgetCmdResponder));
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));

    page->addWidget(new ColorEditWidget(Vec4f(), true))
            .setPreviewDimensions(Vec2i(SCREENHEIGHT / 7, SCREENHEIGHT / 7))
            .setFlags(Widget::Id0 | Widget::NoFocus);

    page->addWidget(new LabelWidget("Red"));

    page->addWidget(new SliderWidget(0.0f, 1.0f, .05f))
            .setFlags(Widget::Id1)
            .setShortcut('r')
            .setUserValue2(NumberValue(CR))
            .setAction(Widget::Modified,    Hu_MenuUpdateColorWidgetColor)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new LabelWidget("Green"));
    page->addWidget(new SliderWidget(0.0f, 1.0f, .05f))
            .setFlags(Widget::Id2)
            .setShortcut('g')
            .setUserValue2(NumberValue(CG))
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction)
            .setAction(Widget::Modified,    Hu_MenuUpdateColorWidgetColor);

    page->addWidget(new LabelWidget("Blue"));
    page->addWidget(new SliderWidget(0.0f, 1.0f, 0.05f))
            .setFlags(Widget::Id3)
            .setShortcut('b')
            .setUserValue2(NumberValue(CB))
            .setAction(Widget::Modified,    Hu_MenuUpdateColorWidgetColor)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new LabelWidget("Opacity")).setFlags(Widget::Id4);
    page->addWidget(new SliderWidget(0.0f, 1.0f, 0.05f))
            .setFlags(Widget::Id5)
            .setShortcut('o')
            .setUserValue2(NumberValue(CA))
            .setAction(Widget::Modified,    Hu_MenuUpdateColorWidgetColor)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
}

void Hu_MenuInitMainPage()
{
#if __JHEXEN__ || __JHERETIC__
    Vec2i origin(110, 56);
#else
    Vec2i origin(97, 64);
#endif

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        origin.y += 8;
    }
#endif

#if __JDOOM__ || __JDOOM64__
    Page *page = Hu_MenuAddPage(new Page("Main", origin, Page::FixedLayout | Page::NoScroll));
#else
    Page *page = Hu_MenuAddPage(new Page("Main", origin, Page::FixedLayout | Page::NoScroll, Hu_MenuDrawMainPage));
#endif
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));

    int y = 0;

#if __JDOOM__ || __JDOOM64__
    page->addWidget(new LabelWidget("", &pMainTitle))
            .setFixedOrigin(Vec2i(-3, -70));
#endif

    page->addWidget(new ButtonWidget)
#if defined(__JDOOM__) && !defined(__JDOOM64__)
            .setPatch(pNGame)
#else
            .setText("New Game")
#endif
            .setFixedY(y)
            .setShortcut('n')
            .setFont(MENU_FONT1)
            //.setUserValue(String("GameType"))
            .setAction(Widget::Deactivated, Hu_MenuSelectSingleplayer)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;

    page->addWidget(new ButtonWidget)
#if defined(__JDOOM__) && !defined(__JDOOM64__)
            .setPatch(pOptions)
#else
            .setText("Options")
#endif
            .setFixedY(y)
            .setShortcut('o')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("Options"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;

#if __JDOOM__ || __JDOOM64__
    page->addWidget(new ButtonWidget)
# if __JDOOM64__
            .setText("Load Game")
# else
            .setPatch(pLoadGame)
# endif
            .setFixedY(y)
            .setShortcut('l')
            .setFont(MENU_FONT1)
            .setAction(Widget::Deactivated, Hu_MenuSelectLoadGame)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;

    page->addWidget(new ButtonWidget)
# if __JDOOM64__
            .setText("Save Game")
# else
            .setPatch(pSaveGame)
# endif
            .setFixedY(y)
            .setShortcut('s')
            .setFont(MENU_FONT1)
            .setAction(Widget::Deactivated, Hu_MenuSelectSaveGame)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;

#else
    page->addWidget(new ButtonWidget("Game Files"))
            .setFixedY(y)
            .setShortcut('f')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("Files"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;
#endif

#if !__JDOOM64__
    page->addWidget(new ButtonWidget)
# if defined(__JDOOM__)
            .setPatch(pReadThis)
# else
            .setText("Info")
# endif
            .setFixedY(y)
# if __JDOOM__
            .setFlags(Widget::Id0)
            .setShortcut('r')
# else
            .setShortcut('i')
# endif
            .setFont(MENU_FONT1)
            .setAction(Widget::Deactivated, Hu_MenuSelectHelp)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;
#endif

    page->addWidget(new ButtonWidget)
#if defined(__JDOOM__) && !defined(__JDOOM64__)
            .setPatch(pQuitGame)
#else
            .setText("Quit Game")
#endif
#if __JDOOM__
            .setFlags(Widget::Id1)
#endif
            .setFixedY(y)
            .setShortcut('q')
            .setFont(MENU_FONT1)
            .setAction(Widget::Deactivated, Hu_MenuSelectQuitGame)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
}

#if 0
void Hu_MenuInitGameTypePage()
{
#if __JDOOM__ || __JDOOM64__
    Vec2i origin(97, 65);
#else
    Vec2i origin(104, 65);
#endif

    Page *page = Hu_MenuAddPage(new Page("GameType", origin, 0, Hu_MenuDrawGameTypePage));
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuPagePtr("Main"));

    int y = 0;

    String labelText = GET_TXT(TXT_SINGLEPLAYER);
    int shortcut     = labelText.first().isLetterOrNumber()? labelText.first() : 0;
    page->addWidget(new ButtonWidget(labelText))
            .setFixedY(y)
            .setFont(MENU_FONT1)
            .setShortcut(shortcut)
            .setAction(Widget::Deactivated, Hu_MenuSelectSingleplayer)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;

    labelText = GET_TXT(TXT_MULTIPLAYER);
    shortcut  = labelText.first().isLetterOrNumber()? labelText.first() : 0;
    page->addWidget(new ButtonWidget(labelText))
            .setFixedY(y)
            .setFont(MENU_FONT1)
            .setShortcut(shortcut)
            .setAction(Widget::Deactivated, Hu_MenuSelectMultiplayer)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
}
#endif

void Hu_MenuInitSkillPage()
{
#if __JHEXEN__
    Vec2i const origin(120, 44);
#elif __JHERETIC__
    Vec2i const origin(38, 30);
#else
    Vec2i const origin(48, 63);
#endif
    Widget::Flags skillButtonFlags[NUM_SKILL_MODES] = {
        Widget::Id0,
        Widget::Id1,
        Widget::Id2 | Widget::DefaultFocus,
        Widget::Id3,
#  if !__JDOOM64__
        Widget::Id4
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

    Page *page = Hu_MenuAddPage(new Page("Skill", origin, Page::FixedLayout | Page::NoScroll,
                                         Hu_MenuDrawSkillPage, Hu_MenuSkipPreviousPageIfSkippingEpisodeSelection));
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuPagePtr("Episode"));

    int y = 0;

    for(uint i = 0; i < NUM_SKILL_MODES; ++i, y += FIXED_LINE_HEIGHT)
    {
#if !__JHEXEN__
        const String labelText = GET_TXT(skillButtonTexts[i]);
        const int shortcut     = labelText.first().isAlphaNumeric() ? int(labelText.first().unicode()) : 0;
#endif

        page->addWidget(new ButtonWidget)
#if !__JHEXEN__
                .setText(labelText)
# if __JDOOM__ || __JDOOM64__
                .setPatch(pSkillModeNames[i])
# endif
                .setShortcut(shortcut)
#endif
                .setFlags(skillButtonFlags[i])
                .setFixedY(y)
                .setFont(MENU_FONT1)
                .setUserValue2(NumberValue(SM_BABY + i))
                .setAction(Widget::Deactivated, Hu_MenuActionInitNewGame)
                .setAction(Widget::FocusGained, Hu_MenuFocusSkillMode);
    }

#if __JDOOM__
    if(gameMode != doom2_hacx && gameMode != doom_chex)
    {
        page->findWidget(Widget::Id4).as<ButtonWidget>().setNoAltText();
    }
#endif
}

#if 0
void Hu_MenuInitMultiplayerPage()
{
#if __JHERETIC__ || __JHEXEN__
    Vec2i const origin(97, 65);
#else
    Vec2i const origin(97, 65);
#endif

    Page *page = Hu_MenuAddPage(new Page("Multiplayer", origin, 0, Hu_MenuDrawMultiplayerPage));
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuPagePtr("GameType"));

    page->addWidget(new ButtonWidget("Join Game"))
            .setFlags(Widget::Id0)
            .setShortcut('j')
            .setFont(MENU_FONT1)
            .setAction(Widget::Deactivated, Hu_MenuSelectJoinGame)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Player Setup"))
            .setShortcut('p')
            .setFont(MENU_FONT1)
            .setUserValue(String("PlayerSetup"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
}
#endif

void Hu_MenuInitPlayerSetupPage()
{
#if __JHERETIC__ || __JHEXEN__
    Vec2i const origin(70, 34);
#else
    Vec2i const origin(70, 54);
#endif

    Page *page = Hu_MenuAddPage(new Page("PlayerSetup", origin, Page::NoScroll, Hu_MenuDrawPlayerSetupPage));
    page->setLeftColumnWidth(.5f);
    page->setOnActiveCallback(Hu_MenuActivatePlayerSetup);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPredefinedFont(MENU_FONT2, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new MobjPreviewWidget)
            .setFixedOrigin(Vec2i(SCREENWIDTH / 2 - 40, 60))
            .setFlags(Widget::Id0 | Widget::PositionFixed);

    page->addWidget(new CVarLineEditWidget("net-name"))
            .setMaxLength(24)
            .setFlags(Widget::Id1 | Widget::LayoutOffset)
            .setFixedY(75);

#if __JHEXEN__
    page->addWidget(new LabelWidget("Class"))
            .setLeft()
            .setFlags(Widget::LayoutOffset)
            .setFixedY(5);

    page->addWidget(new InlineListWidget)
            .addItems(ListWidget::Items() << new ListWidgetItem(GET_TXT(TXT_PLAYERCLASS1), PCLASS_FIGHTER)
                                          << new ListWidgetItem(GET_TXT(TXT_PLAYERCLASS2), PCLASS_CLERIC)
                                          << new ListWidgetItem(GET_TXT(TXT_PLAYERCLASS3), PCLASS_MAGE))
            .setFlags(Widget::Id2)
            .setShortcut('c')
            .setRight()
            .setColor(MENU_COLOR3)
            .setAction(Widget::Modified,    Hu_MenuSelectPlayerSetupPlayerClass)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
#endif

    auto &label = page->addWidget(new LabelWidget("Color"));
    label.setLeft();
#ifdef __JHERETIC__
    label.setFlags(Widget::LayoutOffset);
    label.setFixedY(5);
#else
    DE_UNUSED(label);
#endif

    // Setup the player color selection list.
    /// @todo Read these names from Text definitions.
    int colorIdx = 0;
    ListWidget::Items items;
#if __JHEXEN__
    items << new ListWidgetItem("Red",    colorIdx++);
    items << new ListWidgetItem("Blue",   colorIdx++);
    items << new ListWidgetItem("Yellow", colorIdx++);
    items << new ListWidgetItem("Green",  colorIdx++);
    if(gameMode != hexen_v10) // Hexen v1.0 has only four player colors.
    {
        items << new ListWidgetItem("Jade",   colorIdx++);
        items << new ListWidgetItem("White",  colorIdx++);
        items << new ListWidgetItem("Hazel",  colorIdx++);
        items << new ListWidgetItem("Purple", colorIdx++);
    }
#elif __JHERETIC__
    items << new ListWidgetItem("Green",  colorIdx++);
    items << new ListWidgetItem("Orange", colorIdx++);
    items << new ListWidgetItem("Red",    colorIdx++);
    items << new ListWidgetItem("Blue",   colorIdx++);
#else
    items << new ListWidgetItem("Green",  colorIdx++);
    items << new ListWidgetItem("Indigo", colorIdx++);
    items << new ListWidgetItem("Brown",  colorIdx++);
    items << new ListWidgetItem("Red",    colorIdx++);
#endif
    items << new ListWidgetItem("Automatic", colorIdx++);

    page->addWidget(new InlineListWidget)
            .addItems(items)
            .setFlags(Widget::Id3)
            .setColor(MENU_COLOR3)
            .setRight()
            .setAction(Widget::Modified,    Hu_MenuSelectPlayerColor)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Save Changes"))
            .setShortcut('s')
            .setAction(Widget::Deactivated, Hu_MenuSelectAcceptPlayerSetup)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
}

void Hu_MenuInitSaveOptionsPage()
{
    Page *page = Hu_MenuAddPage(new Page("SaveOptions", Vec2i(60, 50)));
    page->setTitle("Savegame Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new LabelWidget("Confirm quick load/save"))
            .setLeft();
    page->addWidget(new CVarToggleWidget("game-save-confirm"))
            .setRight()
            .setShortcut('q');

    page->addWidget(new LabelWidget("Confirm reborn load"))
            .setLeft();
    page->addWidget(new CVarToggleWidget("game-save-confirm-loadonreborn"))
            .setRight()
            .setShortcut('r');

    page->addWidget(new LabelWidget("Reborn preferences"))
            .setGroup(1)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Load last save"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-save-last-loadonreborn"))
            .setRight()
            .setGroup(1)
            .setShortcut('a');
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitFilesPage()
{
    Page *page = Hu_MenuAddPage(new Page("Files", Vec2i(110, 60), Page::FixedLayout | Page::NoScroll));
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuPagePtr("Main"));

    int y = 0;

    page->addWidget(new ButtonWidget("Load Game"))
            .setFixedY(y)
            .setShortcut('l')
            .setFont(MENU_FONT1)
            .setAction(Widget::Deactivated, Hu_MenuSelectLoadGame)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    y += FIXED_LINE_HEIGHT;

    page->addWidget(new ButtonWidget("Save Game"))
            .setFixedY(y)
            .setShortcut('s')
            .setFont(MENU_FONT1)
            .setAction(Widget::Deactivated, Hu_MenuSelectSaveGame)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
}
#endif

static void deleteGameSave(const String& slotId)
{
    DD_Executef(true, "deletegamesave %s", slotId.c_str());
}

int Hu_MenuLoadSlotCommandResponder(Widget &wi, menucommand_e cmd)
{
    LineEditWidget &edit = wi.as<LineEditWidget>();
    if(cmd == MCMD_DELETE && !wi.isDisabled() && wi.isFocused() && !wi.isActive())
    {
        deleteGameSave(edit.userValue().asText());
        return true;
    }
    if(cmd == MCMD_SELECT && !wi.isDisabled() && wi.isFocused())
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!wi.isActive())
        {
            wi.setFlags(Widget::Active);
            wi.execAction(Widget::Activated);
        }

        wi.setFlags(Widget::Active, UnsetFlags);
        wi.execAction(Widget::Deactivated);
        return true;
    }
    return false; // Not eaten.
}

int Hu_MenuSaveSlotCommandResponder(Widget &wi, menucommand_e cmd)
{
    LineEditWidget &edit = wi.as<LineEditWidget>();
    if(cmd == MCMD_DELETE && !wi.isDisabled() && wi.isFocused() && !wi.isActive())
    {
        deleteGameSave(edit.userValue().asText());
        return true;
    }
    return edit.handleCommand(cmd);
}

void Hu_MenuInitLoadGameAndSaveGamePages()
{
#if __JDOOM__ || __JDOOM64__
    Vec2i const origin(50, 54);
#else
    Vec2i const origin(40, 30);
#endif
    Widget::Flags const saveSlotObjectIds[NUMSAVESLOTS] = {
        Widget::Id0, Widget::Id1, Widget::Id2, Widget::Id3, Widget::Id4, Widget::Id5,
#if !__JHEXEN__
        Widget::Id6, Widget::Id7
#endif
    };

    Page *loadPage = Hu_MenuAddPage(new Page("LoadGame", origin, Page::FixedLayout | Page::NoScroll, Hu_MenuDrawLoadGamePage));
    loadPage->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    loadPage->setPreviousPage(Hu_MenuPagePtr("Main"));

    int y = 0;
    int i = 0;
    for(; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        loadPage->addWidget(new LineEditWidget)
                    .setMaxLength(24)
                    .setEmptyText(GET_TXT(TXT_EMPTYSTRING))
                    .setFixedY(y)
                    .setFlags(saveSlotObjectIds[i] | Widget::Disabled)
                    .setShortcut('0' + i)
                    .setCommandResponder(Hu_MenuLoadSlotCommandResponder)
                    .setUserValue(TextValue(String::asText(i)))
                    .setUserValue2(NumberValue(saveSlotObjectIds[i]))
                    .setAction(Widget::Deactivated, Hu_MenuSelectLoadSlot)
                    .setAction(Widget::FocusLost,   Hu_MenuDefaultFocusAction);
    }

    Page *savePage = Hu_MenuAddPage(new Page("SaveGame", origin, Page::FixedLayout | Page::NoScroll, Hu_MenuDrawSaveGamePage));
    savePage->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    savePage->setPreviousPage(Hu_MenuPagePtr("Main"));

    y = 0;
    i = 0;
    for(; i < NUMSAVESLOTS; ++i, y += FIXED_LINE_HEIGHT)
    {
        savePage->addWidget(new LineEditWidget)
                    .setMaxLength(24)
                    .setEmptyText(GET_TXT(TXT_EMPTYSTRING))
                    .setFixedY(y)
                    .setFlags(saveSlotObjectIds[i])
                    .setShortcut('0' + i)
                    .setCommandResponder(Hu_MenuSaveSlotCommandResponder)
                    .setUserValue(TextValue(String::asText(i)))
                    .setUserValue2(NumberValue(saveSlotObjectIds[i]))
                    .setAction(Widget::Deactivated, Hu_MenuSelectSaveSlot)
                    .setAction(Widget::Activated,   Hu_MenuSaveSlotEdit)
                    .setAction(Widget::FocusLost,   Hu_MenuDefaultFocusAction);
    }
}

void Hu_MenuInitOptionsPage()
{
#if __JHERETIC__ || __JHEXEN__
    Vec2i const origin(110, 45);
#else
    Vec2i const origin(110, 63);
#endif

    Page *page = Hu_MenuAddPage(new Page("Options", origin, Page::NoScroll, Hu_MenuDrawOptionsPage));
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Main"));

    page->addWidget(new ButtonWidget("End Game"))
            .setShortcut('e')
            .setFont(MENU_FONT1)
            .setGroup(1)
            .setAction(Widget::Deactivated, Hu_MenuSelectEndGame)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Player Setup"))
            .setShortcut('p')
            .setGroup(1)
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("PlayerSetup"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Show Taskbar"))
            .setShortcut('t')
            .setFont(MENU_FONT1)
            .setGroup(1)
            .setUserValue2(NumberValue(0))
            .setAction(Widget::Deactivated, Hu_MenuSelectControlPanelLink)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Sound"))
            .setShortcut('s')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("SoundOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Controls"))
            .setShortcut('c')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("ControlOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Gameplay"))
            .setShortcut('g')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("GameplayOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("HUD"))
            .setShortcut('h')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("HUDOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Automap"))
            .setShortcut('a')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("AutomapOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new ButtonWidget("Weapons"))
            .setShortcut('w')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("WeaponOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

#if __JHERETIC__ || __JHEXEN__
    page->addWidget(new ButtonWidget("Inventory"))
            .setShortcut('i')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("InventoryOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
#endif

    page->addWidget(new ButtonWidget("Savegame"))
            .setShortcut('s')
            .setFont(MENU_FONT1)
            .setUserValue(TextValue("SaveOptions"))
            .setAction(Widget::Deactivated, Hu_MenuActionSetActivePage)
            .setAction(Widget::FocusGained,     Hu_MenuDefaultFocusAction);
}

void Hu_MenuInitGameplayOptionsPage()
{
#if __JHEXEN__
    Vec2i const origin(88, 25);
#elif __JHERETIC__
    Vec2i const origin(30, 40);
#else
    Vec2i const origin(30, 40);
#endif

    Page *page = Hu_MenuAddPage(new Page("GameplayOptions", origin));
    page->setLeftColumnWidth(.75f);
    page->setTitle("Gameplay Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new LabelWidget("Always Run")).setLeft();
    page->addWidget(new CVarToggleWidget("ctl-run")).setRight()
            .setShortcut('r');

    page->addWidget(new LabelWidget("Use LookSpring")).setLeft();
    page->addWidget(new CVarToggleWidget("ctl-look-spring")).setRight()
            .setShortcut('l');

    page->addWidget(new LabelWidget("Disable AutoAim")).setLeft();
    page->addWidget(new CVarToggleWidget("ctl-aim-noauto")).setRight()
            .setShortcut('a');

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    page->addWidget(new LabelWidget("Allow Jumping")).setLeft();
    page->addWidget(new CVarToggleWidget("player-jump")).setRight()
            .setShortcut('j');
#endif

#if __JDOOM__
    page->addWidget(new LabelWidget("Fast Monsters")).setLeft();
    page->addWidget(new CVarToggleWidget("game-monsters-fast")).setRight()
            .setShortcut('f');
#endif

#if __JDOOM64__
    page->addWidget(new LabelWidget("Weapon Recoil")).setLeft();
    page->addWidget(new CVarToggleWidget("player-weapon-recoil")).setRight();
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    page->addWidget(new LabelWidget("Compatibility"))
            .setLeft()
            .setGroup(1)
            .setColor(MENU_COLOR2);

# if __JDOOM__ || __JDOOM64__
    page->addWidget(new LabelWidget("Any Boss Trigger 666")).setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-anybossdeath666")).setRight()
            .setGroup(1)
            .setShortcut('b');

#  if !__JDOOM64__
    page->addWidget(new LabelWidget("Av Resurrects Ghosts"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-raiseghosts"))
            .setRight()
            .setGroup(1)
            .setShortcut('g');
# if __JDOOM__
    page->addWidget(new LabelWidget("VileChase uses Av radius"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-vilechase-usevileradius"))
            .setRight()
            .setGroup(1)
            .setShortcut('g');
# endif
#  endif // !__JDOOM64__

    page->addWidget(new LabelWidget("PE Limited To 21 Lost Souls"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-maxskulls"))
            .setRight()
            .setGroup(1)
            .setShortcut('p');

    page->addWidget(new LabelWidget("LS Can Get Stuck Inside Walls"))
            .setLeft()
            .setGroup(1);

    page->addWidget(new CVarToggleWidget("game-skullsinwalls"))
            .setRight()
            .setGroup(1);
# endif // __JDOOM__ || __JDOOM64__

    page->addWidget(new LabelWidget("Monsters Fly Over Obstacles"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-monsters-floatoverblocking"))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Monsters Can Get Stuck\n   In Doors"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-monsters-stuckindoors"))
            .setRight()
            .setGroup(1)
            .setShortcut('d');

    page->addWidget(new LabelWidget("Some Objects Never Hang\n   Over Ledges"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-objects-neverhangoverledges"))
            .setRight()
            .setGroup(1)
            .setShortcut('h');

    page->addWidget(new LabelWidget("Objects Fall Under Own Weight"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-objects-falloff"))
            .setRight()
            .setGroup(1)
            .setShortcut('f');

#if __JDOOM__ || __JDOOM64__
    page->addWidget(new LabelWidget("All Crushed Objects\n   Become A Pile Of Gibs"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-objects-gibcrushednonbleeders"))
            .setRight()
            .setGroup(1)
            .setShortcut('g');
#endif

    page->addWidget(new LabelWidget("Corpses Slide Down Stairs"))
            .setLeft()
            .setGroup(1);

    page->addWidget(new CVarToggleWidget("game-corpse-sliding"))
            .setRight()
            .setGroup(1)
            .setShortcut('s');

    page->addWidget(new LabelWidget("Use Doom's Clipping\n   Code Exactly"))
            .setLeft()
            .setGroup(1);

    page->addWidget(new CVarToggleWidget("game-objects-clipping"))
            .setRight()
            .setGroup(1)
            .setShortcut('c');

    page->addWidget(new LabelWidget("  ^If Not NorthOnly WallRunning"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-player-wallrun-northonly"))
            .setRight()
            .setGroup(1)
            .setShortcut('w');

    page->addWidget(new LabelWidget("Pushable Speed Limit"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-objects-pushable-limit"))
            .setRight()
            .setGroup(1)
            .setShortcut('p');

# if __JDOOM__ || __JDOOM64__

    page->addWidget(new LabelWidget("Zombie Players Can\n   Exit Maps")).setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("game-zombiescanexit")).setRight()
            .setGroup(1)
            .setShortcut('e');

    page->addWidget(new LabelWidget("Fix Ouch Face")).setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-face-ouchfix")).setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Fix Weapon Slot Display")).setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-status-weaponslots-ownedfix")).setRight()
            .setGroup(1);

# endif // __JDOOM__ || __JDOOM64__
#endif // __JDOOM__ || __JHERETIC__ || __JDOOM64__

#if __JHERETIC__
    page->addWidget(new LabelWidget("Powered Staff Damages Ghosts"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("player-weapon-staff-powerghostdamage"))
            .setRight()
            .setGroup(1)
            .setShortcut('g');
#endif

    page->addWidget(new LabelWidget("Vanilla Switch Sound\n   Positioning"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("sound-switch-origin"))
            .setRight()
            .setGroup(1)
            .setShortcut('v');
}

void Hu_MenuInitHUDOptionsPage()
{
#if __JDOOM__ || __JDOOM64__
    Vec2i const origin(97, 40);
#else
    Vec2i const origin(97, 28);
#endif

    Page *page = Hu_MenuAddPage(new Page("HudOptions", origin));
    page->setTitle("HUD Options");
    page->setLeftColumnWidth(.45f);
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new LabelWidget("View Size")).setLeft();

    page->addWidget(new CVarSliderWidget("view-size"))
#if __JDOOM64__
            .setRange(3, 11, 1)
#else
            .setRange(3, 13, 1)
#endif
            .setFloatMode(false)
            .setRight();

    page->addWidget(new LabelWidget("Messages"))
            .setGroup(2)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Shown"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarToggleWidget("msg-show"))
            .setRight()
            .setGroup(2)
            .setShortcut('m');

    page->addWidget(new LabelWidget("Uptime"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarTextualSliderWidget("msg-uptime", 0, 60, 1))
            .setEmptyText("Disabled")
            .setOnethSuffix(" second")
            .setNthSuffix(" seconds")
            .setRight()
            .setGroup(2);

    page->addWidget(new LabelWidget("Size"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarSliderWidget("msg-scale"))
            .setRight()
            .setGroup(2);

    page->addWidget(new LabelWidget("Color"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarColorEditWidget("msg-color-r", "msg-color-g", "msg-color-b"))
            .setRight()
            .setGroup(2)
            .setAction(Widget::Deactivated, CVarColorEditWidget_UpdateCVar)
            .setAction(Widget::Activated,   Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("Crosshair"))
            .setGroup(3)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Symbol"))
            .setGroup(3)
            .setLeft()
            .setShortcut('c');
    page->addWidget(new CVarInlineListWidget("view-cross-type"))
            .addItems(ListWidget::Items() << new ListWidgetItem("None",        0)
                                          << new ListWidgetItem("Cross",       1)
                                          << new ListWidgetItem("Twin Angles", 2)
                                          << new ListWidgetItem("Square",      3)
                                          << new ListWidgetItem("Open Square", 4)
                                          << new ListWidgetItem("Angle",       5))
            .setGroup(3)
            .setRight();

    page->addWidget(new LabelWidget("Size"))
            .setLeft()
            .setGroup(3);
    page->addWidget(new CVarSliderWidget("view-cross-size"))
            .setRight()
            .setGroup(3);

    page->addWidget(new LabelWidget("Thickness"))
            .setLeft()
            .setGroup(3);
    page->addWidget(new CVarSliderWidget("view-cross-width", .5f, 5, .5f))
            .setRight()
            .setGroup(3);

    page->addWidget(new LabelWidget("Angle"))
            .setLeft()
            .setGroup(3);
    page->addWidget(new CVarSliderWidget("view-cross-angle", 0.0f, 1.0f, 0.0625f))
            .setRight()
            .setGroup(3);

    page->addWidget(new LabelWidget("Opacity"))
            .setLeft()
            .setGroup(3);
    page->addWidget(new CVarSliderWidget("view-cross-a"))
            .setRight()
            .setGroup(3);

    page->addWidget(new LabelWidget("Color"))
            .setLeft()
            .setGroup(3);
    page->addWidget(new CVarColorEditWidget("view-cross-r", "view-cross-g", "view-cross-b"))
            .setRight()
            .setGroup(3)
            .setAction(Widget::Deactivated, CVarColorEditWidget_UpdateCVar)
            .setAction(Widget::Activated,   Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("Vitality Color"))
            .setLeft()
            .setGroup(3);
    page->addWidget(new CVarToggleWidget("view-cross-vitality"))
            .setRight()
            .setGroup(3);

    page->addWidget(new LabelWidget("   When Dead"))
            .setLeft()
            .setGroup(3);
    page->addWidget(new CVarColorEditWidget("view-cross-dead-r", "view-cross-dead-g", "view-cross-dead-b"))
            .setRight()
            .setGroup(3)
            .setAction(Widget::Deactivated,     CVarColorEditWidget_UpdateCVar)
            .setAction(Widget::Activated,       Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("   Full Health"))
            .setLeft()
            .setGroup(3);

    page->addWidget(new CVarColorEditWidget("view-cross-live-r", "view-cross-live-g", "view-cross-live-b"))
            .setRight()
            .setGroup(3)
            .setAction(Widget::Deactivated,     CVarColorEditWidget_UpdateCVar)
            .setAction(Widget::Activated,       Hu_MenuActivateColorWidget);

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    page->addWidget(new LabelWidget("Statusbar"))
            .setGroup(4)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Size"))
            .setLeft()
            .setGroup(4);
    page->addWidget(new CVarSliderWidget("hud-status-size"))
            .setRight()
            .setGroup(4);

    page->addWidget(new LabelWidget("Opacity"))
            .setLeft()
            .setGroup(4);
    page->addWidget(new CVarSliderWidget("hud-status-alpha"))
            .setRight()
            .setGroup(4);

#if __JDOOM__
    page->addWidget(new LabelWidget("Single Key Display")).setLeft().setGroup(4);
    page->addWidget(new CVarToggleWidget("hud-keys-combine")).setRight().setGroup(4);
#endif

    page->addWidget(new LabelWidget("AutoHide Status"))
            .setLeft()
            .setGroup(4);
    page->addWidget(new CVarTextualSliderWidget("hud-timer", 0, 60, 1))
            .setEmptyText("Disabled")
            .setOnethSuffix(" second")
            .setNthSuffix(" seconds")
            .setRight()
            .setGroup(4);

    page->addWidget(new LabelWidget("Status UnHide Events"))
            .setGroup(1)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Receive Damage"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-unhide-damage"))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Pickup Health"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-unhide-pickup-health"))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Pickup Armor"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-unhide-pickup-armor"))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Pickup Powerup"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-unhide-pickup-powerup"))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Pickup Weapon"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-unhide-pickup-weapon"))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget)
#if __JHEXEN__
            .setText("Pickup Mana")
#else
            .setText("Pickup Ammo")
#endif
            .setGroup(1)
            .setLeft();
    page->addWidget(new CVarToggleWidget("hud-unhide-pickup-ammo"))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Pickup Key"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("hud-unhide-pickup-key"))
            .setRight()
            .setGroup(1);

#if __JHERETIC__ || __JHEXEN__
    page->addWidget(new LabelWidget("Pickup Item"))
            .setLeft()
            .setGroup(1);

    page->addWidget(new CVarToggleWidget("hud-unhide-pickup-invitem"))
            .setRight()
            .setGroup(1);
#endif // __JHERETIC__ || __JHEXEN__

#endif // __JDOOM__ || __JHERETIC__ || __JHEXEN__

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    page->addWidget(new LabelWidget("Counters"))
            .setGroup(5)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Items"))
            .setLeft()
            .setGroup(5);
    page->addWidget(new CVarInlineListWidget("hud-cheat-counter", CCH_ITEMS | CCH_ITEMS_PRCNT))
            .addItems(ListWidget::Items() << new ListWidgetItem("Hidden",        0)
                                          << new ListWidgetItem("Count",         CCH_ITEMS)
                                          << new ListWidgetItem("Percent",       CCH_ITEMS_PRCNT)
                                          << new ListWidgetItem("Count+Percent", CCH_ITEMS | CCH_ITEMS_PRCNT))
            .setRight()
            .setGroup(5)
            .setShortcut('i');

    page->addWidget(new LabelWidget("Kills"))
            .setLeft()
            .setGroup(5);

    page->addWidget(new CVarInlineListWidget("hud-cheat-counter", CCH_KILLS | CCH_KILLS_PRCNT))
            .addItems(ListWidget::Items() << new ListWidgetItem("Hidden",        0)
                                          << new ListWidgetItem("Count",         CCH_KILLS)
                                          << new ListWidgetItem("Percent",       CCH_KILLS_PRCNT)
                                          << new ListWidgetItem("Count+Percent", CCH_KILLS | CCH_KILLS_PRCNT))
            .setRight()
            .setGroup(5)
            .setShortcut('k');

    page->addWidget(new LabelWidget("Secrets"))
            .setLeft()
            .setGroup(5);
    page->addWidget(new CVarInlineListWidget("hud-cheat-counter", CCH_SECRETS | CCH_SECRETS_PRCNT))
            .addItems(ListWidget::Items() << new ListWidgetItem("Hidden",        0)
                                          << new ListWidgetItem("Count",         CCH_SECRETS)
                                          << new ListWidgetItem("Percent",       CCH_SECRETS_PRCNT)
                                          << new ListWidgetItem("Count+Percent", CCH_SECRETS | CCH_SECRETS_PRCNT))
            .setGroup(5)
            .setRight()
            .setShortcut('s');

    page->addWidget(new LabelWidget("Automap Only"))
            .setLeft()
            .setGroup(5);
    page->addWidget(new CVarToggleWidget("hud-cheat-counter-show-mapopen"))
            .setRight()
            .setGroup(5);

    page->addWidget(new LabelWidget("Size"))
            .setLeft()
            .setGroup(5);
    page->addWidget(new CVarSliderWidget("hud-cheat-counter-scale"))
            .setRight()
            .setGroup(5);

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

    page->addWidget(new LabelWidget("Fullscreen"))
            .setGroup(6)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Size"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarSliderWidget("hud-scale"))
            .setRight()
            .setGroup(6);

    page->addWidget(new LabelWidget("Text Color"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarColorEditWidget("hud-color-r", "hud-color-g", "hud-color-b", "hud-color-a", {}, true))
            .setRight()
            .setGroup(6)
            .setAction(Widget::Deactivated, CVarColorEditWidget_UpdateCVar)
            .setAction(Widget::Activated,   Hu_MenuActivateColorWidget);

#if __JHEXEN__

    page->addWidget(new LabelWidget("Show Mana"))
            .setLeft()
            .setGroup(6);

    page->addWidget(new CVarToggleWidget("hud-mana"))
            .setRight()
            .setGroup(6);

#endif // __JHEXEN__

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__

    page->addWidget(new LabelWidget("Show Ammo"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarToggleWidget("hud-ammo"))
            .setRight()
            .setGroup(6)
            .setShortcut('a');

    page->addWidget(new LabelWidget("Show Armor"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarToggleWidget("hud-armor"))
            .setRight()
            .setGroup(6)
            .setShortcut('r');

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

#if __JDOOM64__

    page->addWidget(new LabelWidget("Show PowerKeys"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarToggleWidget("hud-power"))
            .setRight()
            .setGroup(6)
            .setShortcut('p');

#endif // __JDOOM64__

#if __JDOOM__

    page->addWidget(new LabelWidget("Show Status"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarToggleWidget("hud-face"))
            .setRight()
            .setGroup(6)
            .setShortcut('f');

#endif // __JDOOM__

    page->addWidget(new LabelWidget("Show Health"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarToggleWidget("hud-health"))
            .setRight()
            .setGroup(6)
            .setShortcut('h');

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__

    page->addWidget(new LabelWidget("Show Keys"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarToggleWidget("hud-keys"))
            .setRight()
            .setGroup(6);

#endif // __JDOOM__ || __JDOOM64__ || __JHERETIC__

#if __JHERETIC__ || __JHEXEN__

    page->addWidget(new LabelWidget("Show Ready-Item"))
            .setLeft()
            .setGroup(6);
    page->addWidget(new CVarToggleWidget("hud-currentitem"))
            .setRight()
            .setGroup(6);

#endif // __JHERETIC__ || __JHEXEN__
}

void Hu_MenuInitAutomapOptionsPage()
{
#if __JHERETIC__ || __JHEXEN__
    const Vec2i origin(32, 28);
#else
    const Vec2i origin(70, 40);
#endif

    Page *page = Hu_MenuAddPage(new Page("AutomapOptions", origin));
    page->setLeftColumnWidth(.55f);
    page->setTitle("Automap Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new LabelWidget("Rotation"))
            .setLeft();
    {
        auto *tgl = new CVarToggleWidget("map-rotate");
        tgl->setRight();
        tgl->setShortcut('r');
        tgl->setStateChangeCallback([](CVarToggleWidget::State state) {
            G_SetAutomapRotateMode(state == CVarToggleWidget::Down);
        });
        page->addWidget(tgl);
    }

    page->addWidget(new LabelWidget("Always Update Map"))
            .setLeft();
    page->addWidget(new CVarToggleWidget("map-neverobscure"))
            .setRight()
            .setShortcut('a')
            .setHelpInfo("Update map even when background is opaque");

#if !defined (__JDOOM64__)
    page->addWidget(new LabelWidget("HUD Display"))
            .setLeft();
    page->addWidget(new CVarInlineListWidget("map-huddisplay"))
            .addItems(ListWidget::Items() << new ListWidgetItem("None",      0)
                                          << new ListWidgetItem("Current",   1)
                                          << new ListWidgetItem("Statusbar", 2))
            .setRight()
            .setShortcut('h');
#endif

    page->addWidget(new LabelWidget("Appearance"))
            .setGroup(1)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Background Opacity"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarSliderWidget("map-opacity"))
            .setShortcut('o')
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Line Opacity"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarSliderWidget("map-line-opacity"))
            .setShortcut('l')
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Line Width"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarSliderWidget("map-line-width", 0.5f, 8.f))
            .setRight()
            .setGroup(1);

    page->addWidget(new LabelWidget("Colored Doors"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("map-door-colors"))
            .setRight()
            .setShortcut('d')
            .setGroup(1);

    page->addWidget(new LabelWidget("Door Glow"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarSliderWidget("map-door-glow", 0, 200, 5))
            .setRight()
            .setShortcut('g')
            .setGroup(1);

    page->addWidget(new LabelWidget("Use Custom Colors"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarInlineListWidget("map-customcolors"))
            .addItems(ListWidget::Items() << new ListWidgetItem("Never",  0)
                                          << new ListWidgetItem("Auto",   1)
                                          << new ListWidgetItem("Always", 2))
            .setRight()
            .setGroup(2);

    page->addWidget(new LabelWidget("Wall"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarColorEditWidget("map-wall-r", "map-wall-g", "map-wall-b"))
            .setRight()
            .setShortcut('w')
            .setGroup(2)
            .setAction(Widget::Activated, Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("Floor Height Change"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarColorEditWidget("map-wall-floorchange-r", "map-wall-floorchange-g", "map-wall-floorchange-b"))
            .setRight()
            .setShortcut('f')
            .setGroup(2)
            .setAction(Widget::Activated, Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("Ceiling Height Change"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarColorEditWidget("map-wall-ceilingchange-r", "map-wall-ceilingchange-g", "map-wall-ceilingchange-b"))
            .setRight()
            .setGroup(2)
            .setAction(Widget::Activated, Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("Unseen"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarColorEditWidget("map-wall-unseen-r", "map-wall-unseen-g", "map-wall-unseen-b"))
            .setRight()
            .setGroup(2)
            .setShortcut('u')
            .setAction(Widget::Activated, Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("Thing"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarColorEditWidget("map-mobj-r", "map-mobj-g", "map-mobj-b"))
            .setRight()
            .setGroup(2)
            .setShortcut('t')
            .setAction(Widget::Activated, Hu_MenuActivateColorWidget);

    page->addWidget(new LabelWidget("Background"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarColorEditWidget("map-background-r", "map-background-g", "map-background-b"))
            .setRight()
            .setGroup(2)
            .setShortcut('b')
            .setAction(Widget::Activated, Hu_MenuActivateColorWidget);
}

static bool compareWeaponPriority(const ListWidgetItem *a, const ListWidgetItem *b)
{
    int i = 0, aIndex = -1, bIndex = -1;
    do
    {
        if (cfg.common.weaponOrder[i] == a->userValue())
        {
            aIndex = i;
        }
        if (cfg.common.weaponOrder[i] == b->userValue())
        {
            bIndex = i;
        }
    } while(!(aIndex != -1 && bIndex != -1) && ++i < NUM_WEAPON_TYPES);

    return aIndex < bIndex;
}

void Hu_MenuInitWeaponsPage()
{
#if __JDOOM__ || __JDOOM64__
    Vec2i const origin(78, 40);
#elif __JHERETIC__
    Vec2i const origin(78, 26);
#elif __JHEXEN__
    Vec2i const origin(78, 38);
#endif

    const struct {
        const char *text;
        weapontype_t data;
    } weaponOrder[NUM_WEAPON_TYPES+1] = {
#if __JDOOM__ || __JDOOM64__
        { (const char *)TXT_WEAPON1,             WT_FIRST },
        { (const char *)TXT_WEAPON2,             WT_SECOND },
        { (const char *)TXT_WEAPON3,             WT_THIRD },
        { (const char *)TXT_WEAPON4,             WT_FOURTH },
        { (const char *)TXT_WEAPON5,             WT_FIFTH },
        { (const char *)TXT_WEAPON6,             WT_SIXTH },
        { (const char *)TXT_WEAPON7,             WT_SEVENTH },
        { (const char *)TXT_WEAPON8,             WT_EIGHTH },
        { (const char *)TXT_WEAPON9,             WT_NINETH },
#  if __JDOOM64__
        { (const char *)TXT_WEAPON10,            WT_TENTH },
#  endif
#elif __JHERETIC__
        { (const char *)TXT_TXT_WPNSTAFF,        WT_FIRST },
        { (const char *)TXT_TXT_WPNWAND,         WT_SECOND },
        { (const char *)TXT_TXT_WPNCROSSBOW,     WT_THIRD },
        { (const char *)TXT_TXT_WPNBLASTER,      WT_FOURTH },
        { (const char *)TXT_TXT_WPNSKULLROD,     WT_FIFTH },
        { (const char *)TXT_TXT_WPNPHOENIXROD,   WT_SIXTH },
        { (const char *)TXT_TXT_WPNMACE,         WT_SEVENTH },
        { (const char *)TXT_TXT_WPNGAUNTLETS,    WT_EIGHTH },
#elif __JHEXEN__
        /// @todo We should allow different weapon preferences per player-class.
        { "First",  WT_FIRST },
        { "Second", WT_SECOND },
        { "Third",  WT_THIRD },
        { "Fourth", WT_FOURTH },
#endif
        { "", WT_NOCHANGE}
    };

    Page *page = Hu_MenuAddPage(new Page("WeaponOptions", origin));
    page->setLeftColumnWidth(.5f);
    page->setTitle("Weapons Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new LabelWidget("Priority Order"))
            .setColor(MENU_COLOR2);

    ListWidget::Items weapItems;
    for (int i = 0; weaponOrder[i].data < NUM_WEAPON_TYPES; ++i)
    {
        const char *itemText = weaponOrder[i].text;
        if(itemText && (PTR2INT(itemText) > 0 && PTR2INT(itemText) < NUMTEXT))
        {
            itemText = GET_TXT(PTR2INT(itemText));
        }
        weapItems << new ListWidgetItem(itemText, weaponOrder[i].data);
    }
    std::sort(weapItems.begin(), weapItems.end(), compareWeaponPriority);
    page->addWidget(new ListWidget)
            .addItems(weapItems)
            .setReorderingEnabled(true)
            .setHelpInfo("Use left/right to move weapon up/down")
            .setShortcut('p')
            .setColor(MENU_COLOR3)
            .setAction(Widget::Modified,    Hu_MenuChangeWeaponPriority)
            .setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

    page->addWidget(new LabelWidget("Cycling"))
            .setGroup(1)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Use Priority Order"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("player-weapon-nextmode"))
            .setRight()
            .setGroup(1)
            .setShortcut('o');

    page->addWidget(new LabelWidget("Sequential"))
            .setLeft()
            .setGroup(1);
    page->addWidget(new CVarToggleWidget("player-weapon-cycle-sequential"))
            .setRight()
            .setGroup(1)
            .setShortcut('s');

    page->addWidget(new LabelWidget("Autoswitch"))
            .setGroup(2)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Pickup Weapon"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarInlineListWidget("player-autoswitch"))
            .addItems(ListWidget::Items() << new ListWidgetItem("Never",     0)
                                          << new ListWidgetItem("If Better", 1)
                                          << new ListWidgetItem("Always",    2))
            .setGroup(2)
            .setRight()
            .setShortcut('w');

    page->addWidget(new LabelWidget("   If Not Firing"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarToggleWidget("player-autoswitch-notfiring"))
            .setRight()
            .setGroup(2)
            .setShortcut('f');

    page->addWidget(new LabelWidget("Pickup Ammo"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarInlineListWidget("player-autoswitch-ammo"))
            .addItems(ListWidget::Items() << new ListWidgetItem("Never",     0)
                                          << new ListWidgetItem("If Better", 1)
                                          << new ListWidgetItem("Always",    2))
            .setGroup(2)
            .setRight()
            .setShortcut('a');

#if __JDOOM__ || __JDOOM64__

    page->addWidget(new LabelWidget("Pickup Beserk"))
            .setLeft()
            .setGroup(2);
    page->addWidget(new CVarToggleWidget("player-autoswitch-berserk"))
            .setRight()
            .setGroup(2)
            .setShortcut('b');

#endif
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuInitInventoryOptionsPage()
{
    Page *page = Hu_MenuAddPage(new Page("InventoryOptions", Vec2i(78, 48)));
    page->setLeftColumnWidth(.65f);
    page->setTitle("Inventory Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new LabelWidget("Select Mode"))
            .setLeft();
    page->addWidget(new CVarToggleWidget("ctl-inventory-mode", 0, "Scroll", "Cursor"))
            .setRight()
            .setShortcut('s');

    page->addWidget(new LabelWidget("Wrap Around"))
            .setLeft();
    page->addWidget(new CVarToggleWidget("ctl-inventory-wrap"))
            .setRight()
            .setShortcut('w');

    page->addWidget(new LabelWidget("Choose And Use"))
            .setLeft();
    page->addWidget(new CVarToggleWidget("ctl-inventory-use-immediate"))
            .setRight()
            .setShortcut('c');

    page->addWidget(new LabelWidget("Select Next If Use Failed"))
            .setLeft();
    page->addWidget(new CVarToggleWidget("ctl-inventory-use-next"))
            .setRight()
            .setShortcut('n');

    page->addWidget(new LabelWidget("AutoHide"))
            .setLeft();
    page->addWidget(new CVarTextualSliderWidget("hud-inventory-timer", 0, 30, 1.f))
            .setEmptyText("Disabled")
            .setOnethSuffix(" second")
            .setNthSuffix(" seconds")
            .setShortcut('h')
            .setRight();

    page->addWidget(new LabelWidget("Fullscreen HUD"))
            .setGroup(1)
            .setColor(MENU_COLOR2);

    page->addWidget(new LabelWidget("Max Visible Slots"))
            .setLeft()
            .setGroup(1);

    page->addWidget(new CVarTextualSliderWidget("hud-inventory-slot-max", 0, 16, 1, false))
            .setEmptyText("Automatic")
            .setRight()
            .setGroup(1)
            .setShortcut('v');

    page->addWidget(new LabelWidget("Show Empty Slots"))
            .setGroup(1)
            .setLeft();

    page->addWidget(new CVarToggleWidget("hud-inventory-slot-showempty"))
            .setGroup(1)
            .setRight()
            .setShortcut('e');
}
#endif

void Hu_MenuInitSoundOptionsPage()
{
//#if __JHEXEN__
//    Vector2i const origin(97, 25);
//#elif __JHERETIC__
//    Vector2i const origin(97, 30);
//#elif __JDOOM__ || __JDOOM64__
    Vec2i const origin(97, 40);
//#endif

    Page *page = Hu_MenuAddPage(new Page("SoundOptions", origin));
    page->setLeftColumnWidth(.4f);
    page->setTitle("Sound Options");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    page->addWidget(new LabelWidget("SFX Volume"))
            .setLeft();
    page->addWidget(new CVarSliderWidget("sound-volume", 0, 255, 16, false))
            .setRight()
            .setShortcut('s');

    page->addWidget(new LabelWidget("Music Volume"))
            .setLeft();
    page->addWidget(new CVarSliderWidget("music-volume", 0, 255, 16, false))
        .setRight()
        .setShortcut('m');
}

/**
 * Construct the episode selection menu.
 */
void Hu_MenuInitEpisodePage()
{
#if __JHEXEN__
    Vec2i const origin(120, 44);
#elif __JHERETIC__
    Vec2i const origin(80, 50);
#else
    Vec2i const origin(48, 63);
#endif

    Page *page =
        Hu_MenuAddPage(new Page("Episode", origin, Page::FixedLayout, Hu_MenuDrawEpisodePage));

    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuPagePtr("Main"));
    page->setOnActiveCallback([](Page &page) {
        const auto &items = page.children();
        if (items.size() == 1)
        {
            // If there is only one episode, select it automatically.
            auto &ep = items.front()->as<ButtonWidget>();
            ep.setSilent(true);
            ep.handleCommand(MCMD_SELECT);
            ep.setSilent(false);
        }
    });

    const DictionaryValue::Elements &episodesById = Defs().episodes.lookup("id").elements();
    if (!episodesById.size())
    {
        LOG_WARNING(
            "No episodes are defined. It will not be possible to start a new game from the menu");
        return;
    }

    int y = 0;
    int n = 0;
    for (const auto &pair : episodesById)
    {
        const Record &episodeDef   = *pair.second->as<RecordValue>().record();
        const String  episodeId    = episodeDef.gets("id");
        const String  episodeTitle = G_EpisodeTitle(episodeId);

        if (episodeTitle.empty())
        {
            // Hidden/untitled episode.
            continue;
        }

        auto *btn = new ButtonWidget(episodeTitle);
        btn->setFixedY(y);

        // Has a menu image been specified?
        res::Uri image(episodeDef.gets("menuImage"), RC_NULL);
        if (!image.path().isEmpty())
        {
            // Presently only patches are supported.
            if (!image.scheme().compareWithoutCase("Patches"))
            {
                btn->setPatch(R_DeclarePatch(image.path()));
            }
        }

        // Has a menu shortcut/hotkey been specified?
        /// @todo Validate symbolic dday key names.
        const String shortcut = episodeDef.gets("menuShortcut");
        if(!shortcut.isEmpty() && shortcut.first().isAlphaNumeric())
        {
            btn->setShortcut(shortcut.first().lower());
        }

        // Has a menu help/info text been specified?
        const String helpInfo = episodeDef.gets("menuHelpInfo");
        if (!helpInfo.isEmpty())
        {
            btn->setHelpInfo(helpInfo);
        }

        res::Uri startMap(episodeDef.gets("startMap"), RC_NULL);
        if(P_MapExists(startMap.compose()))
        {
            btn->setAction(Widget::Deactivated, Hu_MenuSelectEpisode);
            btn->setUserValue(TextValue(episodeId));
        }
        else
        {
#if __JDOOM__ || __JHERETIC__
            // In shareware display a prompt to buy the full game.
            if (
#    if __JHERETIC__
                gameMode == heretic_shareware
#    else // __JDOOM__
                gameMode == doom_shareware
#endif
               && startMap.path().toString() != "E1M1")
            {
                btn->setAction(Widget::Deactivated, Hu_MenuActivateNotSharewareEpisode);
            }
            else
#endif
            {
                // Disable this selection and log a warning for the mod author.
                btn->setFlags(Widget::Disabled);
                LOG_RES_WARNING("Failed to locate the starting map \"%s\" for episode '%s'."
                                " This episode will not be selectable from the menu")
                    << startMap << episodeId;
            }
        }

        btn->setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);
        btn->setFont(MENU_FONT1);
        page->addWidget(btn);

        y += FIXED_LINE_HEIGHT;
        n += 1;
    }
}

#if __JHEXEN__
/**
 * Construct the player class selection menu.
 */
void Hu_MenuInitPlayerClassPage()
{
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

    Page *page = Hu_MenuAddPage(new Page("PlayerClass", Vec2i(66, 66), Page::FixedLayout | Page::NoScroll,
                                         Hu_MenuDrawPlayerClassPage, Hu_MenuSkipPreviousPageIfSkippingEpisodeSelection));
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTB));
    page->setPreviousPage(Hu_MenuPagePtr("Episode"));

    uint y = 0;

    // Add the selectable classes.
    int n = 0;
    while(n < count)
    {
        classinfo_t *info = PCLASS_INFO(n++);

        if(!info->userSelectable) continue;

        String text;
        if(info->niceName && (PTR2INT(info->niceName) > 0 && PTR2INT(info->niceName) < NUMTEXT))
        {
            text = String(GET_TXT(PTR2INT(info->niceName)));
        }
        else
        {
            text = String(info->niceName);
        }

        auto *btn = new ButtonWidget(text);

        if (!btn->text().isEmpty() && btn->text().first().isAlphaNumeric()) btn->setShortcut(btn->text().first());
        btn->setFixedY(y);
        btn->setAction(Widget::Deactivated, Hu_MenuSelectPlayerClass);
        btn->setAction(Widget::FocusGained, Hu_MenuFocusOnPlayerClass);
        btn->setUserValue2(NumberValue(info->plrClass));
        btn->setFont(MENU_FONT1);

        page->addWidget(btn);
        y += FIXED_LINE_HEIGHT;
    }

    // Random class button.
    const String labelText = GET_TXT(TXT_RANDOMPLAYERCLASS);
    const int shortcut     = labelText.first().isAlphaNumeric() ? int(labelText.first()) : 0;
    page->addWidget(new ButtonWidget(labelText))
            .setFixedY(y)
            .setShortcut(shortcut)
            .setUserValue2(NumberValue(PCLASS_NONE))
            .setFont(MENU_FONT1)
            .setColor(MENU_COLOR1)
            .setAction(Widget::Deactivated, Hu_MenuSelectPlayerClass)
            .setAction(Widget::FocusGained, Hu_MenuFocusOnPlayerClass);

    // Mobj preview background.
    page->addWidget(new RectWidget)
            .setFlags(Widget::NoFocus | Widget::Id1)
            .setFixedOrigin(Vec2i(108, -58))
            .setOnTickCallback(Hu_MenuPlayerClassBackgroundTicker);

    // Mobj preview.
    page->addWidget(new MobjPreviewWidget)
            .setFlags(Widget::Id0)
            .setFixedOrigin(Vec2i(108 + 55, -58 + 76))
            .setOnTickCallback(Hu_MenuPlayerClassPreviewTicker);
}
#endif

Page *Hu_MenuAddPage(Page *page)
{
    if(!page) return page;

    // Have we already added this page?
    for (auto &other : pages)
    {
        if (other.second == page) return page;
    }

    // Is the name valid?
    String nameInIndex = page->name().lower();
    if(nameInIndex.isEmpty())
    {
        throw Error("Hu_MenuPage", "A page must have a valid (i.e., not empty) name");
    }

    // Is the name unique?
    if(pages.contains(nameInIndex))
    {
        throw Error("Hu_MenuPage", "A page with the name '" + page->name() + "' is already present");
    }

    pages.insert(nameInIndex, page);
    return page;
}

/// @note Called during (post-engine) init and after updating game/engine state.
void Hu_MenuInit()
{
    // Close the menu (if open) and shutdown (if initialized - we're reinitializing).
    Hu_MenuShutdown();

    mnAlpha = mnTargetAlpha = 0;
    currentPage = 0;
    menuActive  = false;

    cursor.hasRotation = false;
    cursor.angle       = 0;
    cursor.animFrame   = 0;
    cursor.animCounter = MENU_CURSOR_TICSPERFRAME;

    DD_Execute(true, "deactivatebcontext menu");

    Hu_MenuLoadResources();

    initAllPages();

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        Page &mainPage = Hu_MenuPage("Main");

        Widget &wiReadThis = mainPage.findWidget(Widget::Id0);
        wiReadThis.setFlags(Widget::Disabled | Widget::Hidden | Widget::NoFocus);

        Widget &wiQuitGame = mainPage.findWidget(Widget::Id1);
        wiQuitGame.setFixedY(wiQuitGame.fixedY() - FIXED_LINE_HEIGHT);
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

bool Hu_MenuIsActive()
{
    return menuActive;
}

void Hu_MenuSetOpacity(float alpha)
{
    // The menu's alpha will start moving towards this target value.
    mnTargetAlpha = alpha;
}

float Hu_MenuOpacity()
{
    return mnAlpha;
}

void Hu_MenuTicker(timespan_t ticLength)
{
#define MENUALPHA_FADE_STEP (.07f)

    // Move towards the target alpha level for the entire menu.
    float diff = mnTargetAlpha - mnAlpha;
    if(fabs(diff) > MENUALPHA_FADE_STEP)
    {
        mnAlpha += float( MENUALPHA_FADE_STEP * ticLength * TICRATE * (diff > 0? 1 : -1) );
    }
    else
    {
        mnAlpha = mnTargetAlpha;
    }

    if(!menuActive) return;

    // Animate cursor rotation?
    if(cfg.common.menuCursorRotate)
    {
        if(cursor.hasRotation)
        {
            cursor.angle += float( 5 * ticLength * TICRATE );
        }
        else if (!fequal(cursor.angle, 0))
        {
            float rewind = float( MENU_CURSOR_REWIND_SPEED * ticLength * TICRATE );
            if(cursor.angle <= rewind || cursor.angle >= 360 - rewind)
                cursor.angle = 0;
            else if(cursor.angle < 180)
                cursor.angle -= rewind;
            else
                cursor.angle += rewind;
        }

        if(cursor.angle >= 360)
            cursor.angle -= 360;
    }

    // Time to think? Updates on 35Hz game ticks.
    if(!DD_IsSharpTick()) return;

    // Advance menu time.
    menuTime++;

    // Animate the cursor graphic?
    if(--cursor.animCounter <= 0)
    {
        cursor.animFrame++;
        cursor.animCounter = MENU_CURSOR_TICSPERFRAME;
        if(cursor.animFrame > MENU_CURSOR_FRAMECOUNT-1)
            cursor.animFrame = 0;
    }

    // Used for Heretic's rotating skulls.
    frame = (menuTime / 3) % 18;

    // Call the active page's ticker.
    currentPage->tick();

#undef MENUALPHA_FADE_STEP
}

bool Hu_MenuHasPage()
{
    return currentPage != 0;
}

Page &Hu_MenuPage()
{
    if(currentPage)
    {
        return *currentPage;
    }
    throw Error("Hu_MenuPage", "No current Page is presently configured");
}

void Hu_MenuSetPage(Page *page, bool canReactivate)
{
    if(!menuActive) return;
    if(!page) return;

    if(!Get(DD_NOVIDEO))
    {
        FR_ResetTypeinTimer();
    }

    cursor.angle = 0; // Stop cursor rotation animation dead (don't rewind).
    menuNominatingQuickSaveSlot = false;

    if(currentPage == page)
    {
        if(!canReactivate) return;
        page->setFocus(0);
    }

    // This is now the "active" page.
    currentPage = page;
    page->activate();
}

bool Hu_MenuIsVisible()
{
    return (menuActive || mnAlpha > .0001f);
}

void Hu_MenuDefaultFocusAction(Widget &, Widget::Action action)
{
    if(action != Widget::FocusGained) return;
    Hu_MenuUpdateCursorState();
}

short Hu_MenuMergeEffectWithDrawTextFlags(short f)
{
    return ((~cfg.common.menuEffectFlags & DTF_NO_EFFECTS) | (f & ~DTF_NO_EFFECTS));
}

void Hu_MenuDrawFocusCursor(const Vec2i &origin, float scale, float alpha)
{
#if __JDOOM__ || __JDOOM64__
# define OFFSET_X         (-22)
# define OFFSET_Y         (-1)
#elif __JHERETIC__ || __JHEXEN__
# define OFFSET_X         (-16)
# define OFFSET_Y         (1)
#endif

    const float angle   = cursor.angle;
    const int cursorIdx = cursor.animFrame;
    patchid_t pCursor   = pCursors[cursorIdx % MENU_CURSOR_FRAMECOUNT];

    patchinfo_t info;
    if(!R_GetPatchInfo(pCursor, &info))
        return;

//    const float scale = /*de::min((focusObjectHeight * 1.267f) /*/ 1; //info.geometry.size.height; //, 1.f);
    Vec2i pos = origin + Vec2i(OFFSET_X, OFFSET_Y) * scale;
//    pos.y -= info.geometry.size.height / 2;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(pos.x, pos.y, 0);
    DGL_Scalef(scale, scale, 1);
    DGL_Rotatef(angle, 0, 0, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, alpha);

    GL_DrawPatch(pCursor, Vec2i(0, 0), 0, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef OFFSET_Y
#undef OFFSET_X
}

void Hu_MenuDrawPageTitle(String title, const Vec2i &origin)
{
    title = Widget::labelText(title);

    if(title.isEmpty()) return;    

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorv(cfg.common.menuTextColors[0]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(title, origin.x, origin.y, ALIGN_TOP, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

void Hu_MenuDrawPageHelp(String helpText, const Vec2i &origin)
{
    if(helpText.isEmpty()) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(SCREENWIDTH / 2, SCREENHEIGHT, 0);
    DGL_Scalef(.666666f, .666666f, 1.f);
    DGL_Translatef(-SCREENWIDTH / 2, -SCREENHEIGHT, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorv(cfg.common.menuTextColors[1]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(helpText, origin.x, origin.y, ALIGN_BOTTOM, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
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
        Get(DD_WINDOW_WIDTH), Get(DD_WINDOW_HEIGHT), scalemode_t(cfg.common.menuScaleMode));
    GL_BeginBorderedProjection(&bp);

    // First determine whether the focus cursor should be visible.
    Widget *focused = Hu_MenuPage().focusWidget();
    bool showFocusCursor = true;
    if(focused && focused->isActive())
    {
        if(is<ColorEditWidget>(focused) || is<InputBindingWidget>(focused))
        {
            showFocusCursor = false;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(SCREENWIDTH/2, SCREENHEIGHT/2, 0);
    DGL_Scalef(cfg.common.menuScale, cfg.common.menuScale, 1);
    DGL_Translatef(-(SCREENWIDTH/2), -(SCREENHEIGHT/2), 0);

    Hu_MenuPage().draw(mnAlpha, showFocusCursor);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    GL_EndBorderedProjection(&bp);

    // Drawing any overlays?
    if(focused && focused->isActive())
    {
        if(is<ColorEditWidget>(focused))
        {
            drawOverlayBackground(OVERLAY_DARKEN);
            GL_BeginBorderedProjection(&bp);

            beginOverlayDraw();
                Hu_MenuPage("ColorWidget").draw();
            endOverlayDraw();

            GL_EndBorderedProjection(&bp);
        }
        if(InputBindingWidget *binds = maybeAs<InputBindingWidget>(focused))
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

static void initAllPages()
{
    Hu_MenuInitColorWidgetPage();
    Hu_MenuInitMainPage();
    //Hu_MenuInitGameTypePage();
    Hu_MenuInitEpisodePage();
#if __JHEXEN__
    Hu_MenuInitPlayerClassPage();
#endif
    Hu_MenuInitSkillPage();
    //Hu_MenuInitMultiplayerPage();
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuInitFilesPage();
#endif
    Hu_MenuInitLoadGameAndSaveGamePages();
    Hu_MenuInitOptionsPage();
    Hu_MenuInitPlayerSetupPage();
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
    pages.deleteAll();
    pages.clear();
}

int Hu_MenuColorWidgetCmdResponder(Page &page, menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_NAV_OUT: {
        Widget *wi = page.userValue().as<NativePointerValue>().nativeObject<Widget>();
        wi->setFlags(Widget::Active, UnsetFlags);
        S_LocalSound(SFX_MENU_CANCEL, NULL);
        colorWidgetActive = false;

        /// @kludge We should re-focus on the object instead.
        cursor.angle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true; }

    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        return true; // Eat these.

    case MCMD_SELECT: {
        Widget *wi = page.userValue().as<NativePointerValue>().nativeObject<Widget>();
        ColorEditWidget &cbox = wi->as<ColorEditWidget>();
        cbox.setFlags(Widget::Active, UnsetFlags);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        colorWidgetActive = false;
        cbox.setColor(page.findWidget(Widget::Id0).as<ColorEditWidget>().color(), 0);

        /// @kludge We should re-focus on the object instead.
        cursor.angle = 0; // Stop cursor rotation animation dead (don't rewind).
        Hu_MenuUpdateCursorState();
        /// kludge end.
        return true; }

    default: break;
    }

    return false;
}

/**
 * Determines if manual episode selection via the menu can be skipped if only one
 * episode is playable.
 *
 * Some demo/shareware game versions use the episode selection menu for the purpose
 * of prompting the user to buy the full version. In such a case, disable skipping.
 *
 * @return  @c true if skipping is allowed.
 */
static bool allowSkipEpisodeSelection()
{
#if __JDOOM__
    if(gameMode == doom_shareware)    return false; // Never.
#elif __JHERETIC__
    if(gameMode == heretic_shareware) return false; // Never.
#endif
    return true;
}

int Hu_MenuSkipPreviousPageIfSkippingEpisodeSelection(Page &page, menucommand_e cmd)
{
    // All we react to are MCMD_NAV_OUT commands.
    if(cmd != MCMD_NAV_OUT) return false;

    Page *previous = page.previousPage();

    // Skip this page if only one episode is playable.
    if(allowSkipEpisodeSelection() && PlayableEpisodeCount() == 1)
    {
        previous = previous->previousPage();
    }

    if(previous)
    {
        S_LocalSound(SFX_MENU_CANCEL, nullptr);
        Hu_MenuSetPage(previous);
    }
    else
    {
        // No previous page so just close the menu.
        S_LocalSound(SFX_MENU_CLOSE, nullptr);
        Hu_MenuCommand(MCMD_CLOSE);
    }

    return true;
}

/// Depending on the current menu state some commands require translating.
static menucommand_e translateCommand(menucommand_e cmd)
{
    // If a close command is received while currently working with a selected
    // "active" widget - interpret the command instead as "navigate out".
    if(menuActive && (cmd == MCMD_CLOSE || cmd == MCMD_CLOSEFAST))
    {
        if(Widget *wi = Hu_MenuPage().focusWidget())
        {
            if(wi->isActive() &&
               (is<LineEditWidget>(wi) || is<ListWidget>(wi) || is<ColorEditWidget>(wi)))
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
    Page *page = colorWidgetActive? Hu_MenuPagePtr("ColorWidget") : Hu_MenuPagePtr();

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
            Hu_MenuSetOpacity(1);
            menuActive = true;
            menuTime = 0;

            currentPage = NULL; // Always re-activate this page.
            Hu_MenuSetPage("Main");

            // Enable the menu binding class
            DD_Execute(true, "activatebcontext menu");
            B_SetContextFallback("menu", Hu_MenuFallbackResponder);
        }
        return;
    }

    page->handleCommand(cmd);
}

int Hu_MenuPrivilegedResponder(event_t *ev)
{
    DE_ASSERT(ev);
    if(Hu_MenuIsActive())
    {
        if(Widget *focused = Hu_MenuPage().focusWidget())
        {
            if(!focused->isDisabled())
            {
                return focused->handleEvent_Privileged(*ev);
            }
        }
    }
    return false;
}

int Hu_MenuResponder(event_t *ev)
{
    DE_ASSERT(ev);
    if(Hu_MenuIsActive())
    {
        if(Widget *focused = Hu_MenuPage().focusWidget())
        {
            if(!focused->isDisabled())
            {
                return focused->handleEvent(*ev);
            }
        }
    }
    return false; // Not eaten.
}

int Hu_MenuFallbackResponder(event_t *ev)
{
    DE_ASSERT(ev);
    Page *page = Hu_MenuPagePtr();

    if(!Hu_MenuIsActive() || !page) return false;

    if(cfg.common.menuShortcutsEnabled)
    {
        if(ev->type == EV_KEY && (ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        {
            for(Widget *wi : page->children())
            {
                if(wi->isDisabled() || wi->isHidden())
                    continue;

                if(wi->flags() & Widget::NoFocus)
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
void Hu_MenuSelectLoadSlot(Widget &wi, Widget::Action action)
{
    LineEditWidget *edit = &wi.as<LineEditWidget>();

    if(action != Widget::Deactivated) return;

    // Linked focus between LoadGame and SaveGame pages.
    Page &saveGamePage = Hu_MenuPage("SaveGame");
    saveGamePage.setFocus(saveGamePage.tryFindWidget(wi.userValue2().asUInt()));

    Page &loadGamePage = Hu_MenuPage("LoadGame");
    loadGamePage.setFocus(loadGamePage.tryFindWidget(wi.userValue2().asUInt()));

    G_SetGameActionLoadSession(edit->userValue().asText());
    Hu_MenuCommand(chooseCloseMethod());
}

#if __JHERETIC__ || __JHEXEN__
void Hu_MenuDrawMainPage(const Page & /*page*/, const Vec2i &origin)
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

    WI_DrawPatch(pMainTitle, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), pMainTitle),
                 Vec2i(origin.x + TITLEOFFSET_X, origin.y + TITLEOFFSET_Y), ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));
#if __JHEXEN__
    GL_DrawPatch(pBullWithFire[(frame + 2) % 7], origin + Vec2i(-73, 24));
    GL_DrawPatch(pBullWithFire[frame],           origin + Vec2i(168, 24));
#elif __JHERETIC__
    GL_DrawPatch(pRotatingSkull[17 - frame],     origin + Vec2i(-70, -46));
    GL_DrawPatch(pRotatingSkull[frame],          origin + Vec2i(122, -46));
#endif

    DGL_Disable(DGL_TEXTURE_2D);

#undef TITLEOFFSET_Y
#undef TITLEOFFSET_X
}
#endif

void Hu_MenuDrawGameTypePage(const Page & /*page*/, const Vec2i &origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PICKGAMETYPE), Vec2i(SCREENWIDTH / 2, origin.y - 28));
}

#if __JHEXEN__
/**
 * A specialization of MNRect_Ticker() which implements the animation logic
 * for the player class selection page's player visual background.
 */
void Hu_MenuPlayerClassBackgroundTicker(Widget &wi)
{
    RectWidget &bg = wi.as<RectWidget>();

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    if(Widget *mop = wi.page().focusWidget())
    {
        playerclass_t pClass = playerclass_t(mop->userValue2().asInt());
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
void Hu_MenuPlayerClassPreviewTicker(Widget &wi)
{
    MobjPreviewWidget &mprev = wi.as<MobjPreviewWidget>();

    // Determine our selection according to the current focus object.
    /// @todo Do not search for the focus object, flag the "random"
    ///        state through a focus action.
    if(Widget *mop = wi.page().focusWidget())
    {
        playerclass_t pClass = playerclass_t(mop->userValue2().asInt());
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

void Hu_MenuDrawPlayerClassPage(const Page & /*page*/, const Vec2i &origin)
{
    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.common.menuTextColors[0][CR], cfg.common.menuTextColors[0][CG], cfg.common.menuTextColors[0][CB], mnRendState->pageAlpha);

    FR_DrawTextXY3("Choose class:", origin.x - 32, origin.y - 42, ALIGN_TOPLEFT,
                   Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}
#endif

void Hu_MenuDrawEpisodePage(const Page &page, const Vec2i &origin)
{
#if __JDOOM__
    DE_UNUSED(page);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorv(cfg.common.menuTextColors[0]);
    FR_SetAlpha(mnRendState->pageAlpha);

    WI_DrawPatch(pEpisode, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), pEpisode),
                 Vec2i(origin.x + 7, origin.y - 25), ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#else
    DE_UNUSED(page);

#if defined (__JHERETIC__)
    String titleText;
#else
    String titleText = "Choose episode:";
#endif

    if (const auto *value = Defs().getValueById("Menu Label|Episode Page Title"))
    {
        titleText = value->text;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.common.menuTextColors[0][CR], cfg.common.menuTextColors[0][CG], cfg.common.menuTextColors[0][CB], mnRendState->pageAlpha);

    FR_DrawTextXY3(titleText, SCREENWIDTH / 2, origin.y - 42, ALIGN_TOP,
                   Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void Hu_MenuDrawSkillPage(const Page & /*page*/, const Vec2i &origin)
{
#if __JDOOM__ || __JDOOM64__
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.common.menuTextColors[0][CR], cfg.common.menuTextColors[0][CG], cfg.common.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pNewGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), pNewGame),
                 Vec2i(origin.x + 48, origin.y - 49), ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));
    WI_DrawPatch(pSkill, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), pSkill),
                 Vec2i(origin.x + 6,  origin.y - 25), ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#elif __JHEXEN__
    Hu_MenuDrawPageTitle("Choose Skill Level:", Vec2i(origin.x + 36, origin.y - 28));
#else
#if defined (__JHERETIC__)
    String titleText;
#else
    String titleText = "Choose Skill Level:";
#endif

    if (const auto *value = Defs().getValueById("Menu Label|Skill Page Title"))
    {
        titleText = value->text;
    }

    Hu_MenuDrawPageTitle(titleText, Vec2i(SCREENWIDTH / 2, origin.y - 28));
#endif
}

/**
 * Called after the save name has been modified and to action the game-save.
 */
void Hu_MenuSelectSaveSlot(Widget &wi, Widget::Action action)
{
    if(action != Widget::Deactivated) return;

    LineEditWidget &edit = wi.as<LineEditWidget>();
    const String saveSlotId = edit.userValue().asText();

    if(menuNominatingQuickSaveSlot)
    {
        Con_SetInteger("game-save-quick-slot", saveSlotId.toInt());
        menuNominatingQuickSaveSlot = false;
    }

    String userDescription = edit.text();
    if(!G_SetGameActionSaveSession(saveSlotId, &userDescription))
    {
        return;
    }

    Page &saveGamePage = Hu_MenuPage("SaveGame");
    saveGamePage.setFocus(saveGamePage.tryFindWidget(wi.userValue2().asUInt()));

    Page &loadGamePage = Hu_MenuPage("LoadGame");
    loadGamePage.setFocus(loadGamePage.tryFindWidget(wi.userValue2().asUInt()));

    Hu_MenuCommand(chooseCloseMethod());
}

void Hu_MenuSaveSlotEdit(Widget &wi, Widget::Action action)
{
    if(action != Widget::Activated) return;
    if(cfg.common.menuGameSaveSuggestDescription)
    {
        auto &edit = wi.as<LineEditWidget>();
        edit.setText(G_DefaultGameStateFolderUserDescription("" /*don't reuse an existing description*/));
    }
}

void Hu_MenuActivateColorWidget(Widget &wi, Widget::Action action)
{
    if(action != Widget::Activated) return;

    ColorEditWidget &cbox = wi.as<ColorEditWidget>();

    Page &colorWidgetPage    = Hu_MenuPage("ColorWidget");
    ColorEditWidget &cboxMix = colorWidgetPage.findWidget(Widget::Id0).as<ColorEditWidget>();
    SliderWidget &sldrRed    = colorWidgetPage.findWidget(Widget::Id1).as<SliderWidget>();
    SliderWidget &sldrGreen  = colorWidgetPage.findWidget(Widget::Id2).as<SliderWidget>();
    SliderWidget &sldrBlue   = colorWidgetPage.findWidget(Widget::Id3).as<SliderWidget>();
    LabelWidget  &labelAlpha = colorWidgetPage.findWidget(Widget::Id4).as<LabelWidget>();
    SliderWidget &sldrAlpha  = colorWidgetPage.findWidget(Widget::Id5).as<SliderWidget>();

    colorWidgetActive = true;

    colorWidgetPage.activate();
    colorWidgetPage.setUserValue(NativePointerValue(&wi));

    cboxMix.setColor(cbox.color(), 0);

    sldrRed  .setValue(cbox.red());
    sldrGreen.setValue(cbox.green());
    sldrBlue .setValue(cbox.blue());
    sldrAlpha.setValue(cbox.alpha());

    labelAlpha.setFlags(Widget::Disabled | Widget::Hidden, (cbox.rgbaMode()? UnsetFlags : SetFlags));
    sldrAlpha. setFlags(Widget::Disabled | Widget::Hidden, (cbox.rgbaMode()? UnsetFlags : SetFlags));
}

void Hu_MenuDrawLoadGamePage(const Page & /*page*/, const Vec2i &origin)
{
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.common.menuTextColors[0][CR], cfg.common.menuTextColors[0][CG], cfg.common.menuTextColors[0][CB], mnRendState->pageAlpha);

#if __JHERETIC__ || __JHEXEN__
    FR_DrawTextXY3(Widget::labelText("Load Game"), SCREENWIDTH / 2, origin.y - 20, ALIGN_TOP, Hu_MenuMergeEffectWithDrawTextFlags(0));
#else
    WI_DrawPatch(pLoadGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), pLoadGame),
                 Vec2i(origin.x - 8, origin.y - 26), ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));
#endif
    DGL_Disable(DGL_TEXTURE_2D);

    Vec2i helpOrigin(SCREENWIDTH / 2, (SCREENHEIGHT / 2) + ((SCREENHEIGHT / 2 - 5) / cfg.common.menuScale));
    Hu_MenuDrawPageHelp("Select to load, [Del] to clear", helpOrigin);
}

void Hu_MenuDrawSaveGamePage(const Page & /*page*/, const Vec2i &origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Save Game", Vec2i(SCREENWIDTH / 2, origin.y - 20));
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.common.menuTextColors[0][CR], cfg.common.menuTextColors[0][CG], cfg.common.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pSaveGame, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), pSaveGame),
                 Vec2i(origin.x - 8, origin.y - 26), ALIGN_TOPLEFT, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif

    Vec2i helpOrigin(SCREENWIDTH / 2, (SCREENHEIGHT / 2) + ((SCREENHEIGHT / 2 - 5) / cfg.common.menuScale));
    Hu_MenuDrawPageHelp("Select to save, [Del] to clear", helpOrigin);
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
void Hu_MenuSelectHelp(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;
    G_StartHelp();
}
#endif

void Hu_MenuDrawOptionsPage(const Page & /*page*/, const Vec2i &origin)
{
#if __JHERETIC__ || __JHEXEN__
    Hu_MenuDrawPageTitle("Options", Vec2i(origin.x + 42, origin.y - 30));
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.common.menuTextColors[0][CR], cfg.common.menuTextColors[0][CG], cfg.common.menuTextColors[0][CB], mnRendState->pageAlpha);

    WI_DrawPatch(pOptionsTitle, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.menuPatchReplaceMode), pOptionsTitle),
                 Vec2i(origin.x + 42, origin.y - 20), ALIGN_TOP, 0, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

void Hu_MenuDrawMultiplayerPage(const Page & /*page*/, const Vec2i &origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_MULTIPLAYER), Vec2i(SCREENWIDTH / 2, origin.y - 28));
}

void Hu_MenuDrawPlayerSetupPage(const Page & /*page*/, const Vec2i &origin)
{
    Hu_MenuDrawPageTitle(GET_TXT(TXT_PLAYERSETUP), Vec2i(SCREENWIDTH / 2, origin.y - 28));
}

void Hu_MenuActionSetActivePage(Widget &wi, Widget::Action action)
{
    if(action != Widget::Deactivated) return;
    Hu_MenuSetPage(Hu_MenuPagePtr(wi.as<ButtonWidget>().userValue().asText()));
}

void Hu_MenuUpdateColorWidgetColor(Widget &wi, Widget::Action action)
{
    if(action != Widget::Modified) return;

    SliderWidget &sldr = wi.as<SliderWidget>();
    float value = sldr.value();
    ColorEditWidget &cboxMix = Hu_MenuPage("ColorWidget").findWidget(Widget::Id0).as<ColorEditWidget>();

    const int component = wi.userValue2().asInt();
    switch(component)
    {
    case CR: cboxMix.setRed  (value); break;
    case CG: cboxMix.setGreen(value); break;
    case CB: cboxMix.setBlue (value); break;
    case CA: cboxMix.setAlpha(value); break;

    default: DE_ASSERT_FAIL("Hu_MenuUpdateColorWidgetColor: Invalid value for data2.");
    }
}

void Hu_MenuChangeWeaponPriority(Widget &wi, Widget::Action action)
{
    if (action == Widget::Modified)
    {
        auto &list = wi.as<ListWidget>();
        for (int i = 0; i < list.itemCount(); ++i)
        {
            cfg.common.weaponOrder[i] = list.itemData(i);
        }
    }
}

void Hu_MenuSelectSingleplayer(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;

    // If a networked game is already in progress inform the user we can't continue.
    /// @todo Allow continue: Ask the user if the networked game should be stopped.
    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NEWGAME, nullptr, 0, nullptr);
        return;
    }

    // Skip episode selection if only one is playable.
    if(allowSkipEpisodeSelection() && PlayableEpisodeCount() == 1)
    {
        mnEpisode = FirstPlayableEpisodeId();
#if __JHEXEN__
        Hu_MenuSetPage("PlayerClass");
#else
        Hu_MenuSetPage("Skill");
#endif
        return;
    }

    // Show the episode selection menu.
    Hu_MenuSetPage("Episode");
}

#if 0
void Hu_MenuSelectMultiplayer(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;

    Page &multiplayerPage = Hu_MenuPage("Multiplayer");

    // Set the appropriate label.
    ButtonWidget *btn = &multiplayerPage.findWidget(Widget::Id0).as<ButtonWidget>();
    if(IS_NETGAME)
    {
        btn->setText("Disconnect");
    }
    else
    {
        btn->setText("Join Game");
    }

    Hu_MenuSetPage(&multiplayerPage);
}
#endif

void Hu_MenuSelectJoinGame(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;

    if(IS_NETGAME)
    {
        DD_Execute(false, "net disconnect");
        Hu_MenuCommand(MCMD_CLOSE);
        return;
    }

    DD_Execute(false, "net setup client");
}

void Hu_MenuActivatePlayerSetup(Page &page)
{
    MobjPreviewWidget &mop = page.findWidget(Widget::Id0).as<MobjPreviewWidget>();
    LineEditWidget &name   = page.findWidget(Widget::Id1).as<LineEditWidget>();
    ListWidget &color      = page.findWidget(Widget::Id3).as<ListWidget>();

#if __JHEXEN__
    mop.setMobjType(PCLASS_INFO(cfg.netClass)->mobjType);
    mop.setPlayerClass(cfg.netClass);
#else
    mop.setMobjType(MT_PLAYER);
    mop.setPlayerClass(PCLASS_PLAYER);
#endif
    mop.setTranslationClass(0);
    mop.setTranslationMap(cfg.common.netColor);

    color.selectItemByValue(cfg.common.netColor);
#if __JHEXEN__
    ListWidget &class_ = page.findWidget(Widget::Id2).as<ListWidget>();
    class_.selectItemByValue(cfg.netClass);
#endif

    name.setText(Con_GetString("net-name"), MNEDIT_STF_NO_ACTION | MNEDIT_STF_REPLACEOLD);
}

#if __JHEXEN__
void Hu_MenuSelectPlayerSetupPlayerClass(Widget &wi, Widget::Action action)
{
    if(action != Widget::Modified) return;

    ListWidget &list = wi.as<ListWidget>();
    int selection = list.selection();
    if(selection >= 0)
    {
        MobjPreviewWidget &mop = wi.page().findWidget(Widget::Id0).as<MobjPreviewWidget>();
        mop.setPlayerClass(selection);
        mop.setMobjType(PCLASS_INFO(selection)->mobjType);
    }
}
#endif

void Hu_MenuSelectPlayerColor(Widget &wi, Widget::Action action)
{
    if(action != Widget::Modified) return;

    // The color translation map is stored in the list item data member.
    ListWidget &list = wi.as<ListWidget>();
    int selection = list.itemData(list.selection());
    if(selection >= 0)
    {
        wi.page().findWidget(Widget::Id0).as<MobjPreviewWidget>().setTranslationMap(selection);
    }
}

void Hu_MenuSelectAcceptPlayerSetup(Widget &wi, Widget::Action action)
{
    Page &page                  = wi.page();
    LineEditWidget &plrNameEdit = page.findWidget(Widget::Id1).as<LineEditWidget>();
#if __JHEXEN__
    ListWidget &plrClassList    = page.findWidget(Widget::Id2).as<ListWidget>();
#endif
    ListWidget &plrColorList    = page.findWidget(Widget::Id3).as<ListWidget>();

#if __JHEXEN__
    cfg.netClass = plrClassList.selection();
#endif
    // The color translation map is stored in the list item data member.
    cfg.common.netColor = plrColorList.itemData(plrColorList.selection());

    if(action != Widget::Deactivated) return;

    char buf[300];
    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, plrNameEdit.text(), 300);
    DD_Execute(false, buf);

    if(IS_NETGAME)
    {
        strcpy(buf, "setname ");
        M_StrCatQuoted(buf, plrNameEdit.text(), 300);
        DD_Execute(false, buf);
#if __JHEXEN__
        // Must do 'setclass' first; the real class and color do not change
        // until the server sends us a notification -- this means if we do
        // 'setcolor' first, the 'setclass' after it will override the color
        // change (or such would appear to be the case).
        DD_Executef(false, "setclass %i", cfg.netClass);
#endif
        DD_Executef(false, "setcolor %i", cfg.common.netColor);
    }

    Hu_MenuSetPage("Options");
}

void Hu_MenuSelectQuitGame(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;
    G_QuitGame();
}

void Hu_MenuSelectEndGame(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;
    DD_Executef(true, "endgame");
}

void Hu_MenuSelectLoadGame(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;

    if(!Get(DD_NOVIDEO))
    {
        if(IS_CLIENT && !Get(DD_PLAYBACK))
        {
            Hu_MsgStart(MSG_ANYKEY, LOADNET, NULL, 0, NULL);
            return;
        }
    }

    Hu_MenuSetPage("LoadGame");
}

void Hu_MenuSelectSaveGame(Widget & /*wi*/, Widget::Action action)
{
    player_t *player = &players[CONSOLEPLAYER];

    if(action != Widget::Deactivated) return;

    if(!Get(DD_NOVIDEO))
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
    Hu_MenuSetPage("SaveGame");
}

#if __JHEXEN__
void Hu_MenuSelectPlayerClass(Widget &wi, Widget::Action action)
{
    Page &skillPage = Hu_MenuPage("Skill");
    int option = wi.userValue2().asInt();

    if(action != Widget::Deactivated) return;

    if(IS_NETGAME)
    {
        P_SetMessageWithFlags(&players[CONSOLEPLAYER], "You can't start a new game from within a netgame!", LMF_NO_HIDE);
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

    ButtonWidget *btn;
    btn = &skillPage.findWidget(Widget::Id0).as<ButtonWidget>();
    btn->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeName[SM_BABY]));
    if(!btn->text().isEmpty() && btn->text().first().isAlphaNumeric()) btn->setShortcut(btn->text().first());

    btn = &skillPage.findWidget(Widget::Id1).as<ButtonWidget>();
    btn->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeName[SM_EASY]));
    if(!btn->text().isEmpty() && btn->text().first().isAlphaNumeric()) btn->setShortcut(btn->text().first());

    btn = &skillPage.findWidget(Widget::Id2).as<ButtonWidget>();
    btn->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeName[SM_MEDIUM]));
    if(!btn->text().isEmpty() && btn->text().first().isAlphaNumeric()) btn->setShortcut(btn->text().first());

    btn = &skillPage.findWidget(Widget::Id3).as<ButtonWidget>();
    btn->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeName[SM_HARD]));
    if(!btn->text().isEmpty() && btn->text().first().isAlphaNumeric()) btn->setShortcut(btn->text().first());

    btn = &skillPage.findWidget(Widget::Id4).as<ButtonWidget>();
    btn->setText(GET_TXT(PCLASS_INFO(mnPlrClass)->skillModeName[SM_NIGHTMARE]));
    if(!btn->text().isEmpty() && btn->text().first().isAlphaNumeric()) btn->setShortcut(btn->text().first());

    switch(mnPlrClass)
    {
    case PCLASS_FIGHTER:    skillPage.setX(120); break;
    case PCLASS_CLERIC:     skillPage.setX(116); break;
    case PCLASS_MAGE:       skillPage.setX(112); break;
    }
    Hu_MenuSetPage(&skillPage);
}

void Hu_MenuFocusOnPlayerClass(Widget &wi, Widget::Action action)
{
    if(action != Widget::FocusGained) return;

    playerclass_t plrClass = playerclass_t(wi.userValue2().asInt());
    MobjPreviewWidget &mop = wi.page().findWidget(Widget::Id0).as<MobjPreviewWidget>();
    mop.setPlayerClass(plrClass);
    mop.setMobjType((PCLASS_NONE == plrClass? MT_NONE : PCLASS_INFO(plrClass)->mobjType));

    Hu_MenuDefaultFocusAction(wi, action);
}
#endif

void Hu_MenuSelectEpisode(Widget &wi, Widget::Action /*action*/)
{
    mnEpisode = wi.as<ButtonWidget>().userValue().asText();
#if __JHEXEN__
    Hu_MenuSetPage("PlayerClass");
#else
    Hu_MenuSetPage("Skill");
#endif
}

#if __JDOOM__ || __JHERETIC__
int Hu_MenuConfirmOrderCommericalVersion(msgresponse_t /*response*/, int /*userValue*/, void * /*context*/)
{
    G_StartHelp();
    return true;
}

void Hu_MenuActivateNotSharewareEpisode(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;
    Hu_MsgStart(MSG_ANYKEY, SWSTRING, Hu_MenuConfirmOrderCommericalVersion, 0, NULL);
}
#endif

void Hu_MenuFocusSkillMode(Widget &wi, Widget::Action action)
{
    if(action != Widget::FocusGained) return;
    mnSkillmode = skillmode_t(wi.userValue2().asInt());
    Hu_MenuDefaultFocusAction(wi, action);
}

#if __JDOOM__ || __JHERETIC__
static int Hu_MenuConfirmInitNewGame(msgresponse_t response, int /*userValue*/, void * /*context*/)
{
    if (response == MSG_YES)
    {
        Hu_MenuInitNewGame(true);
    }
    return true;
}
#endif

/**
 * Initialize a new singleplayer game according to the options set via the menu.
 * @param confirmed  If @c true this game configuration has already been confirmed.
 */
static void Hu_MenuInitNewGame(bool confirmed)
{
#if __JDOOM__ || __JHERETIC__
    const int nightmareTextNum = Defs().getTextNum("NIGHTMARE");
    if (nightmareTextNum >= 0 && strlen(Defs().text[nightmareTextNum].text) > 0)
    {
        if (!confirmed && mnSkillmode == SM_NIGHTMARE)
        {
            Hu_MsgStart(MSG_YESNO, Defs().text[nightmareTextNum].text, Hu_MenuConfirmInitNewGame, 0, NULL);
            return;
        }
    }
#else
    DE_UNUSED(confirmed);
#endif

    Hu_MenuCommand(chooseCloseMethod());

#if __JHEXEN__
    cfg.playerClass[CONSOLEPLAYER] = playerclass_t(mnPlrClass);
#endif

    GameRules newRules{gfw_DefaultGameRules()};
    GameRules_Set(newRules, skill, mnSkillmode);

    const Record &episodeDef = Defs().episodes.find("id", mnEpisode);
    G_SetGameActionNewSession(newRules, mnEpisode, res::makeUri(episodeDef.gets("startMap")));
}

void Hu_MenuActionInitNewGame(Widget & /*wi*/, Widget::Action action)
{
    if(action != Widget::Deactivated) return;
    Hu_MenuInitNewGame(false);
}

void Hu_MenuSelectControlPanelLink(Widget &wi, Widget::Action action)
{
#define NUM_PANEL_NAMES         1

    static const char *panelNames[NUM_PANEL_NAMES] = {
        "taskbar" //,
        //"panel audio",
        //"panel input"
    };

    if(action != Widget::Deactivated) return;

    int idx = wi.userValue2().asInt();
    if(idx < 0 || idx > NUM_PANEL_NAMES - 1)
    {
        idx = 0;
    }

    DD_Execute(true, panelNames[idx]);

#undef NUM_PANEL_NAMES
}

D_CMD(MenuOpen)
{
    DE_UNUSED(src);

    if(argc > 1)
    {
        if(!iCmpStrCase(argv[1], "open"))
        {
            Hu_MenuCommand(MCMD_OPEN);
            return true;
        }
        if(!iCmpStrCase(argv[1], "close"))
        {
            Hu_MenuCommand(MCMD_CLOSE);
            return true;
        }

        const char *pageName = argv[1];
        if(Hu_MenuHasPage(pageName))
        {
            Hu_MenuCommand(MCMD_OPEN);
            Hu_MenuSetPage(pageName);
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
    DE_UNUSED(src, argc);

    if(menuActive)
    {
        const char *cmd = argv[0] + 4;
        if(!iCmpStrCase(cmd, "up"))
        {
            Hu_MenuCommand(MCMD_NAV_UP);
            return true;
        }
        if(!iCmpStrCase(cmd, "down"))
        {
            Hu_MenuCommand(MCMD_NAV_DOWN);
            return true;
        }
        if(!iCmpStrCase(cmd, "left"))
        {
            Hu_MenuCommand(MCMD_NAV_LEFT);
            return true;
        }
        if(!iCmpStrCase(cmd, "right"))
        {
            Hu_MenuCommand(MCMD_NAV_RIGHT);
            return true;
        }
        if(!iCmpStrCase(cmd, "back"))
        {
            Hu_MenuCommand(MCMD_NAV_OUT);
            return true;
        }
        if(!iCmpStrCase(cmd, "delete"))
        {
            Hu_MenuCommand(MCMD_DELETE);
            return true;
        }
        if(!iCmpStrCase(cmd, "select"))
        {
            Hu_MenuCommand(MCMD_SELECT);
            return true;
        }
        if(!iCmpStrCase(cmd, "pagedown"))
        {
            Hu_MenuCommand(MCMD_NAV_PAGEDOWN);
            return true;
        }
        if(!iCmpStrCase(cmd, "pageup"))
        {
            Hu_MenuCommand(MCMD_NAV_PAGEUP);
            return true;
        }
    }
    return false;
}

void Hu_MenuConsoleRegister()
{
    C_VAR_FLOAT("menu-scale",               &cfg.common.menuScale,              0, .1f, 1);
    C_VAR_BYTE ("menu-stretch",             &cfg.common.menuScaleMode,          0, SCALEMODE_FIRST, SCALEMODE_LAST);
    C_VAR_FLOAT("menu-flash-r",             &cfg.common.menuTextFlashColor[CR], 0, 0, 1);
    C_VAR_FLOAT("menu-flash-g",             &cfg.common.menuTextFlashColor[CG], 0, 0, 1);
    C_VAR_FLOAT("menu-flash-b",             &cfg.common.menuTextFlashColor[CB], 0, 0, 1);
    C_VAR_INT  ("menu-flash-speed",         &cfg.common.menuTextFlashSpeed,     0, 0, 50);
    C_VAR_BYTE ("menu-cursor-rotate",       &cfg.common.menuCursorRotate,       0, 0, 1);
    C_VAR_INT  ("menu-effect",              &cfg.common.menuEffectFlags,        0, 0, MEF_EVERYTHING);
    C_VAR_FLOAT("menu-color-r",             &cfg.common.menuTextColors[0][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-color-g",             &cfg.common.menuTextColors[0][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-color-b",             &cfg.common.menuTextColors[0][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-colorb-r",            &cfg.common.menuTextColors[1][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-colorb-g",            &cfg.common.menuTextColors[1][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-colorb-b",            &cfg.common.menuTextColors[1][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-colorc-r",            &cfg.common.menuTextColors[2][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-colorc-g",            &cfg.common.menuTextColors[2][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-colorc-b",            &cfg.common.menuTextColors[2][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-colord-r",            &cfg.common.menuTextColors[3][CR],  0, 0, 1);
    C_VAR_FLOAT("menu-colord-g",            &cfg.common.menuTextColors[3][CG],  0, 0, 1);
    C_VAR_FLOAT("menu-colord-b",            &cfg.common.menuTextColors[3][CB],  0, 0, 1);
    C_VAR_FLOAT("menu-glitter",             &cfg.common.menuTextGlitter,        0, 0, 1);
    C_VAR_INT  ("menu-fog",                 &cfg.common.hudFog,                 0, 0, 5);
    C_VAR_FLOAT("menu-shadow",              &cfg.common.menuShadow,             0, 0, 1);
    C_VAR_INT  ("menu-patch-replacement",   &cfg.common.menuPatchReplaceMode,   0, 0, 1);
    C_VAR_BYTE ("menu-slam",                &cfg.common.menuSlam,               0, 0, 1);
    C_VAR_BYTE ("menu-hotkeys",             &cfg.common.menuShortcutsEnabled,   0, 0, 1);
#if __JDOOM__ || __JDOOM64__
    C_VAR_INT  ("menu-quitsound",           &cfg.menuQuitSound,          0, 0, 1);
#endif
    C_VAR_BYTE ("menu-save-suggestname",    &cfg.common.menuGameSaveSuggestDescription, 0, 0, 1);

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
