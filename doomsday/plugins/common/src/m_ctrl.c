/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * Common controls menu
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#if __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

#define NUM_CONTROLS_ITEMS 0

// Control config flags.
#define CCF_NON_INVERSE  0x1
#define CCF_INVERSE     0x2
#define CCF_STAGED      0x4
#define CCF_REPEAT      0x8

#define BIND_GAP        2
#define SMALL_SCALE     .75f

// TYPES -------------------------------------------------------------------

/** Menu items in the Controls menu are created based on this data. */
typedef struct controlconfig_s {
    const char* itemText;
    const char* bindClass;
    const char* controlName;
    const char* command;
    int flags;
    // Automatically set:
    menuitem_t* item;
} controlconfig_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void M_DrawControlsMenu(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float    menuAlpha;
extern int      menusnds[];

static menuitem_t* ControlsItems;

#if __JDOOM__ || __JDOOM64__
menu_t ControlsDef = {
    MNF_NOHOTKEYS | MNF_DELETEFUNC,
    32, 40,
    M_DrawControlsMenu,
    0, NULL,
    1, MENU_OPTIONS,
    huFontA,                    //1, 0, 0,
    cfg.menuColor2,
    NULL,
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
    huFontA,                    //1, 0, 0,
    cfg.menuColor2,
    NULL,
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
    huFontA,                    //1, 0, 0,
    cfg.menuColor2,
    NULL,
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
    { "turn left", 0, "turn", 0, CCF_INVERSE },
    { "turn right", 0, "turn", 0, CCF_NON_INVERSE },
    { "jump", 0, 0, "impulse jump" },
    { "fly up", 0, "zfly", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "fly down", 0, "zfly", 0, CCF_STAGED | CCF_INVERSE },
    { "fall to ground", 0, 0, "impulse falldown" },
    { "speed", 0, "speed" },
    { "strafe", 0, "strafe" },

    { NULL },

    { "looking" },
    { "look up", 0, "look", 0, CCF_STAGED | CCF_INVERSE },
    { "look down", 0, "look", 0, CCF_STAGED | CCF_NON_INVERSE },
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

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    { NULL },

    { "inventory" },
    { "move left", 0, 0, "invleft" },
    { "move right", 0, 0, "invright" },
    { "use artifact", 0, 0, "impulse useartifact" },
    { "panic!", 0, 0, "impulse panic" },
#endif

#ifdef __JHERETIC__
    { (const char*) TXT_TXT_ARTIINVULNERABILITY, 0, 0, "impulse invulnerability" },
    { (const char*) TXT_TXT_ARTIINVISIBILITY, 0, 0, "impulse invisibility" },
    { (const char*) TXT_TXT_ARTIHEALTH, 0, 0, "impulse health" },
    { (const char*) TXT_TXT_ARTISUPERHEALTH, 0, 0, "impulse superhealth" },
    { (const char*) TXT_TXT_ARTITOMEOFPOWER, 0, 0, "impulse tome" },
    { (const char*) TXT_TXT_ARTITORCH, 0, 0, "impulse torch" },
    { (const char*) TXT_TXT_ARTIFIREBOMB, 0, 0, "impulse firebomb" },
    { (const char*) TXT_TXT_ARTIEGG, 0, 0, "impulse egg" },
    { (const char*) TXT_TXT_ARTIFLY, 0, 0, "impulse fly" },
    { (const char*) TXT_TXT_ARTITELEPORT, 0, 0, "impulse teleport" },
#endif

#ifdef __JHEXEN__
    { (const char*) TXT_TXT_ARTITORCH, 0, 0, "impulse torch" },
    { (const char*) TXT_TXT_ARTIHEALTH, 0, 0, "impulse health" },
    { (const char*) TXT_TXT_ARTISUPERHEALTH, 0, 0, "impulse mysticurn" },
    { (const char*) TXT_TXT_ARTIBOOSTMANA, 0, 0, "impulse krater" },
    { (const char*) TXT_TXT_ARTISPEED, 0, 0, "impulse speedboots" },
    { (const char*) TXT_TXT_ARTIBLASTRADIUS, 0, 0, "impulse blast" },
    { (const char*) TXT_TXT_ARTITELEPORT, 0, 0, "impulse teleport" },
    { (const char*) TXT_TXT_ARTITELEPORTOTHER, 0, 0, "impulse teleportother" },
    { (const char*) TXT_TXT_ARTIPOISONBAG, 0, 0, "impulse poisonbag" },
    { (const char*) TXT_TXT_ARTIINVULNERABILITY, 0, 0, "impulse invulnerability" },
    { (const char*) TXT_TXT_ARTISUMMON, 0, 0, "impulse darkservant" },
    { (const char*) TXT_TXT_ARTIEGG, 0, 0, "impulse egg" },
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
    { "message refresh", 0, 0, "msgrefresh" },
    
    { NULL },

    { "map" },
    { "show/hide map", 0, 0, "automap" },
    { "zoom in", 0, "mapzoom", 0, CCF_NON_INVERSE },
    { "zoom out", 0, "mapzoom", 0, CCF_INVERSE },
    { "zoom maximum", "map", 0, "zoommax" },
    { "pan left", 0, "mappanx", 0, CCF_INVERSE },
    { "pan right", 0, "mappanx", 0, CCF_NON_INVERSE },
    { "pan up", 0, "mappany", 0, CCF_NON_INVERSE },
    { "pan down", 0, "mappany", 0, CCF_INVERSE },
    { "toggle follow", "map", 0, "follow" },
    { "toggle rotation", "map", 0, "rotate" },
    { "add mark", "map", 0, "addmark" },
    { "clear marks", "map", 0, "clearmarks" },

    { NULL },

    { "hud" },
    { "show/hide hud", 0, 0, "showhud" },
    { "smaller view", 0, 0, "viewsize -" },
    { "larger view", 0, 0, "viewsize +" },

    { NULL },

    { "shortcuts" },
    { "pause game", 0, 0, "pause" },
#if !__JDOOM64__
    { "help screen", 0, 0, "helpscreen" },
#endif
    { "end game", 0, 0, "endgame" },
    { "save game", 0, 0, "savegame" },
    { "load game", 0, 0, "loadgame" },
    { "quick save", 0, 0, "quicksave" },
    { "quick load", 0, 0, "quickload" },
    { "sound options", 0, 0, "soundmenu" },
    { "toggle messages", 0, 0, "togglemsgs" },
    { "gamma correction", 0, 0, "togglegamma" },
    { "screenshot", 0, 0, "screenshot" },
    { "quit", 0, 0, "quit" },

    { NULL },

    { "menu" },
    { "show/hide menu", 0, 0, "menu" },
    { "previous menu", "menu", 0, "menucancel" },
    { "move up", "menu", 0, "menuup", CCF_REPEAT },
    { "move down", "menu", 0, "menudown", CCF_REPEAT },
    { "move left", "menu", 0, "menuleft", CCF_REPEAT },
    { "move right", "menu", 0, "menuright", CCF_REPEAT },
    { "select", "menu", 0, "menuselect", CCF_REPEAT },

    { NULL },

    { "on-screen questions" },
    { "answer yes", "message", 0, "messageyes" },
    { "answer no", "message", 0, "messageno" },
    { "cancel", "message", 0, "messagecancel" },
};

// CODE --------------------------------------------------------------------

static void M_EFuncControlConfig(int option, void *data)
{
    controlconfig_t* cc = data;

    if(option == -1)
    {
        // TODO: Delete!
        //Con_Message("Delete requested %s\n", cc->itemText);
                
    }
    else
    {
        // Start grabbing for this control.
        //Con_Message("Grabbing %s\n", cc->item->text);

        grabbing = cc;
        DD_SetInteger(DD_SYMBOLIC_ECHO, true);
    }
}

void M_InitControlsMenu(void)
{
    int count = sizeof(controlConfig) / sizeof(controlConfig[0]);
    int i;

    VERBOSE( Con_Message("M_InitControlsMenu: Creating controls items.\n") );

    // Allocate the menu items array.
    ControlsItems = Z_Calloc(sizeof(menuitem_t) * count, PU_STATIC, 0);

    for(i = 0; i < count; ++i)
    {
        controlconfig_t* cc = &controlConfig[i];
        menuitem_t* item = &ControlsItems[i];
        
        cc->item = item;

        if(cc->itemText && ((int) cc->itemText < NUMTEXT))
        {
            item->text = GET_TXT((int)cc->itemText);
        }
        else
        {
            item->text = (char*) cc->itemText;
        }

        // Inert items.
        if(!cc->itemText)
        {
            item->type = ITT_EMPTY;
        }
        else if(!cc->controlName && !cc->command)
        {
            item->type = ITT_INERT;
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
    int height = M_StringHeight(text, huFontA);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(x, y + height/2, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-x, -y - height/2, 0);

    M_WriteText2(x, y, text, huFontA, 1, 1, 1, menuAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void M_DrawKeyBinding(int* x, int y, const char* name)
{
    int width = M_StringWidth(name, huFontA);
    int height = M_StringHeight(name, huFontA);

    // TODO: Maybe some graphics here?
    GL_SetNoTexture();
    GL_DrawRect(*x, y, width*SMALL_SCALE + 2, height,
#if __JHERETIC__
                0, .5f, 0,
#elif __JHEXEN__
                .5f, 0, 0,
#else
                0, 0, 0,
#endif
                menuAlpha*.6f);

    M_DrawSmallText(*x + 1, y, name);

    *x += width*SMALL_SCALE + 2 + BIND_GAP;
}

void M_DrawJoyMouseBinding(const char* axis, int* x, int y, const char* name, boolean isInverse)
{
    int width;
    int height;
    char temp[256];

    sprintf(temp, "%s%c%s", axis, isInverse? '-' : '+', name);

    width = M_StringWidth(temp, huFontA);
    height = M_StringHeight(temp, huFontA);

    M_DrawSmallText(*x, y, temp);

    *x += width*SMALL_SCALE + BIND_GAP;
}

static const char* findInString(const char* str, const char* token, int n)
{
    int tokenLen = strlen(token);
    const char* at = strstr(str, token);

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

void M_DrawBindings(controlconfig_t* cc, int x, int y, const char* bindings)
{
    const char* ptr = strchr(bindings, ':'), *end, *end2;
    char buf[80], *b;
    boolean isInverse;

    memset(buf, 0, sizeof(buf));

    while(ptr)
    {
        ptr++;
        end = strchr(ptr, '-');
        if(!end) return;

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

        if(!findInString(ptr, "-repeat", end - ptr))
        {
            isInverse = (findInString(ptr, "-inverse", end - ptr) != NULL);

            if(!strncmp(ptr, "key", 3))
            {
                if((cc->flags & CCF_INVERSE) && isInverse ||
                   (cc->flags & CCF_NON_INVERSE) && !isInverse ||
                   !(cc->flags & (CCF_INVERSE | CCF_NON_INVERSE)))
                {
                    M_DrawKeyBinding(&x, y, buf);
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
                    M_DrawJoyMouseBinding("joy", &x, y, buf, isInverse);
                }
                else if(!strncmp(ptr, "mouse", 5))
                {
                    M_DrawJoyMouseBinding("mouse", &x, y, buf, isInverse);
                }
            }
        }

        ptr = end;
        while(*ptr == ' ') ptr++;

        ptr = strchr(ptr, ':');
    }
}

/*
 * M_DrawControlsMenu
 */
void M_DrawControlsMenu(void)
{
    int     i;
    char    controlCmd[80];
    char    buf[1024], *token;
    const char *bc;
    const menu_t *menu = &ControlsDef;
    const menuitem_t *item = menu->items + menu->firstItem;

#if __JDOOM__ || __JDOOM64__
    M_DrawTitle("CONTROLS", menu->y - 28);
    sprintf(buf, "PAGE %i/%i", menu->firstItem / menu->numVisItems + 1,
            menu->itemCount / menu->numVisItems + 1);
    M_WriteText2(160 - M_StringWidth(buf, huFontA) / 2, menu->y - 12, buf,
                 huFontA, 1, .7f, .3f, menuAlpha);
#else
    M_WriteText2(120, 100 - 98/cfg.menuScale, "CONTROLS", huFontB, cfg.menuColor[0],
                 cfg.menuColor[1], cfg.menuColor[2], menuAlpha);

    DGL_Color4f(1, 1, 1, menuAlpha);

    // Draw the page arrows.
    token = (!menu->firstItem || menuTime & 8) ? "invgeml2" : "invgeml1";
    GL_DrawPatch_CS(menu->x, menu->y - 12, W_GetNumForName(token));
    token = (menu->firstItem + menu->numVisItems >= menu->itemCount ||
             menuTime & 8) ? "invgemr2" : "invgemr1";
    GL_DrawPatch_CS(312 - menu->x, menu->y - 12, W_GetNumForName(token));
#endif

    strcpy(buf, "Select to assign new, [Del] to clear");
    M_WriteText2(160 - M_StringWidth(buf, huFontA) / 2,
                 100 + (95/cfg.menuScale) - M_StringHeight(buf, huFontA), buf, huFontA,
#if __JDOOM__
                 1, .7f, .3f,
#else
                 1, 1, 1,
#endif
                 menuAlpha);

    for(i = 0; i < menu->numVisItems && menu->firstItem + i < menu->itemCount;
        i++, item++)
    {
        controlconfig_t* cc = item->data;

        if(item->type != ITT_EFUNC)
            continue;

        if(cc->controlName)
        {
            B_BindingsForControl(0, cc->controlName, buf, sizeof(buf));
        }
        else
        {
            B_BindingsForCommand(cc->command, buf, sizeof(buf));
        }
        /*
        memset(prbuff, 0, sizeof(prbuff));
        strncpy(prbuff, buf, sizeof(prbuff) - 1);*/

        /*
        if(ctrl->flags & CLF_ACTION)
            sprintf(controlCmd, "+%s", ctrl->command);
        else
            strcpy(controlCmd, ctrl->command);
        // Let's gather all the bindings for this command in all bind classes.
        if(!B_BindingsForCommand(controlCmd, buff, 0, true))
            strcpy(buff, "NONE");
        */

        /*
        // Now we must interpret what the bindings string says.
        // It may contain characters we can't print.
        strcpy(prbuff, "");
        token = strtok(buff, " ");
        while(token)
        {
            if(token[0] == '+')
            {
                spacecat(prbuff, token + 1);
            }
            if((token[0] == '*' && !(ctrl->flags & CLF_REPEAT)) ||
               token[0] == '-')
            {
                spacecat(prbuff, token);
            }
            token = strtok(NULL, " ");
        }
        strupr(prbuff);*/
        /*
        if(grabbing == cc)
        {
            // We're grabbing for this control.
            spacecat(prbuff, "...");
        }*/
        /*
#if __JHEXEN__
        M_WriteText2(menu->x + 154, menu->y + (i * menu->itemHeight), prbuff,
                        huFontA, 1, 0.7f, 0.3f, menuAlpha);
#else
        M_WriteText2(menu->x + 134, menu->y + (i * menu->itemHeight), prbuff,
                        huFontA, 1, 1, 1, menuAlpha);
#endif*/
#if __JHEXEN__
        M_DrawBindings(cc, menu->x + 154, menu->y + (i * menu->itemHeight), buf);
#else
        M_DrawBindings(cc, menu->x + 134, menu->y + (i * menu->itemHeight), buf);
#endif
    }
}

void M_ControlGrabDrawer(void)
{
    const char* text;
    
    if(!grabbing) return;
    
    GL_SetNoTexture();
    GL_DrawRect(0, 0, 320, 200, 0, 0, 0, .7f);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    
    DGL_Translatef(160, 100, 0);
    DGL_Scalef(SMALL_SCALE, SMALL_SCALE, 1);
    DGL_Translatef(-160, -100, 0);
    
    text = "press key or move controller for";
    M_WriteText2(160 - M_StringWidth(text, huFontA)/2, 98 - M_StringHeight(text, huFontA), 
                 text, huFontA, .75f, .75f, .75f, 1);
    M_WriteText2(160 - M_StringWidth(grabbing->item->text, huFontB)/2, 
                 102, grabbing->item->text, huFontB, 1, 1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

int M_ControlsPrivilegedResponder(event_t *event)
{
    // We're interested in key or button down events.
    if(grabbing && event->type == EV_SYMBOLIC)
    {
        char cmd[512];
        const char* symbol = 0;
        const char* bindClass = "game";

        if(sizeof(const char*) == sizeof(event->data1)) // 32-bit
        {
            symbol = (const char*) event->data1;
        }
        else // 64-bit
        {
            symbol = (const char*)(((int64_t)event->data1) | (((int64_t)event->data2)) << 32);            
        }

        if(strncmp(symbol, "echo-", 5))
        {
            return false;
        }
        if(!strncmp(symbol, "echo-key-", 9) && strcmp(symbol + strlen(symbol) - 5, "-down"))
        {
           return false;
        }
        
        //Con_Message("got %s\n", symbol);
        
        if(grabbing->bindClass)
        {
            bindClass = grabbing->bindClass;
        }
        
        if(grabbing->command)
        {
            sprintf(cmd, "bindevent {%s:%s} {%s}", bindClass, &symbol[5], grabbing->command);
            
            // Check for repeats.
            if(grabbing->flags & CCF_REPEAT)
            {
                const char* downPtr = 0;
                char temp[256];
                downPtr = strstr(symbol + 5, "-down");
                if(downPtr)
                {
                    char temp2[256];
                    memset(temp2, 0, sizeof(temp2));
                    strncpy(temp2, symbol + 5, downPtr - symbol - 5);
                    sprintf(temp, "; bindevent {%s:%s-repeat} {%s}", bindClass, temp2, 
                            grabbing->command);
                    strcat(cmd, temp);
                }
            }
        }
        else if(grabbing->controlName)
        {
            // Have to exclude the state part.
            boolean inv = (grabbing->flags & CCF_INVERSE) != 0;
            char temp3[256];
            const char *end = strchr(symbol + 5, '-');
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
            if(inv)
            {
                strcat(temp3, "-inverse");
            }
            
            sprintf(cmd, "bindcontrol {%s} {%s}", grabbing->controlName, temp3);
        }                
        
        Con_Message("bind command: %s\n", cmd);
        DD_Execute(true, cmd);
        
        /*
        // We shall issue a silent console command, but first we need
        // a textual representation of the event.
        B_FormEventString(evname, event->type, event->state, event->data1);

        // If this binding already exists, remove it.
        sprintf(cmd, "%s%s", grabbing->flags & CLF_ACTION ? "+" : "",
                grabbing->command);

        memset(buff, 0, sizeof(buff));

        // Check for bindings in this class only?
        if(B_BindingsForCommand(cmd, buff, grabbing->bindClass, false))
            if(findtoken(buff, evname, " "))    // Get rid of it?
            {
                del = true;
                strcpy(buff, "");
            }

        if(!del)
            sprintf(buff, "\"%s\"", grabbing->command);

        sprintf(cmd, "%s bdc%d %s %s",
                grabbing->flags & CLF_REPEAT ? "bindr" : "bind",
                grabbing->bindClass, evname + 1, buff);

        DD_Execute(false, cmd);
         */

        // We've finished the grab.
        grabbing = 0;
        DD_SetInteger(DD_SYMBOLIC_ECHO, false);
        S_LocalSound(menusnds[5], NULL);
        return true;
    }
    return false;
}
