/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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

#ifndef __JHERETIC_ACTIONS_H__
#define __JHERETIC_ACTIONS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "dd_share.h"

// These must correspond the action_t's in the actions array!
// Things that are needed in building ticcmds should be here.
typedef enum {
    // Game controls.
    A_TURNLEFT,
    A_TURNRIGHT,
    A_FORWARD,
    A_BACKWARD,
    A_STRAFELEFT,
    A_STRAFERIGHT,
    A_FIRE,
    A_USE,
    A_STRAFE,
    A_SPEED,
    A_FLYUP,
    A_FLYDOWN,
    A_FLYCENTER,
    A_LOOKUP,
    A_LOOKDOWN,
    A_LOOKCENTER,
    A_USEARTIFACT,
    A_MLOOK,
    A_JLOOK,
    A_NEXTWEAPON,
    A_PREVIOUSWEAPON,
    A_WEAPON1,
    A_WEAPON2,
    A_WEAPON3,
    A_WEAPON4,
    A_WEAPON5,
    A_WEAPON6,
    A_WEAPON7,
    A_WEAPON8,
    A_WEAPON9,

    // Item hotkeys.
    A_INVULNERABILITY,
    A_INVISIBILITY,
    A_HEALTH,
    A_SUPERHEALTH,
    A_TOMEOFPOWER,
    A_TORCH,
    A_FIREBOMB,
    A_EGG,
    A_FLY,
    A_TELEPORT,
    A_PANIC,

    // Game system actions.
    A_STOPDEMO,

    A_JUMP,

    A_MAPZOOMIN,
    A_MAPZOOMOUT,
    A_MAPPANUP,
    A_MAPPANDOWN,
    A_MAPPANLEFT,
    A_MAPPANRIGHT,

    A_WEAPONCYCLE1,
    NUM_ACTIONS
} haction_t;

// This is the actions array.
extern action_t actions[NUM_ACTIONS + 1];

#endif
