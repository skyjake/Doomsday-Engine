/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * Common controls menu
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
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
} controlconfig_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void M_DrawControlsMenu(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float    menuAlpha;
extern int      menusnds[];

static menuitem_t* ControlsItems;

#ifdef __JDOOM__
menu_t ControlsDef = {
    MNF_NOHOTKEYS,
    32, 40,
    M_DrawControlsMenu,
    0, NULL,
    1, MENU_OPTIONS,
    huFontA,                    //1, 0, 0,
    cfg.menuColor2,
    NULL,
    LINEHEIGHT_A,
    0, 17
};
#endif

#ifdef __JHERETIC__
menu_t ControlsDef = {
    MNF_NOHOTKEYS,
    32, 26,
    M_DrawControlsMenu,
    0, NULL,
    1, MENU_OPTIONS,
    huFontA,                    //1, 0, 0,
    cfg.menuColor2,
    NULL,
    LINEHEIGHT_A,
    0, 15
};
#endif

#ifdef __JHEXEN__
menu_t ControlsDef = {
    MNF_NOHOTKEYS,
    32, 21,
    M_DrawControlsMenu,
    0, NULL,
    1, MENU_OPTIONS,
    huFontA,                    //1, 0, 0,
    cfg.menuColor2,
    NULL,
    LINEHEIGHT_A,
    0, 16
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
    
    { "looking" },
    { "look up", 0, "look", 0, CCF_STAGED | CCF_INVERSE },
    { "look down", 0, "look", 0, CCF_STAGED | CCF_NON_INVERSE },
    { "look center", 0, 0, "impulse lookcenter" },

    { "weapons" },
    { "attack/fire", 0, "attack" },
    { "next weapon", 0, 0, "impulse nextweapon" },
    { "previous weapon", 0, 0, "impulse prevweapon" },
    // TODO: Game-specific.
    
    // TODO: Inventory.
    
    { "chat" },
    { "begin chat", 0, 0, "beginchat" },
    { "begin chat (p1)", 0, 0, "beginchat 0" },
    { "begin chat (p2)", 0, 0, "beginchat 1" },
    { "begin chat (p3)", 0, 0, "beginchat 2" },
    { "begin chat (p4)", 0, 0, "beginchat 3" },
    { "message refresh", 0, 0, "msgrefresh" },
    { "send message", "chat", 0, "chatcomplete" },
    { "cancel message", "chat", 0, "chatcancel" },
    { "send macro 1", "chat", 0, "chatsendmacro 0" },
    { "send macro 2", "chat", 0, "chatsendmacro 1" },
    { "send macro 3", "chat", 0, "chatsendmacro 2" },
    { "send macro 4", "chat", 0, "chatsendmacro 3" },
    { "send macro 5", "chat", 0, "chatsendmacro 4" },
    { "send macro 6", "chat", 0, "chatsendmacro 5" },
    { "send macro 7", "chat", 0, "chatsendmacro 6" },
    { "send macro 8", "chat", 0, "chatsendmacro 7" },
    { "send macro 9", "chat", 0, "chatsendmacro 8" },
    { "send macro 10", "chat", 0, "chatsendmacro 9" },
    { "backspace", "chat", 0, "chatdelete" },
    
    { "map" },
    { "show/hide map", 0, 0, "automap" },
    { "zoom in", 0, "mapzoom", 0, CCF_NON_INVERSE },
    { "zoom out", 0, "mapzoom", 0, CCF_INVERSE },
    { "zoom maximum", "map", 0, "zoommax" },
    { "pan left", 0, "mappanx", 0, CCF_INVERSE },
    { "pan right", 0, "mappanx", 0, CCF_NON_INVERSE },
    { "pan up", 0, "mappany", 0, CCF_NON_INVERSE },
    { "pan down", 0, "mappany", 0, CCF_INVERSE },
    { "toggle follow", 0, 0, "follow" },
    { "toggle rotation", 0, 0, "rotate" },
    { "add mark", 0, 0, "addmark" },
    { "clear marks", 0, 0, "clearmarks" },

    { "hud" },
    { "show/hide hud", 0, 0, "showhud" },
    { "smaller view", 0, 0, "viewsize -" },
    { "larger view", 0, 0, "viewsize +" },

    { "menu shortcuts" },
    { "pause game", 0, 0, "pause" },
    { "end game", 0, 0, "endgame" },
    { "quit", 0, 0, "quit" },
#if !__JDOOM64__
    { "help screen", 0, 0, "helpscreen" },
#endif
    { "save game", 0, 0, "savegame" },
    { "load game", 0, 0, "loadgame" },
    { "quick save", 0, 0, "quicksave" },
    { "quick load", 0, 0, "quickload" },
    { "sound options", 0, 0, "soundmenu" },
    { "toggle messages", 0, 0, "togglemsgs" },
    { "gamma correction", 0, 0, "togglegamma" },
    { "screenshot", 0, 0, "screenshot" },
    
    { "menu" },
    { "show/hide menu", 0, 0, "menu" },
    { "previous menu", "menu", 0, "menucancel" },
    { "move up", "menu", 0, "menuup", CCF_REPEAT },
    { "move down", "menu", 0, "menudown", CCF_REPEAT },
    { "move left", "menu", 0, "menuleft", CCF_REPEAT },
    { "move right", "menu", 0, "menuright", CCF_REPEAT },
    { "select", "menu", 0, "menuselect", CCF_REPEAT },

    { "on-screen questions" },
    { "answer yes", "message", 0, "messageyes" },
    { "answer no", "message", 0, "messageno" },    
    { "cancel", "message", 0, "messagecancel" },    
};

// CODE --------------------------------------------------------------------

static void M_EFuncControlConfig(int option, void *data)
{
    controlconfig_t* cc = data;
    
    //grabbing = controls + option;
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

        item->text = (char*) cc->itemText;

        // Inert items.
        if(!cc->controlName && !cc->command)
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

/*
 * spacecat
 */
void spacecat(char *str, const char *catstr)
{
    if(str[0])
        strcat(str, " ");

    if(!stricmp(catstr, "smcln"))
        catstr = ";";

    strcat(str, catstr);
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
    GL_DrawRect(*x, y, width*SMALL_SCALE + 2, height, 0, 0, 0, menuAlpha*.6f);

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

#if __JDOOM__
    M_DrawTitle("CONTROLS", menu->y - 28);
    sprintf(buf, "PAGE %i/%i", menu->firstItem / menu->numVisItems + 1,
            menu->itemCount / menu->numVisItems + 1);
    M_WriteText2(160 - M_StringWidth(buf, huFontA) / 2, menu->y - 12, buf,
                 huFontA, 1, .7f, .3f, menuAlpha);
#else
    M_WriteText2(120, 2, "CONTROLS", huFontB, cfg.menuColor[0],
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
                 195 - M_StringHeight(buf, huFontA), buf, huFontA, 
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
            if(!B_BindingsForControl(0, cc->controlName, 
                                     /*cc->flags & CCF_NON_INVERSE? BFCI_ONLY_NON_INVERSE :
                                     cc->flags & CCF_INVERSE? BFCI_ONLY_INVERSE :*/
                                     BFCI_BOTH, buf, sizeof(buf)))
            {
                
            }   
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

/*
 *  findtoken
 */
int findtoken(char *string, char *token, char *delim)
{
    char   *ptr = strtok(string, delim);

    while(ptr)
    {
        if(!stricmp(ptr, token))
            return true;
        ptr = strtok(NULL, delim);
    }
    return false;
}

/*
 *  D_PrivilegedResponder
 */
int D_PrivilegedResponder(event_t *event)
{
    char    cmd[256], buff[256], evname[80];

    // We're interested in key or button down events.
    if(grabbing && event->state == EVS_DOWN &&
       (event->type == EV_KEY || event->type == EV_MOUSE_BUTTON ||
        event->type == EV_JOY_BUTTON || event->type == EV_POV))
    {
        // We'll grab this event.
        boolean del = false;

        // Check for a cancel.
        if(event->type == EV_KEY)
        {
            if(event->data1 == DDKEY_ESCAPE)
            {
                grabbing = 0;
                return true;
            }
        }

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
        S_LocalSound(menusnds[5], NULL);

        return true;
    }
    /*
    // Process the screen shot key right away.
    if(devparm && event->type == EV_KEY && event->data1 == DDKEY_F1)
    {
        if(event->state == EVS_DOWN)
            G_ScreenShot();
        // All F1 events are eaten.
        return true;
    }*/
    return false;
}
