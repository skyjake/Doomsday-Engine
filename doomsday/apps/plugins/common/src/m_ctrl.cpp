/** @file m_ctrl.cpp  Controls menu page and associated widgets.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "m_ctrl.h"

#include <cstring>
#include <cctype>
#include <cstdio>

#include "hu_menu.h"
#include "hu_stuff.h"
#include "menu/page.h"
#include "menu/widgets/inputbindingwidget.h"
#include "menu/widgets/labelwidget.h"

using namespace de;

namespace common {
namespace menu {

static controlconfig_t controlConfig[] =
{
    { "Movement", 0, 0, 0, 0 },
    { "Forward", 0, "walk", 0, CCF_NON_INVERSE },
    { "Backward", 0, "walk", 0, CCF_INVERSE },
    { "Strafe Left", 0, "sidestep", 0, CCF_INVERSE },
    { "Strafe Right", 0, "sidestep", 0, CCF_NON_INVERSE },
    { "Turn Left", 0, "turn", 0, CCF_STAGED | CCF_INVERSE | CCF_SIDESTEP_MODIFIER },
    { "Turn Right", 0, "turn", 0, CCF_STAGED | CCF_NON_INVERSE | CCF_SIDESTEP_MODIFIER },
    { "Jump", 0, 0, "impulse jump", 0 },
    { "Use", 0, 0, "impulse use", 0 },
    { "Fly Up", 0, "zfly", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "Fly Down", 0, "zfly", 0, CCF_STAGED | CCF_INVERSE },
    { "Fall To Ground", 0, 0, "impulse falldown", 0 },
    { "Speed", 0, "speed", 0, 0 },
    { "Strafe", 0, "strafe", 0, 0 },

    { "Looking", 0, 0, 0, 0 },
    { "Look Up", 0, "look", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "Look Down", 0, "look", 0, CCF_STAGED | CCF_INVERSE },
    { "Look Center", 0, 0, "impulse lookcenter", 0 },

    { "Weapons", 0, 0, 0, 0 },
    { "Attack/Fire", 0, "attack", 0, 0 },
    { "Next Weapon", 0, 0, "impulse nextweapon", 0 },
    { "Previous Weapon", 0, 0, "impulse prevweapon", 0 },

#if __JDOOM__ || __JDOOM64__
    { "Fist/Chainsaw", 0, 0, "impulse weapon1", 0 },
    { "Chainsaw/Fist", 0, 0, "impulse weapon8", 0 },
    { "Pistol", 0, 0, "impulse weapon2", 0 },
    { "Super SG/Shotgun", 0, 0, "impulse weapon3", 0 },
    { "Shotgun/Super SG", 0, 0, "impulse weapon9", 0 },
    { "Chaingun", 0, 0, "impulse weapon4", 0 },
    { "Rocket Launcher", 0, 0, "impulse weapon5", 0 },
    { "Plasma Rifle", 0, 0, "impulse weapon6", 0 },
    { "BFG 9000", 0, 0, "impulse weapon7", 0 },
#endif
#if __JDOOM64__
    { "Unmaker", 0, 0, "impulse weapon10", 0 },
#endif

#if __JHERETIC__
    { "Gauntlets/Staff", 0, 0, "impulse weapon1", 0 },
    { "Elvenwand", 0, 0, "impulse weapon2", 0 },
    { "Crossbow", 0, 0, "impulse weapon3", 0 },
    { "Dragon Claw", 0, 0, "impulse weapon4", 0 },
    { "Hellstaff", 0, 0, "impulse weapon5", 0 },
    { "Phoenix Rod", 0, 0, "impulse weapon6", 0 },
    { "Firemace", 0, 0, "impulse weapon7", 0 },
#endif

#if __JHEXEN__
    { "Weapon 1", 0, 0, "impulse weapon1", 0 },
    { "Weapon 2", 0, 0, "impulse weapon2", 0 },
    { "Weapon 3", 0, 0, "impulse weapon3", 0 },
    { "Weapon 4", 0, 0, "impulse weapon4", 0 },
#endif

#if __JHERETIC__ || __JHEXEN__
    { "Inventory", 0, 0, 0, 0 },
    { "Move Left", 0, 0, "impulse previtem", CCF_REPEAT },
    { "Move Right", 0, 0, "impulse nextitem", CCF_REPEAT },
    { "Use Item", 0, 0, "impulse useitem", 0 },
    { "Panic!", 0, 0, "impulse panic", 0 },
#endif

#ifdef __JHERETIC__
    { /* (char const *) TXT_TXT_INV_INVULNERABILITY */ "Invincibility", 0, 0, "impulse invulnerability", 0 },
    { (char const *) TXT_TXT_INV_INVISIBILITY, 0, 0, "impulse invisibility", 0 },
    { (char const *) TXT_TXT_INV_HEALTH, 0, 0, "impulse health", 0 },
    { (char const *) TXT_TXT_INV_SUPERHEALTH, 0, 0, "impulse superhealth", 0 },
    { (char const *) TXT_TXT_INV_TOMEOFPOWER, 0, 0, "impulse tome", 0 },
    { (char const *) TXT_TXT_INV_TORCH, 0, 0, "impulse torch", 0 },
    { /* (char const *) TXT_TXT_INV_FIREBOMB */ "Time Bomb", 0, 0, "impulse firebomb", 0 },
    { (char const *) TXT_TXT_INV_EGG, 0, 0, "impulse egg", 0 },
    { (char const *) TXT_TXT_INV_FLY, 0, 0, "impulse fly", 0 },
    { (char const *) TXT_TXT_INV_TELEPORT, 0, 0, "impulse teleport", 0 },
#endif

#ifdef __JHEXEN__
    { (char const *) TXT_TXT_INV_TORCH, 0, 0, "impulse torch", 0 },
    { (char const *) TXT_TXT_INV_HEALTH, 0, 0, "impulse health", 0 },
    { (char const *) TXT_TXT_INV_SUPERHEALTH, 0, 0, "impulse mysticurn", 0 },
    { (char const *) TXT_TXT_INV_BOOSTMANA, 0, 0, "impulse krater", 0 },
    { (char const *) TXT_TXT_INV_SPEED, 0, 0, "impulse speedboots", 0 },
    { (char const *) TXT_TXT_INV_BLASTRADIUS, 0, 0, "impulse blast", 0 },
    { (char const *) TXT_TXT_INV_TELEPORT, 0, 0, "impulse teleport", 0 },
    { (char const *) TXT_TXT_INV_TELEPORTOTHER, 0, 0, "impulse teleportother", 0 },
    { (char const *) TXT_TXT_INV_POISONBAG, 0, 0, "impulse poisonbag", 0 },
    { (char const *) TXT_TXT_INV_INVULNERABILITY, 0, 0, "impulse invulnerability", 0 },
    { (char const *) TXT_TXT_INV_SUMMON, 0, 0, "impulse darkservant", 0 },
    { (char const *) TXT_TXT_INV_EGG, 0, 0, "impulse egg", 0 },
#endif

    { "Chat", 0, 0, 0, 0 },
    { "Open Chat", 0, 0, "beginchat", CCF_MULTIPLAYER },

#if __JDOOM__ || __JDOOM64__
    { "Green Chat", 0, 0, "beginchat 0", CCF_MULTIPLAYER },
    { "Indigo Chat", 0, 0, "beginchat 1", CCF_MULTIPLAYER },
    { "Brown Chat", 0, 0, "beginchat 2", CCF_MULTIPLAYER },
    { "Red Chat", 0, 0, "beginchat 3", CCF_MULTIPLAYER },
#endif

#if __JHERETIC__
    { "Green Chat", 0, 0, "beginchat 0", CCF_MULTIPLAYER },
    { "Yellow Chat", 0, 0, "beginchat 1", CCF_MULTIPLAYER },
    { "Red Chat", 0, 0, "beginchat 2", CCF_MULTIPLAYER },
    { "Blue Chat", 0, 0, "beginchat 3", CCF_MULTIPLAYER },
#endif

    { "Send Message", "chat", 0, "chatcomplete", 0 },
    { "Cancel Message", "chat", 0, "chatcancel", 0 },
    { "Macro 1", "chat", 0, "chatsendmacro 0", 0 },
    { "Macro 2", "chat", 0, "chatsendmacro 1", 0 },
    { "Macro 3", "chat", 0, "chatsendmacro 2", 0 },
    { "Macro 4", "chat", 0, "chatsendmacro 3", 0 },
    { "Macro 5", "chat", 0, "chatsendmacro 4", 0 },
    { "Macro 6", "chat", 0, "chatsendmacro 5", 0 },
    { "Macro 7", "chat", 0, "chatsendmacro 6", 0 },
    { "Macro 8", "chat", 0, "chatsendmacro 7", 0 },
    { "Macro 9", "chat", 0, "chatsendmacro 8", 0 },
    { "Macro 10", "chat", 0, "chatsendmacro 9", 0 },
    { "Backspace", "chat", 0, "chatdelete", CCF_REPEAT },

    { "Map", 0, 0, 0, 0 },
    { "Show/Hide Map", 0, 0, "impulse automap", 0 },
    { "Zoom In", 0, "mapzoom", 0, CCF_NON_INVERSE },
    { "Zoom Out", 0, "mapzoom", 0, CCF_INVERSE },
    { "Zoom Maximum", "map", 0, "impulse zoommax", 0 },
    { "Pan Left", 0, "mappanx", 0, CCF_INVERSE },
    { "Pan Right", 0, "mappanx", 0, CCF_NON_INVERSE },
    { "Pan Up", 0, "mappany", 0, CCF_NON_INVERSE },
    { "Pan Down", 0, "mappany", 0, CCF_INVERSE },
    { "Toggle Follow", "map", 0, "impulse follow", 0 },
    { "Toggle Rotation", "map", 0, "impulse rotate", 0 },
    { "Add Mark", "map", 0, "impulse addmark", 0 },
    { "Clear Marks", "map", 0, "impulse clearmarks", 0 },

    { "HUD", 0, 0, 0, 0 },
    { "Show HUD", 0, 0, "impulse showhud", 0 },
    { "Show Score", 0, 0, "impulse showscore", CCF_REPEAT },
    { "Smaller View", 0, 0, "sub view-size 1", CCF_REPEAT },
    { "Larger View", 0, 0, "add view-size 1", CCF_REPEAT },

    { "Msg Refresh", 0, 0, "impulse msgrefresh", 0 },

    { "Shortcuts", 0, 0, 0, 0 },
    { "Show Taskbar", 0, 0, "taskbar", 0 },
    { "Pause Game", 0, 0, "pause", 0 },
#if !__JDOOM64__
    { "Help Screen", "shortcut", 0, "helpscreen", 0 },
#endif
    { "End Game", "shortcut", 0, "endgame", 0 },
    { "Save Game", "shortcut", 0, "menu savegame", 0 },
    { "Load Game", "shortcut", 0, "menu loadgame", 0 },
    { "Quick Save", "shortcut", 0, "quicksave", 0 },
    { "Quick Load", "shortcut", 0, "quickload", 0 },
    { "Sound Options", "shortcut", 0, "menu soundoptions", 0 },
    { "Toggle Messages", "shortcut", 0, "toggle msg-show", 0 },
    { "Gamma Adjust", "shortcut", 0, "togglegamma", 0 },
    { "Screenshot", "shortcut", 0, "screenshot", 0 },
    { "Quit", "shortcut", 0, "quit", 0 },

    { "Menu", 0, 0, 0, 0 },
    { "Show/Hide Menu", "shortcut", 0, "menu", 0 },
    { "Previous Menu", "menu", 0, "menuback", CCF_REPEAT },
    { "Move Up", "menu", 0, "menuup", CCF_REPEAT },
    { "Move Down", "menu", 0, "menudown", CCF_REPEAT },
    { "Move Left", "menu", 0, "menuleft", CCF_REPEAT },
    { "Move Right", "menu", 0, "menuright", CCF_REPEAT },
    { "Select", "menu", 0, "menuselect", 0 },

    { "On-Screen Questions", 0, 0, 0, 0 },
    { "Answer Yes", "message", 0, "messageyes", 0 },
    { "Answer No", "message", 0, "messageno", 0 },
    { "Cancel", "message", 0, "messagecancel", 0 },

//    { "Virtual Reality", 0, 0, 0, 0 },
//    { "Reset Tracking", 0, 0, "resetriftpose", 0 }
};

static void Hu_MenuDrawControlsPage(Page const &page, Vector2i const &offset);

void Hu_MenuActivateBindingsGrab(Widget &, Widget::Action)
{
    // Start grabbing for this control.
    DD_SetInteger(DD_SYMBOLIC_ECHO, true);
}

void Hu_MenuInitControlsPage()
{
    Page *page = Hu_MenuAddPage(new Page("ControlOptions", Vector2i(32, 40), 0, Hu_MenuDrawControlsPage));
    page->setLeftColumnWidth(.4f);
    page->setTitle("Controls");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    int group = 0;
    int const configCount = sizeof(controlConfig) / sizeof(controlConfig[0]);
    for(int i = 0; i < configCount; ++i)
    {
        controlconfig_t *binds = &controlConfig[i];

        char const *labelText = binds->text;
        if (labelText && (PTR2INT(labelText) > 0 && PTR2INT(labelText) < NUMTEXT))
        {
            labelText = GET_TXT(PTR2INT(labelText));
        }

        if (!binds->command && !binds->controlName)
        {
            // Inert.
            page->addWidget(new LabelWidget(labelText))
                    .setGroup(++group)
                    .setColor(MENU_COLOR2);
        }
        else
        {
            page->addWidget(new LabelWidget(labelText))
                    .setLeft()
                    .setGroup(group);

            InputBindingWidget *binding = new InputBindingWidget;
            binding->binds = binds;
            binding->setRight();
            binding->setGroup(group);
            binding->setAction(Widget::Activated,   Hu_MenuActivateBindingsGrab);
            binding->setAction(Widget::FocusGained, Hu_MenuDefaultFocusAction);

            page->addWidget(binding);
        }
    }
}

static void Hu_MenuDrawControlsPage(Page const & /*page*/, Vector2i const & /*offset*/)
{
    Vector2i origin(SCREENWIDTH / 2, (SCREENHEIGHT / 2) + ((SCREENHEIGHT / 2 - 5) / cfg.common.menuScale));
    Hu_MenuDrawPageHelp("Select to assign new, [Del] to clear", origin);
}

void Hu_MenuControlGrabDrawer(char const *niceName, float alpha)
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTA));
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetColorAndAlpha(cfg.common.menuTextColors[1][CR], cfg.common.menuTextColors[1][CG], cfg.common.menuTextColors[1][CB], alpha);
    FR_DrawTextXY3("Press key or move controller for", SCREENWIDTH/2, SCREENHEIGHT/2-2, ALIGN_BOTTOM, Hu_MenuMergeEffectWithDrawTextFlags(DTF_ONLY_SHADOW));

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.common.menuTextColors[2][CR], cfg.common.menuTextColors[2][CG], cfg.common.menuTextColors[2][CB], alpha);
    FR_DrawTextXY3(niceName, SCREENWIDTH/2, SCREENHEIGHT/2+2, ALIGN_TOP, Hu_MenuMergeEffectWithDrawTextFlags(DTF_ONLY_SHADOW));

    DGL_Disable(DGL_TEXTURE_2D);
}

} // namespace menu
} // namespace common

