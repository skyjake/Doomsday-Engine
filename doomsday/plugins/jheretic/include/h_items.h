/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * Items: key cards, artifacts, weapon, ammunition.
 */

#ifndef __H_ITEMS__
#define __H_ITEMS__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"

#ifdef __GNUG__
#pragma interface
#endif

#define WEAPON_INFO(weaponnum, pclass, fmode) (&weaponinfo[weaponnum][pclass].mode[fmode])

typedef struct {
    int             gamemodebits;   // Game modes, weapon is available in.
    int             ammotype[NUMAMMO];  // required ammo types.
    int             pershot[NUMAMMO];   // Ammo used per shot of each type.
    boolean         autofire;           // (True)= fire when raised if fire held.
    int             upstate;
    int             raisesound;         // Sound played when weapon is raised.
    int             downstate;
    int             readystate;
    int             readysound;         // Sound played WHILE weapon is readyied.
    int             atkstate;
    int             holdatkstate;
    int             flashstate;
    int             static_switch;  // Weapon is not lowered during switch.
} weaponmodeinfo_t;

// Weapon info: sprite frames, ammunition use.
typedef struct {
    weaponmodeinfo_t mode[NUMWEAPLEVELS];
} weaponinfo_t;

extern weaponinfo_t weaponinfo[NUMWEAPONS][NUMCLASSES];

void            P_InitWeaponInfo(void);

#define NUMINVENTORYSLOTS   14
typedef struct {
    artitype_e      type;
    int             count;
} inventory_t;

#endif
