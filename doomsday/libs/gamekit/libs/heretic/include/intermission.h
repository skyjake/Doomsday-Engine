/** @file intermission.h  Heretic specific intermission screens.
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBHERETIC_INTERMISSION_H
#define LIBHERETIC_INTERMISSION_H
#ifdef __cplusplus

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "h_player.h"
#include <doomsday/uri.h>

/**
 * Structure passed to IN_Begin(), etc...
 */
/*struct wbplayerstruct_t
{
    dd_bool inGame;  ///< Whether the player is in game.

    int kills;
    int items;
    int secret;
    int time;
    int frags[MAXPLAYERS];
    int score;       ///< Current score on entry, modified on return.
};*/

struct wbstartstruct_t
{
    res::Uri currentMap;
    res::Uri nextMap;
    bool didSecret;      /**< @c true= the secret map has been visited during the
                              game session. Used to generate the visited maps info
                              for backward compatibility purposes. */
/*
    int maxKills;
    int maxItems;
    int maxSecret;
    int maxFrags;
    int parTime;
    int pNum;           ///< Index of this player in game.
    wbplayerstruct_t plyr[MAXPLAYERS];
*/
};

void IN_Init();
void IN_Shutdown();

/**
 * Begin the intermission using the given game session and player configuration.
 *
 * @param wbstartstruct  Configuration to use for the intermission. Ownership is
 *                       @em not given to IN_Begin() however it is assumed that
 *                       this structure is @em not modified while the intermission
 *                       is in progress.
 */
void IN_Begin(const wbstartstruct_t &wbstartstruct);

/**
 * End the current intermission.
 */
void IN_End();

/**
 * Process game tic for the intermission.
 *
 * @note Handles user input due to timing issues in netgames.
 */
void IN_Ticker();

/**
 * Draw the intermission.
 */
void IN_Drawer();

/**
 * Change the current intermission state.
 */
void IN_SetState(int stateNum /*interludestate_t st*/);

void IN_SetTime(int time);

/**
 * Skip to the next state in the intermission.
 */
void IN_SkipToNext();

/// To be called to register the console commands and variables of this module.
void IN_ConsoleRegister();

#endif // __cplusplus
#endif // LIBHERETIC_INTERMISSION_H
