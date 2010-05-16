/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * m_defs.h: Common menu defines and types.
 */

#ifndef __MENU_COMMON_DEFS_H_
#define __MENU_COMMON_DEFS_H_

#include "r_common.h"

typedef enum {
    ITT_EMPTY,
    ITT_EFUNC,
    ITT_LRFUNC,
    ITT_SETMENU
} menuitemtype_t;

// Menu item flags
#define MIF_NOTALTTXT           0x01 // Don't use alt text instead of lump (M_NMARE)

typedef struct {
    menuitemtype_t  type;
    int             flags;
    const char*     text;
    void          (*func) (int option, void *data);
    int             option;
    patchid_t*      patch;
    void*           data;
} menuitem_t;

// Menu flags
#define MNF_NOHOTKEYS           0x00000001 // Hotkeys are disabled.
#define MNF_DELETEFUNC          0x00000002 // MCMD_DELETE causes a call to item's func

typedef struct unscaledmenustate_s {
    int             numVisItems;
    int             y;
} unscaledmenustate_t;

typedef struct {
    int             flags;
    int             x;
    int             y;
    void          (*drawFunc) (void);
    int             itemCount;
    const menuitem_t* items;
    int             lastOn;
    int             prevMenu; // menutype_t
    gamefontid_t    font; // Font for menu items.
    float*          color;
    int             itemHeight;
    // For multipage menus.
    int             firstItem, numVisItems;
    // Scalable menus.
    unscaledmenustate_t unscaled;
} menu_t;

#endif
