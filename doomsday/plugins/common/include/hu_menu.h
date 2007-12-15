/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
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
    MCMD_NAV_OUT, // Navigate "out" of the current menu (up a level).
    MCMD_NAV_LEFT,
    MCMD_NAV_RIGHT,
    MCMD_NAV_DOWN,
    MCMD_NAV_UP,
    MCMD_SELECT // Execute whatever action is attaced to the current item.
} menucommand_e;

void            Hu_MenuRegister(void);
void            Hu_MenuInit(void);

void            Hu_MenuTicker(timespan_t time);
boolean         Hu_MenuResponder(event_t *ev);
void            Hu_MenuDrawer(void);

void            Hu_MenuCommand(menucommand_e cmd);

boolean         Hu_MenuIsActive(void);
void            Hu_MenuSetAlpha(float alpha);
float           Hu_MenuAlpha(void);

DEFCC(CCmdMenuAction);

#endif
