/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * m_ctrl.c: Common controls menu.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <ctype.h>

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
#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

#define NUM_CONTROLS_ITEMS      0

// Control config flags.
#define CCF_NON_INVERSE         0x1
#define CCF_INVERSE             0x2
#define CCF_STAGED              0x4
#define CCF_REPEAT              0x8
#define CCF_SIDESTEP_MODIFIER   0x10

#define BIND_GAP                2
#define SMALL_SCALE             .75f

// Binding iteration flags for M_IterateBindings().
#define MIBF_IGNORE_REPEATS     0x1

// TYPES -------------------------------------------------------------------

typedef enum {
    MIBT_KEY,
    MIBT_MOUSE,
    MIBT_JOY
} bindingitertype_t;

/** Menu items in the Controls menu are created based on this data. */
typedef struct controlconfig_s {
    const char*     itemText;
    const char*     bindContext;
    const char*     controlName;
    const char*     command;
    int             flags;

    // Automatically set:
    menuitem_t*     item;
} controlconfig_t;

typedef struct bindingdrawerdata_s {
    int             x;
    int             y;
} bindingdrawerdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void M_DrawControlsMenu(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void M_IterateBindings(controlconfig_t* cc, const char* bindings, int flags, void* data,
                       void (*callback)(bindingitertype_t type, int bid, const char* event,
                                        boolean isInverse, void *data));

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

static menuitem_t* ControlsItems;

#if __JDOOM__ || __JDOOM64__
menu_t ControlsDef = {
    MNF_NOHOTKEYS | MNF_DELETEFUNC,
    32, 40,
    M_DrawControlsMenu,
    0, NULL,
    1, MENU_OPTIONS,
    GF_FONTA,                    //1, 0, 0,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 17, { 17, 40 }
};
#endif

#ifdef __JHERETIC__
menu_t ControlsDef = {
    MNF_NOHOTKEYS | MNF_DELETEFUNC,
    32, 26,
    M_DrawControlsMenu,
    0, NULL,
    1, MENU_OPTIONS,
    GF_FONTA,                    //1, 0, 0,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 15, { 15, 26 }
};
#endif

#ifdef __JHEXEN__
menu_t ControlsDef = {
    MNF_NOHOTKEYS | MNF_DELETEFUNC,
    32, 21,
    M_DrawControlsMenu,
    0, NULL,
    1, MENU_OPTIONS,
    GF_FONTA,                    //1, 0, 0,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, 16, { 16, 21 }
};
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

controlconfig_t* grabbing = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static controlconfig_t controlConfig[] =
{
    { "movement" },
    { "forward", 0, "walk", 0, CCF_NON_INVERSE },
    { "backward", 0, "walk", 0, CCF_INVERSE },
    { "strafe left", 0, "sidestep", 0, CCF_INVERSE },
    { "strafe right", 0, "sidestep", 0, CCF_NON_INVERSE },
    { "turn left", 0, "turn", 0, CCF_STAGED | CCF_INVERSE | CCF_SIDESTEP_MODIFIER },
    { "turn right", 0, "turn", 0, CCF_STAGED | CCF_NON_INVERSE | CCF_SIDESTEP_MODIFIER },
    { "jump", 0, 0, "impulse jump" },
    { "use", 0, 0, "impulse use" },
    { "fly up", 0, "zfly", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "fly down", 0, "zfly", 0, CCF_STAGED | CCF_INVERSE },
    { "fall to ground", 0, 0, "impulse falldown" },
    { "speed", 0, "speed" },
    { "strafe", 0, "strafe" },

    { NULL },

    { "looking" },
    { "look up", 0, "look", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "look down", 0, "look", 0, CCF_STAGED | CCF_INVERSE },
    { "look center", 0, 0, "impulse lookcenter" },

    { NULL },

    { "weapons" },
    { "attack/fire", 0, "attack" },
    { "next weapon", 0, 0, "impulse nextweapon" },
    { "previous weapon", 0, 0, "impulse prevweapon" },

#if __JDOOM__ || __JDOOM64__
    { "fist/chainsaw", 0, 0, "impulse weapon1" },
    { "chainsaw/fist", 0, 0, "impulse weapon8" },
    { "pistol", 0, 0, "impulse weapon2" },
    { "shotgun/super sg", 0, 0, "impulse weapon3" },
    { "super sg/shotgun", 0, 0, "impulse weapon9" },
    { "chaingun", 0, 0, "impulse weapon4" },
    { "rocket launcher", 0, 0, "impulse weapon5" },
    { "plasma rifle", 0, 0, "impulse weapon6" },
    { "bfg 9000", 0, 0, "impulse weapon7" },
#endif
#if __JDOOM64__
    { "unmaker", 0, 0, "impulse weapon10" },
#endif

#if __JHERETIC__
    { "staff/gauntlets", 0, 0, "impulse weapon1" },
    { "elvenwand", 0, 0, "impulse weapon2" },
    { "crossbow", 0, 0, "impulse weapon3" },
    { "dragon claw", 0, 0, "impulse weapon4" },
    { "hellstaff", 0, 0, "impulse weapon5" },
    { "phoenix rod", 0, 0, "impulse weapon6" },
    { "firemace", 0, 0, "impulse weapon7" },
#endif

#if __JHEXEN__
    { "weapon 1", 0, 0, "impulse weapon1" },
    { "weapon 2", 0, 0, "impulse weapon2" },
    { "weapon 3", 0, 0, "impulse weapon3" },
    { "weapon 4", 0, 0, "impulse weapon4" },
#endif

#if __JHERETIC__ || __JHEXEN__
    { NULL },

    { "inventory" },
    { "move left", 0, 0, "impulse previtem", CCF_REPEAT },
    { "move right", 0, 0, "impulse nextitem", CCF_REPEAT },
    { "use item", 0, 0, "impulse useitem" },
    { "panic!", 0, 0, "impulse panic" },
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

    { NULL },

    { "chat" },
    { "open chat", 0, 0, "beginchat" },

#if __JDOOM__ || __JDOOM64__
    { "green chat", 0, 0, "beginchat 0" },
    { "indigo chat", 0, 0, "beginchat 1" },
    { "brown chat", 0, 0, "beginchat 2" },
    { "red chat", 0, 0, "beginchat 3" },
#endif

#if __JHERETIC__
    { "green chat", 0, 0, "beginchat 0" },
    { "yellow chat", 0, 0, "beginchat 1" },
    { "red chat", 0, 0, "beginchat 2" },
    { "blue chat", 0, 0, "beginchat 3" },
#endif

    { "send message", "chat", 0, "chatcomplete" },
    { "cancel message", "chat", 0, "chatcancel" },
    { "macro 1", "chat", 0, "chatsendmacro 0" },
    { "macro 2", "chat", 0, "chatsendmacro 1" },
    { "macro 3", "chat", 0, "chatsendmacro 2" },
    { "macro 4", "chat", 0, "chatsendmacro 3" },
    { "macro 5", "chat", 0, "chatsendmacro 4" },
    { "macro 6", "chat", 0, "chatsendmacro 5" },
    { "macro 7", "chat", 0, "chatsendmacro 6" },
    { "macro 8", "chat", 0, "chatsendmacro 7" },
    { "macro 9", "chat", 0, "chatsendmacro 8" },
    { "macro 10", "chat", 0, "chatsendmacro 9" },
    { "backspace", "chat", 0, "chatdelete" },

    { NULL },

    { "map" },
    { "show/hide map", 0, 0, "impulse automap" },
    { "zoom in", 0, "mapzoom", 0, CCF_NON_INVERSE },
    { "zoom out", 0, "mapzoom", 0, CCF_INVERSE },
    { "zoom maximum", "map", 0, "impulse zoommax" },
    { "pan left", 0, "mappanx", 0, CCF_INVERSE },
    { "pan right", 0, "mappanx", 0, CCF_NON_INVERSE },
    { "pan up", 0, "mappany", 0, CCF_NON_INVERSE },
    { "pan down", 0, "mappany", 0, CCF_INVERSE },
    { "toggle follow", "map", 0, "impulse follow" },
    { "toggle rotation", "map", 0, "impulse rotate" },
    { "add mark", "map", 0, "impulse addmark" },
    { "clear marks", "map", 0, "impulse clearmarks" },

    { NULL },

    { "hud" },
    { "show hud", 0, 0, "impulse showhud" },
    { "show score", 0, 0, "impulse showscore", CCF_REPEAT },
    { "smaller view", 0, 0, "sub view-size 1", CCF_REPEAT },
    { "larger view", 0, 0, "add view-size 1", CCF_REPEAT },

    { "message refresh", 0, 0, "impulse msgrefresh" },

    { NULL },

    { "shortcuts" },
    { "pause game", 0, 0, "pause" },
#if !__JDOOM64__
    { "help screen", "shortcut", 0, "helpscreen" },
#endif
    { "end game", "shortcut", 0, "endgame" },
    { "save game", "shortcut", 0, "savegame" },
    { "load game", "shortcut", 0, "loadgame" },
    { "quick save", "shortcut", 0, "quicksave" },
    { "quick load", "shortcut", 0, "quickload" },
    { "sound options", "shortcut", 0, "soundmenu" },
    { "toggle messages", "shortcut", 0, "togglemsgs" },
    { "gamma correction", "shortcut", 0, "togglegamma" },
    { "screenshot", "shortcut", 0, "screenshot" },
    { "quit", "shortcut", 0, "quit" },

    { NULL },

    { "menu" },
    { "show/hide menu", "shortcut", 0, "menu" },
    { "previous menu", "menu", 0, "menuback" },
    { "move up", "menu", 0, "menuup", CCF_REPEAT },
    { "move down", "menu", 0, "menudown", CCF_REPEAT },
    { "move left", "menu", 0, "menuleft", CCF_REPEAT },
    { "move right", "menu", 0, "menuright", CCF_REPEAT },
    { "select", "menu", 0, "menuselect" },

    { NULL },

    { "on-screen questions" },
    { "answer yes", "message", 0, "messageyes" },
    { "answer no", "message", 0, "messageno" },
    { "cancel", "message", 0, "messagecancel" },
};

// CODE --------------------------------------------------------------------

static void M_DeleteBinding(bindingitertype_t type, int bid, const char* name, boolean isInverse,
                            void* data)
{
    DD_Executef(true, "delbind %i", bid);
}

static void M_EFuncControlConfig(int option, void* data)
{
    controlconfig_t*    cc = data;
    char                buf[1024];

    if(option == -1)
    {
        if(cc->controlName)
        {
            B_BindingsForControl(0, cc->controlName, BFCI_BOTH, buf, sizeof(buf));
        }
        else
        {
            B_BindingsForCommand(cc->command, buf, sizeof(buf));
        }

        M_IterateBindings(cc, buf, 0, NULL, M_DeleteBinding);
    }
    else
    {
        // Start grabbing for this control.
        grabbing = cc;
        DD_SetInteger(DD_SYMBOLIC_ECHO, true);
    }
}

void M_InitControlsMenu(void)
{
    int                 i, count =
        sizeof(controlConfig) / sizeof(controlConfig[0]);


    VERBOSE( Con_Message("M_InitControlsMenu: Creating controls items.\n") );

    // Allocate the menu items array.
    ControlsItems = Z_Calloc(sizeof(menuitem_t) * count, PU_STATIC, 0);

    for(i = 0; i < count; ++i)
    {
        controlconfig_t* cc = &controlConfig[i];
        menuitem_t* item = &ControlsItems[i];

        cc->item = item;


        if(cc->itemText && ((unsigned int) cc->itemText < NUMTEXT))
        {
            item->text = GET_TXT((unsigned int)cc->itemText);
        }
        else
        {
            item->text = (char*) cc->itemText;
        }

        // Inert items.
        if(!cc->itemText || (!cc->command && !cc->controlName))
        {
            item->type = ITT_EMPTY;
        }
        else
        {
            item->type = ITT_EFUNC;
            item->func = M_EFuncControlConfig;
            item->data = cc;
        }
    }

    ControlsDef.items = ControlsItems;
    ControlsDef.itemCount = count;
}

static void M_DrawSmallText(int x, int y, const char* text)
{
    int                 height = M_StringHeight(text, GF_FONTA);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(x, y + height/2, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-x, -y - height/2, 0);

    M_WriteText2(x, y, text, GF_FONTA, 1, 1, 1, Hu_MenuAlpha());

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

static void M_DrawBinding(bindingitertype_t type, int bid, const char* name, boolean isInverse, void *data)
{
#if __JHERETIC__
    static const float  bgRGB[] = { 0, .5f, 0 };
#elif __JHEXEN__
    static const float  bgRGB[] = { .5f, 0, 0 };
#else
    static const float  bgRGB[] = { 0, 0, 0 };
#endif

    bindingdrawerdata_t* d = data;
    int                 width, height;

    if(type == MIBT_KEY)
    {
        width = M_StringWidth(name, GF_FONTA);
        height = M_StringHeight(name, GF_FONTA);

        DGL_SetNoMaterial();
        DGL_DrawRect(d->x, d->y, width*SMALL_SCALE + 2, height,
                     bgRGB[0], bgRGB[1], bgRGB[2], Hu_MenuAlpha() * .6f);

        M_DrawSmallText(d->x + 1, d->y, name);

        d->x += width * SMALL_SCALE + 2 + BIND_GAP;
    }
    else
    {
        char                temp[256];

        sprintf(temp, "%s%c%s", type == MIBT_MOUSE? "mouse" : "joy",
                isInverse? '-' : '+', name);

        width = M_StringWidth(temp, GF_FONTA);
        height = M_StringHeight(temp, GF_FONTA);

        M_DrawSmallText(d->x, d->y, temp);

        d->x += width * SMALL_SCALE + BIND_GAP;
    }
}

static const char* findInString(const char* str, const char* token, int n)
{
    int                 tokenLen = strlen(token);
    const char*         at = strstr(str, token);

    if(!at)
    {
        // Not there at all.
        return NULL;
    }

    if(at - str <= n - tokenLen)
    {
        return at;
    }

    // Past the end.
    return NULL;
}

void M_IterateBindings(controlconfig_t* cc, const char* bindings, int flags, void* data,
                       void (*callback)(bindingitertype_t type, int bid, const char* event,
                                        boolean isInverse, void *data))
{
    const char*         ptr = strchr(bindings, ':');
    const char*         begin, *end, *end2, *k, *bindingStart, *bindingEnd;
    int                 bid;
    char                buf[80], *b;
    boolean             isInverse;

    memset(buf, 0, sizeof(buf));

    while(ptr)
    {
        // Read the binding identifier.
        for(k = ptr; k > bindings && *k != '@'; --k);

        if(*k == '@')
        {
            for(begin = k - 1; begin > bindings && isdigit(*(begin - 1)); --begin);
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
                if(((cc->flags & CCF_INVERSE) && isInverse) ||
                   ((cc->flags & CCF_NON_INVERSE) && !isInverse) ||
                   !(cc->flags & (CCF_INVERSE | CCF_NON_INVERSE)))
                {
                    callback(!strncmp(ptr, "key", 3)? MIBT_KEY :
                             !strncmp(ptr, "mouse", 5)? MIBT_MOUSE : MIBT_JOY, bid, buf,
                             isInverse, data);
                }
            }
            else
            {
                if(!(cc->flags & (CCF_INVERSE | CCF_NON_INVERSE)) || (cc->flags & CCF_INVERSE))
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

/**
 * M_DrawControlsMenu
 */
void M_DrawControlsMenu(void)
{
#if __JHERETIC__ || __JHEXEN__
    patchid_t token;
#endif
    const menu_t* menu = &ControlsDef;
    const menuitem_t* item = menu->items + menu->firstItem;
    char buf[1024];
    int i;

#if __JDOOM__ || __JDOOM64__
    M_DrawTitle("CONTROLS", menu->y - 28);
    Hu_MenuPageString(buf, menu);
    M_WriteText3(160 - M_StringWidth(buf, GF_FONTA) / 2, menu->y - 12, buf,
                 GF_FONTA, 1, .7f, .3f, Hu_MenuAlpha(), true, true, 0);
#else
    M_WriteText3(120, 100 - 98/cfg.menuScale, "CONTROLS", GF_FONTB, cfg.menuColor[0],
                 cfg.menuColor[1], cfg.menuColor[2], Hu_MenuAlpha(), true, true, 0);

    DGL_Color4f(1, 1, 1, Hu_MenuAlpha());

    // Draw the page arrows.
    token = dpInvPageLeft[!menu->firstItem || (menuTime & 8)].id;
    Hu_DrawPatch(token, menu->x, menu->y - 12);
    token = dpInvPageRight[menu->firstItem + menu->numVisItems >= menu->itemCount || (menuTime & 8)].id;
    Hu_DrawPatch(token, 312 - menu->x, menu->y - 12);
#endif

    strcpy(buf, "Select to assign new, [Del] to clear");
    M_WriteText3(160 - M_StringWidth(buf, GF_FONTA) / 2,
                 100 + (95/cfg.menuScale) - M_StringHeight(buf, GF_FONTA), buf, GF_FONTA,
#if __JDOOM__
                 1, .7f, .3f,
#else
                 1, 1, 1,
#endif
                 Hu_MenuAlpha(), true, true, 0);

    for(i = 0; i < menu->numVisItems && menu->firstItem + i < menu->itemCount;
        i++, item++)
    {
        controlconfig_t* cc = item->data;
        bindingdrawerdata_t draw;

        if(item->type != ITT_EFUNC)
            continue;

        if(cc->controlName)
        {
            B_BindingsForControl(0, cc->controlName, BFCI_BOTH, buf, sizeof(buf));
        }
        else
        {
            B_BindingsForCommand(cc->command, buf, sizeof(buf));
        }
#if __JHEXEN__
        draw.x = menu->x + 154;
#else
        draw.x = menu->x + 134;
#endif
        draw.y = menu->y + (i * menu->itemHeight);
        M_IterateBindings(cc, buf, MIBF_IGNORE_REPEATS, &draw, M_DrawBinding);
    }
}

void M_ControlGrabDrawer(void)
{
    const char*         text;

    if(!grabbing)
        return;

    DGL_SetNoMaterial();
    DGL_DrawRect(0, 0, 320, 200, 0, 0, 0, .7f);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(160, 100, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-160, -100, 0);

    text = "press key or move controller for";
    M_WriteText2(160 - M_StringWidth(text, GF_FONTA)/2, 98 - M_StringHeight(text, GF_FONTA),
                 text, GF_FONTA, .75f, .75f, .75f, 1);
    M_WriteText2(160 - M_StringWidth(grabbing->item->text, GF_FONTB)/2,
                 102, grabbing->item->text, GF_FONTB, 1, 1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

int M_ControlsPrivilegedResponder(event_t* ev)
{
    // We're interested in key or button down events.
    if(grabbing && ev->type == EV_SYMBOLIC)
    {
        char            cmd[512];
        const char*     symbol = 0;
        const char*     bindContext = "game";

        if(sizeof(const char*) == sizeof(ev->data1)) // 32-bit
        {
            symbol = (const char*) ev->data1;
        }
        else // 64-bit
        {
            symbol = (const char*)(((int64_t)ev->data1) | (((int64_t)ev->data2)) << 32);
        }

        if(strncmp(symbol, "echo-", 5))
        {
            return false;
        }
        if(!strncmp(symbol, "echo-key-", 9) && strcmp(symbol + strlen(symbol) - 5, "-down"))
        {
           return false;
        }

        if(grabbing->bindContext)
        {
            bindContext = grabbing->bindContext;
        }

        if(grabbing->command)
        {
            sprintf(cmd, "bindevent {%s:%s} {%s}", bindContext, &symbol[5], grabbing->command);

            // Check for repeats.
            if(grabbing->flags & CCF_REPEAT)
            {
                const char*         downPtr = 0;
                char                temp[256];

                downPtr = strstr(symbol + 5, "-down");
                if(downPtr)
                {
                    char                temp2[256];

                    memset(temp2, 0, sizeof(temp2));
                    strncpy(temp2, symbol + 5, downPtr - symbol - 5);
                    sprintf(temp, "; bindevent {%s:%s-repeat} {%s}", bindContext, temp2,
                            grabbing->command);
                    strcat(cmd, temp);
                }
            }
        }
        else if(grabbing->controlName)
        {   // Have to exclude the state part.
            boolean             inv = (grabbing->flags & CCF_INVERSE) != 0;
            boolean             isStaged = (grabbing->flags & CCF_STAGED) != 0;
            char                temp3[256];
            char                extra[256];
            const char*         end = strchr(symbol + 5, '-');

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
            if(grabbing->flags & CCF_SIDESTEP_MODIFIER)
            {
                sprintf(cmd, "bindcontrol sidestep {%s + modifier-1-down}", temp3);
                DD_Execute(true, cmd);

                strcpy(extra, " + modifier-1-up");
            }

            sprintf(cmd, "bindcontrol {%s} {%s%s}", grabbing->controlName, temp3, extra);
        }

        VERBOSE( Con_Message("M_ControlsPrivilegedResponder: %s\n", cmd) );
        DD_Execute(true, cmd);

        /*
        // We shall issue a silent console command, but first we need
        // a textual representation of the ev.
        B_FormEventString(evname, ev->type, ev->state, ev->data1);

        // If this binding already exists, remove it.
        sprintf(cmd, "%s%s", grabbing->flags & CLF_ACTION ? "+" : "",
                grabbing->command);

        memset(buff, 0, sizeof(buff));

        // Check for bindings in this class only?
        if(B_BindingsForCommand(cmd, buff, grabbing->bindContext, false))
            if(findtoken(buff, evname, " "))    // Get rid of it?
            {
                del = true;
                strcpy(buff, "");
            }

        if(!del)
            sprintf(buff, "\"%s\"", grabbing->command);

        sprintf(cmd, "%s bdc%d %s %s",
                grabbing->flags & CLF_REPEAT ? "bindr" : "bind",
                grabbing->bindContext, evname + 1, buff);

        DD_Execute(false, cmd);
         */

        // We've finished the grab.
        grabbing = 0;
        DD_SetInteger(DD_SYMBOLIC_ECHO, false);
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        return true;
    }

    return false;
}
