/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * x_items.h: Items, key cards/artifacts/weapons/ammunition...
 */

#ifndef __X_ITEMS_H__
#define __X_ITEMS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "h2def.h"

#ifdef __GNUG__
#pragma interface
#endif

#define NUMINVENTORYSLOTS   NUMARTIFACTS

#define WEAPON_INFO(weaponnum, pclass, fmode) ( \
    &weaponinfo[weaponnum][pclass].mode[fmode])

typedef struct {
    int             gamemodebits;  // Game modes, weapon is available in.
    int             ammotype[NUM_AMMO_TYPES]; // required ammo types.
    int             pershot[NUM_AMMO_TYPES]; // Ammo used per shot of each type.
    boolean         autofire; // @c true = fire when raised if fire held.
    int             upstate;
    int             raisesound; // Sound played when weapon is raised.
    int             downstate;
    int             readystate;
    int             readysound; // Sound played WHILE weapon is readyied.
    int             atkstate;
    int             holdatkstate;
    int             flashstate;
} weaponmodeinfo_t;

// Weapon info: sprite frames, ammunition use.
typedef struct {
    weaponmodeinfo_t mode[NUMWEAPLEVELS];
} weaponinfo_t;

typedef struct {
    int             type;
    int             count;
} inventory_t;

extern weaponinfo_t weaponinfo[NUM_WEAPON_TYPES][NUM_PLAYER_CLASSES];

#endif
