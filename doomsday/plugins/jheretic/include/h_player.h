/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __H_PLAYER__
#define __H_PLAYER__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "h_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

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
typedef enum {
    // No clipping, walk through barriers.
    CF_NOCLIP = 1,
    // No damage, no health loss.
    CF_GODMODE = 2,
    // Not really a cheat, just a debug aid.
    CF_NOMOMENTUM = 4
} cheat_t;

typedef struct player_s {
    ddplayer_t     *plr;           // Pointer to the engine's player data.
    playerstate_t   playerstate;

    playerclass_t        class;         // player class type

    // bounded/scaled total momentum
    fixed_t         bob;

    // This is only used between levels,
    // mo->health is used during levels.
    int             health;
    int             armorpoints;
    // Armor type is 0-2.
    int             armortype;

    // Power ups. invinc and invis are tic counters.
    int             powers[NUM_POWER_TYPES];
    boolean         keys[NUM_KEY_TYPES];
    boolean         backpack;

    signed int      frags[MAXPLAYERS];
    weapontype_t    readyweapon;

    // Is wp_nochange if not changing.
    weapontype_t    pendingweapon;

    boolean         weaponowned[NUM_WEAPON_TYPES];
    int             ammo[NUM_AMMO_TYPES];
    int             maxammo[NUM_AMMO_TYPES];

    // true if button down last tic
    int             attackdown;
    int             usedown;

    // Bit flags, for cheats and debug.
    // See cheat_t, above.
    int             cheats;

    // Refired shots are less accurate.
    int             refire;

    // For intermission stats.
    int             killcount;
    int             itemcount;
    int             secretcount;

    // For screen flashing (red or bright).
    int             damagecount;
    int             bonuscount;

    // Who did damage (NULL for floors/ceilings).
    mobj_t         *attacker;

    // Player skin colorshift,
    //  0-3 for which color to draw player.
    int             colormap;

    // Overlay view sprites (gun, etc).
    pspdef_t        psprites[NUMPSPRITES];

    // True if secret level has been done.
    boolean         didsecret;

    // The player's view pitch is centering back to zero.
    boolean         centering;

    // The player can jump if this counter is zero.
    int             jumptics;

    int             update, startspot;

    // Target view to a mobj (NULL=disabled).
    mobj_t*         viewlock;      // $democam
    int             lockFull;

    int             flyheight;

    //
    // DJS - Here follows Heretic specific player_t properties
    //
    inventory_t     inventory[NUMINVENTORYSLOTS];
    int             inv_ptr;
    int             curpos;
    artitype_e      readyArtifact;
    int             artifactCount;
    int             inventorySlotNum;

    int             flamecount;    // for flame thrower duration


    int             morphTics;     // player is a chicken if > 0
    int             chickenPeck;   // chicken peck countdown
    mobj_t         *rain1;         // active rain maker 1
    mobj_t         *rain2;         // active rain maker 2

} player_t;


#endif
