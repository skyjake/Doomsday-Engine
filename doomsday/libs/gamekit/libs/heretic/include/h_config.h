/**\file h_config.h
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
 * jHeretic configuration.
 *
 * Global settings. Most of these are console variables.
 */

#ifndef __JHERETIC_CONFIG_H__
#define __JHERETIC_CONFIG_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomsday.h"
#include "doomdef.h"
//#include "hu_lib.h"
#include "../../common/include/config.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    HUD_AMMO,
    HUD_ARMOR,
    HUD_KEYS,
    HUD_HEALTH,
    HUD_READYITEM,
    HUD_LOG,
    NUMHUDDISPLAYS
};

// Hud Unhide Events (the hud will unhide on these events if enabled).
typedef enum {
    HUE_FORCE = -1,
    HUE_ON_DAMAGE,
    HUE_ON_PICKUP_HEALTH,
    HUE_ON_PICKUP_ARMOR,
    HUE_ON_PICKUP_POWER,
    HUE_ON_PICKUP_WEAPON,
    HUE_ON_PICKUP_AMMO,
    HUE_ON_PICKUP_KEY,
    HUE_ON_PICKUP_INVITEM,
    NUMHUDUNHIDEEVENTS
} hueevent_t;

// Counter Cheat flags.
#define CCH_KILLS           0x01
#define CCH_ITEMS           0x02
#define CCH_SECRETS         0x04
#define CCH_KILLS_PRCNT     0x08
#define CCH_ITEMS_PRCNT     0x10
#define CCH_SECRETS_PRCNT   0x20

typedef struct jheretic_config_s {
    libcommon_config_t common;

    byte            secretMsg;
    byte            bobWeaponLower;
    byte            hudShown[NUMHUDDISPLAYS]; // HUD data visibility.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.

    byte            moveCheckZ;    // if true, mobjs can move over/under each other.
    byte            slidingCorpses;
    byte            allowMonsterFloatOverBlocking; // if true, floating mobjs are allowed to climb over mobjs blocking the way.

    byte            noCoopDamage;
    byte            noTeamDamage;
    byte            respawnMonstersNightmare;
    int             corpseTime;

    byte            netRespawn;
    byte            netSlot;

    playerclass_t   playerClass[MAXPLAYERS];
    int             playerColor[MAXPLAYERS];

    /**
     * Compatibility options.
     * \todo Put these into an array so we can use a bit array to change
     * multiple options based on a compatibility mode (ala PrBoom).
     */
    byte            monstersStuckInDoors;
    byte            avoidDropoffs;
    byte            moveBlock; // Dont handle large negative movement in P_TryMoveXY.
    byte            wallRunNorthOnly; // If handle large make exception for wallrunning

    byte            fallOff; // Objects fall under their own weight.
    byte            fixFloorFire; // Fix Heretic bug; explode Maulotaur floor fire when feetclipped.
    byte            fixPlaneScrollMaterialsEastOnly; // Fix Heretic bug; plane materials would only scroll east.

    // jHeretic specific
    int             ringFilter;
    float           inventoryTimer; // Number of seconds until the invetory auto-hides.
    byte            inventoryWrap;
    byte            inventoryUseNext;
    byte            inventoryUseImmediate;
    int             inventorySlotMaxVis;
    byte            inventorySlotShowEmpty;
    byte            inventorySelectMode;
    int             tomeCounter;
    int             tomeSound;
    byte            staffPowerDamageToGhosts;
} game_config_t;

extern game_config_t cfg;      // in g_game.c

#ifdef __cplusplus
} // extern "C"
#endif

#endif
