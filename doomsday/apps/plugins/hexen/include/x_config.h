/**\file x_config.h
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

#ifndef __JHEXEN_CONFIG_H__
#define __JHEXEN_CONFIG_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "doomsday.h"
#include "h2def.h"
//#include "hu_lib.h"
#include "../../common/include/config.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    HUD_MANA,
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

// This struct should be cleaned up. Currently some of the data isn't
// even used any more.

typedef struct jhexen_config_s {
    libcommon_config_t common;

    byte            overrideHubMsg; // skip the transition hub message when 1
    int             mlookInverseY;
    byte            hudShown[NUMHUDDISPLAYS]; // HUD data visibility.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.

    int             translucentIceCorpse;
    byte            allowMonsterFloatOverBlocking; // if true, floating mobjs are allowed to climb over mobjs blocking the way.
    byte            deathkingsAutoRespawnChance; // allow script 255 to spawn things (0-100%)

    byte            netClass;
    byte            netRandomClass;

    playerclass_t   playerClass[MAXPLAYERS];
    byte            playerColor[MAXPLAYERS];

    float           inventoryTimer; // Number of seconds until the invetory auto-hides.
    byte            inventoryWrap;
    byte            inventoryUseNext;
    byte            inventoryUseImmediate;
    int             inventorySlotMaxVis;
    byte            inventorySlotShowEmpty;
    byte            inventorySelectMode;
} game_config_t;

extern game_config_t cfg;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
