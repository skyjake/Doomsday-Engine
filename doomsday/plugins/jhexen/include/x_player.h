/* $Id: d_player.h 3305 2006-06-11 17:00:36Z skyjake $
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
//#include "p_mobj.h"

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
    ticcmd_t        cmd;

    pclass_t        class;         // player class type

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
    artitype_e      readyArtifact;
    int             artifactCount;
    int             inventorySlotNum;
    int             powers[NUMPOWERS];
    int             keys;
    int             pieces;        // Fourth Weapon pieces
    weapontype_t    readyweapon;
    weapontype_t    pendingweapon; // wp_nochange if not changing
    boolean         weaponowned[NUMWEAPONS];
    int             ammo[NUMAMMO];  // mana
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
    int             viewlock;      // $democam
} player_t;

#endif
