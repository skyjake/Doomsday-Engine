/**\file m_ctrl.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_menu.h"
#include "m_ctrl.h"

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

typedef enum {
    MIBT_KEY,
    MIBT_MOUSE,
    MIBT_JOY
} bindingitertype_t;

typedef struct bindingdrawerdata_s {
    Point2Raw origin;
    float alpha;
} bindingdrawerdata_t;

static mndata_bindings_t controlConfig[] =
{
    { "Movement" },
    { "Forward", 0, "walk", 0, CCF_NON_INVERSE },
    { "Backward", 0, "walk", 0, CCF_INVERSE },
    { "Strafe Left", 0, "sidestep", 0, CCF_INVERSE },
    { "Strafe Right", 0, "sidestep", 0, CCF_NON_INVERSE },
    { "Turn Left", 0, "turn", 0, CCF_STAGED | CCF_INVERSE | CCF_SIDESTEP_MODIFIER },
    { "Turn Right", 0, "turn", 0, CCF_STAGED | CCF_NON_INVERSE | CCF_SIDESTEP_MODIFIER },
    { "Jump", 0, 0, "impulse jump" },
    { "Use", 0, 0, "impulse use" },
    { "Fly Up", 0, "zfly", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "Fly Down", 0, "zfly", 0, CCF_STAGED | CCF_INVERSE },
    { "Fall To Ground", 0, 0, "impulse falldown" },
    { "Speed", 0, "speed" },
    { "Strafe", 0, "strafe" },

    { "Looking" },
    { "Look Up", 0, "look", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "Look Down", 0, "look", 0, CCF_STAGED | CCF_INVERSE },
    { "Look Center", 0, 0, "impulse lookcenter" },

    { "Weapons" },
    { "Attack/Fire", 0, "attack" },
    { "Next Weapon", 0, 0, "impulse nextweapon" },
    { "Previous Weapon", 0, 0, "impulse prevweapon" },

#if __JDOOM__ || __JDOOM64__
    { "Fist/Chainsaw", 0, 0, "impulse weapon1" },
    { "Chainsaw/Fist", 0, 0, "impulse weapon8" },
    { "Pistol", 0, 0, "impulse weapon2" },
    { "Super SG/Shotgun", 0, 0, "impulse weapon3" },
    { "Shotgun/Super SG", 0, 0, "impulse weapon9" },
    { "Chaingun", 0, 0, "impulse weapon4" },
    { "Rocket Launcher", 0, 0, "impulse weapon5" },
    { "Plasma Rifle", 0, 0, "impulse weapon6" },
    { "BFG 9000", 0, 0, "impulse weapon7" },
#endif
#if __JDOOM64__
    { "Unmaker", 0, 0, "impulse weapon10" },
#endif

#if __JHERETIC__
    { "Gauntlets/Staff", 0, 0, "impulse weapon1" },
    { "Elvenwand", 0, 0, "impulse weapon2" },
    { "Crossbow", 0, 0, "impulse weapon3" },
    { "Dragon Claw", 0, 0, "impulse weapon4" },
    { "Hellstaff", 0, 0, "impulse weapon5" },
    { "Phoenix Rod", 0, 0, "impulse weapon6" },
    { "Firemace", 0, 0, "impulse weapon7" },
#endif

#if __JHEXEN__
    { "Weapon 1", 0, 0, "impulse weapon1" },
    { "Weapon 2", 0, 0, "impulse weapon2" },
    { "Weapon 3", 0, 0, "impulse weapon3" },
    { "Weapon 4", 0, 0, "impulse weapon4" },
#endif

#if __JHERETIC__ || __JHEXEN__
    { "Inventory" },
    { "Move Left", 0, 0, "impulse previtem", CCF_REPEAT },
    { "Move Right", 0, 0, "impulse nextitem", CCF_REPEAT },
    { "Use Item", 0, 0, "impulse useitem" },
    { "Panic!", 0, 0, "impulse panic" },
#endif

#ifdef __JHERETIC__
    { (const char*) TXT_TXT_INV_INVULNERABILITY, 0, 0, "impulse invulnerability" },
    { (const char*) TXT_TXT_INV_INVISIBILITY, 0, 0, "impulse invisibility" },
    { (const char*) TXT_TXT_INV_HEALTH, 0, 0, "impulse health" },
    { (const char*) TXT_TXT_INV_SUPERHEALTH, 0, 0, "impulse superhealth" },
    { (const char*) TXT_TXT_INV_TOMEOFPOWER, 0, 0, "impulse tome" },
    { (const char*) TXT_TXT_INV_TORCH, 0, 0, "impulse torch" },
    { (const char*) TXT_TXT_INV_FIREBOMB, 0, 0, "impulse firebomb" },
    { (const char*) TXT_TXT_INV_EGG, 0, 0, "impulse egg" },
    { (const char*) TXT_TXT_INV_FLY, 0, 0, "impulse fly" },
    { (const char*) TXT_TXT_INV_TELEPORT, 0, 0, "impulse teleport" },
#endif

#ifdef __JHEXEN__
    { (const char*) TXT_TXT_INV_TORCH, 0, 0, "impulse torch" },
    { (const char*) TXT_TXT_INV_HEALTH, 0, 0, "impulse health" },
    { (const char*) TXT_TXT_INV_SUPERHEALTH, 0, 0, "impulse mysticurn" },
    { (const char*) TXT_TXT_INV_BOOSTMANA, 0, 0, "impulse krater" },
    { (const char*) TXT_TXT_INV_SPEED, 0, 0, "impulse speedboots" },
    { (const char*) TXT_TXT_INV_BLASTRADIUS, 0, 0, "impulse blast" },
    { (const char*) TXT_TXT_INV_TELEPORT, 0, 0, "impulse teleport" },
    { (const char*) TXT_TXT_INV_TELEPORTOTHER, 0, 0, "impulse teleportother" },
    { (const char*) TXT_TXT_INV_POISONBAG, 0, 0, "impulse poisonbag" },
    { (const char*) TXT_TXT_INV_INVULNERABILITY, 0, 0, "impulse invulnerability" },
    { (const char*) TXT_TXT_INV_SUMMON, 0, 0, "impulse darkservant" },
    { (const char*) TXT_TXT_INV_EGG, 0, 0, "impulse egg" },
#endif

    { "Chat" },
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

    { "Send Message", "chat", 0, "chatcomplete" },
    { "Cancel Message", "chat", 0, "chatcancel" },
    { "Macro 1", "chat", 0, "chatsendmacro 0" },
    { "Macro 2", "chat", 0, "chatsendmacro 1" },
    { "Macro 3", "chat", 0, "chatsendmacro 2" },
    { "Macro 4", "chat", 0, "chatsendmacro 3" },
    { "Macro 5", "chat", 0, "chatsendmacro 4" },
    { "Macro 6", "chat", 0, "chatsendmacro 5" },
    { "Macro 7", "chat", 0, "chatsendmacro 6" },
    { "Macro 8", "chat", 0, "chatsendmacro 7" },
    { "Macro 9", "chat", 0, "chatsendmacro 8" },
    { "Macro 10", "chat", 0, "chatsendmacro 9" },
    { "Backspace", "chat", 0, "chatdelete", CCF_REPEAT },

    { "Map" },
    { "Show/Hide Map", 0, 0, "impulse automap" },
    { "Zoom In", 0, "mapzoom", 0, CCF_NON_INVERSE },
    { "Zoom Out", 0, "mapzoom", 0, CCF_INVERSE },
    { "Zoom Maximum", "map", 0, "impulse zoommax" },
    { "Pan Left", 0, "mappanx", 0, CCF_INVERSE },
    { "Pan Right", 0, "mappanx", 0, CCF_NON_INVERSE },
    { "Pan Up", 0, "mappany", 0, CCF_NON_INVERSE },
    { "Pan Down", 0, "mappany", 0, CCF_INVERSE },
    { "Toggle Follow", "map", 0, "impulse follow" },
    { "Toggle Rotation", "map", 0, "impulse rotate" },
    { "Add Mark", "map", 0, "impulse addmark" },
    { "Clear Marks", "map", 0, "impulse clearmarks" },

    { "HUD" },
    { "Show HUD", 0, 0, "impulse showhud" },
    { "Show Score", 0, 0, "impulse showscore", CCF_REPEAT },
    { "Smaller View", 0, 0, "sub view-size 1", CCF_REPEAT },
    { "Larger View", 0, 0, "add view-size 1", CCF_REPEAT },

    { "Message Refresh", 0, 0, "impulse msgrefresh" },

    { "Shortcuts" },
    { "Pause Game", 0, 0, "pause" },
#if !__JDOOM64__
    { "Help Screen", "shortcut", 0, "helpscreen" },
#endif
    { "End Game", "shortcut", 0, "endgame" },
    { "Save Game", "shortcut", 0, "savegame" },
    { "Load Game", "shortcut", 0, "loadgame" },
    { "Quick Save", "shortcut", 0, "quicksave" },
    { "Quick Load", "shortcut", 0, "quickload" },
    { "Sound Options", "shortcut", 0, "menu soundoptions" },
    { "Toggle Messages", "shortcut", 0, "toggle msg-show" },
    { "Gamma Correction", "shortcut", 0, "togglegamma" },
    { "Screenshot", "shortcut", 0, "screenshot" },
    { "Quit", "shortcut", 0, "quit" },

    { "Menu" },
    { "Show/Hide Menu", "shortcut", 0, "menu" },
    { "Previous Menu", "menu", 0, "menuback", CCF_REPEAT },
    { "Move Up", "menu", 0, "menuup", CCF_REPEAT },
    { "Move Down", "menu", 0, "menudown", CCF_REPEAT },
    { "Move Left", "menu", 0, "menuleft", CCF_REPEAT },
    { "Move Right", "menu", 0, "menuright", CCF_REPEAT },
    { "Select", "menu", 0, "menuselect" },

    { "On-Screen Questions" },
    { "Answer Yes", "message", 0, "messageyes" },
    { "Answer No", "message", 0, "messageno" },
    { "Cancel", "message", 0, "messagecancel" },
};

static void deleteBinding(bindingitertype_t type, int bid, const char* name, boolean isInverse, void* data)
{
    DD_Executef(true, "delbind %i", bid);
}

int Hu_MenuActivateBindingsGrab(mn_object_t* obj, mn_actionid_t action, void* paramaters)
{
     // Start grabbing for this control.
    DD_SetInteger(DD_SYMBOLIC_ECHO, true);
    return 0;
}

void Hu_MenuInitControlsPage(void)
{
#if __JDOOM__ || __JDOOM64__
    const Point2Raw pageOrigin = { 32, 40 };
#elif __JHERETIC__
    const Point2Raw pageOrigin = { 32, 40 };
#elif __JHEXEN__
    const Point2Raw pageOrigin = { 32, 40 };
#endif
    int i, textCount, bindingsCount, totalItems, group;
    int configCount = sizeof(controlConfig) / sizeof(controlConfig[0]);
    size_t objectIdx, textIdx;
    mn_object_t* objects;
    mndata_text_t* texts;
    mn_page_t* page;

    VERBOSE( Con_Message("Hu_MenuInitControlsPage: Creating controls items.") )

    textCount = 0;
    bindingsCount = 0;
    for(i = 0; i < configCount; ++i)
    {
        mndata_bindings_t* binds = &controlConfig[i];
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

    // Allocate the menu items array.
    totalItems = textCount + bindingsCount + 1/*terminator*/;
    objects = (mn_object_t*)Z_Calloc(sizeof(*objects) * totalItems, PU_GAMESTATIC, 0);
    if(!objects)
        Con_Error("Hu_MenuInitControlsPage: Failed on allocation of %lu bytes for items array.",
            (unsigned long) (sizeof(*objects) * totalItems));

    texts = (mndata_text_t*)Z_Calloc(sizeof(*texts) * textCount, PU_GAMESTATIC, 0);
    if(!texts)
        Con_Error("Hu_MenuInitControlsPage: Failed on allocation of %lu bytes for texts array.",
            (unsigned long) (sizeof(*texts) * textCount));

    objectIdx = 0;
    textIdx = 0;
    group = 0;
    for(i = 0; i < configCount; ++i)
    {
        mndata_bindings_t* binds = &controlConfig[i];

        if(!binds->command && !binds->controlName)
        {
            // Inert.
            mn_object_t* ob    = &objects[objectIdx++];
            mndata_text_t* txt = &texts[textIdx++];

            ob->_type = MN_TEXT;
            txt->text = binds->text;
            ob->_typedata = txt;
            ob->_pageFontIdx = MENU_FONT1;
            ob->_pageColorIdx = MENU_COLOR2;
            ob->ticker = MNText_Ticker;
            ob->drawer = MNText_Drawer;
            ob->updateGeometry = MNText_UpdateGeometry;

            // A new group begins;
            ob->_group = ++group;
        }
        else
        {
            mn_object_t* labelOb    = &objects[objectIdx++];
            mn_object_t* bindingsOb = &objects[objectIdx++];
            mndata_text_t* txt = &texts[textIdx++];

            labelOb->_type = MN_TEXT;
            txt->text = binds->text;
            labelOb->_typedata = txt;
            labelOb->ticker = MNText_Ticker;
            labelOb->drawer = MNText_Drawer;
            labelOb->updateGeometry = MNText_UpdateGeometry;
            labelOb->_pageFontIdx = MENU_FONT1;
            labelOb->_pageColorIdx = MENU_COLOR1;
            labelOb->_group = group;

            bindingsOb->_type = MN_BINDINGS;
            bindingsOb->ticker = MNBindings_Ticker;
            bindingsOb->drawer = MNBindings_Drawer;
            bindingsOb->cmdResponder = MNBindings_CommandResponder;
            bindingsOb->privilegedResponder = MNBindings_PrivilegedResponder;
            bindingsOb->updateGeometry = MNBindings_UpdateGeometry;
            bindingsOb->actions[MNA_ACTIVE].callback = Hu_MenuActivateBindingsGrab;
            bindingsOb->actions[MNA_FOCUS].callback = Hu_MenuDefaultFocusAction;
            bindingsOb->_typedata = binds;
            bindingsOb->_group = group;
        }
    }
    objects[objectIdx]._type = MN_NONE; // Terminate.

    page = Hu_MenuNewPage("ControlOptions", &pageOrigin, 0, Hu_MenuPageTicker, Hu_MenuDrawControlsPage, NULL, NULL);
    page->objects = objects;
    MNPage_SetTitle(page, "Controls");
    MNPage_SetPredefinedFont(page, MENU_FONT1, FID(GF_FONTA));
    MNPage_SetPreviousPage(page, Hu_MenuFindPageByName("Options"));
}

static void drawSmallText(const char* string, int x, int y, float alpha)
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

static void drawBinding(bindingitertype_t type, int bid, const char* name,
    boolean isInverse, void *data)
{
#define BIND_GAP                (2)

#if __JHERETIC__
    static const float bgRGB[] = { 0, .5f, 0 };
#elif __JHEXEN__
    static const float bgRGB[] = { .5f, 0, 0 };
#else
    static const float bgRGB[] = { 0, 0, 0 };
#endif

    bindingdrawerdata_t* d = data;
    int width, height;

    FR_SetFont(FID(GF_FONTA));

    if(type == MIBT_KEY)
    {
        width = FR_TextWidth(name);
        height = FR_TextHeight(name);

        DGL_SetNoMaterial();
        DGL_DrawRectf2Color(d->origin.x, d->origin.y, width*SMALL_SCALE + 2, height, bgRGB[0], bgRGB[1], bgRGB[2], d->alpha * .6f);

        DGL_Enable(DGL_TEXTURE_2D);
        drawSmallText(name, d->origin.x + 1, d->origin.y, d->alpha);
        DGL_Disable(DGL_TEXTURE_2D);

        d->origin.x += width * SMALL_SCALE + 2 + BIND_GAP;
    }
    else
    {
        char temp[256];

        sprintf(temp, "%s%c%s", type == MIBT_MOUSE? "mouse" : "joy", isInverse? '-' : '+', name);

        width = FR_TextWidth(temp);
        height = FR_TextHeight(temp);

        DGL_Enable(DGL_TEXTURE_2D);
        drawSmallText(temp, d->origin.x, d->origin.y, d->alpha);
        DGL_Disable(DGL_TEXTURE_2D);

        d->origin.x += width * SMALL_SCALE + BIND_GAP;
    }

#undef BIND_GAP
}

static const char* findInString(const char* str, const char* token, int n)
{
    int tokenLen = strlen(token);
    const char* at = strstr(str, token);

    if(!at)
    {   // Not there at all.
        return NULL;
    }

    if(at - str <= n - tokenLen)
    {
        return at;
    }

    // Past the end.
    return NULL;
}

static void iterateBindings(const mndata_bindings_t* binds, const char* bindings, int flags, void* data,
    void (*callback)(bindingitertype_t type, int bid, const char* ev, boolean isInverse, void *data))
{
    const char* ptr = strchr(bindings, ':');
    const char* begin, *end, *end2, *k, *bindingStart, *bindingEnd;
    char buf[80], *b;
    boolean isInverse;
    int bid;
    assert(binds);

    memset(buf, 0, sizeof(buf));

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
        while(*ptr == ' ')
            ptr++;

        ptr = strchr(ptr, ':');
    }
}

mn_object_t* MNBindings_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNBindings::New: Failed on allocation of %lu bytes for new MNBindings.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_bindings_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNBindings::New: Failed on allocation of %lu bytes for mndata_bindings_t.", (unsigned long) sizeof(mndata_bindings_t));

    ob->_type = MN_BINDINGS;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->updateGeometry = MNBindings_UpdateGeometry;
    ob->drawer = MNBindings_Drawer;
    ob->cmdResponder = MNBindings_CommandResponder;
    ob->privilegedResponder = MNBindings_PrivilegedResponder;

    return ob;
}

void MNBindings_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_BINDINGS);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNBindings_Drawer(mn_object_t* obj, const Point2Raw* origin)
{
    mndata_bindings_t* binds = (mndata_bindings_t*)obj->_typedata;
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

int MNBindings_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    mndata_bindings_t* binds = (mndata_bindings_t*)obj->_typedata;
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
        return true;
      }
    case MCMD_SELECT:
        S_LocalSound(SFX_MENU_CYCLE, NULL);
        obj->_flags |= MNF_ACTIVE;
        if(MNObject_HasAction(obj, MNA_ACTIVE))
        {
            MNObject_ExecAction(obj, MNA_ACTIVE, NULL);
            return true;
        }
        break;
    default:
        break;
    }
    return false; // Not eaten.
}

void MNBindings_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    // @todo calculate visible dimensions properly!
    assert(obj);
    Rect_SetWidthHeight(obj->_geometry, 60, 10 * SMALL_SCALE);
}

/**
 * Hu_MenuDrawControlsPage
 */
void Hu_MenuDrawControlsPage(mn_page_t* page, const Point2Raw* offset)
{
    Point2Raw origin;
    origin.x = SCREENWIDTH/2;
    origin.y = (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale);
    Hu_MenuDrawPageHelp("Select to assign new, [Del] to clear", origin.x, origin.y);
}

void Hu_MenuControlGrabDrawer(const char* niceName, float alpha)
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

void MNBindings_Ticker(mn_object_t* ob)
{
    mndata_bindings_t* binds = (mndata_bindings_t*) ob->_typedata;
    assert(ob && ob->_type == MN_BINDINGS);

    // Stub.
}

int MNBindings_PrivilegedResponder(mn_object_t* obj, event_t* ev)
{
    assert(obj && ev);
    // We're interested in key or button down events.
    if((obj->_flags & MNF_ACTIVE) && ev->type == EV_SYMBOLIC)
    {
        mndata_bindings_t* binds = (mndata_bindings_t*) obj->_typedata;
        const char* bindContext = "game";
        const char* symbol = 0;
        char cmd[512];

#ifndef __64BIT__
        symbol = (const char*) ev->data1;
#else
        symbol = (const char*)(((int64_t)ev->data1) | (((int64_t)ev->data2)) << 32);
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
                Con_Message("The Delete key in the Menu context is reserved for deleting bindings.");
                return false;
            }
        }

        if(binds->command)
        {
            const char* extraCondition = (binds->flags & CCF_MULTIPLAYER? " + multiplayer" : "");
            sprintf(cmd, "bindevent {%s:%s%s} {%s}", bindContext, &symbol[5], extraCondition, binds->command);

            // Check for repeats.
            if(binds->flags & CCF_REPEAT)
            {
                const char* downPtr = 0;
                char temp[256];

                downPtr = strstr(symbol + 5, "-down");
                if(downPtr)
                {
                    char temp2[256];

                    memset(temp2, 0, sizeof(temp2));
                    strncpy(temp2, symbol + 5, downPtr - symbol - 5);
                    sprintf(temp, "; bindevent {%s:%s-repeat} {%s}", bindContext, temp2, binds->command);
                    strcat(cmd, temp);
                }
            }
        }
        else if(binds->controlName)
        {
            // Have to exclude the state part.
            boolean inv = (binds->flags & CCF_INVERSE) != 0;
            boolean isStaged = (binds->flags & CCF_STAGED) != 0;
            const char* end = strchr(symbol + 5, '-');
            char temp3[256];
            char extra[256];

            end = strchr(end + 1, '-');

            if(!end)
            {
                Con_Error("what! %s\n", symbol);
            }

            memset(temp3, 0, sizeof(temp3));
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

        VERBOSE( Con_Message("MNBindings_PrivilegedResponder: %s", cmd) );
        DD_Execute(true, cmd);

        // We've finished the grab.
        obj->_flags &= ~MNF_ACTIVE;
        DD_SetInteger(DD_SYMBOLIC_ECHO, false);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        return true;
    }

    return false;
}

const char* MNBindings_ControlName(mn_object_t* obj)
{
    mndata_bindings_t* binds = (mndata_bindings_t*) obj->_typedata;
    assert(obj);
    assert(binds);

    // Map to a text definition?
    if(PTR2INT(binds->text) > 0 && PTR2INT(binds->text) < NUMTEXT)
    {
        return GET_TXT(PTR2INT(binds->text));
    }

    return binds->text;
}
