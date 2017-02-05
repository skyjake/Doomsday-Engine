/**\file d_config.h
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
 * jDoom configuration.
 * Global settings. Most of these are console variables.
 */

#ifndef __JDOOM_SETTINGS_H__
#define __JDOOM_SETTINGS_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "api_uri.h"
#include "doomdef.h"
//#include "hu_lib.h"
#include "../../common/include/config.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    HUD_HEALTH,
    HUD_ARMOR,
    HUD_AMMO,
    HUD_KEYS,
    HUD_FRAGS,
    HUD_FACE,
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
    NUMHUDUNHIDEEVENTS
} hueevent_t;

// Counter Cheat flags.
#define CCH_KILLS           0x01
#define CCH_ITEMS           0x02
#define CCH_SECRETS         0x04
#define CCH_KILLS_PRCNT     0x08
#define CCH_ITEMS_PRCNT     0x10
#define CCH_SECRETS_PRCNT   0x20

typedef struct jdoom_config_s {
    libcommon_config_t common;

    int             menuQuitSound;
    byte            secretMsg;
    byte            hudKeysCombine; // One icon per color (e.g. if red key and red skull is owned only show red key).
    byte            bobWeaponLower;
    byte            hudShown[NUMHUDDISPLAYS]; // HUD data visibility.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.

    byte            berserkAutoSwitch;
    byte            deathLookUp;    // look up when killed

    byte            moveCheckZ;     // if true, mobjs can move over/under each other.
    byte            slidingCorpses;
    byte            allowMonsterFloatOverBlocking; // if true, floating mobjs are allowed to climb over mobjs blocking the way.
    byte            respawnMonstersNightmare;
    int             corpseTime;

    byte            noCoopDamage;
    byte            noTeamDamage;
    byte            noCoopWeapons;
    byte            noCoopAnything; // disable all multiplayer objects in co-op
    byte            coopRespawnItems;
    byte            noNetBFG;
    byte            netBFGFreeLook; // Allow free-aim with BFG.
    byte            netRespawn;
    byte            netSlot;
    byte            killMessages;

    int             playerColor[MAXPLAYERS];

    /**
     * Compatibility options:
     * @todo Put these into an array so we can use a bit array to change
     * multiple options based on a compatibility mode (ala PrBoom).
     */
    byte            raiseGhosts;
    byte            vileChaseUseVileRadius;
    byte            maxSkulls;
    byte            allowSkullsInWalls;
    byte            anyBossDeath;
    byte            monstersStuckInDoors;
    byte            avoidDropoffs;
    byte            moveBlock; // Dont handle large negative movement in P_TryMoveXY.
    byte            wallRunNorthOnly; // If handle large make exception for wallrunning
    byte            zombiesCanExit; // Zombie players can exit levels.
    byte            fallOff; // Objects fall under their own weight.
    byte            gibCrushedNonBleeders;
    byte            fixOuchFace;
    byte            fixStatusbarOwnedWeapons;
} game_config_t;

extern game_config_t cfg;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
