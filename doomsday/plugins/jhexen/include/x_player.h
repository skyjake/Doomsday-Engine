/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
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


#ifndef __X_PLAYER__
#define __X_PLAYER__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
//#include "d_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "h2def.h" // "p_mobj.h"

#ifdef __GNUG__
#pragma interface
#endif

//
// Player states.
//
typedef enum {
    // Playing or camping.
    PST_LIVE,
    // Dead on the ground, view follows killer.
    PST_DEAD,
    // Ready to restart/respawn???
    PST_REBORN
} playerstate_t;

//
// Player internal flags, for cheats and debug.
//
#if 0
typedef enum {
    // No clipping, walk through barriers.
    CF_NOCLIP = 1,
    // No damage, no health loss.
    CF_GODMODE = 2,
    // Not really a cheat, just a debug aid.
    CF_NOMOMENTUM = 4
} cheat_t;
#endif

    // No clipping, walk through barriers.
#define CF_NOCLIP 1
    // No damage, no health loss.
#define CF_GODMODE 2
    // Not really a cheat, just a debug aid.
#define CF_NOMOMENTUM 4

// Extended player information, Hexen specific.
typedef struct player_s {
    ddplayer_t     *plr;           // Pointer to the engine's player data.
    playerstate_t   playerstate;

    playerclass_t        class;         // player class type

    //  fixed_t     viewheight;             // base height above floor for viewz
    //  fixed_t     deltaviewheight;        // squat speed
    fixed_t         bob;           // bounded/scaled total momentum

    int             flyheight;
    //int           lookdir;
    boolean         centering;
    int             health;        // only used between levels, mo->health
    // is used during levels
    int             armorpoints[NUMARMOR];

    inventory_t     inventory[NUMINVENTORYSLOTS];
    int             inv_ptr;
    int             curpos;
    artitype_e      readyArtifact;
    int             artifactCount;
    int             inventorySlotNum;
    int             powers[NUM_POWER_TYPES];
    int             keys;
    int             pieces;        // Fourth Weapon pieces
    weapontype_t    readyweapon;
    weapontype_t    pendingweapon; // wp_nochange if not changing
    boolean         weaponowned[NUM_WEAPON_TYPES];
    int             ammo[NUM_AMMO_TYPES];  // mana
    int             attackdown, usedown;    // true if button down last tic
    int             cheats;        // bit flags
    signed int      frags[MAXPLAYERS];  // kills of other players

    int             refire;        // refired shots are less accurate

    int             killcount, itemcount, secretcount;  // for intermission

    int             damagecount, bonuscount;    // for screen flashing
    int             poisoncount;   // screen flash for poison damage
    mobj_t         *poisoner;      // NULL for non-player mobjs
    mobj_t         *attacker;      // who did damage (NULL for floors)
    int             colormap;      // 0-3 for which color to draw player
    pspdef_t        psprites[NUMPSPRITES];  // view sprites (gun, etc)
    int             morphTics;     // player is a pig if > 0
    uint            jumptics;      // delay the next jump for a moment
    unsigned int    worldTimer;    // total time the player's been playing
    int             update, startspot;
    // Target view to a mobj (NULL=disabled).
    mobj_t*         viewlock;      // $democam
    int             lockFull;
} player_t;

#endif
