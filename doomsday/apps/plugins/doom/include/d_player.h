/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * d_player.h: Player data structures.
 */

#ifndef __D_PLAYER__
#define __D_PLAYER__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "d_items.h"
#include "p_pspr.h"
#include "p_mobj.h"
#include "g_controls.h"

//
// Player states.
//
typedef enum {
    PST_LIVE, // Playing or camping.
    PST_DEAD, // Dead on the ground, view follows killer.
    PST_REBORN // Ready to restart/respawn???
} playerstate_t;

//
// Player internal flags, for cheats and debug.
//
typedef enum {
    CF_NOCLIP = 0x1, // No clipping, walk through barriers.
    CF_GODMODE = 0x2, // No damage, no health loss.
    CF_NOMOMENTUM = 0x4 // Not really a cheat, just a debug aid.
} cheat_t;

typedef struct player_s {
    ddplayer_t*     plr; // Pointer to the engine's player data.
    playerstate_t   playerState;
    playerclass_t   class_; // Player class type.
    playerbrain_t   brain;

    int             health; // This is only used between levels, mo->health is used during levels.
    int             armorPoints;
    int             armorType; // Armor type is 0-2.
    int             powers[NUM_POWER_TYPES]; // Power ups. invinc and invis are tic counters.
    dd_bool         keys[NUM_KEY_TYPES];
    dd_bool         backpack;
    int             frags[MAXPLAYERS];
    weapontype_t    readyWeapon;
    weapontype_t    pendingWeapon; // Is wp_nochange if not changing.
    struct playerweapon_s {
        dd_bool         owned;
    } weapons[NUM_WEAPON_TYPES];
    struct playerammo_s {
        int             owned;
        int             max;
    } ammo[NUM_AMMO_TYPES];

    int             attackDown; // True if button down last tic.
    int             useDown;

    int             cheats; // Bit flags, for cheats and debug, see cheat_t, above.
    int             refire; // Refired shots are less accurate.

    // For intermission stats:
    int             killCount;
    int             itemCount;
    int             secretCount;

    // For screen flashing (red or bright):
    int             damageCount;
    int             bonusCount;

    mobj_t*         attacker; // Who did damage (NULL for floors/ceilings).
    int             colorMap; // Player skin colorshift, 0-3 for which color to draw player.
    pspdef_t        pSprites[NUMPSPRITES]; // Overlay view sprites (gun, etc).
    dd_bool         didSecret; // True if secret level has been done.

    int             jumpTics; // The player can jump if this counter is zero.
    int             airCounter;
    int             flyHeight;
    int             rebornWait; // The player can be reborn if this counter is zero.
    dd_bool         centering; // The player's view pitch is centering back to zero.
    int             update, startSpot;

    coord_t         viewOffset[3]; // Relative to position of the player mobj.
    coord_t         viewZ; // Focal origin above r.z.
    coord_t         viewHeight; // Base height above floor for viewZ.
    coord_t         viewHeightDelta;
    coord_t         bob; // Bounded/scaled total momentum.

    // Target view to a mobj (NULL=disabled):
    mobj_t*         viewLock; // $democam
    int             lockFull;

#ifdef __cplusplus
    void write(writer_s *writer, struct playerheader_s &plrHdr) const;
    void read(reader_s *reader, struct playerheader_s &plrHdr);
#endif
} player_t;

#endif
