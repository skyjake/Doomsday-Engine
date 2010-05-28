/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * hu_menu.h: Menu widget stuff, episode selection and such.
 */

#ifndef __COMMON_HUD_MENU__
#define __COMMON_HUD_MENU__

#include "dd_types.h"

typedef enum menucommand_e {
    MCMD_OPEN, // Open the menu.
    MCMD_CLOSE, // Close the menu.
    MCMD_CLOSEFAST, // Instantly close the menu.
    MCMD_NAV_OUT, // Navigate "out" of the current menu (up a level).
    MCMD_NAV_LEFT,
    MCMD_NAV_RIGHT,
    MCMD_NAV_DOWN,
    MCMD_NAV_UP,
    MCMD_NAV_PAGEDOWN,
    MCMD_NAV_PAGEUP,
    MCMD_SELECT, // Execute whatever action is attaced to the current item.
    MCMD_DELETE
} menucommand_e;

// Not to be confused with the size of the description in the save file.
#define HU_SAVESTRINGSIZE       (24)

// Sounds played in the menu.
#if __JDOOM__ || __JDOOM64__
#define SFX_MENU_CLOSE      (SFX_SWTCHX)
#define SFX_MENU_OPEN       (SFX_SWTCHN)
#define SFX_MENU_CANCEL     (SFX_SWTCHN)
#define SFX_MENU_NAV_UP     (SFX_PSTOP)
#define SFX_MENU_NAV_RIGHT  (SFX_PSTOP)
#define SFX_MENU_ACCEPT     (SFX_PISTOL)
#define SFX_MENU_CYCLE      (SFX_PISTOL) // Cycle available options.
#define SFX_MENU_SLIDER_MOVE (SFX_STNMOV)
#define SFX_QUICKSAVE_PROMPT (SFX_SWTCHN)
#define SFX_QUICKLOAD_PROMPT (SFX_SWTCHN)
#elif __JHERETIC__
#define SFX_MENU_CLOSE      (SFX_DORCLS)
#define SFX_MENU_OPEN       (SFX_SWITCH)
#define SFX_MENU_CANCEL     (SFX_SWITCH)
#define SFX_MENU_NAV_UP     (SFX_SWITCH)
#define SFX_MENU_NAV_RIGHT  (SFX_SWITCH)
#define SFX_MENU_ACCEPT     (SFX_DORCLS)
#define SFX_MENU_CYCLE      (SFX_DORCLS) // Cycle available options.
#define SFX_MENU_SLIDER_MOVE (SFX_STNMOV)
#define SFX_QUICKSAVE_PROMPT (SFX_CHAT)
#define SFX_QUICKLOAD_PROMPT (SFX_CHAT)
#elif __JHEXEN__
#define SFX_MENU_CLOSE      (SFX_DOOR_LIGHT_CLOSE)
#define SFX_MENU_OPEN       (SFX_DOOR_LIGHT_CLOSE)
#define SFX_MENU_CANCEL     (SFX_PICKUP_KEY)
#define SFX_MENU_NAV_UP     (SFX_FIGHTER_HAMMER_HITWALL)
#define SFX_MENU_NAV_RIGHT  (SFX_FIGHTER_HAMMER_HITWALL)
#define SFX_MENU_ACCEPT     (SFX_PLATFORM_STOP)
#define SFX_MENU_CYCLE      (SFX_CHAT) // Cycle available options.
#define SFX_MENU_SLIDER_MOVE (SFX_PICKUP_KEY)
#define SFX_QUICKSAVE_PROMPT (SFX_CHAT)
#define SFX_QUICKLOAD_PROMPT (SFX_CHAT)
#endif

extern float menu_glitter, menu_shadow;

void            Hu_MenuRegister(void);
void            Hu_MenuInit(void);

void            Hu_MenuTicker(timespan_t time);
int             Hu_MenuResponder(event_t* ev);
void            Hu_MenuDrawer(void);

void            Hu_MenuCommand(menucommand_e cmd);

boolean         Hu_MenuIsActive(void);
void            Hu_MenuSetAlpha(float alpha);
float           Hu_MenuAlpha(void);
void            Hu_MenuPageString(char* page, const menu_t* menu);

void            M_DrawMenuText(const char* string, int x, int y);
void            M_DrawMenuText2(const char* string, int x, int y, gamefontid_t font);
void            M_DrawMenuText3(const char* string, int x, int y, gamefontid_t font, short flags);
void            M_DrawMenuText4(const char* string, int x, int y, gamefontid_t font, short flags, float glitterStrength);
void            M_DrawMenuText5(const char* string, int x, int y, gamefontid_t font, short flags, float glitterStrength, float shadowStrength);

boolean         M_EditResponder(event_t* ev);

DEFCC(CCmdMenuAction);

#endif
