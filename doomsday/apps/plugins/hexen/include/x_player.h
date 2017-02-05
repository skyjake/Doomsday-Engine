/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * x_player.h:
 */

#ifndef __X_PLAYER_H__
#define __X_PLAYER_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "x_items.h"
#include "p_pspr.h"
#include "p_mobj.h"
#include "g_controls.h"

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
    playerclass_t   class_; // Player class type.
    playerbrain_t   brain;

    int             flyHeight;
    int             health; // Only used between maps, mo->health is used during.
    int             armorPoints[NUMARMOR];

    int             powers[NUM_POWER_TYPES];
    int             keys;
    int             pieces; // Fourth Weapon pieces.
    weapontype_t    readyWeapon;
    weapontype_t    pendingWeapon; // wp_nochange if not changing.
    struct playerweapon_s {
        dd_bool         owned;
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
    int             jumpTics; // Delay the next jump for a moment.
    int             airCounter;
    int             rebornWait; // The player can be reborn if this counter is zero.
    dd_bool         centering;
    unsigned int    worldTimer; // Total time the player's been playing.
    int             update;
    int             startSpot;
    int             overridePalette; // Palette that overrides the regular pain/etc. palette.

    float           viewOffset[3]; // Relative to position of the player mobj.
    float           viewZ; // Focal origin above r.z.
    float           viewHeight; // Base height above floor for viewZ.
    float           viewHeightDelta;
    float           bob; // Bounded/scaled total momentum.

    // Target view to a mobj (NULL=disabled).
    mobj_t*         viewLock; // $democam
    int             lockFull;

#ifdef __cplusplus
    void write(Writer1 *writer, struct playerheader_s &plrHdr) const;
    void read(Reader1 *reader, struct playerheader_s &plrHdr);
#endif
} player_t;

#endif
