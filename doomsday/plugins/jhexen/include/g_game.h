/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * g_game.h:
 */

#ifndef __G_GAME_H__
#define __G_GAME_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "x_config.h"

typedef struct {
    gamemode_t      gameMode;
    int             gameModeBits;
    // This is returned in D_Get(DD_GAME_MODE), max 16 chars.
    char            gameModeString[17];

    gameaction_t    action;
    gamestate_t     state, stateLast;
    boolean         paused;
    boolean         userGame; // Ok to save / end game.

    playerprofile_t playerProfile;
    struct {
        playerclass_t   pClass; // Original class, current may differ.
        byte            color; // Current color.
    } players[MAXPLAYERS];

    skillmode_t     skill;
    int             episode; // Unused in jHexen.

    struct {
        int             id; // Id of the current map.
        int             startTic; // Game tic at map start.
        // Position indicator for cooperative net-play reborn.
        int             rebornPosition;
    } map;
    struct {
        int             id; // Id of the previous map.
        int             leavePosition;
    } mapPrev;

    byte            netEpisode; // Unused in jHexen.
    byte            netMap;
    byte            netSkill;
    byte            netSlot;

    gamerules_t     rules;
    gameconfig_t    cfg;
} game_state_t;

#define PLRPROFILE          (gs.playerProfile)
#define GAMERULES           (gs.rules)

extern game_state_t gs;

extern int gsvMapMusic;

void            G_PrintMapList(void);

#endif
