/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * x_player.h:
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
#include "x_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

#include "g_controls.h"

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

#define CF_NOCLIP           (1) // No clipping, walk through barriers.
#define CF_GODMODE          (2) // No damage, no health loss.
#define CF_NOMOMENTUM       (4) // Not really a cheat, just a debug aid.

// Extended player information, Hexen specific.
typedef struct player_s {
    ddplayer_t*     plr; // Pointer to the engine's player data.
    playerstate_t   playerState;
    playerclass_t   class; // Player class type.
    playerbrain_t   brain;

    fixed_t         bob; // Bounded/scaled total momentum.

    int             flyHeight;
    boolean         centering;
    int             health; // Only used between levels, mo->health is used during levels.
    int             armorPoints[NUMARMOR];

    inventory_t     inventory[NUMINVENTORYSLOTS];
    int             invPtr;
    int             curPos;
    artitype_e      readyArtifact;
    int             artifactCount;
    int             inventorySlotNum;
    int             powers[NUM_POWER_TYPES];
    int             keys;
    int             pieces; // Fourth Weapon pieces.
    weapontype_t    readyWeapon;
    weapontype_t    pendingWeapon; // wp_nochange if not changing.
    struct playerweapon_s {
        boolean         owned;
    } weapons[NUM_WEAPON_TYPES];
    struct playerammo_s {
        int             owned;
    } ammo[NUM_AMMO_TYPES]; // Mana.
    int             attackDown, useDown; // true if button down last tic.
    int             cheats; // Bit flags.
    signed int      frags[MAXPLAYERS]; // Kills of other players.

    int             refire; // Refired shots are less accurate.

    int             killCount, itemCount, secretCount; // For intermission.

    int             damageCount, bonusCount; // For screen flashing.
    int             poisonCount; // Screen flash for poison damage.
    mobj_t*         poisoner; // NULL for non-player mobjs.
    mobj_t*         attacker; // Who did damage (NULL for floors).
    int             colorMap; // 0-3 for which color to draw player.
    pspdef_t        pSprites[NUMPSPRITES];  // view sprites (gun, etc).
    int             morphTics; // Player is a pig if > 0.
    uint            jumpTics; // Delay the next jump for a moment.
    unsigned int    worldTimer; // Total time the player's been playing.
    int             update, startSpot;
    // Target view to a mobj (NULL=disabled).
    mobj_t*         viewLock; // $democam
    int             lockFull;
} player_t;

#endif
