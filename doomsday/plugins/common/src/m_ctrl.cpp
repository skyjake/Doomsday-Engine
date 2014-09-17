/** @file m_ctrl.cpp  Controls menu page and associated widgets.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "menu/page.h"
#include "menu/widgets/inputbindingwidget.h"
#include "menu/widgets/labelwidget.h"

using namespace de;

namespace common {
namespace menu {

// Control config flags.
#define CCF_NON_INVERSE         0x1
#define CCF_INVERSE             0x2
#define CCF_STAGED              0x4
#define CCF_REPEAT              0x8
#define CCF_SIDESTEP_MODIFIER   0x10
#define CCF_MULTIPLAYER         0x20

#define SMALL_SCALE             .75f

// Binding iteration flags
#define MIBF_IGNORE_REPEATS     0x1

enum bindingitertype_t
{
    MIBT_KEY,
    MIBT_MOUSE,
    MIBT_JOY
};

struct bindingdrawerdata_t
{
    Point2Raw origin;
    float alpha;
};

struct controlconfig_t
{
    char const *text;
    char const *bindContext;
    char const *controlName;
    char const *command;
    int flags;
};
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
    { (char const *) TXT_TXT_INV_INVULNERABILITY, 0, 0, "impulse invulnerability", 0 },
    { (char const *) TXT_TXT_INV_INVISIBILITY, 0, 0, "impulse invisibility", 0 },
    { (char const *) TXT_TXT_INV_HEALTH, 0, 0, "impulse health", 0 },
    { (char const *) TXT_TXT_INV_SUPERHEALTH, 0, 0, "impulse superhealth", 0 },
    { (char const *) TXT_TXT_INV_TOMEOFPOWER, 0, 0, "impulse tome", 0 },
    { (char const *) TXT_TXT_INV_TORCH, 0, 0, "impulse torch", 0 },
    { (char const *) TXT_TXT_INV_FIREBOMB, 0, 0, "impulse firebomb", 0 },
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

    { "Message Refresh", 0, 0, "impulse msgrefresh", 0 },

    { "Shortcuts", 0, 0, 0, 0 },
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
    { "Gamma Correction", "shortcut", 0, "togglegamma", 0 },
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

    { "Virtual Reality", 0, 0, 0, 0 },
    { "Reset Tracking", 0, 0, "resetriftpose", 0 }
};

static void deleteBinding(bindingitertype_t /*type*/, int bid, char const * /*name*/, dd_bool /*isInverse*/, void * /*data*/)
{
    DD_Executef(true, "delbind %i", bid);
}

void Hu_MenuActivateBindingsGrab(Widget * /*ob*/, Widget::mn_actionid_t /*action*/)
{
     // Start grabbing for this control.
    DD_SetInteger(DD_SYMBOLIC_ECHO, true);
}

void Hu_MenuInitControlsPage()
{
#if __JDOOM__ || __JDOOM64__
    Point2Raw const pageOrigin(32, 40);
#elif __JHERETIC__
    Point2Raw const pageOrigin(32, 40);
#elif __JHEXEN__
    Point2Raw const pageOrigin(32, 40);
#endif
    int configCount = sizeof(controlConfig) / sizeof(controlConfig[0]);

    LOGDEV_VERBOSE("Hu_MenuInitControlsPage: Creating controls items");

    int textCount = 0;
    int bindingsCount = 0;
    for(int i = 0; i < configCount; ++i)
    {
        controlconfig_t *binds = &controlConfig[i];
        if(!binds->command && !binds->controlName)
        {
            ++textCount;
        }
        else
        {
            ++textCount;
            ++bindingsCount;
        }
    }

    Page *page = Hu_MenuAddPage(new Page("ControlOptions", pageOrigin, 0, Hu_MenuDrawControlsPage));
    page->setTitle("Controls");
    page->setPredefinedFont(MENU_FONT1, FID(GF_FONTA));
    page->setPreviousPage(Hu_MenuPagePtr("Options"));

    int group = 0;
    for(int i = 0; i < configCount; ++i)
    {
        controlconfig_t *binds = &controlConfig[i];

        char const *labelText = binds->text;
        if(labelText && (PTR2INT(labelText) > 0 && PTR2INT(labelText) < NUMTEXT))
        {
            labelText = GET_TXT(PTR2INT(labelText));
        }

        if(!binds->command && !binds->controlName)
        {
            // Inert.
            LabelWidget *label = new LabelWidget(labelText);
            label->_pageColorIdx = MENU_COLOR2;

            // A new group begins.
            label->setGroup(++group);

            page->widgets() << label;
        }
        else
        {
            LabelWidget *label = new LabelWidget(labelText);
            label->setGroup(group);

            page->widgets() << label;

            InputBindingWidget *binding = new InputBindingWidget;
            binding->binds = binds;
            binding->setGroup(group);
            binding->actions[Widget::MNA_ACTIVE].callback = Hu_MenuActivateBindingsGrab;
            binding->actions[Widget::MNA_FOCUS ].callback = Hu_MenuDefaultFocusAction;

            page->widgets() << binding;
        }
    }
}

static void drawSmallText(char const *string, int x, int y, float alpha)
{
    int height = FR_TextHeight(string);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(x, y + height/2, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-x, -y - height/2, 0);

    FR_SetColorAndAlpha(1, 1, 1, alpha);
    FR_DrawTextXY3(string, x, y, ALIGN_TOPLEFT, DTF_NO_EFFECTS);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

static void drawBinding(bindingitertype_t type, int /*bid*/, const char *name,
    dd_bool isInverse, void *context)
{
#define BIND_GAP                (2)

#if __JHERETIC__
    static float const bgRGB[] = { 0, .5f, 0 };
#elif __JHEXEN__
    static float const bgRGB[] = { .5f, 0, 0 };
#else
    static float const bgRGB[] = { 0, 0, 0 };
#endif

    bindingdrawerdata_t *d = (bindingdrawerdata_t *)context;

    FR_SetFont(FID(GF_FONTA));

    if(type == MIBT_KEY)
    {
        int const width  = FR_TextWidth(name);
        int const height = FR_TextHeight(name);

        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(d->origin.x, d->origin.y, width*SMALL_SCALE + 2, height, bgRGB[0], bgRGB[1], bgRGB[2], d->alpha * .6f);

        DGL_Enable(DGL_TEXTURE_2D);
        drawSmallText(name, d->origin.x + 1, d->origin.y, d->alpha);
        DGL_Disable(DGL_TEXTURE_2D);

        d->origin.x += width * SMALL_SCALE + 2 + BIND_GAP;
    }
    else
    {
        char buf[256];
        sprintf(buf, "%s%c%s", type == MIBT_MOUSE? "mouse" : "joy", isInverse? '-' : '+', name);

        int const width  = FR_TextWidth(buf);
        ///int const height = FR_TextHeight(temp);

        DGL_Enable(DGL_TEXTURE_2D);
        drawSmallText(buf, d->origin.x, d->origin.y, d->alpha);
        DGL_Disable(DGL_TEXTURE_2D);

        d->origin.x += width * SMALL_SCALE + BIND_GAP;
    }

#undef BIND_GAP
}

static char const *findInString(char const *str, char const *token, int n)
{
    int tokenLen = strlen(token);
    char const *at = strstr(str, token);

    // Not there at all?
    if(!at) return 0;

    if(at - str <= n - tokenLen)
    {
        return at;
    }

    // Past the end.
    return 0;
}

static void iterateBindings(controlconfig_t const *binds, char const *bindings, int flags, void *data,
    void (*callback)(bindingitertype_t type, int bid, char const *ev, dd_bool isInverse, void *data))
{
    DENG2_ASSERT(binds != 0);

    char const *ptr = strchr(bindings, ':');
    char const *begin, *end, *end2, *k, *bindingStart, *bindingEnd;
    char buf[80], *b;
    dd_bool isInverse;
    int bid;

    std::memset(buf, 0, sizeof(buf));

    while(ptr)
    {
        // Read the binding identifier.
        for(k = ptr; k > bindings && *k != '@'; --k);

        if(*k == '@')
        {
            for(begin = k - 1; begin > bindings && isdigit(*(begin - 1)); --begin) {}
            bid = strtol(begin, NULL, 10);
        }
        else
        {
            // No identifier??
            bid = 0;
        }

        // Find the end of the entire binding.
        bindingStart = k + 1;
        bindingEnd = strchr(bindingStart, '@');
        if(!bindingEnd)
        {
            // Then point to the end of the string.
            bindingEnd = strchr(k + 1, 0);
        }

        ptr++;
        end = strchr(ptr, '-');
        if(!end)
            return;

        end++;
        b = buf;
        while(*end && *end != ' ' && *end != '-' && *end != '+')
        {
            *b++ = *end++;
        }
        *b = 0;

        end2 = strchr(end, ' ');
        if(!end2)
            end = end + strlen(end); // Then point to the end.
        else
            end = end2;

        if(!findInString(bindingStart, "modifier-1-down", bindingEnd - bindingStart) &&
           (!(flags & MIBF_IGNORE_REPEATS) || !findInString(ptr, "-repeat", end - ptr)))
        {
            isInverse = (findInString(ptr, "-inverse", end - ptr) != NULL);

            if(!strncmp(ptr, "key", 3) || strstr(ptr, "-button") ||
               !strncmp(ptr, "mouse-left", 10) || !strncmp(ptr, "mouse-middle", 12) ||
               !strncmp(ptr, "mouse-right", 11))
            {
                if(((binds->flags & CCF_INVERSE) && isInverse) ||
                   ((binds->flags & CCF_NON_INVERSE) && !isInverse) ||
                   !(binds->flags & (CCF_INVERSE | CCF_NON_INVERSE)))
                {
                    callback(!strncmp(ptr, "key", 3)? MIBT_KEY :
                             !strncmp(ptr, "mouse", 5)? MIBT_MOUSE : MIBT_JOY, bid, buf,
                             isInverse, data);
                }
            }
            else
            {
                if(!(binds->flags & (CCF_INVERSE | CCF_NON_INVERSE)) || (binds->flags & CCF_INVERSE))
                {
                    isInverse = !isInverse;
                }
                if(!strncmp(ptr, "joy", 3))
                {
                    callback(MIBT_JOY, bid, buf, isInverse, data);
                }
                else if(!strncmp(ptr, "mouse", 5))
                {
                    callback(MIBT_MOUSE, bid, buf, isInverse, data);
                }
            }
        }

        ptr = end;
        while(*ptr == ' ') { ptr++; }

        ptr = strchr(ptr, ':');
    }
}

InputBindingWidget::InputBindingWidget()
    : Widget()
    , binds(0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
}

void InputBindingWidget::draw(Point2Raw const *origin)
{
    bindingdrawerdata_t draw;
    char buf[1024];

    if(binds->controlName)
    {
        B_BindingsForControl(0, binds->controlName, BFCI_BOTH, buf, sizeof(buf));
    }
    else
    {
        B_BindingsForCommand(binds->command, buf, sizeof(buf));
    }
    draw.origin.x = origin->x;
    draw.origin.y = origin->y;
    draw.alpha = mnRendState->pageAlpha;
    iterateBindings(binds, buf, MIBF_IGNORE_REPEATS, &draw, drawBinding);
}

int InputBindingWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_DELETE: {
        char buf[1024];

        S_LocalSound(SFX_MENU_CANCEL, NULL);
        if(binds->controlName)
        {
            B_BindingsForControl(0, binds->controlName, BFCI_BOTH, buf, sizeof(buf));
        }
        else
        {
            B_BindingsForCommand(binds->command, buf, sizeof(buf));
        }

        iterateBindings(binds, buf, 0, NULL, deleteBinding);

        // If deleting the menuselect binding, automatically rebind it Return;
        // otherwise the user would be stuck without a way to make further bindings.
        if(binds->command && !strcmp(binds->command, "menuselect"))
        {
            DD_Execute(true, "bindevent menu:key-return menuselect");
        }
        return true; }

    case MCMD_SELECT:
        S_LocalSound(SFX_MENU_CYCLE, NULL);
        Widget::_flags |= MNF_ACTIVE;
        if(hasAction(MNA_ACTIVE))
        {
            execAction(MNA_ACTIVE);
            return true;
        }
        break;

    default: break;
    }

    return false; // Not eaten.
}

void InputBindingWidget::updateGeometry(Page * /*page*/)
{
    // @todo calculate visible dimensions properly!
    Rect_SetWidthHeight(_geometry, 60, 10 * SMALL_SCALE);
}

/**
 * Hu_MenuDrawControlsPage
 */
void Hu_MenuDrawControlsPage(Page * /*page*/, Point2Raw const * /*offset*/)
{
    Point2Raw origin;
    origin.x = SCREENWIDTH/2;
    origin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);
    Hu_MenuDrawPageHelp("Select to assign new, [Del] to clear", origin.x, origin.y);
}

void Hu_MenuControlGrabDrawer(char const *niceName, float alpha)
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTA));
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetColorAndAlpha(cfg.menuTextColors[1][CR], cfg.menuTextColors[1][CG], cfg.menuTextColors[1][CB], alpha);
    FR_DrawTextXY3("Press key or move controller for", SCREENWIDTH/2, SCREENHEIGHT/2-2, ALIGN_BOTTOM, MN_MergeMenuEffectWithDrawTextFlags(DTF_ONLY_SHADOW));

    FR_SetFont(FID(GF_FONTB));
    FR_SetColorAndAlpha(cfg.menuTextColors[2][CR], cfg.menuTextColors[2][CG], cfg.menuTextColors[2][CB], alpha);
    FR_DrawTextXY3(niceName, SCREENWIDTH/2, SCREENHEIGHT/2+2, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(DTF_ONLY_SHADOW));

    DGL_Disable(DGL_TEXTURE_2D);
}

int InputBindingWidget::handleEvent_Privileged(event_t *ev)
{
    DENG2_ASSERT(ev != 0);

    // We're interested in key or button down events.
    if((Widget::_flags & MNF_ACTIVE) && ev->type == EV_SYMBOLIC)
    {
        char const *bindContext = "game";
        char const *symbol = 0;
        char cmd[512];

#ifndef __64BIT__
        symbol = (char const *) ev->data1;
#else
        {
            uint64_t address = (uint32_t) ev->data2;
            address <<= 32;
            address |= (uint32_t) ev->data1;
            symbol = (char const *) address;
        }
#endif

        if(strncmp(symbol, "echo-", 5))
        {
            return false;
        }
        if(!strncmp(symbol, "echo-key-", 9) && strcmp(symbol + strlen(symbol) - 5, "-down"))
        {
           return false;
        }

        if(binds->bindContext)
        {
            bindContext = binds->bindContext;

            if((!strcmp(bindContext, "menu") || !strcmp(bindContext, "shortcut")) &&
               !strcmp(symbol + 5, "key-delete-down"))
            {
                throw Error("InputBindingWidget::handleEvent_Priviledged", "The Delete key in the Menu context is reserved for deleting bindings");
                return false;
            }
        }

        if(binds->command)
        {
            char const *extraCondition = (binds->flags & CCF_MULTIPLAYER? " + multiplayer" : "");
            sprintf(cmd, "bindevent {%s:%s%s} {%s}", bindContext, &symbol[5], extraCondition, binds->command);

            // Check for repeats.
            if(binds->flags & CCF_REPEAT)
            {
                char const *downPtr = 0;
                char temp[256];

                downPtr = strstr(symbol + 5, "-down");
                if(downPtr)
                {
                    char temp2[256];

                    std::memset(temp2, 0, sizeof(temp2));
                    strncpy(temp2, symbol + 5, downPtr - symbol - 5);
                    sprintf(temp, "; bindevent {%s:%s-repeat} {%s}", bindContext, temp2, binds->command);
                    strcat(cmd, temp);
                }
            }
        }
        else if(binds->controlName)
        {
            // Have to exclude the state part.
            dd_bool inv      = (binds->flags & CCF_INVERSE) != 0;
            dd_bool isStaged = (binds->flags & CCF_STAGED) != 0;
            char const *end = strchr(symbol + 5, '-');
            char temp3[256];
            char extra[256];

            end = strchr(end + 1, '-');

            if(!end)
            {
                Con_Error("what! %s\n", symbol);
            }

            std::memset(temp3, 0, sizeof(temp3));
            strncpy(temp3, symbol + 5, end - symbol - 5);

            // Check for inverse.
            if(!strncmp(end, "-neg", 4))
            {
                inv = !inv;
            }
            if(isStaged && (!strncmp(temp3, "key-", 4) || strstr(temp3, "-button") ||
                            !strcmp(temp3, "mouse-left") || !strcmp(temp3, "mouse-middle") ||
                            !strcmp(temp3, "mouse-right")))
            {
                // Staging is for keys and buttons.
                strcat(temp3, "-staged");
            }
            if(inv)
            {
                strcat(temp3, "-inverse");
            }

            strcpy(extra, "");
            if(binds->flags & CCF_SIDESTEP_MODIFIER)
            {
                sprintf(cmd, "bindcontrol sidestep {%s + modifier-1-down}", temp3);
                DD_Execute(true, cmd);

                strcpy(extra, " + modifier-1-up");
            }

            sprintf(cmd, "bindcontrol {%s} {%s%s}", binds->controlName, temp3, extra);
        }

        LOGDEV_INPUT_MSG("MNBindings_PrivilegedResponder: %s") << cmd;
        DD_Execute(true, cmd);

        // We've finished the grab.
        Widget::_flags &= ~MNF_ACTIVE;
        DD_SetInteger(DD_SYMBOLIC_ECHO, false);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        return true;
    }

    return false;
}

char const *InputBindingWidget::controlName()
{
    DENG2_ASSERT(binds != 0);
    // Map to a text definition?
    if(PTR2INT(binds->text) > 0 && PTR2INT(binds->text) < NUMTEXT)
    {
        return GET_TXT(PTR2INT(binds->text));
    }
    return binds->text;
}

} // namespace menu
} // namespace common

