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

extern char* yesno[];

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

boolean         M_EditResponder(event_t* ev);

DEFCC(CCmdMenuAction);

#endif
