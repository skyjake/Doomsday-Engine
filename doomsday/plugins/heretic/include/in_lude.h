/** @file in_lude.h  Heretic specific intermission screens.
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

#ifndef LIBHERETIC_IN_LUDE_H
#define LIBHERETIC_IN_LUDE_H
#ifdef __cplusplus

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "h_player.h"
#include <doomsday/uri.h>

extern dd_bool intermission;
extern int interState;
extern int interTime;

/**
 * Structure passed to IN_Init(), etc...
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
    dd_bool didSecret;  ///< @c true= splash the secret level.
    de::Uri currentMap;
    de::Uri nextMap;
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

/// To be called to register the console commands and variables of this module.
void WI_Register();

/**
 * Begin the intermission using the given game session and player configuration.
 *
 * @param wbstartstruct  Configuration to use for the intermission. Ownership is
 *                       @em not given to WI_Init() however it is assumed that
 *                       this structure is @em not modified while the intermission
 *                       is in progress.
 */
void IN_Init(wbstartstruct_t const &wbstartstruct);

void IN_SkipToNext();
void IN_Stop();

void IN_Ticker();
void IN_Drawer();

void IN_WaitStop();
void IN_LoadPics();
void IN_UnloadPics();
void IN_CheckForSkip();
void IN_InitStats();
void IN_InitDeathmatchStats();
void IN_InitNetgameStats();

#endif // __cplusplus
#endif // LIBHERETIC_IN_LUDE_H
