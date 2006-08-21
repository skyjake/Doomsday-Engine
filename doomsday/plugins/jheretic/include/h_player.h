/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
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
    ticcmd_t        cmd;

    pclass_t        class;         // player class type

    // bounded/scaled total momentum
    fixed_t         bob;

    // This is only used between levels,
    // mo->health is used during levels.
    int             health;
    int             armorpoints;
    // Armor type is 0-2.
    int             armortype;

    // Power ups. invinc and invis are tic counters.
    int             powers[NUMPOWERS];
    boolean         keys[NUMKEYS];
    boolean         backpack;

    signed int      frags[MAXPLAYERS];
    weapontype_t    readyweapon;

    // Is wp_nochange if not changing.
    weapontype_t    pendingweapon;

    boolean         weaponowned[NUMWEAPONS];
    int             ammo[NUMAMMO];
    int             maxammo[NUMAMMO];

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

    // Target view to a player (0=disabled, 1=player zero, etc.).
    int             viewlock;      // $democam

    int             flyheight;

    //
    // DJS - Here follows Heretic specific player_t properties
    //
    inventory_t     inventory[NUMINVENTORYSLOTS];
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
